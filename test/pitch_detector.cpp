/*=============================================================================
   Copyright (c) 2014-2018 Joel de Guzman. All rights reserved.

   Distributed under the MIT License [ https://opensource.org/licenses/MIT ]
=============================================================================*/
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <infra/doctest.hpp>

#include <q/support/literals.hpp>
#include <q/pitch/pitch_detector.hpp>

#include <vector>
#include <iostream>
#include "notes.hpp"

namespace q = cycfi::q;
using namespace q::literals;
using std::fixed;

constexpr auto pi = q::pi;
constexpr auto sps = 44100;

// Set this to 1 or 2 if you want verbose print outs
constexpr auto verbosity = 0;

struct test_result
{
   float ave_error = 0.0f;
   float min_error = 100.0f;
   float max_error = 0.0f;
};

test_result process(
   std::vector<float>&& in
 , q::frequency actual_frequency
 , q::frequency lowest_freq
 , q::frequency highest_freq
 , std::string name = "")
{
   if (verbosity > 1)
      std::cout << fixed << "Actual Frequency: "
      << double(actual_frequency) << std::endl;

   if (name.empty())
      name = std::to_string(int(double(actual_frequency)));

   auto max_val = *std::max_element(in.begin(), in.end(),
      [](auto a, auto b) { return std::abs(a) < std::abs(b); }
   );
   max_val = std::abs(max_val);

   ////////////////////////////////////////////////////////////////////////////
   // Process

   q::pitch_detector    pd(lowest_freq, highest_freq, sps, -60_dB);
   auto                 result = test_result{};
   auto                 frames = 0;

   for (auto i = 0; i != in.size(); ++i)
   {
      auto s = in[i];

      // Period Detection
      bool is_ready = pd(s);

      if (is_ready)
      {
         auto frequency = pd.frequency();
         if (frequency != 0.0f)
         {
            auto error = 1200.0 * std::log2(frequency / double(actual_frequency));
            if (verbosity > 1)
            {
               std::cout
                  << fixed
                  << frequency
                  << " Error: "
                  << error
                  << " cent(s)."
                  << std::endl
               ;
            }

            result.ave_error += std::abs(error);
            ++frames;
            result.min_error = std::min<float>(result.min_error, std::abs(error));
            result.max_error = std::max<float>(result.max_error, std::abs(error));
         }
      }
   }

   result.ave_error /= frames;
   return result;
}

struct params
{
   float _offset = 0.0;          // Waveform offset
   float _2nd_harmonic = 2;      // Second harmonic multiple
   float _3rd_harmonic = 3;      // Second harmonic multiple
   float _1st_level = 0.3;       // Fundamental level
   float _2nd_level = 0.4;       // Second harmonic level
   float _3rd_level = 0.3;       // Third harmonic level
   float _1st_offset = 0.0;      // Fundamental phase offset
   float _2nd_offset = 0.0;      // Second harmonic phase offset
   float _3rd_offset = 0.0;      // Third harmonic phase offset
};

std::vector<float>
gen_harmonics(q::frequency freq, params const& params_)
{
   auto period = double(sps / freq);
   float offset = params_._offset;
   std::size_t buff_size = sps; // 1 second

   std::vector<float> signal(buff_size);
   for (int i = 0; i < buff_size; i++)
   {
      auto angle = (i + offset) / period;
      signal[i] += params_._1st_level
         * std::sin(2 * pi * (angle + params_._1st_offset));
      signal[i] += params_._2nd_level
         * std::sin(params_._2nd_harmonic * 2 * pi * (angle + params_._2nd_offset));
      signal[i] += params_._3rd_level
         * std::sin(params_._3rd_harmonic * 2 * pi * (angle + params_._3rd_offset));
   }
   return signal;
}

void process(
   params const& params_
 , q::frequency actual_frequency
 , q::frequency lowest_freq
 , q::frequency highest_freq
 , float ave_error_expected
 , float min_error_expected
 , float max_error_expected
 , std::string name = ""
)
{
   auto result = process(
      gen_harmonics(actual_frequency, params_)
    , actual_frequency, lowest_freq, highest_freq, name
   );

   if (verbosity > 0)
   {
      std::cout << fixed << "Average Error: " << result.ave_error << " cent(s)." << std::endl;
      std::cout << fixed << "Min Error:     " << result.min_error << " cent(s)." << std::endl;
      std::cout << fixed << "Max Error:     " << result.max_error << " cent(s)." << std::endl;
   }

   CHECK_MESSAGE(result.ave_error < ave_error_expected,
      "Average error exceeded: " << result.ave_error <<
      " expecting: " << ave_error_expected);

   CHECK_MESSAGE(result.min_error < min_error_expected,
      "Minimum error exceeded: " << result.min_error <<
      " expecting: " << min_error_expected);

   CHECK_MESSAGE(result.max_error < max_error_expected,
      "Maximum error exceeded: " << result.max_error <<
      " expecting: " << max_error_expected);
}

void process(
   params const& params_
 , q::frequency actual_frequency
 , q::frequency lowest_freq
 , float ave_error_expected
 , float min_error_expected
 , float max_error_expected
 , std::string name = ""
)
{
   process(
      params_, actual_frequency, lowest_freq * 0.8, lowest_freq * 5
    , ave_error_expected, min_error_expected, max_error_expected, name
   );
}

using namespace notes;

TEST_CASE("Test_middle_C")
{
   process(params{}, middle_c, 200_Hz, 0.0014, 0.000097, 0.0024);
}

TEST_CASE("Test_middle_A")
{
   process(params{}, 440_Hz, 200_Hz, 0.0019, 0.0000001, 0.0073);
}

TEST_CASE("Test_low_E")
{
   process(params{}, low_e, low_e, 0.000035, 000035, 0.000035);
}

TEST_CASE("Test_E_12th")
{
   process(params{}, low_e_12th, low_e, 0.0002, 0.000035, 0.00036);
}

TEST_CASE("Test_E_24th")
{
   process(params{}, low_e_24th, low_e, 0.00022, 0.000035, 0.00052, "low_e_24th");
}

TEST_CASE("Test_A")
{
   process(params{}, a, a, 0.000001, 0.000001, 0.000001);
}

TEST_CASE("Test_A_12th")
{
   process(params{}, a_12th, a, 0.000002, 0.000001, 0.00013);
}

TEST_CASE("Test_A_24th")
{
   process(params{}, a_24th, a, 0.00050, 0.000001, 0.001);
}

TEST_CASE("Test_D")
{
   process(params{}, d, d, 0.00018, 0.000022, 0.00034);
}

TEST_CASE("Test_D_12th")
{
   process(params{}, d_12th, d, 0.00058, 0.000022, 0.0017);
}

TEST_CASE("Test_D_24th")
{
   process(params{}, d_24th, d, 0.0062, 0.0026, 0.011);
}

TEST_CASE("Test_G")
{
   process(params{}, g, g, 0.000061, 0.000061, 0.000061);
}

TEST_CASE("Test_G_12th")
{
   process(params{}, g_12th, g, 0.00018, 0.000061, 0.00021);
}

TEST_CASE("Test_G_24th")
{
   process(params{}, g_24th, g, 0.000089, 0.000061, 0.00034);
}

TEST_CASE("Test_B")
{
   process(params{}, b, b, 0.00061, 0.000003,  0.0015);
}

TEST_CASE("Test_B_12th")
{
   process(params{}, b_12th, b, 0.0080, 0.00033, 0.0125);
}

TEST_CASE("Test_B_24th")
{
   process(params{}, b_24th, b, 0.0043, 0.000003, 0.012);
}

TEST_CASE("Test_high_E")
{
   process(params{}, high_e, high_e, 0.00076, 0.000035, 0.0021);
}

TEST_CASE("Test_high_E_12th")
{
   process(params{}, high_e_12th, high_e, 0.0027, 0.000035, 0.0071);
}

TEST_CASE("Test_high_E_24th")
{
   process(params{}, high_e_24th, high_e, 0.035, 0.0061, 0.045);
}

TEST_CASE("Test_non_integer_harmonics")
{
   params params_;
   params_._2nd_harmonic = 2.003;
   params_._offset = 30000;
   process(params_, low_e, low_e, 1.1, 0.95, 1.1, "non_integer");
}

TEST_CASE("Test_phase_offsets")
{
   params params_;
   params_._1st_offset = 0.1;
   params_._2nd_offset = 0.5;
   params_._3rd_offset = 0.4;
   process(params_, low_e, low_e, 0.00014, 0.000035, 0.00036, "phase_offset");
}

TEST_CASE("Test_missing_fundamental")
{
   params params_;
   params_._1st_level = 0.0;
   params_._2nd_level = 0.5;
   params_._3rd_level = 0.5;
   process(params_, low_e, low_e, 0.000035, 0.000035, 0.000035, "missing_fundamental");
}


