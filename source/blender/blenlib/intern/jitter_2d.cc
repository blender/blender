/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 * \brief Jitter offset table
 */

#include "MEM_guardedalloc.h"
#include <cmath>
#include <cstring>

#include "BLI_jitter_2d.h"
#include "BLI_rand.h"

#include "BLI_strict_flags.h" /* IWYU pragma: keep. Keep last. */

void BLI_jitterate1(float (*jit1)[2], float (*jit2)[2], int num, float radius1)
{
  int i, j, k;
  float vecx, vecy, dvecx, dvecy, x, y, len;

  for (i = num - 1; i >= 0; i--) {
    dvecx = dvecy = 0.0;
    x = jit1[i][0];
    y = jit1[i][1];
    for (j = num - 1; j >= 0; j--) {
      if (i != j) {
        vecx = jit1[j][0] - x - 1.0f;
        vecy = jit1[j][1] - y - 1.0f;
        for (k = 3; k > 0; k--) {
          if (fabsf(vecx) < radius1 && fabsf(vecy) < radius1) {
            len = sqrtf(vecx * vecx + vecy * vecy);
            if (len > 0 && len < radius1) {
              len = len / radius1;
              dvecx += vecx / len;
              dvecy += vecy / len;
            }
          }
          vecx += 1.0f;

          if (fabsf(vecx) < radius1 && fabsf(vecy) < radius1) {
            len = sqrtf(vecx * vecx + vecy * vecy);
            if (len > 0 && len < radius1) {
              len = len / radius1;
              dvecx += vecx / len;
              dvecy += vecy / len;
            }
          }
          vecx += 1.0f;

          if (fabsf(vecx) < radius1 && fabsf(vecy) < radius1) {
            len = sqrtf(vecx * vecx + vecy * vecy);
            if (len > 0 && len < radius1) {
              len = len / radius1;
              dvecx += vecx / len;
              dvecy += vecy / len;
            }
          }
          vecx -= 2.0f;
          vecy += 1.0f;
        }
      }
    }

    x -= dvecx / 18.0f;
    y -= dvecy / 18.0f;
    x -= floorf(x);
    y -= floorf(y);
    jit2[i][0] = x;
    jit2[i][1] = y;
  }
  memcpy(jit1, jit2, 2 * uint(num) * sizeof(float));
}

void BLI_jitterate2(float (*jit1)[2], float (*jit2)[2], int num, float radius2)
{
  int i, j;
  float vecx, vecy, dvecx, dvecy, x, y;

  for (i = num - 1; i >= 0; i--) {
    dvecx = dvecy = 0.0;
    x = jit1[i][0];
    y = jit1[i][1];
    for (j = num - 1; j >= 0; j--) {
      if (i != j) {
        vecx = jit1[j][0] - x - 1.0f;
        vecy = jit1[j][1] - y - 1.0f;

        if (fabsf(vecx) < radius2) {
          dvecx += vecx * radius2;
        }
        vecx += 1.0f;
        if (fabsf(vecx) < radius2) {
          dvecx += vecx * radius2;
        }
        vecx += 1.0f;
        if (fabsf(vecx) < radius2) {
          dvecx += vecx * radius2;
        }

        if (fabsf(vecy) < radius2) {
          dvecy += vecy * radius2;
        }
        vecy += 1.0f;
        if (fabsf(vecy) < radius2) {
          dvecy += vecy * radius2;
        }
        vecy += 1.0f;
        if (fabsf(vecy) < radius2) {
          dvecy += vecy * radius2;
        }
      }
    }

    x -= dvecx / 2.0f;
    y -= dvecy / 2.0f;
    x -= floorf(x);
    y -= floorf(y);
    jit2[i][0] = x;
    jit2[i][1] = y;
  }
  memcpy(jit1, jit2, uint(num) * sizeof(float[2]));
}

void BLI_jitter_init(float (*jitarr)[2], int num)
{
  float (*jit2)[2];
  float number_fl, number_fl_sqrt;
  float x, rad1, rad2, rad3;
  RNG *rng;
  int i;

  if (num == 0) {
    return;
  }

  number_fl = float(num);
  number_fl_sqrt = sqrtf(number_fl);

  jit2 = MEM_malloc_arrayN<float[2]>(2 + size_t(num), "initjit");
  rad1 = 1.0f / number_fl_sqrt;
  rad2 = 1.0f / number_fl;
  rad3 = number_fl_sqrt / number_fl;

  rng = BLI_rng_new(31415926 + uint(num));

  x = 0;
  for (i = 0; i < num; i++) {
    jitarr[i][0] = x + rad1 * float(0.5 - BLI_rng_get_double(rng));
    jitarr[i][1] = float(i) / number_fl + rad1 * float(0.5 - BLI_rng_get_double(rng));
    x += rad3;
    x -= floorf(x);
  }

  BLI_rng_free(rng);

  for (i = 0; i < 24; i++) {
    BLI_jitterate1(jitarr, jit2, num, rad1);
    BLI_jitterate1(jitarr, jit2, num, rad1);
    BLI_jitterate2(jitarr, jit2, num, rad2);
  }

  MEM_freeN(jit2);

  /* Finally, move jitter to be centered around (0, 0). */
  for (i = 0; i < num; i++) {
    jitarr[i][0] -= 0.5f;
    jitarr[i][1] -= 0.5f;
  }
}
