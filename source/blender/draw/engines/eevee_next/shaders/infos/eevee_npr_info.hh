/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(npr_surface_common)
EARLY_FRAGMENT_TEST(true)
SAMPLER(INDEX_NPR_TX_SLOT, UINT_2D, npr_index_tx)
/* eevee_gbuffer_data */
DEFINE("GBUFFER_LOAD")
SAMPLER(GBUF_NORMAL_NPR_TX_SLOT, FLOAT_2D_ARRAY, gbuf_normal_tx)
SAMPLER(GBUF_HEADER_NPR_TX_SLOT, UINT_2D, gbuf_header_tx)
SAMPLER(GBUF_CLOSURE_NPR_TX_SLOT, FLOAT_2D_ARRAY, gbuf_closure_tx)
/* eevee_gbuffer_data */
SAMPLER(RADIANCE_TX_SLOT, FLOAT_2D, radiance_tx)
/* eevee_deferred_combine */
SAMPLER(DIRECT_RADIANCE_NPR_TX_SLOT_1 + 0, UINT_2D, direct_radiance_1_tx)
SAMPLER(DIRECT_RADIANCE_NPR_TX_SLOT_1 + 1, UINT_2D, direct_radiance_2_tx)
SAMPLER(DIRECT_RADIANCE_NPR_TX_SLOT_1 + 2, UINT_2D, direct_radiance_3_tx)
SAMPLER(INDIRECT_RADIANCE_NPR_TX_SLOT_1 + 0, FLOAT_2D, indirect_radiance_1_tx)
SAMPLER(INDIRECT_RADIANCE_NPR_TX_SLOT_1 + 1, FLOAT_2D, indirect_radiance_2_tx)
SAMPLER(INDIRECT_RADIANCE_NPR_TX_SLOT_1 + 2, FLOAT_2D, indirect_radiance_3_tx)
PUSH_CONSTANT(BOOL, use_split_radiance)
/* eevee_deferred_combine */
SAMPLER(BACK_RADIANCE_TX_SLOT, FLOAT_2D, radiance_back_tx)
SAMPLER(BACK_HIZ_TX_SLOT, FLOAT_2D, hiz_back_tx)
PUSH_CONSTANT(INT, npr_index)
DEFINE("NPR_SHADER")
FRAGMENT_OUT(0, VEC4, out_radiance)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_shared)
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_light_data)
ADDITIONAL_INFO(eevee_shadow_data)
ADDITIONAL_INFO(eevee_utility_texture)
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(eevee_hiz_data)
ADDITIONAL_INFO(eevee_render_pass_inout)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_npr)
FRAGMENT_SOURCE("eevee_surf_deferred_npr_frag.glsl")
ADDITIONAL_INFO(npr_surface_common)
GPU_SHADER_CREATE_END()
