/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once

#  include "gpu_glsl_cpp_stubs.hh"

#  include "gpencil_shader_shared.hh"

#  include "draw_view_info.hh"
#  include "gpu_shader_fullscreen_info.hh"

#  define COMPOSITE
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpencil_fx_common)
SAMPLER(0, FLOAT_2D, colorBuf)
SAMPLER(1, FLOAT_2D, revealBuf)
/* Reminder: This is considered SRC color in blend equations.
 * Same operation on all buffers. */
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, fragRevealage)
FRAGMENT_SOURCE("gpencil_vfx_frag.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_composite)
DO_STATIC_COMPILATION()
DEFINE("COMPOSITE")
PUSH_CONSTANT(bool, isFirstPass)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_colorize)
DO_STATIC_COMPILATION()
DEFINE("COLORIZE")
PUSH_CONSTANT(float3, lowColor)
PUSH_CONSTANT(float3, highColor)
PUSH_CONSTANT(float, factor)
PUSH_CONSTANT(int, mode)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_blur)
DO_STATIC_COMPILATION()
DEFINE("BLUR")
PUSH_CONSTANT(float2, offset)
PUSH_CONSTANT(int, sampCount)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_transform)
DO_STATIC_COMPILATION()
DEFINE("TRANSFORM")
PUSH_CONSTANT(float2, axisFlip)
PUSH_CONSTANT(float2, waveDir)
PUSH_CONSTANT(float2, waveOffset)
PUSH_CONSTANT(float, wavePhase)
PUSH_CONSTANT(float2, swirlCenter)
PUSH_CONSTANT(float, swirlAngle)
PUSH_CONSTANT(float, swirlRadius)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_glow)
DO_STATIC_COMPILATION()
DEFINE("GLOW")
PUSH_CONSTANT(float4, glowColor)
PUSH_CONSTANT(float2, offset)
PUSH_CONSTANT(int, sampCount)
PUSH_CONSTANT(float4, threshold)
PUSH_CONSTANT(bool, firstPass)
PUSH_CONSTANT(bool, glowUnder)
PUSH_CONSTANT(int, blendMode)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_rim)
DO_STATIC_COMPILATION()
DEFINE("RIM")
PUSH_CONSTANT(float2, blurDir)
PUSH_CONSTANT(float2, uvOffset)
PUSH_CONSTANT(float3, rimColor)
PUSH_CONSTANT(float3, maskColor)
PUSH_CONSTANT(int, sampCount)
PUSH_CONSTANT(int, blendMode)
PUSH_CONSTANT(bool, isFirstPass)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_shadow)
DO_STATIC_COMPILATION()
DEFINE("SHADOW")
PUSH_CONSTANT(float4, shadowColor)
PUSH_CONSTANT(float2, uvRotX)
PUSH_CONSTANT(float2, uvRotY)
PUSH_CONSTANT(float2, uvOffset)
PUSH_CONSTANT(float2, blurDir)
PUSH_CONSTANT(float2, waveDir)
PUSH_CONSTANT(float2, waveOffset)
PUSH_CONSTANT(float, wavePhase)
PUSH_CONSTANT(int, sampCount)
PUSH_CONSTANT(bool, isFirstPass)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpencil_fx_pixelize)
DO_STATIC_COMPILATION()
DEFINE("PIXELIZE")
PUSH_CONSTANT(float2, targetPixelSize)
PUSH_CONSTANT(float2, targetPixelOffset)
PUSH_CONSTANT(float2, accumOffset)
PUSH_CONSTANT(int, sampCount)
ADDITIONAL_INFO(gpencil_fx_common)
ADDITIONAL_INFO(gpu_fullscreen)
GPU_SHADER_CREATE_END()
