/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Scatter pass: Use sprites to scatter the color of very bright pixel to have higher quality blur.
 *
 * We only scatter one quad per sprite and one sprite per 4 pixels to reduce vertex shader
 * invocations and overdraw.
 */
#pragma once

#include "eevee_depth_of_field_lib.bsl.hh"
#include "gpu_shader_math_vector_reduce_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

#define linearstep(p0, p1, v) (clamp(((v) - (p0)) / abs((p1) - (p0)), 0.0f, 1.0f))

namespace eevee::dof::scatter {

struct VertOut {
  /** Colors, weights, and Circle of confusion radii for the 4 pixels to scatter. */
  [[flat]] float4 color_and_coc1;
  [[flat]] float4 color_and_coc2;
  [[flat]] float4 color_and_coc3;
  [[flat]] float4 color_and_coc4;
  /** Scaling factor for the bokeh distance. */
  [[flat]] float distance_scale;
  /** Sprite pixel position with origin at sprite center. In pixels. */
  [[no_perspective]] float2 rect_uv1;
  [[no_perspective]] float2 rect_uv2;
  [[no_perspective]] float2 rect_uv3;
  [[no_perspective]] float2 rect_uv4;
};

struct Resources {
  [[storage(0, read)]] const ScatterRect (&scatter_list_buf)[];

  [[push_constant]] const bool use_bokeh_lut;

  [[uniform(0)]] const DepthOfFieldData &dof_buf;

  [[sampler(0)]] sampler2D occlusion_tx;
  [[sampler(1)]] sampler2D bokeh_lut_tx;
};

[[vertex]]
void vert_main([[resource_table]] Resources &srt,
               [[instance_id]] const int inst_id,
               [[vertex_id]] const int vert_id,
               [[out]] VertOut &v_out,
               [[position]] float4 &out_position)
{
  if (uint(inst_id) >= srt.dof_buf.scatter_max_rect) {
    /* Very unlikely to happen but better avoid out of bound access. */
    out_position = float4(0.0f);
    return;
  }

  ScatterRect rect = srt.scatter_list_buf[inst_id];

  v_out.color_and_coc1 = rect.color_and_coc[0];
  v_out.color_and_coc2 = rect.color_and_coc[1];
  v_out.color_and_coc3 = rect.color_and_coc[2];
  v_out.color_and_coc4 = rect.color_and_coc[3];

  float2 uv = float2(vert_id & 1, vert_id >> 1) * 2.0f - 1.0f;
  uv = uv * rect.half_extent;

  out_position = float4(uv + rect.offset, 0.0f, 1.0f);
  /* NDC range [-1..1]. */
  out_position.xy = (out_position.xy / float2(textureSize(srt.occlusion_tx, 0).xy)) * 2.0f - 1.0f;

  if (srt.use_bokeh_lut) {
    /* Bias scale to avoid sampling at the texture's border. */
    v_out.distance_scale = (float(DOF_BOKEH_LUT_SIZE) / float(DOF_BOKEH_LUT_SIZE - 1));
    float2 uv_div = 1.0f / (v_out.distance_scale * abs(rect.half_extent));
    v_out.rect_uv1 = ((uv + quad_offsets[0]) * uv_div) * 0.5f + 0.5f;
    v_out.rect_uv2 = ((uv + quad_offsets[1]) * uv_div) * 0.5f + 0.5f;
    v_out.rect_uv3 = ((uv + quad_offsets[2]) * uv_div) * 0.5f + 0.5f;
    v_out.rect_uv4 = ((uv + quad_offsets[3]) * uv_div) * 0.5f + 0.5f;
    /* Only for sampling. */
    v_out.distance_scale *= reduce_max(abs(rect.half_extent));
  }
  else {
    v_out.distance_scale = 1.0f;
    v_out.rect_uv1 = uv + quad_offsets[0];
    v_out.rect_uv2 = uv + quad_offsets[1];
    v_out.rect_uv3 = uv + quad_offsets[2];
    v_out.rect_uv4 = uv + quad_offsets[3];
  }
}

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[fragment]]
void frag_main([[resource_table]] Resources &srt,
               [[frag_coord]] const float4 frag_co,
               [[in]] const VertOut &v_out,
               [[out]] FragOut &frag_out)
{
  float4 coc4 = float4(v_out.color_and_coc1.w,
                       v_out.color_and_coc2.w,
                       v_out.color_and_coc3.w,
                       v_out.color_and_coc4.w);
  float4 shapes;
  if (srt.use_bokeh_lut) {
    shapes = float4(texture(srt.bokeh_lut_tx, v_out.rect_uv1).r,
                    texture(srt.bokeh_lut_tx, v_out.rect_uv2).r,
                    texture(srt.bokeh_lut_tx, v_out.rect_uv3).r,
                    texture(srt.bokeh_lut_tx, v_out.rect_uv4).r);
  }
  else {
    shapes = float4(length(v_out.rect_uv1),
                    length(v_out.rect_uv2),
                    length(v_out.rect_uv3),
                    length(v_out.rect_uv4));
  }
  shapes *= v_out.distance_scale;
  /* Becomes signed distance field in pixel units. */
  shapes -= coc4;
  /* Smooth the edges a bit to fade out the undersampling artifacts. */
  shapes = saturate(1.0f - linearstep(-0.8f, 0.8f, shapes));
  /* Outside of bokeh shape. Try to avoid overloading ROPs. */
  if (reduce_max(shapes) == 0.0f) {
    gpu_discard_fragment();
    return;
  }

  if (!no_scatter_occlusion) {
    /* Works because target is the same size as occlusion_tx. */
    float2 uv = frag_co.xy / float2(textureSize(srt.occlusion_tx, 0).xy);
    float2 occlusion_data = texture(srt.occlusion_tx, uv).rg;
    /* Fix tilling artifacts. (Slide 90) */
    constexpr float correction_fac = 1.0f - DOF_FAST_GATHER_COC_ERROR;
    /* Occlude the sprite with geometry from the same field using a chebychev test (slide 85). */
    float mean = occlusion_data.x;
    float variance = occlusion_data.y;
    shapes *= variance * safe_rcp(variance + square(max(coc4 * correction_fac - mean, 0.0f)));
  }

  frag_out.color = (v_out.color_and_coc1 * shapes[0] + v_out.color_and_coc2 * shapes[1] +
                    v_out.color_and_coc3 * shapes[2] + v_out.color_and_coc4 * shapes[3]);
  /* Do not accumulate alpha. This has already been accumulated by the gather pass. */
  frag_out.color.a = 0.0f;

  if (debug_scatter_perf) {
    frag_out.color.rgb = average(frag_out.color.rgb) * float3(1.0f, 0.0f, 0.0f);
  }
}

}  // namespace eevee::dof::scatter

PipelineGraphic eevee_depth_of_field_scatter(eevee::dof::scatter::vert_main,
                                             eevee::dof::scatter::frag_main);
