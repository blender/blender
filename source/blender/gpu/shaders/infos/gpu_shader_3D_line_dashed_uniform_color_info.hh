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

/* TODO(jbakker): Skipped as data doesn't fit as push constant. */
GPU_SHADER_CREATE_INFO(gpu_shader_3D_line_dashed_uniform_color)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_out(flat_color_iface)
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .vertex_source("gpu_shader_3D_line_dashed_uniform_color_vert.glsl")
    .fragment_source("gpu_shader_2D_line_dashed_frag.glsl")
    .do_static_compilation(true);
