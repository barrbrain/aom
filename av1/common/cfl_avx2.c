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

#include <immintrin.h>

#include "./av1_rtcd.h"

#include "av1/common/cfl.h"

/**
 * Subtracts avg_q3 from the active part of the CfL prediction buffer.
 *
 * The CfL prediction buffer is always of size CFL_BUF_SQUARE. However, the
 * active area is specified using width and height.
 *
 * Note: We don't need to worry about going over the active area, as long as we
 * stay inside the CfL prediction buffer.
 */
void av1_cfl_subtract_avx2(int16_t *pred_buf_q3, int width, int height,
                           int16_t avg_q3) {
  const __m256i avg_x16 = _mm256_set1_epi16(avg_q3);

  const int16_t *end = pred_buf_q3 + height * width;
  do {
    __m256i val_x16 = _mm256_loadu_si256((__m256i *)pred_buf_q3);
    _mm256_storeu_si256((__m256i *)pred_buf_q3,
                        _mm256_sub_epi16(val_x16, avg_x16));
  } while ((pred_buf_q3 += 16) < end);
}

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
static void cfl_luma_subsampling_420_lbd_avx2(const uint8_t *input,
                                              int input_stride,
                                              int16_t *pred_buf_q3,
					      int out_stride,
                                              int width, int height) {
  (void)width;  // Max chroma width is 16, so all widths fit in one __m256i

  const __m256i twos = _mm256_set1_epi8(2);  // Thirty two twos
  const int luma_stride = input_stride << 1;
  const int16_t *end = pred_buf_q3 + height * out_stride;
  do {
    // Load 32 values for the top and bottom rows.
    // t_0, t_1, ... t_31
    __m256i top = _mm256_loadu_si256((__m256i *)(input));
    // b_0, b_1, ... b_31
    __m256i bot = _mm256_loadu_si256((__m256i *)(input + input_stride));

    // Horizontal add of the 32 values into 16 values that are multiplied by 2
    // (t_0 + t_1) * 2, (t_2 + t_3) * 2, ... (t_30 + t_31) *2
    top = _mm256_maddubs_epi16(top, twos);
    // (b_0 + b_1) * 2, (b_2 + b_3) * 2, ... (b_30 + b_31) *2
    bot = _mm256_maddubs_epi16(bot, twos);

    // Add the 16 values in top with the 16 values in bottom
    _mm256_storeu_si256((__m256i *)pred_buf_q3, _mm256_add_epi16(top, bot));

    input += luma_stride;
    pred_buf_q3 += out_stride;
  } while (pred_buf_q3 < end);
}

cfl_subsample_lbd_fn get_subsample_lbd_fn_avx2(int sub_x, int sub_y) {
  static const cfl_subsample_lbd_fn subsample_lbd[2][2] = {
    //  (sub_y == 0, sub_x == 0)       (sub_y == 0, sub_x == 1)
    //  (sub_y == 1, sub_x == 0)       (sub_y == 1, sub_x == 1)
    { cfl_luma_subsampling_444_lbd, cfl_luma_subsampling_422_lbd },
    { cfl_luma_subsampling_440_lbd, cfl_luma_subsampling_420_lbd_avx2 },
  };
  // AND sub_x and sub_y with 1 to ensures that an attacker won't be able to
  // index the function pointer array out of bounds.
  return subsample_lbd[sub_y & 1][sub_x & 1];
}

static INLINE __m256i get_scaled_luma_q0_avx2(const __m256i *input,
                                              __m256i alpha_q12,
                                              __m256i alpha_sign) {
  __m256i ac_q3 = _mm256_loadu_si256(input);
  __m256i ac_sign = _mm256_sign_epi16(alpha_sign, ac_q3);
  __m256i scaled_luma_q0 =
      _mm256_mulhrs_epi16(_mm256_abs_epi16(ac_q3), alpha_q12);
  return _mm256_sign_epi16(scaled_luma_q0, ac_sign);
}

static INLINE void cfl_predict_lbd_x(const int16_t *pred_buf_q3, uint8_t *dst,
                                     int dst_stride, TX_SIZE tx_size,
                                     int alpha_q3, int width) {
  const int height = tx_size_high[tx_size];
  const __m256i alpha_q12 = _mm256_set1_epi16(abs(alpha_q3) * (1 << 9));
  const __m256i alpha_sign = _mm256_set1_epi16(alpha_q3 < 0 ? -1 : 1);
  const __m256i dc_q0 = _mm256_set1_epi16(*dst);
  const int16_t *row_end = pred_buf_q3 + height * width;
  do {
    __m256i scaled_luma_q0 =
        get_scaled_luma_q0_avx2((__m256i *)pred_buf_q3, alpha_q12, alpha_sign);
    __m256i tmp0 = _mm256_add_epi16(scaled_luma_q0, dc_q0);
    __m256i tmp1 = tmp0;
    if (width == 32) {
      scaled_luma_q0 = get_scaled_luma_q0_avx2((__m256i *)(pred_buf_q3 + 16),
                                               alpha_q12, alpha_sign);
      tmp1 = _mm256_add_epi16(scaled_luma_q0, dc_q0);
    }
    __m256i res = _mm256_packus_epi16(tmp0, tmp1);
    if (width == 4) {
      *(int32_t *)dst = _mm256_extract_epi32(res, 0);
      *(int32_t *)(dst + dst_stride) = _mm256_extract_epi32(res, 1);
      *(int32_t *)(dst + 2 * dst_stride) = _mm256_extract_epi32(res, 2);
      *(int32_t *)(dst + 3 * dst_stride) = _mm256_extract_epi32(res, 3);
    } else if (width == 8)
      _mm_storel_epi64((__m128i *)dst, _mm256_castsi256_si128(res));
    else if (width == 16) {
      res = _mm256_permute4x64_epi64(res, 0xD8);
      _mm_store_si128((__m128i *)dst, _mm256_castsi256_si128(res));
    } else {
      res = _mm256_permute4x64_epi64(res, 0xD8);
      _mm256_storeu_si256((__m256i *)dst, res);
    }
    dst += dst_stride * (32 / width);
    pred_buf_q3 += 32;
  } while (pred_buf_q3 < row_end);
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

cfl_predict_lbd_fn get_predict_lbd_fn_avx2(TX_SIZE tx_size) {
  static const cfl_predict_lbd_fn predict_lbd[4] = {
    cfl_predict_lbd_4, cfl_predict_lbd_8, cfl_predict_lbd_16, cfl_predict_lbd_32
  };
  const int width_log2 = tx_size_wide_log2[tx_size];
  return predict_lbd[(width_log2 - 2) & 3];
}
