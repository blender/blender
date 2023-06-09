/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* EEVEE_shaders_effect_motion_blur_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_effect_motion_blur)
    .additional_info("eevee_legacy_common_lib")
    .additional_info("eevee_legacy_common_utiltex_lib")
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_defines_info")
    .fragment_source("effect_motion_blur_frag.glsl")
    .sampler(0, ImageType::FLOAT_2D, "colorBuffer")
    .sampler(1, ImageType::DEPTH_2D, "depthBuffer")
    .sampler(2, ImageType::FLOAT_2D, "velocityBuffer")
    .sampler(3, ImageType::FLOAT_2D, "tileMaxBuffer")
    .push_constant(Type::FLOAT, "depthScale")
    .push_constant(Type::IVEC2, "tileBufferSize")
    .push_constant(Type::VEC2, "viewportSize")
    .push_constant(Type::VEC2, "viewportSizeInv")
    .push_constant(Type::BOOL, "isPerspective")
    .push_constant(Type::VEC2, "nearFar")
    .fragment_out(0, Type::VEC4, "fragColor")
    .auto_resource_location(true)
    .do_static_compilation(true);

/* EEVEE_shaders_effect_motion_blur_object_sh_get */
GPU_SHADER_INTERFACE_INFO(eevee_legacy_motion_object_iface, "")
    .smooth(Type::VEC3, "currWorldPos")
    .smooth(Type::VEC3, "prevWorldPos")
    .smooth(Type::VEC3, "nextWorldPos");

GPU_SHADER_CREATE_INFO(eevee_legacy_effect_motion_blur_object_common)
    .additional_info("eevee_legacy_common_lib")
    .additional_info("draw_view")
    .vertex_source("object_motion_vert.glsl")
    .fragment_source("object_motion_frag.glsl")
    .vertex_out(eevee_legacy_motion_object_iface)
    .fragment_out(0, Type::VEC4, "outData")
    .push_constant(Type::MAT4, "currModelMatrix")
    .push_constant(Type::MAT4, "prevModelMatrix")
    .push_constant(Type::MAT4, "nextModelMatrix")
    .push_constant(Type::MAT4, "prevViewProjMatrix")
    .push_constant(Type::MAT4, "currViewProjMatrix")
    .push_constant(Type::MAT4, "nextViewProjMatrix")
    .push_constant(Type::BOOL, "useDeform");

GPU_SHADER_CREATE_INFO(eevee_legacy_effect_motion_blur_object_hair)
    .define("HAIR")
    .define("HAIR_SHADER")
    .additional_info("eevee_legacy_hair_lib")
    .additional_info("eevee_legacy_effect_motion_blur_object_common")
    .sampler(0, ImageType::FLOAT_BUFFER, "prvBuffer")
    .sampler(1, ImageType::FLOAT_BUFFER, "nxtBuffer")
    .do_static_compilation(true)
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_effect_motion_blur_object)
    .additional_info("eevee_legacy_effect_motion_blur_object_common")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "prv")
    .vertex_in(2, Type::VEC3, "nxt")
    .do_static_compilation(true)
    .auto_resource_location(true);

/* EEVEE_shaders_effect_motion_blur_velocity_tiles_sh_get */
GPU_SHADER_CREATE_INFO(eevee_legacy_effect_motion_blur_velocity_tiles_common)
    .additional_info("draw_fullscreen")
    .additional_info("eevee_legacy_defines_info")
    .fragment_source("effect_velocity_tile_frag.glsl")
    .sampler(0, ImageType::FLOAT_2D, "velocityBuffer")
    .push_constant(Type::VEC2, "viewportSize")
    .push_constant(Type::VEC2, "viewportSizeInv")
    .push_constant(Type::IVEC2, "velocityBufferSize")
    .fragment_out(0, Type::VEC4, "tileMaxVelocity")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_effect_motion_blur_velocity_tiles_GATHER)
    .define("TILE_GATHER")
    .additional_info("eevee_legacy_effect_motion_blur_velocity_tiles_common")
    .push_constant(Type::IVEC2, "gatherStep")
    .do_static_compilation(true)
    .auto_resource_location(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_effect_motion_blur_velocity_tiles_EXPANSION)
    .define("TILE_EXPANSION")
    .additional_info("eevee_legacy_effect_motion_blur_velocity_tiles_common")
    .do_static_compilation(true)
    .auto_resource_location(true);
