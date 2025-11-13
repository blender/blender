/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "eevee_light_infos.hh"
#  include "eevee_lightprobe_infos.hh"
#  include "eevee_sampling_infos.hh"
#  include "eevee_shadow_infos.hh"
#  include "eevee_volume_resolved_infos.hh"
#  include "eevee_volume_shared.hh"
#  include "gpu_shader_fullscreen_infos.hh"

#  define SPHERE_PROBE
#endif

#include "gpu_shader_create_info.hh"

#pragma once

GPU_SHADER_CREATE_INFO(eevee_volume_scatter)
LOCAL_GROUP_SIZE(VOLUME_GROUP_SIZE, VOLUME_GROUP_SIZE, VOLUME_GROUP_SIZE)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_light_data)
ADDITIONAL_INFO(eevee_lightprobe_data)
ADDITIONAL_INFO(eevee_shadow_data)
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(eevee_utility_texture)
ADDITIONAL_INFO(eevee_volume_properties_data)
SAMPLER(0, sampler3D, scattering_history_tx)
SAMPLER(1, sampler3D, extinction_history_tx)
IMAGE(5, UFLOAT_11_11_10, write, image3D, out_scattering_img)
IMAGE(6, UFLOAT_11_11_10, write, image3D, out_extinction_img)
COMPUTE_SOURCE("eevee_volume_scatter_comp.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_volume_scatter_with_lights)
ADDITIONAL_INFO(eevee_volume_scatter)
DEFINE("VOLUME_LIGHTING")
DEFINE("VOLUME_IRRADIANCE")
DEFINE("VOLUME_SHADOW")
SAMPLER(9, sampler3D, extinction_tx)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_volume_occupancy_convert)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(gpu_fullscreen)
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
IMAGE(VOLUME_HIT_DEPTH_SLOT, SFLOAT_32, read, image3D, hit_depth_img)
IMAGE(VOLUME_HIT_COUNT_SLOT, UINT_32, read_write, uimage2D, hit_count_img)
IMAGE(VOLUME_OCCUPANCY_SLOT, UINT_32, read_write, uimage3DAtomic, occupancy_img)
FRAGMENT_SOURCE("eevee_occupancy_convert_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_volume_integration)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_sampling_data)
COMPUTE_SOURCE("eevee_volume_integration_comp.glsl")
LOCAL_GROUP_SIZE(VOLUME_INTEGRATION_GROUP_SIZE, VOLUME_INTEGRATION_GROUP_SIZE, 1)
/* Inputs. */
SAMPLER(0, sampler3D, in_scattering_tx)
SAMPLER(1, sampler3D, in_extinction_tx)
/* Outputs. */
IMAGE(0, UFLOAT_11_11_10, write, image3D, out_scattering_img)
IMAGE(1, UFLOAT_11_11_10, write, image3D, out_transmittance_img)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_volume_resolve)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_volume_lib)
ADDITIONAL_INFO(gpu_fullscreen)
ADDITIONAL_INFO(eevee_render_pass_out)
ADDITIONAL_INFO(eevee_hiz_data)
FRAGMENT_SOURCE("eevee_volume_resolve_frag.glsl")
FRAGMENT_OUT_DUAL(0, float4, out_radiance, SRC_0)
FRAGMENT_OUT_DUAL(0, float4, out_transmittance, SRC_1)
/** TODO(Miguel Pozo): Volume RenderPasses. */
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
