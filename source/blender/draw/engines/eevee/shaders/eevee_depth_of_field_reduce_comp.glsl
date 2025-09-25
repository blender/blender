/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Reduce copy pass: filter fireflies and split color between scatter and gather input.
 *
 * NOTE: The texture can end up being too big because of the mipmap padding. We correct for
 * that during the convolution phase.
 *
 * Inputs:
 * - Output of setup pass (half-resolution) and reduce downsample pass (quarter res).
 * Outputs:
 * - Half-resolution padded to avoid mipmap misalignment (so possibly not matching input size).
 * - Gather input color (whole mip chain), Scatter rect list, Signed CoC (whole mip chain).
 */

#include "infos/eevee_depth_of_field_infos.hh"

COMPUTE_SHADER_CREATE_INFO(eevee_depth_of_field_reduce)

#include "eevee_depth_of_field_lib.glsl"
#include "gpu_shader_math_safe_lib.glsl"
#include "gpu_shader_math_vector_reduce_lib.glsl"

/* NOTE: Do not compare alpha as it is not scattered by the scatter pass. */
float dof_scatter_neighborhood_rejection(float3 color)
{
  color = min(float3(dof_buf.scatter_neighbor_max_color), color);

  float validity = 0.0f;

  /* Centered in the middle of 4 quarter res texel. */
  float2 texel_size = 1.0f / float2(textureSize(downsample_tx, 0).xy);
  float2 uv = ((float2(gl_GlobalInvocationID.xy) + 0.5f) * 0.5f) * texel_size;

  for (int i = 0; i < 4; i++) {
    float2 sample_uv = uv + quad_offsets[i] * texel_size;
    float3 ref = textureLod(downsample_tx, sample_uv, 0.0f).rgb;

    ref = min(float3(dof_buf.scatter_neighbor_max_color), ref);
    float diff = reduce_max(max(float3(0.0f), abs(ref - color)));

    constexpr float rejection_threshold = 0.7f;
    diff = saturate(diff / rejection_threshold - 1.0f);
    validity = max(validity, diff);
  }

  return validity;
}

/* This avoids Bokeh sprite popping in and out at the screen border and
 * drawing Bokeh sprites larger than the screen. */
float dof_scatter_screen_border_rejection(float coc, int2 texel)
{
  float2 screen_size = float2(imageSize(inout_color_lod0_img));
  float2 uv = (float2(texel) + 0.5f) / screen_size;
  float2 screen_pos = uv * screen_size;
  float min_screen_border_distance = reduce_min(min(screen_pos, screen_size - screen_pos));
  /* Full-resolution to half-resolution CoC. */
  coc *= 0.5f;
  /* Allow 10px transition. */
  constexpr float rejection_hardness = 1.0f / 10.0f;
  return saturate((min_screen_border_distance - abs(coc)) * rejection_hardness + 1.0f);
}

float dof_scatter_luminosity_rejection(float3 color)
{
  constexpr float rejection_hardness = 1.0f;
  return saturate(reduce_max(color - dof_buf.scatter_color_threshold) * rejection_hardness);
}

float dof_scatter_coc_radius_rejection(float coc)
{
  constexpr float rejection_hardness = 0.3f;
  return saturate((abs(coc) - dof_buf.scatter_coc_threshold) * rejection_hardness);
}

float fast_luma(float3 color)
{
  return (2.0f * color.g) + color.r + color.b;
}

#define cache_size (gl_WorkGroupSize.x)
shared float4 color_cache[cache_size][cache_size];
shared float coc_cache[cache_size][cache_size];

void store_color_cache(uint2 coord, float4 value)
{
  color_cache[coord.y][coord.x] = value;
}
float4 load_color_cache(uint2 coord)
{
  return color_cache[coord.y][coord.x];
}

void store_coc_cache(uint2 coord, float value)
{
  coc_cache[coord.y][coord.x] = value;
}
float load_coc_cache(uint2 coord)
{
  return coc_cache[coord.y][coord.x];
}
float4 load4_coc_cache(uint2 coord)
{
  return float4(load_coc_cache(coord + quad_offsets_u[0]),
                load_coc_cache(coord + quad_offsets_u[1]),
                load_coc_cache(coord + quad_offsets_u[2]),
                load_coc_cache(coord + quad_offsets_u[3]));
}

void main()
{
  int2 texel = min(int2(gl_GlobalInvocationID.xy), imageSize(inout_color_lod0_img) - 1);
  uint2 thread_co = gl_LocalInvocationID.xy;

  float4 local_color = imageLoad(inout_color_lod0_img, texel);
  float local_coc = imageLoad(in_coc_lod0_img, texel).r;

  /* Only scatter if luminous enough. */
  float do_scatter = dof_scatter_luminosity_rejection(local_color.rgb);
  /* Only scatter if CoC is big enough. */
  do_scatter *= dof_scatter_coc_radius_rejection(local_coc);
  /* Only scatter if CoC is not too big to avoid performance issues. */
  do_scatter *= dof_scatter_screen_border_rejection(local_coc, texel);
  /* Only scatter if neighborhood is different enough. */
  do_scatter *= dof_scatter_neighborhood_rejection(local_color.rgb);
  /* For debugging. */
  if (no_scatter_pass) {
    do_scatter = 0.0f;
  }

  /* Use coc_cache for broadcasting the do_scatter result. */
  store_coc_cache(thread_co, do_scatter);
  barrier();

  /* Load the same value for each thread quad. */
  float4 do_scatter4 = load4_coc_cache(thread_co & ~1u);
  barrier();

  /* Load level 0 into cache. */
  store_color_cache(thread_co, local_color);
  store_coc_cache(thread_co, local_coc);
  barrier();

  /* Add a scatter sprite for each 2x2 pixel neighborhood passing the threshold. */
  bool any_scatter = any(greaterThan(do_scatter4, float4(0.0f)));
  if (all(equal(thread_co & 1u, uint2(0))) && any_scatter) {
    /* Apply energy conservation to anamorphic scattered bokeh. */
    do_scatter4 *= reduce_max(dof_buf.bokeh_anisotropic_scale_inv);
    /* Circle of Confusion. */
    float4 coc4 = load4_coc_cache(thread_co);
    /* We are scattering at half resolution, so divide CoC by 2. */
    coc4 *= 0.5f;
    /* Sprite center position. Center sprite around the 4 texture taps. */
    float2 offset = float2(gl_GlobalInvocationID.xy) + 1;
    /* Add 2.5 to max_coc because the max_coc may not be centered on the sprite origin
     * and because we smooth the bokeh shape a bit in the pixel shader. */
    float2 half_extent = reduce_max(abs(coc4)) * dof_buf.bokeh_anisotropic_scale + 2.5f;
    /* Follows quad_offsets order. */
    float3 color4_0 = load_color_cache(thread_co + quad_offsets_u[0]).rgb;
    float3 color4_1 = load_color_cache(thread_co + quad_offsets_u[1]).rgb;
    float3 color4_2 = load_color_cache(thread_co + quad_offsets_u[2]).rgb;
    float3 color4_3 = load_color_cache(thread_co + quad_offsets_u[3]).rgb;
    /* Issue a sprite for each field if any CoC matches. */
    if (any(lessThan(do_scatter4 * sign(coc4), float4(0.0f)))) {
      /* Same value for all threads. Not an issue if we don't sync access to it. */
      scatter_fg_indirect_buf.vertex_len = 4u;
      /* Issue 1 strip instance per sprite. */
      uint rect_id = atomicAdd(scatter_fg_indirect_buf.instance_len, 1u);
      if (rect_id < dof_buf.scatter_max_rect) {

        float4 coc4_fg = max(float4(0.0f), -coc4);
        float4 fg_weights = dof_layer_weight(coc4_fg) * dof_sample_weight(coc4_fg) * do_scatter4;
        /* Filter NaNs. */
        fg_weights = select(fg_weights, float4(0.0f), equal(coc4_fg, float4(0.0f)));

        ScatterRect rect_fg;
        rect_fg.offset = offset;
        /* Negate extent to flip the sprite. Mimics optical phenomenon. */
        rect_fg.half_extent = -half_extent;
        /* NOTE: Since we flipped the quad along (1,-1) line, we need to also swap the (1,1) and
         * (0,0) values so that quad_offsets is in the right order in the vertex shader. */

        /* Circle of Confusion absolute radius in half-resolution pixels. */
        rect_fg.color_and_coc[0].a = coc4_fg[0];
        rect_fg.color_and_coc[1].a = coc4_fg[3];
        rect_fg.color_and_coc[2].a = coc4_fg[2];
        rect_fg.color_and_coc[3].a = coc4_fg[1];
        /* Apply weights. */
        rect_fg.color_and_coc[0].rgb = color4_0 * fg_weights[0];
        rect_fg.color_and_coc[1].rgb = color4_3 * fg_weights[3];
        rect_fg.color_and_coc[2].rgb = color4_2 * fg_weights[2];
        rect_fg.color_and_coc[3].rgb = color4_1 * fg_weights[1];

        scatter_fg_list_buf[rect_id] = rect_fg;
      }
    }
    if (any(greaterThan(do_scatter4 * sign(coc4), float4(0.0f)))) {
      /* Same value for all threads. Not an issue if we don't sync access to it. */
      scatter_bg_indirect_buf.vertex_len = 4u;
      /* Issue 1 strip instance per sprite. */
      uint rect_id = atomicAdd(scatter_bg_indirect_buf.instance_len, 1u);
      if (rect_id < dof_buf.scatter_max_rect) {
        float4 coc4_bg = max(float4(0.0f), coc4);
        float4 bg_weights = dof_layer_weight(coc4_bg) * dof_sample_weight(coc4_bg) * do_scatter4;
        /* Filter NaNs. */
        bg_weights = select(bg_weights, float4(0.0f), equal(coc4_bg, float4(0.0f)));

        ScatterRect rect_bg;
        rect_bg.offset = offset;
        rect_bg.half_extent = half_extent;

        /* Circle of Confusion absolute radius in half-resolution pixels. */
        rect_bg.color_and_coc[0].a = coc4_bg[0];
        rect_bg.color_and_coc[1].a = coc4_bg[1];
        rect_bg.color_and_coc[2].a = coc4_bg[2];
        rect_bg.color_and_coc[3].a = coc4_bg[3];
        /* Apply weights. */
        rect_bg.color_and_coc[0].rgb = color4_0 * bg_weights[0];
        rect_bg.color_and_coc[1].rgb = color4_1 * bg_weights[1];
        rect_bg.color_and_coc[2].rgb = color4_2 * bg_weights[2];
        rect_bg.color_and_coc[3].rgb = color4_3 * bg_weights[3];

        scatter_bg_list_buf[rect_id] = rect_bg;
      }
    }
  }

  /* Remove scatter color from gather. */
  float4 color_lod0 = load_color_cache(thread_co) * (1.0f - do_scatter);
  store_color_cache(thread_co, color_lod0);

  imageStore(inout_color_lod0_img, texel, color_lod0);

  /* Recursive downsample. */
  for (uint i = 1u; i < DOF_MIP_COUNT; i++) {
    barrier();
    uint mask = ~(~0u << i);
    if (all(equal(gl_LocalInvocationID.xy & mask, uint2(0)))) {
      uint ofs = 1u << (i - 1u);

      /* TODO(fclem): Could use wave shuffle intrinsics to avoid LDS as suggested by the paper. */
      float4 coc4;
      coc4[0] = load_coc_cache(thread_co + uint2(ofs, 0));
      coc4[1] = load_coc_cache(thread_co + uint2(ofs, ofs));
      coc4[2] = load_coc_cache(thread_co + uint2(0, ofs));
      coc4[3] = load_coc_cache(thread_co + uint2(0, 0));
      float4 colors[4];
      colors[0] = load_color_cache(thread_co + uint2(ofs, 0));
      colors[1] = load_color_cache(thread_co + uint2(ofs, ofs));
      colors[2] = load_color_cache(thread_co + uint2(0, ofs));
      colors[3] = load_color_cache(thread_co + uint2(0, 0));

      float4 weights = dof_bilateral_coc_weights(coc4) * dof_bilateral_color_weights(colors);
      /* Normalize so that the sum is 1. */
      weights *= safe_rcp(reduce_add(weights));

      float4 color_lod = weighted_sum_array(colors, weights);
      float coc_lod = dot(coc4, weights);

      store_color_cache(thread_co, color_lod);
      store_coc_cache(thread_co, coc_lod);

      int2 texel = int2(gl_GlobalInvocationID.xy >> i);

      if (i == 1) {
        imageStore(out_color_lod1_img, texel, color_lod);
        imageStore(out_coc_lod1_img, texel, float4(coc_lod));
      }
      else if (i == 2) {
        imageStore(out_color_lod2_img, texel, color_lod);
        imageStore(out_coc_lod2_img, texel, float4(coc_lod));
      }
      else /* if (i == 3) */ {
        imageStore(out_color_lod3_img, texel, color_lod);
        imageStore(out_coc_lod3_img, texel, float4(coc_lod));
      }
    }
  }
}
