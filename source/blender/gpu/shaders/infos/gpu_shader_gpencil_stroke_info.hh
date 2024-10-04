/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_NAMED_INTERFACE_INFO(gpencil_stroke_vert_iface, geometry_in)
SMOOTH(VEC4, finalColor)
SMOOTH(FLOAT, finalThickness)
GPU_SHADER_NAMED_INTERFACE_END(geometry_in)
GPU_SHADER_NAMED_INTERFACE_INFO(gpencil_stroke_geom_iface, geometry_out)
SMOOTH(VEC4, mColor)
SMOOTH(VEC2, mTexCoord)
GPU_SHADER_NAMED_INTERFACE_END(geometry_out)

GPU_SHADER_CREATE_INFO(gpu_shader_gpencil_stroke_base)
VERTEX_IN(0, VEC4, color)
VERTEX_IN(1, VEC3, pos)
VERTEX_IN(2, FLOAT, thickness)
VERTEX_OUT(gpencil_stroke_vert_iface)
FRAGMENT_OUT(0, VEC4, fragColor)

UNIFORM_BUF(0, GPencilStrokeData, gpencil_stroke_data)

PUSH_CONSTANT(MAT4, ModelViewProjectionMatrix)
PUSH_CONSTANT(MAT4, ProjectionMatrix)
FRAGMENT_SOURCE("gpu_shader_gpencil_stroke_frag.glsl")
TYPEDEF_SOURCE("GPU_shader_shared.hh")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_gpencil_stroke)
ADDITIONAL_INFO(gpu_shader_gpencil_stroke_base)
GEOMETRY_LAYOUT(PrimitiveIn::LINES_ADJACENCY, PrimitiveOut::TRIANGLE_STRIP, 13)
GEOMETRY_OUT(gpencil_stroke_geom_iface)
VERTEX_SOURCE("gpu_shader_gpencil_stroke_vert.glsl")
GEOMETRY_SOURCE("gpu_shader_gpencil_stroke_geom.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(gpu_shader_gpencil_stroke_no_geom)
METAL_BACKEND_ONLY()
DEFINE("USE_GEOMETRY_IFACE_COLOR")
ADDITIONAL_INFO(gpu_shader_gpencil_stroke_base)
VERTEX_OUT(gpencil_stroke_geom_iface)
VERTEX_SOURCE("gpu_shader_gpencil_stroke_vert_no_geom.glsl")
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()
