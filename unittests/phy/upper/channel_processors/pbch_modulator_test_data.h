#ifndef SRSGNB_UNITTESTS_PHY_UPPER_CHANNEL_PROCESSORS_PBCH_MODULATOR_TEST_DATA_H
#define SRSGNB_UNITTESTS_PHY_UPPER_CHANNEL_PROCESSORS_PBCH_MODULATOR_TEST_DATA_H

// This file was generated using the following MATLAB scripts:
//   + "srsPBCHmodulatorUnittest.m"

#include "../../resource_grid_test_doubles.h"
#include "srsgnb/adt/complex.h"
#include "srsgnb/phy/upper/channel_processors/pbch_modulator.h"
#include "srsgnb/support/file_vector.h"
#include <array>

namespace srsgnb {

struct test_case_t {
  pbch_modulator::config_t config;
  file_vector<uint8_t> data;
  file_vector<resource_grid_writer_spy::expected_entry_t> symbols;
};

static const std::vector<test_case_t> pbch_modulator_test_data = {
    // clang-format off
  {{223, 0, 0, 0, 1.0, { 0}},{"pbch_modulator_test_input0.dat"},{"pbch_modulator_test_output0.dat"}},
  {{118, 1, 0, 0, 1.0, { 0}},{"pbch_modulator_test_input1.dat"},{"pbch_modulator_test_output1.dat"}},
  {{299, 2, 0, 0, 1.0, { 0}},{"pbch_modulator_test_input2.dat"},{"pbch_modulator_test_output2.dat"}},
  {{321, 3, 0, 0, 1.0, { 0}},{"pbch_modulator_test_input3.dat"},{"pbch_modulator_test_output3.dat"}},
  {{427, 4, 0, 0, 1.0, { 0}},{"pbch_modulator_test_input4.dat"},{"pbch_modulator_test_output4.dat"}},
  {{511, 5, 0, 0, 1.0, { 0}},{"pbch_modulator_test_input5.dat"},{"pbch_modulator_test_output5.dat"}},
  {{86, 6, 0, 0, 1.0, { 0}},{"pbch_modulator_test_input6.dat"},{"pbch_modulator_test_output6.dat"}},
  {{264, 7, 0, 0, 1.0, { 0}},{"pbch_modulator_test_input7.dat"},{"pbch_modulator_test_output7.dat"}},
    // clang-format on
};

} // namespace srsgnb

#endif // SRSGNB_UNITTESTS_PHY_UPPER_CHANNEL_PROCESSORS_PBCH_MODULATOR_TEST_DATA_H
