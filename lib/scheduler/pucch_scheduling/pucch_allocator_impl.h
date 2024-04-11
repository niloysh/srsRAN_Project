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

#include "../cell/resource_grid.h"
#include "../config/ue_configuration.h"
#include "pucch_allocator.h"
#include "pucch_resource_manager.h"
#include "srsran/scheduler/scheduler_dci.h"

namespace srsran {

/// Implementation of the PUCCH allocator interface.
class pucch_allocator_impl final : public pucch_allocator
{
public:
  explicit pucch_allocator_impl(const cell_configuration& cell_cfg_,
                                unsigned                  max_pucchs_per_slot,
                                unsigned                  max_ul_grants_per_slot_);

  ~pucch_allocator_impl() override;

  /// Updates the internal slot_point and tracking of PUCCH resource usage; and resets the PUCCH common allocation grid.
  void slot_indication(slot_point sl_tx) override;

  optional<unsigned> alloc_common_pucch_harq_ack_ue(cell_resource_allocator&    res_alloc,
                                                    rnti_t                      tcrnti,
                                                    unsigned                    k0,
                                                    unsigned                    k1,
                                                    const pdcch_dl_information& dci_info) override;

  optional<unsigned> alloc_common_and_ded_harq_res(cell_resource_allocator&     res_alloc,
                                                   rnti_t                       rnti,
                                                   const ue_cell_configuration& ue_cell_cfg,
                                                   unsigned                     k0,
                                                   unsigned                     k1,
                                                   const pdcch_dl_information&  dci_info) override;

  optional<unsigned> alloc_ded_pucch_harq_ack_ue(cell_resource_allocator&     res_alloc,
                                                 rnti_t                       crnti,
                                                 const ue_cell_configuration& ue_cell_cfg,
                                                 unsigned                     k0,
                                                 unsigned                     k1) override;

  void pucch_allocate_sr_opportunity(cell_slot_resource_allocator& slot_alloc,
                                     rnti_t                        crnti,
                                     const ue_cell_configuration&  ue_cell_cfg) override;

  void pucch_allocate_csi_opportunity(cell_slot_resource_allocator& pucch_slot_alloc,
                                      rnti_t                        crnti,
                                      const ue_cell_configuration&  ue_cell_cfg,
                                      unsigned                      csi_part1_nof_bits) override;

  pucch_uci_bits remove_ue_uci_from_pucch(cell_slot_resource_allocator& slot_alloc,
                                          rnti_t                        crnti,
                                          const ue_cell_configuration&  ue_cell_cfg) override;

  bool has_common_pucch_f1_grant(rnti_t rnti, slot_point sl_tx) const override;

private:
  // Structs with the info about the PUCCH resources.
  struct pucch_res_alloc_cfg {
    // True if the struct has a valid config.
    unsigned   pucch_res_indicator;
    grant_info first_hop_res;
    // Contains grant only if intra-slot freq-hopping is active.
    grant_info second_hop_res;
    // Cyclic-shift.
    unsigned cs;
    // PUCCH format.
    pucch_format format;
  };

  // Contains the existing PUCCH grants currently allocated to a given UE.
  struct existing_pucch_grants {
    pucch_info* format1_sr_grant{nullptr};
    pucch_info* format1_harq_grant{nullptr};
    pucch_info* format1_harq_common_grant{nullptr};
    pucch_info* format2_grant{nullptr};
  };

  // Allocates the PUCCH (common) resource for HARQ-(N)-ACK.
  optional<pucch_res_alloc_cfg> alloc_pucch_common_res_harq(cell_slot_resource_allocator&  pucch_alloc,
                                                            const dci_context_information& dci_info);

  optional<unsigned> exec_common_and_ded_res_alloc(cell_slot_resource_allocator& pucch_alloc,
                                                   pucch_info*                   existing_grant,
                                                   rnti_t                        rnti,
                                                   const ue_cell_configuration&  ue_cell_cfg,
                                                   pucch_res_alloc_cfg           common_res_cfg,
                                                   const pucch_resource&         ded_res_cfg);

  // Helper that allocates a NEW PUCCH HARQ grant (Format 1).
  optional<unsigned> allocate_new_format1_harq_grant(cell_slot_resource_allocator& pucch_slot_alloc,
                                                     rnti_t                        crnti,
                                                     const ue_cell_configuration&  ue_cell_cfg,
                                                     pucch_info*                   existing_sr_grant);

  // Helper that add an HARQ-ACK bit to existing PUCCH HARQ grant (Format 1).
  optional<unsigned> add_harq_ack_bit_to_format1_grant(pucch_info&         existing_harq_grant,
                                                       pucch_info*         existing_sr_grant,
                                                       rnti_t              rnti,
                                                       slot_point          sl_tx,
                                                       const pucch_config& pucch_cfg);

  // Helper that allocates a new PUCCH HARQ grant (Format 2) for CSI.
  void allocate_new_csi_grant(cell_slot_resource_allocator& pucch_slot_alloc,
                              rnti_t                        crnti,
                              const ue_cell_configuration&  ue_cell_cfg,
                              unsigned                      csi_part1_bits);

  // Helper that replaces PUCCH grant Format 1 with Format 2 grant for CSI reporting.
  void convert_to_format2_csi(cell_slot_resource_allocator& pucch_slot_alloc,
                              pucch_info&                   existing_sr_grant,
                              rnti_t                        rnti,
                              const ue_cell_configuration&  ue_cell_cfg,
                              unsigned                      csi_part1_nof_bits);

  // Helper that replaces PUCCH grant Format 1 with Format 2 grant for HARQ-ACK reporting.
  optional<unsigned> convert_to_format2_harq(cell_slot_resource_allocator& pucch_slot_alloc,
                                             pucch_info&                   existing_harq_grant,
                                             pucch_info*                   existing_sr_grant,
                                             rnti_t                        rnti,
                                             const ue_cell_configuration&  ue_cell_cfg,
                                             unsigned                      harq_ack_bits_increment);

  // Helper that changes the current PUCCH Format 2 grant (specifically used for CSI reporting) into a PUCCH Format 2
  // resource for the HARQ-ACK + CSI.
  optional<unsigned> change_format2_resource(cell_slot_resource_allocator&              pucch_slot_alloc,
                                             pucch_info&                                existing_grant,
                                             rnti_t                                     rnti,
                                             const ue_cell_configuration&               ue_cell_cfg,
                                             unsigned                                   harq_ack_bits_increment,
                                             optional<pucch_harq_resource_alloc_record> harq_f2_res);

  // Helper that adds HARQ-ACK bits to a PUCCH Format 2 grant for HARQ-ACK.
  optional<unsigned> add_harq_bits_to_harq_f2_grant(pucch_info&                  existing_f2_grant,
                                                    slot_point                   sl_tx,
                                                    rnti_t                       crnti,
                                                    const ue_cell_configuration& ue_cell_cfg,
                                                    unsigned                     harq_ack_bits_increment);

  struct pucch_com_ded_res {
    pucch_res_alloc_cfg   pucch_common_info;
    const pucch_resource& pucch_ded_cfg;
  };

  optional<pucch_com_ded_res> find_common_and_ded_harq_res_available(cell_slot_resource_allocator&  pucch_alloc,
                                                                     pucch_info*                    existing_grant,
                                                                     rnti_t                         rnti,
                                                                     const ue_cell_configuration&   ue_cell_cfg,
                                                                     const dci_context_information& dci_info);

  // Helper that removes the existing PUCCH Format 1 grants (both HARQ-ACK and SR).
  void remove_pucch_format1_from_grants(cell_slot_resource_allocator& slot_alloc,
                                        rnti_t                        crnti,
                                        const pucch_config&           pucch_cfg);

  // Helper that removes the existing PUCCH Format 2 grant for CSI.
  void remove_format2_csi_from_grants(cell_slot_resource_allocator& slot_alloc,
                                      rnti_t                        crnti,
                                      const ue_cell_configuration&  ue_cell_cfg);

  // Fills the PUCCH HARQ grant for common resources.
  void fill_pucch_harq_common_grant(pucch_info& pucch_info, rnti_t rnti, const pucch_res_alloc_cfg& pucch_res);

  // Fills the PUCCH Format 1 grant.
  void fill_pucch_ded_format1_grant(pucch_info&           pucch_grant,
                                    rnti_t                crnti,
                                    const pucch_resource& pucch_ded_res_cfg,
                                    unsigned              harq_ack_bits,
                                    sr_nof_bits           sr_bits);

  // Fills the PUCCH Format 2 grant.
  void fill_pucch_format2_grant(pucch_info&                  pucch_grant,
                                rnti_t                       crnti,
                                const pucch_resource&        pucch_ded_res_cfg,
                                const ue_cell_configuration& ue_cell_cfg,
                                unsigned                     nof_prbs,
                                unsigned                     harq_ack_bits,
                                sr_nof_bits                  sr_bits,
                                unsigned                     csi_part1_bits);

  // Helper that retrieves the existing grants allocated to a given UE for a given slot.
  existing_pucch_grants
  get_existing_pucch_grants(static_vector<pucch_info, MAX_PUCCH_PDUS_PER_SLOT>& pucchs, rnti_t rnti, slot_point sl_ack);

  unsigned get_max_pucch_grants(unsigned currently_allocated_puschs);

  using slot_alloc_list = static_vector<rnti_t, MAX_PUCCH_PDUS_PER_SLOT>;

  // \brief Ring of PUCCH allocations indexed by slot.
  circular_array<slot_alloc_list, cell_resource_allocator::RING_ALLOCATOR_SIZE> pucch_common_alloc_grid;

  const unsigned            PUCCH_FORMAT_1_NOF_PRBS{1};
  const cell_configuration& cell_cfg;
  const unsigned            max_pucch_grants_per_slot;
  const unsigned            max_ul_grants_per_slot;
  slot_point                last_sl_ind;
  pucch_resource_manager    resource_manager;

  srslog::basic_logger& logger;
};

} // namespace srsran
