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

GPU_SHADER_CREATE_INFO(eevee_surf_forward)
DEFINE("MAT_FORWARD")
/* Early fragment test is needed for render passes support for forward surfaces. */
/* NOTE: This removes the possibility of using gl_FragDepth. */
EARLY_FRAGMENT_TEST(true)
FRAGMENT_OUT_DUAL(0, float4, out_radiance, SRC_0)
FRAGMENT_OUT_DUAL(0, float4, out_transmittance, SRC_1)
FRAGMENT_SOURCE("eevee_surf_forward_frag.glsl")
/* Optionally added depending on the material. */
//  ADDITIONAL_INFO(eevee_render_pass_out)
//  ADDITIONAL_INFO(eevee_cryptomatte_out)
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_light_data)
ADDITIONAL_INFO(eevee_lightprobe_data)
ADDITIONAL_INFO(eevee_utility_texture)
ADDITIONAL_INFO(eevee_sampling_data)
ADDITIONAL_INFO(eevee_shadow_data)
ADDITIONAL_INFO(eevee_hiz_data)
ADDITIONAL_INFO(eevee_volume_lib)
GPU_SHADER_CREATE_END()
