/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "eevee_debug_shared.hh"
#  include "eevee_fullscreen_infos.hh"
#  include "eevee_light_infos.hh"
#  include "eevee_lightprobe_infos.hh"
#  include "eevee_sampling_infos.hh"
#  include "eevee_shadow_infos.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_deferred_tile_classify)
FRAGMENT_SOURCE("eevee_deferred_tile_classify_frag.glsl")
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_fullscreen)
SUBPASS_IN(1, uint, Uint2DArray, in_gbuffer_header, DEFERRED_GBUFFER_ROG_ID)
TYPEDEF_SOURCE("draw_shader_shared.hh")
PUSH_CONSTANT(int, current_bit)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_deferred_combine)
/* Early fragment test is needed to avoid processing fragments background fragments. */
EARLY_FRAGMENT_TEST(true)
/* Inputs. */
SAMPLER(2, usampler2D, direct_radiance_1_tx)
SAMPLER(4, usampler2D, direct_radiance_2_tx)
SAMPLER(5, usampler2D, direct_radiance_3_tx)
SAMPLER(6, sampler2D, indirect_radiance_1_tx)
SAMPLER(7, sampler2D, indirect_radiance_2_tx)
SAMPLER(8, sampler2D, indirect_radiance_3_tx)
IMAGE(5, SFLOAT_16_16_16_16, read_write, image2D, radiance_feedback_img)
FRAGMENT_OUT(0, float4, out_combined)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_gbuffer_data)
ADDITIONAL_INFO(eevee_render_pass_out)
ADDITIONAL_INFO(eevee_hiz_data)
ADDITIONAL_INFO(eevee_fullscreen)
ADDITIONAL_INFO(draw_view)
FRAGMENT_SOURCE("eevee_deferred_combine_frag.glsl")
/* NOTE: Both light IDs have a valid specialized assignment of '-1' so only when default is
 * present will we instead dynamically look-up ID from the uniform buffer. */
SPECIALIZATION_CONSTANT(bool, render_pass_diffuse_light_enabled, false)
SPECIALIZATION_CONSTANT(bool, render_pass_specular_light_enabled, false)
SPECIALIZATION_CONSTANT(bool, render_pass_normal_enabled, false)
SPECIALIZATION_CONSTANT(bool, render_pass_position_enabled, false)
SPECIALIZATION_CONSTANT(bool, use_radiance_feedback, false)
SPECIALIZATION_CONSTANT(bool, use_split_radiance, true)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_deferred_aov_clear)
/* Early fragment test is needed to avoid processing fragments without correct GBuffer data. */
EARLY_FRAGMENT_TEST(true)
ADDITIONAL_INFO(eevee_render_pass_out)
ADDITIONAL_INFO(eevee_fullscreen)
FRAGMENT_SOURCE("eevee_deferred_aov_clear_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/* -------------------------------------------------------------------- */
/** \name Debug
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_debug_gbuffer)
DO_STATIC_COMPILATION()
FRAGMENT_OUT_DUAL(0, float4, out_color_add, SRC_0)
FRAGMENT_OUT_DUAL(0, float4, out_color_mul, SRC_1)
PUSH_CONSTANT(int, debug_mode)
TYPEDEF_SOURCE("eevee_debug_shared.hh")
FRAGMENT_SOURCE("eevee_debug_gbuffer_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_fullscreen)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_gbuffer_data)
GPU_SHADER_CREATE_END()

/** \} */
