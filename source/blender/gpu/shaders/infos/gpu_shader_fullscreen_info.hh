/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(gpu_fullscreen_iface)
SMOOTH(float2, screen_uv)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_fullscreen)
VERTEX_OUT(gpu_fullscreen_iface)
VERTEX_SOURCE("gpu_shader_fullscreen_vert.glsl")
GPU_SHADER_CREATE_END()
