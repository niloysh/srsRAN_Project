/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * This file is part of srsRAN.
 *
 * srsRAN is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of
 * the License, or (at your option) any later version.
 *
 * srsRAN is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * A copy of the GNU Affero General Public License can be found in
 * the LICENSE file in the top-level directory of this distribution
 * and at http://www.gnu.org/licenses/.
 *
 */

#pragma once

#include "srsran/du/du_wrapper.h"
#include "srsran/du/du_wrapper_config.h"
#include <memory>

namespace srsran {
namespace srs_du {

/// Instantiates a Distributed Unit (DU) wrapper object with the given configuration and dependencies.
std::unique_ptr<du_wrapper> make_du_wrapper(const du_wrapper_config& du_cfg, du_wrapper_dependencies&& dependencies);

} // namespace srs_du
} // namespace srsran
