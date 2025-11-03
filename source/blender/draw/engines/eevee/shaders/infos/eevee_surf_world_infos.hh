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

GPU_SHADER_CREATE_INFO(eevee_surf_world)
PUSH_CONSTANT(float, world_opacity_fade)
PUSH_CONSTANT(float, world_background_blur)
PUSH_CONSTANT(int4, world_coord_packed)
EARLY_FRAGMENT_TEST(true)
FRAGMENT_OUT(0, float4, out_background)
FRAGMENT_SOURCE("eevee_surf_world_frag.glsl")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_lightprobe_sphere_data)
ADDITIONAL_INFO(eevee_volume_probe_data)
ADDITIONAL_INFO(eevee_sampling_data)
/* Optionally added depending on the material. */
// ADDITIONAL_INFO(eevee_render_pass_out)
// ADDITIONAL_INFO(eevee_cryptomatte_out)
ADDITIONAL_INFO(eevee_utility_texture)
GPU_SHADER_CREATE_END()
