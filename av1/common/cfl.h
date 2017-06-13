/*
 * Copyright (c) 2016, Alliance for Open Media. All rights reserved
 *
 * This source code is subject to the terms of the BSD 2 Clause License and
 * the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
 * was not distributed with this source code in the LICENSE file, you can
 * obtain it at www.aomedia.org/license/software. If the Alliance for Open
 * Media Patent License 1.0 was not distributed with this source code in the
 * PATENTS file, you can obtain it at www.aomedia.org/license/patent.
 */

#ifndef AV1_COMMON_CFL_H_
#define AV1_COMMON_CFL_H_

#include <assert.h>

#include "av1/common/enums.h"

// Forward declaration of AV1_COMMON, in order to avoid creating a cyclic
// dependency by importing av1/common/onyxc_int.h
typedef struct AV1Common AV1_COMMON;

// Forward declaration of MACROBLOCK, in order to avoid creating a cyclic
// dependency by importing av1/common/blockd.h
typedef struct macroblockd MACROBLOCKD;

typedef struct {
  // Pixel buffer containing the luma pixels used as prediction for chroma
  uint8_t y_pix[MAX_SB_SQUARE];

  // Height and width of the luma prediction block currently in the pixel buffer
  int y_height, y_width;

  // Chroma subsampling
  int subsampling_x, subsampling_y;

  // CfL Performs its own block level DC_PRED for each chromatic plane
  double dc_pred[CFL_PRED_PLANES];

  // The rate associated with each alpha codeword
  int uvec_costs[CFL_ALPHABET_SIZE];
  int mag_costs[CFL_ALPHABET_SIZE];

} CFL_CTX;

// Q15 magnitudes used in the codebook
static const int cfl_alpha_mags[CFL_MAGS_SIZE] = {
  0,    512,   -512,  683,    -683,  1024,   -1024, 1365,  -1365,
  2048, -2048, 2731,  -2731,  4096,  -4096,  5461,  -5461,
  8192, -8192, 10923, -10923, 16384, -16384, 21845, -21845
};

// vector codebook used to code the combination of alpha_u and alpha_v
static const int cfl_alpha_codes[CFL_PRED_PLANES][CFL_ALPHABET_SIZE][CFL_ALPHABET_SIZE] = {
  { { 16, 18, 20, 14, 12, 22, 10, 13, 11, 15, 9, 17, 7, 24, 19, 21 },
    { 16, 18, 20, 14, 12, 22, 10, 11, 9, 13, 7, 15, 5, 24, 17, 19 },
    { 16, 18, 20, 14, 12, 22, 10, 15, 13, 17, 11, 19, 9, 24, 21, 23 },
    { 16, 18, 20, 14, 12, 22, 10, 13, 11, 15, 9, 17, 7, 24, 19, 21 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 14, 16, 18, 12, 10, 20, 8, 11, 9, 13, 7, 15, 5, 22, 17, 19 },
    { 16, 18, 20, 14, 12, 22, 10, 15, 13, 17, 11, 19, 9, 24, 21, 23 },
    { 12, 14, 16, 10, 8, 18, 6, 9, 7, 11, 5, 13, 3, 20, 15, 17 },
    { 16, 18, 20, 14, 12, 22, 10, 15, 13, 17, 11, 19, 9, 24, 21, 23 },
    { 10, 12, 14, 8, 6, 16, 4, 9, 7, 11, 5, 13, 3, 18, 15, 17 },
    { 8, 10, 12, 6, 4, 14, 2, 7, 5, 9, 3, 11, 1, 16, 13, 15 },
    { 12, 14, 16, 10, 8, 18, 6, 11, 9, 13, 7, 15, 5, 20, 17, 19 },
    { 14, 16, 18, 12, 10, 20, 8, 13, 11, 15, 9, 17, 7, 22, 19, 21 },
    { 16, 18, 20, 14, 12, 22, 10, 15, 13, 17, 11, 19, 9, 24, 21, 23 },
    { 10, 12, 14, 8, 6, 16, 4, 9, 7, 11, 5, 13, 3, 18, 15, 17 },
    { 8, 10, 12, 6, 4, 14, 2, 7, 5, 9, 3, 11, 1, 16, 13, 15 } },
  { { 13, 15, 17, 11, 9, 19, 7, 12, 10, 14, 8, 16, 6, 21, 18, 20 },
    { 15, 17, 19, 13, 11, 21, 9, 12, 10, 14, 8, 16, 6, 23, 18, 20 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 11, 13, 15, 9, 7, 17, 5, 10, 8, 12, 6, 14, 4, 19, 16, 18 },
    { 15, 17, 19, 13, 11, 21, 9, 16, 14, 18, 12, 20, 10, 23, 22, 24 },
    { 15, 17, 19, 13, 11, 21, 9, 14, 12, 16, 10, 18, 8, 23, 20, 22 },
    { 9, 11, 13, 7, 5, 15, 3, 10, 8, 12, 6, 14, 4, 17, 16, 18 },
    { 15, 17, 19, 13, 11, 21, 9, 14, 12, 16, 10, 18, 8, 23, 20, 22 },
    { 7, 9, 11, 5, 3, 13, 1, 8, 6, 10, 4, 12, 2, 15, 14, 16 },
    { 15, 17, 19, 13, 11, 21, 9, 16, 14, 18, 12, 20, 10, 23, 22, 24 },
    { 15, 17, 19, 13, 11, 21, 9, 16, 14, 18, 12, 20, 10, 23, 22, 24 },
    { 12, 14, 16, 10, 8, 18, 6, 11, 9, 13, 7, 15, 5, 20, 17, 19 },
    { 10, 12, 14, 8, 6, 16, 4, 9, 7, 11, 5, 13, 3, 18, 15, 17 },
    { 8, 10, 12, 6, 4, 14, 2, 7, 5, 9, 3, 11, 1, 16, 13, 15 },
    { 14, 16, 18, 12, 10, 20, 8, 13, 11, 15, 9, 17, 7, 22, 19, 21 },
    { 16, 18, 20, 14, 12, 22, 10, 15, 13, 17, 11, 19, 9, 24, 21, 23 } }
};

void cfl_init(CFL_CTX *cfl, AV1_COMMON *cm, int subsampling_x,
              int subsampling_y);

void cfl_dc_pred(MACROBLOCKD *xd, BLOCK_SIZE plane_bsize);

static INLINE double cfl_idx_to_alpha(int uvec_idx, int mag_idx,
                                      CFL_PRED_TYPE pred_type) {
  if (uvec_idx == 0) {
    assert(mag_idx == 0);
    return 0.;
  }
  const int idx = cfl_alpha_codes[pred_type][uvec_idx - 1][mag_idx];
  return cfl_alpha_mags[idx] * (1. / (1 << 15));
}

void cfl_predict_block(const CFL_CTX *cfl, uint8_t *dst, int dst_stride,
                       int row, int col, TX_SIZE tx_size, double dc_pred,
                       double alpha);

void cfl_store(CFL_CTX *cfl, const uint8_t *input, int input_stride, int row,
               int col, TX_SIZE tx_size);

double cfl_load(const CFL_CTX *cfl, uint8_t *output, int output_stride, int row,
                int col, int width, int height);
#endif  // AV1_COMMON_CFL_H_
