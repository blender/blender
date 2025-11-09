/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"

#  include "gpu_index_load_infos.hh"

#  include "overlay_common_infos.hh"
#  include "overlay_shader_shared.hh"
#endif

#include "overlay_common_infos.hh"

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

/* clang-format off */
CREATE_INFO_VARIANT(overlay_armature_sphere_outline_selectable, overlay_armature_sphere_outline, overlay_select)
CREATE_INFO_VARIANT(overlay_armature_sphere_outline_clipped, overlay_armature_sphere_outline, drw_clipped)
CREATE_INFO_VARIANT(overlay_armature_sphere_outline_selectable_clipped, overlay_armature_sphere_outline_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_INTERFACE_INFO(overlay_armature_sphere_solid_iface)
FLAT(float3, final_state_color)
FLAT(float3, final_bone_color)
/* Cannot interpolate matrix. */
FLAT(float4, sphere_matrix0)
FLAT(float4, sphere_matrix1)
FLAT(float4, sphere_matrix2)
FLAT(float4, sphere_matrix3)
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

/* clang-format off */
CREATE_INFO_VARIANT(overlay_armature_sphere_solid_selectable, overlay_armature_sphere_solid, overlay_select)
CREATE_INFO_VARIANT(overlay_armature_sphere_solid_clipped, overlay_armature_sphere_solid, drw_clipped)
CREATE_INFO_VARIANT(overlay_armature_sphere_solid_selectable_clipped, overlay_armature_sphere_solid_selectable, drw_clipped)
/* clang-format on */

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

/* clang-format off */
CREATE_INFO_VARIANT(overlay_armature_shape_outline_selectable, overlay_armature_shape_outline, overlay_select)
CREATE_INFO_VARIANT(overlay_armature_shape_outline_clipped, overlay_armature_shape_outline, drw_clipped)
CREATE_INFO_VARIANT(overlay_armature_shape_outline_selectable_clipped, overlay_armature_shape_outline_selectable, drw_clipped)
/* clang-format on */

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

/* clang-format off */
CREATE_INFO_VARIANT(overlay_armature_shape_solid_selectable, overlay_armature_shape_solid, overlay_select)
CREATE_INFO_VARIANT(overlay_armature_shape_solid_clipped, overlay_armature_shape_solid, drw_clipped)
CREATE_INFO_VARIANT(overlay_armature_shape_solid_selectable_clipped, overlay_armature_shape_solid_selectable, drw_clipped)
/* clang-format on */

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

/* clang-format off */
CREATE_INFO_VARIANT(overlay_armature_shape_wire_selectable, overlay_armature_shape_wire, overlay_select)
CREATE_INFO_VARIANT(overlay_armature_shape_wire_clipped, overlay_armature_shape_wire, drw_clipped)
CREATE_INFO_VARIANT(overlay_armature_shape_wire_selectable_clipped, overlay_armature_shape_wire_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_CREATE_INFO(overlay_armature_shape_wire_strip)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_shape_wire)
DEFINE("FROM_LINE_STRIP")
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_armature_shape_wire_strip_selectable, overlay_armature_shape_wire_strip, overlay_select)
CREATE_INFO_VARIANT(overlay_armature_shape_wire_strip_clipped, overlay_armature_shape_wire_strip, drw_clipped)
CREATE_INFO_VARIANT(overlay_armature_shape_wire_strip_selectable_clipped, overlay_armature_shape_wire_strip_selectable, drw_clipped)
/* clang-format on */

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

/* clang-format off */
CREATE_INFO_VARIANT(overlay_armature_envelope_outline_selectable, overlay_armature_envelope_outline, overlay_select)
CREATE_INFO_VARIANT(overlay_armature_envelope_outline_clipped, overlay_armature_envelope_outline, drw_clipped)
CREATE_INFO_VARIANT(overlay_armature_envelope_outline_selectable_clipped, overlay_armature_envelope_outline_selectable, drw_clipped)
/* clang-format on */

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

/* clang-format off */
CREATE_INFO_VARIANT(overlay_armature_envelope_solid_selectable, overlay_armature_envelope_solid, overlay_select)
CREATE_INFO_VARIANT(overlay_armature_envelope_solid_clipped, overlay_armature_envelope_solid, drw_clipped)
CREATE_INFO_VARIANT(overlay_armature_envelope_solid_selectable_clipped, overlay_armature_envelope_solid_selectable, drw_clipped)
/* clang-format on */

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

/* clang-format off */
CREATE_INFO_VARIANT(overlay_armature_stick, overlay_armature_stick_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_armature_stick_selectable, overlay_armature_stick_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_armature_stick_clipped, overlay_armature_stick, drw_clipped)
CREATE_INFO_VARIANT(overlay_armature_stick_selectable_clipped, overlay_armature_stick_selectable, drw_clipped)
/* clang-format on */

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

CREATE_INFO_VARIANT(overlay_armature_dof_clipped, overlay_armature_dof, drw_clipped)

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

/* clang-format off */
CREATE_INFO_VARIANT(overlay_armature_wire, overlay_armature_wire_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_armature_wire_selectable, overlay_armature_wire_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_armature_wire_clipped, overlay_armature_wire, drw_clipped)
CREATE_INFO_VARIANT(overlay_armature_wire_selectable_clipped, overlay_armature_wire_selectable, drw_clipped)
/* clang-format on */

/** \} */
