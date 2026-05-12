/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_defines.hh"
#include "eevee_shadow_shared.hh"
#include "gpu_shader_utildefines_lib.glsl"

namespace eevee::shadow {

struct PageClear {
  [[storage(2, read)]] const ShadowPagesInfoData &pages_infos_buf;
  [[storage(6, read)]] const uint (&dst_coord_buf)[SHADOW_RENDER_MAP_SIZE];
  [[image(SHADOW_ATLAS_IMG_SLOT, read_write, UINT_32)]] uimage2DArrayAtomic shadow_atlas_img;
};

/**
 * Virtual shadow-mapping: Page Clear.
 *
 * Equivalent to a frame-buffer depth clear but only for pages pushed to the clear_page_buf.
 */
[[compute, local_size(SHADOW_PAGE_CLEAR_GROUP_SIZE, SHADOW_PAGE_CLEAR_GROUP_SIZE)]]
void page_clear_comp([[resource_table]] PageClear &srt,
                     [[global_invocation_id]] const uint3 global_id)
{
  /* We clear the destination pixels directly for the atomicMin technique. */
  uint page_packed = srt.dst_coord_buf[global_id.z];
  uint3 page_co = shadow_page_unpack(page_packed);
  page_co.xy = page_co.xy * SHADOW_PAGE_RES + global_id.xy;

  imageStoreFast(srt.shadow_atlas_img, int3(page_co), uint4(floatBitsToUint(FLT_MAX)));
}

PipelineCompute page_clear(page_clear_comp);

}  // namespace eevee::shadow
