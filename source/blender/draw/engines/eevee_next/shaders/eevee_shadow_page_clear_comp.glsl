/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Virtual shadow-mapping: Page Clear.
 *
 * Equivalent to a frame-buffer depth clear but only for pages pushed to the clear_page_buf.
 */

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

void main()
{
  uint page_packed = clear_list_buf[gl_GlobalInvocationID.z];
  uvec3 page_co = shadow_page_unpack(page_packed);
  page_co.xy = page_co.xy * SHADOW_PAGE_RES + gl_GlobalInvocationID.xy;

  /* Clear to FLT_MAX instead of 1 so the far plane doesn't cast shadows onto farther objects. */
  imageStore(shadow_atlas_img, ivec3(page_co), uvec4(floatBitsToUint(FLT_MAX)));
}
