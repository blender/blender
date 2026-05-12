/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_infos.hh"

SHADER_LIBRARY_CREATE_INFO(draw_gpencil)

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "draw_view_lib.glsl"

#include "gpu_shader_math_constants_lib.glsl"
#include "gpu_shader_math_matrix_transform_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"
#include "gpu_shader_utildefines_lib.glsl"

#ifndef DRW_GPENCIL_INFO
#  error Missing additional info draw_gpencil
#endif

#define MITER_LIMIT_TYPE_BEVEL -1.0f
#define MITER_LIMIT_TYPE_ROUND -2.0f

#ifdef GPU_FRAGMENT_SHADER
float gpencil_stroke_hardess_mask(float dist, float hardfac)
{
  dist = clamp(1.0f - dist, 0.0f, 1.0f);
  if (hardfac > 0.999f) {
    return step(1e-8f, dist);
  }
  else {
    /* Modulate the falloff profile */
    float hardness = 1.0f - hardfac;
    dist = pow(dist, mix(0.0f, 10.0f, hardness));
    return smoothstep(0.0f, 1.0f, dist);
  }
}

/**
 *
 * Calculate the mask for the pixel in the main segment (1) by using the distance factor to
 * the center line.
 *
 *      *====================*
 *       \                  /
 *        p1------1>------p2
 *       / \              / \
 *      /   *============*   \
 *     0                      1
 *    /                        \
 *   /                          \
 * p0                            p3
 *
 * Segments: 0 (p0->p1)
 *           1 (p1->p2)
 *           2 (p2->p3)
 *
 * Each point can have a different corner type, stored as p1: miter_limit.x, p2: miter_limit.y
 *
 */
float gpencil_stroke_segment_mask(
    float2 p1, float2 p2, float2 p0, float2 p3, float thickness, float hardfac, float2 miter_limit)
{
  bool both_round = miter_limit.x == MITER_LIMIT_TYPE_ROUND &&
                    miter_limit.y == MITER_LIMIT_TYPE_ROUND;

  bool is_start = distance_squared(p0, p1) < 1e-6;
  bool is_end = distance_squared(p2, p3) < 1e-6;
  bool both_ends = is_start && is_end;

  float radius = thickness * 0.5f;
  float2 pos1 = gl_FragCoord.xy - p1;
  float2 line1 = p2 - p1;
  float2 tan1 = orthogonal(line1);
  float len_sq1 = length_squared(line1);

  /* Calculate the factor along the main segment. */
  float t1 = dot(pos1, line1) / len_sq1;

  /* The distance factor squared to the main segment. This is clamped and will lead to round
   * corners. */
  float dist = length_squared(pos1 - saturate(t1) * line1);

  if (both_round || both_ends) {
    dist = sqrt(dist) / radius;
    return gpencil_stroke_hardess_mask(dist, hardfac);
  }

  float2 line0 = p1 - p0;
  float2 tan0 = orthogonal(line0);
  float len_sq0 = length_squared(line0);

  float2 pos2 = gl_FragCoord.xy - p2;
  float2 line2 = p3 - p2;
  float2 tan2 = orthogonal(line2);
  float len_sq2 = length_squared(line2);

  /* Calculate the non-normalized factor along the other segments. */
  float t0 = dot(pos1, line0);
  float t2 = dot(pos2, line2);

  /* Normalize all segment directions. */
  float2 tan_norm0 = tan0 / sqrt(len_sq0);
  float2 tan_norm1 = tan1 / sqrt(len_sq1);
  float2 tan_norm2 = tan2 / sqrt(len_sq2);

  /* Get the squared distance to the main segment. */
  float dist_sq_1 = length_squared(pos1 - t1 * line1);

  /* Check if the pixel is within the corner region between segments 1 and 0. */
  if (t1 <= 0.0f && t0 >= 0.0f && !is_start && miter_limit.x != MITER_LIMIT_TYPE_ROUND) {
    if (miter_limit.x == MITER_LIMIT_TYPE_BEVEL) {
      /* Bevel by cutting with a the half angle line. */
      float2 bevel_tan = orthogonal(tan_norm0 - tan_norm1);

      dist = dot(pos1, bevel_tan) / dot(tan_norm1, bevel_tan);
      dist *= dist;
    }
    else {
      /* Continue the main line to get a sharp corner. */
      dist = dist_sq_1;
    }
  }

  /* Check if the pixel is within the corner region between segments 1 and 2. */
  if (t1 >= 1.0f && t2 <= 0.0f && !is_end && miter_limit.y != MITER_LIMIT_TYPE_ROUND) {
    if (miter_limit.y == MITER_LIMIT_TYPE_BEVEL) {
      /* Bevel by cutting with a the half angle line. */
      float2 bevel_tan = orthogonal(tan_norm2 - tan_norm1);

      dist = dot(pos2, bevel_tan) / dot(tan_norm1, bevel_tan);
      dist *= dist;
    }
    else {
      /* Continue the main line to get a sharp corner. */
      dist = dist_sq_1;
    }
  }

  dist = sqrt(dist) / radius;
  return gpencil_stroke_hardess_mask(dist, hardfac);
}

float gpencil_stroke_mask(float2 p1,
                          float2 p2,
                          float2 p0,
                          float2 p3,
                          float2 uv,
                          uint mat_flag,
                          float thickness,
                          float hardfac,
                          float2 miter_limit)
{
  if (flag_test(mat_flag, GP_STROKE_ALIGNMENT)) {
    /* Dot or Squares. */
    uv = uv * 2.0 - 1.0;
    if (flag_test(mat_flag, GP_STROKE_DOTS)) {
      return gpencil_stroke_hardess_mask(length(uv), hardfac);
    }
    else {
      uv = abs(uv);
      return gpencil_stroke_hardess_mask(max(uv.x, uv.y), hardfac);
    }
  }
  else {
    /* Line mask */
    return gpencil_stroke_segment_mask(p1, p2, p0, p3, thickness, hardfac, miter_limit);
  }
}

#endif

struct PointData {
  bool cyclical;
  int mat, stroke_id, point_id, packed_data;
};

PointData decode_ma(int4 ma)
{
  PointData data;

  data.mat = ma.x;
  data.stroke_id = ma.y;
  /* Take the absolute because the sign is for cyclical. */
  data.point_id = abs(ma.z);
  /* Aspect, UV Rotation and Hardness. */
  data.packed_data = ma.w;
  /* Cyclical is stored in the sign of the point index. */
  data.cyclical = ma.z < 0;

  return data;
}

float4 ndc_and_radius_to_screen_space(float4 ndc, float radius, float2 viewport_res)
{
  return float4(((ndc.xy / ndc.w) * 0.5f + 0.5f) * viewport_res, ndc.w, radius / ndc.w);
}

float4 screen_space_to_ndc(float4 ss, float2 viewport_res)
{
  return float4((ss.xy / viewport_res - 0.5f) * 2.0f * ss.z, 0.0f, ss.z);
}

float screen_space_to_radius(float4 ss)
{
  return ss.w * ss.z;
}

float2 gpencil_decode_aspect(int packed_data)
{
  float asp = float(uint(packed_data) & 0x1FFu) * (1.0f / 255.0f);
  return (asp > 1.0f) ? float2(1.0f, (asp - 1.0f)) : float2(asp, 1.0f);
}

float gpencil_decode_uvrot(int packed_data)
{
  uint udata = uint(packed_data);
  float uvrot = 1e-8f + float((udata & 0x1FE00u) >> 9u) * (1.0f / 255.0f);
  return ((udata & 0x20000u) != 0u) ? -uvrot : uvrot;
}

float gpencil_decode_hardness(int packed_data)
{
  return float((uint(packed_data) & 0x3FC0000u) >> 18u) * (1.0f / 255.0f);
}

float gpencil_decode_miter_limit(int packed_data)
{
  uint miter_data = (uint(packed_data) & 0xFC000000u) >> 26u;
  if (miter_data == GP_CORNER_TYPE_ROUND_BITS) {
    return MITER_LIMIT_TYPE_ROUND;
  }
  else if (miter_data == GP_CORNER_TYPE_BEVEL_BITS) {
    return MITER_LIMIT_TYPE_BEVEL;
  }
  float miter_angle = float(miter_data) * (M_PI / GP_CORNER_TYPE_MITER_NUMBER);
  return cos(miter_angle);
}

float2 gpencil_project_to_screenspace(float4 v, float4 viewport_res)
{
  return ((v.xy / v.w) * 0.5f + 0.5f) * viewport_res.xy;
}

float gpencil_stroke_thickness_modulate(float thickness, float4 viewport_res)
{
  /* Modify stroke thickness by object scale. */
  thickness = length(to_float3x3(drw_modelmat()) * float3(thickness * M_SQRT1_3));

  /* World space point size. */
  thickness *= drw_view().winmat[1][1] * viewport_res.y;

  return thickness;
}

#ifdef GPU_VERTEX_SHADER

int gpencil_stroke_point_id()
{
  return (gl_VertexID & ~GP_IS_STROKE_VERTEX_BIT) >> GP_VERTEX_ID_SHIFT;
}

bool gpencil_is_stroke_vertex()
{
  return flag_test(gl_VertexID, GP_IS_STROKE_VERTEX_BIT);
}

float4 discard_ndc()
{
  /* We set the vertex at the camera origin to generate 0 fragments. */
  return float4(0.0f, 0.0f, -3e36f, 0.0f);
}

/**
 * Calculate a shape that fully covers the segment.
 *
 * Will generate either a trapezoid or a rectangle depending on which has smaller area.
 */
float4 dot_segment(float2 xy, float4 ss1, float4 ss2, bool is_squares, float4 viewport_res)
{
  float l = 0.0f;
  float2 local_x = safe_normalize_and_get_length(ss2.xy - ss1.xy, l);
  float2 local_y = orthogonal(local_x);

  float r1 = ss1.w;
  float r2 = ss2.w;

  /* Scale each circle up by the diagonal of the square. */
  if (is_squares) {
    r1 *= M_SQRT2;
    r2 *= M_SQRT2;
  }

  float x = xy.x;
  float y = xy.y;

  float max_r = max(r1, r2);
  float a = r2 - r1;
  float cos_theta = -a / l;
  float sin_theta = sqrt(1.0f - cos_theta * cos_theta);
  float tan_half_theta = (1.0f - cos_theta) / sin_theta;

  /* Discard segments that are behind the camera. */
  if (ss1.z < 0.0f || ss2.z < 0.0f) {
    return discard_ndc();
  }

  float2 local = float2(0.0f, 0.0f);

  /* Check if one circle is inside the other. */
  if (abs(cos_theta) > 1.0f) {
    if (r1 > r2) {
      local = xy * r1;
    }
    else {
      local = xy * r2 + float2(l, 0.0f);
    }
  }
  else {
    /* Calculate the area of both shapes divided by the width */
    float area_trapezoid = 0.5f * (r1 / tan_half_theta + r2 * tan_half_theta);
    float area_rectangle = max_r;

    /* Determent if it is better to use a trapezoid or a rectangle. */
    if (area_trapezoid < area_rectangle) {
      if (x == -1.0f) {
        local.x += -r1;
        local.y += y * r1 / tan_half_theta;
      }
      else {
        local.x += l + r2;
        local.y += y * r2 * tan_half_theta;
      }
    }
    else {
      if (x == -1.0f) {
        local.x -= r1;
        local.y += y * max_r;
      }
      else {
        local.x += l + r2;
        local.y += y * max_r;
      }
    }
  }

  float2 ssp = ss1.xy + local.x * local_x + local.y * local_y;

  float z_depth = mix(ss1.z, ss2.z, 0.5f);
  return screen_space_to_ndc(float4(ssp, z_depth, 0.0f), viewport_res.xy);
}

float2 get_rotation(float4 viewport_res,
                    gpMaterialFlag material_flags,
                    float2 alignment_rot,
                    int4 ma,
                    int4 ma2,
                    float2 line,
                    float2 line_adj,
                    float3 wpos1,
                    float uv_rot,
                    float2 ss1)
{
  uint alignment_mode = material_flags & GP_STROKE_ALIGNMENT;

  /* For one point strokes use object alignment. */
  if (alignment_mode == GP_STROKE_ALIGNMENT_STROKE && ma.x == -1 && ma2.x == -1) {
    alignment_mode = GP_STROKE_ALIGNMENT_OBJECT;
  }

  float2 x_axis;
  if (alignment_mode == GP_STROKE_ALIGNMENT_STROKE) {
    x_axis = (ma2.x == -1) ? line_adj : line;
  }
  else if (alignment_mode == GP_STROKE_ALIGNMENT_FIXED) {
    /* Default for no-material drawing. */
    x_axis = float2(1.0f, 0.0f);
  }
  else { /* GP_STROKE_ALIGNMENT_OBJECT */
    float4 ndc_x = drw_point_world_to_homogenous(wpos1 + drw_modelmat()[0].xyz);
    float2 ss_x = gpencil_project_to_screenspace(ndc_x, viewport_res);
    x_axis = safe_normalize(ss_x - ss1);
  }

  float rot_sin = sqrt(max(0.0f, 1.0f - uv_rot * uv_rot)) * sign(uv_rot);
  float rot_cos = abs(uv_rot);
  /* TODO(@fclem): Optimize these 2 matrix multiply into one by only having one rotation angle
   * and using a cosine approximation. */
  x_axis = float2x2(rot_cos, -rot_sin, rot_sin, rot_cos) * x_axis;
  x_axis = float2x2(alignment_rot.x, -alignment_rot.y, alignment_rot.y, alignment_rot.x) * x_axis;

  return x_axis;
}

/**
 * Returns value of gl_Position.
 *
 * To declare in vertex shader.
 * in ivec4 ma, ma1, ma2, ma3;
 * in float4 pos, pos1, pos2, pos3, uv1, uv2, col1, col2, fcol1;
 *
 * All of these attributes are quad loaded the same way
 * as GL_LINES_ADJACENCY would feed a geometry shader:
 * - ma reference the previous adjacency point.
 * - ma1 reference the current line first point.
 * - ma2 reference the current line second point.
 * - ma3 reference the next adjacency point.
 * Note that we are rendering quad instances and not using any index buffer
 *(except for fills).
 *
 * Material : x is material index, y is stroke_id, z is point_id,
 *            w is aspect & rotation & hardness packed.
 * Position : contains thickness in 4th component.
 * UV : xy is UV for fills, z is U of stroke, w is strength.
 *
 *
 * WARNING: Max attribute count is actually 14 because OSX OpenGL implementation
 * considers gl_VertexID and gl_InstanceID as vertex attribute. (see #74536)
 */
float4 gpencil_vertex(float4 viewport_res,
                      gpMaterialFlag material_flags,
                      float2 alignment_rot,
                      /* World Position. */
                      float3 &out_P,
                      /* World Normal. */
                      float3 &out_N,
                      /* Vertex Color. */
                      float4 &out_color,
                      /* Stroke Strength. */
                      float &out_strength,
                      /* UV coordinates. */
                      float2 &out_uv,
                      /* Screen-Space positions. */
                      float2 &out_sspos_0,
                      float4 &out_sspos_1,
                      float4 &out_sspos_2,
                      float2 &out_sspos_3,
                      /* Object-space accumulated length from the start of the stroke
                        (x: point 1, y: point 2, z: point density).
                        Note: Z must be already given. */
                      float3 &out_point_length,
                      /* Stroke aspect ratio and rotation direction
                        (xy: aspect ration, zw: rotation direction). */
                      float4 &out_aspect,
                      /* Stroke thickness and miter limits (x: clamped, y: unclamped,
                       * z: miter limit segment start, w: miter limit segment end). */
                      float4 &out_thickness,
                      /* Stroke hardness. */
                      float &out_hardness)
{
  int stroke_point_id = gpencil_stroke_point_id();

  /* Attribute Loading. */
  float4 pos = texelFetch(gp_pos_tx, (stroke_point_id - 1) * 3 + 0);
  float4 pos1 = texelFetch(gp_pos_tx, (stroke_point_id + 0) * 3 + 0);
  float4 pos2 = texelFetch(gp_pos_tx, (stroke_point_id + 1) * 3 + 0);
  float4 pos3 = texelFetch(gp_pos_tx, (stroke_point_id + 2) * 3 + 0);
  int4 ma = floatBitsToInt(texelFetch(gp_pos_tx, (stroke_point_id - 1) * 3 + 1));
  int4 ma1 = floatBitsToInt(texelFetch(gp_pos_tx, (stroke_point_id + 0) * 3 + 1));
  int4 ma2 = floatBitsToInt(texelFetch(gp_pos_tx, (stroke_point_id + 1) * 3 + 1));
  int4 ma3 = floatBitsToInt(texelFetch(gp_pos_tx, (stroke_point_id + 2) * 3 + 1));
  float4 uv1 = texelFetch(gp_pos_tx, (stroke_point_id + 0) * 3 + 2);
  float4 uv2 = texelFetch(gp_pos_tx, (stroke_point_id + 1) * 3 + 2);

  float4 col1 = texelFetch(gp_col_tx, (stroke_point_id + 0) * 2 + 0);
  float4 col2 = texelFetch(gp_col_tx, (stroke_point_id + 1) * 2 + 0);
  float4 fcol1 = texelFetch(gp_col_tx, (stroke_point_id + 0) * 2 + 1);

#  define thickness1 pos1.w
#  define thickness2 pos2.w
#  define strength1 uv1.w
#  define strength2 uv2.w

  float4 out_ndc;

  if (gpencil_is_stroke_vertex()) {
    uint placement_mode = material_flags & GP_DOTS_PLACEMENT_MODE;
    bool is_single_dot = placement_mode == GP_DOTS_PLACEMENT_MODE_COUNT &&
                         out_point_length.z == 1.0f;
    bool is_dot = flag_test(material_flags, GP_STROKE_ALIGNMENT);
    bool is_squares = !flag_test(material_flags, GP_STROKE_DOTS);

    bool is_first = (ma.x == -1);
    bool is_last = (ma3.x == -1);
    bool is_single = is_first && (ma2.x == -1);

    PointData point_data1 = decode_ma(ma1);
    PointData point_data2 = decode_ma(ma2);

    /* Join the first and last point if the curve is cyclical. */
    if (point_data1.cyclical && !is_single) {
      if (is_first) {
        /* The first point will have the index of the last point. */
        PointData point_data = decode_ma(ma);
        int last_stroke_id = point_data.stroke_id;
        ma = floatBitsToInt(texelFetch(gp_pos_tx, (last_stroke_id - 2) * 3 + 1));
        pos = texelFetch(gp_pos_tx, (last_stroke_id - 2) * 3 + 0);
      }

      if (is_last) {
        PointData point_data3 = decode_ma(ma3);
        int first_stroke_id = point_data3.stroke_id;
        ma3 = floatBitsToInt(texelFetch(gp_pos_tx, (first_stroke_id + 2) * 3 + 1));
        pos3 = texelFetch(gp_pos_tx, (first_stroke_id + 2) * 3 + 0);
      }
    }

    /* Special Case. Stroke with single vert are rendered as dots. Do not discard them. */
    if (!is_dot && is_single) {
      is_dot = true;
      is_squares = false;
      is_single_dot = true;
    }

    /* Endpoints, we discard the vertices. */
    if (!(is_dot && is_single_dot) && ma2.x == -1) {
      return discard_ndc();
    }

    /* Avoid using a vertex attribute for quad positioning. */
    float x = float(gl_VertexID & 1) * 2.0f - 1.0f; /* [-1..1] */
    float y = float(gl_VertexID & 2) - 1.0f;        /* [-1..1] */

    bool use_curr = (is_dot && is_single_dot) || (x == -1.0f);

    float3 wpos0 = transform_point(drw_modelmat(), pos.xyz);
    float3 wpos1 = transform_point(drw_modelmat(), pos1.xyz);
    float3 wpos2 = transform_point(drw_modelmat(), pos2.xyz);
    float3 wpos3 = transform_point(drw_modelmat(), pos3.xyz);
    float3 wpos_adj = (use_curr) ? wpos0 : wpos3;

    float3 T;
    if (is_dot && is_single_dot) {
      /* Shade as facing billboards. */
      T = drw_view().viewinv[0].xyz;
    }
    else if (use_curr && ma.x != -1) {
      T = wpos1 - wpos_adj;
    }
    else {
      T = wpos2 - wpos1;
    }
    T = safe_normalize(T);

    float3 B = cross(T, drw_view().viewinv[2].xyz);
    out_N = normalize(cross(B, T));

    float4 ndc0 = drw_point_world_to_homogenous(wpos0);
    float4 ndc1 = drw_point_world_to_homogenous(wpos1);
    float4 ndc2 = drw_point_world_to_homogenous(wpos2);
    float4 ndc3 = drw_point_world_to_homogenous(wpos3);

    out_ndc = (use_curr) ? ndc1 : ndc2;
    out_P = (use_curr) ? wpos1 : wpos2;
    out_strength = abs((use_curr) ? strength1 : strength2);

    float radius1 = gpencil_stroke_thickness_modulate(thickness1, viewport_res) / 2.0;
    float radius2 = gpencil_stroke_thickness_modulate(thickness2, viewport_res) / 2.0;

    float2 ss0 = gpencil_project_to_screenspace(ndc0, viewport_res);
    float4 ss1 = ndc_and_radius_to_screen_space(ndc1, radius1, viewport_res.xy);
    float4 ss2 = ndc_and_radius_to_screen_space(ndc2, radius2, viewport_res.xy);
    float2 ss3 = gpencil_project_to_screenspace(ndc3, viewport_res);

    /* Screen-space Lines tangents. */
    float2 line = safe_normalize(ss2.xy - ss1.xy);
    float2 line1 = safe_normalize(ss1.xy - ss0);
    float2 line2 = safe_normalize(ss3 - ss2.xy);
    float2 line_adj = (use_curr) ? line1 : line2;

    float thickness = abs((use_curr) ? thickness1 : thickness2);
    thickness = gpencil_stroke_thickness_modulate(thickness, viewport_res);
    /* The radius attribute can have negative values. Make sure that it's not negative by clamping
     * to 0. */
    float clamped_thickness = max(0.0f, thickness);

    out_uv = float2(x, y) * 0.5f + 0.5f;
    out_hardness = gpencil_decode_hardness(use_curr ? point_data1.packed_data :
                                                      point_data2.packed_data);

    out_sspos_1 = ss1;
    if (ma2.x != -1) {
      out_sspos_2 = ss2;
    }
    else {
      out_sspos_2 = out_sspos_1;
    }
    if (ma.x != -1) {
      out_sspos_0 = ss0;
    }
    else {
      out_sspos_0 = out_sspos_1.xy;
    }
    if (ma3.x != -1) {
      out_sspos_3 = ss3;
    }
    else {
      out_sspos_3 = out_sspos_2.xy;
    }

    /* Z is already given. */
    out_point_length.xy = float2(uv1.z, uv2.z);

    if (is_dot && !is_single_dot) {
      out_thickness.x = clamped_thickness / out_ndc.w;
      out_thickness.y = thickness / out_ndc.w;

      /* Rotation: Encoded as Cos + Sin sign. */
      float uv_rot = gpencil_decode_uvrot(point_data1.packed_data);
      float2 x_axis = get_rotation(viewport_res,
                                   material_flags,
                                   alignment_rot,
                                   ma,
                                   ma2,
                                   line,
                                   line_adj,
                                   wpos1,
                                   uv_rot,
                                   ss1.xy);
      out_aspect.xy = gpencil_decode_aspect(point_data1.packed_data);
      /* Invert for vertex shader. */
      out_aspect.xy = 1.0f / out_aspect.xy;
      out_aspect.zw = x_axis;

      out_ndc = dot_segment(float2(x, y), ss1, ss2, is_squares, viewport_res);

      out_ndc /= out_ndc.w;
      out_ndc.z = (use_curr) ? (ndc1.z / ndc1.w) : (ndc2.z / ndc2.w);

      out_uv.x = (use_curr) ? uv1.z : uv2.z;
    }
    else if (is_dot && is_single_dot) {
      /* Rotation: Encoded as Cos + Sin sign. */
      float uv_rot = gpencil_decode_uvrot(point_data1.packed_data);
      float2 x_axis = get_rotation(viewport_res,
                                   material_flags,
                                   alignment_rot,
                                   ma,
                                   ma2,
                                   line,
                                   line_adj,
                                   wpos1,
                                   uv_rot,
                                   ss1.xy);
      /* Rotate 90 degrees counter-clockwise. */
      float2 y_axis = float2(-x_axis.y, x_axis.x);

      out_aspect.xy = gpencil_decode_aspect(point_data1.packed_data);
      out_aspect.zw = x_axis;

      x *= out_aspect.x;
      y *= out_aspect.y;

      /* Invert for vertex shader. */
      out_aspect.xy = 1.0f / out_aspect.xy;

      out_ndc.xy += (x * x_axis + y * y_axis) * viewport_res.zw * clamped_thickness;
      out_sspos_0.xy = ss1.xy + x_axis * 0.5f;

      out_thickness.x = (is_squares) ? 1e18f : (clamped_thickness / out_ndc.w);
      out_thickness.y = (is_squares) ? 1e18f : (thickness / out_ndc.w);
      out_thickness.z = MITER_LIMIT_TYPE_ROUND;
      out_thickness.w = MITER_LIMIT_TYPE_ROUND;
    }
    else {
      bool is_stroke_start = (ma.x == -1 && x == -1.0f);
      bool is_stroke_end = (ma3.x == -1 && x == 1.0f);

      float miter_limit1 = gpencil_decode_miter_limit(point_data1.packed_data);
      float miter_limit2 = gpencil_decode_miter_limit(point_data2.packed_data);

      float cos_angle1 = -dot(line, line1);
      float cos_angle2 = -dot(line, line2);

      out_thickness.z = miter_limit1;
      out_thickness.w = miter_limit2;

      if (cos_angle1 > miter_limit1 && miter_limit1 != MITER_LIMIT_TYPE_ROUND) {
        out_thickness.z = MITER_LIMIT_TYPE_BEVEL;
      }
      if (cos_angle2 > miter_limit2 && miter_limit2 != MITER_LIMIT_TYPE_ROUND) {
        out_thickness.w = MITER_LIMIT_TYPE_BEVEL;
      }

      float miter_limit = use_curr ? miter_limit1 : miter_limit2;

      if (miter_limit == MITER_LIMIT_TYPE_BEVEL || miter_limit == MITER_LIMIT_TYPE_ROUND) {
        miter_limit = 0.5f; /* Default to cos(60) */
      }

      /* Prevent the limit from becoming to small. */
      if (miter_limit < 0.5f) {
        miter_limit = 0.5f;
      }

      /* Mitter tangent vector. */
      float2 miter_tan = safe_normalize(line_adj + line);
      float miter_dot = dot(miter_tan, line_adj);
      float cos_angle_adj = (use_curr) ? cos_angle1 : cos_angle2;
      /* Break corners after a certain angle to avoid really thick corners. */
      bool miter_break = cos_angle_adj > miter_limit;
      miter_tan = (miter_break || is_stroke_start || is_stroke_end) ? line :
                                                                      (miter_tan / miter_dot);
      /* Rotate 90 degrees counter-clockwise. */
      float2 miter = float2(-miter_tan.y, miter_tan.x);

      out_thickness.x = clamped_thickness / out_ndc.w;
      out_thickness.y = thickness / out_ndc.w;
      out_aspect = float4(1.0f, 1.0f, 1.0f, 0.0f);

      float2 screen_ofs = miter * y;

      /* Reminder: we packed the cap flag into the sign of strength and thickness sign. */
      if ((is_stroke_start && strength1 > 0.0f) || (is_stroke_end && thickness1 > 0.0f) ||
          (miter_break && !is_stroke_start && !is_stroke_end))
      {
        screen_ofs += line * x;
      }

      out_ndc.xy += screen_ofs * viewport_res.zw * clamped_thickness;

      out_uv.x = (use_curr) ? uv1.z : uv2.z;
    }

    out_color = (use_curr) ? col1 : col2;
  }
  else {
    out_P = transform_point(drw_modelmat(), pos1.xyz);
    out_ndc = drw_point_world_to_homogenous(out_P);
    out_uv = uv1.xy;
    out_thickness.x = 1e18f;
    out_thickness.y = 1e20f;
    out_thickness.z = MITER_LIMIT_TYPE_ROUND;
    out_thickness.w = MITER_LIMIT_TYPE_ROUND;
    out_hardness = 1.0f;
    out_aspect = float4(1.0f, 1.0f, 1.0f, 0.0f);
    out_sspos_0 = float2(0.0f);
    out_sspos_1 = float4(0.0f);
    out_sspos_2 = float4(0.0f);
    out_sspos_3 = float2(0.0f);
    out_point_length = float3(0.0f);

    /* Flat normal following camera and object bounds. */
    float3 V = drw_world_incident_vector(drw_modelmat()[3].xyz);
    float3 N = drw_normal_world_to_object(V);
    N *= drw_object_infos().orco_mul;
    N = drw_normal_world_to_object(N);
    out_N = safe_normalize(N);

    /* Decode fill opacity. */
    out_color = float4(fcol1.rgb, floor(fcol1.a / 10.0f) / 10000.0f);

    /* We still offset the fills a little to avoid overlaps */
    out_ndc.z += 0.000002f;
  }

#  undef thickness1
#  undef thickness2
#  undef strength1
#  undef strength2

  return out_ndc;
}

float4 gpencil_vertex(float4 viewport_res,
                      float3 &out_P,
                      float3 &out_N,
                      float4 &out_color,
                      float &out_strength,
                      float2 &out_uv,
                      float2 &out_sspos_0,
                      float4 &out_sspos_1,
                      float4 &out_sspos_2,
                      float2 &out_sspos_3,
                      float3 &out_point_length,
                      float4 &out_aspect,
                      float4 &out_thickness,
                      float &out_hardness)
{
  return gpencil_vertex(viewport_res,
                        gpMaterialFlag(0u),
                        float2(1.0f, 0.0f),
                        out_P,
                        out_N,
                        out_color,
                        out_strength,
                        out_uv,
                        out_sspos_0,
                        out_sspos_1,
                        out_sspos_2,
                        out_sspos_3,
                        out_point_length,
                        out_aspect,
                        out_thickness,
                        out_hardness);
}

#endif
