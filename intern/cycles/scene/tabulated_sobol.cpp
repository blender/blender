/* SPDX-FileCopyrightText: 2019-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

/* This file is based on the paper "Stochastic Generation of (t, s)
 * Sample Sequences" by Helmer, Christensen, and Kensler.
 */

#include "scene/tabulated_sobol.h"
#include "util/hash.h"

#include <math.h>
#include <vector>

CCL_NAMESPACE_BEGIN

void tabulated_sobol_generate_4D(float4 points[], int size, int rng_seed)
{
  /* Xor values for generating the (4D) Owen-scrambled Sobol sequence.
   * These permute the order we visit the strata in, which is what
   * makes the code below produce the scrambled Sobol sequence.  Other
   * choices are also possible, but result in different sequences. */
  static uint xors[4][32] = {
      {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
       0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
      {0x00000000, 0x00000001, 0x00000001, 0x00000007, 0x00000001, 0x00000013, 0x00000015,
       0x0000007f, 0x00000001, 0x00000103, 0x00000105, 0x0000070f, 0x00000111, 0x00001333,
       0x00001555, 0x00007fff, 0x00000001, 0x00010003, 0x00010005, 0x0007000f, 0x00010011,
       0x00130033, 0x00150055, 0x007f00ff, 0x00010101, 0x01030303, 0x01050505, 0x070f0f0f,
       0x01111111, 0x13333333, 0x15555555, 0x7fffffff},
      {0x00000000, 0x00000001, 0x00000003, 0x00000001, 0x00000005, 0x0000001f, 0x0000002b,
       0x0000003d, 0x00000011, 0x00000133, 0x00000377, 0x00000199, 0x00000445, 0x00001ccf,
       0x00002ddb, 0x0000366d, 0x00000101, 0x00010303, 0x00030707, 0x00010909, 0x00051515,
       0x001f3f3f, 0x002b6b6b, 0x003dbdbd, 0x00101011, 0x01303033, 0x03707077, 0x01909099,
       0x04515145, 0x1cf3f3cf, 0x2db6b6db, 0x36dbdb6d},
      {0x00000000, 0x00000001, 0x00000000, 0x00000003, 0x0000000d, 0x0000000c, 0x00000005,
       0x0000004f, 0x00000014, 0x000000e7, 0x00000329, 0x0000039c, 0x00000011, 0x00001033,
       0x00000044, 0x000030bb, 0x0000d1cd, 0x0000c2ec, 0x00005415, 0x0004fc3f, 0x00015054,
       0x000e5c97, 0x0032e5b9, 0x0039725c, 0x00000101, 0x01000303, 0x00000404, 0x03000b0b,
       0x0d001d1d, 0x0c002c2c, 0x05004545, 0x4f00cfcf},
  };

  /* Randomize the seed, in case it's incrementing.  The constant is just a
   * random number, and has no other significance. */
  uint rng_i = hash_hp_seeded_uint(rng_seed, 0x44605a73);

  points[0].x = hash_hp_float(rng_i++);
  points[0].y = hash_hp_float(rng_i++);
  points[0].z = hash_hp_float(rng_i++);
  points[0].w = hash_hp_float(rng_i++);

  /* Subdivide the domain into smaller and smaller strata, filling in new
   * points as we go. */
  for (int log_N = 0, N = 1; N < size; log_N++, N *= 2) {
    float strata_count = (float)(N * 2);
    for (int i = 0; i < N && (N + i) < size; i++) {
      /* Find the strata that are already occupied in this cell. */
      uint occupied_x_stratum = (uint)(points[i ^ xors[0][log_N]].x * strata_count);
      uint occupied_y_stratum = (uint)(points[i ^ xors[1][log_N]].y * strata_count);
      uint occupied_z_stratum = (uint)(points[i ^ xors[2][log_N]].z * strata_count);
      uint occupied_w_stratum = (uint)(points[i ^ xors[3][log_N]].w * strata_count);

      /* Generate a new point in the unoccupied strata. */
      points[N + i].x = ((float)(occupied_x_stratum ^ 1) + hash_hp_float(rng_i++)) / strata_count;
      points[N + i].y = ((float)(occupied_y_stratum ^ 1) + hash_hp_float(rng_i++)) / strata_count;
      points[N + i].z = ((float)(occupied_z_stratum ^ 1) + hash_hp_float(rng_i++)) / strata_count;
      points[N + i].w = ((float)(occupied_w_stratum ^ 1) + hash_hp_float(rng_i++)) / strata_count;
    }
  }
}

CCL_NAMESPACE_END
