/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_gbuffer_types.bsl.hh"
#include "gpu_shader_fullscreen_lib.glsl"

namespace eevee::deferred::tiles {

[[vertex]]
void fullscreen_vert([[vertex_id]] const int vert_id, [[position]] float4 &out_position)
{
  fullscreen_vertex(vert_id, out_position);
}

struct TileSubpassIn {
  [[subpass_input(1, usampler2DArray),
    raster_order_group(DEFERRED_GBUFFER_ROG_ID)]] uint in_gbuffer_header;
};

struct TileClassification {
  [[push_constant]] int current_bit;
};

/**
 * This pass load Gbuffer data and output a stencil of pixel to process for each
 * lighting complexity.
 */
[[fragment]]
void classify_tiles([[resource_table]] const TileClassification &srt,
                    [[subpass_in]] const TileSubpassIn &frag_in)
{
  gbuffer::Header header = gbuffer::Header::from_data(frag_in.in_gbuffer_header);
  int closure_count = int(header.closure_len());
  int is_transmission = 0;
  if (header.has_transmission()) {
    is_transmission = 1 << 2;
  }

#if defined(GPU_ARB_shader_stencil_export) || defined(GPU_METAL)
  gl_FragStencilRefARB = closure_count | is_transmission;
#else
  /* Instead of setting the stencil at once, we do it (literally) bit by bit.
   * Discard fragments that do not have a number of closure whose bit-pattern
   * overlap the current stencil un-masked bit. */
  if ((srt.current_bit & (closure_count | is_transmission)) == 0) {
    gpu_discard_fragment();
    return;
  }
#endif
}

}  // namespace eevee::deferred::tiles

PipelineGraphic eevee_deferred_tile_classify(eevee::deferred::tiles::fullscreen_vert,
                                             eevee::deferred::tiles::classify_tiles);
