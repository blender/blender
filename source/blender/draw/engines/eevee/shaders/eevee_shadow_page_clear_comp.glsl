/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Page Clear.
 *
 * Equivalent to a frame-buffer depth clear but only for pages pushed to the clear_page_buf.
 */

#include "infos/eevee_shadow_pipeline_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_shadow_page_clear)

#include "gpu_shader_utildefines_lib.glsl"

void main()
{
  /* We clear the destination pixels directly for the atomicMin technique. */
  uint page_packed = dst_coord_buf[gl_GlobalInvocationID.z];
  uint3 page_co = shadow_page_unpack(page_packed);
  page_co.xy = page_co.xy * SHADOW_PAGE_RES + gl_GlobalInvocationID.xy;

  imageStoreFast(shadow_atlas_img, int3(page_co), uint4(floatBitsToUint(FLT_MAX)));
}
