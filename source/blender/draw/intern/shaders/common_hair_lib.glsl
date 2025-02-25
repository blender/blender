/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_info.hh"

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
#ifdef HAIR_SHADER
#  define COMMON_HAIR_LIB

#  ifndef DRW_HAIR_INFO
#    error Ensure createInfo includes draw_hair for general use or eevee_legacy_hair_lib for EEVEE.
#  endif

struct CurvePoint {
  vec3 position;
  /* Position along the curve length. O at root, 1 at the tip. */
  float time;
};

CurvePoint hair_get_point(int point_id)
{
  SHADER_LIBRARY_CREATE_INFO(draw_hair)

  vec4 data = texelFetch(hairPointBuffer, point_id);
  CurvePoint pt;
  pt.position = data.xyz;
  pt.time = data.w;
  return pt;
}

/* -- Subdivision stage -- */
/**
 * We use a transform feedback or compute shader to preprocess the strands and add more subdivision
 * to it. For the moment these are simple smooth interpolation but one could hope to see the full
 * children particle modifiers being evaluated at this stage.
 *
 * If no more subdivision is needed, we can skip this step.
 */

float hair_get_local_time()
{
#  ifdef GPU_VERTEX_SHADER
  VERTEX_SHADER_CREATE_INFO(draw_hair)
  return float(gl_VertexID % hairStrandsRes) / float(hairStrandsRes - 1);
#  elif defined(GPU_COMPUTE_SHADER)
  COMPUTE_SHADER_CREATE_INFO(draw_hair_refine_compute)
  return float(gl_GlobalInvocationID.y) / float(hairStrandsRes - 1);
#  else
  return 0;
#  endif
}

int hair_get_id()
{
#  ifdef GPU_VERTEX_SHADER
  VERTEX_SHADER_CREATE_INFO(draw_hair)
  return gl_VertexID / hairStrandsRes;
#  elif defined(GPU_COMPUTE_SHADER)
  COMPUTE_SHADER_CREATE_INFO(draw_hair_refine_compute)
  return int(gl_GlobalInvocationID.x) + hairStrandOffset;
#  else
  return 0;
#  endif
}

#  ifdef HAIR_PHASE_SUBDIV
COMPUTE_SHADER_CREATE_INFO(draw_hair_refine_compute)

int hair_get_base_id(float local_time, int strand_segments, out float interp_time)
{
  float time_per_strand_seg = 1.0 / float(strand_segments);

  float ratio = local_time / time_per_strand_seg;
  interp_time = fract(ratio);

  return int(ratio);
}

void hair_get_interp_attrs(
    out vec4 data0, out vec4 data1, out vec4 data2, out vec4 data3, out float interp_time)
{
  float local_time = hair_get_local_time();

  int hair_id = hair_get_id();
  int strand_offset = int(texelFetch(hairStrandBuffer, hair_id).x);
  int strand_segments = int(texelFetch(hairStrandSegBuffer, hair_id).x);

  int id = hair_get_base_id(local_time, strand_segments, interp_time);

  int ofs_id = id + strand_offset;

  data0 = texelFetch(hairPointBuffer, ofs_id - 1);
  data1 = texelFetch(hairPointBuffer, ofs_id);
  data2 = texelFetch(hairPointBuffer, ofs_id + 1);
  data3 = texelFetch(hairPointBuffer, ofs_id + 2);

  if (id <= 0) {
    /* root points. Need to reconstruct previous data. */
    data0 = data1 * 2.0 - data2;
  }
  if (id + 1 >= strand_segments) {
    /* tip points. Need to reconstruct next data. */
    data3 = data2 * 2.0 - data1;
  }
}
#  endif

/* -- Drawing stage -- */
/**
 * For final drawing, the vertex index and the number of vertex per segment
 */

#  if !defined(HAIR_PHASE_SUBDIV) && defined(GPU_VERTEX_SHADER)
VERTEX_SHADER_CREATE_INFO(draw_hair)

int hair_get_strand_id()
{
  return gl_VertexID / (hairStrandsRes * hairThicknessRes);
}

int hair_get_base_id()
{
  return gl_VertexID / hairThicknessRes;
}

/* Copied from cycles. */
float hair_shaperadius(float shape, float root, float tip, float time)
{
  float radius = 1.0 - time;

  if (shape < 0.0) {
    radius = pow(radius, 1.0 + shape);
  }
  else {
    radius = pow(radius, 1.0 / (1.0 - shape));
  }

  if (hairCloseTip && (time > 0.99)) {
    return 0.0;
  }

  return (radius * (root - tip)) + tip;
}

#    if defined(OS_MAC) && defined(GPU_OPENGL)
in float dummy;
#    endif

void hair_get_center_pos_tan_binor_time(bool is_persp,
                                        vec3 camera_pos,
                                        vec3 camera_z,
                                        out vec3 wpos,
                                        out vec3 wtan,
                                        out vec3 wbinor,
                                        out float time,
                                        out float thickness)
{
  int id = hair_get_base_id();
  CurvePoint data = hair_get_point(id);
  wpos = data.position;
  time = data.time;

#    if defined(OS_MAC) && defined(GPU_OPENGL)
  /* Generate a dummy read to avoid the driver bug with shaders having no
   * vertex reads on macOS (#60171) */
  wpos.y += dummy * 0.0;
#    endif

  if (time == 0.0) {
    /* Hair root */
    wtan = hair_get_point(id + 1).position - wpos;
  }
  else {
    wtan = wpos - hair_get_point(id - 1).position;
  }

  mat4 obmat = hairDupliMatrix;
  wpos = (obmat * vec4(wpos, 1.0)).xyz;
  wtan = -normalize(to_float3x3(obmat) * wtan);

  vec3 camera_vec = (is_persp) ? camera_pos - wpos : camera_z;
  wbinor = normalize(cross(camera_vec, wtan));

  thickness = hair_shaperadius(hairRadShape, hairRadRoot, hairRadTip, time);
}

void hair_get_pos_tan_binor_time(bool is_persp,
                                 mat4 invmodel_mat,
                                 vec3 camera_pos,
                                 vec3 camera_z,
                                 out vec3 wpos,
                                 out vec3 wtan,
                                 out vec3 wbinor,
                                 out float time,
                                 out float thickness,
                                 out float thick_time)
{
  hair_get_center_pos_tan_binor_time(
      is_persp, camera_pos, camera_z, wpos, wtan, wbinor, time, thickness);
  if (hairThicknessRes > 1) {
    thick_time = float(gl_VertexID % hairThicknessRes) / float(hairThicknessRes - 1);
    thick_time = thickness * (thick_time * 2.0 - 1.0);
    /* Take object scale into account.
     * NOTE: This only works fine with uniform scaling. */
    float scale = 1.0 / length(to_float3x3(invmodel_mat) * wbinor);
    wpos += wbinor * thick_time * scale;
  }
  else {
    /* NOTE: Ensures 'hairThickTime' is initialized -
     * avoids undefined behavior on certain macOS configurations. */
    thick_time = 0.0;
  }
}

float hair_get_customdata_float(const samplerBuffer cd_buf)
{
  int id = hair_get_strand_id();
  return texelFetch(cd_buf, id).r;
}

vec2 hair_get_customdata_vec2(const samplerBuffer cd_buf)
{
  int id = hair_get_strand_id();
  return texelFetch(cd_buf, id).rg;
}

vec3 hair_get_customdata_vec3(const samplerBuffer cd_buf)
{
  int id = hair_get_strand_id();
  return texelFetch(cd_buf, id).rgb;
}

vec4 hair_get_customdata_vec4(const samplerBuffer cd_buf)
{
  int id = hair_get_strand_id();
  return texelFetch(cd_buf, id).rgba;
}

vec3 hair_get_strand_pos()
{
  int id = hair_get_strand_id() * hairStrandsRes;
  return hair_get_point(id).position;
}

vec2 hair_get_barycentric()
{
  /* To match cycles without breaking into individual segment we encode if we need to invert
   * the first component into the second component. We invert if the barycentricTexCo.y
   * is NOT 0.0 or 1.0. */
  int id = hair_get_base_id();
  return vec2(float((id % 2) == 1), float(((id % 4) % 3) > 0));
}

#  endif

/* To be fed the result of hair_get_barycentric from vertex shader. */
vec2 hair_resolve_barycentric(vec2 vert_barycentric)
{
  if (fract(vert_barycentric.y) != 0.0) {
    return vec2(vert_barycentric.x, 0.0);
  }
  else {
    return vec2(1.0 - vert_barycentric.x, 0.0);
  }
}

/* Hair interpolation functions. */
vec4 hair_get_weights_cardinal(float t)
{
  float t2 = t * t;
  float t3 = t2 * t;
#  if defined(CARDINAL)
  float fc = 0.71;
#  else /* defined(CATMULL_ROM) */
  float fc = 0.5;
#  endif

  vec4 weights;
  /* GLSL Optimized version of key_curve_position_weights() */
  float fct = t * fc;
  float fct2 = t2 * fc;
  float fct3 = t3 * fc;
  weights.x = (fct2 * 2.0 - fct3) - fct;
  weights.y = (t3 * 2.0 - fct3) + (-t2 * 3.0 + fct2) + 1.0;
  weights.z = (-t3 * 2.0 + fct3) + (t2 * 3.0 - (2.0 * fct2)) + fct;
  weights.w = fct3 - fct2;
  return weights;
}

/* TODO(fclem): This one is buggy, find why. (it's not the optimization!!) */
vec4 hair_get_weights_bspline(float t)
{
  float t2 = t * t;
  float t3 = t2 * t;

  vec4 weights;
  /* GLSL Optimized version of key_curve_position_weights() */
  weights.xz = vec2(-0.16666666, -0.5) * t3 + (0.5 * t2 + 0.5 * vec2(-t, t) + 0.16666666);
  weights.y = (0.5 * t3 - t2 + 0.66666666);
  weights.w = (0.16666666 * t3);
  return weights;
}

vec4 hair_interp_data(vec4 v0, vec4 v1, vec4 v2, vec4 v3, vec4 w)
{
  return v0 * w.x + v1 * w.y + v2 * w.z + v3 * w.w;
}
#endif
