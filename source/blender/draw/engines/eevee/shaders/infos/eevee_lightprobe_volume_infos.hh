/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "eevee_common_infos.hh"
#  include "eevee_debug_shared.hh"
#  include "eevee_lightprobe_infos.hh"
#  include "eevee_lightprobe_shared.hh"
#  include "eevee_uniform_infos.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define SPHERE_PROBE
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Display
 * \{ */

GPU_SHADER_INTERFACE_INFO(eevee_debug_surfel_iface)
SMOOTH(float3, P)
FLAT(int, surfel_index)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_debug_surfels)
TYPEDEF_SOURCE("draw_shader_shared.hh")
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_debug_shared.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(draw_view)
VERTEX_SOURCE("eevee_debug_surfels_vert.glsl")
VERTEX_OUT(eevee_debug_surfel_iface)
FRAGMENT_SOURCE("eevee_debug_surfels_frag.glsl")
FRAGMENT_OUT(0, float4, out_color)
STORAGE_BUF(0, read, Surfel, surfels_buf[])
PUSH_CONSTANT(float, debug_surfel_radius)
PUSH_CONSTANT(int, debug_mode)
BUILTINS(BuiltinBits::CLIP_CONTROL)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(eevee_debug_irradiance_grid_iface)
SMOOTH(float4, interp_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(eevee_debug_irradiance_grid)
TYPEDEF_SOURCE("eevee_defines.hh")
TYPEDEF_SOURCE("eevee_debug_shared.hh")
TYPEDEF_SOURCE("eevee_lightprobe_shared.hh")
ADDITIONAL_INFO(draw_view)
FRAGMENT_OUT(0, float4, out_color)
VERTEX_OUT(eevee_debug_irradiance_grid_iface)
SAMPLER(0, sampler3D, debug_data_tx)
PUSH_CONSTANT(float4x4, grid_mat)
PUSH_CONSTANT(int, debug_mode)
PUSH_CONSTANT(float, debug_value)
VERTEX_SOURCE("eevee_debug_irradiance_grid_vert.glsl")
FRAGMENT_SOURCE("eevee_debug_irradiance_grid_frag.glsl")
BUILTINS(BuiltinBits::CLIP_CONTROL)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

/** \} */
