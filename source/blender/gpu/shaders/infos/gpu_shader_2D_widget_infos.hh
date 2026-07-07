/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "GPU_shader_shared.hh"
#  include "gpu_srgb_to_framebuffer_space_infos.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define widgetID 0
#endif

#include "gpu_interface_infos.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(gpu_widget_shadow_iface)
SMOOTH(float, shadowFalloff)
SMOOTH(float, innerMask)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_2D_widget_shadow)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
PUSH_CONSTANT_ARRAY(float4, parameters, 4)
PUSH_CONSTANT(float, alpha)
VERTEX_IN(0, uint, vflag)
VERTEX_OUT(gpu_widget_shadow_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("gpu_shader_2D_widget_shadow_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_2D_widget_shadow_frag.glsl")
GPU_SHADER_CREATE_END()
