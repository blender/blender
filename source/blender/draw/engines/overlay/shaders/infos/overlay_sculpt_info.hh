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

GPU_SHADER_INTERFACE_INFO(overlay_sculpt_mask_iface)
FLAT(float3, faceset_color)
SMOOTH(float, mask_color)
SMOOTH(float4, final_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_sculpt_mask)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(float, mask_opacity)
PUSH_CONSTANT(float, face_sets_opacity)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float3, fset)
VERTEX_IN(2, float, msk)
VERTEX_OUT(overlay_sculpt_mask_iface)
VERTEX_SOURCE("overlay_sculpt_mask_vert.glsl")
FRAGMENT_SOURCE("overlay_sculpt_mask_frag.glsl")
FRAGMENT_OUT(0, float4, frag_color)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_sculpt_mask)
