/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "overlay_common_info.hh"

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
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_sculpt_mask)
