/*
 *  Copyright (c) 2013 The WebM project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

#include "./prob.h"
#if CONFIG_DAALA_EC
#include "entcode.h"
#include <string.h>
#endif

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

#if CONFIG_DAALA_EC
/* Round[Table[N[Sum[(1/r)*((1 + Floor[(r - 1)*p/256])/r), {r, 128, 255}]/
     Sum[(1/r), {r, 128, 255}]*32768], {p, 0, 255}]] */
const uint16_t ec_norm[256] = {
    185,   185,   368,   461,   613,   721,   864,   978,  1118,  1233,  1370,
   1490,  1624,  1745,  1877,  2001,  2137,  2257,  2386,  2508,  2641,  2763,
   2895,  3020,  3151,  3272,  3404,  3527,  3658,  3781,  3912,  4038,  4177,
   4294,  4421,  4548,  4676,  4800,  4931,  5057,  5187,  5309,  5438,  5569,
   5695,  5822,  5948,  6075,  6208,  6330,  6457,  6591,  6715,  6836,  6968,
   7094,  7223,  7344,  7476,  7604,  7731,  7854,  7984,  8109,  8261,  8369,
   8495,  8621,  8749,  8877,  9005,  9129,  9259,  9390,  9513,  9639,  9767,
   9896, 10022, 10148, 10280, 10403, 10530, 10660, 10782, 10923, 11046, 11163,
  11298, 11421, 11549, 11675, 11804, 11933, 12058, 12184, 12323, 12440, 12567,
  12694, 12822, 12949, 13072, 13200, 13333, 13458, 13585, 13712, 13840, 13967,
  14097, 14221, 14353, 14476, 14605, 14729, 14858, 14985, 15111, 15239, 15368,
  15493, 15621, 15747, 15875, 16000, 16126, 16248, 16430, 16520, 16645, 16768,
  16897, 17021, 17150, 17275, 17407, 17529, 17660, 17783, 17914, 18039, 18166,
  18292, 18427, 18547, 18674, 18801, 18932, 19056, 19185, 19310, 19442, 19568,
  19698, 19819, 19950, 20074, 20204, 20328, 20469, 20584, 20713, 20835, 20968,
  21093, 21221, 21347, 21477, 21605, 21725, 21845, 21990, 22108, 22241, 22365,
  22500, 22620, 22749, 22872, 23005, 23129, 23258, 23378, 23516, 23639, 23766,
  23891, 24023, 24147, 24276, 24399, 24553, 24659, 24787, 24914, 25042, 25164,
  25295, 25424, 25552, 25674, 25803, 25932, 26058, 26177, 26314, 26438, 26573,
  26693, 26822, 26946, 27077, 27199, 27333, 27459, 27588, 27711, 27840, 27968,
  28097, 28220, 28350, 28474, 28614, 28730, 28859, 28987, 29114, 29241, 29367,
  29496, 29624, 29748, 29876, 30005, 30131, 30260, 30385, 30511, 30644, 30767,
  30893, 31023, 31148, 31278, 31401, 31535, 31657, 31790, 31907, 32047, 32159,
  32307, 32403, 32583
};

typedef struct tree_node tree_node;

struct tree_node {
  vpx_tree_index index;
  uint8_t probs[16];
  uint8_t prob;
  int path;
  int len;
  int l;
  int r;
  uint16_t pdf;
};

/* Compute the probability of this node in Q23 */
static uint32_t tree_node_prob(tree_node n, int i) {
  uint32_t prob;
  /* 1.0 in Q23 */
  prob = 16777216;
  for (; i < n.len; i++) {
    prob = prob*n.probs[i] >> 8;
  }
  return prob;
}

static int tree_node_cmp(tree_node a, tree_node b) {
  int i;
  uint32_t pa;
  uint32_t pb;
  for (i = 0; i < OD_MINI(a.len, b.len) && a.probs[i] == b.probs[i]; i++);
  pa = tree_node_prob(a, i);
  pb = tree_node_prob(b, i);
  return pa > pb ? 1 : pa < pb ? -1 : 0;
}

/* Given a Q15 probability for symbol subtree rooted at tree[n], this function
    computes the probability of each symbol (defined as a node that has no
    children). */
static uint16_t tree_node_compute_probs(tree_node *tree, int n, uint16_t pdf) {
  if (tree[n].l == 0) {
    /* This prevents probability computations in Q15 that underflow from
        producing a symbol that has zero probability. */
    if (pdf == 0) pdf = 1;
    tree[n].pdf = pdf;
    return pdf;
  }
  else {
    /* We process the smaller probability first,  */
    if (tree[n].prob < 128) {
      uint16_t lp;
      uint16_t rp;
      lp = (((uint32_t)pdf)*tree[n].prob + 128) >> 8;
      lp = tree_node_compute_probs(tree, tree[n].l, lp);
      rp = tree_node_compute_probs(tree, tree[n].r, lp > pdf ? 0 : pdf - lp);
      return lp + rp;
    }
    else {
      uint16_t rp;
      uint16_t lp;
      rp = (((uint32_t)pdf)*(256 - tree[n].prob) + 128) >> 8;
      rp = tree_node_compute_probs(tree, tree[n].r, rp);
      lp = tree_node_compute_probs(tree, tree[n].l, rp > pdf ? 0 : pdf - rp);
      return lp + rp;
    }
  }
}

static int tree_node_extract(tree_node *tree, int n, int symb,
 uint16_t *pdf, vpx_tree_index *index, int *path, int *len) {
  if (tree[n].l == 0) {
    pdf[symb] = tree[n].pdf;
    index[symb] = tree[n].index;
    path[symb] = tree[n].path;
    len[symb] = tree[n].len;
    return symb + 1;
  }
  else {
    symb = tree_node_extract(tree, tree[n].l, symb, pdf, index, path, len);
    return tree_node_extract(tree, tree[n].r, symb, pdf, index, path, len);
  }
}

int tree_to_cdf(const vpx_tree_index *tree, const vpx_prob *probs,
 vpx_tree_index root, uint16_t *cdf, vpx_tree_index *index, int *path,
 int *len) {
  tree_node symb[2*16-1];
  int nodes;
  int next[16];
  int size;
  int nsymbs;
  int i;
  /* Create the root node with probability 1 in Q15. */
  symb[0].index = root;
  symb[0].path = 0;
  symb[0].len = 0;
  symb[0].l = symb[0].r = 0;
  nodes = 1;
  next[0] = 0;
  size = 1;
  nsymbs = 1;
  while (size > 0 && nsymbs < 16) {
    int m;
    tree_node n;
    vpx_tree_index j;
    uint8_t prob;
    m = 0;
    /* Find the internal node with the largest probability. */
    for (i = 1; i < size; i++) {
      if (tree_node_cmp(symb[next[i]], symb[next[m]]) > 0) m = i;
    }
    i = next[m];
    memmove(&next[m], &next[m + 1], sizeof(*next)*(size - (m + 1)));
    size--;
    /* Split this symbol into two symbols */
    n = symb[i];
    j = n.index;
    prob = probs[j >> 1];
    /* Left */
    n.index = tree[j];
    n.path <<= 1;
    n.len++;
    n.probs[n.len - 1] = prob;
    symb[nodes] = n;
    if (n.index > 0) {
      next[size++] = nodes;
    }
    /* Right */
    n.index = tree[j + 1];
    n.path += 1;
    n.probs[n.len - 1] = 256 - prob;
    symb[nodes + 1] = n;
    if (n.index > 0) {
      next[size++] = nodes + 1;
    }
    symb[i].prob = prob;
    symb[i].l = nodes;
    symb[i].r = nodes + 1;
    nodes += 2;
    nsymbs++;
  }
  /* Compute the probabilities of each symbol in Q15 */
  tree_node_compute_probs(symb, 0, 32768);
  /* Extract the cdf, index, path and length */
  tree_node_extract(symb, 0, 0, cdf, index, path, len);
  /* Convert to CDF */
  for (i = 1; i < nsymbs; i++) {
    cdf[i] = cdf[i - 1] + cdf[i];
  }
  return nsymbs;
}
#endif
