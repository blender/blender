/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_keying_screen)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(FLOAT, smoothness)
PUSH_CONSTANT(INT, number_of_markers)
STORAGE_BUF(0, READ, vec2, marker_positions[])
STORAGE_BUF(1, READ, vec4, marker_colors[])
IMAGE(0, GPU_RGBA16F, WRITE, FLOAT_2D, output_img)
COMPUTE_SOURCE("compositor_keying_screen.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
