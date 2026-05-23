/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "eevee_fullscreen_infos.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_hiz_debug)
DO_STATIC_COMPILATION()
FRAGMENT_OUT_DUAL(0, float4, out_debug_color_add, SRC_0)
FRAGMENT_OUT_DUAL(0, float4, out_debug_color_mul, SRC_1)
FRAGMENT_SOURCE("eevee_hiz_debug_frag.glsl")
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_hiz_data)
ADDITIONAL_INFO(eevee_fullscreen)
GPU_SHADER_CREATE_END()
