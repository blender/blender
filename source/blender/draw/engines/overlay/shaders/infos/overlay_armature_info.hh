/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_glsl_cpp_stubs.hh"

#  include "draw_object_infos_info.hh"
#  include "draw_view_info.hh"

#  include "gpu_index_load_info.hh"

#  include "overlay_common_info.hh"
#  include "overlay_shader_shared.hh"
#endif

#include "overlay_common_info.hh"

GPU_SHADER_CREATE_INFO(overlay_frag_output)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(overlay_armature_wire_iface)
FLAT(float4, final_color)
FLAT(float2, edge_start)
NO_PERSPECTIVE(float2, edge_pos)
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
STORAGE_BUF(0, read, float4x4, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_sphere_outline)

GPU_SHADER_INTERFACE_INFO(overlay_armature_sphere_solid_iface)
FLAT(float3, final_state_color)
FLAT(float3, final_bone_color)
FLAT(float4x4, sphere_matrix)
SMOOTH(float3, view_position)
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
STORAGE_BUF(0, read, float4x4, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_sphere_solid)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Shapes
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_armature_shape_outline_iface)
FLAT(float4, final_color)
FLAT(float2, edge_start)
NO_PERSPECTIVE(float2, edge_pos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_outline)
DO_STATIC_COMPILATION()
STORAGE_BUF_FREQ(0, read, float, pos[], GEOMETRY)
STORAGE_BUF(1, read, float4x4, data_buf[])
PUSH_CONSTANT(int2, gpu_attr_0)
VERTEX_OUT(overlay_armature_shape_outline_iface)
VERTEX_SOURCE("overlay_armature_shape_outline_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_shape_outline)

GPU_SHADER_INTERFACE_INFO(overlay_armature_shape_solid_iface)
SMOOTH(float4, final_color)
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
STORAGE_BUF(0, read, float4x4, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_shape_solid)

GPU_SHADER_INTERFACE_INFO(overlay_armature_shape_wire_iface)
FLAT(float4, final_color)
FLAT(float, wire_width)
NO_PERSPECTIVE(float, edge_coord)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_wire)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(bool, do_smooth_wire)
STORAGE_BUF_FREQ(0, read, float, pos[], GEOMETRY)
STORAGE_BUF(1, read, float4x4, data_buf[])
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(bool, use_arrow_drawing)
VERTEX_OUT(overlay_armature_shape_wire_iface)
VERTEX_SOURCE("overlay_armature_shape_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_shape_wire_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.hh")
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
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(0, float2, pos0)
VERTEX_IN(1, float2, pos1)
VERTEX_IN(2, float2, pos2)
VERTEX_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_envelope_outline_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, read, BoneEnvelopeData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_envelope_outline)

GPU_SHADER_INTERFACE_INFO(overlay_armature_envelope_solid_iface)
FLAT(float3, final_state_color)
FLAT(float3, final_bone_color)
SMOOTH(float3, view_normal)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_envelope_solid)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(0, float3, pos)
VERTEX_OUT(overlay_armature_envelope_solid_iface)
PUSH_CONSTANT(bool, is_distance)
VERTEX_SOURCE("overlay_armature_envelope_solid_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_envelope_solid_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, read, BoneEnvelopeData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_armature_envelope_solid)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Stick
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_armature_stick_iface)
NO_PERSPECTIVE(float, color_fac)
FLAT(float4, final_wire_color)
FLAT(float4, final_inner_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_stick_base)
TYPEDEF_SOURCE("overlay_shader_shared.hh")
/* Bone aligned screen space. */
VERTEX_IN(0, float2, pos)
VERTEX_IN(1, int, vclass)
VERTEX_OUT(overlay_armature_stick_iface)
VERTEX_SOURCE("overlay_armature_stick_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_stick_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, read, BoneStickData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_armature_stick, overlay_armature_stick_base)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Degrees of Freedom
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_dof)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(0, float2, pos)
VERTEX_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_dof_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_dof_solid_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, read, ExtraInstanceData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_armature_dof)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Wire
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_wire_base)
TYPEDEF_SOURCE("overlay_shader_shared.hh")
PUSH_CONSTANT(float, alpha)
VERTEX_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, read, VertexData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_armature_wire, overlay_armature_wire_base)

/** \} */
