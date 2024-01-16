/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * This pass load Gbuffer data and output a stencil of pixel to process for each
 * lighting complexity.
 */

#pragma BLENDER_REQUIRE(eevee_gbuffer_lib.glsl)

void main()
{
  GBufferReader gbuf = gbuffer_read_header_closure_types(in_gbuffer_header);

#if defined(GPU_ARB_shader_stencil_export) || defined(GPU_METAL)
  gl_FragStencilRefARB = gbuf.closure_count;
#else
  /* Instead of setting the stencil at once, we do it (literally) bit by bit.
   * Discard fragments that do not have a number of closure whose bit-pattern
   * overlap the current stencil un-masked bit. */
  if ((current_bit & int(gbuf.closure_count)) == 0) {
    discard;
    return;
  }
#endif
}
