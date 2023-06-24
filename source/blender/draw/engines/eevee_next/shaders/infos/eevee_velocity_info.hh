/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Surface Velocity
 *
 * Combined with the depth pre-pass shader.
 * Outputs the view motion vectors for animated objects.
 * \{ */

/* Pass world space deltas to the fragment shader.
 * This is to make sure that the resulting motion vectors are valid even with displacement. */
GPU_SHADER_INTERFACE_INFO(eevee_velocity_surface_iface, "motion")
    .smooth(Type::VEC3, "prev")
    .smooth(Type::VEC3, "next");

GPU_SHADER_CREATE_INFO(eevee_velocity_camera)
    .define("VELOCITY_CAMERA")
    .uniform_buf(VELOCITY_CAMERA_PREV_BUF, "CameraData", "camera_prev")
    .uniform_buf(VELOCITY_CAMERA_CURR_BUF, "CameraData", "camera_curr")
    .uniform_buf(VELOCITY_CAMERA_NEXT_BUF, "CameraData", "camera_next");

GPU_SHADER_CREATE_INFO(eevee_velocity_geom)
    .define("MAT_VELOCITY")
    .storage_buf(VELOCITY_OBJ_PREV_BUF_SLOT, Qualifier::READ, "mat4", "velocity_obj_prev_buf[]")
    .storage_buf(VELOCITY_OBJ_NEXT_BUF_SLOT, Qualifier::READ, "mat4", "velocity_obj_next_buf[]")
    .storage_buf(VELOCITY_GEO_PREV_BUF_SLOT, Qualifier::READ, "vec4", "velocity_geo_prev_buf[]")
    .storage_buf(VELOCITY_GEO_NEXT_BUF_SLOT, Qualifier::READ, "vec4", "velocity_geo_next_buf[]")
    .storage_buf(VELOCITY_INDIRECTION_BUF_SLOT,
                 Qualifier::READ,
                 "VelocityIndex",
                 "velocity_indirection_buf[]")
    .vertex_out(eevee_velocity_surface_iface)
    .fragment_out(0, Type::VEC4, "out_velocity")
    .additional_info("eevee_velocity_camera");

/** \} */
