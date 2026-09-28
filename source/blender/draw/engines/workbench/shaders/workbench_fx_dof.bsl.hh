/* SPDX-FileCopyrightText: 2019-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Separable Hexagonal Bokeh Blur by Colin Barré-Brisebois
 * https://colinbarrebrisebois.com/2017/04/18/hexagonal-bokeh-blur-revisited-part-1-basic-3-pass-version/
 * Converted and adapted from HLSL to GLSL by Clément Foucault
 */

#pragma once

#include "draw_view.bsl.hh"
#include "gpu_shader_fullscreen.bsl.hh"
#include "gpu_shader_math_safe.bsl.hh"
#include "gpu_shader_math_vector_reduce.bsl.hh"

namespace workbench::dof {

[[vertex]]
void vert([[vertex_id]] const int vert_id, [[position]] float4 &position)
{
  fullscreen_vertex(vert_id, position);
}

#define MAX_COC_SIZE 100.0f

float2 dof_encode_coc(float near, float far)
{
  return float2(near, far) / MAX_COC_SIZE;
}
float dof_decode_coc(float2 cocs)
{
  return max(cocs.x, cocs.y) * MAX_COC_SIZE;
}
float dof_decode_signed_coc(float2 cocs)
{
  return ((cocs.x > cocs.y) ? cocs.x : -cocs.y) * MAX_COC_SIZE;
}

float4 sum_weighted(float4 val0, float4 val1, float4 val2, float4 val3, float4 weights)
{
  return (val0 * weights[0] + val1 * weights[1] + val2 * weights[2] + val3 * weights[3]) *
         safe_rcp(weights[0] + weights[1] + weights[2] + weights[3]);
}

struct DepthOfField {
  [[push_constant]] float2 inverted_viewport_size;
  [[push_constant]] float2 near_far;
  [[push_constant]] float3 dof_params;
  [[push_constant]] float noise_offset;

  /* divide by sensor size to get the normalized size */
  template<typename T> T calculate_coc(T z_depth)
  {
    float aperture_size = dof_params.x;
    float distance = dof_params.y;
    float invsensor_size = dof_params.z;
    return aperture_size * (distance / z_depth - 1.0f) * invsensor_size;
  }
};

template float DepthOfField::calculate_coc<float>(float);
template float4 DepthOfField::calculate_coc<float4>(float4);

/*
 * NOTE: Keep the sampler bind points consistent between the steps.
 *
 * [[sampler(0)]] sampler2D input_coc_tx;
 * [[sampler(1)]] sampler2D scene_color_tx;
 * [[sampler(2)]] sampler2D scene_depth_tx;
 * [[sampler(3)]] sampler2D half_res_color_tx;
 * [[sampler(4)]] sampler2D blur_tx;
 * [[sampler(5)]] sampler2D noise_tx;
 */

/**
 * ----------------- STEP 0 ------------------
 * Custom COC aware down-sampling. Half res pass.
 */

struct Prepare {
  [[sampler(1)]] sampler2D scene_color_tx;
  [[sampler(2)]] sampler2D scene_depth_tx;
};

struct PrepareOut {
  [[frag_color(0)]] float4 half_res_color;
  [[frag_color(1)]] float2 normalized_coc;
};

[[fragment]] void prepare([[resource_table]] DepthOfField &dof,
                          [[resource_table]] Prepare &srt,
                          [[resource_table]] draw::View &views,
                          [[out]] PrepareOut &out,
                          [[frag_coord]] const float4 frag_co)
{
  int4 texel = int4(frag_co.xyxy) * 2 + int4(0, 0, 1, 1);

  float4 color1 = texelFetch(srt.scene_color_tx, texel.xy, 0);
  float4 color2 = texelFetch(srt.scene_color_tx, texel.zw, 0);
  float4 color3 = texelFetch(srt.scene_color_tx, texel.zy, 0);
  float4 color4 = texelFetch(srt.scene_color_tx, texel.xw, 0);

  float4 depths;
  depths.x = texelFetch(srt.scene_depth_tx, texel.xy, 0).x;
  depths.y = texelFetch(srt.scene_depth_tx, texel.zw, 0).x;
  depths.z = texelFetch(srt.scene_depth_tx, texel.zy, 0).x;
  depths.w = texelFetch(srt.scene_depth_tx, texel.xw, 0).x;

  ViewMatrices view = views.get(0);

  float4 zdepths = float4(view.depth_screen_to_view(depths.x),
                          view.depth_screen_to_view(depths.y),
                          view.depth_screen_to_view(depths.z),
                          view.depth_screen_to_view(depths.w));
  float4 cocs_near = dof.calculate_coc(zdepths);
  float4 cocs_far = -cocs_near;

  float coc_near = max(reduce_max(cocs_near), 0.0f);
  float coc_far = max(reduce_max(cocs_far), 0.0f);

  /* now we need to write the near-far fields premultiplied by the coc
   * also use bilateral weighting by each coc values to avoid bleeding. */
  float4 near_weights = step(0.0f, cocs_near) *
                        clamp(1.0f - abs(coc_near - cocs_near), 0.0f, 1.0f);
  float4 far_weights = step(0.0f, cocs_far) * clamp(1.0f - abs(coc_far - cocs_far), 0.0f, 1.0f);

  /* now write output to weighted buffers. */
  /* Take far plane pixels in priority. */
  float4 w = any(notEqual(far_weights, float4(0.0f))) ? far_weights : near_weights;
  out.half_res_color = sum_weighted(color1, color2, color3, color4, w);
  out.half_res_color = clamp(out.half_res_color, 0.0f, 3.0f);

  out.normalized_coc = dof_encode_coc(coc_near, coc_far);
}

/**
 * ----------------- STEP  1 ------------------
 * Custom COC aware down-sampling. Quarter res pass.
 */

struct DownSample {
  [[sampler(0)]] sampler2D input_coc_tx;
  [[sampler(1)]] sampler2D scene_color_tx;
};

struct DownSampleOut {
  [[frag_color(0)]] float4 color;
  [[frag_color(1)]] float2 cocs;
};

[[fragment]] void downsample([[resource_table]] DownSample &srt,
                             [[out]] DownSampleOut &out,
                             [[frag_coord]] const float4 frag_co)
{
  float4 texel = float4(frag_co.xyxy) * 2.0f + float4(0.0f, 0.0f, 1.0f, 1.0f);
  texel = (texel - 0.5f) / float4(textureSize(srt.scene_color_tx, 0).xyxy);

  /* Using texelFetch can bypass the mip range setting on some platform.
   * Using texture LOD fixes this issue. Note that we need to disable filtering to get the right
   * texel values. */
  float4 color1 = textureLod(srt.scene_color_tx, texel.xy, 0.0f);
  float4 color2 = textureLod(srt.scene_color_tx, texel.zw, 0.0f);
  float4 color3 = textureLod(srt.scene_color_tx, texel.zy, 0.0f);
  float4 color4 = textureLod(srt.scene_color_tx, texel.xw, 0.0f);

  float2 cocs1 = textureLod(srt.input_coc_tx, texel.xy, 0.0f).rg;
  float2 cocs2 = textureLod(srt.input_coc_tx, texel.zw, 0.0f).rg;
  float2 cocs3 = textureLod(srt.input_coc_tx, texel.zy, 0.0f).rg;
  float2 cocs4 = textureLod(srt.input_coc_tx, texel.xw, 0.0f).rg;

  float4 cocs_near = float4(cocs1.r, cocs2.r, cocs3.r, cocs4.r) * MAX_COC_SIZE;
  float4 cocs_far = float4(cocs1.g, cocs2.g, cocs3.g, cocs4.g) * MAX_COC_SIZE;

  float coc_near = reduce_max(cocs_near);
  float coc_far = reduce_max(cocs_far);

  /* Now we need to write the near-far fields pre-multiplied by the COC
   * also use bilateral weighting by each COC values to avoid bleeding. */
  float4 near_weights = step(0.0f, cocs_near) *
                        clamp(1.0f - abs(coc_near - cocs_near), 0.0f, 1.0f);
  float4 far_weights = step(0.0f, cocs_far) * clamp(1.0f - abs(coc_far - cocs_far), 0.0f, 1.0f);

  /* now write output to weighted buffers. */
  float4 w = any(notEqual(far_weights, float4(0.0f))) ? far_weights : near_weights;
  out.color = sum_weighted(color1, color2, color3, color4, w);

  out.cocs = dof_encode_coc(coc_near, coc_far);
}

/**
 * ----------------- STEP 2 ------------------
 * Blur vertically and diagonally.
 * Outputs vertical blur and combined blur in MRT
 */

static constexpr int dof_sample_count = 49;

struct Blur1 {
  [[sampler(0)]] sampler2D input_coc_tx;
  [[sampler(3)]] sampler2D half_res_color_tx;
  [[sampler(5)]] sampler2D noise_tx;
  [[uniform(1)]] float4 samples[dof_sample_count];

  float2 get_random_vector(float2 frag_co, float offset) const
  {
    /* Interleaved gradient noise by Jorge Jimenez
     * http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare */
    float ign = fract(offset +
                      52.9829189f * fract(0.06711056f * frag_co.x + 0.00583715f * frag_co.y));
    float bn = texelFetch(noise_tx, int2(frag_co.xy) % 64, 0).a;
    float ang = M_PI * 2.0f * fract(bn + offset);
    return float2(cos(ang), sin(ang)) * sqrt(ign);
  }
};

struct Blur1Out {
  [[frag_color(0)]] float4 color;
};

[[fragment]] void blur1([[resource_table]] DepthOfField &dof,
                        [[resource_table]] Blur1 &srt,
                        [[out]] Blur1Out &out,
                        [[frag_coord]] const float4 frag_co)
{
  float2 uv = frag_co.xy * dof.inverted_viewport_size * 2.0f;

  float2 size = float2(textureSize(srt.half_res_color_tx, 0).xy);
  int2 texel = int2(uv * size);

  float4 color = float4(0.0f);
  float tot = 0.0f;

  float coc = dof_decode_coc(texelFetch(srt.input_coc_tx, texel, 0).rg);
  float max_radius = coc;
  float2 noise = srt.get_random_vector(frag_co.xy, dof.noise_offset) * 0.2f *
                 clamp(max_radius * 0.2f - 4.0f, 0.0f, 1.0f);
  for (int i = 0; i < dof_sample_count; i++) {
    float2 tc = uv + (noise + srt.samples[i].xy) * dof.inverted_viewport_size * max_radius;

    /* decode_signed_coc return biggest coc. */
    coc = abs(dof_decode_signed_coc(texture(srt.input_coc_tx, tc).rg));

    float lod = log2(clamp((coc + min(coc, max_radius)) * 0.5f - 21.0f, 0.0f, 16.0f) * 0.25f);
    float4 samp = textureLod(srt.half_res_color_tx, tc, lod);

    float radius = srt.samples[i].z * max_radius;
    float weight = abs(coc) * smoothstep(radius - 0.5f, radius + 0.5f, abs(coc));

    color += samp * weight;
    tot += weight;
  }

  if (tot > 0.0f) {
    out.color = color / tot;
  }
  else {
    out.color = textureLod(srt.half_res_color_tx, uv, 0.0f);
  }
}

/**
 * ----------------- STEP 3 ------------------
 * 3x3 Median Filter
 * Morgan McGuire and Kyle Whitson
 * http://graphics.cs.williams.edu
 *
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright 2006 Morgan McGuire and Williams College, All rights reserved.
 */

struct Blur2 {
  [[sampler(0)]] sampler2D input_coc_tx;
  [[sampler(4)]] sampler2D blur_tx;
};

struct Blur2Out {
  [[frag_color(0)]] float4 color;
};

[[force_inline]] void s2(float4 &a, float4 &b)
{
  float4 temp = a;
  a = min(a, b);
  b = max(temp, b);
}

[[force_inline]] void mn3(float4 &a, float4 &b, float4 &c)
{
  s2(a, b);
  s2(a, c);
}

[[force_inline]] void mx3(float4 &a, float4 &b, float4 &c)
{
  s2(b, c);
  s2(a, c);
}

[[force_inline]] void mnmx3(float4 &a, float4 &b, float4 &c)
{
  mx3(a, b, c);
  s2(a, b); /* 3 exchanges */
}

[[force_inline]] void mnmx4(float4 &a, float4 &b, float4 &c, float4 &d)
{
  s2(a, b);
  s2(c, d);
  s2(a, c);
  s2(b, d); /* 4 exchanges */
}

[[force_inline]] void mnmx5(float4 &a, float4 &b, float4 &c, float4 &d, float4 &e)
{
  s2(a, b);
  s2(c, d);
  mn3(a, c, e);
  mx3(b, d, e); /* 6 exchanges */
}

[[force_inline]] void mnmx6(float4 &a, float4 &b, float4 &c, float4 &d, float4 &e, float4 &f)
{
  s2(a, d);
  s2(b, e);
  s2(c, f);
  mn3(a, b, c);
  mx3(d, e, f); /* 7 exchanges */
}

[[fragment]] void blur2([[resource_table]] Blur2 &srt,
                        [[out]] Blur2Out &out,
                        [[frag_coord]] const float4 frag_co)
{
  /* Half Res pass */
  float2 pixel_size = 1.0f / float2(textureSize(srt.blur_tx, 0).xy);
  float2 uv = frag_co.xy * pixel_size.xy;
  float coc = dof_decode_coc(texture(srt.input_coc_tx, uv).rg);
  /* Only use this filter if coc is > 9.0f
   * since this filter is not weighted by CoC
   * and can bleed a bit. */
  float rad = clamp(coc - 9.0f, 0.0f, 1.0f);

  float4 v[9];

  /* Add the pixels which make up our window to the pixel array. */
  for (int dX = -1; dX <= 1; dX++) [[unroll]] {
    for (int dY = -1; dY <= 1; dY++) [[unroll]] {
      float2 offset = float2(float(dX), float(dY));
      /* If a pixel in the window is located at (x+dX, y+dY), put it at index (dX + R)(2R + 1) +
       * (dY + R) of the pixel array. This will fill the pixel array, with the top left pixel of
       * the window at pixel[0] and the bottom right pixel of the window at pixel[N-1]. */
      v[(dX + 1) * 3 + (dY + 1)] = texture(srt.blur_tx, uv + offset * pixel_size * rad);
    }
  }

  /* Starting with a subset of size 6, remove the min and max each time */
  mnmx6(v[0], v[1], v[2], v[3], v[4], v[5]);
  mnmx5(v[1], v[2], v[3], v[4], v[6]);
  mnmx4(v[2], v[3], v[4], v[7]);
  mnmx3(v[3], v[4], v[8]);
  out.color = v[4];
}

/**
 * ----------------- STEP 4 ------------------
 */

struct Resolve {
  [[sampler(2)]] sampler2D scene_depth_tx;
  [[sampler(3)]] sampler2D half_res_color_tx;
};

struct ResolveOut {
  [[frag_color(0), index(0)]] float4 color_add;
  [[frag_color(0), index(1)]] float4 color_mul;
};

[[fragment]] void resolve([[resource_table]] DepthOfField &dof,
                          [[resource_table]] Resolve &srt,
                          [[resource_table]] draw::View &views,
                          [[out]] ResolveOut &out,
                          [[frag_coord]] const float4 frag_co)
{
  /* Full-screen pass. */
  float2 pixel_size = 0.5f / float2(textureSize(srt.half_res_color_tx, 0).xy);
  float2 uv = frag_co.xy * pixel_size;

  ViewMatrices view = views.get(0);

  /* TODO: MAKE SURE TO ALIGN SAMPLE POSITION TO AVOID OFFSET IN THE BOKEH. */
  float depth = texelFetch(srt.scene_depth_tx, int2(frag_co.xy), 0).r;
  float zdepth = view.depth_screen_to_view(depth);
  float coc = dof.calculate_coc(zdepth);

  float blend = smoothstep(1.0f, 3.0f, abs(coc));
  out.color_add = texture(srt.half_res_color_tx, uv) * blend;
  out.color_mul = float4(1.0f - blend);
}

}  // namespace workbench::dof

PipelineGraphic workbench_effect_dof_prepare(workbench::dof::vert, workbench::dof::prepare);
PipelineGraphic workbench_effect_dof_downsample(workbench::dof::vert, workbench::dof::downsample);
PipelineGraphic workbench_effect_dof_blur1(workbench::dof::vert, workbench::dof::blur1);
PipelineGraphic workbench_effect_dof_blur2(workbench::dof::vert, workbench::dof::blur2);
PipelineGraphic workbench_effect_dof_resolve(workbench::dof::vert, workbench::dof::resolve);
