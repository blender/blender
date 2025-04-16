/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_info.hh"

#ifdef GPU_LIBRARY_SHADER
#  define HAIR_SHADER
#  define DRW_HAIR_INFO
#endif

SHADER_LIBRARY_CREATE_INFO(draw_modelmat)
SHADER_LIBRARY_CREATE_INFO(draw_hair)

#include "draw_curves_lib.glsl" /* TODO rename to curve. */
#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
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
float3 attr_load_orco(float4 orco)
{
  float3 P = hair_get_strand_pos();
  float3 lP = transform_point(drw_modelinv(), P);
  return drw_object_orco(lP);
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

float4 attr_load_tangent(samplerBuffer cd_buf)
{
  /* Not supported for the moment. */
  return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
float3 attr_load_uv(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curve_interp_flat.strand_id).rgb;
}
float4 attr_load_color(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curve_interp_flat.strand_id).rgba;
}
float4 attr_load_vec4(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curves_attribute_element_id()).rgba;
}
float3 attr_load_vec3(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curves_attribute_element_id()).rgb;
}
float2 attr_load_vec2(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curves_attribute_element_id()).rg;
}
float attr_load_float(samplerBuffer cd_buf)
{
  return texelFetch(cd_buf, curves_attribute_element_id()).r;
}

/** \} */

#endif
