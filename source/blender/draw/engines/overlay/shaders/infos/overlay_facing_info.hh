/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(overlay_facing)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
VERTEX_SOURCE("overlay_facing_vert.glsl")
FRAGMENT_SOURCE("overlay_facing_frag.glsl")
FRAGMENT_OUT(0, VEC4, fragColor)
ADDITIONAL_INFO(draw_mesh)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_facing_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_facing)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()
