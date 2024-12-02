/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_view_info.hh"

#  define SPHERE_PROBE
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(eevee_lookdev_display_iface)
SMOOTH(VEC2, uv_coord)
FLAT(UINT, sphere_id)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_lookdev_display)
VERTEX_SOURCE("eevee_lookdev_display_vert.glsl")
VERTEX_OUT(eevee_lookdev_display_iface)
PUSH_CONSTANT(VEC2, viewportSize)
PUSH_CONSTANT(VEC2, invertedViewportSize)
PUSH_CONSTANT(IVEC2, anchor)
SAMPLER(0, FLOAT_2D, metallic_tx)
SAMPLER(1, FLOAT_2D, diffuse_tx)
FRAGMENT_OUT(0, VEC4, out_color)
FRAGMENT_SOURCE("eevee_lookdev_display_frag.glsl")
DEPTH_WRITE(DepthWrite::ANY)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
