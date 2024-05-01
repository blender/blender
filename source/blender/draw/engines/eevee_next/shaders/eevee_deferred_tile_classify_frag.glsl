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
  int closure_count = gbuffer_closure_count(in_gbuffer_header);
  int has_transmission = 0;
  if (gbuffer_has_transmission(in_gbuffer_header)) {
    has_transmission = 1 << 2;
  }

#if defined(GPU_ARB_shader_stencil_export) || defined(GPU_METAL)
  gl_FragStencilRefARB = closure_count | has_transmission;
#else
  /* Instead of setting the stencil at once, we do it (literally) bit by bit.
   * Discard fragments that do not have a number of closure whose bit-pattern
   * overlap the current stencil un-masked bit. */
  if ((current_bit & (closure_count | has_transmission)) == 0) {
    discard;
    return;
  }
#endif
}
