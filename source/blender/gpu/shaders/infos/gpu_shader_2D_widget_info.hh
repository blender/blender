/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "GPU_shader_shared.hh"
#  include "gpu_srgb_to_framebuffer_space_info.hh"

#  define widgetID 0
#endif

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(gpu_widget_iface)
FLAT(FLOAT, discardFac)
FLAT(FLOAT, lineWidth)
FLAT(VEC2, outRectSize)
FLAT(VEC4, borderColor)
FLAT(VEC4, embossColor)
FLAT(VEC4, outRoundCorners)
NO_PERSPECTIVE(FLOAT, butCo)
NO_PERSPECTIVE(VEC2, uvInterp)
NO_PERSPECTIVE(VEC4, innerColor)
GPU_SHADER_INTERFACE_END()

/* TODO(fclem): Share with C code. */
#define MAX_PARAM 12
#define MAX_INSTANCE 6

GPU_SHADER_CREATE_INFO(gpu_shader_2D_widget_shared)
DEFINE_VALUE("MAX_PARAM", STRINGIFY(MAX_PARAM))
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(VEC3, checkerColorAndSize)
VERTEX_OUT(gpu_widget_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("gpu_shader_2D_widget_base_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_2D_widget_base_frag.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_2D_widget_base)
DO_STATIC_COMPILATION()
/* gl_InstanceID is supposed to be 0 if not drawing instances, but this seems
 * to be violated in some drivers. For example, macOS 10.15.4 and Intel Iris
 * causes #78307 when using gl_InstanceID outside of instance. */
DEFINE_VALUE("widgetID", "0")
PUSH_CONSTANT_ARRAY(VEC4, parameters, MAX_PARAM)
ADDITIONAL_INFO(gpu_shader_2D_widget_shared)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_2D_widget_base_inst)
DO_STATIC_COMPILATION()
DEFINE_VALUE("widgetID", "gl_InstanceID")
PUSH_CONSTANT_ARRAY(VEC4, parameters, (MAX_PARAM * MAX_INSTANCE))
ADDITIONAL_INFO(gpu_shader_2D_widget_shared)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(gpu_widget_shadow_iface)
SMOOTH(FLOAT, shadowFalloff)
SMOOTH(FLOAT, innerMask)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_2D_widget_shadow)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT_ARRAY(VEC4, parameters, 4)
PUSH_CONSTANT(FLOAT, alpha)
VERTEX_IN(0, UINT, vflag)
VERTEX_OUT(gpu_widget_shadow_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("gpu_shader_2D_widget_shadow_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_2D_widget_shadow_frag.glsl")
GPU_SHADER_CREATE_END()
