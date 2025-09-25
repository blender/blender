/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "gpu_shader_fullscreen_infos.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_merge_depth)
SAMPLER(0, sampler2DDepth, depth_tx)
FRAGMENT_SOURCE("workbench_merge_depth_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
DEPTH_WRITE(DepthWrite::ANY)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_overlay_depth)
FRAGMENT_SOURCE("workbench_overlay_depth_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
DEPTH_WRITE(DepthWrite::ANY)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
