/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_fullscreen.bsl.hh"

namespace workbench::depth {

[[vertex]]
void vert([[vertex_id]] const int vert_id, [[position]] float4 &position)
{
  fullscreen_vertex(vert_id, position);
}

struct Merge {
  [[sampler(0)]] sampler2DDepth depth_tx;
};

/* Merge a depth texture into the current framebuffer. */
[[fragment]] void merge([[resource_table]] const Merge &srt,
                        [[frag_coord]] const float4 frag_co,
                        [[frag_depth(any)]] float &out_depth)
{
  float2 screen_uv = frag_co.xy / float2(textureSize(srt.depth_tx, 0));
  out_depth = texture(srt.depth_tx, screen_uv).r;
}

/* Prepare the Depth Buffer for the Overlay Engine.
 * Set the depth to 0 for "In Front" objects which have set a specific stencil bit.
 * This way, the Overlay engine doesn't draw on top of them. */
[[fragment]] void overlay([[frag_depth(any)]] float &out_depth)
{
  out_depth = 0.0f;
}

}  // namespace workbench::depth

PipelineGraphic workbench_merge_depth(workbench::depth::vert, workbench::depth::merge);
PipelineGraphic workbench_overlay_depth(workbench::depth::vert, workbench::depth::overlay);
