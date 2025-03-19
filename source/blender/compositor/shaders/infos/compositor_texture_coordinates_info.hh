/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_texture_coordinates)
LOCAL_GROUP_SIZE(16, 16)
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_texture_coordinates.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
