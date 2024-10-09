/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(overlay_sculpt_curves_selection_iface)
SMOOTH(FLOAT, mask_weight)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_sculpt_curves_selection)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(BOOL, is_point_domain)
PUSH_CONSTANT(FLOAT, selection_opacity)
SAMPLER(1, FLOAT_BUFFER, selection_tx)
VERTEX_OUT(overlay_sculpt_curves_selection_iface)
VERTEX_SOURCE("overlay_sculpt_curves_selection_vert.glsl")
FRAGMENT_SOURCE("overlay_sculpt_curves_selection_frag.glsl")
FRAGMENT_OUT(0, VEC4, out_color)
ADDITIONAL_INFO(draw_hair)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_sculpt_curves_selection_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_sculpt_curves_selection)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(overlay_sculpt_curves_cage_iface)
NO_PERSPECTIVE(VEC2, edgePos)
FLAT(VEC2, edgeStart)
SMOOTH(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_sculpt_curves_cage)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, FLOAT, selection)
VERTEX_OUT(overlay_sculpt_curves_cage_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
PUSH_CONSTANT(FLOAT, opacity)
VERTEX_SOURCE("overlay_sculpt_curves_cage_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_frag.glsl")
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_sculpt_curves_cage_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_sculpt_curves_cage)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()
