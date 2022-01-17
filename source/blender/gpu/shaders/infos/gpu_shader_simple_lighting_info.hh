
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2022 Blender Foundation.
 * All rights reserved.
 */

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
    .push_constant(0, Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(16, Type::MAT3, "NormalMatrix")
    .typedef_source("GPU_shader_shared.h")
    .vertex_source("gpu_shader_3D_normal_vert.glsl")
    .fragment_source("gpu_shader_simple_lighting_frag.glsl")
    .do_static_compilation(true);
