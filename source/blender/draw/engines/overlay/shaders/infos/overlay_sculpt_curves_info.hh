/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_object_infos_info.hh"
#  include "draw_view_info.hh"

#  define HAIR_SHADER
#  define DRW_HAIR_INFO
#endif

#include "overlay_common_info.hh"

GPU_SHADER_INTERFACE_INFO(overlay_sculpt_curves_selection_iface)
SMOOTH(float, mask_weight)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_sculpt_curves_selection)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(bool, is_point_domain)
PUSH_CONSTANT(float, selection_opacity)
SAMPLER(1, samplerBuffer, selection_tx)
VERTEX_OUT(overlay_sculpt_curves_selection_iface)
VERTEX_SOURCE("overlay_sculpt_curves_selection_vert.glsl")
FRAGMENT_SOURCE("overlay_sculpt_curves_selection_frag.glsl")
FRAGMENT_OUT(0, float4, out_color)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_hair)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_sculpt_curves_selection)

GPU_SHADER_INTERFACE_INFO(overlay_sculpt_curves_cage_iface)
NO_PERSPECTIVE(float2, edge_pos)
FLAT(float2, edge_start)
SMOOTH(float4, final_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_sculpt_curves_cage)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float, selection)
VERTEX_OUT(overlay_sculpt_curves_cage_iface)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
PUSH_CONSTANT(float, opacity)
VERTEX_SOURCE("overlay_sculpt_curves_cage_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_sculpt_curves_cage)
