/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(draw_view_lib.glsl)

#ifndef DRAW_MODELMAT_CREATE_INFO
#  error Missing draw_modelmat additional create info on shader create info
#endif

#if defined(UNIFORM_RESOURCE_ID)
/* TODO(fclem): Legacy API. To remove. */
#  define resource_id drw_ResourceID
#  define DRW_RESOURCE_ID_VARYING_SET

#elif defined(GPU_VERTEX_SHADER)
#  if defined(UNIFORM_RESOURCE_ID_NEW)
#    define resource_id (drw_ResourceID >> DRW_VIEW_SHIFT)
#  else
#    define resource_id gpu_InstanceIndex
#  endif
#  define DRW_RESOURCE_ID_VARYING_SET drw_ResourceID_iface.resource_index = resource_id;

#elif defined(GPU_GEOMETRY_SHADER)
#  define resource_id drw_ResourceID_iface_in[0].resource_index

#elif defined(GPU_FRAGMENT_SHADER)
#  define resource_id drw_ResourceID_iface.resource_index
#endif

mat4x4 drw_modelmat()
{
  return drw_matrix_buf[resource_id].model;
}
mat4x4 drw_modelinv()
{
  return drw_matrix_buf[resource_id].model_inverse;
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
  return transpose(mat3x3(drw_modelinv()));
}
mat3x3 drw_norinv()
{
  return transpose(mat3x3(drw_modelmat()));
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
  return (mat3x3(drw_view.viewmat) * (drw_normat() * lN));
}
vec3 drw_normal_view_to_object(vec3 vN)
{
  return (drw_norinv() * (mat3x3(drw_view.viewinv) * vN));
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
  return (drw_view.viewmat * (drw_modelmat() * vec4(lP, 1.0))).xyz;
}
vec3 drw_point_view_to_object(vec3 vP)
{
  return (drw_modelinv() * (drw_view.viewinv * vec4(vP, 1.0))).xyz;
}

vec4 drw_point_object_to_homogenous(vec3 lP)
{
  return (drw_view.winmat * (drw_view.viewmat * (drw_modelmat() * vec4(lP, 1.0))));
}
vec3 drw_point_object_to_ndc(vec3 lP)
{
  return drw_perspective_divide(drw_point_object_to_homogenous(lP));
}

/** \} */
