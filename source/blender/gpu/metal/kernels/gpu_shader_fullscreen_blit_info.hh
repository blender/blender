/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(fullscreen_blit_iface).smooth(Type::float2_t, "screen_uv");

GPU_SHADER_CREATE_INFO(fullscreen_blit)
    .vertex_in(0, Type::float2_t, "pos")
    .vertex_out(fullscreen_blit_iface)
    .fragment_out(0, Type::float4_t, "fragColor")
    .push_constant(Type::float2_t, "fullscreen")
    .push_constant(Type::float2_t, "size")
    .push_constant(Type::float2_t, "dst_offset")
    .push_constant(Type::float2_t, "src_offset")
    .push_constant(Type::int_t, "mip")
    .sampler(0, ImageType::Float2D, "imageTexture", Frequency::PASS)
    .vertex_source("gpu_shader_fullscreen_blit_vert.glsl")
    .fragment_source("gpu_shader_fullscreen_blit_frag.glsl")
    .do_static_compilation(true);
