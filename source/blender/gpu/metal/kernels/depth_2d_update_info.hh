/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(depth_2d_update_iface, "").smooth(Type::VEC2, "texCoord_interp");

GPU_SHADER_CREATE_INFO(depth_2d_update_info_base)
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_out(depth_2d_update_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::VEC2, "extent")
    .push_constant(Type::VEC2, "offset")
    .push_constant(Type::VEC2, "size")
    .push_constant(Type::INT, "mip")
    .sampler(0, ImageType::FLOAT_2D, "source_data", Frequency::PASS)
    .depth_write(DepthWrite::ANY)
    .vertex_source("depth_2d_update_vert.glsl");

GPU_SHADER_CREATE_INFO(depth_2d_update_float)
    .metal_backend_only(true)
    .fragment_source("depth_2d_update_float_frag.glsl")
    .additional_info("depth_2d_update_info_base")
    .do_static_compilation(true)
    .depth_write(DepthWrite::ANY);

GPU_SHADER_CREATE_INFO(depth_2d_update_int24)
    .metal_backend_only(true)
    .fragment_source("depth_2d_update_int24_frag.glsl")
    .additional_info("depth_2d_update_info_base")
    .do_static_compilation(true)
    .depth_write(DepthWrite::ANY);

GPU_SHADER_CREATE_INFO(depth_2d_update_int32)
    .metal_backend_only(true)
    .fragment_source("depth_2d_update_int32_frag.glsl")
    .additional_info("depth_2d_update_info_base")
    .do_static_compilation(true)
    .depth_write(DepthWrite::ANY);
