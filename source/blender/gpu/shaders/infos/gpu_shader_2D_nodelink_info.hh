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

GPU_SHADER_INTERFACE_INFO(nodelink_iface, "")
    .smooth(Type::VEC4, "finalColor")
    .smooth(Type::FLOAT, "colorGradient")
    .smooth(Type::FLOAT, "lineU")
    .flat(Type::FLOAT, "lineLength")
    .flat(Type::FLOAT, "dashFactor")
    .flat(Type::FLOAT, "dashAlpha")
    .flat(Type::INT, "isMainLine");

GPU_SHADER_CREATE_INFO(gpu_shader_2D_nodelink)
    .vertex_in(0, Type::VEC2, "uv")
    .vertex_in(1, Type::VEC2, "pos")
    .vertex_in(2, Type::VEC2, "expand")
    .vertex_out(nodelink_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .uniform_buf(0, "NodeLinkData", "node_link_data", Frequency::PASS)
    .push_constant(0, Type::MAT4, "ModelViewProjectionMatrix")
    .vertex_source("gpu_shader_2D_nodelink_vert.glsl")
    .fragment_source("gpu_shader_2D_nodelink_frag.glsl")
    .typedef_source("GPU_shader_shared.h")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(gpu_shader_2D_nodelink_inst)
    .vertex_in(0, Type::VEC2, "uv")
    .vertex_in(1, Type::VEC2, "pos")
    .vertex_in(2, Type::VEC2, "expand")
    .vertex_in(3, Type::VEC2, "P0")
    .vertex_in(4, Type::VEC2, "P1")
    .vertex_in(5, Type::VEC2, "P2")
    .vertex_in(6, Type::VEC2, "P3")
    .vertex_in(7, Type::IVEC4, "colid_doarrow")
    .vertex_in(8, Type::VEC4, "start_color")
    .vertex_in(9, Type::VEC4, "end_color")
    .vertex_in(10, Type::IVEC2, "domuted")
    .vertex_in(11, Type::FLOAT, "dim_factor")
    .vertex_in(12, Type::FLOAT, "thickness")
    .vertex_in(13, Type::FLOAT, "dash_factor")
    .vertex_in(14, Type::FLOAT, "dash_alpha")
    .vertex_out(nodelink_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .uniform_buf(0, "NodeLinkInstanceData", "node_link_data", Frequency::PASS)
    .push_constant(0, Type::MAT4, "ModelViewProjectionMatrix")
    .vertex_source("gpu_shader_2D_nodelink_vert.glsl")
    .fragment_source("gpu_shader_2D_nodelink_frag.glsl")
    .typedef_source("GPU_shader_shared.h")
    .define("USE_INSTANCE")
    .do_static_compilation(true);
