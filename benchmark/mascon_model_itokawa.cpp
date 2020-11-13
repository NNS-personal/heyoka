// Copyright 2020 Francesco Biscani (bluescarni@gmail.com), Dario Izzo (dario.izzo@gmail.com)
//
// This file is part of the heyoka library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <boost/numeric/odeint.hpp>
#include <chrono>
#include <cmath>
#include <fmt/core.h>
#include <iomanip>
#include <iostream>
#include <vector>

#include <heyoka/detail/igor.hpp>
#include <heyoka/expression.hpp>
#include <heyoka/math_functions.hpp>
#include <heyoka/taylor.hpp>

#include "data/mascon_itokawa.hpp"

// This benchmark builds a Taylor integrator for the motion around asteroid Itokawa.
// The mascon model for Itokawa was generated using a thetraedral mesh built upon
// the polyhedral surface model available. Model units are L = asteroid diameter, M = asteroid mass.
// TODO: check the exact values

using namespace heyoka;
using namespace std::chrono;
namespace odeint = boost::numeric::odeint;

namespace benchmark::kw
{
IGOR_MAKE_NAMED_ARGUMENT(G);
} // namespace benchmark::kw

// Pairwise summation of a vector of doubles. TODO make the other one templated?
// https://en.wikipedia.org/wiki/Pairwise_summation
double pairwise_sum(std::vector<double> sum)
{
    if (sum.size() == std::numeric_limits<decltype(sum.size())>::max()) {
        throw std::overflow_error("Overflow detected in pairwise_sum()");
    }

    if (sum.empty()) {
        return 0.;
    }

    while (sum.size() != 1u) {
        std::vector<double> new_sum;

        for (decltype(sum.size()) i = 0; i < sum.size(); i += 2u) {
            if (i + 1u == sum.size()) {
                // We are at the last element of the vector
                // and the size of the vector is odd. Just append
                // the existing value.
                new_sum.push_back(std::move(sum[i]));
            } else {
                new_sum.push_back(std::move(sum[i]) + std::move(sum[i + 1u]));
            }
        }

        new_sum.swap(sum);
    }
    return sum[0];
}

// Here we implement the r.h.s. functor to be used with boost odeint routines.
struct mascon_dynamics {
    // data members
    std::vector<std::vector<double>> m_mascon_points;
    std::vector<double> m_mascon_masses;
    double m_p, m_q, m_r;
    double m_G;
    // constructor
    template <typename P, typename M>
    mascon_dynamics(const P &mascon_points, const M &mascon_masses, double p, double q, double r, double G)
        : m_mascon_masses(std::begin(mascon_masses), std::end(mascon_masses)), m_p(p), m_q(q), m_r(r), m_G(G)
    {
        for (auto i = 0u; i < std::size(m_mascon_masses); ++i) {
            m_mascon_points.push_back(
                std::vector<double>{mascon_points[i][0], mascon_points[i][1], mascon_points[i][2]});
        }
    }
    // call operator
    void operator()(const std::vector<double> &x, std::vector<double> &dxdt, const double /* t */)
    {
        auto dim = std::size(m_mascon_masses);
        std::vector<double> x_acc, y_acc, z_acc;

        // FIRST: Assemble the acceleration due to mascons
        for (decltype(dim) i = 0; i < dim; ++i) {
            auto r2 = (x[0] - m_mascon_points[i][0]) * (x[0] - m_mascon_points[i][0])
                      + (x[1] - m_mascon_points[i][1]) * (x[1] - m_mascon_points[i][1])
                      + (x[2] - mascon_points_itokawa[i][2]) * (x[2] - m_mascon_points[i][2]);
            auto mGpow = m_G * std::pow(r2, -3. / 2.) * m_mascon_masses[i];
            x_acc.push_back((m_mascon_points[i][0] - x[0]) * mGpow);
            y_acc.push_back((m_mascon_points[i][1] - x[1]) * mGpow);
            z_acc.push_back((m_mascon_points[i][2] - x[2]) * mGpow);
        }
        // SECOND: centripetal and Coriolis
        // w x w x r
        auto centripetal_x = -m_q * m_q * x[0] - m_r * m_r * x[0] + m_q * x[1] * m_p + m_r * x[2] * m_p;
        auto centripetal_y = -m_p * m_p * x[1] - m_r * m_r * x[1] + m_p * x[0] * m_q + m_r * x[2] * m_q;
        auto centripetal_z = -m_p * m_p * x[2] - m_q * m_q * x[2] + m_p * x[0] * m_r + m_q * x[1] * m_r;
        // 2 w x v
        auto coriolis_x = 2. * (m_q * x[5] - m_r * x[4]);
        auto coriolis_y = 2. * (m_r * x[3] - m_p * x[5]);
        auto coriolis_z = 2. * (m_p * x[4] - m_q * x[3]);

        dxdt[0] = x[3];
        dxdt[1] = x[4];
        dxdt[2] = x[5];
        dxdt[3] = pairwise_sum(x_acc) - centripetal_x - coriolis_x;
        dxdt[4] = pairwise_sum(y_acc) - centripetal_y - coriolis_y;
        dxdt[5] = pairwise_sum(z_acc) - centripetal_z - coriolis_z;
    }
};

template <typename P, typename M>
double compute_energy(const std::vector<double> x, const P &mascon_points, const M &mascon_masses, double p, double q,
                      double r, double G)
{
    double kinetic = (x[3] * x[3] + x[4] * x[4] + x[5] * x[5]) / 2.;
    double potential_g = 0.;
    for (decltype(std::size(mascon_masses)) i = 0u; i < std::size(mascon_masses); ++i) {
        double distance = std::sqrt((x[0] - mascon_points[i][0]) * (x[0] - mascon_points[i][0])
                                    + (x[1] - mascon_points[i][1]) * (x[1] - mascon_points[i][1])
                                    + (x[2] - mascon_points[i][2]) * (x[2] - mascon_points[i][2]));
        potential_g -= G * mascon_masses[i] / distance;
    }
    double potential_c = -0.5 * (p * p + q * q + r * r) * (x[0] * x[0] + x[1] * x[1] + x[2] * x[2])
                         + 0.5 * (x[0] * p + x[1] * q + x[2] * r) * (x[0] * p + x[1] * q + x[2] * r);
    return kinetic + potential_g + potential_c;
}

// mascon_points -> [N,3] array containing the positions of the masses (units L)
// mascon_masses -> [N] array containing the position of the masses (units M)
// G -> Cavendish constant (units L^3/T^2/M)
// pd, qd, rd -> angular velocity of the asteroid in the frame used for the mascon model (units rad/T)
//
// Note, units must be consistent. Choosing L and M is done via the mascon model, T is drived by the value of G. The
// angular velocity must be consequent (equivalently one can choose the units for w and induce them on the value of G).
template <typename P, typename M, typename... KwArgs>
std::vector<std::pair<expression, expression>> make_mascon_system(const P &mascon_points, const M &mascon_masses,
                                                                  double pd, double qd, double rd, KwArgs &&... kw_args)
{
    // 1 - Check input consistency (TODO)
    // 2 - We parse the unnamed arguments
    igor::parser p{kw_args...};
    if constexpr (p.has_unnamed_arguments()) {
        static_assert(detail::always_false_v<KwArgs...>,
                      "The variadic arguments in the construction of an N-body system contain "
                      "unnamed arguments.");
    } else {
        // G constant (defaults to 1).
        auto G_const = [&p]() {
            if constexpr (p.has(benchmark::kw::G)) {
                return expression{number{std::forward<decltype(p(benchmark::kw::G))>(p(benchmark::kw::G))}};
            } else {
                return expression{number{1.}};
            }
        }();

        // 3 - Create the return value.
        std::vector<std::pair<expression, expression>> retval;
        // 4 - Main code
        auto dim = std::size(mascon_masses);
        auto [x, y, z, vx, vy, vz] = make_vars("x", "y", "z", "vx", "vy", "vz");
        // Assemble the contributions to the x/y/z accelerations from each mass.
        std::vector<expression> x_acc, y_acc, z_acc;
        // Assembling the r.h.s.
        // FIRST: the acceleration due to the mascon points
        for (decltype(dim) i = 0; i < dim; ++i) {
            auto x_masc = expression{number{mascon_points[i][0]}};
            auto y_masc = expression{number{mascon_points[i][1]}};
            auto z_masc = expression{number{mascon_points[i][2]}};
            auto m_masc = expression{number{mascon_masses[i]}};
            auto r2 = (x - x_masc) * (x - x_masc) + (y - y_masc) * (y - y_masc) + (z - z_masc) * (z - z_masc);

            x_acc.push_back(G_const * m_masc * (x_masc - x) * pow(r2, expression{number{-3. / 2}}));
            y_acc.push_back(G_const * m_masc * (y_masc - y) * pow(r2, expression{number{-3. / 2}}));
            z_acc.push_back(G_const * m_masc * (z_masc - z) * pow(r2, expression{number{-3. / 2}}));
        }
        // SECOND: centripetal and Coriolis
        auto p = expression{number{pd}};
        auto q = expression{number{qd}};
        auto r = expression{number{rd}};
        // w x w x r
        auto centripetal_x = -q * q * x - r * r * x + q * y * p + r * z * p;
        auto centripetal_y = -p * p * y - r * r * y + p * x * q + r * z * q;
        auto centripetal_z = -p * p * z - q * q * z + p * x * r + q * y * r;
        // 2 w x v
        auto coriolis_x = expression{number{2.}} * (q * vz - r * vy);
        auto coriolis_y = expression{number{2.}} * (r * vx - p * vz);
        auto coriolis_z = expression{number{2.}} * (p * vy - q * vx);

        // Assembling the return vector containing l.h.s. and r.h.s. (note the fundamental use of pairwise_sum for
        // efficiency and to allow compact mode to do his job)
        retval.push_back(prime(x) = vx);
        retval.push_back(prime(y) = vy);
        retval.push_back(prime(z) = vz);
        retval.push_back(prime(vx) = pairwise_sum(x_acc) - centripetal_x - coriolis_x);
        retval.push_back(prime(vy) = pairwise_sum(y_acc) - centripetal_y - coriolis_y);
        retval.push_back(prime(vz) = pairwise_sum(z_acc) - centripetal_z - coriolis_z);

        return retval;
    }
}

int main(int argc, char *argv[])
{
    // Equations of motion.
    // L = 661.885851381733m (computed from the mascon model and since Itokawa is 535.3104705810547m long from the NASA
    // 3D model) M = 3.51E10 Kg (from wikipedia) G = 6.67430E-11 (wikipedia again) induced time units: T = sqrt(L^3/G/M)
    // = 11125.466397427153s The asteroid angular velocity in our units is thus Wz = 2pi / (12.132 * 60 * 60 / T)
    // = 1.6005276908596755
    auto wz = 1.6005276908596755;
    auto eom = make_mascon_system(mascon_points_itokawa, mascon_masses_itokawa, 0., 0., wz, benchmark::kw::G = 1.);
    // Initial conditions
    auto r0 = 2.;
    auto v0 = std::sqrt(1. / r0) - wz * r0;
    auto incl = 0. / 360 * 6.28;
    std::vector<double> ic = {r0, 0., 0., 0., std::cos(incl) * v0, std::sin(incl) * v0};
    // Initial Energy
    double E0 = compute_energy(ic, mascon_points_itokawa, mascon_masses_itokawa, 0., 0., wz, 1.);

    // Lets use boost libraries.
    double test_time = 10.;
    // The error stepper
    //typedef odeint::runge_kutta_cash_karp54<std::vector<double>> error_stepper_type;
    typedef odeint::runge_kutta_fehlberg78<std::vector<double>> error_stepper_type;
    // The adaptive strategy
    typedef odeint::controlled_runge_kutta<error_stepper_type> controlled_stepper_type;
    controlled_stepper_type controlled_stepper;
    // The dynamics
    mascon_dynamics dynamics(mascon_points_itokawa, mascon_masses_itokawa, 0., 0., wz, 1.);
    auto start = high_resolution_clock::now();
    odeint::integrate_adaptive(odeint::make_controlled<error_stepper_type>(1.0e-14, 1.0e-14), dynamics, ic, 0.0, test_time,
                               1e-8);
    auto stop = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(stop - start);
    std::cout << "Integration time (RKF7(8)): " << duration.count() / 1e6 << "s" << std::endl;
    auto energy = compute_energy(ic, mascon_points_itokawa, mascon_masses_itokawa, 0., 0., wz, 1.);
    fmt::print("Energy error: {}, ", (energy - E0) / E0);


    ic = {r0, 0., 0., 0., std::sqrt(1. / r0) - wz * r0, 0.};
    // Constructing the integrator.
    start = high_resolution_clock::now();
    taylor_adaptive<double> taylor{eom, ic, kw::compact_mode = true, kw::tol = 1e-14};
    stop = high_resolution_clock::now();
    duration = duration_cast<microseconds>(stop - start);
    std::cout << "\nTime to construct the integrator: " << duration.count() / 1e6 << "s" << std::endl;

    start = high_resolution_clock::now();
    taylor.propagate_until(test_time);
    stop = high_resolution_clock::now();
    duration = duration_cast<microseconds>(stop - start);
    std::cout << "Integration time (Taylor): " << duration.count() / 1e6 << "s" << std::endl;

    energy = compute_energy(taylor.get_state(), mascon_points_itokawa, mascon_masses_itokawa, 0., 0., wz, 1.);
    fmt::print("Energy error: {}, ", (energy - E0) / E0);
    // double dt = 0.2;
    // for (auto i = 0u; i < 3000; ++i) {
    //    taylor.propagate_for(dt);
    //    auto state = taylor.get_state();
    //    auto energy = compute_energy(state, mascon_points_itokawa, mascon_masses_itokawa, 0., 0., wz, 1.);
    //    fmt::print("{}, ", (energy - E0) / E0);
    //    // fmt::print("[{}, {}, {}, {}, {}, {}],\n", state[0], state[1], state[2], state[3], state[4], state[5]);
    //}
    return 0;
}
