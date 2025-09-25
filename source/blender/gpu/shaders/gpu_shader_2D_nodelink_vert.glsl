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

#include "infos/gpu_shader_2D_nodelink_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_nodelink)

#include "gpu_shader_attribute_load_lib.glsl"
#include "gpu_shader_math_vector_compare_lib.glsl"

void main()
{
  constexpr float start_gradient_threshold = 0.35f;
  constexpr float end_gradient_threshold = 0.65f;

  const NodeLinkData link = link_data_buf[gl_InstanceID];

  const float2 P0 = link.bezier_P0;
  const float2 P1 = link.bezier_P1;
  const float2 P2 = link.bezier_P2;
  const float2 P3 = link.bezier_P3;

  const uint3 color_ids = gpu_attr_decode_uchar4_to_uint4(link.color_ids).xyz;

  const float4 color_start = (color_ids[0] < 3u) ? link.start_color :
                                                   link_uniforms.colors[color_ids[0]];
  const float4 color_end = (color_ids[1] < 3u) ? link.end_color :
                                                 link_uniforms.colors[color_ids[1]];
  const float4 color_shadow = link_uniforms.colors[color_ids[2]];
  float line_thickness = link.thickness;

  /* Each instance contains both the outline and the "main" line on top. */
  constexpr int mid_vertex = 65;
  bool is_outline_pass = gl_VertexID < mid_vertex;

  interp_flat.line_thickness = line_thickness;
  interp_flat.is_main_line = (expand.y == 1.0f && !is_outline_pass) ? 1 : 0;
  interp_flat.has_back_link = int(link.has_back_link);
  interp_flat.aspect = link_uniforms.aspect;
  /* Parameters for the dashed line. */
  interp_flat.dash_length = link.dash_length;
  interp_flat.dash_factor = link.dash_factor;
  interp_flat.dash_alpha = link.dash_alpha;
  /* Approximate line length, no need for real bezier length calculation. */
  interp_flat.line_length = distance(P0, P3);
  /* TODO: Incorrect U, this leads to non-uniform dash distribution. */
  interp.line_uv = uv;

  if ((expand.y == 1.0f) && link.has_back_link) {
    /* Increase width because two links are drawn. */
    line_thickness *= 1.7f;
  }

  if (is_outline_pass) {
    /* Outline pass. */
    interp.final_color = color_shadow;
  }
  else {
    /* Second pass. */
    if (uv.x < start_gradient_threshold) {
      interp.final_color = color_start;
    }
    else if (uv.x > end_gradient_threshold) {
      interp.final_color = color_end;
    }
    else {
      float mixFactor = (uv.x - start_gradient_threshold) /
                        (end_gradient_threshold - start_gradient_threshold);
      interp.final_color = mix(color_start, color_end, mixFactor);
    }
    line_thickness *= 0.65f;
    if (link.do_muted) {
      interp.final_color[3] = 0.65f;
    }
  }
  interp.final_color.a *= link.dim_factor;

  float t = uv.x;
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
  point += (pos.x * tangent + pos.y * normal) * link_uniforms.arrow_size;

  gl_Position = ModelViewProjectionMatrix * float4(point, 0.0f, 1.0f);

  float2 exp_axis = expand.x * tangent + expand.y * normal;

  /* rotate & scale the expand axis */
  exp_axis = ModelViewProjectionMatrix[0].xy * exp_axis.xx +
             ModelViewProjectionMatrix[1].xy * exp_axis.yy;

  float expand_dist = line_thickness * (uv.y * 2.0f - 1.0f);

  /* Expand into a line */
  gl_Position.xy += exp_axis * link_uniforms.aspect * expand_dist;

  /* If the link is not muted or is not a reroute arrow the points are squashed to the center of
   * the line. Magic numbers are defined in `drawnode.cc`. */
  if ((expand.x == 1.0f && !link.do_muted) ||
      (expand.y != 1.0f && (pos.x < 0.70f || pos.x > 0.71f) && !link.do_arrow))
  {
    gl_Position.xy *= 0.0f;
  }
}
