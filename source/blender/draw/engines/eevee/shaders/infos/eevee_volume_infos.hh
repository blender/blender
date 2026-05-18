/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "eevee_fullscreen_infos.hh"
#  include "eevee_lightprobe_infos.hh"
#  include "eevee_sampling_infos.hh"
#  include "eevee_volume_resolved_infos.hh"
#  include "eevee_volume_shared.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define SPHERE_PROBE
#endif

#include "gpu_shader_create_info.hh"

#pragma once

GPU_SHADER_CREATE_INFO(eevee_volume_occupancy_convert)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(eevee_fullscreen)
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
IMAGE(VOLUME_HIT_DEPTH_SLOT, SFLOAT_32, read, image3D, hit_depth_img)
IMAGE(VOLUME_HIT_COUNT_SLOT, UINT_32, read_write, uimage2D, hit_count_img)
IMAGE(VOLUME_OCCUPANCY_SLOT, UINT_32, read_write, uimage3DAtomic, occupancy_img)
FRAGMENT_SOURCE("eevee_occupancy_convert_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
