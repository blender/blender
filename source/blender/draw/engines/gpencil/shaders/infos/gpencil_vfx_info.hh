/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpencil_fx_common)
    .sampler(0, ImageType::FLOAT_2D, "colorBuf")
    .sampler(1, ImageType::FLOAT_2D, "revealBuf")
    /* Reminder: This is considered SRC color in blend equations.
     * Same operation on all buffers. */
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "fragRevealage")
    .fragment_source("gpencil_vfx_frag.glsl");

GPU_SHADER_CREATE_INFO(gpencil_fx_composite)
    .do_static_compilation(true)
    .define("COMPOSITE")
    .push_constant(Type::BOOL, "isFirstPass")
    .additional_info("gpencil_fx_common", "draw_fullscreen");

GPU_SHADER_CREATE_INFO(gpencil_fx_colorize)
    .do_static_compilation(true)
    .define("COLORIZE")
    .push_constant(Type::VEC3, "lowColor")
    .push_constant(Type::VEC3, "highColor")
    .push_constant(Type::FLOAT, "factor")
    .push_constant(Type::INT, "mode")
    .additional_info("gpencil_fx_common", "draw_fullscreen");

GPU_SHADER_CREATE_INFO(gpencil_fx_blur)
    .do_static_compilation(true)
    .define("BLUR")
    .push_constant(Type::VEC2, "offset")
    .push_constant(Type::INT, "sampCount")
    .additional_info("gpencil_fx_common", "draw_fullscreen");

GPU_SHADER_CREATE_INFO(gpencil_fx_transform)
    .do_static_compilation(true)
    .define("TRANSFORM")
    .push_constant(Type::VEC2, "axisFlip")
    .push_constant(Type::VEC2, "waveDir")
    .push_constant(Type::VEC2, "waveOffset")
    .push_constant(Type::FLOAT, "wavePhase")
    .push_constant(Type::VEC2, "swirlCenter")
    .push_constant(Type::FLOAT, "swirlAngle")
    .push_constant(Type::FLOAT, "swirlRadius")
    .additional_info("gpencil_fx_common", "draw_fullscreen");

GPU_SHADER_CREATE_INFO(gpencil_fx_glow)
    .do_static_compilation(true)
    .define("GLOW")
    .push_constant(Type::VEC4, "glowColor")
    .push_constant(Type::VEC2, "offset")
    .push_constant(Type::INT, "sampCount")
    .push_constant(Type::VEC4, "threshold")
    .push_constant(Type::BOOL, "firstPass")
    .push_constant(Type::BOOL, "glowUnder")
    .push_constant(Type::INT, "blendMode")
    .additional_info("gpencil_fx_common", "draw_fullscreen");

GPU_SHADER_CREATE_INFO(gpencil_fx_rim)
    .do_static_compilation(true)
    .define("RIM")
    .push_constant(Type::VEC2, "blurDir")
    .push_constant(Type::VEC2, "uvOffset")
    .push_constant(Type::VEC3, "rimColor")
    .push_constant(Type::VEC3, "maskColor")
    .push_constant(Type::INT, "sampCount")
    .push_constant(Type::INT, "blendMode")
    .push_constant(Type::BOOL, "isFirstPass")
    .additional_info("gpencil_fx_common", "draw_fullscreen");

GPU_SHADER_CREATE_INFO(gpencil_fx_shadow)
    .do_static_compilation(true)
    .define("SHADOW")
    .push_constant(Type::VEC4, "shadowColor")
    .push_constant(Type::VEC2, "uvRotX")
    .push_constant(Type::VEC2, "uvRotY")
    .push_constant(Type::VEC2, "uvOffset")
    .push_constant(Type::VEC2, "blurDir")
    .push_constant(Type::VEC2, "waveDir")
    .push_constant(Type::VEC2, "waveOffset")
    .push_constant(Type::FLOAT, "wavePhase")
    .push_constant(Type::INT, "sampCount")
    .push_constant(Type::BOOL, "isFirstPass")
    .additional_info("gpencil_fx_common", "draw_fullscreen");

GPU_SHADER_CREATE_INFO(gpencil_fx_pixelize)
    .do_static_compilation(true)
    .define("PIXELIZE")
    .push_constant(Type::VEC2, "targetPixelSize")
    .push_constant(Type::VEC2, "targetPixelOffset")
    .push_constant(Type::VEC2, "accumOffset")
    .push_constant(Type::INT, "sampCount")
    .additional_info("gpencil_fx_common", "draw_fullscreen");
