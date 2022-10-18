/* SPDX-License-Identifier: Apache-2.0
 * Copyright 2019-2022 Blender Foundation */

/* This file is based on "Progressive Multi-Jittered Sample Sequences"
 * by Christensen, Kensler, and Kilpatrick, but with a much simpler and
 * faster implementation based on "Stochastic Generation of (t, s)
 * Sample Sequences" by Helmer, Christensen, and Kensler.
 */

#include "scene/jitter.h"
#include "util/hash.h"

#include <math.h>
#include <vector>

CCL_NAMESPACE_BEGIN

void progressive_multi_jitter_02_generate_2D(float2 points[], int size, int rng_seed)
{
  /* Xor values for generating the PMJ02 sequence.  These permute the
   * order we visit the strata in, which is what makes the code below
   * produce the PMJ02 sequence.  Other choices are also possible, but
   * result in different sequences. */
  static uint xors[2][32] = {
      {0x00000000, 0x00000000, 0x00000002, 0x00000006, 0x00000006, 0x0000000e, 0x00000036,
       0x0000004e, 0x00000016, 0x0000002e, 0x00000276, 0x000006ce, 0x00000716, 0x00000c2e,
       0x00003076, 0x000040ce, 0x00000116, 0x0000022e, 0x00020676, 0x00060ece, 0x00061716,
       0x000e2c2e, 0x00367076, 0x004ec0ce, 0x00170116, 0x002c022e, 0x02700676, 0x06c00ece,
       0x07001716, 0x0c002c2e, 0x30007076, 0x4000c0ce},
      {0x00000000, 0x00000001, 0x00000003, 0x00000003, 0x00000007, 0x0000001b, 0x00000027,
       0x0000000b, 0x00000017, 0x0000013b, 0x00000367, 0x0000038b, 0x00000617, 0x0000183b,
       0x00002067, 0x0000008b, 0x00000117, 0x0001033b, 0x00030767, 0x00030b8b, 0x00071617,
       0x001b383b, 0x00276067, 0x000b808b, 0x00160117, 0x0138033b, 0x03600767, 0x03800b8b,
       0x06001617, 0x1800383b, 0x20006067, 0x0000808b}};

  uint rng_i = rng_seed;

  points[0].x = hash_hp_float(rng_i++);
  points[0].y = hash_hp_float(rng_i++);

  /* Subdivide the domain into smaller and smaller strata, filling in new
   * points as we go. */
  for (int log_N = 0, N = 1; N < size; log_N++, N *= 2) {
    float strata_count = (float)(N * 2);
    for (int i = 0; i < N && (N + i) < size; i++) {
      /* Find the strata that are already occupied in this cell. */
      uint occupied_x_stratum = (uint)(points[i ^ xors[0][log_N]].x * strata_count);
      uint occupied_y_stratum = (uint)(points[i ^ xors[1][log_N]].y * strata_count);

      /* Generate a new point in the unoccupied strata. */
      points[N + i].x = ((float)(occupied_x_stratum ^ 1) + hash_hp_float(rng_i++)) / strata_count;
      points[N + i].y = ((float)(occupied_y_stratum ^ 1) + hash_hp_float(rng_i++)) / strata_count;
    }
  }
}

CCL_NAMESPACE_END
