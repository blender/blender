/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_info.hh"

#ifdef GPU_LIBRARY_SHADER
#  define HAIR_SHADER
#  define DRW_HAIR_INFO
#endif

SHADER_LIBRARY_CREATE_INFO(draw_modelmat_new)
SHADER_LIBRARY_CREATE_INFO(draw_resource_handle_new)
SHADER_LIBRARY_CREATE_INFO(draw_hair_new)

#include "common_hair_lib.glsl" /* TODO rename to curve. */
#include "draw_model_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_matrix_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

#ifdef GPU_VERTEX_SHADER

/* -------------------------------------------------------------------- */
/** \name Curve
 *
 * Curve objects loads attributes from buffers through sampler buffers.
 * Per attribute scope follows loading order.
 * \{ */

#  ifdef OBINFO_LIB
vec3 attr_load_orco(vec4 orco)
{
  vec3 P = hair_get_strand_pos();
  vec3 lP = transform_point(ModelMatrixInverse, P);
  return OrcoTexCoFactors[0].xyz + lP * OrcoTexCoFactors[1].xyz;
}
#  endif

int g_curves_attr_id = 0;

/* Return the index to use for looking up the attribute value in the sampler
 * based on the attribute scope (point or spline). */
int curves_attribute_element_id()
{
  int id = curve_interp_flat.strand_id;
  if (drw_curves.is_point_attribute[g_curves_attr_id][0] != 0u) {
#  ifdef COMMON_HAIR_LIB
    id = hair_get_base_id();
#  endif
  }

  g_curves_attr_id += 1;
  return id;
}

vec4 attr_load_tangent(samplerBuffer cd_buf)
{
  /* Not supported for the moment. */
  return vec4(0.0, 0.0, 0.0, 1.0);
}
vec3 attr_load_uv(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curve_interp_flat.strand_id).rgb;
}
vec4 attr_load_color(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curve_interp_flat.strand_id).rgba;
}
vec4 attr_load_vec4(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curves_attribute_element_id()).rgba;
}
vec3 attr_load_vec3(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curves_attribute_element_id()).rgb;
}
vec2 attr_load_vec2(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curves_attribute_element_id()).rg;
}
float attr_load_float(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curves_attribute_element_id()).r;
}

/** \} */

#endif
