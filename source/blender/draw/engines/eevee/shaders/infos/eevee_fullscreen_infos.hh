/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(eevee_fullscreen_iface)
SMOOTH(float2, screen_uv)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_fullscreen)
VERTEX_OUT(eevee_fullscreen_iface)
VERTEX_SOURCE("eevee_fullscreen_vert.glsl")
GPU_SHADER_CREATE_END()
