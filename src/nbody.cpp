// Copyright 2020, 2021 Francesco Biscani (bluescarni@gmail.com), Dario Izzo (dario.izzo@gmail.com)
//
// This file is part of the heyoka library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <random>
#include <stdexcept>
#include <utility>
#include <vector>

#include <boost/math/constants/constants.hpp>
#include <boost/numeric/conversion/cast.hpp>

#include <fmt/format.h>

#include <heyoka/detail/string_conv.hpp>
#include <heyoka/expression.hpp>
#include <heyoka/math.hpp>
#include <heyoka/nbody.hpp>
#include <heyoka/number.hpp>
#include <heyoka/variable.hpp>

namespace heyoka
{

namespace detail

{

std::vector<std::pair<expression, expression>> make_nbody_sys_fixed_masses(std::uint32_t n, number Gconst,
                                                                           std::vector<number> masses)
{
    assert(n >= 2u);

    if (masses.size() != n) {
        using namespace fmt::literals;

        throw std::invalid_argument(
            "Inconsistent sizes detected while creating an N-body system: the vector of masses has a size of "
            "{}, while the number of bodies is {}"_format(masses.size(), n));
    }

    // Create the state variables.
    std::vector<expression> x_vars, y_vars, z_vars, vx_vars, vy_vars, vz_vars;

    for (std::uint32_t i = 0; i < n; ++i) {
        const auto i_str = li_to_string(i);

        x_vars.emplace_back(variable("x_" + i_str));
        y_vars.emplace_back(variable("y_" + i_str));
        z_vars.emplace_back(variable("z_" + i_str));

        vx_vars.emplace_back(variable("vx_" + i_str));
        vy_vars.emplace_back(variable("vy_" + i_str));
        vz_vars.emplace_back(variable("vz_" + i_str));
    }

    // Create the return value.
    std::vector<std::pair<expression, expression>> retval;

    // Accumulators for the accelerations on the bodies.
    // The i-th element of x/y/z_acc contains the list of
    // accelerations on body i due to all the other bodies.
    std::vector<std::vector<expression>> x_acc;
    x_acc.resize(boost::numeric_cast<decltype(x_acc.size())>(n));
    auto y_acc = x_acc;
    auto z_acc = x_acc;

    // Detect if we are creating a restricted problem. In a restricted
    // problem, the first n particles have mass, the remaining ones do not.
    std::uint32_t n_fc_massive = 0, n_fc_massless = 0;

    // Determine the number of massive particles at the beginning
    // of the masses vector.
    auto it = masses.begin();
    while (it != masses.end() && !is_zero(*it)) {
        ++n_fc_massive;
        ++it;
    }

    // Determine the number of massless particles following
    // the first group of massive particles at the beginning
    // of the masses vector.
    while (it != masses.end() && is_zero(*it)) {
        ++n_fc_massless;
        ++it;
    }
    assert(n_fc_massive + n_fc_massless <= n);

    if (n_fc_massless != 0u && n_fc_massive + n_fc_massless == n) {
        // We have some massless particles, and the vector of masses
        // is divided into two parts: massive particles followed by
        // massless particles. Thus, we are in the restricted case.

        // Compute the accelerations exerted by the massive particles
        // on all particles.
        for (std::uint32_t i = 0; i < n_fc_massive; ++i) {
            // r' = v.
            retval.push_back(prime(x_vars[i]) = vx_vars[i]);
            retval.push_back(prime(y_vars[i]) = vy_vars[i]);
            retval.push_back(prime(z_vars[i]) = vz_vars[i]);

            // Compute the total acceleration on body i,
            // accumulating the result in x/y/z_acc. Part of the
            // calculation will be re-used to compute
            // the contribution from i to the total acceleration
            // on body j.
            for (std::uint32_t j = i + 1u; j < n; ++j) {
                auto diff_x = x_vars[j] - x_vars[i];
                auto diff_y = y_vars[j] - y_vars[i];
                auto diff_z = z_vars[j] - z_vars[i];

                auto r_m3 = pow(square(diff_x) + square(diff_y) + square(diff_z), expression{-3. / 2});
                if (j < n_fc_massive) {
                    // Body j is massive and it interacts mutually with body i.
                    // NOTE: the idea here is that we want to help the CSE process
                    // when computing the Taylor decomposition. Thus, we try
                    // to maximise the re-use of an expression with the goal
                    // of having it simplified in the CSE.
                    auto fac_j = expression{Gconst * masses[j]} * r_m3;
                    auto c_ij = expression{-masses[i] / masses[j]};

                    // Acceleration exerted by j on i.
                    x_acc[i].push_back(diff_x * fac_j);
                    y_acc[i].push_back(diff_y * fac_j);
                    z_acc[i].push_back(diff_z * fac_j);

                    // Acceleration exerted by i on j.
                    x_acc[j].push_back(diff_x * fac_j * c_ij);
                    y_acc[j].push_back(diff_y * fac_j * c_ij);
                    z_acc[j].push_back(diff_z * fac_j * c_ij);
                } else {
                    // Body j is massless, add the acceleration
                    // on it due to the massive body i.
                    auto fac = expression{-Gconst * masses[i]} * r_m3;

                    x_acc[j].push_back(diff_x * fac);
                    y_acc[j].push_back(diff_y * fac);
                    z_acc[j].push_back(diff_z * fac);
                }
            }

            // Add the expressions of the accelerations to the system.
            retval.push_back(prime(vx_vars[i]) = pairwise_sum(x_acc[i]));
            retval.push_back(prime(vy_vars[i]) = pairwise_sum(y_acc[i]));
            retval.push_back(prime(vz_vars[i]) = pairwise_sum(z_acc[i]));
        }

        // All the accelerations on the massless particles
        // have already been accumulated in the loop above.
        // We just need to perform the pairwise sums on x/y/z_acc.
        for (std::uint32_t i = n_fc_massive; i < n; ++i) {
            retval.push_back(prime(x_vars[i]) = vx_vars[i]);
            retval.push_back(prime(y_vars[i]) = vy_vars[i]);
            retval.push_back(prime(z_vars[i]) = vz_vars[i]);

            retval.push_back(prime(vx_vars[i]) = pairwise_sum(x_acc[i]));
            retval.push_back(prime(vy_vars[i]) = pairwise_sum(y_acc[i]));
            retval.push_back(prime(vz_vars[i]) = pairwise_sum(z_acc[i]));
        }
    } else {
        for (std::uint32_t i = 0; i < n; ++i) {
            // r' = v.
            retval.push_back(prime(x_vars[i]) = vx_vars[i]);
            retval.push_back(prime(y_vars[i]) = vy_vars[i]);
            retval.push_back(prime(z_vars[i]) = vz_vars[i]);

            // Compute the total acceleration on body i,
            // accumulating the result in x/y/z_acc. Part of the
            // calculation will be re-used to compute
            // the contribution from i to the total acceleration
            // on body j.
            for (std::uint32_t j = i + 1u; j < n; ++j) {
                auto diff_x = x_vars[j] - x_vars[i];
                auto diff_y = y_vars[j] - y_vars[i];
                auto diff_z = z_vars[j] - z_vars[i];

                auto r_m3 = pow(square(diff_x) + square(diff_y) + square(diff_z), expression{-3. / 2});
                if (is_zero(masses[j])) {
                    // NOTE: special-case for m_j = 0, so that
                    // we avoid a division by zero in the other branch.
                    auto fac = expression{-Gconst * masses[i]} * r_m3;

                    x_acc[j].push_back(diff_x * fac);
                    y_acc[j].push_back(diff_y * fac);
                    z_acc[j].push_back(diff_z * fac);
                } else {
                    // NOTE: the idea here is that we want to help the CSE process
                    // when computing the Taylor decomposition. Thus, we try
                    // to maximise the re-use of an expression with the goal
                    // of having it simplified in the CSE.
                    auto fac_j = expression{Gconst * masses[j]} * r_m3;
                    auto c_ij = expression{-masses[i] / masses[j]};

                    // Acceleration exerted by j on i.
                    x_acc[i].push_back(diff_x * fac_j);
                    y_acc[i].push_back(diff_y * fac_j);
                    z_acc[i].push_back(diff_z * fac_j);

                    // Acceleration exerted by i on j.
                    x_acc[j].push_back(diff_x * fac_j * c_ij);
                    y_acc[j].push_back(diff_y * fac_j * c_ij);
                    z_acc[j].push_back(diff_z * fac_j * c_ij);
                }
            }

            // Add the expressions of the accelerations to the system.
            retval.push_back(prime(vx_vars[i]) = pairwise_sum(x_acc[i]));
            retval.push_back(prime(vy_vars[i]) = pairwise_sum(y_acc[i]));
            retval.push_back(prime(vz_vars[i]) = pairwise_sum(z_acc[i]));
        }
    }

    return retval;
}

std::vector<std::pair<expression, expression>> make_nbody_sys_par_masses(std::uint32_t n, number Gconst,
                                                                         std::uint32_t n_massive)
{
    assert(n >= 2u);

    if (n_massive > n) {
        using namespace fmt::literals;

        throw std::invalid_argument("The number of massive bodies, {}, cannot be larger than the "
                                    "total number of bodies, {}"_format(n_massive, n));
    }

    // Create the state variables.
    std::vector<expression> x_vars, y_vars, z_vars, vx_vars, vy_vars, vz_vars;

    for (std::uint32_t i = 0; i < n; ++i) {
        const auto i_str = li_to_string(i);

        x_vars.emplace_back(variable("x_" + i_str));
        y_vars.emplace_back(variable("y_" + i_str));
        z_vars.emplace_back(variable("z_" + i_str));

        vx_vars.emplace_back(variable("vx_" + i_str));
        vy_vars.emplace_back(variable("vy_" + i_str));
        vz_vars.emplace_back(variable("vz_" + i_str));
    }

    // Create the return value.
    std::vector<std::pair<expression, expression>> retval;

    // Accumulators for the accelerations on the bodies.
    // The i-th element of x/y/z_acc contains the list of
    // accelerations on body i due to all the other bodies.
    std::vector<std::vector<expression>> x_acc;
    x_acc.resize(boost::numeric_cast<decltype(x_acc.size())>(n));
    auto y_acc = x_acc;
    auto z_acc = x_acc;

    // Compute the accelerations exerted by the massive particles
    // on all particles.
    for (std::uint32_t i = 0; i < n_massive; ++i) {
        // r' = v.
        retval.push_back(prime(x_vars[i]) = vx_vars[i]);
        retval.push_back(prime(y_vars[i]) = vy_vars[i]);
        retval.push_back(prime(z_vars[i]) = vz_vars[i]);

        // Compute the total acceleration on body i,
        // accumulating the result in x/y/z_acc. Part of the
        // calculation will be re-used to compute
        // the contribution from i to the total acceleration
        // on body j.
        for (std::uint32_t j = i + 1u; j < n; ++j) {
            auto diff_x = x_vars[j] - x_vars[i];
            auto diff_y = y_vars[j] - y_vars[i];
            auto diff_z = z_vars[j] - z_vars[i];

            auto r_m3 = pow(square(diff_x) + square(diff_y) + square(diff_z), expression{-3. / 2});
            if (j < n_massive) {
                // Body j is massive and it interacts mutually with body i.
                // NOTE: the idea here is that we want to help the CSE process
                // when computing the Taylor decomposition. Thus, we try
                // to maximise the re-use of an expression with the goal
                // of having it simplified in the CSE.
                auto fac_j = expression{Gconst} * par[j] * r_m3;
                auto c_ij = -par[i] / par[j];

                // Acceleration exerted by j on i.
                x_acc[i].push_back(diff_x * fac_j);
                y_acc[i].push_back(diff_y * fac_j);
                z_acc[i].push_back(diff_z * fac_j);

                // Acceleration exerted by i on j.
                x_acc[j].push_back(diff_x * fac_j * c_ij);
                y_acc[j].push_back(diff_y * fac_j * c_ij);
                z_acc[j].push_back(diff_z * fac_j * c_ij);
            } else {
                // Body j is massless, add the acceleration
                // on it due to the massive body i.
                auto fac = expression{-Gconst} * par[i] * r_m3;

                x_acc[j].push_back(diff_x * fac);
                y_acc[j].push_back(diff_y * fac);
                z_acc[j].push_back(diff_z * fac);
            }
        }

        // Add the expressions of the accelerations to the system.
        retval.push_back(prime(vx_vars[i]) = pairwise_sum(x_acc[i]));
        retval.push_back(prime(vy_vars[i]) = pairwise_sum(y_acc[i]));
        retval.push_back(prime(vz_vars[i]) = pairwise_sum(z_acc[i]));
    }

    // All the accelerations on the massless particles
    // have already been accumulated in the loop above.
    // We just need to perform the pairwise sums on x/y/z_acc.
    for (std::uint32_t i = n_massive; i < n; ++i) {
        retval.push_back(prime(x_vars[i]) = vx_vars[i]);
        retval.push_back(prime(y_vars[i]) = vy_vars[i]);
        retval.push_back(prime(z_vars[i]) = vz_vars[i]);

        retval.push_back(prime(vx_vars[i]) = pairwise_sum(x_acc[i]));
        retval.push_back(prime(vy_vars[i]) = pairwise_sum(y_acc[i]));
        retval.push_back(prime(vz_vars[i]) = pairwise_sum(z_acc[i]));
    }

    return retval;
}

namespace
{

// 3d dot product helper.
template <typename T>
auto dot(const T &a, const T &b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

} // namespace

} // namespace detail

// Helper to generate a random elliptic orbit and convert it to cartesian variables. The min/max
// values of a, e, i, om, Om and f are passed in the bounds array. mu is the gravitational parameter
// of the two-body system.
std::array<double, 6> random_elliptic_state(double mu, const std::array<std::pair<double, double>, 6> &bounds,
                                            unsigned seed)
{
    using namespace fmt::literals;

    // Validate input params.
    if (!std::isfinite(mu) || mu <= 0) {
        throw std::invalid_argument(
            "Invalid mu parameter used in random_elliptic_state(): it must be positive and finite, but it is {} instead"_format(
                mu));
    }

    for (const auto &b : bounds) {
        const auto &[lb, ub] = b;

        if (!std::isfinite(lb) || !std::isfinite(ub) || !(ub > lb)) {
            throw std::invalid_argument(
                "Invalid lower/upper bounds detected in random_elliptic_state(): the bounds must be finite and such that ub > lb, but they are [{}, {}) instead"_format(
                    lb, ub));
        }
    }

    // Element-specific validation.
    const auto &[a_min, a_max] = bounds[0];
    if (a_min <= 0) {
        throw std::invalid_argument(
            "Invalid minimum semi-major axis detected in random_elliptic_state(): a_min must be positive, but it is {} instead"_format(
                a_min));
    }
    if ((a_max - a_min) > std::numeric_limits<double>::max()) {
        throw std::overflow_error("Overflow error in the semi-major axis range passed to random_elliptic_state()");
    }

    const auto &[e_min, e_max] = bounds[1];
    if (e_min <= 0 || e_max > 1) {
        throw std::invalid_argument(
            "Invalid eccentricity range detected in random_elliptic_state(): the range must be (0, 1), but it is [{}, {}) instead"_format(
                e_min, e_max));
    }

    const auto &[inc_min, inc_max] = bounds[2];
    if (inc_min <= 0 || inc_max > boost::math::constants::pi<double>()) {
        throw std::invalid_argument(
            "Invalid inclination range detected in random_elliptic_state(): the range must be (0, π), but it is [{}, {}) instead"_format(
                inc_min, inc_max));
    }

    const auto &[om_min, om_max] = bounds[3];
    if ((om_max - om_min) > std::numeric_limits<double>::max()) {
        throw std::overflow_error("Overflow error in the omega range passed to random_elliptic_state()");
    }

    const auto &[Om_min, Om_max] = bounds[4];
    if ((Om_max - Om_min) > std::numeric_limits<double>::max()) {
        throw std::overflow_error("Overflow error in the Omega range passed to random_elliptic_state()");
    }

    const auto &[f_min, f_max] = bounds[5];
    if ((f_max - f_min) > std::numeric_limits<double>::max()) {
        throw std::overflow_error("Overflow error in the true anomaly range passed to random_elliptic_state()");
    }

    // Setup the rng.
    static thread_local std::mt19937 rng;
    rng.seed(static_cast<decltype(rng())>(seed));

    // Throw the dice for the orbital elements.
    std::uniform_real_distribution rdist(a_min, a_max);
    using p_type = decltype(rdist.param());
    const auto a = rdist(rng);
    rdist.param(p_type(e_min, e_max));
    const auto e = rdist(rng);
    rdist.param(p_type(inc_min, inc_max));
    const auto inc = rdist(rng);
    rdist.param(p_type(om_min, om_max));
    const auto om = rdist(rng);
    rdist.param(p_type(Om_min, Om_max));
    const auto Om = rdist(rng);
    rdist.param(p_type(f_min, f_max));
    const auto f = rdist(rng);

    // Transform into cartesian state.
    using std::atan;
    using std::cos;
    using std::sin;
    using std::sqrt;
    using std::tan;

    // Fetch the elliptic anomaly.
    const auto E = 2 * atan(sqrt((1 - e) / (1 + e)) * tan(f / 2));

    // Mean motion.
    const auto n = sqrt(mu / (a * a * a));

    // Position/velocity in the orbital frame.
    const auto q = std::array{a * (cos(E) - e), a * sqrt(1 - e * e) * sin(E), 0.};
    const auto vq
        = std::array{-n * a * sin(E) / (1 - e * cos(E)), n * a * sqrt(1 - e * e) * cos(E) / (1 - e * cos(E)), 0.};

    // The rotation matrix.
    const auto r1 = std::array{cos(Om) * cos(om) - sin(Om) * cos(inc) * sin(om),
                               -cos(Om) * sin(om) - sin(Om) * cos(inc) * cos(om), sin(Om) * sin(inc)};
    const auto r2 = std::array{sin(Om) * cos(om) + cos(Om) * cos(inc) * sin(om),
                               -sin(Om) * sin(om) + cos(Om) * cos(inc) * cos(om), -cos(Om) * sin(inc)};
    const auto r3 = std::array{sin(inc) * sin(om), sin(inc) * cos(om), cos(inc)};

    // Final position/velocity.
    using detail::dot;
    const auto x = std::array{dot(r1, q), dot(r2, q), dot(r3, q)};
    const auto v = std::array{dot(r1, vq), dot(r2, vq), dot(r3, vq)};

    return {x[0], x[1], x[2], v[0], v[1], v[2]};
}

// Convert the input cartesian state into Keplerian orbital elements.
std::array<double, 6> cartesian_to_oe(double mu, const std::array<double, 6> &s)
{
    if (std::any_of(s.begin(), s.end(), [](const auto &x) { return !std::isfinite(x); })) {
        throw std::invalid_argument("Non-finite values detected in the cartesian state passed to cartesian_to_oe()");
    }

    const auto [x, y, z, vx, vy, vz] = s;
    const auto pos = std::array{x, y, z};
    const auto vel = std::array{vx, vy, vz};

    // Cross product helper.
    auto cross = [](const auto &a, const auto &b) {
        auto [a1, a2, a3] = a;
        auto [b1, b2, b3] = b;

        return std::array{a2 * b3 - a3 * b2, a3 * b1 - a1 * b3, a1 * b2 - a2 * b1};
    };

    // Divide array by scalar.
    auto div = [](const auto &a, const auto &d) { return std::array{a[0] / d, a[1] / d, a[2] / d}; };

    // Array subtraction.
    auto sub = [](const auto &a, const auto &b) { return std::array{a[0] - b[0], a[1] - b[1], a[2] - b[2]}; };

    // Array norm2.
    auto norm2 = [](const auto &a) { return a[0] * a[0] + a[1] * a[1] + a[2] * a[2]; };

    // Array norm.
    auto norm = [norm2](const auto &a) { return std::sqrt(norm2(a)); };

    const auto h = cross(pos, vel);
    const auto e_v = sub(div(cross(vel, h), mu), div(pos, norm(pos)));
    const auto n = std::array{-h[1], h[0], 0.};

    using detail::dot;
    using std::acos;
    auto f = acos(dot(e_v, pos) / (norm(e_v) * norm(pos)));
    if (dot(pos, vel) < 0) {
        f = 2 * boost::math::constants::pi<double>() - f;
    }

    const auto inc = acos(h[2] / norm(h));

    const auto e = norm(e_v);

    auto Om = acos(n[0] / norm(n));
    if (n[1] < 0) {
        Om = 2 * boost::math::constants::pi<double>() - Om;
    }

    auto om = acos(dot(n, e_v) / (norm(n) * norm(e_v)));
    if (e_v[2] < 0) {
        om = 2 * boost::math::constants::pi<double>() - om;
    }

    const auto a = 1 / (2 / norm(pos) - norm2(vel) / mu);

    return {a, e, inc, om, Om, f};
}

} // namespace heyoka
