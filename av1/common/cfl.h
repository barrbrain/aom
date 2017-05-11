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

  // Count the number of TX blocks in a predicted block to know when you are at
  // the last one, so you can check for skips.
  // TODO(any) Is there a better way to do this?
  int num_tx_blk[CFL_PRED_PLANES];
} CFL_CTX;

// Q15 magnitudes used in the codebook
static const int cfl_alpha_mags[CFL_MAGS_SIZE] = {
  0,    512,   -512,  683,    -683,  1024,   -1024, 1365,  -1365,
  2048, -2048, 2731,  -2731,  4096,  -4096,  5461,  -5461,
  8192, -8192, 10923, -10923, 16384, -16384, 21845, -21845
};

// vector codebook used to code the combination of alpha_u and alpha_v
static const int cfl_alpha_codes[CFL_PRED_PLANES][CFL_ALPHABET_SIZE][CFL_ALPHABET_SIZE] = {
  { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 8, 7, 10, 9, 12, 11, 14, 13, 16, 15, 18, 17, 20, 19, 22, 21 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 6, 5, 8, 7, 10, 9, 12, 11, 14, 13, 16, 15, 18, 17, 20, 19 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 4, 3, 6, 5, 8, 7, 10, 9, 12, 11, 14, 13, 16, 15, 18, 17 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 2, 1, 4, 3, 6, 5, 8, 7, 10, 9, 12, 11, 14, 13, 16, 15 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 } },
  { { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 10, 9, 12, 11, 14, 13, 16, 15, 18, 17, 20, 19, 22, 21, 24, 23 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 8, 7, 10, 9, 12, 11, 14, 13, 16, 15, 18, 17, 20, 19, 22, 21 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 6, 5, 8, 7, 10, 9, 12, 11, 14, 13, 16, 15, 18, 17, 20, 19 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 4, 3, 6, 5, 8, 7, 10, 9, 12, 11, 14, 13, 16, 15, 18, 17 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 2, 1, 4, 3, 6, 5, 8, 7, 10, 9, 12, 11, 14, 13, 16, 15 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 },
    { 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18 },
    { 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22 },
    { 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24 } }
};

void cfl_init(CFL_CTX *cfl, AV1_COMMON *cm, int subsampling_x,
              int subsampling_y);

void cfl_dc_pred(MACROBLOCKD *xd, BLOCK_SIZE plane_bsize, TX_SIZE tx_size);

static INLINE double cfl_idx_to_alpha(int cfl_uvec_idx, int cfl_mag_idx,
                                      CFL_PRED_TYPE pred_type) {
  const int idx = cfl_alpha_codes[pred_type][cfl_uvec_idx][cfl_mag_idx];
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
