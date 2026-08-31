/* SPDX-FileCopyrightText: 2018-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_matrix_construct_lib.glsl"

namespace builtin::socket {

/* TODO(lone_noel): Share with C code. */
#define MAX_SOCKET_PARAMETERS 4
#define MAX_SOCKET_INSTANCE 32

struct Parameters {
  [[push_constant]] float4 parameters[MAX_SOCKET_PARAMETERS];
};

struct ParametersInstancing {
  [[push_constant]] float4 parameters[MAX_SOCKET_PARAMETERS * MAX_SOCKET_INSTANCE];
};

struct Resources {
  [[compilation_constant]] int use_instancing;

  [[push_constant]] float4x4 ModelViewProjectionMatrix;
};

struct VertOut {
  [[flat]] float4 final_color;
  [[flat]] float4 final_outline_color;
  [[flat]] float final_dot_radius;
  [[flat]] float final_outline_thickness;
  [[flat]] float aa_size;
  [[flat]] float2 extrusion;
  [[flat]] int final_shape;
  [[smooth]] float2 uv;
};

struct FragOut {
  [[frag_color(0)]] float4 color;
};

[[vertex]] void main_vert(
    [[resource_table]] const Resources &srt,
    [[resource_table, condition(use_instancing == 0)]] const Parameters &params,
    [[resource_table, condition(use_instancing == 1)]] const ParametersInstancing &params_inst,
    [[vertex_id]] const int vert_id,
    [[instance_id]] const int inst_id,
    [[position]] float4 &out_pos,
    [[out]] VertOut &v_out)
{
  float4 rect, color_inner, color_outline;
  float outline_thickness, outline_offset, shape, aspect;

  if (srt.use_instancing) [[static_branch]] {
    /* Unpack instancing. */
    int offset = inst_id * MAX_SOCKET_PARAMETERS;
    rect = params_inst.parameters[offset + 0];
    color_inner = params_inst.parameters[offset + 1];
    color_outline = params_inst.parameters[offset + 2];
    outline_thickness = params_inst.parameters[offset + 3].x;
    outline_offset = params_inst.parameters[offset + 3].y;
    shape = params_inst.parameters[offset + 3].z;
    aspect = params_inst.parameters[offset + 3].w;
  }
  else {
    /* Unpack no-instancing. */
    rect = params.parameters[0];
    color_inner = params.parameters[1];
    color_outline = params.parameters[2];
    outline_thickness = params.parameters[3].x;
    outline_offset = params.parameters[3].y;
    shape = params.parameters[3].z;
    aspect = params.parameters[3].w;
  }

  /* Scale the original rectangle to accommodate the diagonal of the diamond shape. */
  float2 originalRectSize = rect.yw - rect.xz;
  float offset = 0.125f * min(originalRectSize.x, originalRectSize.y) +
                 outline_offset * outline_thickness;
  float2 ofs = float2(offset, -offset);
  float2 pos;
  switch (vert_id) {
    default:
    case 0: {
      pos = rect.xz + ofs.yy;
      break;
    }
    case 1: {
      pos = rect.xw + ofs.yx;
      break;
    }
    case 2: {
      pos = rect.yz + ofs.xy;
      break;
    }
    case 3: {
      pos = rect.yw + ofs.xx;
      break;
    }
  }

  out_pos = srt.ModelViewProjectionMatrix * float4(pos, 0.0f, 1.0f);

  float2 rectSize = rect.yw - rect.xz + 2.0f * float2(outline_offset, outline_offset);
  float minSize = min(rectSize.x, rectSize.y);

  float2 centeredCoordinates = pos - ((rect.xz + rect.yw) / 2.0f);
  v_out.uv = centeredCoordinates / minSize;

  /* Calculate the necessary "extrusion" of the coordinates to draw the middle part of
   * multi sockets. */
  float ratio = rectSize.x / rectSize.y;
  v_out.extrusion = (ratio > 1.0f) ? float2((ratio - 1.0f) / 2.0f, 0.0f) :
                                     float2(0.0f, ((1.0f / ratio) - 1.0f) / 2.0f);

  /* Shape parameters. */
  v_out.final_shape = int(shape);
  v_out.final_outline_thickness = outline_thickness / minSize;
  v_out.final_dot_radius = outline_thickness / minSize;
  v_out.aa_size = 1.0f * aspect / minSize;

  /* Pass through parameters. */
  v_out.final_color = color_inner;
  v_out.final_outline_color = color_outline;
}

/* Values in `eNodeSocketDisplayShape` in DNA_node_types.h. Keep in sync. */
enum SocketDisplayShape : int {
  SOCK_DISPLAY_SHAPE_CIRCLE = 0,
  SOCK_DISPLAY_SHAPE_SQUARE = 1,
  SOCK_DISPLAY_SHAPE_DIAMOND = 2,
  SOCK_DISPLAY_SHAPE_CIRCLE_DOT = 3,
  SOCK_DISPLAY_SHAPE_SQUARE_DOT = 4,
  SOCK_DISPLAY_SHAPE_DIAMOND_DOT = 5,
  SOCK_DISPLAY_SHAPE_LINE = 6,
  SOCK_DISPLAY_SHAPE_VOLUME_GRID = 7,
  SOCK_DISPLAY_SHAPE_LIST = 8,
};

/* Calculates a squared distance field of a square. */
float square_sdf(float2 absCo, float2 half_size)
{
  float2 extruded_co = absCo - half_size;
  float2 clamped_extruded_co = float2(max(0.0f, extruded_co.x), max(0.0f, extruded_co.y));

  float exterior_distance_squared = dot(clamped_extruded_co, clamped_extruded_co);

  float interior_distance = min(max(extruded_co.x, extruded_co.y), 0.0f);
  float interior_distance_squared = interior_distance * interior_distance;

  return exterior_distance_squared - interior_distance_squared;
}

float2 rotate_45(float2 co)
{
  return from_rotation(AngleRadian{M_PI * 0.25f}) * co;
}

/* Calculates an upper and lower limit for an anti-aliased cutoff of the squared distance. */
float2 calculate_thresholds(float aa_size, float threshold)
{
  /* Use the absolute on one of the factors to preserve the sign. */
  float inner_threshold = (threshold - 0.5f * aa_size) * abs(threshold - 0.5f * aa_size);
  float outer_threshold = (threshold + 0.5f * aa_size) * abs(threshold + 0.5f * aa_size);
  return float2(inner_threshold, outer_threshold);
}

[[fragment]] void main_frag([[resource_table]] const Resources &srt,
                            [[in]] const VertOut &v_out,
                            [[out]] FragOut &frag_out)
{
  float2 absUV = abs(v_out.uv);
  float2 co = max(float2(absUV - v_out.extrusion), float2(0.0f));

  float distance_squared = 0.0f;
  float alpha_threshold = 0.0f;
  float dot_threshold = -1.0f;

  constexpr float circle_radius = 0.5f;
  const float square_radius = 0.5f / sqrt(2.0f / M_PI) * M_SQRT1_2;
  const float diamond_radius = 0.5f / sqrt(2.0f / M_PI) * M_SQRT1_2;
  constexpr float corner_rounding = 0.0f;

  switch (v_out.final_shape) {
    default:
    case SOCK_DISPLAY_SHAPE_CIRCLE: {
      distance_squared = dot(co, co);
      alpha_threshold = circle_radius;
      break;
    }
    case SOCK_DISPLAY_SHAPE_CIRCLE_DOT: {
      distance_squared = dot(co, co);
      alpha_threshold = circle_radius;
      dot_threshold = v_out.final_dot_radius;
      break;
    }
    case SOCK_DISPLAY_SHAPE_SQUARE: {
      distance_squared = square_sdf(co, float2(square_radius - corner_rounding));
      alpha_threshold = corner_rounding;
      break;
    }
    case SOCK_DISPLAY_SHAPE_SQUARE_DOT: {
      distance_squared = square_sdf(co, float2(square_radius - corner_rounding));
      alpha_threshold = corner_rounding;
      dot_threshold = v_out.final_dot_radius;
      break;
    }
    case SOCK_DISPLAY_SHAPE_DIAMOND: {
      distance_squared = square_sdf(abs(rotate_45(co)), float2(diamond_radius - corner_rounding));
      alpha_threshold = corner_rounding;
      break;
    }
    case SOCK_DISPLAY_SHAPE_DIAMOND_DOT: {
      distance_squared = square_sdf(abs(rotate_45(co)), float2(diamond_radius - corner_rounding));
      alpha_threshold = corner_rounding;
      dot_threshold = v_out.final_dot_radius;
      break;
    }
    case SOCK_DISPLAY_SHAPE_LINE: {
      distance_squared = square_sdf(co, float2(square_radius * 0.75, square_radius * 1.4));
      alpha_threshold = corner_rounding;
      break;
    }
    case SOCK_DISPLAY_SHAPE_VOLUME_GRID: {
      constexpr float rect_side_length = 0.25f;
      const float2 oversize = float2(0.0f, square_radius * 1.4) / 2.5f;
      const float2 rect_corner = max(float2(rect_side_length), v_out.extrusion / 2.0f + oversize) +
                                 v_out.final_outline_thickness / 4.0f;
      const float2 mirrored_uv = abs(abs(v_out.uv) - rect_corner);
      distance_squared = square_sdf(mirrored_uv,
                                    rect_corner + v_out.final_outline_thickness / 2.0f);
      alpha_threshold = corner_rounding;
      break;
    }
    case SOCK_DISPLAY_SHAPE_LIST: {
      constexpr float2 rect_side_length = float2(0.5f, 0.25f);
      const float2 oversize = float2(0.0f, square_radius * 1.4) / 2.5f;
      const float2 rect_corner = max(rect_side_length, v_out.extrusion / 2.0f + oversize) +
                                 v_out.final_outline_thickness / 4.0f;
      const float2 mirrored_uv = float2(
          abs(v_out.uv.x),
          abs(abs(abs(v_out.uv.y) - rect_corner.y / 1.5f) - rect_corner.y / 1.5f));
      distance_squared = square_sdf(
          mirrored_uv, (rect_corner + v_out.final_outline_thickness / 2.0f) / float2(1.0f, 1.5f));
      break;
    }
  }

  float2 alpha_thresholds = calculate_thresholds(v_out.aa_size, alpha_threshold);
  float2 outline_thresholds = calculate_thresholds(
      v_out.aa_size, alpha_threshold - v_out.final_outline_thickness);
  float2 dot_thresholds = calculate_thresholds(v_out.aa_size, dot_threshold);

  float alpha_mask = smoothstep(alpha_thresholds[1], alpha_thresholds[0], distance_squared);
  float dot_mask = smoothstep(dot_thresholds[1], dot_thresholds[0], dot(co, co));
  float outline_mask = smoothstep(outline_thresholds[0], outline_thresholds[1], distance_squared) +
                       dot_mask;

  frag_out.color = mix(v_out.final_color, v_out.final_outline_color, outline_mask);
  frag_out.color.a *= alpha_mask;
}

}  // namespace builtin::socket

PipelineGraphic gpu_shader_2D_node_socket(builtin::socket::main_vert,
                                          builtin::socket::main_frag,
                                          builtin::socket::Resources{.use_instancing = 0});
PipelineGraphic gpu_shader_2D_node_socket_inst(builtin::socket::main_vert,
                                               builtin::socket::main_frag,
                                               builtin::socket::Resources{.use_instancing = 1});
