/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once

#  include "gpu_shader_compat.hh"

#  include "gpencil_shader_shared.hh"

#  include "draw_view_infos.hh"
#  include "gpu_shader_fullscreen_infos.hh"

#  define COMPOSITE
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpencil_fx_common)
SAMPLER(0, sampler2D, color_buf)
SAMPLER(1, sampler2D, reveal_buf)
/* Reminder: This is considered SRC color in blend equations.
 * Same operation on all buffers. */
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, fragRevealage)
FRAGMENT_SOURCE("gpencil_vfx_frag.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_composite)
DO_STATIC_COMPILATION()
DEFINE("COMPOSITE")
PUSH_CONSTANT(bool, is_first_pass)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_colorize)
DO_STATIC_COMPILATION()
DEFINE("COLORIZE")
PUSH_CONSTANT(float3, low_color)
PUSH_CONSTANT(float3, high_color)
PUSH_CONSTANT(float, factor)
PUSH_CONSTANT(int, mode)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_blur)
DO_STATIC_COMPILATION()
DEFINE("BLUR")
PUSH_CONSTANT(float2, offset)
PUSH_CONSTANT(int, samp_count)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_transform)
DO_STATIC_COMPILATION()
DEFINE("TRANSFORM")
PUSH_CONSTANT(float2, axis_flip)
PUSH_CONSTANT(float2, wave_dir)
PUSH_CONSTANT(float2, wave_offset)
PUSH_CONSTANT(float, wave_phase)
PUSH_CONSTANT(float2, swirl_center)
PUSH_CONSTANT(float, swirl_angle)
PUSH_CONSTANT(float, swirl_radius)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_glow)
DO_STATIC_COMPILATION()
DEFINE("GLOW")
PUSH_CONSTANT(float4, glow_color)
PUSH_CONSTANT(float2, offset)
PUSH_CONSTANT(int, samp_count)
PUSH_CONSTANT(float4, threshold)
PUSH_CONSTANT(bool, first_pass)
PUSH_CONSTANT(bool, glow_under)
PUSH_CONSTANT(int, blend_mode)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_rim)
DO_STATIC_COMPILATION()
DEFINE("RIM")
PUSH_CONSTANT(float2, blur_dir)
PUSH_CONSTANT(float2, uv_offset)
PUSH_CONSTANT(float3, rim_color)
PUSH_CONSTANT(float3, mask_color)
PUSH_CONSTANT(int, samp_count)
PUSH_CONSTANT(int, blend_mode)
PUSH_CONSTANT(bool, is_first_pass)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_shadow)
DO_STATIC_COMPILATION()
DEFINE("SHADOW")
PUSH_CONSTANT(float4, shadow_color)
PUSH_CONSTANT(float2, uv_rot_x)
PUSH_CONSTANT(float2, uv_rot_y)
PUSH_CONSTANT(float2, uv_offset)
PUSH_CONSTANT(float2, blur_dir)
PUSH_CONSTANT(float2, wave_dir)
PUSH_CONSTANT(float2, wave_offset)
PUSH_CONSTANT(float, wave_phase)
PUSH_CONSTANT(int, samp_count)
PUSH_CONSTANT(bool, is_first_pass)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_pixelize)
DO_STATIC_COMPILATION()
DEFINE("PIXELIZE")
PUSH_CONSTANT(float2, target_pixel_size)
PUSH_CONSTANT(float2, target_pixel_offset)
PUSH_CONSTANT(float2, accum_offset)
PUSH_CONSTANT(int, samp_count)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()
