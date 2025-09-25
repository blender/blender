/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "eevee_camera_shared.hh"
#  include "eevee_velocity_shared.hh"

#  define VELOCITY_CAMERA
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Surface Velocity
 *
 * Combined with the depth pre-pass shader.
 * Outputs the view motion vectors for animated objects.
 * \{ */

/* Pass world space deltas to the fragment shader.
 * This is to make sure that the resulting motion vectors are valid even with displacement.
 * WARNING: The next value is invalid when rendering the viewport. */
GPU_SHADER_NAMED_INTERFACE_INFO(eevee_velocity_surface_iface, motion)
SMOOTH(float3, prev)
SMOOTH(float3, next)
GPU_SHADER_NAMED_INTERFACE_END(motion)

GPU_SHADER_CREATE_INFO(eevee_velocity_camera)
DEFINE("VELOCITY_CAMERA")
TYPEDEF_SOURCE("eevee_velocity_shared.hh")
TYPEDEF_SOURCE("eevee_camera_shared.hh")
UNIFORM_BUF(VELOCITY_CAMERA_PREV_BUF, CameraData, camera_prev)
UNIFORM_BUF(VELOCITY_CAMERA_CURR_BUF, CameraData, camera_curr)
UNIFORM_BUF(VELOCITY_CAMERA_NEXT_BUF, CameraData, camera_next)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_velocity_geom)
DEFINE("MAT_VELOCITY")
TYPEDEF_SOURCE("eevee_velocity_shared.hh")
STORAGE_BUF(VELOCITY_OBJ_PREV_BUF_SLOT, read, float4x4, velocity_obj_prev_buf[])
STORAGE_BUF(VELOCITY_OBJ_NEXT_BUF_SLOT, read, float4x4, velocity_obj_next_buf[])
STORAGE_BUF(VELOCITY_GEO_PREV_BUF_SLOT, read, float4, velocity_geo_prev_buf[])
STORAGE_BUF(VELOCITY_GEO_NEXT_BUF_SLOT, read, float4, velocity_geo_next_buf[])
STORAGE_BUF(VELOCITY_INDIRECTION_BUF_SLOT, read, VelocityIndex, velocity_indirection_buf[])
VERTEX_OUT(eevee_velocity_surface_iface)
FRAGMENT_OUT(0, float4, out_velocity)
ADDITIONAL_INFO(eevee_velocity_camera)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_vertex_copy)
COMPUTE_SOURCE("eevee_vertex_copy_comp.glsl")
LOCAL_GROUP_SIZE(VERTEX_COPY_GROUP_SIZE)
STORAGE_BUF(0, read, float, in_buf[])
STORAGE_BUF(1, write, float4, out_buf[])
PUSH_CONSTANT(int, start_offset)
PUSH_CONSTANT(int, vertex_stride)
PUSH_CONSTANT(int, vertex_count)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */
