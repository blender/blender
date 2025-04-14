/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_common_shader_shared.hh"
#  include "draw_object_infos_info.hh"
#  include "draw_view_info.hh"

#  include "gpu_index_load_info.hh"

#  include "overlay_common_info.hh"
#  include "overlay_shader_shared.h"
#endif

#include "overlay_common_info.hh"

GPU_SHADER_CREATE_INFO(overlay_frag_output)
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(overlay_armature_wire_iface)
FLAT(float4, finalColor)
FLAT(float2, edgeStart)
NO_PERSPECTIVE(float2, edgePos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_common)
PUSH_CONSTANT(float, alpha)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

/* -------------------------------------------------------------------- */
/** \name Armature Sphere
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_sphere_outline)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float2, pos)
VERTEX_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_sphere_outline_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, READ, float4x4, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_sphere_outline)

GPU_SHADER_INTERFACE_INFO(overlay_armature_sphere_solid_iface)
FLAT(float3, finalStateColor)
FLAT(float3, finalBoneColor)
FLAT(float4x4, sphereMatrix)
SMOOTH(float3, viewPosition)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_sphere_solid)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float2, pos)
/* Per instance. */
VERTEX_IN(1, float4, color)
DEPTH_WRITE(DepthWrite::GREATER)
VERTEX_OUT(overlay_armature_sphere_solid_iface)
VERTEX_SOURCE("overlay_armature_sphere_solid_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_sphere_solid_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, READ, float4x4, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_sphere_solid)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Shapes
 * \{ */

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_armature_shape_outline_iface, geom_in)
SMOOTH(float4, pPos)
SMOOTH(float3, vPos)
SMOOTH(float2, ssPos)
SMOOTH(float4, vColSize)
GPU_SHADER_NAMED_INTERFACE_END(geom_in)
GPU_SHADER_NAMED_INTERFACE_INFO(overlay_armature_shape_outline_flat_iface, geom_flat_in)
FLAT(int, inverted)
GPU_SHADER_NAMED_INTERFACE_END(geom_flat_in)

GPU_SHADER_INTERFACE_INFO(overlay_armature_shape_outline_no_geom_iface)
FLAT(float4, finalColor)
FLAT(float2, edgeStart)
NO_PERSPECTIVE(float2, edgePos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_outline)
DO_STATIC_COMPILATION()
STORAGE_BUF_FREQ(0, READ, float, pos[], GEOMETRY)
STORAGE_BUF(1, READ, float4x4, data_buf[])
PUSH_CONSTANT(int2, gpu_attr_0)
VERTEX_OUT(overlay_armature_shape_outline_no_geom_iface)
VERTEX_SOURCE("overlay_armature_shape_outline_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_shape_outline)

GPU_SHADER_INTERFACE_INFO(overlay_armature_shape_solid_iface)
SMOOTH(float4, finalColor)
FLAT(int, inverted)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_solid)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float3, nor)
/* Per instance. */
DEPTH_WRITE(DepthWrite::GREATER)
VERTEX_OUT(overlay_armature_shape_solid_iface)
VERTEX_SOURCE("overlay_armature_shape_solid_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_shape_solid_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, READ, float4x4, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_shape_solid)

GPU_SHADER_INTERFACE_INFO(overlay_armature_shape_wire_iface)
FLAT(float4, finalColor)
FLAT(float, wire_width)
NO_PERSPECTIVE(float, edgeCoord)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_wire)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(bool, do_smooth_wire)
STORAGE_BUF_FREQ(0, READ, float, pos[], GEOMETRY)
STORAGE_BUF(1, READ, float4x4, data_buf[])
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(bool, use_arrow_drawing)
VERTEX_OUT(overlay_armature_shape_wire_iface)
VERTEX_SOURCE("overlay_armature_shape_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_shape_wire_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.h")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_shape_wire)

GPU_SHADER_CREATE_INFO(overlay_armature_shape_wire_strip)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_shape_wire)
DEFINE("FROM_LINE_STRIP")
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_shape_wire_strip)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Envelope
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_envelope_outline)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.h")
VERTEX_IN(0, float2, pos0)
VERTEX_IN(1, float2, pos1)
VERTEX_IN(2, float2, pos2)
VERTEX_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_envelope_outline_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, READ, BoneEnvelopeData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_envelope_outline)

GPU_SHADER_INTERFACE_INFO(overlay_armature_envelope_solid_iface)
FLAT(float3, finalStateColor)
FLAT(float3, finalBoneColor)
SMOOTH(float3, normalView)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_envelope_solid)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.h")
VERTEX_IN(0, float3, pos)
VERTEX_OUT(overlay_armature_envelope_solid_iface)
PUSH_CONSTANT(bool, isDistance)
VERTEX_SOURCE("overlay_armature_envelope_solid_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_envelope_solid_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, READ, BoneEnvelopeData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_envelope_solid)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Stick
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_armature_stick_iface)
NO_PERSPECTIVE(float, colorFac)
FLAT(float4, finalWireColor)
FLAT(float4, finalInnerColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_stick_base)
TYPEDEF_SOURCE("overlay_shader_shared.h")
/* Bone aligned screen space. */
VERTEX_IN(0, float2, pos)
VERTEX_IN(1, int, vclass)
VERTEX_OUT(overlay_armature_stick_iface)
VERTEX_SOURCE("overlay_armature_stick_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_stick_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, READ, BoneStickData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_armature_stick, overlay_armature_stick_base)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Degrees of Freedom
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_dof)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.h")
VERTEX_IN(0, float2, pos)
VERTEX_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_dof_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_dof_solid_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, READ, ExtraInstanceData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_armature_dof)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Wire
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_wire_base)
TYPEDEF_SOURCE("overlay_shader_shared.h")
PUSH_CONSTANT(float, alpha)
VERTEX_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, READ, VertexData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_armature_wire, overlay_armature_wire_base)

/** \} */
