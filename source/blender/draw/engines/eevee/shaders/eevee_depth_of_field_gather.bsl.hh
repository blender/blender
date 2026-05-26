/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_depth_of_field_accumulator.bsl.hh"
#include "eevee_depth_of_field_tiles.bsl.hh"

namespace eevee::dof {

struct Gather {
  [[legacy_info]] ShaderCreateInfo draw_view;

  [[resource_table]] srt_t<Accumulator> accumulator;

  [[sampler(0)]] sampler2D color_tx;
  [[sampler(1)]] sampler2D color_bilinear_tx;
  [[sampler(2)]] sampler2D coc_tx;

  [[image(2, write, SFLOAT_16_16_16_16)]] image2D out_color_img;
  [[image(3, write, SFLOAT_16)]] image2D out_weight_img;

  [[compilation_constant]] const bool is_foreground;
  [[compilation_constant]] const bool use_lut;
};

/**
 * Gather pass: Convolve foreground and background parts in separate passes.
 *
 * Using the min&max CoC tile buffer, we select the best appropriate method to blur the scene
 * color. A fast gather path is taken if there is not many CoC variation inside the tile.
 *
 * We sample using an octaweb sampling pattern. We randomize the kernel center and each ring
 * rotation to ensure maximum coverage.
 *
 * Outputs:
 * - Color * Weight, Weight, Occlusion 'CoC' Depth (mean and variance)
 */
namespace gather {

struct Resources {
  [[image(4, write, SFLOAT_16_16)]] image2D out_occlusion_img;
};

[[compute, local_size(DOF_GATHER_GROUP_SIZE, DOF_GATHER_GROUP_SIZE)]]
void comp_main([[resource_table]] Resources &srt,
               [[resource_table]] Gather &gather,
               [[resource_table]] const Tiles &tiles,
               [[global_invocation_id]] const uint3 global_id,
               [[local_invocation_id]] const uint3 local_id,
               [[local_invocation_index]] const uint local_index)
{
  [[resource_table]] Accumulator &accum = gather.accumulator;

  int2 tile_co = int2(global_id.xy / DOF_TILES_SIZE);
  CocTile coc_tile = dof_coc_tile_load(tiles.in_tiles_fg_img, tiles.in_tiles_bg_img, tile_co);
  CocTilePrediction prediction = dof_coc_tile_prediction_get(coc_tile, false);

  float base_radius, min_radius, min_intersectable_radius;
  bool can_early_out;
  if (gather.is_foreground) {
    base_radius = -coc_tile.fg_min_coc;
    min_radius = -coc_tile.fg_max_coc;
    min_intersectable_radius = -coc_tile.fg_max_intersectable_coc;
    can_early_out = !prediction.do_foreground;
  }
  else {
    base_radius = coc_tile.bg_max_coc;
    min_radius = coc_tile.bg_min_coc;
    min_intersectable_radius = coc_tile.bg_min_intersectable_coc;
    can_early_out = !prediction.do_background;
  }

  bool do_fast_gather = dof_do_fast_gather(base_radius, min_radius, accum.is_foreground, false);

  /* Gather at half resolution. Divide CoC by 2. */
  base_radius *= 0.5f;
  min_intersectable_radius *= 0.5f;

  bool do_density_change = accum.dof_do_density_change(base_radius, min_intersectable_radius);

  float4 out_color;
  float out_weight;
  float2 out_occlusion;

  if (can_early_out) {
    out_color = float4(0.0f);
    out_weight = 0.0f;
    out_occlusion = float2(0.0f, 0.0f);
  }
  else if (do_fast_gather) {
    accum.dof_gather_accumulator(float2(global_id.xy),
                                 gather.color_tx,
                                 gather.color_bilinear_tx,
                                 gather.coc_tx,
                                 base_radius,
                                 min_intersectable_radius,
                                 true,
                                 false,
                                 out_color,
                                 out_weight,
                                 out_occlusion);
  }
  else if (do_density_change) {
    accum.dof_gather_accumulator(float2(global_id.xy),
                                 gather.color_tx,
                                 gather.color_bilinear_tx,
                                 gather.coc_tx,
                                 base_radius,
                                 min_intersectable_radius,
                                 false,
                                 true,
                                 out_color,
                                 out_weight,
                                 out_occlusion);
  }
  else {
    accum.dof_gather_accumulator(float2(global_id.xy),
                                 gather.color_tx,
                                 gather.color_bilinear_tx,
                                 gather.coc_tx,
                                 base_radius,
                                 min_intersectable_radius,
                                 false,
                                 false,
                                 out_color,
                                 out_weight,
                                 out_occlusion);
  }

  int2 out_texel = int2(global_id.xy);
  imageStore(gather.out_color_img, out_texel, out_color);
  imageStore(gather.out_weight_img, out_texel, float4(out_weight));
  imageStore(srt.out_occlusion_img, out_texel, out_occlusion.xyxy);
}

}  // namespace gather

/**
 * Hole-fill pass: Gather background parts where foreground is present.
 *
 * Using the min&max CoC tile buffer, we select the best appropriate method to blur the scene
 * color. A fast gather path is taken if there is not many CoC variation inside the tile.
 *
 * We sample using an octaweb sampling pattern. We randomize the kernel center and each ring
 * rotation to ensure maximum coverage.
 */
namespace hole_fill {

[[compute, local_size(DOF_GATHER_GROUP_SIZE, DOF_GATHER_GROUP_SIZE)]]
void comp_main([[resource_table]] Gather &gather,
               [[resource_table]] const Tiles &tiles,
               [[global_invocation_id]] const uint3 global_id,
               [[local_invocation_id]] const uint3 local_id,
               [[local_invocation_index]] const uint local_index)
{
  [[resource_table]] Accumulator &accum = gather.accumulator;

  int2 tile_co = int2(global_id.xy / DOF_TILES_SIZE);
  CocTile coc_tile = dof_coc_tile_load(tiles.in_tiles_fg_img, tiles.in_tiles_bg_img, tile_co);
  CocTilePrediction prediction = dof_coc_tile_prediction_get(coc_tile, false);

  float base_radius = -coc_tile.fg_min_coc;
  float min_radius = -coc_tile.fg_max_coc;
  float min_intersectable_radius = dof_tile_large_coc;
  bool can_early_out = !prediction.do_hole_fill;

  bool do_fast_gather = dof_do_fast_gather(base_radius, min_radius, accum.is_foreground, false);

  /* Gather at half resolution. Divide CoC by 2. */
  base_radius *= 0.5f;
  min_intersectable_radius *= 0.5f;

  float4 out_color = float4(0.0f);
  float out_weight = 0.0f;
  float2 unused_occlusion = float2(0.0f, 0.0f);

  if (can_early_out) {
    /* Early out. */
  }
  else if (do_fast_gather) {
    accum.dof_gather_accumulator(float2(global_id.xy),
                                 gather.color_tx,
                                 gather.color_bilinear_tx,
                                 gather.coc_tx,
                                 base_radius,
                                 min_intersectable_radius,
                                 true,
                                 false,
                                 out_color,
                                 out_weight,
                                 unused_occlusion);
  }
  else {
    accum.dof_gather_accumulator(float2(global_id.xy),
                                 gather.color_tx,
                                 gather.color_bilinear_tx,
                                 gather.coc_tx,
                                 base_radius,
                                 min_intersectable_radius,
                                 false,
                                 false,
                                 out_color,
                                 out_weight,
                                 unused_occlusion);
  }

  int2 out_texel = int2(global_id.xy);
  imageStore(gather.out_color_img, out_texel, out_color);
  imageStore(gather.out_weight_img, out_texel, float4(out_weight));
}

}  // namespace hole_fill

}  // namespace eevee::dof

#ifndef GLSL_CPP_STUBS
PipelineCompute eevee_depth_of_field_gather_background_lut(eevee::dof::gather::comp_main,
                                                           eevee::dof::Accumulator{
                                                               .is_hole_fill = false,
                                                               .is_resolve = false,
                                                               .is_foreground = false,
                                                               .use_lut = true,
                                                           });
PipelineCompute eevee_depth_of_field_gather_background_no_lut(eevee::dof::gather::comp_main,
                                                              eevee::dof::Accumulator{
                                                                  .is_hole_fill = false,
                                                                  .is_resolve = false,
                                                                  .is_foreground = false,
                                                                  .use_lut = false,
                                                              });
PipelineCompute eevee_depth_of_field_gather_foreground_lut(eevee::dof::gather::comp_main,
                                                           eevee::dof::Accumulator{
                                                               .is_hole_fill = false,
                                                               .is_resolve = false,
                                                               .is_foreground = true,
                                                               .use_lut = true,
                                                           });
PipelineCompute eevee_depth_of_field_gather_foreground_no_lut(eevee::dof::gather::comp_main,
                                                              eevee::dof::Accumulator{
                                                                  .is_hole_fill = false,
                                                                  .is_resolve = false,
                                                                  .is_foreground = true,
                                                                  .use_lut = false,
                                                              });

PipelineCompute eevee_depth_of_field_hole_fill(eevee::dof::hole_fill::comp_main,
                                               eevee::dof::Accumulator{
                                                   .is_hole_fill = true,
                                                   .is_resolve = false,
                                                   .is_foreground = false,
                                                   .use_lut = false,
                                               });
#endif
