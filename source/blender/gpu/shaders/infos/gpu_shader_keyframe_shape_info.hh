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

GPU_SHADER_INTERFACE_INFO(keyframe_shape_iface, "")
    .flat(Type::VEC4, "finalColor")
    .flat(Type::VEC4, "finalOutlineColor")
    .flat(Type::VEC4, "radii")
    .flat(Type::VEC4, "thresholds")
    .flat(Type::INT, "finalFlags");

GPU_SHADER_CREATE_INFO(gpu_shader_keyframe_shape)
    .vertex_in(0, Type::VEC4, "color")
    .vertex_in(1, Type::VEC4, "outlineColor")
    .vertex_in(2, Type::VEC2, "pos")
    .vertex_in(3, Type::FLOAT, "size")
    .vertex_in(4, Type ::INT, "flags")
    .vertex_out(keyframe_shape_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(0, Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(16, Type::VEC2, "ViewportSize")
    .push_constant(24, Type::FLOAT, "outline_scale")
    .vertex_source("gpu_shader_keyframe_shape_vert.glsl")
    .fragment_source("gpu_shader_keyframe_shape_frag.glsl")
    .do_static_compilation(true);
