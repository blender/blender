/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(overlay_sculpt_mask_iface)
FLAT(VEC3, faceset_color)
SMOOTH(FLOAT, mask_color)
SMOOTH(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_sculpt_mask)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(FLOAT, maskOpacity)
PUSH_CONSTANT(FLOAT, faceSetsOpacity)
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC3, fset)
VERTEX_IN(2, FLOAT, msk)
VERTEX_OUT(overlay_sculpt_mask_iface)
VERTEX_SOURCE("overlay_sculpt_mask_vert.glsl")
FRAGMENT_SOURCE("overlay_sculpt_mask_frag.glsl")
FRAGMENT_OUT(0, VEC4, fragColor)
ADDITIONAL_INFO(draw_mesh)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_sculpt_mask_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_sculpt_mask)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()
