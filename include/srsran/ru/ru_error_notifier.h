/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#pragma once

#include "srsran/ran/slot_point.h"

namespace srsran {

/// Radio Unit error context.
struct ru_error_context {
  /// Slot context.
  slot_point slot;
  /// Radio sector identifier.
  unsigned sector;
};

/// Radio Unit interface error notifier.
class ru_error_notifier
{
public:
  /// Default destructor.
  virtual ~ru_error_notifier() = default;

  /// \brief Notifies a late downlink message.
  ///
  /// \param[in] context Context of the error.
  virtual void on_late_downlink_message(const ru_error_context& context) = 0;
};

} // namespace srsran
