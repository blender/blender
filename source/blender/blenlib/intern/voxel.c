/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bli
 */

#include "BLI_utildefines.h"
#include "BLI_voxel.h"

#include "BLI_strict_flags.h" /* Keep last. */

BLI_INLINE float D(const float *data, const int res[3], int x, int y, int z)
{
  CLAMP(x, 0, res[0] - 1);
  CLAMP(y, 0, res[1] - 1);
  CLAMP(z, 0, res[2] - 1);
  return data[BLI_VOXEL_INDEX(x, y, z, res)];
}

/* *** nearest neighbor *** */

/* returns highest integer <= x as integer (slightly faster than floor()) */
BLI_INLINE int FLOORI(float x)
{
  const int r = (int)x;
  return ((x >= 0.0f) || (float)r == x) ? r : (r - 1);
}

/**
 * clamp function, cannot use the CLAMPIS macro,
 * it sometimes returns unwanted results apparently related to
 * gcc optimization flag `-fstrict-overflow` which is enabled at `-O2`
 *
 * this causes the test (x + 2) < 0 with int x == 2147483647 to return false (x being an integer,
 * x + 2 should wrap around to -2147483647 so the test < 0 should return true, which it doesn't).
 */
BLI_INLINE int64_t _clamp(int a, int b, int c)
{
  return (a < b) ? b : ((a > c) ? c : a);
}

float BLI_voxel_sample_trilinear(const float *data, const int res[3], const float co[3])
{
  if (data) {

    const float xf = co[0] * (float)res[0] - 0.5f;
    const float yf = co[1] * (float)res[1] - 0.5f;
    const float zf = co[2] * (float)res[2] - 0.5f;

    const int x = FLOORI(xf), y = FLOORI(yf), z = FLOORI(zf);

    const int64_t xc[2] = {
        _clamp(x, 0, res[0] - 1),
        _clamp(x + 1, 0, res[0] - 1),
    };
    const int64_t yc[2] = {
        _clamp(y, 0, res[1] - 1) * res[0],
        _clamp(y + 1, 0, res[1] - 1) * res[0],
    };
    const int64_t zc[2] = {
        _clamp(z, 0, res[2] - 1) * res[0] * res[1],
        _clamp(z + 1, 0, res[2] - 1) * res[0] * res[1],
    };

    const float dx = xf - (float)x;
    const float dy = yf - (float)y;
    const float dz = zf - (float)z;

    const float u[2] = {1.0f - dx, dx};
    const float v[2] = {1.0f - dy, dy};
    const float w[2] = {1.0f - dz, dz};

    return w[0] *
               (v[0] * (u[0] * data[xc[0] + yc[0] + zc[0]] + u[1] * data[xc[1] + yc[0] + zc[0]]) +
                v[1] * (u[0] * data[xc[0] + yc[1] + zc[0]] + u[1] * data[xc[1] + yc[1] + zc[0]])) +
           w[1] *
               (v[0] * (u[0] * data[xc[0] + yc[0] + zc[1]] + u[1] * data[xc[1] + yc[0] + zc[1]]) +
                v[1] * (u[0] * data[xc[0] + yc[1] + zc[1]] + u[1] * data[xc[1] + yc[1] + zc[1]]));
  }
  return 0.0f;
}
