/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_curves_lib.glsl"
#include "eevee_geom_types_lib.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Curve
 *
 * Curve objects loads attributes from buffers through sampler buffers.
 * Per attribute scope follows loading order.
 * \{ */

float3 attr_load_orco(CurvesPoint point, float4 /*orco*/, int /*index*/)
{
  /* NOTE: Doesn't support ORCO attribute. */
  return point.orco_default;
}

/* Return the index to use for looking up the attribute value in the sampler
 * based on the attribute scope (point or spline). */
int curves_attribute_element_id(CurvesPoint point, int index)
{
  const auto &curves_buf = buffer_get(draw_curves_infos, drw_curves);
  if (curves_buf.is_point_attribute[index][0] != 0u) {
    return point.point_id;
  }
  return point.curve_id;
}

float4 attr_load_tangent(CurvesPoint /*point*/, samplerBuffer /*cd_buf*/, int /*index*/)
{
  /* Not supported for the moment. */
  return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
float3 attr_load_uv(CurvesPoint point, samplerBuffer cd_buf, int /*index*/)
{
  return texelFetch(cd_buf, point.curve_id).rgb;
}
float4 attr_load_color(CurvesPoint point, samplerBuffer cd_buf, int /*index*/)
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
