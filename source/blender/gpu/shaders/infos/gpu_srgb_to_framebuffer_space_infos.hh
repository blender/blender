/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "gpu_srgb_to_framebuffer_space_infos.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_srgb_to_framebuffer_space)
PUSH_CONSTANT(bool, srgbTarget)
DEFINE("blender_srgb_to_framebuffer_space(a) a")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_scene_linear_to_rec709_space)
PUSH_CONSTANT(float3x3, gpu_scene_linear_to_rec709)
DEFINE("BLENDER_SCENE_LINEAR_TO_REC709")
GPU_SHADER_CREATE_END()
