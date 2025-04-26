/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_object_infos_info.hh"
#  include "draw_view_info.hh"
#endif

#include "overlay_common_info.hh"

GPU_SHADER_CREATE_INFO(overlay_facing_base)
VERTEX_IN(0, float3, pos)
VERTEX_SOURCE("overlay_facing_vert.glsl")
FRAGMENT_SOURCE("overlay_facing_frag.glsl")
FRAGMENT_OUT(0, float4, frag_color)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_facing, overlay_facing_base)
