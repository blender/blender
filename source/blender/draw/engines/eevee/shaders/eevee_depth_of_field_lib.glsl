/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

/**
 * Depth of Field utils.
 */

#include "infos/eevee_common_infos.hh"

#include "draw_view_lib.glsl"
#include "gpu_shader_math_base_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Constants.
 * \{ */

#ifndef DOF_SLIGHT_FOCUS_DENSITY
#  define DOF_SLIGHT_FOCUS_DENSITY 2
#endif

#ifdef DOF_RESOLVE_PASS
#  define IS_RESOLVE true
#else
#  define IS_RESOLVE false
#endif
#ifdef DOF_FOREGROUND_PASS
#  define IS_FOREGROUND DOF_FOREGROUND_PASS
#else
#  define IS_FOREGROUND false
#endif
/* Debug options */
#define debug_gather_perf false
#define debug_scatter_perf false
#define debug_resolve_perf false

#define no_smooth_intersection false
#define no_gather_occlusion false
#define no_gather_mipmaps false
#define no_gather_random false
#define no_gather_filtering false
#define no_scatter_occlusion false
#define no_scatter_pass false
#define no_foreground_pass false
#define no_background_pass false
#define no_slight_focus_pass false
#define no_focus_pass false
#define no_hole_fill_pass false

/* Distribute weights between near/slight-focus/far fields (slide 117). */
#define dof_layer_threshold (4.0f)
/* Make sure it overlaps. */
#define dof_layer_offset_fg (0.5f + 1.0f)
/* Extra offset for convolution layers to avoid light leaking from background. */
#define dof_layer_offset (0.5f + 0.5f)

#define dof_max_slight_focus_radius DOF_MAX_SLIGHT_FOCUS_RADIUS

const uint2 quad_offsets_u[4] = uint2_array(uint2(0, 1), uint2(1, 1), uint2(1, 0), uint2(0, 0));
const float2 quad_offsets[4] = float2_array(
    float2(-0.5f, 0.5f), float2(0.5f, 0.5f), float2(0.5f, -0.5f), float2(-0.5f, -0.5f));

/** \} */

/* -------------------------------------------------------------------- */
/** \name Weighting and downsampling utils.
 * \{ */

float dof_hdr_color_weight(float4 color)
{
  /* Very fast "luma" weighting. */
  float luma = (color.g * 2.0f) + (color.r + color.b);
  /* TODO(fclem) Pass correct exposure. */
  constexpr float exposure = 1.0f;
  return 1.0f / (luma * exposure + 4.0f);
}

float dof_coc_select(float4 cocs)
{
  /* Select biggest coc. */
  float selected_coc = cocs.x;
  if (abs(cocs.y) > abs(selected_coc)) {
    selected_coc = cocs.y;
  }
  if (abs(cocs.z) > abs(selected_coc)) {
    selected_coc = cocs.z;
  }
  if (abs(cocs.w) > abs(selected_coc)) {
    selected_coc = cocs.w;
  }
  return selected_coc;
}

/* NOTE: Do not forget to normalize weights afterwards. */
float4 dof_bilateral_coc_weights(float4 cocs)
{
  float chosen_coc = dof_coc_select(cocs);

  constexpr float scale = 4.0f; /* TODO(fclem) revisit. */
  /* NOTE: The difference between the cocs should be inside a abs() function,
   * but we follow UE4 implementation to improve how dithered transparency looks (see slide 19). */
  return saturate(1.0f - (chosen_coc - cocs) * scale);
}

/* NOTE: Do not forget to normalize weights afterwards. */
float4 dof_bilateral_color_weights(float4 colors[4])
{
  float4 weights;
  for (int i = 0; i < 4; i++) {
    weights[i] = dof_hdr_color_weight(colors[i]);
  }
  return weights;
}

/* Returns signed Circle of confusion radius (in pixel) based on depth buffer value [0..1]. */
float dof_coc_from_depth(DepthOfFieldData dof_data, float2 uv, float depth)
{
  if (is_panoramic(dof_data.camera_type)) {
    /* Use radial depth. */
    depth = -length(drw_point_screen_to_view(float3(uv, depth)));
  }
  else {
    depth = drw_depth_screen_to_view(depth);
  }
  return coc_radius_from_camera_depth(dof_data, depth);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gather & Scatter Weighting
 * \{ */

float dof_layer_weight(float coc, const bool is_foreground)
{
  /* NOTE: These are full-resolution pixel CoC value. */
  if (IS_RESOLVE) {
    return saturate(-abs(coc) + dof_layer_threshold + dof_layer_offset) *
           float(is_foreground ? (coc <= 0.5f) : (coc > -0.5f));
  }
  else {
    coc *= 2.0f; /* Account for half pixel gather. */
    float threshold = dof_layer_threshold -
                      ((is_foreground) ? dof_layer_offset_fg : dof_layer_offset);
    return saturate(((is_foreground) ? -coc : coc) - threshold);
  }
}
float4 dof_layer_weight(float4 coc)
{
  /* NOTE: Used for scatter pass which already flipped the sign correctly. */
  coc *= 2.0f; /* Account for half pixel gather. */
  return saturate(coc - dof_layer_threshold + dof_layer_offset);
}

/* NOTE: This is half-resolution CoC radius. */
float dof_sample_weight(float coc)
{
#if 1 /* Optimized */
  return min(1.0f, 1.0f / square(coc));
#else
  /* Full intensity if CoC radius is below the pixel footprint. */
  constexpr float min_coc = 1.0f;
  coc = max(min_coc, abs(coc));
  return (M_PI * min_coc * min_coc) / (M_PI * coc * coc);
#endif
}
float4 dof_sample_weight(float4 coc)
{
#if 1 /* Optimized */
  return min(float4(1.0f), 1.0f / square(coc));
#else
  /* Full intensity if CoC radius is below the pixel footprint. */
  constexpr float min_coc = 1.0f;
  coc = max(float4(min_coc), abs(coc));
  return (M_PI * min_coc * min_coc) / (M_PI * coc * coc);
#endif
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle of Confusion tiles
 * \{ */

struct CocTile {
  float fg_min_coc;
  float fg_max_coc;
  float fg_max_intersectable_coc;
  float bg_min_coc;
  float bg_max_coc;
  float bg_min_intersectable_coc;
};

/* WATCH: Might have to change depending on the texture format. */
#define dof_tile_large_coc 1024.0f

/* Init a CoC tile for reduction algorithms. */
CocTile dof_coc_tile_init()
{
  CocTile tile;
  tile.fg_min_coc = 0.0f;
  tile.fg_max_coc = -dof_tile_large_coc;
  tile.fg_max_intersectable_coc = dof_tile_large_coc;
  tile.bg_min_coc = dof_tile_large_coc;
  tile.bg_max_coc = 0.0f;
  tile.bg_min_intersectable_coc = dof_tile_large_coc;
  return tile;
}

CocTile dof_coc_tile_unpack(float3 fg, float3 bg)
{
  CocTile tile;
  tile.fg_min_coc = -fg.x;
  tile.fg_max_coc = -fg.y;
  tile.fg_max_intersectable_coc = -fg.z;
  tile.bg_min_coc = bg.x;
  tile.bg_max_coc = bg.y;
  tile.bg_min_intersectable_coc = bg.z;
  return tile;
}

/* WORKAROUND(@fclem): GLSL compilers differs in what qualifiers are requires to pass images as
 * parameters. Workaround by using defines. */
#define dof_coc_tile_load(tiles_fg_img_, tiles_bg_img_, texel_) \
  dof_coc_tile_unpack( \
      imageLoad(tiles_fg_img_, clamp(texel_, int2(0), imageSize(tiles_fg_img_) - 1)).xyz, \
      imageLoad(tiles_bg_img_, clamp(texel_, int2(0), imageSize(tiles_bg_img_) - 1)).xyz)

void dof_coc_tile_pack(CocTile tile, out float3 out_fg, out float3 out_bg)
{
  out_fg.x = -tile.fg_min_coc;
  out_fg.y = -tile.fg_max_coc;
  out_fg.z = -tile.fg_max_intersectable_coc;
  out_bg.x = tile.bg_min_coc;
  out_bg.y = tile.bg_max_coc;
  out_bg.z = tile.bg_min_intersectable_coc;
}

#define dof_coc_tile_store(tiles_fg_img_, tiles_bg_img_, texel_out_, tile_data_) \
  if (true) { \
    float3 out_fg; \
    float3 out_bg; \
    dof_coc_tile_pack(tile_data_, out_fg, out_bg); \
    imageStore(tiles_fg_img_, texel_out_, out_fg.xyzz); \
    imageStore(tiles_bg_img_, texel_out_, out_bg.xyzz); \
  }

bool dof_do_fast_gather(float max_absolute_coc, float min_absolute_coc, const bool is_foreground)
{
  float min_weight = dof_layer_weight((is_foreground) ? -min_absolute_coc : min_absolute_coc,
                                      is_foreground);
  if (min_weight < 1.0f) {
    return false;
  }
  /* FIXME(fclem): This is a workaround to fast gather triggering too early. Since we use custom
   * opacity mask, the opacity is not given to be 100% even for after normal threshold. */
  if (is_foreground && min_absolute_coc < dof_layer_threshold) {
    return false;
  }
  return (max_absolute_coc - min_absolute_coc) < (DOF_FAST_GATHER_COC_ERROR * max_absolute_coc);
}

struct CocTilePrediction {
  bool do_foreground;
  bool do_slight_focus;
  bool do_focus;
  bool do_background;
  bool do_hole_fill;
};

/**
 * Using the tile CoC infos, predict which convolutions are required and the ones that can be
 * skipped.
 */
CocTilePrediction dof_coc_tile_prediction_get(CocTile tile)
{
  /* Based on tile value, predict what pass we need to load. */
  CocTilePrediction predict;

  predict.do_foreground = (-tile.fg_min_coc > dof_layer_threshold - dof_layer_offset_fg);
  bool fg_fully_opaque = predict.do_foreground &&
                         dof_do_fast_gather(-tile.fg_min_coc, -tile.fg_max_coc, true);
  predict.do_background = !fg_fully_opaque &&
                          (tile.bg_max_coc > dof_layer_threshold - dof_layer_offset);
#if 0 /* Unused. */
  bool bg_fully_opaque = predict.do_background &&
                         dof_do_fast_gather(-tile.bg_max_coc, tile.bg_min_coc, false);
#endif
  predict.do_hole_fill = !fg_fully_opaque && -tile.fg_min_coc > 0.0f;
  predict.do_focus = !fg_fully_opaque;
  predict.do_slight_focus = !fg_fully_opaque;

#if 0 /* Debug */
  predict.do_foreground = predict.do_background = predict.do_hole_fill = true;
#endif
  return predict;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gathering
 * \{ */

/**
 * Generate samples in a square pattern with the ring radius. X is the center tile.
 *
 *    Dist1          Dist2
 *                 6 5 4 3 2
 *    3 2 1        7       1
 *    . X 0        .   X   0
 *    . . .        .       .
 *                 . . . . .
 *
 * Samples are expected to be mirrored to complete the pattern.
 */
int2 dof_square_ring_sample_offset(int ring_distance, int sample_id)
{
  int2 offset;
  if (sample_id < ring_distance) {
    offset.x = ring_distance;
    offset.y = sample_id;
  }
  else if (sample_id < ring_distance * 3) {
    offset.x = ring_distance - sample_id + ring_distance;
    offset.y = ring_distance;
  }
  else {
    offset.x = -ring_distance;
    offset.y = ring_distance - sample_id + 3 * ring_distance;
  }
  return offset;
}

/** \} */
