/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "eevee_debug_shared.hh"
#  include "eevee_fullscreen_infos.hh"
#  include "eevee_lightprobe_infos.hh"
#  include "eevee_sampling_infos.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Debug
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_debug_gbuffer)
DO_STATIC_COMPILATION()
FRAGMENT_OUT_DUAL(0, float4, out_color_add, SRC_0)
FRAGMENT_OUT_DUAL(0, float4, out_color_mul, SRC_1)
PUSH_CONSTANT(int, debug_mode)
TYPEDEF_SOURCE("eevee_debug_shared.hh")
FRAGMENT_SOURCE("eevee_debug_gbuffer_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(eevee_fullscreen)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_gbuffer_data)
GPU_SHADER_CREATE_END()

/** \} */
