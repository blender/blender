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

#pragma once

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(flat_color_iface, "").flat(Type::VEC4, "finalColor");
GPU_SHADER_INTERFACE_INFO(no_perspective_color_iface, "").no_perspective(Type::VEC4, "finalColor");
GPU_SHADER_INTERFACE_INFO(smooth_color_iface, "").smooth(Type::VEC4, "finalColor");
GPU_SHADER_INTERFACE_INFO(smooth_tex_coord_interp_iface, "").smooth(Type::VEC2, "texCoord_interp");
GPU_SHADER_INTERFACE_INFO(smooth_radii_iface, "").smooth(Type::VEC2, "radii");
GPU_SHADER_INTERFACE_INFO(smooth_radii_outline_iface, "").smooth(Type::VEC4, "radii");
GPU_SHADER_INTERFACE_INFO(flat_color_smooth_tex_coord_interp_iface, "")
    .flat(Type::VEC4, "finalColor")
    .smooth(Type::VEC2, "texCoord_interp");
