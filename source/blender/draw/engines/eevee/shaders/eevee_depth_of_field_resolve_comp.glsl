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

#include "infos/eevee_depth_of_field_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_depth_of_field_resolve)

#include "eevee_depth_of_field_accumulator_lib.glsl"

/* Workarounds for Metal/AMD issue where atomicMax lead to incorrect results.
 * See #123052 */
#if defined(GPU_METAL)
#  define threadgroup_size (gl_WorkGroupSize.x * gl_WorkGroupSize.y)
shared float array_of_values[threadgroup_size];

/* Only works for 2D thread-groups where the size is a power of 2. */
float parallelMax(const float value)
{
  uint thread_id = gl_LocalInvocationIndex;
  array_of_values[thread_id] = value;
  barrier();

  for (uint i = threadgroup_size; i > 0; i >>= 1) {
    uint half_width = i >> 1;
    if (thread_id < half_width) {
      array_of_values[thread_id] = max(array_of_values[thread_id],
                                       array_of_values[thread_id + half_width]);
    }
    barrier();
  }

  return array_of_values[0];
}
#endif

shared uint shared_max_slight_focus_abs_coc;

/**
 * Returns The max CoC in the Slight Focus range inside this compute tile.
 */
float dof_slight_focus_coc_tile_get(float2 frag_coord)
{
  float local_abs_max = 0.0f;
  /* Sample in a cross (X) pattern. This covers all pixels over the whole tile, as long as
   * dof_max_slight_focus_radius is less than the group size. */
  for (int i = 0; i < 4; i++) {
    float2 sample_uv = (frag_coord + quad_offsets[i] * 2.0f * dof_max_slight_focus_radius) /
                       float2(textureSize(color_tx, 0));
    float depth = reverse_z::read(textureLod(depth_tx, sample_uv, 0.0f).r);
    float coc = dof_coc_from_depth(dof_buf, sample_uv, depth);
    coc = clamp(coc, -dof_buf.coc_abs_max, dof_buf.coc_abs_max);
    if (abs(coc) < dof_max_slight_focus_radius) {
      local_abs_max = max(local_abs_max, abs(coc));
    }
  }

#if defined(GPU_METAL) && defined(GPU_ATI)
  return parallelMax(local_abs_max);

#else
  if (gl_LocalInvocationIndex == 0u) {
    shared_max_slight_focus_abs_coc = floatBitsToUint(0.0f);
  }
  barrier();
  /* Use atomic reduce operation. */
  atomicMax(shared_max_slight_focus_abs_coc, floatBitsToUint(local_abs_max));
  /* "Broadcast" result across all threads. */
  barrier();

  return uintBitsToFloat(shared_max_slight_focus_abs_coc);
#endif
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
  color = colorspace_YCoCg_from_scene_linear(color);
  color = mix(color, clamp(color, neighbor_min, neighbor_max), fac);
  color = colorspace_scene_linear_from_YCoCg(color);
  return color;
}

void main()
{
  float2 frag_coord = float2(gl_GlobalInvocationID.xy) + 0.5f;
  int2 tile_co = int2(frag_coord / float(DOF_TILES_SIZE * 2));

  CocTile coc_tile = dof_coc_tile_load(in_tiles_fg_img, in_tiles_bg_img, tile_co);
  CocTilePrediction prediction = dof_coc_tile_prediction_get(coc_tile);

  float2 uv = frag_coord / float2(textureSize(color_tx, 0));
  float2 uv_halfres = (frag_coord * 0.5f) / float2(textureSize(color_bg_tx, 0));

  float slight_focus_max_coc = 0.0f;
  if (prediction.do_slight_focus) {
    slight_focus_max_coc = dof_slight_focus_coc_tile_get(frag_coord);
    prediction.do_slight_focus = slight_focus_max_coc >= 0.5f;
    if (prediction.do_slight_focus) {
      prediction.do_focus = false;
    }
  }

  if (prediction.do_focus) {
    float depth = reverse_z::read(textureLod(depth_tx, uv, 0.0f).r);
    float center_coc = (dof_coc_from_depth(dof_buf, uv, depth));
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
    layer_color = textureLod(color_hole_fill_tx, uv_halfres, 0.0f);
    layer_weight = textureLod(weight_hole_fill_tx, uv_halfres, 0.0f).r;
    if (do_debug_color) {
      layer_color.rgb *= hole_fill_color;
    }
    out_color = layer_color * safe_rcp(layer_weight);
    weight = float(layer_weight > 0.0f);
  }

  if (!no_background_pass && prediction.do_background) {
    layer_color = textureLod(color_bg_tx, uv_halfres, 0.0f);
    layer_weight = textureLod(weight_bg_tx, uv_halfres, 0.0f).r;
    if (do_debug_color) {
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
    dof_slight_focus_gather(depth_tx,
                            color_tx,
                            bokeh_lut_tx,
                            slight_focus_max_coc,
                            layer_color,
                            layer_weight,
                            center_coc);
    if (do_debug_color) {
      layer_color.rgb *= slight_focus_color;
    }

    /* Composite slight defocus. */
    out_color = out_color * (1.0f - layer_weight) + layer_color;
    weight = weight * (1.0f - layer_weight) + layer_weight;

    // out_color.rgb = dof_neighborhood_clamp(frag_coord, out_color.rgb, center_coc, layer_weight);
  }

  if (!no_focus_pass && prediction.do_focus) {
    layer_color = colorspace_safe_color(textureLod(color_tx, uv, 0.0f));
    layer_weight = 1.0f;
    if (do_debug_color) {
      layer_color.rgb *= focus_color;
    }
    /* Composite in focus. */
    out_color = out_color * (1.0f - layer_weight) + layer_color;
    weight = weight * (1.0f - layer_weight) + layer_weight;
  }

  if (!no_foreground_pass && prediction.do_foreground) {
    layer_color = textureLod(color_fg_tx, uv_halfres, 0.0f);
    layer_weight = textureLod(weight_fg_tx, uv_halfres, 0.0f).r;
    if (do_debug_color) {
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

  imageStore(out_color_img, int2(gl_GlobalInvocationID.xy), out_color);
}
