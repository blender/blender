/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * 2D Cubic Bezier thick line drawing
 */

/**
 * `uv.x` is position along the curve, defining the tangent space.
 * `uv.y` is "signed" distance (compressed to [0..1] range) from the pos in expand direction
 * `pos` is the verts position in the curve tangent space
 */

#pragma once
#pragma create_info

#include "gpu_shader_compat.hh"

#include "GPU_shader_shared.hh"

#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_math_vector_compare_lib.glsl"

namespace builtin::nodelink {

struct NodeLinkVertIn {
  [[attribute(0)]] float2 uv;
  [[attribute(1)]] float2 pos;
  [[attribute(2)]] float2 expand;
};

struct NodeLinkVertOut {
  [[smooth]] float4 final_color;
  [[smooth]] float2 line_uv;
  [[flat]] float line_length;
  [[flat]] float line_thickness;
  [[flat]] float dash_length;
  [[flat]] float dash_factor;
  [[flat]] float dash_alpha;
  [[flat]] float aspect;
  [[flat]] int has_back_link;
  [[flat]] int is_main_line;
};

struct NodeLinkFragOut {
  [[frag_color(0)]] float4 color;
};

struct NodeLinkSRT {
  [[push_constant]] float4x4 ModelViewProjectionMatrix;
  [[storage(0, read)]] NodeLinkData (&link_data_buf)[];
  [[uniform(0)]] NodeLinkUniformData &link_uniforms;
};

[[vertex]] void vert([[vertex_id]] const int gl_VertexID,
                     [[instance_id]] const int gl_InstanceID,
                     [[resource_table]] const NodeLinkSRT &srt,
                     [[in]] const NodeLinkVertIn &v_in,
                     [[position]] float4 &gl_Position,
                     [[out]] NodeLinkVertOut &interp)
{
  constexpr float start_gradient_threshold = 0.35f;
  constexpr float end_gradient_threshold = 0.65f;

  const NodeLinkData link = srt.link_data_buf[gl_InstanceID];

  const float2 P0 = link.bezier_P0;
  const float2 P1 = link.bezier_P1;
  const float2 P2 = link.bezier_P2;
  const float2 P3 = link.bezier_P3;

  const uint3 color_ids = gpu_attr_decode_uchar4_to_uint4(link.color_ids).xyz;

  const float4 color_start = (color_ids[0] < 3u) ? link.start_color :
                                                   srt.link_uniforms.colors[color_ids[0]];
  const float4 color_end = (color_ids[1] < 3u) ? link.end_color :
                                                 srt.link_uniforms.colors[color_ids[1]];
  const float4 color_shadow = srt.link_uniforms.colors[color_ids[2]];
  float line_thickness = link.thickness;

  /* Each instance contains both the outline and the "main" line on top. */
  constexpr int mid_vertex = 65;
  bool is_outline_pass = gl_VertexID < mid_vertex;

  interp.line_thickness = line_thickness;
  interp.is_main_line = (v_in.expand.y == 1.0f && !is_outline_pass) ? 1 : 0;
  interp.has_back_link = int(link.has_back_link);
  interp.aspect = srt.link_uniforms.aspect;
  /* Parameters for the dashed line. */
  interp.dash_length = link.dash_length;
  interp.dash_factor = link.dash_factor;
  interp.dash_alpha = link.dash_alpha;
  /* Approximate line length, no need for real bezier length calculation. */
  interp.line_length = distance(P0, P3);
  /* TODO: Incorrect U, this leads to non-uniform dash distribution. */
  interp.line_uv = v_in.uv;

  if ((v_in.expand.y == 1.0f) && link.has_back_link) {
    /* Increase width because two links are drawn. */
    line_thickness *= 1.7f;
  }

  if (is_outline_pass) {
    /* Outline pass. */
    interp.final_color = color_shadow;
  }
  else {
    /* Second pass. */
    if (v_in.uv.x < start_gradient_threshold) {
      interp.final_color = color_start;
    }
    else if (v_in.uv.x > end_gradient_threshold) {
      interp.final_color = color_end;
    }
    else {
      float mixFactor = (v_in.uv.x - start_gradient_threshold) /
                        (end_gradient_threshold - start_gradient_threshold);
      interp.final_color = mix(color_start, color_end, mixFactor);
    }
    line_thickness *= 0.65f;
    if (link.do_muted) {
      interp.final_color[3] = 0.65f;
    }
  }
  interp.final_color.a *= link.dim_factor;

  float t = v_in.uv.x;
  float t2 = t * t;
  float t2_3 = 3.0f * t2;
  float one_minus_t = 1.0f - t;
  float one_minus_t2 = one_minus_t * one_minus_t;
  float one_minus_t2_3 = 3.0f * one_minus_t2;

  float2 point = (P0 * one_minus_t2 * one_minus_t + P1 * one_minus_t2_3 * t +
                  P2 * t2_3 * one_minus_t + P3 * t2 * t);

  float2 tangent = ((P1 - P0) * one_minus_t2_3 + (P2 - P1) * 6.0f * (t - t2) + (P3 - P2) * t2_3);

  /* Tangent space at t. If the inner and outer control points overlap, the tangent is invalid.
   * Use the vector between the sockets instead. */
  tangent = is_zero(tangent) ? normalize(P3 - P0) : normalize(tangent);
  float2 normal = tangent.yx * float2(-1.0f, 1.0f);

  /* Position vertex on the curve tangent space */
  point += (v_in.pos.x * tangent + v_in.pos.y * normal) * srt.link_uniforms.arrow_size;

  gl_Position = srt.ModelViewProjectionMatrix * float4(point, 0.0f, 1.0f);

  float2 exp_axis = v_in.expand.x * tangent + v_in.expand.y * normal;

  /* rotate & scale the expand axis */
  exp_axis = srt.ModelViewProjectionMatrix[0].xy * exp_axis.xx +
             srt.ModelViewProjectionMatrix[1].xy * exp_axis.yy;

  float expand_dist = line_thickness * (v_in.uv.y * 2.0f - 1.0f);

  /* Expand into a line */
  gl_Position.xy += exp_axis * srt.link_uniforms.aspect * expand_dist;

  /* If the link is not muted or is not a reroute arrow the points are squashed to the center of
   * the line. Magic numbers are defined in `drawnode.cc`. */
  if ((v_in.expand.x == 1.0f && !link.do_muted) ||
      (v_in.expand.y != 1.0f && (v_in.pos.x < 0.70f || v_in.pos.x > 0.71f) && !link.do_arrow))
  {
    gl_Position.xy *= 0.0f;
  }
}

#define ANTIALIAS 0.75f

float get_line_alpha(float2 line_uv, float line_thickness, float center, float relative_radius)
{
  float radius = relative_radius * line_thickness;
  float sdf = abs(line_thickness * (line_uv.y - center));
  return smoothstep(radius, radius - ANTIALIAS, sdf);
}

[[fragment]] void frag([[in]] const NodeLinkVertOut &interp, [[out]] NodeLinkFragOut &frag_out)
{
  float dash_frag_alpha = 1.0f;
  if (interp.dash_factor < 1.0f) {
    float distance_along_line = interp.line_length * interp.line_uv.x;

    /* Checking if `normalized_distance <= interp.dash_factor` is already enough for a basic
     * dash, however we want to handle a nice anti-alias. */

    float dash_center = interp.dash_length * interp.dash_factor * 0.5f;
    float normalized_distance_triangle =
        1.0f -
        abs((fract((distance_along_line - dash_center) / interp.dash_length)) * 2.0f - 1.0f);
    float t = interp.aspect * ANTIALIAS / interp.dash_length;
    float slope = 1.0f / (2.0f * t);

    float unclamped_alpha = 1.0f - slope * (normalized_distance_triangle - interp.dash_factor + t);
    float alpha = max(interp.dash_alpha, min(unclamped_alpha, 1.0f));

    dash_frag_alpha = alpha;
  }

  if (interp.is_main_line == 0) {
    frag_out.color = interp.final_color;
    frag_out.color.a *= get_line_alpha(interp.line_uv, interp.line_thickness, 0.5f, 0.5f) *
                        dash_frag_alpha;
    return;
  }

  if (interp.has_back_link == 0) {
    frag_out.color = interp.final_color;
    frag_out.color.a *= get_line_alpha(interp.line_uv, interp.line_thickness, 0.5f, 0.5f) *
                        dash_frag_alpha;
  }
  else {
    /* Draw two links right next to each other, the main link and the back-link. */
    float4 main_link_color = interp.final_color;
    main_link_color.a *= get_line_alpha(interp.line_uv, interp.line_thickness, 0.75f, 0.3f);

    float4 back_link_color = float4(float3(0.8f), 1.0f);
    back_link_color.a *= get_line_alpha(interp.line_uv, interp.line_thickness, 0.2f, 0.25f);

    /* Combine both links. */
    frag_out.color.rgb = main_link_color.rgb * main_link_color.a +
                         back_link_color.rgb * back_link_color.a;
    frag_out.color.a = main_link_color.a * dash_frag_alpha + back_link_color.a;
  }
}

}  // namespace builtin::nodelink

PipelineGraphic gpu_shader_2D_nodelink(builtin::nodelink::vert, builtin::nodelink::frag);
