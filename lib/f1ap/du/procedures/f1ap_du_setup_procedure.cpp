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

#include "f1ap_du_setup_procedure.h"
#include "../../f1ap_asn1_utils.h"
#include "../f1ap_du_context.h"
#include "srsran/asn1/f1ap/common.h"
#include "srsran/f1ap/f1ap_message.h"
#include "srsran/ran/band_helper.h"
#include "srsran/support/async/async_timer.h"

using namespace srsran;
using namespace srsran::srs_du;
using namespace asn1::f1ap;

f1ap_du_setup_procedure::f1ap_du_setup_procedure(const f1_setup_request_message& request_,
                                                 f1ap_message_notifier&          cu_notif_,
                                                 f1ap_event_manager&             ev_mng_,
                                                 timer_factory&                  timers,
                                                 f1ap_du_context&                du_ctxt_) :
  request(request_),
  cu_notifier(cu_notif_),
  ev_mng(ev_mng_),
  logger(srslog::fetch_basic_logger("DU-F1")),
  du_ctxt(du_ctxt_),
  f1_setup_wait_timer(timers.create_timer())
{
}

void f1ap_du_setup_procedure::operator()(coro_context<async_task<f1_setup_response_message>>& ctx)
{
  CORO_BEGIN(ctx);

  while (true) {
    transaction = ev_mng.transactions.create_transaction();
    if (not transaction.valid()) {
      CORO_EARLY_RETURN(create_f1_setup_result());
    }

    // Send request to CU.
    send_f1_setup_request();

    // Await CU response.
    CORO_AWAIT(transaction);

    if (not retry_required()) {
      // No more attempts. Exit loop.
      break;
    }

    // Await timer.
    logger.debug("Received F1SetupFailure with Time to Wait IE - reinitiating F1 setup in {}s (retry={}/{})",
                 time_to_wait.count(),
                 f1_setup_retry_no,
                 request.max_setup_retries);
    CORO_AWAIT(
        async_wait_for(f1_setup_wait_timer, std::chrono::duration_cast<std::chrono::milliseconds>(time_to_wait)));
  }

  // Forward procedure result to DU manager.
  CORO_RETURN(create_f1_setup_result());
}

void f1ap_du_setup_procedure::send_f1_setup_request()
{
  // Save the gNB-DU-Id before the F1 Setup is completed for the purpose of logging.
  du_ctxt.du_id = request.gnb_du_id;

  f1ap_message msg = {};
  // set F1AP PDU contents
  msg.pdu.set_init_msg();
  msg.pdu.init_msg().load_info_obj(ASN1_F1AP_ID_F1_SETUP);
  f1_setup_request_s& setup_req = msg.pdu.init_msg().value.f1_setup_request();

  setup_req->transaction_id = transaction.id();

  // DU-global parameters.
  setup_req->gnb_du_id           = static_cast<uint64_t>(request.gnb_du_id);
  setup_req->gnb_du_name_present = not request.gnb_du_name.empty();
  if (setup_req->gnb_du_name_present) {
    setup_req->gnb_du_name.from_string(request.gnb_du_name);
  }
  setup_req->gnb_du_rrc_version.latest_rrc_version.from_number(request.rrc_version);

  setup_req->gnb_du_served_cells_list_present = true;
  setup_req->gnb_du_served_cells_list.resize(request.served_cells.size());
  for (unsigned i = 0; i != request.served_cells.size(); ++i) {
    const f1_cell_setup_params& cell_cfg = request.served_cells[i];
    setup_req->gnb_du_served_cells_list[i].load_info_obj(ASN1_F1AP_ID_GNB_DU_SERVED_CELLS_LIST);
    gnb_du_served_cells_item_s& f1ap_cell = setup_req->gnb_du_served_cells_list[i]->gnb_du_served_cells_item();

    // Fill Served PLMNs
    f1ap_cell.served_cell_info.served_plmns.resize(1);
    auto plmn_bytes                                    = cell_cfg.nr_cgi.plmn_id.to_bytes();
    f1ap_cell.served_cell_info.served_plmns[0].plmn_id = plmn_bytes;

    // Fill slicing information.
    f1ap_cell.served_cell_info.served_plmns[0].ie_exts_present                        = not cell_cfg.slices.empty();
    f1ap_cell.served_cell_info.served_plmns[0].ie_exts.tai_slice_support_list_present = not cell_cfg.slices.empty();
    for (const s_nssai_t& s_nssai : cell_cfg.slices) {
      slice_support_item_s slice{};
      slice.snssai.sst.from_number(s_nssai.sst.value());
      slice.snssai.sd_present = s_nssai.sd.is_set();
      if (slice.snssai.sd_present) {
        slice.snssai.sd.from_number(s_nssai.sd.value());
      }
      f1ap_cell.served_cell_info.served_plmns[0].ie_exts.tai_slice_support_list.push_back(slice);
    }

    // Fill Served Cell Information.
    f1ap_cell.served_cell_info.nr_pci         = cell_cfg.pci;
    f1ap_cell.served_cell_info.nr_cgi.plmn_id = plmn_bytes;
    f1ap_cell.served_cell_info.nr_cgi.nr_cell_id.from_number(cell_cfg.nr_cgi.nci.value());
    f1ap_cell.served_cell_info.five_gs_tac_present = true;
    f1ap_cell.served_cell_info.five_gs_tac.from_number(cell_cfg.tac);
    const unsigned nof_crbs = band_helper::get_n_rbs_from_bw(
        MHz_to_bs_channel_bandwidth(cell_cfg.dl_carrier.carrier_bw_mhz), cell_cfg.scs_common, frequency_range::FR1);
    const double absolute_freq_point_a = band_helper::get_abs_freq_point_a_from_f_ref(
        band_helper::nr_arfcn_to_freq(cell_cfg.dl_carrier.arfcn_f_ref), nof_crbs, cell_cfg.scs_common);
    if (cell_cfg.duplx_mode == duplex_mode::TDD) {
      tdd_info_s& tdd           = f1ap_cell.served_cell_info.nr_mode_info.set_tdd();
      tdd.nr_freq_info.nr_arfcn = band_helper::freq_to_nr_arfcn(absolute_freq_point_a);
      tdd.nr_freq_info.freq_band_list_nr.resize(1);
      tdd.nr_freq_info.freq_band_list_nr[0].freq_band_ind_nr = nr_band_to_uint(cell_cfg.dl_carrier.band);

      tdd.tx_bw.nr_scs.value = (nr_scs_opts::options)to_numerology_value(cell_cfg.scs_common);

      bool res = asn1::number_to_enum(tdd.tx_bw.nr_nrb, nof_crbs);
      srsran_assert(res, "Invalid number of CRBs for DL carrier BW");
    } else {
      fdd_info_s& fdd              = f1ap_cell.served_cell_info.nr_mode_info.set_fdd();
      fdd.dl_nr_freq_info.nr_arfcn = band_helper::freq_to_nr_arfcn(absolute_freq_point_a);
      fdd.dl_nr_freq_info.freq_band_list_nr.resize(1);
      fdd.dl_nr_freq_info.freq_band_list_nr[0].freq_band_ind_nr = nr_band_to_uint(cell_cfg.dl_carrier.band);
      const double ul_absolute_freq_point_a                     = band_helper::get_abs_freq_point_a_from_f_ref(
          band_helper::nr_arfcn_to_freq(cell_cfg.ul_carrier->arfcn_f_ref), nof_crbs, cell_cfg.scs_common);
      fdd.ul_nr_freq_info.nr_arfcn = band_helper::freq_to_nr_arfcn(ul_absolute_freq_point_a);
      fdd.ul_nr_freq_info.freq_band_list_nr.resize(1);
      fdd.ul_nr_freq_info.freq_band_list_nr[0].freq_band_ind_nr = nr_band_to_uint(cell_cfg.ul_carrier->band);

      fdd.dl_tx_bw.nr_scs.value = (nr_scs_opts::options)to_numerology_value(cell_cfg.scs_common);
      unsigned nof_dl_crbs      = band_helper::get_n_rbs_from_bw(
          MHz_to_bs_channel_bandwidth(cell_cfg.dl_carrier.carrier_bw_mhz), cell_cfg.scs_common, frequency_range::FR1);
      bool res = asn1::number_to_enum(fdd.dl_tx_bw.nr_nrb, nof_dl_crbs);
      srsran_assert(res, "Invalid number of CRBs for DL carrier BW");
      fdd.ul_tx_bw.nr_scs.value = (nr_scs_opts::options)to_numerology_value(cell_cfg.scs_common);
      unsigned nof_ul_crbs      = band_helper::get_n_rbs_from_bw(
          MHz_to_bs_channel_bandwidth(cell_cfg.ul_carrier->carrier_bw_mhz), cell_cfg.scs_common, frequency_range::FR1);
      res = asn1::number_to_enum(fdd.ul_tx_bw.nr_nrb, nof_ul_crbs);
      srsran_assert(res, "Invalid number of CRBs for DL carrier BW");
    }
    f1ap_cell.served_cell_info.meas_timing_cfg = cell_cfg.packed_meas_time_cfg.copy();

    // Add System Information related to the cell.
    f1ap_cell.gnb_du_sys_info_present  = true;
    f1ap_cell.gnb_du_sys_info.mib_msg  = cell_cfg.packed_mib.copy();
    f1ap_cell.gnb_du_sys_info.sib1_msg = cell_cfg.packed_sib1.copy();
  }

  // send request
  logger.info("F1 Setup: Sending F1 Setup Request to CU-CP...");
  cu_notifier.on_new_message(msg);
}

bool f1ap_du_setup_procedure::retry_required()
{
  if (transaction.aborted()) {
    // Timeout or cancellation case.
    return false;
  }
  const f1ap_transaction_response& cu_pdu_response = transaction.response();
  if (cu_pdu_response.has_value()) {
    // Success case.
    return false;
  }

  if (cu_pdu_response.error().value.type().value !=
      f1ap_elem_procs_o::unsuccessful_outcome_c::types_opts::f1_setup_fail) {
    // Invalid response type.
    return false;
  }

  const f1_setup_fail_ies_container& f1_setup_fail = *cu_pdu_response.error().value.f1_setup_fail();
  if (not f1_setup_fail.time_to_wait_present) {
    // CU didn't command a waiting time.
    logger.debug("CU-CP did not set any retry waiting time");
    return false;
  }
  if (f1_setup_retry_no++ >= request.max_setup_retries) {
    // Number of retries exceeded, or there is no time to wait.
    logger.error("Reached maximum number of F1 Setup connection retries ({})", request.max_setup_retries);
    return false;
  }

  time_to_wait = std::chrono::seconds{f1_setup_fail.time_to_wait.to_number()};
  return true;
}

f1_setup_response_message f1ap_du_setup_procedure::create_f1_setup_result()
{
  f1_setup_response_message res{};

  if (not transaction.valid()) {
    // Transaction could not be allocated.
    logger.error("{}: Procedure cancelled. Cause: Failed to allocate transaction.", name());
    res.result    = f1_setup_response_message::result_code::proc_failure;
    du_ctxt.du_id = gnb_du_id_t::invalid;
    return res;
  }
  if (transaction.aborted()) {
    // Abortion/timeout case.
    logger.error("{}: Procedure cancelled. Cause: Timeout reached.", name());
    res.result    = f1_setup_response_message::result_code::timeout;
    du_ctxt.du_id = gnb_du_id_t::invalid;
    return res;
  }
  const f1ap_transaction_response& cu_pdu_response = transaction.response();

  if (cu_pdu_response.has_value() and cu_pdu_response.value().value.type().value ==
                                          f1ap_elem_procs_o::successful_outcome_c::types_opts::f1_setup_resp) {
    res.result = f1_setup_response_message::result_code::success;

    // Update F1 DU Context (taking values from request).
    du_ctxt.du_id       = request.gnb_du_id;
    du_ctxt.gnb_du_name = request.gnb_du_name;
    du_ctxt.served_cells.resize(request.served_cells.size());
    for (unsigned i = 0; i != du_ctxt.served_cells.size(); ++i) {
      du_ctxt.served_cells[i].nr_cgi = request.served_cells[i].nr_cgi;
    }

    logger.info("{}: Procedure completed successfully.", name());

  } else if (cu_pdu_response.has_value() and cu_pdu_response.error().value.type().value !=
                                                 f1ap_elem_procs_o::unsuccessful_outcome_c::types_opts::f1_setup_fail) {
    logger.error(
        "{}: Received PDU with unexpected PDU type {}", name(), cu_pdu_response.value().value.type().to_string());
    res.result    = f1_setup_response_message::result_code::invalid_response;
    du_ctxt.du_id = gnb_du_id_t::invalid;
  } else {
    const auto& fail           = *cu_pdu_response.error().value.f1_setup_fail();
    res.result                 = f1_setup_response_message::result_code::f1_setup_failure;
    res.f1_setup_failure_cause = get_cause_str(fail.cause);
    logger.debug("{}: F1 Setup Failure with cause \"{}\"", name(), get_cause_str(fail.cause));
    du_ctxt.du_id = gnb_du_id_t::invalid;
  }
  return res;
}
