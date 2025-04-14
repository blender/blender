/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_info.hh"

#ifdef GPU_LIBRARY_SHADER
SHADER_LIBRARY_CREATE_INFO(draw_modelmat)
#endif

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_matrix_lib.glsl"
#include "gpu_shader_math_vector_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Mesh
 *
 * Mesh objects attributes are loaded using vertex input attributes.
 * \{ */

#ifdef OBINFO_LIB
float3 attr_load_orco(float4 orco)
{
#  ifdef GPU_VERTEX_SHADER
  /* We know when there is no orco layer when orco.w is 1.0 because it uses the generic vertex
   * attribute (which is [0,0,0,1]). */
  if (orco.w == 1.0f) {
    /* If the object does not have any deformation, the orco layer calculation is done on the fly
     * using the orco_madd factors. */
    return drw_object_orco(pos);
  }
#  endif
  return orco.xyz * 0.5f + 0.5f;
}
#endif
float4 attr_load_tangent(float4 tangent)
{
  tangent.xyz = safe_normalize(drw_normal_object_to_world(tangent.xyz));
  return tangent;
}
float4 attr_load_vec4(float4 attr)
{
  return attr;
}
float3 attr_load_vec3(float3 attr)
{
  return attr;
}
float2 attr_load_vec2(float2 attr)
{
  return attr;
}
float attr_load_float(float attr)
{
  return attr;
}
float4 attr_load_color(float4 attr)
{
  return attr;
}
float3 attr_load_uv(float3 attr)
{
  return attr;
}

/** \} */
