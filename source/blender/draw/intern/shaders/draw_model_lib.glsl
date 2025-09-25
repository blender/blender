/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_view_infos.hh"

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
#  ifdef WITH_CUSTOM_IDS
  uint id = resource_id_buf[gpu_BaseInstance + gl_InstanceID].x;
#  else
  uint id = resource_id_buf[gpu_BaseInstance + gl_InstanceID];
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
  uint inst_id = gpu_BaseInstance + gl_InstanceID;
  return resource_id_buf[gpu_BaseInstance + gl_InstanceID].y;
#  endif
#endif
  return 0;
}

float4x4 drw_modelmat()
{
  return drw_matrix_buf[drw_resource_id()].model;
}
float4x4 drw_modelinv()
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
float3x3 drw_normat()
{
  return transpose(to_float3x3(drw_modelinv()));
}
float3x3 drw_norinv()
{
  return transpose(to_float3x3(drw_modelmat()));
}

/* -------------------------------------------------------------------- */
/** \name Transform Normal
 *
 * Space conversion helpers for normal vectors.
 * \{ */

float3 drw_normal_object_to_world(float3 lN)
{
  return (drw_normat() * lN);
}
float3 drw_normal_world_to_object(float3 N)
{
  return (drw_norinv() * N);
}

float3 drw_normal_object_to_view(float3 lN)
{
  return (to_float3x3(drw_view().viewmat) * (drw_normat() * lN));
}
float3 drw_normal_view_to_object(float3 vN)
{
  return (drw_norinv() * (to_float3x3(drw_view().viewinv) * vN));
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Transform Normal
 *
 * Space conversion helpers for points (coordinates).
 * \{ */

float3 drw_point_object_to_world(float3 lP)
{
  return (drw_modelmat() * float4(lP, 1.0f)).xyz;
}
float3 drw_point_world_to_object(float3 P)
{
  return (drw_modelinv() * float4(P, 1.0f)).xyz;
}

float3 drw_point_object_to_view(float3 lP)
{
  return (drw_view().viewmat * (drw_modelmat() * float4(lP, 1.0f))).xyz;
}
float3 drw_point_view_to_object(float3 vP)
{
  return (drw_modelinv() * (drw_view().viewinv * float4(vP, 1.0f))).xyz;
}

float4 drw_point_object_to_homogenous(float3 lP)
{
  return (drw_view().winmat * (drw_view().viewmat * (drw_modelmat() * float4(lP, 1.0f))));
}
float3 drw_point_object_to_ndc(float3 lP)
{
  return drw_perspective_divide(drw_point_object_to_homogenous(lP));
}

/** \} */
