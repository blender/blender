/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_interface_info.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(gpu_widget_iface, "")
    .flat(Type::FLOAT, "discardFac")
    .flat(Type::FLOAT, "lineWidth")
    .flat(Type::VEC2, "outRectSize")
    .flat(Type::VEC4, "borderColor")
    .flat(Type::VEC4, "embossColor")
    .flat(Type::VEC4, "outRoundCorners")
    .no_perspective(Type::FLOAT, "butCo")
    .no_perspective(Type::VEC2, "uvInterp")
    .no_perspective(Type::VEC4, "innerColor");

/* TODO(fclem): Share with C code. */
#define MAX_PARAM 12
#define MAX_INSTANCE 6

GPU_SHADER_CREATE_INFO(gpu_shader_2D_widget_shared)
    .define("MAX_PARAM", STRINGIFY(MAX_PARAM))
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC3, "checkerColorAndSize")
    .vertex_out(gpu_widget_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("gpu_shader_2D_widget_base_vert.glsl")
    .fragment_source("gpu_shader_2D_widget_base_frag.glsl")
    .additional_info("gpu_srgb_to_framebuffer_space");

GPU_SHADER_CREATE_INFO(gpu_shader_2D_widget_base)
    .do_static_compilation(true)
    /* gl_InstanceID is supposed to be 0 if not drawing instances, but this seems
     * to be violated in some drivers. For example, macOS 10.15.4 and Intel Iris
     * causes #78307 when using gl_InstanceID outside of instance. */
    .define("widgetID", "0")
    .push_constant(Type::VEC4, "parameters", MAX_PARAM)
    .additional_info("gpu_shader_2D_widget_shared");

GPU_SHADER_CREATE_INFO(gpu_shader_2D_widget_base_inst)
    .do_static_compilation(true)
    .define("widgetID", "gl_InstanceID")
    .push_constant(Type::VEC4, "parameters", (MAX_PARAM * MAX_INSTANCE))
    .additional_info("gpu_shader_2D_widget_shared");

GPU_SHADER_INTERFACE_INFO(gpu_widget_shadow_iface, "").smooth(Type::FLOAT, "shadowFalloff");

GPU_SHADER_CREATE_INFO(gpu_shader_2D_widget_shadow)
    .do_static_compilation(true)
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC4, "parameters", 4)
    .push_constant(Type::FLOAT, "alpha")
    .vertex_in(0, Type::UINT, "vflag")
    .vertex_out(gpu_widget_shadow_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("gpu_shader_2D_widget_shadow_vert.glsl")
    .fragment_source("gpu_shader_2D_widget_shadow_frag.glsl");
