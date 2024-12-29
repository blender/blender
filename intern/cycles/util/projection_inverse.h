/* SPDX-FileCopyrightText: 2011-2022 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "util/defines.h"

CCL_NAMESPACE_BEGIN

ccl_device_forceinline bool projection_inverse_impl(ccl_private float R[4][4],
                                                    ccl_private float M[4][4])
{
  /* SPDX-License-Identifier: BSD-3-Clause
   * Adapted from code:
   * Copyright (c) 2002, Industrial Light & Magic, a division of Lucas
   * Digital Ltd. LLC. All rights reserved. */

  /* forward elimination */
  for (int i = 0; i < 4; i++) {
    int pivot = i;
    float pivotsize = M[i][i];

    if (pivotsize < 0) {
      pivotsize = -pivotsize;
    }

    for (int j = i + 1; j < 4; j++) {
      float tmp = M[j][i];

      if (tmp < 0) {
        tmp = -tmp;
      }

      if (tmp > pivotsize) {
        pivot = j;
        pivotsize = tmp;
      }
    }

    if (UNLIKELY(pivotsize == 0.0f)) {
      return false;
    }

    if (pivot != i) {
      for (int j = 0; j < 4; j++) {
        float tmp;

        tmp = M[i][j];
        M[i][j] = M[pivot][j];
        M[pivot][j] = tmp;

        tmp = R[i][j];
        R[i][j] = R[pivot][j];
        R[pivot][j] = tmp;
      }
    }

    for (int j = i + 1; j < 4; j++) {
      const float f = M[j][i] / M[i][i];

      for (int k = 0; k < 4; k++) {
        M[j][k] -= f * M[i][k];
        R[j][k] -= f * R[i][k];
      }
    }
  }

  /* backward substitution */
  for (int i = 3; i >= 0; --i) {
    float f = M[i][i];

    if (UNLIKELY(f == 0.0f)) {
      return false;
    }

    for (int j = 0; j < 4; j++) {
      M[i][j] /= f;
      R[i][j] /= f;
    }

    for (int j = 0; j < i; j++) {
      f = M[j][i];

      for (int k = 0; k < 4; k++) {
        M[j][k] -= f * M[i][k];
        R[j][k] -= f * R[i][k];
      }
    }
  }

  return true;
}

CCL_NAMESPACE_END
