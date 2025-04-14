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
/** \name Grease Pencil
 *
 * Grease Pencil objects have one uv and one color attribute layer.
 * \{ */

/* Globals to feed the load functions. */
packed_float2 g_uvs;
packed_float4 g_color;

#ifdef OBINFO_LIB
float3 attr_load_orco(float4 orco)
{
  float3 lP = drw_point_world_to_object(interp.P);
  return drw_object_orco(lP);
}
#endif
float4 attr_load_tangent(float4 tangent)
{
  return float4(0.0f, 0.0f, 0.0f, 1.0f);
}
float3 attr_load_uv(float3 dummy)
{
  return float3(g_uvs, 0.0f);
}
float4 attr_load_color(float4 dummy)
{
  return g_color;
}
float4 attr_load_vec4(float4 attr)
{
  return float4(0.0f);
}
float3 attr_load_vec3(float3 attr)
{
  return float3(0.0f);
}
float2 attr_load_vec2(float2 attr)
{
  return float2(0.0f);
}
float attr_load_float(float attr)
{
  return 0.0f;
}

/** \} */
