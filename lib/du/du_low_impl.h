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

#include "srsran/du/du_low_config.h"
#include "srsran/phy/upper/upper_phy.h"

namespace srsran {

class du_low_impl final : public upper_phy
{
public:
  du_low_impl(const du_low_configuration& du_low_cfg);

  // See interface for documentation.
  upper_phy_error_handler& get_error_handler() override { return phy_up->get_error_handler(); }

  // See interface for documentation.
  upper_phy_rx_symbol_handler& get_rx_symbol_handler() override { return phy_up->get_rx_symbol_handler(); }

  // See interface for documentation.
  upper_phy_timing_handler& get_timing_handler() override { return phy_up->get_timing_handler(); }

  // See interface for documentation.
  downlink_processor_pool& get_downlink_processor_pool() override { return phy_up->get_downlink_processor_pool(); }

  // See interface for documentation.
  resource_grid_pool& get_downlink_resource_grid_pool() override { return phy_up->get_downlink_resource_grid_pool(); }

  // See interface for documentation.
  resource_grid_pool& get_uplink_resource_grid_pool() override { return phy_up->get_uplink_resource_grid_pool(); }

  // See interface for documentation.
  uplink_request_processor& get_uplink_request_processor() override { return phy_up->get_uplink_request_processor(); }

  // See interface for documentation.
  uplink_slot_pdu_repository& get_uplink_slot_pdu_repository() override
  {
    return phy_up->get_uplink_slot_pdu_repository();
  }

  // See interface for documentation.
  const downlink_pdu_validator& get_downlink_pdu_validator() const override
  {
    return phy_up->get_downlink_pdu_validator();
  }

  // See interface for documentation.
  const uplink_pdu_validator& get_uplink_pdu_validator() const override { return phy_up->get_uplink_pdu_validator(); }

  // See interface for documentation.
  void set_error_notifier(upper_phy_error_notifier& notifier) override { return phy_up->set_error_notifier(notifier); }

  // See interface for documentation.
  void set_timing_notifier(upper_phy_timing_notifier& notifier) override
  {
    return phy_up->set_timing_notifier(notifier);
  }

  // See interface for documentation.
  void set_rx_results_notifier(upper_phy_rx_results_notifier& notifier) override
  {
    return phy_up->set_rx_results_notifier(notifier);
  }

  // See interface for documentation.
  void stop() override { phy_up->stop(); }

private:
  srslog::basic_logger& logger;

  std::unique_ptr<upper_phy> phy_up;
};

} // namespace srsran
