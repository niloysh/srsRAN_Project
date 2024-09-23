/*
 *
 * Copyright 2021-2024 Software Radio Systems Limited
 *
 * By using this file, you agree to the terms and conditions set
 * forth in the LICENSE file which can be found at the top level of
 * the distribution.
 *
 */

#include "config_generators.h"
#include "lib/scheduler/logging/scheduler_metrics_ue_configurator.h"

using namespace srsran;
using namespace test_helpers;

namespace {

class dummy_sched_configuration_notifier : public sched_configuration_notifier
{
public:
  void on_ue_config_complete(du_ue_index_t ue_index, bool ue_creation_result) override {}
  void on_ue_delete_response(du_ue_index_t ue_index) override {}
};

class dummy_scheduler_ue_metrics_notifier : public scheduler_metrics_notifier
{
public:
  void report_metrics(const scheduler_cell_metrics& metrics) override {}
};

class dummy_sched_metrics_ue_configurator : public sched_metrics_ue_configurator
{
public:
  void handle_ue_creation(du_ue_index_t ue_index, rnti_t rnti, pci_t pcell_pci, unsigned num_prbs) override {}
  void handle_ue_reconfiguration(du_ue_index_t ue_index) override {}
  void handle_ue_deletion(du_ue_index_t ue_index) override {}
};

} // namespace

test_sched_config_manager::test_sched_config_manager(const cell_config_builder_params& builder_params_,
                                                     const scheduler_expert_config&    expert_cfg_) :
  builder_params(builder_params_),
  expert_cfg(expert_cfg_),
  cfg_notifier(std::make_unique<dummy_sched_configuration_notifier>()),
  metric_notifier(std::make_unique<dummy_scheduler_ue_metrics_notifier>()),
  ue_metrics_configurator(std::make_unique<dummy_sched_metrics_ue_configurator>()),
  metrics_handler(std::chrono::milliseconds{1000}, *metric_notifier),
  cfg_mng(scheduler_config{expert_cfg, *cfg_notifier, *metric_notifier}, metrics_handler)
{
  default_cell_req = test_helpers::make_default_sched_cell_configuration_request(builder_params);
  default_ue_req   = test_helpers::create_default_sched_ue_creation_request(builder_params, {lcid_t::LCID_MIN_DRB});
}

test_sched_config_manager::~test_sched_config_manager() {}

const cell_configuration* test_sched_config_manager::add_cell(const sched_cell_configuration_request_message& msg)
{
  return cfg_mng.add_cell(msg);
}

const ue_configuration* test_sched_config_manager::add_ue(const sched_ue_creation_request_message& cfg_req)
{
  auto ue_ev = cfg_mng.add_ue(cfg_req);
  if (ue_ev.valid()) {
    const ue_configuration* ret = &ue_ev.next_config();
    ue_ev.notify_completion();
    return ret;
  }
  return nullptr;
}

bool test_sched_config_manager::rem_ue(du_ue_index_t ue_index)
{
  auto ev = cfg_mng.remove_ue(ue_index);
  return ev.valid();
}
