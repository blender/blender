/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Recombine Pass: Load separate convolution layer and composite with self
 * slight defocus convolution and in-focus fields.
 *
 * The half-resolution gather methods are fast but lack precision for small CoC areas.
 * To fix this we do a brute-force gather to have a smooth transition between
 * in-focus and defocus regions.
 */

#pragma once

#include "eevee_depth_of_field_accumulator.bsl.hh"
#include "eevee_depth_of_field_tiles.bsl.hh"

namespace eevee::dof::resolve {

struct Resources {
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[resource_table]] srt_t<Accumulator> accumulator;

  [[specialization_constant(false)]] const bool do_debug_color;

  [[sampler(0)]] sampler2DDepth depth_tx;
  [[sampler(1)]] sampler2D color_tx;
  [[sampler(2)]] sampler2D color_bg_tx;
  [[sampler(3)]] sampler2D color_fg_tx;
  [[sampler(4)]] sampler2D color_hole_fill_tx;
  [[sampler(7)]] sampler2D weight_bg_tx;
  [[sampler(8)]] sampler2D weight_fg_tx;
  [[sampler(9)]] sampler2D weight_hole_fill_tx;
  [[sampler(10)]] sampler2D stable_color_tx;

  [[image(2, write, SFLOAT_16_16_16_16)]] image2D out_color_img;

  [[shared]] uint shared_max_slight_focus_abs_coc;

  /**
   * Returns The max CoC in the Slight Focus range inside this compute tile.
   */
  float slight_focus_coc_tile_get(float2 frag_coord, uint local_index)
  {
    [[resource_table]] const Accumulator &accum = accumulator;

    float local_abs_max = 0.0f;
    /* Sample in a cross (X) pattern. This covers all pixels over the whole tile, as long as
     * dof_max_slight_focus_radius is less than the group size. */
    for (int i = 0; i < 4; i++) {
      float2 sample_uv = (frag_coord + quad_offsets[i] * 2.0f * dof_max_slight_focus_radius) /
                         float2(textureSize(color_tx, 0));
      float depth = reverse_z::read(textureLod(depth_tx, sample_uv, 0.0f).r);
      float coc = dof_coc_from_depth(accum.dof_buf, sample_uv, depth);
      coc = clamp(coc, -accum.dof_buf.coc_abs_max, accum.dof_buf.coc_abs_max);
      if (abs(coc) < dof_max_slight_focus_radius) {
        local_abs_max = max(local_abs_max, abs(coc));
      }
    }

    if (local_index == 0u) {
      shared_max_slight_focus_abs_coc = floatBitsToUint(0.0f);
    }
    barrier();
    /* Use atomic reduce operation. */
    atomicMax(shared_max_slight_focus_abs_coc, floatBitsToUint(local_abs_max));
    /* "Broadcast" result across all threads. */
    barrier();

    return uintBitsToFloat(shared_max_slight_focus_abs_coc);
  }

  float3 dof_neighborhood_clamp(float2 frag_coord, float3 color, float center_coc, float weight)
  {
    /* Stabilize color by clamping with the stable half res neighborhood. */
    float3 neighbor_min, neighbor_max;
    constexpr float2 corners[4] = float2_array(
        float2(-1, -1), float2(1, -1), float2(-1, 1), float2(1, 1));
    for (int i = 0; i < 4; i++) {
      /**
       * Visit the 4 half-res texels around (and containing) the full-resolution texel.
       * Here a diagram of a full-screen texel (f) in the bottom left corner of a half res texel.
       * We sample the stable half-resolution texture at the 4 location denoted by (h).
       * ┌───────┬───────┐
       * │     h │     h │
       * │       │       │
       * │       │ f     │
       * ├───────┼───────┤
       * │     h │     h │
       * │       │       │
       * │       │       │
       * └───────┴───────┘
       */
      float2 uv_sample = ((frag_coord + corners[i]) * 0.5f) /
                         float2(textureSize(stable_color_tx, 0));
      /* Reminder: The content of this buffer is YCoCg + CoC. */
      float3 ycocg_sample = textureLod(stable_color_tx, uv_sample, 0.0f).rgb;
      neighbor_min = (i == 0) ? ycocg_sample : min(neighbor_min, ycocg_sample);
      neighbor_max = (i == 0) ? ycocg_sample : max(neighbor_max, ycocg_sample);
    }
    /* Pad the bounds in the near in focus region to get back a bit of detail. */
    float padding = 0.125f * saturate(1.0f - square(center_coc) / square(8.0f));
    neighbor_max += abs(neighbor_min) * padding;
    neighbor_min -= abs(neighbor_min) * padding;
    /* Progressively apply the clamp to avoid harsh transition. Also mask by weight. */
    float fac = saturate(square(max(0.0f, abs(center_coc) - 0.5f)) * 4.0f) * weight;
    /* Clamp in YCoCg space to avoid too much color drift. */
    color = colorspace::YCoCg_from_scene_linear(color);
    color = mix(color, clamp(color, neighbor_min, neighbor_max), fac);
    color = colorspace::scene_linear_from_YCoCg(color);
    return color;
  }
};

[[compute, local_size(DOF_RESOLVE_GROUP_SIZE, DOF_RESOLVE_GROUP_SIZE)]]
void comp_main([[resource_table]] Resources &srt,
               [[resource_table]] Tiles &tiles,
               [[global_invocation_id]] const uint3 global_id,
               [[local_invocation_index]] const uint local_index)
{
  [[resource_table]] Accumulator &accum = srt.accumulator;

  float2 frag_coord = float2(global_id.xy) + 0.5f;
  int2 tile_co = int2(frag_coord / float(DOF_TILES_SIZE * 2));

  CocTile coc_tile = dof_coc_tile_load(tiles.in_tiles_fg_img, tiles.in_tiles_bg_img, tile_co);
  CocTilePrediction prediction = dof_coc_tile_prediction_get(coc_tile, true);

  float2 uv = frag_coord / float2(textureSize(srt.color_tx, 0));
  float2 uv_halfres = (frag_coord * 0.5f) / float2(textureSize(srt.color_bg_tx, 0));

  float slight_focus_max_coc = 0.0f;
  if (prediction.do_slight_focus) {
    slight_focus_max_coc = srt.slight_focus_coc_tile_get(frag_coord, local_index);
    prediction.do_slight_focus = slight_focus_max_coc >= 0.5f;
    if (prediction.do_slight_focus) {
      prediction.do_focus = false;
    }
  }

  if (prediction.do_focus) {
    float depth = reverse_z::read(textureLod(srt.depth_tx, uv, 0.0f).r);
    float center_coc = (dof_coc_from_depth(accum.dof_buf, uv, depth));
    prediction.do_focus = abs(center_coc) <= 0.5f;
  }

  float4 out_color = float4(0.0f);
  float weight = 0.0f;

  float4 layer_color;
  float layer_weight;

  constexpr float3 hole_fill_color = float3(0.2f, 0.1f, 1.0f);
  constexpr float3 background_color = float3(0.1f, 0.2f, 1.0f);
  constexpr float3 slight_focus_color = float3(1.0f, 0.2f, 0.1f);
  constexpr float3 focus_color = float3(1.0f, 1.0f, 0.1f);
  constexpr float3 foreground_color = float3(0.2f, 1.0f, 0.1f);

  if (!no_hole_fill_pass && prediction.do_hole_fill) {
    layer_color = textureLod(srt.color_hole_fill_tx, uv_halfres, 0.0f);
    layer_weight = textureLod(srt.weight_hole_fill_tx, uv_halfres, 0.0f).r;
    if (srt.do_debug_color) {
      layer_color.rgb *= hole_fill_color;
    }
    out_color = layer_color * safe_rcp(layer_weight);
    weight = float(layer_weight > 0.0f);
  }

  if (!no_background_pass && prediction.do_background) {
    layer_color = textureLod(srt.color_bg_tx, uv_halfres, 0.0f);
    layer_weight = textureLod(srt.weight_bg_tx, uv_halfres, 0.0f).r;
    if (srt.do_debug_color) {
      layer_color.rgb *= background_color;
    }
    /* Always prefer background to hole_fill pass. */
    layer_color *= safe_rcp(layer_weight);
    layer_weight = float(layer_weight > 0.0f);
    /* Composite background. */
    out_color = out_color * (1.0f - layer_weight) + layer_color;
    weight = weight * (1.0f - layer_weight) + layer_weight;
    /* Fill holes with the composited background. */
    out_color *= safe_rcp(weight);
    weight = float(weight > 0.0f);
  }

  if (!no_slight_focus_pass && prediction.do_slight_focus) {
    float center_coc;
    if (accum.use_lut) [[static_branch]] {
      accum.dof_slight_focus_gather(float2(global_id.xy) + 0.5f,
                                    srt.depth_tx,
                                    srt.color_tx,
                                    accum.bokeh_lut_tx,
                                    slight_focus_max_coc,
                                    layer_color,
                                    layer_weight,
                                    center_coc);
    }
    else {
      accum.dof_slight_focus_gather(float2(global_id.xy) + 0.5f,
                                    srt.depth_tx,
                                    srt.color_tx,
                                    srt.color_tx, /* Dummy. */
                                    slight_focus_max_coc,
                                    layer_color,
                                    layer_weight,
                                    center_coc);
    }

    if (srt.do_debug_color) {
      layer_color.rgb *= slight_focus_color;
    }

    /* Composite slight defocus. */
    out_color = out_color * (1.0f - layer_weight) + layer_color;
    weight = weight * (1.0f - layer_weight) + layer_weight;

    // out_color.rgb = dof_neighborhood_clamp(frag_coord, out_color.rgb, center_coc, layer_weight);
  }

  if (!no_focus_pass && prediction.do_focus) {
    layer_color = colorspace::safe_color(textureLod(srt.color_tx, uv, 0.0f));
    layer_weight = 1.0f;
    if (srt.do_debug_color) {
      layer_color.rgb *= focus_color;
    }
    /* Composite in focus. */
    out_color = out_color * (1.0f - layer_weight) + layer_color;
    weight = weight * (1.0f - layer_weight) + layer_weight;
  }

  if (!no_foreground_pass && prediction.do_foreground) {
    layer_color = textureLod(srt.color_fg_tx, uv_halfres, 0.0f);
    layer_weight = textureLod(srt.weight_fg_tx, uv_halfres, 0.0f).r;
    if (srt.do_debug_color) {
      layer_color.rgb *= foreground_color;
    }
    /* Composite foreground. */
    out_color = out_color * (1.0f - layer_weight) + layer_color;
  }

  /* Fix float precision issue in alpha compositing. */
  if (out_color.a > 0.99f) {
    out_color.a = 1.0f;
  }

  if (debug_resolve_perf && prediction.do_slight_focus) {
    out_color.rgb *= float3(1.0f, 0.1f, 0.1f);
  }

  imageStore(srt.out_color_img, int2(global_id.xy), out_color);
}

}  // namespace eevee::dof::resolve

#ifndef GLSL_CPP_STUBS
PipelineCompute eevee_depth_of_field_resolve_lut(eevee::dof::resolve::comp_main,
                                                 eevee::dof::Accumulator{
                                                     .is_hole_fill = false,
                                                     .is_resolve = true,
                                                     .is_foreground = false,
                                                     .use_lut = true,
                                                 });
PipelineCompute eevee_depth_of_field_resolve_no_lut(eevee::dof::resolve::comp_main,
                                                    eevee::dof::Accumulator{
                                                        .is_hole_fill = false,
                                                        .is_resolve = true,
                                                        .is_foreground = false,
                                                        .use_lut = false,
                                                    });
#endif
