/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#endif

#ifdef GLSL_CPP_STUBS
#  define MAT_TRANSPARENT
#  define MAT_RAYCAST
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Common
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_raycast)
DEFINE("MAT_RAYCAST")
SAMPLER(RAYCAST_DEPTH_TEX_SLOT, sampler2D, raycast_depth_tx)
SAMPLER(OBJECT_ID_TEX_SLOT, usampler2D, object_id_tx)
SAMPLER(PREPASS_NORMAL_TEX_SLOT, sampler2D, prepass_normal_tx)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Surface Velocity
 *
 * Combined with the depth pre-pass shader.
 * Outputs the view motion vectors for animated objects.
 * \{ */

/* Pass world space deltas to the fragment shader.
 * This is to make sure that the resulting motion vectors are valid even with displacement.
 * WARNING: The next value is invalid when rendering the viewport. */
GPU_SHADER_NAMED_INTERFACE_INFO(eevee_velocity_surface_iface, motion)
SMOOTH(float3, prev)
SMOOTH(float3, next)
GPU_SHADER_NAMED_INTERFACE_END(motion)

/* WORKAROUND: Until we get condition support for interfaces. */
GPU_SHADER_CREATE_INFO(eevee_velocity_iface_info)
VERTEX_OUT(eevee_velocity_surface_iface)
GPU_SHADER_CREATE_END()

/** \} */
