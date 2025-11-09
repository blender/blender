/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "BLI_utildefines_variadic.h"

#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"

#  include "eevee_common_infos.hh"
#  include "eevee_light_infos.hh"
#  include "eevee_sampling_infos.hh"
#  include "eevee_shadow_infos.hh"
#  include "eevee_shadow_shared.hh"
#  include "eevee_uniform_infos.hh"
#  include "eevee_volume_infos.hh"

#  define CURVES_SHADER
#  define DRW_HAIR_INFO

#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO

#  define SHADOW_UPDATE_ATOMIC_RASTER
#  define MAT_TRANSPARENT

#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_surf_volume)
DEFINE("MAT_VOLUME")
/* Only the front fragments have to be invoked. */
EARLY_FRAGMENT_TEST(true)
IMAGE(VOLUME_PROP_SCATTERING_IMG_SLOT, UFLOAT_11_11_10, read_write, image3D, out_scattering_img)
IMAGE(VOLUME_PROP_EXTINCTION_IMG_SLOT, UFLOAT_11_11_10, read_write, image3D, out_extinction_img)
IMAGE(VOLUME_PROP_EMISSION_IMG_SLOT, UFLOAT_11_11_10, read_write, image3D, out_emissive_img)
IMAGE(VOLUME_PROP_PHASE_IMG_SLOT, SFLOAT_16, read_write, image3D, out_phase_img)
IMAGE(VOLUME_PROP_PHASE_WEIGHT_IMG_SLOT, SFLOAT_16, read_write, image3D, out_phase_weight_img)
IMAGE(VOLUME_OCCUPANCY_SLOT, UINT_32, read, uimage3DAtomic, occupancy_img)
FRAGMENT_SOURCE("eevee_surf_volume_frag.glsl")
ADDITIONAL_INFO(draw_modelmat_common)
ADDITIONAL_INFO(draw_view)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(eevee_utility_texture)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_surf_occupancy)
DEFINE("MAT_OCCUPANCY")
/* All fragments need to be invoked even if we write to the depth buffer. */
EARLY_FRAGMENT_TEST(false)
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
PUSH_CONSTANT(bool, use_fast_method)
IMAGE(VOLUME_HIT_DEPTH_SLOT, SFLOAT_32, write, image3D, hit_depth_img)
IMAGE(VOLUME_HIT_COUNT_SLOT, UINT_32, read_write, uimage2DAtomic, hit_count_img)
IMAGE(VOLUME_OCCUPANCY_SLOT, UINT_32, read_write, uimage3DAtomic, occupancy_img)
FRAGMENT_SOURCE("eevee_surf_occupancy_frag.glsl")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_sampling_data)
GPU_SHADER_CREATE_END()
