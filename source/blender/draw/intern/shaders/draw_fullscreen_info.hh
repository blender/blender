/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(fullscreen_iface)
SMOOTH(VEC4, uvcoordsvar)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(draw_fullscreen)
VERTEX_OUT(fullscreen_iface)
VERTEX_SOURCE("common_fullscreen_vert.glsl")
GPU_SHADER_CREATE_END()
