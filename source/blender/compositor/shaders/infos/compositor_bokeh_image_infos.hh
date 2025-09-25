/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(compositor_bokeh_image)
LOCAL_GROUP_SIZE(16, 16)
PUSH_CONSTANT(float, exterior_angle)
PUSH_CONSTANT(float, rotation)
PUSH_CONSTANT(float, roundness)
PUSH_CONSTANT(float, catadioptric)
PUSH_CONSTANT(float, lens_shift)
IMAGE(0, SFLOAT_16_16_16_16, write, image2D, output_img)
COMPUTE_SOURCE("compositor_bokeh_image.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
