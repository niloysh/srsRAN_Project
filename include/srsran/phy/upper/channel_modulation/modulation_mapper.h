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

#include "srsran/adt/bit_buffer.h"
#include "srsran/adt/complex.h"
#include "srsran/adt/span.h"
#include "srsran/ran/sch/modulation_scheme.h"

namespace srsran {

/// \brief Modulation mapper public interface.
///
/// Maps bits to complex symbols as specified in TS38.211 Section 5.1.
class modulation_mapper
{
public:
  /// Gets the modulation scaling factor.
  static float get_modulation_scaling(modulation_scheme);

  /// Default destructor.
  virtual ~modulation_mapper() = default;

  /// \brief Maps a sequence of bits to a sequence of floating point complex symbols according to the given modulation
  /// scheme.
  ///
  /// \param[in]  input   The sequence of bits. Each entry corresponds to a single bit.
  /// \param[out] symbols The resulting sequence of complex-valued symbols.
  /// \param[in]  scheme  The modulation scheme determining the mapping.
  /// \remark The length of \c input should be equal to the length of \c symbols times the modulation order (that is,
  /// the number of bits per modulated symbol).
  virtual void modulate(span<cf_t> symbols, const bit_buffer& input, modulation_scheme scheme) = 0;

  /// \brief Maps a sequence of bits to a sequence of 8-bit integer complex symbols according to the given modulation
  /// scheme.
  ///
  /// \param[in]  input   The sequence of bits. Each entry corresponds to a single bit.
  /// \param[out] symbols The resulting sequence of complex-valued symbols.
  /// \param[in]  scheme  The modulation scheme determining the mapping.
  /// \remark The length of \c input should be equal to the length of \c symbols times the modulation order (that is,
  /// the number of bits per modulated symbol).
  /// \return The scaling factor applied to the mapped symbols.
  virtual float modulate(span<ci8_t> symbols, const bit_buffer& input, modulation_scheme scheme) = 0;
};

} // namespace srsran
