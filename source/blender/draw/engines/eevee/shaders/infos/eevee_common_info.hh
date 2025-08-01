/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_object_infos_info.hh"
#  include "draw_view_info.hh"
#  include "eevee_shader_shared.hh"

#  define EEVEE_SAMPLING_DATA
#  define EEVEE_UTILITY_TX
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

GPU_SHADER_CREATE_INFO(eevee_shared)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_shader_shared.hh")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_global_ubo)
UNIFORM_BUF(UNIFORM_BUF_SLOT, UniformData, uniform_buf)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_hiz_data)
SAMPLER(HIZ_TEX_SLOT, sampler2D, hiz_tx)
ADDITIONAL_INFO(eevee_global_ubo)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_sampling_data)
DEFINE("EEVEE_SAMPLING_DATA")
ADDITIONAL_INFO(eevee_shared)
STORAGE_BUF(SAMPLING_BUF_SLOT, read, SamplingData, sampling_buf)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_utility_texture)
DEFINE("EEVEE_UTILITY_TX")
SAMPLER(RBUFS_UTILITY_TEX_SLOT, sampler2DArray, utility_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_NAMED_INTERFACE_INFO(eevee_clip_plane_iface, clip_interp)
SMOOTH(float, clip_distance)
GPU_SHADER_NAMED_INTERFACE_END(clip_interp)

GPU_SHADER_CREATE_INFO(eevee_clip_plane)
VERTEX_OUT(eevee_clip_plane_iface)
UNIFORM_BUF(CLIP_PLANE_BUF, ClipPlaneData, clip_plane)
DEFINE("MAT_CLIP_PLANE")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_sphere_data)
DEFINE("SPHERE_PROBE")
UNIFORM_BUF(SPHERE_PROBE_BUF_SLOT, SphereProbeData, lightprobe_sphere_buf[SPHERE_PROBE_MAX])
SAMPLER(SPHERE_PROBE_TEX_SLOT, sampler2DArray, lightprobe_spheres_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_volume_probe_data)
UNIFORM_BUF(IRRADIANCE_GRID_BUF_SLOT, VolumeProbeData, grids_infos_buf[IRRADIANCE_GRID_MAX])
/* NOTE: Use uint instead of IrradianceBrickPacked because Metal needs to know the exact type.
 */
STORAGE_BUF(IRRADIANCE_BRICK_BUF_SLOT, read, uint, bricks_infos_buf[])
SAMPLER(VOLUME_PROBE_TEX_SLOT, sampler3D, irradiance_atlas_tx)
DEFINE("IRRADIANCE_GRID_SAMPLING")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_planar_data)
DEFINE("SPHERE_PROBE")
UNIFORM_BUF(PLANAR_PROBE_BUF_SLOT, PlanarProbeData, probe_planar_buf[PLANAR_PROBE_MAX])
SAMPLER(PLANAR_PROBE_RADIANCE_TEX_SLOT, sampler2DArray, planar_radiance_tx)
SAMPLER(PLANAR_PROBE_DEPTH_TEX_SLOT, sampler2DArrayDepth, planar_depth_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_lightprobe_data)
ADDITIONAL_INFO(eevee_lightprobe_sphere_data)
ADDITIONAL_INFO(eevee_volume_probe_data)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_light_data)
STORAGE_BUF(LIGHT_CULL_BUF_SLOT, read, LightCullingData, light_cull_buf)
STORAGE_BUF(LIGHT_BUF_SLOT, read, LightData, light_buf[])
STORAGE_BUF(LIGHT_ZBIN_BUF_SLOT, read, uint, light_zbin_buf[])
STORAGE_BUF(LIGHT_TILE_BUF_SLOT, read, uint, light_tile_buf[])
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_shadow_data)
/* SHADOW_READ_ATOMIC macro indicating shadow functions should use `usampler2DArrayAtomic` as
 * the atlas type. */
DEFINE("SHADOW_READ_ATOMIC")
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
SAMPLER(SHADOW_ATLAS_TEX_SLOT, usampler2DArrayAtomic, shadow_atlas_tx)
SAMPLER(SHADOW_TILEMAPS_TEX_SLOT, usampler2D, shadow_tilemaps_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_shadow_data_non_atomic)
SAMPLER(SHADOW_ATLAS_TEX_SLOT, usampler2DArray, shadow_atlas_tx)
SAMPLER(SHADOW_TILEMAPS_TEX_SLOT, usampler2D, shadow_tilemaps_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surfel_common)
STORAGE_BUF(SURFEL_BUF_SLOT, read_write, Surfel, surfel_buf[])
STORAGE_BUF(CAPTURE_BUF_SLOT, read, CaptureInfoData, capture_info_buf)
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
