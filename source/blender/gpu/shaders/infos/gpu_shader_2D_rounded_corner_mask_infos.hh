/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "GPU_shader_shared.hh"
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(gpu_rounded_corner_mask_iface)
/* Position relative to the center of the corner's arc, in pixels, mirrored so that the corner of
 * the rectangle is always in the positive quadrant. */
SMOOTH(float2, arc_co)
FLAT(float, arc_radius)
GPU_SHADER_INTERFACE_END()

/**
 * Draws the anti-aliased area that a rounded corner cuts away from each corner of a rectangle.
 * Uses one instance per corner.
 */
GPU_SHADER_CREATE_INFO(gpu_shader_2D_rounded_corner_mask)
VERTEX_OUT(gpu_rounded_corner_mask_iface)
FRAGMENT_OUT(0, float4, fragColor)
PUSH_CONSTANT(float4x4, ModelViewProjectionMatrix)
/* The rectangle to round the corners of, as (`xmin`, `xmax`, `ymin`, `ymax`). */
PUSH_CONSTANT(float4, rect)
PUSH_CONSTANT(float4, color)
/* The radius of each corner in pixels, counter-clockwise from the top right. A radius of zero
 * leaves that corner square. */
PUSH_CONSTANT(float4, radii)
VERTEX_SOURCE("gpu_shader_2D_rounded_corner_mask_vert.glsl")
FRAGMENT_SOURCE("gpu_shader_2D_rounded_corner_mask_frag.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
