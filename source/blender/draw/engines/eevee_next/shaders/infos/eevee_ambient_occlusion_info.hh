/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_view_info.hh"
#  include "eevee_common_info.hh"

#  define HORIZON_OCCLUSION
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_ambient_occlusion_pass)
DEFINE("HORIZON_OCCLUSION")
COMPUTE_SOURCE("eevee_ambient_occlusion_pass_comp.glsl")
LOCAL_GROUP_SIZE(AMBIENT_OCCLUSION_PASS_TILE_SIZE, AMBIENT_OCCLUSION_PASS_TILE_SIZE)
IMAGE(0, GPU_RGBA16F, READ, FLOAT_2D_ARRAY, in_normal_img)
PUSH_CONSTANT(INT, in_normal_img_layer_index)
IMAGE(1, GPU_R16F, WRITE, FLOAT_2D_ARRAY, out_ao_img)
PUSH_CONSTANT(INT, out_ao_img_layer_index)
SPECIALIZATION_CONSTANT(INT, ao_slice_count, 2)
SPECIALIZATION_CONSTANT(INT, ao_step_count, 8)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_hiz_data)
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(eevee_utility_texture)
ADDITIONAL_INFO(eevee_global_ubo)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
