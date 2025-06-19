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

/* All attributes are loaded in order. This allow us to use a global counter to retrieve the
 * correct grid xform. */
/* TODO(fclem): This is very dangerous as it requires a reset for each time `attrib_load` is
 * called. Instead, the right attribute index should be passed to attr_load_* functions. */
int g_attr_id = 0;

/* Point clouds and curves are not compatible with volume grids.
 * They will fall back to their own attributes loading. */
#if defined(MAT_VOLUME) && !defined(MAT_GEOM_CURVES) && !defined(MAT_GEOM_POINTCLOUD)
#  if defined(VOLUME_INFO_LIB) && !defined(MAT_GEOM_WORLD)
#    define GRID_ATTRIBUTES
#  endif

/* -------------------------------------------------------------------- */
/** \name Volume
 *
 * Volume objects loads attributes from "grids" in the form of 3D textures.
 * Per grid transform order is following loading order.
 * \{ */

#  ifdef GRID_ATTRIBUTES
float3 g_lP = float3(0.0f);
#  else
float3 g_wP = float3(0.0f);
#  endif

float3 grid_coordinates()
{
#  ifdef GRID_ATTRIBUTES
  float3 co = (drw_volume.grids_xform[g_attr_id] * float4(g_lP, 1.0f)).xyz;
#  else
  /* Only for test shaders. All the runtime shaders require `draw_object_infos` and
   * `draw_volume_infos`. */
  float3 co = float3(0.0f);
#  endif
  g_attr_id += 1;
  return co;
}

float3 attr_load_orco(sampler3D tex)
{
  g_attr_id += 1;
#  ifdef GRID_ATTRIBUTES
  return drw_object_orco(g_lP);
#  else
  return g_wP;
#  endif
}
float4 attr_load_tangent(sampler3D tex)
{
  g_attr_id += 1;
  return float4(0);
}
float4 attr_load_vec4(sampler3D tex)
{
  return texture(tex, grid_coordinates());
}
float3 attr_load_vec3(sampler3D tex)
{
  return texture(tex, grid_coordinates()).rgb;
}
float2 attr_load_vec2(sampler3D tex)
{
  return texture(tex, grid_coordinates()).rg;
}
float attr_load_float(sampler3D tex)
{
  return texture(tex, grid_coordinates()).r;
}
float4 attr_load_color(sampler3D tex)
{
  return texture(tex, grid_coordinates());
}
float3 attr_load_uv(sampler3D attr)
{
  g_attr_id += 1;
  return float3(0);
}

/** \} */
#endif
