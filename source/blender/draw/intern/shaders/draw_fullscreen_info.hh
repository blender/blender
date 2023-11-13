/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(fullscreen_iface, "").smooth(Type::VEC4, "uvcoordsvar");

GPU_SHADER_CREATE_INFO(draw_fullscreen)
    .vertex_out(fullscreen_iface)
    .vertex_source("common_fullscreen_vert.glsl");
