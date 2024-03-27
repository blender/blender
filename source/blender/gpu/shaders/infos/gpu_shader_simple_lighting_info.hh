/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(smooth_normal_iface, "").smooth(Type::VEC3, "normal");

GPU_SHADER_CREATE_INFO(gpu_shader_simple_lighting)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "nor")
    .vertex_out(smooth_normal_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .uniform_buf(0, "SimpleLightingData", "simple_lighting_data", Frequency::PASS)
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::MAT3, "NormalMatrix")
    .typedef_source("GPU_shader_shared.hh")
    .vertex_source("gpu_shader_3D_normal_vert.glsl")
    .fragment_source("gpu_shader_simple_lighting_frag.glsl")
    .do_static_compilation(true);
