/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_geom_types_lib.bsl.hh"
#include "gpu_shader_codegen_lib.glsl"
#include "gpu_shader_math_vector_safe_lib.glsl"

/* -------------------------------------------------------------------- */
/** \name Mesh
 *
 * Mesh objects attributes are loaded using vertex input attributes.
 * \{ */

float3 attr_load_orco(MeshVertex vert, float4 orco, int /*index*/)
{
  /* We know when there is no orco layer when orco.w is 1.0 because it uses the generic vertex
   * attribute (which is [0,0,0,1]). */
  if (orco.w == 1.0f) {
    /* If the object does not have any deformation, the orco layer calculation is done on the fly
     * using the orco_madd factors. */
    return vert.orco_default;
  }
  return orco.xyz * 0.5f + 0.5f;
}

float4 attr_load_tangent(MeshVertex vert, float4 tangent, int /*index*/)
{
  /* Same as normal_object_to_world. */
  tangent.xyz = safe_normalize(tangent.xyz * vert.world_to_object);
  return tangent;
}
float4 attr_load_float4(MeshVertex /*vert*/, float4 attr, int /*index*/)
{
  return attr;
}
float3 attr_load_float3(MeshVertex /*vert*/, float3 attr, int /*index*/)
{
  return attr;
}
float2 attr_load_float2(MeshVertex /*vert*/, float2 attr, int /*index*/)
{
  return attr;
}
float attr_load_float(MeshVertex /*vert*/, float attr, int /*index*/)
{
  return attr;
}
float4 attr_load_color(MeshVertex /*vert*/, float4 attr, int /*index*/)
{
  return attr;
}
float3 attr_load_uv(MeshVertex /*vert*/, float3 attr, int /*index*/)
{
  return attr;
}

/** \} */
