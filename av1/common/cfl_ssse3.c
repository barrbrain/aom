/*
 * Copyright (c) 2017, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#include <tmmintrin.h>

#include "./av1_rtcd.h"

#include "av1/common/cfl.h"

/**
 * Adds 4 pixels (in a 2x2 grid) and multiplies them by 2. Resulting in a more
 * precise version of a box filter 4:2:0 pixel subsampling in Q3.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 *
 * Note: For 4:2:0 luma subsampling, the width will never be greater than 16.
 */
static void cfl_luma_subsampling_420_lbd_ssse3(const uint8_t *input,
                                               int input_stride,
                                               int16_t *pred_buf_q3, int width,
                                               int height) {
  const __m128i twos = _mm_set1_epi8(2);  // Sixteen twos

  // Sixteen int8 values fit in one __m128i register. If this is enough to do
  // the entire row, the next value is two rows down, otherwise we move to the
  // next sixteen values.
  //   width   next
  //     4      64
  //     8      64
  //    16      16
  const int next = 64 >> ((width == 16) << 1);

  // Values in the prediction buffer are subsample. so there are half of them
  const int next_chroma = next >> 1;

  // When the width is less than 16, we double the stride, because we process
  // four lines by iteration (instead of two).
  const int luma_stride = input_stride << (1 + (width < 16));
  const int chroma_stride = CFL_BUF_LINE << (width < 16);

  const int16_t *end = pred_buf_q3 + height * CFL_BUF_LINE;
  do {
    // Load 16 values for the top and bottom rows.
    // t_0, t_1, ... t_15
    __m128i top = _mm_loadu_si128((__m128i *)(input));
    // b_0, b_1, ... b_15
    __m128i bot = _mm_loadu_si128((__m128i *)(input + input_stride));

    // Load either the next line or the next 16 values
    __m128i next_top = _mm_loadu_si128((__m128i *)(input + next));
    __m128i next_bot =
        _mm_loadu_si128((__m128i *)(input + next + input_stride));

    // Horizontal add of the 16 values into 8 values that are multiplied by 2
    // (t_0 + t_1) * 2, (t_2 + t_3) * 2, ... (t_14 + t_15) *2
    top = _mm_maddubs_epi16(top, twos);
    next_top = _mm_maddubs_epi16(next_top, twos);
    // (b_0 + b_1) * 2, (b_2 + b_3) * 2, ... (b_14 + b_15) *2
    bot = _mm_maddubs_epi16(bot, twos);
    next_bot = _mm_maddubs_epi16(next_bot, twos);

    // Add the 8 values in top with the 8 values in bottom
    _mm_storeu_si128((__m128i *)pred_buf_q3, _mm_add_epi16(top, bot));
    _mm_storeu_si128((__m128i *)(pred_buf_q3 + next_chroma),
                     _mm_add_epi16(next_top, next_bot));

    input += luma_stride;
    pred_buf_q3 += chroma_stride;
  } while (pred_buf_q3 < end);
}

cfl_subsample_lbd_fn get_subsample_lbd_fn_ssse3(int sub_x, int sub_y) {
  static const cfl_subsample_lbd_fn subsample_lbd[2][2] = {
    //  (sub_y == 0, sub_x == 0)       (sub_y == 0, sub_x == 1)
    //  (sub_y == 1, sub_x == 0)       (sub_y == 1, sub_x == 1)
    { cfl_luma_subsampling_444_lbd, cfl_luma_subsampling_422_lbd },
    { cfl_luma_subsampling_440_lbd, cfl_luma_subsampling_420_lbd_ssse3 },
  };
  // AND sub_x and sub_y with 1 to ensures that an attacker won't be able to
  // index the function pointer array out of bounds.
  return subsample_lbd[sub_y & 1][sub_x & 1];
}

static INLINE __m128i get_scaled_luma_q0_ssse3(const __m128i *input,
                                               __m128i alpha_q12,
                                               __m128i alpha_sign) {
  __m128i ac_q3 = _mm_loadu_si128(input);
  __m128i ac_sign = _mm_srai_epi16(ac_q3, 15);
  ac_q3 = _mm_xor_si128(ac_q3, ac_sign);
  ac_q3 = _mm_sub_epi16(ac_q3, ac_sign);
  ac_sign = _mm_xor_si128(ac_sign, alpha_sign);
  __m128i scaled_luma_q0 = _mm_mulhrs_epi16(ac_q3, alpha_q12);
  scaled_luma_q0 = _mm_xor_si128(scaled_luma_q0, ac_sign);
  scaled_luma_q0 = _mm_sub_epi16(scaled_luma_q0, ac_sign);
  return scaled_luma_q0;
}

static INLINE void cfl_predict_lbd_x(const int16_t *pred_buf_q3, uint8_t *dst,
                                     int dst_stride, TX_SIZE tx_size,
                                     int alpha_q3, int width) {
  const int height = tx_size_high[tx_size];
  const __m128i zeros = _mm_setzero_si128();
  const __m128i alpha_q12 = _mm_set1_epi16(abs(alpha_q3) * (1 << 9));
  const __m128i alpha_sign = alpha_q3 < 0 ? _mm_set1_epi16(-1) : zeros;
  const __m128i dc_q0 = _mm_set1_epi16(*dst);
  uint8_t *row_end = dst + height * dst_stride;
  do {
    __m128i scaled_luma_q0 = get_scaled_luma_q0_ssse3((__m128i *)(pred_buf_q3),
                                                      alpha_q12, alpha_sign);
    __m128i tmp0 = _mm_add_epi16(scaled_luma_q0, dc_q0);
    __m128i tmp1 = tmp0;
    if (width != 4) {
      scaled_luma_q0 = get_scaled_luma_q0_ssse3((__m128i *)(pred_buf_q3 + 8),
                                                alpha_q12, alpha_sign);
      tmp1 = _mm_add_epi16(scaled_luma_q0, dc_q0);
    }
    __m128i res = _mm_packus_epi16(tmp0, tmp1);
    if (width == 4)
      _mm_store_ss((float *)(dst), (__m128)res);
    else if (width == 8)
      _mm_storel_epi64((__m128i *)(dst), res);
    else
      _mm_storeu_si128((__m128i *)(dst), res);
    if (width == 32) {
      scaled_luma_q0 = get_scaled_luma_q0_ssse3((__m128i *)(pred_buf_q3 + 16),
                                                alpha_q12, alpha_sign);
      tmp0 = _mm_add_epi16(scaled_luma_q0, dc_q0);
      scaled_luma_q0 = get_scaled_luma_q0_ssse3((__m128i *)(pred_buf_q3 + 24),
                                                alpha_q12, alpha_sign);
      tmp1 = _mm_add_epi16(scaled_luma_q0, dc_q0);
      res = _mm_packus_epi16(tmp0, tmp1);
      _mm_storeu_si128((__m128i *)(dst + 16), res);
    }
    dst += dst_stride;
    pred_buf_q3 += CFL_BUF_LINE;
  } while (dst < row_end);
}

static void cfl_predict_lbd_4(const int16_t *pred_buf_q3, uint8_t *dst,
                              int dst_stride, TX_SIZE tx_size, int alpha_q3) {
  cfl_predict_lbd_x(pred_buf_q3, dst, dst_stride, tx_size, alpha_q3, 4);
}

static void cfl_predict_lbd_8(const int16_t *pred_buf_q3, uint8_t *dst,
                              int dst_stride, TX_SIZE tx_size, int alpha_q3) {
  cfl_predict_lbd_x(pred_buf_q3, dst, dst_stride, tx_size, alpha_q3, 8);
}

static void cfl_predict_lbd_16(const int16_t *pred_buf_q3, uint8_t *dst,
                               int dst_stride, TX_SIZE tx_size, int alpha_q3) {
  cfl_predict_lbd_x(pred_buf_q3, dst, dst_stride, tx_size, alpha_q3, 16);
}

static void cfl_predict_lbd_32(const int16_t *pred_buf_q3, uint8_t *dst,
                               int dst_stride, TX_SIZE tx_size, int alpha_q3) {
  cfl_predict_lbd_x(pred_buf_q3, dst, dst_stride, tx_size, alpha_q3, 32);
}

cfl_predict_lbd_fn get_predict_lbd_fn_ssse3(TX_SIZE tx_size) {
  static const cfl_predict_lbd_fn predict_lbd[4] = {
    cfl_predict_lbd_4, cfl_predict_lbd_8, cfl_predict_lbd_16, cfl_predict_lbd_32
  };
  const int width_log2 = tx_size_wide_log2[tx_size];
  return predict_lbd[(width_log2 - 2) & 3];
}

int sum_block_4x4_ssse3(int16_t *pred_buf) {
  const __m128i zeros = _mm_setzero_si128();
  __m128i r0 = _mm_loadl_epi64((__m128i *)pred_buf);
  __m128i r1 = _mm_loadl_epi64((__m128i *)(pred_buf + CFL_BUF_LINE));
  __m128i r2 = _mm_loadl_epi64((__m128i *)(pred_buf + 2 * CFL_BUF_LINE));
  __m128i r3 = _mm_loadl_epi64((__m128i *)(pred_buf + 3 * CFL_BUF_LINE));

  // At this stage in CfL, the maximum value in the CfL prediction buffer can
  // only reach 15 bit, so it is safe to do one addition inside 16 bit.
  // r0 = r0_0 + r2_0, r1_0 + r3_0, ..., r0_3 + r2_3, r1_3 + r3_3
  r0 = _mm_add_epi16(_mm_unpacklo_epi16(r0, r1), _mm_unpacklo_epi16(r2, r3));

  // For the other additions, we need to convert to 32 bits.
  // To do so, we add the low part with the high part.
  // r0 = r0_0 + r0_4, r0_1 + r0_5, r0_2 + r0_6, r0_3 + r0_7
  r0 = _mm_add_epi32(_mm_unpacklo_epi16(r0, zeros),
                     _mm_unpackhi_epi16(r0, zeros));

  // r0 = r0_0 + r0_2, r0_1 + r0_3, ...
  r0 = _mm_add_epi32(r0, _mm_shuffle_epi32(r0, _MM_SHUFFLE(1, 0, 3, 2)));
  // r0 = r0_0 + r0_1, ...
  r0 = _mm_add_epi32(r0, _mm_shuffle_epi32(r0, _MM_SHUFFLE(2, 3, 0, 1)));

  return _mm_cvtsi128_si32(r0);
}

int sum_block_8x8_ssse3(int16_t *pred_buf) {
  const __m128i zeros = _mm_setzero_si128();
  __m128i r0 = _mm_loadu_si128((__m128i *)pred_buf);
  __m128i r1 = _mm_loadu_si128((__m128i *)(pred_buf + CFL_BUF_LINE));
  __m128i r2 = _mm_loadu_si128((__m128i *)(pred_buf + 2 * CFL_BUF_LINE));
  __m128i r3 = _mm_loadu_si128((__m128i *)(pred_buf + 3 * CFL_BUF_LINE));
  __m128i r4 = _mm_loadu_si128((__m128i *)(pred_buf + 4 * CFL_BUF_LINE));
  __m128i r5 = _mm_loadu_si128((__m128i *)(pred_buf + 5 * CFL_BUF_LINE));
  __m128i r6 = _mm_loadu_si128((__m128i *)(pred_buf + 6 * CFL_BUF_LINE));
  __m128i r7 = _mm_loadu_si128((__m128i *)(pred_buf + 7 * CFL_BUF_LINE));

  // At this stage in CfL, the maximum value in the CfL prediction buffer can
  // only reach 15 bit, so it is safe to do one addition inside 16 bit.
  // r0 = r0_0 + r1_0, r0_1 + r1_1, ..., r0_7 + r1_7
  r0 = _mm_add_epi16(r0, r1);
  r2 = _mm_add_epi16(r2, r3);
  r4 = _mm_add_epi16(r4, r5);
  r6 = _mm_add_epi16(r6, r7);

  // For the other additions, we need to convert to 32 bits.
  // To do so, we add the low part with the high part.
  // r0 = r0_0 + r0_4, r0_1 + r0_5, r0_2 + r0_6, r0_3 + r0_7
  r0 = _mm_add_epi32(_mm_unpacklo_epi16(r0, zeros),
                     _mm_unpackhi_epi16(r0, zeros));
  r2 = _mm_add_epi32(_mm_unpacklo_epi16(r2, zeros),
                     _mm_unpackhi_epi16(r2, zeros));
  r4 = _mm_add_epi32(_mm_unpacklo_epi16(r4, zeros),
                     _mm_unpackhi_epi16(r4, zeros));
  r6 = _mm_add_epi32(_mm_unpacklo_epi16(r6, zeros),
                     _mm_unpackhi_epi16(r6, zeros));

  // r0 = r0_0 + r2_0, r0_1 + r2_1, r0_2 + r2_2, r0_3 + r2_3
  r0 = _mm_add_epi32(r0, r2);
  // r4 = r4_0 + r6_0, r4_1 + r6_1, r4_2 + r6_2, r4_3 + r6_3
  r4 = _mm_add_epi32(r4, r6);
  // r0 = r0_0 + r4_0, r0_1 + r4_1, r0_2 + r4_2, r0_3 + r4_3
  r0 = _mm_add_epi32(r0, r4);

  // r0 = r0_0 + r0_2, r0_1 + r0_3, ...
  r0 = _mm_add_epi32(r0, _mm_shuffle_epi32(r0, _MM_SHUFFLE(1, 0, 3, 2)));
  // r0 = r0_0 + r0_1, ...
  r0 = _mm_add_epi32(r0, _mm_shuffle_epi32(r0, _MM_SHUFFLE(2, 3, 0, 1)));

  return _mm_cvtsi128_si32(r0);
}

cfl_sum_block_fn get_sum_block_fn_ssse3(TX_SIZE tx_size) {
  static const cfl_sum_block_fn sum_block[TX_SIZES_ALL] = {
    sum_block_4x4_ssse3,  // 4x4
    sum_block_8x8_ssse3,  // 8x8
    cfl_sum_block_null,   // 16x16
    cfl_sum_block_null,   // 32x32
#if CONFIG_TX64X64
    cfl_sum_block_null,  // 64x64 (invalid CFL size)
#endif                   // CONFIG_TX64X64
    cfl_sum_block_null,  // 4x8
    cfl_sum_block_null,  // 8x4
    cfl_sum_block_null,  // 8x16
    cfl_sum_block_null,  // 16x8
    cfl_sum_block_null,  // 16x32
    cfl_sum_block_null,  // 32x16
#if CONFIG_TX64X64
    cfl_sum_block_null,  // 32x64 (invalid CFL size)
    cfl_sum_block_null,  // 64x32 (invalid CFL size)
#endif                   // CONFIG_TX64X64
    cfl_sum_block_null,  // 4x16 (invalid CFL size)
    cfl_sum_block_null,  // 16x4 (invalid CFL size)
    cfl_sum_block_null,  // 8x32 (invalid CFL size)
    cfl_sum_block_null,  // 32x8 (invalid CFL size)
#if CONFIG_TX64X64
    cfl_sum_block_null,  // 16x64 (invalid CFL size)
    cfl_sum_block_null,  // 64x16 (invalid CFL size)
#endif                   // CONFIG_TX64X64
  };
  // Modulo TX_SIZES_ALL to ensure that an attacker won't be able to
  // index the function pointer array out of bounds.
  return sum_block[tx_size % TX_SIZES_ALL];
}
