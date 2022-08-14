/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Surface Velocity
 *
 * Combined with the depth prepass shader.
 * Outputs the view motion vectors for animated objects.
 * \{ */

/* Pass world space deltas to the fragment shader.
 * This is to make sure that the resulting motion vectors are valid even with displacement. */
GPU_SHADER_INTERFACE_INFO(eevee_velocity_surface_iface, "motion")
    .smooth(Type::VEC3, "prev")
    .smooth(Type::VEC3, "next");

GPU_SHADER_CREATE_INFO(eevee_velocity_camera)
    .define("VELOCITY_CAMERA")
    .uniform_buf(1, "CameraData", "camera_prev")
    .uniform_buf(2, "CameraData", "camera_curr")
    .uniform_buf(3, "CameraData", "camera_next");

GPU_SHADER_CREATE_INFO(eevee_velocity_geom)
    .define("MAT_VELOCITY")
    .auto_resource_location(true)
    .storage_buf(4, Qualifier::READ, "mat4", "velocity_obj_prev_buf[]", Frequency::PASS)
    .storage_buf(5, Qualifier::READ, "mat4", "velocity_obj_next_buf[]", Frequency::PASS)
    .storage_buf(6, Qualifier::READ, "vec4", "velocity_geo_prev_buf[]", Frequency::PASS)
    .storage_buf(7, Qualifier::READ, "vec4", "velocity_geo_next_buf[]", Frequency::PASS)
    .storage_buf(
        7, Qualifier::READ, "VelocityIndex", "velocity_indirection_buf[]", Frequency::PASS)
    .vertex_out(eevee_velocity_surface_iface)
    .fragment_out(0, Type::VEC4, "out_velocity")
    .additional_info("eevee_velocity_camera");

/** \} */
