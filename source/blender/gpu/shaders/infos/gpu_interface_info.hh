/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
GPU_SHADER_INTERFACE_INFO(smooth_icon_interp_iface, "")
    .smooth(Type::VEC2, "texCoord_interp")
    .smooth(Type::VEC2, "mask_coord_interp");
