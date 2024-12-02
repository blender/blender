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
/** \name Mesh
 *
 * Mesh objects attributes are loaded using vertex input attributes.
 * \{ */

#ifdef OBINFO_LIB
vec3 attr_load_orco(vec4 orco)
{
#  ifdef GPU_VERTEX_SHADER
  /* We know when there is no orco layer when orco.w is 1.0 because it uses the generic vertex
   * attribute (which is [0,0,0,1]). */
  if (orco.w == 1.0) {
    /* If the object does not have any deformation, the orco layer calculation is done on the fly
     * using the orco_madd factors. */
    return OrcoTexCoFactors[0].xyz + pos * OrcoTexCoFactors[1].xyz;
  }
#  endif
  return orco.xyz * 0.5 + 0.5;
}
#endif
vec4 attr_load_tangent(vec4 tangent)
{
  tangent.xyz = safe_normalize(drw_normal_object_to_world(tangent.xyz));
  return tangent;
}
vec4 attr_load_vec4(vec4 attr)
{
  return attr;
}
vec3 attr_load_vec3(vec3 attr)
{
  return attr;
}
vec2 attr_load_vec2(vec2 attr)
{
  return attr;
}
float attr_load_float(float attr)
{
  return attr;
}
vec4 attr_load_color(vec4 attr)
{
  return attr;
}
vec3 attr_load_uv(vec3 attr)
{
  return attr;
}

/** \} */
