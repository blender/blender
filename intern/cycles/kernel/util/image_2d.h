/* SPDX-FileCopyrightText: 2011-2026 Blender Foundation
 *
 * SPDX-License-Identifier: Apache-2.0 */

#pragma once

#include "kernel/globals.h"
#include "kernel/sample/lcg.h"

#include "util/atomic.h"
#include "util/defines.h"
#include "util/math_fast.h"
#include "util/types_image.h"

CCL_NAMESPACE_BEGIN

ccl_device_forceinline int kernel_image_udim_map(KernelGlobals kg,
                                                 const int id,
                                                 ccl_private float2 &uv)
{
  if (id >= 0) {
    return id;
  }

  const int tx = (int)uv.x;
  const int ty = (int)uv.y;
  if (tx < 0 || ty < 0 || tx >= 10) {
    return KERNEL_IMAGE_NONE;
  }

  int udim_id = -id - 1;
  const int num_udims = kernel_data_fetch(image_texture_udims, udim_id++).tile;
  const int tile = 1001 + 10 * ty + tx;

  for (int i = 0; i < num_udims; i++) {
    const KernelImageUDIM udim = kernel_data_fetch(image_texture_udims, udim_id++);
    if (udim.tile == tile) {
      /* If we found the tile, offset the UVs to be relative to it. */
      uv.x -= tx;
      uv.y -= ty;
      return udim.image_texture_id;
    }
  }

  return KERNEL_IMAGE_NONE;
}

CCL_NAMESPACE_END
