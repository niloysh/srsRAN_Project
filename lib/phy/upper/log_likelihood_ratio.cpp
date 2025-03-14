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

#include "srsran/phy/upper/log_likelihood_ratio.h"
#include "srsran/srsvec/compare.h"
#include <cmath>
#include <optional>

#ifdef __AVX2__
#include <immintrin.h>
#endif // __AVX2__
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif // __ARM_NEON

using namespace srsran;

/// Computes the sum when at least one of the summands is plus/minus infinity.
/// Note that also the indeterminate case +LLR_INFTY + (-LLR_INFTY) is set to zero.
static std::optional<log_likelihood_ratio> tackle_special_sums(log_likelihood_ratio val_a, log_likelihood_ratio val_b)
{
  if (val_a == -val_b) {
    return log_likelihood_ratio(0);
  }

  // When at least one of the summands is infinity, the sum is also infinity (with sign).
  // The indeterminate case LLR_INFTY + (-LLR_INFTY) has already been dealt with in the previous case.
  if (log_likelihood_ratio::isinf(val_a)) {
    return val_a;
  }
  if (log_likelihood_ratio::isinf(val_b)) {
    return val_b;
  }

  return std::nullopt;
}

log_likelihood_ratio log_likelihood_ratio::operator+=(log_likelihood_ratio rhs)
{
  if (auto special = tackle_special_sums(*this, rhs)) {
    *this = special.value();
    return *this;
  }

  // When not dealing with special cases, classic saturated sum.
  int tmp = value + static_cast<int>(rhs.value);
  if (std::abs(tmp) > LLR_MAX) {
    *this = ((tmp > 0) ? LLR_MAX : -LLR_MAX);
    return *this;
  }
  *this = tmp;
  return *this;
}

log_likelihood_ratio log_likelihood_ratio::promotion_sum(log_likelihood_ratio a, log_likelihood_ratio b)
{
  if (auto special = tackle_special_sums(a, b)) {
    return special.value();
  }

  // When not dealing with special cases, promotion sum: if the sum exceeds LLR_MAX (in absolute value), then return
  // LLR_INFTY (with the proper sign).
  int tmp = a.value + static_cast<int>(b.value);
  if (std::abs(tmp) > log_likelihood_ratio::LLR_MAX) {
    return ((tmp > 0) ? log_likelihood_ratio::LLR_INFTY : -log_likelihood_ratio::LLR_INFTY);
  }
  return tmp;
}

log_likelihood_ratio log_likelihood_ratio::quantize(float value, float range_limit)
{
  srsran_assert(range_limit > 0, "Second input must be positive.");

  float clipped = value;
  if (std::abs(value) > range_limit) {
    clipped = std::copysign(range_limit, value);
  }
  return static_cast<value_type>(std::round(clipped / range_limit * LLR_MAX));
}

#ifdef __AVX2__
// Hard decision function with bit packing.
static void hard_decision_simd(bit_buffer& hard_bits, const int8_t* soft_bits, unsigned len)
{
  // Number of bits processed on each loop cycle.
  static constexpr unsigned AVX2_B_SIZE = 32;

  unsigned i_bit = 0;

  // Destination buffer.
  span<uint8_t> packed_buffer = hard_bits.get_buffer();

  for (unsigned max_bit = (len / AVX2_B_SIZE) * AVX2_B_SIZE; i_bit != max_bit; i_bit += AVX2_B_SIZE) {
    // Load AVX2_B_SIZE LLRs.
    __m256i soft_epi8 = _mm256_loadu_si256(reinterpret_cast<const __m256i*>(soft_bits + i_bit));

    // Shuffle soft_epi8: the soft bits are taken in groups of 8 and, inside each group, we reverse their order (this is
    // because, once we convert the soft bits into hard bits, the hard bits forming a byte need to be reversed before
    // being stored in packed format).
    // Remark: Recall that _mm256_set_epi8 takes as inputs (c31, c30, ..., c1, c0), which means that the control values
    // associated with the bytes of soft_epi8 (as the output of the shuffle operations) must be read down up.
    soft_epi8 = _mm256_shuffle_epi8(soft_epi8,
                                    _mm256_set_epi8(8,
                                                    9,
                                                    10,
                                                    11,
                                                    12,
                                                    13,
                                                    14,
                                                    15,
                                                    0,
                                                    1,
                                                    2,
                                                    3,
                                                    4,
                                                    5,
                                                    6,
                                                    7,
                                                    8,
                                                    9,
                                                    10,
                                                    11,
                                                    12,
                                                    13,
                                                    14,
                                                    15,
                                                    0,
                                                    1,
                                                    2,
                                                    3,
                                                    4,
                                                    5,
                                                    6,
                                                    7));

    // To obtain 32 packed hard bits from soft_epi8, we first compare with 1 - for each LLR, all the bits of the
    // corresponding output byte are set to 0 if the LLR is positive (that is, 1 > LLR is false), and are set to 1 if
    // the LLR is negative or null (that is, 1 > LLR is true). Finally, it suffices to pick the MSB of all bytes.
    soft_epi8      = _mm256_cmpgt_epi8(_mm256_set1_epi8(1), soft_epi8);
    uint32_t bytes = _mm256_movemask_epi8(soft_epi8);

    // Write the packed bits into 4 bytes of the internal buffer.
    std::memcpy(packed_buffer.begin(), &bytes, sizeof(uint32_t));

    // Advance buffer.
    packed_buffer = packed_buffer.last(packed_buffer.size() - (AVX2_B_SIZE / 8));
  }

  for (; i_bit != len; ++i_bit) {
    uint8_t hard_bit = (soft_bits[i_bit] <= 0) ? 1 : 0;

    hard_bits.insert(hard_bit, i_bit, 1);
  }
}
#endif // __AVX2__

#ifdef __ARM_NEON
static void hard_decision_simd(bit_buffer& hard_bits, const int8_t* soft_bits, unsigned len)
{
  const uint8x16_t mask_msb_u8    = vdupq_n_u8(0x80);
  const int8_t     shift_mask[16] = {-7, -6, -5, -4, -3, -2, -1, 0, -7, -6, -5, -4, -3, -2, -1, 0};
  const int8x16_t  shift_mask_s8 =
      vcombine_s8(vcreate_s8(*((uint64_t*)shift_mask)), vcreate_s8(*((uint64_t*)&shift_mask[8])));

  // Number of bits processed on each loop cycle.
  static constexpr unsigned NEON_B_SIZE = 16;

  unsigned i_bit = 0;

  // Destination buffer.
  span<uint8_t> packed_buffer = hard_bits.get_buffer();

  for (; i_bit != (len / NEON_B_SIZE) * NEON_B_SIZE; i_bit += NEON_B_SIZE) {
    // Read soft bits.
    uint8x16_t soft_bits_u8 = vld1q_u8(reinterpret_cast<const uint8_t*>(&soft_bits[i_bit]));

    // Replace 0-valued soft bits with -1.
    uint8x16_t zero_mask = vceqq_u8(soft_bits_u8, vdupq_n_u8(0));
    soft_bits_u8         = vbslq_u8(zero_mask, vdupq_n_u8(255), soft_bits_u8);

    // Reverse 8 bytes in every double-word, MSBs of each byte will form a mask.
    soft_bits_u8 = vrev64q_u8(soft_bits_u8);

    // Generate masks of MSB bits shifted to appropriate position.
    uint8x16_t msb_bits_u8 = vandq_u8(soft_bits_u8, mask_msb_u8);
    uint8x16_t mask_u8     = vshlq_u8(msb_bits_u8, shift_mask_s8);

    // Obtain 16 packed hard bits from the mask.
    mask_u8 = vpaddq_u8(mask_u8, mask_u8);
    mask_u8 = vpaddq_u8(mask_u8, mask_u8);
    mask_u8 = vpaddq_u8(mask_u8, mask_u8);

    // Write the packed bits into 2 bytes of the internal buffer.
    vst1q_lane_u16(reinterpret_cast<uint16_t*>(packed_buffer.begin()), vreinterpretq_u16_u8(mask_u8), 0);

    // Advance buffer.
    packed_buffer = packed_buffer.last(packed_buffer.size() - (NEON_B_SIZE / 8));
  }

  for (; i_bit != len; ++i_bit) {
    uint8_t hard_bit = (soft_bits[i_bit] <= 0) ? 1 : 0;

    hard_bits.insert(hard_bit, i_bit, 1);
  }
}
#endif // __ARM_NEON

void srsran::clamp(span<log_likelihood_ratio>       out,
                   span<const log_likelihood_ratio> in,
                   log_likelihood_ratio             low,
                   log_likelihood_ratio             high)
{
  srsran_assert(out.size() == in.size(),
                "Input size (i.e., {}) is not equal to the output size (i.e., {}).",
                in.size(),
                out.size());
  unsigned len = in.size();
  unsigned i   = 0;

#if defined(__AVX512F__) && defined(__AVX512BW__)
  const __m512i* in_ptr  = reinterpret_cast<const __m512i*>(in.data());
  __m512i*       out_ptr = reinterpret_cast<__m512i*>(out.data());

  __m512i low_epi8  = _mm512_set1_epi8(low.to_int());
  __m512i high_epi8 = _mm512_set1_epi8(high.to_int());

  // Clamping function.
  const auto clamp_func = [&low_epi8, &high_epi8](__m512i in_epi8) {
    __m512i temp = in_epi8;

    __mmask64 high_mask = _mm512_cmp_epi8_mask(temp, high_epi8, _MM_CMPINT_GT);
    temp                = _mm512_mask_blend_epi8(high_mask, temp, high_epi8);

    __mmask64 low_mask = _mm512_cmp_epi8_mask(low_epi8, temp, _MM_CMPINT_GT);
    temp               = _mm512_mask_blend_epi8(low_mask, temp, low_epi8);

    return temp;
  };

  // Clamps 64 LLR at a time.
  for (unsigned len_simd = (len / 64) * 64; i != len_simd; i += 64) {
    _mm512_storeu_si512(out_ptr++, clamp_func(_mm512_loadu_si512(in_ptr++)));
  }

  // Clamps the rest.
  __mmask64 remainder_mask = (1UL << (len % 64UL)) - 1UL;
  _mm512_mask_storeu_epi8(out_ptr, remainder_mask, clamp_func(_mm512_maskz_loadu_epi8(remainder_mask, in_ptr)));
#else // defined(__AVX512F__) && defined(__AVX512BW__)
#ifdef __AVX2__
  const __m256i* in_ptr  = reinterpret_cast<const __m256i*>(in.data());
  __m256i*       out_ptr = reinterpret_cast<__m256i*>(out.data());

  __m256i low_epi8  = _mm256_set1_epi8(low.to_int());
  __m256i high_epi8 = _mm256_set1_epi8(high.to_int());

  for (unsigned len_simd = (len / 32) * 32; i != len_simd; i += 32) {
    __m256i temp = _mm256_loadu_si256(in_ptr++);

    __m256i high_mask = _mm256_cmpgt_epi8(temp, high_epi8);
    temp              = _mm256_blendv_epi8(temp, high_epi8, high_mask);

    __m256i low_mask = _mm256_cmpgt_epi8(low_epi8, temp);
    temp             = _mm256_blendv_epi8(temp, low_epi8, low_mask);

    _mm256_storeu_si256(out_ptr++, temp);
  }
#endif // __AVX2__
#ifdef __ARM_NEON
  int8x16_t low_s8  = vdupq_n_s8(static_cast<int8_t>(low.to_int()));
  int8x16_t high_s8 = vdupq_n_s8(static_cast<int8_t>(high.to_int()));

  const int8_t* in_ptr  = reinterpret_cast<const int8_t*>(in.data());
  int8_t*       out_ptr = reinterpret_cast<int8_t*>(out.data());

  for (unsigned len_simd = (len / 16) * 16; i != len_simd; i += 16) {
    int8x16_t temp = vld1q_s8(in_ptr + i);

    uint8x16_t high_mask = vcgtq_s8(temp, high_s8);
    temp                 = vbslq_s8(high_mask, high_s8, temp);

    uint8x16_t low_mask = vcltq_s8(temp, low_s8);
    temp                = vbslq_s8(low_mask, low_s8, temp);

    vst1q_s8(out_ptr + i, temp);
  }
#endif // __ARM_NEON

  for (; i != len; ++i) {
    out[i] = std::clamp(in[i], low, high);
  }
#endif // defined(__AVX512F__) && defined(__AVX512BW__)
}

bool srsran::hard_decision(bit_buffer& hard_bits, span<const log_likelihood_ratio> soft_bits)
{
  // Make sure that there is enough space in the output to accommodate the hard bits.
  srsran_assert(soft_bits.size() <= hard_bits.size(),
                "Input size (i.e., {}) does not fit into the output buffer with size {}",
                soft_bits.size(),
                hard_bits.size());

  unsigned nof_bits = soft_bits.size();

#if defined(__AVX2__) || defined(__ARM_NEON)

  hard_decision_simd(hard_bits, reinterpret_cast<const int8_t*>(soft_bits.data()), nof_bits);

#else
  for (unsigned i_bit = 0; i_bit != nof_bits; ++i_bit) {
    // Compute hard bit.
    uint8_t hard_bit = soft_bits[i_bit].to_hard_bit();

    // Insert into the bit buffer.
    hard_bits.insert(hard_bit, i_bit, 1);
  }
#endif // __AVX2__ or __ARM_NEON

  // Return false if it finds a zero in the soft bits.
  return srsvec::find(soft_bits, log_likelihood_ratio(0)) == soft_bits.end();
}
