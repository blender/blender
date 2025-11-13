/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_infos.hh"

#ifdef GPU_LIBRARY_SHADER
#  define CURVES_SHADER
#  define DRW_HAIR_INFO
#endif

SHADER_LIBRARY_CREATE_INFO(draw_modelmat)
SHADER_LIBRARY_CREATE_INFO(draw_curves)

#include "draw_curves_lib.glsl"
#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "eevee_geom_types_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"

#include "gpu_shader_math_vector_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Curve
 *
 * Curve objects loads attributes from buffers through sampler buffers.
 * Per attribute scope follows loading order.
 * \{ */

#ifdef OBINFO_LIB
float3 attr_load_orco(CurvesPoint point, float4 orco, int index)
{
  float3 lP = curves::get_curve_root_pos(point.point_id, point.curve_segment);
  return drw_object_orco(lP);
}
#endif

/* Return the index to use for looking up the attribute value in the sampler
 * based on the attribute scope (point or spline). */
int curves_attribute_element_id(CurvesPoint point, int index)
{
  if (drw_curves.is_point_attribute[index][0] != 0u) {
    return int(point.point_id);
  }
  return point.curve_id;
}

float4 attr_load_tangent(CurvesPoint point, samplerBuffer cd_buf, int index)
{
  /* Not supported for the moment. */
  return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
float3 attr_load_uv(CurvesPoint point, samplerBuffer cd_buf, int index)
{
  return texelFetch(cd_buf, point.curve_id).rgb;
}
float4 attr_load_color(CurvesPoint point, samplerBuffer cd_buf, int index)
{
  return texelFetch(cd_buf, point.curve_id).rgba;
}
float4 attr_load_float4(CurvesPoint point, samplerBuffer cd_buf, int index)
{
  return texelFetch(cd_buf, curves_attribute_element_id(point, index)).rgba;
}
float3 attr_load_float3(CurvesPoint point, samplerBuffer cd_buf, int index)
{
  return texelFetch(cd_buf, curves_attribute_element_id(point, index)).rgb;
}
float2 attr_load_float2(CurvesPoint point, samplerBuffer cd_buf, int index)
{
  return texelFetch(cd_buf, curves_attribute_element_id(point, index)).rg;
}
float attr_load_float(CurvesPoint point, samplerBuffer cd_buf, int index)
{
  return texelFetch(cd_buf, curves_attribute_element_id(point, index)).r;
}

/** \} */
