/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view_info.hh"

#include "draw_view_lib.glsl"

#if !defined(DRAW_MODELMAT_CREATE_INFO) && !defined(GLSL_CPP_STUBS)
#  error Missing draw_modelmat additional create info on shader create info
#endif

#if defined(GPU_VERTEX_SHADER)
VERTEX_SHADER_CREATE_INFO(draw_resource_id_varying)
#elif defined(GPU_FRAGMENT_SHADER)
FRAGMENT_SHADER_CREATE_INFO(draw_resource_id_varying)
#elif defined(GPU_LIBRARY_SHADER)
SHADER_LIBRARY_CREATE_INFO(draw_resource_id_varying)
#endif

uint drw_resource_id_raw()
{
#if defined(GPU_VERTEX_SHADER)
#  if defined(RESOURCE_ID_FALLBACK)
#    ifdef WITH_CUSTOM_IDS
  uint id = in_resource_id.x;
#    else
  uint id = in_resource_id;
#    endif
#  else
#    ifdef WITH_CUSTOM_IDS
  uint id = resource_id_buf[gpu_BaseInstance + gl_InstanceID].x;
#    else
  uint id = resource_id_buf[gpu_BaseInstance + gl_InstanceID];
#    endif
#  endif
  return id;

#elif defined(GPU_FRAGMENT_SHADER) || defined(GPU_LIBRARY_SHADER)
  return drw_ResourceID_iface.resource_index;
#endif
  return 0;
}

uint drw_resource_id()
{
  return drw_resource_id_raw() >> DRW_VIEW_SHIFT;
}

uint drw_custom_id()
{
#ifdef WITH_CUSTOM_IDS
#  if defined(GPU_VERTEX_SHADER)
#    if defined(RESOURCE_ID_FALLBACK)
  return in_resource_id.y;
#    else
  uint inst_id = gpu_BaseInstance + gl_InstanceID;
  return resource_id_buf[gpu_BaseInstance + gl_InstanceID].y;
#    endif
#  endif
#endif
  return 0;
}

mat4x4 drw_modelmat()
{
  return drw_matrix_buf[drw_resource_id()].model;
}
mat4x4 drw_modelinv()
{
  return drw_matrix_buf[drw_resource_id()].model_inverse;
}

/**
 * Usually Normal matrix is `transpose(inverse(ViewMatrix * ModelMatrix))`.
 *
 * But since it is slow to multiply matrices we decompose it. Decomposing
 * inversion and transposition both invert the product order leaving us with
 * the same original order:
 * transpose(ViewMatrixInverse) * transpose(ModelMatrixInverse)
 *
 * Knowing that the view matrix is orthogonal, the transpose is also the inverse.
 * NOTE: This is only valid because we are only using the mat3 of the ViewMatrixInverse.
 * ViewMatrix * transpose(ModelMatrixInverse)
 */
mat3x3 drw_normat()
{
  return transpose(to_float3x3(drw_modelinv()));
}
mat3x3 drw_norinv()
{
  return transpose(to_float3x3(drw_modelmat()));
}

/* -------------------------------------------------------------------- */
/** \name Transform Normal
 *
 * Space conversion helpers for normal vectors.
 * \{ */

vec3 drw_normal_object_to_world(vec3 lN)
{
  return (drw_normat() * lN);
}
vec3 drw_normal_world_to_object(vec3 N)
{
  return (drw_norinv() * N);
}

vec3 drw_normal_object_to_view(vec3 lN)
{
  return (to_float3x3(drw_view().viewmat) * (drw_normat() * lN));
}
vec3 drw_normal_view_to_object(vec3 vN)
{
  return (drw_norinv() * (to_float3x3(drw_view().viewinv) * vN));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Normal
 *
 * Space conversion helpers for points (coordinates).
 * \{ */

vec3 drw_point_object_to_world(vec3 lP)
{
  return (drw_modelmat() * vec4(lP, 1.0)).xyz;
}
vec3 drw_point_world_to_object(vec3 P)
{
  return (drw_modelinv() * vec4(P, 1.0)).xyz;
}

vec3 drw_point_object_to_view(vec3 lP)
{
  return (drw_view().viewmat * (drw_modelmat() * vec4(lP, 1.0))).xyz;
}
vec3 drw_point_view_to_object(vec3 vP)
{
  return (drw_modelinv() * (drw_view().viewinv * vec4(vP, 1.0))).xyz;
}

vec4 drw_point_object_to_homogenous(vec3 lP)
{
  return (drw_view().winmat * (drw_view().viewmat * (drw_modelmat() * vec4(lP, 1.0))));
}
vec3 drw_point_object_to_ndc(vec3 lP)
{
  return drw_perspective_divide(drw_point_object_to_homogenous(lP));
}

/** \} */
