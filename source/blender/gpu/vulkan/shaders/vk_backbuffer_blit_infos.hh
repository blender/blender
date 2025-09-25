/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(vk_backbuffer_blit)
LOCAL_GROUP_SIZE(16, 16)
IMAGE(0, SFLOAT_16_16_16_16, read, Float2D, src_img)
IMAGE(1, SFLOAT_16_16_16_16, write, Float2D, dst_img)
PUSH_CONSTANT(float, sdr_scale)
PUSH_CONSTANT(bool, use_gamma22)
COMPUTE_SOURCE("vk_backbuffer_blit_comp.glsl")
ADDITIONAL_INFO(gpu_srgb_to_framebuffer_space)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(vk_backbuffer_blit_gamma22)
ADDITIONAL_INFO(vk_backbuffer_blit)
DEFINE("USE_GAMMA22")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
