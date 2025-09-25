/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(depth_2d_update_iface).smooth(Type::float2_t, "texCoord_interp");

GPU_SHADER_CREATE_INFO(depth_2d_update_info_base)
    .vertex_in(0, Type::float2_t, "pos")
    .vertex_out(depth_2d_update_iface)
    .fragment_out(0, Type::float4_t, "fragColor")
    .push_constant(Type::float2_t, "extent")
    .push_constant(Type::float2_t, "offset")
    .push_constant(Type::float2_t, "size")
    .push_constant(Type::int_t, "mip")
    .depth_write(DepthWrite::ANY)
    .vertex_source("depth_2d_update_vert.glsl");

GPU_SHADER_CREATE_INFO(depth_2d_update_float)
    .metal_backend_only(true)
    .fragment_source("depth_2d_update_float_frag.glsl")
    .sampler(0, ImageType::Float2D, "source_data", Frequency::PASS)
    .additional_info("depth_2d_update_info_base")
    .do_static_compilation(true)
    .depth_write(DepthWrite::ANY);

GPU_SHADER_CREATE_INFO(depth_2d_update_int24)
    .metal_backend_only(true)
    .fragment_source("depth_2d_update_int24_frag.glsl")
    .additional_info("depth_2d_update_info_base")
    .sampler(0, ImageType::Int2D, "source_data", Frequency::PASS)
    .do_static_compilation(true)
    .depth_write(DepthWrite::ANY);

GPU_SHADER_CREATE_INFO(depth_2d_update_int32)
    .metal_backend_only(true)
    .fragment_source("depth_2d_update_int32_frag.glsl")
    .additional_info("depth_2d_update_info_base")
    .sampler(0, ImageType::Int2D, "source_data", Frequency::PASS)
    .do_static_compilation(true)
    .depth_write(DepthWrite::ANY);
