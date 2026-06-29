/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Blit the rendered lookdev spheres onto the combined pass.
 * Also output closest depth for correct intersection with overlays.
 */

#pragma once

#include "gpu_shader_compat.hh"

namespace eevee::lookdev {

struct DisplayVertOut {
  [[smooth]] float2 uv_coord;
  [[flat]] uint sphere_id;
};

struct DisplayFragOut {
  [[frag_color(0)]] float4 color;
};

struct Display {
  [[push_constant]] const float2 viewportSize;
  [[push_constant]] const float2 invertedViewportSize;
  [[push_constant]] const int2 anchor;

  [[sampler(0)]] sampler2D metallic_tx;
  [[sampler(1)]] sampler2D diffuse_tx;
};

[[vertex]]
void display_vert([[resource_table]] Display &srt,
                  [[vertex_id]] const int vert_id,
                  [[instance_id]] const int inst_id,
                  [[position]] float4 &out_position,
                  [[out]] DisplayVertOut &v_out)
{
  uint vert_index = (vert_id < 3) ? uint(vert_id) : uint(vert_id - 2);

  float2 uv = float2(vert_index / 2, vert_index % 2);
  v_out.uv_coord = uv;
  v_out.sphere_id = uint(inst_id);

  float2 sphere_size = float2(textureSize(srt.metallic_tx, 0)) * srt.invertedViewportSize;
  float2 margin = float2(0.125f, -0.125f) * sphere_size;
  float2 anchor_point = float2(1.0f, -1.0f) -
                        float2(srt.viewportSize.x - srt.anchor.x, -srt.anchor.y) *
                            srt.invertedViewportSize * float2(2.0f) -
                        margin;

  float2 offset = anchor_point -
                  float2(sphere_size.x * (inst_id + 1) + margin.x * 2.0f * inst_id, 0.0f);
  float2 co = uv * sphere_size + offset;
  out_position = float4(co, 0.0f, 1.0f);
}

[[fragment]]
void display_frag([[resource_table]] Display &srt,
                  [[in]] const DisplayVertOut &v_out,
                  [[out]] DisplayFragOut &frag_out,
                  [[frag_depth(any)]] float &out_depth)
{
  float2 texture_size = float2(textureSize(srt.metallic_tx, 0));
  float2 texel_size = float2(1.0f) / texture_size;

  float distance_from_center = distance(v_out.uv_coord, float2(0.5f));
  if (distance_from_center > 0.5f) {
    gpu_discard_fragment();
    return;
  }
  float smooth_size = texel_size.x * 1.5f;
  float smooth_offset = texel_size.x * 0.5f;
  float factor = (distance_from_center - 0.5f + smooth_offset + smooth_size) *
                 (1.0f / smooth_size);
  float alpha = clamp(1.0f - factor, 0.0f, 1.0f);

  float4 color = v_out.sphere_id == 0 ? texture(srt.metallic_tx, v_out.uv_coord.xy, 0) :
                                        texture(srt.diffuse_tx, v_out.uv_coord.xy, 0);
  color.a = alpha;
  frag_out.color = color;

  /* Ensure balls are on top of overlays. */
  out_depth = 0.0f;
}

}  // namespace eevee::lookdev

PipelineGraphic eevee_lookdev_display(eevee::lookdev::display_vert, eevee::lookdev::display_frag);
