/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "eevee_light_shared.hh"
#  include "eevee_lightprobe_shared.hh"
#  include "eevee_sampling_shared.hh"
#  include "eevee_shadow_shared.hh"
#  include "eevee_uniform_infos.hh"
#  include "eevee_uniform_shared.hh"

#  define EEVEE_SAMPLING_DATA
#  define MAT_CLIP_PLANE
#  define PLANAR_PROBES
#  define MAT_RENDER_PASS_SUPPORT
#  define SHADOW_READ_ATOMIC

/* Stub for C++ compilation. */
struct NodeTree {
  float crypto_hash;
  float _pad0;
  float _pad1;
  float _pad2;
};
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

/* Stub for C++ compilation. */
/* TODO(fclem): Use it for actual interface. */
GPU_SHADER_CREATE_INFO(eevee_node_tree)
UNIFORM_BUF(0 /*GPU_NODE_TREE_UBO_SLOT*/, NodeTree, node_tree)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_hiz_data)
SAMPLER(HIZ_TEX_SLOT, sampler2D, hiz_tx)
ADDITIONAL_INFO(eevee_global_ubo)
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
DEFINE("GBUFFER_LOAD")
SAMPLER(12, usampler2DArray, gbuf_header_tx)
SAMPLER(13, sampler2DArray, gbuf_closure_tx)
SAMPLER(14, sampler2DArray, gbuf_normal_tx)
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

/** \} */
