/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_object_infos_info.hh"

#ifdef GPU_LIBRARY_SHADER
SHADER_LIBRARY_CREATE_INFO(draw_modelmat_new)
#endif

#include "draw_model_lib.glsl"
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
vec3 attr_load_orco(vec4 orco)
{
  vec3 lP = drw_point_world_to_object(interp.P);
  return OrcoTexCoFactors[0].xyz + lP * OrcoTexCoFactors[1].xyz;
}
#endif
vec4 attr_load_tangent(vec4 tangent)
{
  return vec4(0.0, 0.0, 0.0, 1.0);
}
vec3 attr_load_uv(vec3 dummy)
{
  return vec3(g_uvs, 0.0);
}
vec4 attr_load_color(vec4 dummy)
{
  return g_color;
}
vec4 attr_load_vec4(vec4 attr)
{
  return vec4(0.0);
}
vec3 attr_load_vec3(vec3 attr)
{
  return vec3(0.0);
}
vec2 attr_load_vec2(vec2 attr)
{
  return vec2(0.0);
}
float attr_load_float(float attr)
{
  return 0.0;
}

/** \} */
