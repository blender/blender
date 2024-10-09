/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(workbench_merge_depth)
SAMPLER(0, DEPTH_2D, depth_tx)
FRAGMENT_SOURCE("workbench_merge_depth_frag.glsl")
ADDITIONAL_INFO(draw_fullscreen)
DEPTH_WRITE(DepthWrite::ANY)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_overlay_depth)
FRAGMENT_SOURCE("workbench_overlay_depth_frag.glsl")
ADDITIONAL_INFO(draw_fullscreen)
DEPTH_WRITE(DepthWrite::ANY)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(workbench_extract_stencil)
FRAGMENT_OUT(0, UINT, out_stencil_value)
FRAGMENT_SOURCE("workbench_extract_stencil.glsl")
ADDITIONAL_INFO(draw_fullscreen)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
