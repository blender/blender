/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/* Based on :
 * McGuire and Bavoil, Weighted Blended Order-Independent Transparency, Journal of
 * Computer Graphics Techniques (JCGT), vol. 2, no. 2, 122–141, 2013
 */

#pragma once

#include "gpu_shader_fullscreen.bsl.hh"

namespace workbench::transparency {

[[vertex]]
void vert([[vertex_id]] const int vert_id, [[position]] float4 &position)
{
  fullscreen_vertex(vert_id, position);
}

struct Resolve {
  [[sampler(0)]] sampler2D transparent_accum;
  [[sampler(1)]] sampler2D transparent_revealage;
};

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]]
void resolve_weight_blended_order_independant_transparency([[resource_table]] const Resolve &srt,
                                                           [[frag_coord]] const float4 frag_co,
                                                           [[out]] FragOut &frag_out)
{
  float2 screen_uv = frag_co.xy / float2(textureSize(srt.transparent_accum, 0).xy);
  /* Revealage is actually stored in transparent_accum alpha channel.
   * This is a workaround to older hardware not having separate blend equation per render target.
   */
  float4 trans_accum = texture(srt.transparent_accum, screen_uv);
  float trans_weight = texture(srt.transparent_revealage, screen_uv).r;
  float trans_reveal = trans_accum.a;

  /* Listing 4 */
  frag_out.color.rgb = trans_accum.rgb / clamp(trans_weight, 1e-4f, 5e4f);
  frag_out.color.a = 1.0f - trans_reveal;
}

}  // namespace workbench::transparency

PipelineGraphic workbench_transparent_resolve(
    workbench::transparency::vert,
    workbench::transparency::resolve_weight_blended_order_independant_transparency);
