// Copyright 2020 Francesco Biscani (bluescarni@gmail.com), Dario Izzo (dario.izzo@gmail.com)
//
// This file is part of the heyoka library.
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#include <heyoka/config.hpp>

#include <initializer_list>
#include <tuple>
#include <vector>

#if defined(HEYOKA_HAVE_REAL128)

#include <mp++/real128.hpp>

#endif

#include <heyoka/binary_operator.hpp>
#include <heyoka/expression.hpp>
#include <heyoka/llvm_state.hpp>

#include "catch.hpp"
#include "test_utils.hpp"

using namespace heyoka;
using namespace heyoka_test;

const auto fp_types = std::tuple<double, long double
#if defined(HEYOKA_HAVE_REAL128)
                                 ,
                                 mppp::real128
#endif
                                 >{};

TEST_CASE("taylor mul")
{
    auto tester = [](auto fp_x) {
        using fp_t = decltype(fp_x);

        using Catch::Matchers::Message;

        auto x = "x"_var, y = "y"_var;

        // Number-number tests.
        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>(
                "jet", {expression{binary_operator{binary_operator::type::mul, 2_dbl, 3_dbl}}, x + y}, 1, 1);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{3}};
            jet.resize(4);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == 3);
            REQUIRE(jet[2] == approximately(fp_t{6}));
            REQUIRE(jet[3] == approximately(fp_t{5}));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>(
                "jet", {expression{binary_operator{binary_operator::type::mul, 2_dbl, 3_dbl}}, x + y}, 1, 2);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{-2}, fp_t{3}, fp_t{-3}};
            jet.resize(8);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == -2);
            REQUIRE(jet[2] == 3);
            REQUIRE(jet[3] == -3);
            REQUIRE(jet[4] == 6);
            REQUIRE(jet[5] == 6);
            REQUIRE(jet[6] == 5);
            REQUIRE(jet[7] == -5);
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>(
                "jet", {expression{binary_operator{binary_operator::type::mul, 2_dbl, 3_dbl}}, x + y}, 2, 1);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{3}};
            jet.resize(6);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == 3);
            REQUIRE(jet[2] == approximately(fp_t{6}));
            REQUIRE(jet[3] == approximately(fp_t{5}));
            REQUIRE(jet[4] == 0);
            REQUIRE(jet[5] == approximately(fp_t{1} / 2 * (fp_t{6} + jet[3])));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>(
                "jet", {expression{binary_operator{binary_operator::type::mul, 2_dbl, 3_dbl}}, x + y}, 2, 2);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{-2}, fp_t{3}, fp_t{-3}};
            jet.resize(12);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == -2);
            REQUIRE(jet[2] == 3);
            REQUIRE(jet[3] == -3);
            REQUIRE(jet[4] == approximately(fp_t{6}));
            REQUIRE(jet[5] == approximately(fp_t{6}));
            REQUIRE(jet[6] == approximately(fp_t{5}));
            REQUIRE(jet[7] == approximately(-fp_t{5}));
            REQUIRE(jet[8] == 0);
            REQUIRE(jet[9] == 0);
            REQUIRE(jet[10] == approximately(.5 * (fp_t{6} + jet[6])));
            REQUIRE(jet[11] == approximately(.5 * (fp_t{6} + jet[7])));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>(
                "jet", {expression{binary_operator{binary_operator::type::mul, 2_dbl, 3_dbl}}, x + y}, 3, 3);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{-2}, fp_t{-1}, fp_t{3}, fp_t{2}, fp_t{4}};
            jet.resize(24);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == -2);
            REQUIRE(jet[2] == -1);
            REQUIRE(jet[3] == 3);
            REQUIRE(jet[4] == 2);
            REQUIRE(jet[5] == 4);
            REQUIRE(jet[6] == approximately(fp_t{6}));
            REQUIRE(jet[7] == approximately(fp_t{6}));
            REQUIRE(jet[8] == approximately(fp_t{6}));
            REQUIRE(jet[9] == approximately(fp_t{5}));
            REQUIRE(jet[10] == approximately(fp_t{0}));
            REQUIRE(jet[11] == approximately(fp_t{3}));
            REQUIRE(jet[12] == 0);
            REQUIRE(jet[13] == 0);
            REQUIRE(jet[14] == 0);
            REQUIRE(jet[15] == approximately(fp_t{1} / 2 * (fp_t{6} + jet[9])));
            REQUIRE(jet[16] == approximately(fp_t{1} / 2 * (fp_t{6} + jet[10])));
            REQUIRE(jet[17] == approximately(fp_t{1} / 2 * (fp_t{6} + jet[11])));
            REQUIRE(jet[18] == 0);
            REQUIRE(jet[19] == 0);
            REQUIRE(jet[20] == 0);
            REQUIRE(jet[21] == approximately(1 / fp_t{6} * (2 * jet[15])));
            REQUIRE(jet[22] == approximately(1 / fp_t{6} * (2 * jet[16])));
            REQUIRE(jet[23] == approximately(1 / fp_t{6} * (2 * jet[17])));
        }

        // Variable-number tests.
        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {y * 2_dbl, x * -4_dbl}, 1, 1);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{3}};
            jet.resize(4);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == 3);
            REQUIRE(jet[2] == approximately(fp_t{6}));
            REQUIRE(jet[3] == approximately(-fp_t{8}));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {y * 2_dbl, x * -4_dbl}, 1, 2);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{1}, fp_t{3}, fp_t{-4}};
            jet.resize(8);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == 1);

            REQUIRE(jet[2] == 3);
            REQUIRE(jet[3] == -4);

            REQUIRE(jet[4] == approximately(fp_t{6}));
            REQUIRE(jet[5] == approximately(-fp_t{8}));

            REQUIRE(jet[6] == approximately(-fp_t{8}));
            REQUIRE(jet[7] == approximately(-fp_t{4}));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {y * 2_dbl, x * -4_dbl}, 2, 1);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{3}};
            jet.resize(6);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == 3);
            REQUIRE(jet[2] == approximately(fp_t{6}));
            REQUIRE(jet[3] == approximately(-fp_t{8}));
            REQUIRE(jet[4] == approximately(jet[3]));
            REQUIRE(jet[5] == approximately(-2 * jet[2]));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {y * 2_dbl, x * -4_dbl}, 2, 2);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{-1}, fp_t{3}, fp_t{4}};
            jet.resize(12);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == -1);
            REQUIRE(jet[2] == 3);
            REQUIRE(jet[3] == 4);
            REQUIRE(jet[4] == approximately(fp_t{6}));
            REQUIRE(jet[5] == approximately(fp_t{8}));
            REQUIRE(jet[6] == approximately(-fp_t{8}));
            REQUIRE(jet[7] == approximately(fp_t{4}));
            REQUIRE(jet[8] == approximately(jet[6]));
            REQUIRE(jet[9] == approximately(jet[7]));
            REQUIRE(jet[10] == approximately(-2 * jet[4]));
            REQUIRE(jet[11] == approximately(-2 * jet[5]));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {y * 2_dbl, x * -4_dbl}, 3, 3);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{-1}, fp_t{0}, fp_t{3}, fp_t{4}, fp_t{-5}};
            jet.resize(24);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == -1);
            REQUIRE(jet[2] == 0);

            REQUIRE(jet[3] == 3);
            REQUIRE(jet[4] == 4);
            REQUIRE(jet[5] == -5);

            REQUIRE(jet[6] == approximately(fp_t{6}));
            REQUIRE(jet[7] == approximately(fp_t{8}));
            REQUIRE(jet[8] == approximately(fp_t{-10}));

            REQUIRE(jet[9] == approximately(-fp_t{8}));
            REQUIRE(jet[10] == approximately(fp_t{4}));
            REQUIRE(jet[11] == approximately(fp_t{0}));

            REQUIRE(jet[12] == approximately(jet[9]));
            REQUIRE(jet[13] == approximately(jet[10]));
            REQUIRE(jet[14] == approximately(jet[11]));

            REQUIRE(jet[15] == approximately(-2 * jet[6]));
            REQUIRE(jet[16] == approximately(-2 * jet[7]));
            REQUIRE(jet[17] == approximately(-2 * jet[8]));

            REQUIRE(jet[18] == approximately(1 / fp_t{6} * 4 * jet[15]));
            REQUIRE(jet[19] == approximately(1 / fp_t{6} * 4 * jet[16]));
            REQUIRE(jet[20] == approximately(1 / fp_t{6} * 4 * jet[17]));

            REQUIRE(jet[21] == approximately(-1 / fp_t{6} * 8 * jet[12]));
            REQUIRE(jet[22] == approximately(-1 / fp_t{6} * 8 * jet[13]));
            REQUIRE(jet[23] == approximately(-1 / fp_t{6} * 8 * jet[14]));
        }

        // Number/variable tests.
        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {2_dbl * y, -4_dbl * x}, 1, 1);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{3}};
            jet.resize(4);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == 3);
            REQUIRE(jet[2] == approximately(fp_t{6}));
            REQUIRE(jet[3] == approximately(-fp_t{8}));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {2_dbl * y, -4_dbl * x}, 1, 2);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{-1}, fp_t{3}, fp_t{4}};
            jet.resize(8);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == -1);

            REQUIRE(jet[2] == 3);
            REQUIRE(jet[3] == 4);

            REQUIRE(jet[4] == approximately(fp_t{6}));
            REQUIRE(jet[5] == approximately(fp_t{8}));

            REQUIRE(jet[6] == approximately(-fp_t{8}));
            REQUIRE(jet[7] == approximately(fp_t{4}));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {2_dbl * y, -4_dbl * x}, 2, 1);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{3}};
            jet.resize(6);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == 3);
            REQUIRE(jet[2] == approximately(fp_t{6}));
            REQUIRE(jet[3] == approximately(-fp_t{8}));
            REQUIRE(jet[4] == approximately(jet[3]));
            REQUIRE(jet[5] == approximately(-2 * jet[2]));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {2_dbl * y, -4_dbl * x}, 2, 2);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{-1}, fp_t{3}, fp_t{4}};
            jet.resize(12);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == -1);
            REQUIRE(jet[2] == 3);
            REQUIRE(jet[3] == 4);
            REQUIRE(jet[4] == approximately(fp_t{6}));
            REQUIRE(jet[5] == approximately(fp_t{8}));
            REQUIRE(jet[6] == approximately(-fp_t{8}));
            REQUIRE(jet[7] == approximately(fp_t{4}));
            REQUIRE(jet[8] == approximately(jet[6]));
            REQUIRE(jet[9] == approximately(jet[7]));
            REQUIRE(jet[10] == approximately(-2 * jet[4]));
            REQUIRE(jet[11] == approximately(-2 * jet[5]));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {2_dbl * y, -4_dbl * x}, 3, 3);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{-1}, fp_t{0}, fp_t{3}, fp_t{4}, fp_t{-5}};
            jet.resize(24);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == -1);
            REQUIRE(jet[2] == 0);

            REQUIRE(jet[3] == 3);
            REQUIRE(jet[4] == 4);
            REQUIRE(jet[5] == -5);

            REQUIRE(jet[6] == approximately(fp_t{6}));
            REQUIRE(jet[7] == approximately(fp_t{8}));
            REQUIRE(jet[8] == approximately(fp_t{-10}));

            REQUIRE(jet[9] == approximately(-fp_t{8}));
            REQUIRE(jet[10] == approximately(fp_t{4}));
            REQUIRE(jet[11] == approximately(fp_t{0}));

            REQUIRE(jet[12] == approximately(jet[9]));
            REQUIRE(jet[13] == approximately(jet[10]));
            REQUIRE(jet[14] == approximately(jet[11]));

            REQUIRE(jet[15] == approximately(-2 * jet[6]));
            REQUIRE(jet[16] == approximately(-2 * jet[7]));
            REQUIRE(jet[17] == approximately(-2 * jet[8]));

            REQUIRE(jet[18] == approximately(1 / fp_t{6} * 4 * jet[15]));
            REQUIRE(jet[19] == approximately(1 / fp_t{6} * 4 * jet[16]));
            REQUIRE(jet[20] == approximately(1 / fp_t{6} * 4 * jet[17]));

            REQUIRE(jet[21] == approximately(-1 / fp_t{6} * 8 * jet[12]));
            REQUIRE(jet[22] == approximately(-1 / fp_t{6} * 8 * jet[13]));
            REQUIRE(jet[23] == approximately(-1 / fp_t{6} * 8 * jet[14]));
        }

        // Variable/variable tests.
        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {x * y, y * x}, 1, 1);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{3}};
            jet.resize(4);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == 3);
            REQUIRE(jet[2] == approximately(fp_t{6}));
            REQUIRE(jet[3] == approximately(fp_t{6}));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {x * y, y * x}, 1, 2);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{1}, fp_t{3}, fp_t{-4}};
            jet.resize(8);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == 1);

            REQUIRE(jet[2] == 3);
            REQUIRE(jet[3] == -4);

            REQUIRE(jet[4] == approximately(fp_t{6}));
            REQUIRE(jet[5] == approximately(-fp_t{4}));

            REQUIRE(jet[6] == approximately(fp_t{6}));
            REQUIRE(jet[7] == approximately(-fp_t{4}));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {x * y, y * x}, 2, 1);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{3}};
            jet.resize(6);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == 3);
            REQUIRE(jet[2] == approximately(fp_t{6}));
            REQUIRE(jet[3] == approximately(fp_t{6}));
            REQUIRE(jet[4] == approximately(fp_t{1} / 2 * (jet[2] * 3 + jet[3] * 2)));
            REQUIRE(jet[5] == approximately(fp_t{1} / 2 * (jet[2] * 3 + jet[3] * 2)));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {x * y, y * x}, 2, 2);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{1}, fp_t{3}, fp_t{-4}};
            jet.resize(12);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == 1);

            REQUIRE(jet[2] == 3);
            REQUIRE(jet[3] == -4);

            REQUIRE(jet[4] == approximately(fp_t{6}));
            REQUIRE(jet[5] == approximately(-fp_t{4}));

            REQUIRE(jet[6] == approximately(fp_t{6}));
            REQUIRE(jet[7] == approximately(-fp_t{4}));

            REQUIRE(jet[8] == approximately(fp_t{1} / 2 * (jet[4] * 3 + jet[6] * 2)));
            REQUIRE(jet[9] == approximately(fp_t{1} / 2 * (jet[5] * -4 + jet[7] * 1)));

            REQUIRE(jet[10] == approximately(fp_t{1} / 2 * (jet[4] * 3 + jet[6] * 2)));
            REQUIRE(jet[11] == approximately(fp_t{1} / 2 * (jet[5] * -4 + jet[7] * 1)));
        }

        {
            llvm_state s{"", 0};

            s.add_taylor_jet_batch<fp_t>("jet", {x * y, y * x}, 3, 3);

            s.compile();

            auto jptr = s.fetch_taylor_jet_batch<fp_t>("jet");

            std::vector<fp_t> jet{fp_t{2}, fp_t{1}, fp_t{3}, fp_t{3}, fp_t{-4}, fp_t{6}};
            jet.resize(24);

            jptr(jet.data());

            REQUIRE(jet[0] == 2);
            REQUIRE(jet[1] == 1);
            REQUIRE(jet[2] == 3);

            REQUIRE(jet[3] == 3);
            REQUIRE(jet[4] == -4);
            REQUIRE(jet[5] == 6);

            REQUIRE(jet[6] == approximately(fp_t{6}));
            REQUIRE(jet[7] == approximately(-fp_t{4}));
            REQUIRE(jet[8] == approximately(fp_t{18}));

            REQUIRE(jet[9] == approximately(fp_t{6}));
            REQUIRE(jet[10] == approximately(-fp_t{4}));
            REQUIRE(jet[11] == approximately(fp_t{18}));

            REQUIRE(jet[12] == approximately(fp_t{1} / 2 * (jet[6] * 3 + jet[9] * 2)));
            REQUIRE(jet[13] == approximately(fp_t{1} / 2 * (jet[7] * -4 + jet[10] * 1)));
            REQUIRE(jet[14] == approximately(fp_t{1} / 2 * (jet[8] * 6 + jet[11] * 3)));

            REQUIRE(jet[15] == approximately(fp_t{1} / 2 * (jet[6] * 3 + jet[9] * 2)));
            REQUIRE(jet[16] == approximately(fp_t{1} / 2 * (jet[7] * -4 + jet[10] * 1)));
            REQUIRE(jet[17] == approximately(fp_t{1} / 2 * (jet[8] * 6 + jet[11] * 3)));

            REQUIRE(jet[18] == approximately(1 / fp_t{6} * (2 * jet[12] * 3 + 2 * jet[6] * jet[9] + 2 * 2 * jet[15])));
            REQUIRE(jet[19]
                    == approximately(1 / fp_t{6} * (2 * jet[13] * -4 + 2 * jet[7] * jet[10] + 2 * 1 * jet[16])));
            REQUIRE(jet[20] == approximately(1 / fp_t{6} * (2 * jet[14] * 6 + 2 * jet[8] * jet[11] + 2 * 3 * jet[17])));

            REQUIRE(jet[21] == approximately(1 / fp_t{6} * (2 * jet[12] * 3 + 2 * jet[6] * jet[9] + 2 * 2 * jet[15])));
            REQUIRE(jet[22]
                    == approximately(1 / fp_t{6} * (2 * jet[13] * -4 + 2 * jet[7] * jet[10] + 2 * 1 * jet[16])));
            REQUIRE(jet[23] == approximately(1 / fp_t{6} * (2 * jet[14] * 6 + 2 * jet[8] * jet[11] + 2 * 3 * jet[17])));
        }
    };

    tuple_for_each(fp_types, tester);
}
