/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_test)
    .typedef_source("GPU_shader_shared.h")
    .fragment_out(0, Type::UVEC4, "out_test")
    .additional_info("draw_fullscreen");

GPU_SHADER_CREATE_INFO(gpu_math_test)
    .fragment_source("gpu_math_test.glsl")
    .additional_info("gpu_shader_test")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_compute_1d_test)
    .local_group_size(1)
    .image(1, GPU_RGBA32F, Qualifier::WRITE, ImageType::FLOAT_1D, "img_output")
    .compute_source("gpu_compute_1d_test.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_compute_2d_test)
    .local_group_size(1, 1)
    .image(1, GPU_RGBA32F, Qualifier::WRITE, ImageType::FLOAT_2D, "img_output")
    .compute_source("gpu_compute_2d_test.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_compute_ibo_test)
    .local_group_size(1)
    .storage_buf(0, Qualifier::WRITE, "uint", "out_indices[]")
    .compute_source("gpu_compute_ibo_test.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_compute_vbo_test)
    .local_group_size(1)
    .storage_buf(0, Qualifier::WRITE, "vec4", "out_positions[]")
    .compute_source("gpu_compute_vbo_test.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_compute_ssbo_test)
    .local_group_size(1)
    .storage_buf(0, Qualifier::WRITE, "int", "data_out[]")
    .compute_source("gpu_compute_ssbo_test.glsl")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_compute_ssbo_binding_test)
    .local_group_size(1)
    .storage_buf(0, Qualifier::WRITE, "int", "data0[]")
    .storage_buf(1, Qualifier::WRITE, "int", "data1[]")
    .compute_source("gpu_compute_dummy_test.glsl")
    .do_static_compilation(true);

/* Push constants. */
GPU_SHADER_CREATE_INFO(gpu_push_constants_base_test)
    .local_group_size(1)
    .storage_buf(0, Qualifier::WRITE, "float", "data_out[]")
    .compute_source("gpu_push_constants_test.glsl");

GPU_SHADER_CREATE_INFO(gpu_push_constants_test)
    .additional_info("gpu_push_constants_base_test")
    .push_constant(Type::FLOAT, "float_in")
    .push_constant(Type::VEC2, "vec2_in")
    .push_constant(Type::VEC3, "vec3_in")
    .push_constant(Type::VEC4, "vec4_in")
    .do_static_compilation(true);

/* Push constants size test. */
GPU_SHADER_CREATE_INFO(gpu_push_constants_128bytes_test)
    .additional_info("gpu_push_constants_test")
    .push_constant(Type::FLOAT, "filler", 20)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_push_constants_256bytes_test)
    .additional_info("gpu_push_constants_128bytes_test")
    .push_constant(Type::FLOAT, "filler2", 32)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_push_constants_512bytes_test)
    .additional_info("gpu_push_constants_256bytes_test")
    .push_constant(Type::FLOAT, "filler3", 64)
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_shadow_test)
    .fragment_source("eevee_shadow_test.glsl")
    .additional_info("gpu_shader_test")
    .additional_info("eevee_shared")
    .do_static_compilation(true);
