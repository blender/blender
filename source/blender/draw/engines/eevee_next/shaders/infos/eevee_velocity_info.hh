
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
    .fragment_out(0, Type::VEC4, "out_velocity_view")
    .additional_info("eevee_velocity_camera");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Velocity Resolve
 *
 * Computes velocity for static objects.
 * Also converts motion to camera space (as opposed to view space) if needed.
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_velocity_resolve)
    .do_static_compilation(true)
    .local_group_size(8, 8)
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .image(0, GPU_RG16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "velocity_view_img")
    .image(1, GPU_RG16F, Qualifier::WRITE, ImageType::FLOAT_2D, "velocity_camera_img")
    .additional_info("eevee_shared")
    .compute_source("eevee_velocity_resolve_comp.glsl")
    .additional_info("draw_view", "eevee_velocity_camera");

/** \} */
