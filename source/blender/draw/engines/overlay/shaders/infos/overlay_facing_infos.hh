/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#endif

#include "overlay_common_infos.hh"

GPU_SHADER_CREATE_INFO(overlay_facing_base)
VERTEX_IN(0, float3, pos)
VERTEX_SOURCE("overlay_facing_vert.glsl")
FRAGMENT_SOURCE("overlay_facing_frag.glsl")
FRAGMENT_OUT(0, float4, frag_color)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_facing, overlay_facing_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_facing_selectable, overlay_facing_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_facing_clipped, overlay_facing, drw_clipped)
CREATE_INFO_VARIANT(overlay_facing_selectable_clipped, overlay_facing_selectable, drw_clipped)
/* clang-format on */
