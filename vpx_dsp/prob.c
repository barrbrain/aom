/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include <stdio.h>
#include <string.h>
#include "./prob.h"

const uint8_t vpx_norm[256] = {
  0, 7, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
  3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
  2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static unsigned int tree_merge_probs_impl(unsigned int i,
                                          const vpx_tree_index *tree,
                                          const vpx_prob *pre_probs,
                                          const unsigned int *counts,
                                          vpx_prob *probs) {
  const int l = tree[i];
  const unsigned int left_count =
      (l <= 0) ? counts[-l]
               : tree_merge_probs_impl(l, tree, pre_probs, counts, probs);
  const int r = tree[i + 1];
  const unsigned int right_count =
      (r <= 0) ? counts[-r]
               : tree_merge_probs_impl(r, tree, pre_probs, counts, probs);
  const unsigned int ct[2] = { left_count, right_count };
  probs[i >> 1] = mode_mv_merge_probs(pre_probs[i >> 1], ct);
  return left_count + right_count;
}

void vpx_tree_merge_probs(const vpx_tree_index *tree, const vpx_prob *pre_probs,
                          const unsigned int *counts, vpx_prob *probs) {
  tree_merge_probs_impl(0, tree, pre_probs, counts, probs);
}

#define BYTETOBINARYPATTERN "%d%d%d%d%d%d%d%d"
#define BYTETOBINARY(byte)  \
  (byte & 0x80 ? 1 : 0), \
  (byte & 0x40 ? 1 : 0), \
  (byte & 0x20 ? 1 : 0), \
  (byte & 0x10 ? 1 : 0), \
  (byte & 0x08 ? 1 : 0), \
  (byte & 0x04 ? 1 : 0), \
  (byte & 0x02 ? 1 : 0), \
  (byte & 0x01 ? 1 : 0)

/* Compute the Q15 CDF of a binary tree with given internal node probabilities.
   The number of symbols in the CDF is capped at 16 and when the number of leaf
    nodes is greater than 16 the resulting CDF will contain both internal and
    leaf nodes (internal nodes will be split in order of highest probability
    first).
   The vpx_tree_index for each node in the CDF will be in ind[] as well as the
    set of decisions that lead to that entry in pth[] with its length in len[].
   The return value is the number of symbols in the CDF. */
int tree_to_cdf(const vpx_tree_index *tree, const vpx_prob *probs,
 vpx_tree_index root, uint16_t *cdf, vpx_tree_index *ind, int *pth, int *len) {
  int nsymbs;
  uint16_t pdf[16];
  int next[16];
  int size;
  int i;
  ind[0] = root;
  pdf[0] = 32768;
  pth[0] = 0;
  len[0] = 0;
  nsymbs = 1;
  next[0] = 0;
  size = 1;
  while (size > 0 && nsymbs < 16) {
    int m;
    vpx_tree_index j;
    uint16_t prob;
    int path;
    int bits;
    /* Find the internal node with the largest probability. */
    m = 0;
    for (i = 1; i < size; i++) if (pdf[next[i]] > pdf[next[m]]) m = i;
    i = next[m];
    memmove(&next[m], &next[m + 1], sizeof(*next)*(size - (m + 1)));
    size--;
    for (m = 0; m < size; m++) if (next[m] > i) next[m]++;
    /* Split this symbol into two symbols. */
    prob = pdf[i];
    j = ind[i];
    path = pth[i];
    bits = len[i];
    fprintf(stderr, "node.index = %i\n", j);
    fprintf(stderr, "node.prob = %i\n", prob);
    fprintf(stderr, "probs[%i] = %i\n", j >> 1, probs[j >> 1]);
    memmove(&pdf[i + 1], &pdf[i], sizeof(*pdf)*(nsymbs - i));
    memmove(&ind[i + 1], &ind[i], sizeof(*ind)*(nsymbs - i));
    memmove(&pth[i + 1], &pth[i], sizeof(*pth)*(nsymbs - i));
    memmove(&len[i + 1], &len[i], sizeof(*len)*(nsymbs - i));
    nsymbs++;
    ind[i] = tree[j];
    pth[i] = (path << 1) + 0;
    len[i] = bits + 1;
    pdf[i] = ((prob*((uint32_t)probs[j >> 1] << 7)) + (1 << (15 - 1))) >> 15;
    ind[i + 1] = tree[j + 1];
    pth[i + 1] = (path << 1) + 1;
    len[i + 1] = bits + 1;
    pdf[i + 1] = prob - pdf[i];
    /* Queue any new internal nodes. */
    if (ind[i] > 0) {
      next[size++] = i;
    }
    if (ind[i + 1] > 0) {
      next[size++] = i + 1;
    }
    fprintf(stderr, "nsymbs = %i\n", nsymbs);
    for (i = 0; i < nsymbs; i++) {
      fprintf(stderr, "%i %i:" BYTETOBINARYPATTERN " %i %i\n", ind[i], pth[i],
       BYTETOBINARY(pth[i]), len[i], pdf[i]);
    }
    fprintf(stderr, "---------------------\n");
  }
  cdf[0] = pdf[0];
  for (i = 1; i < nsymbs; i++) {
    cdf[i] = cdf[i - 1] + pdf[i];
  }
  return nsymbs;
}
