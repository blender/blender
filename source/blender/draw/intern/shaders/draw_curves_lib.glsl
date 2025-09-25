/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_infos.hh"

#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_matrix_conversion_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"

/**
 * Library to create hairs dynamically from control points.
 * This is less bandwidth intensive than fetching the vertex attributes
 * but does more ALU work per vertex. This also reduces the amount
 * of data the CPU has to precompute and transfer for each update.
 */

/* Avoid including hair functionality for shaders and materials which do not require hair.
 * Required to prevent compilation failure for missing shader inputs and uniforms when hair library
 * is included via other libraries. These are only specified in the ShaderCreateInfo when needed.
 */
#ifdef CURVES_SHADER
#  ifndef DRW_HAIR_INFO
#    error Ensure createInfo includes draw_hair.
#  endif

namespace curves {

struct Segment {
  /* Index of this segment. Used to load indirection buffer. */
  uint id;
  /* Vertex index inside this segment. */
  uint v_idx;
  /* Restart triangle strip if true. Only for cylinder topology. */
  bool is_end_of_segment;
};

/* Indirection buffer indexing. */
Segment segment_get(uint vertex_id)
{
  const auto &drw_curves = buffer_get(draw_curves_infos, drw_curves);

  const bool is_cylinder = drw_curves.half_cylinder_face_count > 1u;
  Segment segment;
  segment.id = vertex_id / drw_curves.vertex_per_segment;
  segment.v_idx = vertex_id % drw_curves.vertex_per_segment;
  segment.is_end_of_segment = is_cylinder && (segment.v_idx == drw_curves.vertex_per_segment - 1);
  if (is_cylinder && !segment.is_end_of_segment && (segment.id & 1u) == 0u) {
    /* The topology is not actually restarted and the winding order changes (because we skip an odd
     * number of triangles). So we have to manually reverse the winding so that is stays
     * consistent.
     */
    segment.v_idx ^= 1u;
  }
  return segment;
}

/* Result of interpreting the indirection buffer.
 * The indirection buffer maps drawn segments back to their curves and curve segments.
 * This is needed for attribute loading. */
struct Indirection {
  /* Can be equal to INT_MAX with ribbon draw type. */
  int curve_id;
  /* Segment ID starting at 0 at curve start. */
  int curve_segment;
  /* Restart triangle strip if true. Only for ribbon topology. */
  bool is_end_of_curve;
  /* Does these vertices correspond to the last point of a cyclic curve (duplicate of start). */
  bool is_cyclic_point;
};

Indirection indirection_get(Segment segment)
{
  const auto &curves_indirection_buf = sampler_get(draw_curves, curves_indirection_buf);
  const auto &drw_curves = buffer_get(draw_curves_infos, drw_curves);

  Indirection ind;
  ind.is_cyclic_point = false;
  ind.is_end_of_curve = false;
  ind.curve_id = 0;

  constexpr int cyclic_endpoint_pivot = INT_MAX / 2;
  constexpr int end_of_curve = INT_MAX;

  int indirection_value = texelFetch(curves_indirection_buf, int(segment.id)).r;
  if (indirection_value == end_of_curve) {
    ind.curve_segment = 0;
    ind.is_end_of_curve = true;
  }
  else if (indirection_value >= 0) {
    /* This is start of curve. The indirection value is the curve ID. */
    ind.curve_segment = 0;
    ind.curve_id = indirection_value;
  }
  else if (indirection_value <= -cyclic_endpoint_pivot) {
    /* This is the last segment of a cyclic curve. The indirection value is the offset to the start
     * of the curve offsetted by cyclic_endpoint_pivot. */
    ind.curve_segment = -indirection_value - cyclic_endpoint_pivot;
    ind.is_cyclic_point = true;
  }
  else {
    /* This is a normal segment. The indirection value is the offset to the start of the curve. */
    ind.curve_segment = -indirection_value;
  }

  if (ind.curve_segment != 0) {
    ind.curve_id = texelFetch(curves_indirection_buf, int(segment.id) - ind.curve_segment).r;
  }

  const bool is_cylinder = drw_curves.half_cylinder_face_count > 1u;
  if (is_cylinder) {
    bool is_end_of_segment = (segment.v_idx & 1u) != 0u;
    ind.curve_segment += int(is_end_of_segment);
    /* Only the end of the segment is to be considered the cyclic point. */
    if (!is_end_of_segment) {
      ind.is_cyclic_point = false;
    }
  }
  return ind;
}

int point_id_get(Segment segment, Indirection indirection)
{
  const auto &drw_curves = buffer_get(draw_curves_infos, drw_curves);

  const bool is_cylinder = drw_curves.half_cylinder_face_count > 1u;
  if (is_cylinder) {
    return int(segment.id) + indirection.curve_id + int(segment.v_idx & 1u);
  }
  return int(segment.id) - indirection.curve_id;
}

float azimuthal_offset_get(Segment segment)
{
  const auto &drw_curves = buffer_get(draw_curves_infos, drw_curves);

  float offset;
  const bool is_strand = drw_curves.half_cylinder_face_count == 0u;
  const bool is_cylinder = drw_curves.half_cylinder_face_count > 1u;
  if (is_strand) {
    offset = 0.5f;
  }
  else if (is_cylinder) {
    offset = float(segment.v_idx >> 1) / float(drw_curves.half_cylinder_face_count);
  }
  else {
    offset = float(segment.v_idx);
  }
  return offset * 2.0f - 1.0f;
}

float4 point_position_and_radius_get(uint point_id)
{
  const auto &curves_pos_rad_buf = buffer_get(draw_curves, curves_pos_rad_buf);

  return texelFetch(curves_pos_rad_buf, int(point_id));
}

float3 point_position_get(uint point_id)
{
  const auto &curves_pos_rad_buf = buffer_get(draw_curves, curves_pos_rad_buf);

  return texelFetch(curves_pos_rad_buf, int(point_id)).rgb;
}

struct Point {
  /* Position of the evaluated curve point (not the shape / cylinder point). */
  float3 P;
  /* Tangent vector going from the root to the tip of the curve. */
  float3 T;

  float radius;
  /* Lateral/Azimuthal offset from the center of the curve's width. Range [-1..1]. */
  float azimuthal_offset;

  int point_id;
  int curve_id;
  int curve_segment;
};

/* Return data about the curve point. */
Point point_get(uint vertex_id)
{
  Segment segment = segment_get(vertex_id);
  Indirection indirection = indirection_get(segment);

  Point pt;
  pt.point_id = point_id_get(segment, indirection);
  pt.curve_id = indirection.curve_id;
  pt.curve_segment = indirection.curve_segment;

  float4 pos_rad = point_position_and_radius_get(pt.point_id);

  bool restart_strip = indirection.is_end_of_curve || segment.is_end_of_segment;
  pt.P = (restart_strip) ? float3(NAN_FLT) : pos_rad.xyz;
  pt.radius = pos_rad.w;
  pt.azimuthal_offset = azimuthal_offset_get(segment);

  if (pt.curve_segment == 0) {
    /* Hair root. */
    pt.T = point_position_get(pt.point_id + 1) - pt.P;
  }
  else if (indirection.is_cyclic_point) {
    /* Cyclic end point must match start point. */
    pt.T = point_position_get(pt.point_id - pt.curve_segment + 1) - pt.P;
  }
  else {
    pt.T = pt.P - point_position_get(pt.point_id - 1);
  }
  return pt;
}

Point object_to_world(Point pt, float4x4 object_to_world)
{
  pt.P = transform_point(object_to_world, pt.P);
  pt.T = normalize(transform_direction(object_to_world, pt.T));
  pt.radius *= length(to_scale(object_to_world)) * M_SQRT1_3;
  return pt;
}

struct ShapePoint {
  /* Curve tangent space. */
  float3 curve_N;
  float3 curve_T;
  float3 curve_B;
  /* Position on the curve shape. */
  float3 P;
  /* Shading normal at the position on the curve shape. */
  float3 N;
};

/**
 * Return the position of the expanded position in world-space.
 * \arg pt : world space curve point.
 * \arg V : world space view vector (toward viewer) at `pt.P`.
 */
ShapePoint shape_point_get(const Point pt, const float3 V)
{
  const bool is_strand = buffer_get(draw_curves_infos, drw_curves).half_cylinder_face_count == 0u;

  ShapePoint shape;
  /* Shading tangent is inverted because of legacy reason. */
  /* TODO(fclem): Change user code. */
  shape.curve_T = -pt.T;
  shape.curve_B = normalize(cross(pt.T, V));
  shape.curve_N = cross(shape.curve_B, pt.T);
  /* Point in curve azimuthal space. */
  const float2 lP = float2(pt.azimuthal_offset, sin_from_cos(abs(pt.azimuthal_offset)));
  shape.N = shape.curve_B * lP.x + shape.curve_N * lP.y;
  shape.P = pt.P + shape.N * (is_strand ? 0.0f : pt.radius);
  return shape;
}

float get_customdata_float(const int curve_id, const samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curve_id).x;
}

float2 get_customdata_vec2(const int curve_id, const samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curve_id).xy;
}

float3 get_customdata_vec3(const int curve_id, const samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curve_id).xyz;
}

float4 get_customdata_vec4(const int curve_id, const samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curve_id).xyzw;
}

float3 get_curve_root_pos(const int point_id, const int curve_segment)
{
  const auto &curves_pos_rad_buf = buffer_get(draw_curves, curves_pos_rad_buf);

  int curve_start = point_id - curve_segment;
  return texelFetch(curves_pos_rad_buf, curve_start).xyz;
}

}  // namespace curves

#endif
