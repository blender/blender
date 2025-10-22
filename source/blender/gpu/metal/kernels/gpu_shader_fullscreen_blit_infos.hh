/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#pragma once

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(fullscreen_blit_iface)
SMOOTH(float2, screen_uv)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(fullscreen_blit)
VERTEX_IN(0, float2, pos)
VERTEX_OUT(fullscreen_blit_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float2, fullscreen)
PUSH_CONSTANT(float2, size)
PUSH_CONSTANT(float2, dst_offset)
PUSH_CONSTANT(float2, src_offset)
PUSH_CONSTANT(int, mip)
SAMPLER(0, sampler2D, imageTexture)
VERTEX_SOURCE("gpu_shader_fullscreen_blit_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_fullscreen_blit_frag.glsl")
DO_STATIC_COMPILATION();
GPU_SHADER_CREATE_END()
