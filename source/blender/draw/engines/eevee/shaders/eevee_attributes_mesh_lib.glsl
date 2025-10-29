/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_infos.hh"

#ifdef GPU_LIBRARY_SHADER
SHADER_LIBRARY_CREATE_INFO(draw_modelmat)
#endif

#include "draw_model_lib.glsl"
#include "draw_object_infos_lib.glsl"
#include "eevee_geom_types_lib.glsl"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Mesh
 *
 * Mesh objects attributes are loaded using vertex input attributes.
 * \{ */

#ifdef OBINFO_LIB
float3 attr_load_orco(MeshVertex vert, float4 orco, int index)
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
float4 attr_load_tangent(MeshVertex vert, float4 tangent, int index)
{
  tangent.xyz = safe_normalize(drw_normal_object_to_world(tangent.xyz));
  return tangent;
}
float4 attr_load_float4(MeshVertex vert, float4 attr, int index)
{
  return attr;
}
float3 attr_load_float3(MeshVertex vert, float3 attr, int index)
{
  return attr;
}
float2 attr_load_float2(MeshVertex vert, float2 attr, int index)
{
  return attr;
}
float attr_load_float(MeshVertex vert, float attr, int index)
{
  return attr;
}
float4 attr_load_color(MeshVertex vert, float4 attr, int index)
{
  return attr;
}
float3 attr_load_uv(MeshVertex vert, float3 attr, int index)
{
  return attr;
}

/** \} */
