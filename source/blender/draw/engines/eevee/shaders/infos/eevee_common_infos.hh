/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#  include "eevee_uniform_infos.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define EEVEE_SAMPLING_DATA
#  define MAT_CLIP_PLANE
#  define MAT_RENDER_PASS_SUPPORT
#  define MAT_TRANSPARENT
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_hiz_data)
SAMPLER(HIZ_TEX_SLOT, sampler2D, hiz_tx)
ADDITIONAL_INFO(eevee_global_ubo)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_hiz_prev_data)
SAMPLER(HIZ_PREVIOUS_LAYER_TEX_SLOT, sampler2D, hiz_prev_tx)
ADDITIONAL_INFO(eevee_global_ubo)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_previous_layer_radiance)
SAMPLER(RADIANCE_PREVIOUS_LAYER_TEX_SLOT, sampler2D, previous_layer_radiance_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_utility_texture)
SAMPLER(RBUFS_UTILITY_TEX_SLOT, sampler2DArray, utility_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_volume_properties_data)
ADDITIONAL_INFO(eevee_global_ubo)
IMAGE(VOLUME_PROP_SCATTERING_IMG_SLOT, UFLOAT_11_11_10, read, image3D, in_scattering_img)
IMAGE(VOLUME_PROP_EXTINCTION_IMG_SLOT, UFLOAT_11_11_10, read, image3D, in_extinction_img)
IMAGE(VOLUME_PROP_EMISSION_IMG_SLOT, UFLOAT_11_11_10, read, image3D, in_emission_img)
IMAGE(VOLUME_PROP_PHASE_IMG_SLOT, SFLOAT_16, read, image3D, in_phase_img)
IMAGE(VOLUME_PROP_PHASE_WEIGHT_IMG_SLOT, SFLOAT_16, read, image3D, in_phase_weight_img)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_gbuffer_data)
SAMPLER(GBUF_HEADER_TEX_SLOT, usampler2DArray, gbuf_header_tx)
SAMPLER(GBUF_CLOSURE_TEX_SLOT, sampler2DArray, gbuf_closure_tx)
SAMPLER(GBUF_NORMAL_TEX_SLOT, sampler2DArray, gbuf_normal_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_render_pass_out)
DEFINE("MAT_RENDER_PASS_SUPPORT")
ADDITIONAL_INFO(eevee_global_ubo)
IMAGE_FREQ(RBUFS_COLOR_SLOT, SFLOAT_16_16_16_16, write, image2DArray, rp_color_img, PASS)
IMAGE_FREQ(RBUFS_VALUE_SLOT, SFLOAT_16, write, image2DArray, rp_value_img, PASS)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_cryptomatte_out)
STORAGE_BUF(CRYPTOMATTE_BUF_SLOT, read, float2, cryptomatte_object_buf[])
IMAGE_FREQ(RBUFS_CRYPTOMATTE_SLOT, SFLOAT_32_32_32_32, write, image2D, rp_cryptomatte_img, PASS)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_raycast)
DEFINE("MAT_RAYCAST")
SAMPLER(RAYCAST_DEPTH_TEX_SLOT, sampler2D, raycast_depth_tx)
SAMPLER(OBJECT_ID_TEX_SLOT, usampler2D, object_id_tx)
SAMPLER(PREPASS_NORMAL_TEX_SLOT, sampler2D, prepass_normal_tx)
GPU_SHADER_CREATE_END()

/* Used for shaders that need the final accumulated volume transmittance and scattering. */
GPU_SHADER_CREATE_INFO(eevee_volume_lib)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(draw_view)
SAMPLER(VOLUME_SCATTERING_TEX_SLOT, sampler3D, volume_scattering_tx)
SAMPLER(VOLUME_TRANSMITTANCE_TEX_SLOT, sampler3D, volume_transmittance_tx)
GPU_SHADER_CREATE_END()

/** \} */
