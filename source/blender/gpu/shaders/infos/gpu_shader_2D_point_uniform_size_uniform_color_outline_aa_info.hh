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

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_point_uniform_size_uniform_color_outline_aa)
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_out(smooth_radii_outline_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(0, Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(20, Type::VEC4, "color")
    .push_constant(24, Type::VEC4, "outlineColor")
    .push_constant(28, Type::FLOAT, "size")
    .push_constant(29, Type::FLOAT, "outlineWidth")
    .vertex_source("gpu_shader_2D_point_uniform_size_outline_aa_vert.glsl")
    .fragment_source("gpu_shader_point_uniform_color_outline_aa_frag.glsl")
    .do_static_compilation(true);
