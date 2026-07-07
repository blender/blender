/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"

#  include "gpu_index_load_infos.hh"

#  include "overlay_shader_shared.hh"
#endif

#include "overlay_common_infos.hh"

/* -------------------------------------------------------------------- */
/** \name Extra shapes
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_iface)
NO_PERSPECTIVE(float2, edge_pos)
FLAT(float2, edge_start)
FLAT(float4, final_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, int, vclass)
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_extra_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, read, ExtraInstanceData, data_buf[])
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_extra_selectable, overlay_extra, overlay_select)
CREATE_INFO_VARIANT(overlay_extra_clipped, overlay_extra, drw_clipped)
CREATE_INFO_VARIANT(overlay_extra_selectable_clipped, overlay_extra_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_CREATE_INFO(overlay_extra_spot_cone)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_extra)
DEFINE("IS_SPOT_CONE")
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_extra_spot_cone_clipped, overlay_extra_spot_cone, drw_clipped)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Irradiance Grid
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_grid_iface)
FLAT(float4, final_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_grid_base)
SAMPLER(0, sampler2DDepth, depth_buffer)
PUSH_CONSTANT(float4x4, grid_model_matrix)
PUSH_CONSTANT(bool, is_transform)
VERTEX_OUT(overlay_extra_grid_iface)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_extra_lightprobe_grid_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_extra_grid, overlay_extra_grid_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_extra_grid_selectable, overlay_extra_grid_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_extra_grid_clipped, overlay_extra_grid, drw_clipped)
CREATE_INFO_VARIANT(overlay_extra_grid_selectable_clipped, overlay_extra_grid_selectable, drw_clipped)
/* clang-format on */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ground-lines
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_extra_groundline)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
/* Instance attributes. */
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_extra_groundline_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, read, float4, data_buf[])
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_extra_groundline_selectable, overlay_extra_groundline, overlay_select)
CREATE_INFO_VARIANT(overlay_extra_groundline_clipped, overlay_extra_groundline, drw_clipped)
CREATE_INFO_VARIANT(overlay_extra_groundline_selectable_clipped, overlay_extra_groundline_selectable, drw_clipped)
/* clang-format on */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extra wires
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_wire_iface)
NO_PERSPECTIVE(float2, stipple_coord)
FLAT(float2, stipple_start)
FLAT(float4, final_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_wire_base)
VERTEX_OUT(overlay_extra_wire_iface)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_extra_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_wire_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.hh")
STORAGE_BUF(0, read, VertexData, data_buf[])
PUSH_CONSTANT(int, colorid)
DEFINE_VALUE("pos", "data_buf[gl_VertexID].pos_.xyz")
DEFINE_VALUE("color", "data_buf[gl_VertexID].color_")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_extra_wire, overlay_extra_wire_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_extra_wire_selectable, overlay_extra_wire_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_extra_wire_clipped, overlay_extra_wire, drw_clipped)
CREATE_INFO_VARIANT(overlay_extra_wire_selectable_clipped, overlay_extra_wire_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_CREATE_INFO(overlay_extra_wire_object_base)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float4, color)
/* If colorid is equal to 0 (i.e: Not specified) use color attribute and stippling. */
VERTEX_IN(2, int, colorid)
VERTEX_OUT(overlay_extra_wire_iface)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_extra_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_wire_frag.glsl")
DEFINE("OBJECT_WIRE")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_extra_wire_object, overlay_extra_wire_object_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_extra_wire_object_selectable, overlay_extra_wire_object_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_extra_wire_object_clipped, overlay_extra_wire_object, drw_clipped)
CREATE_INFO_VARIANT(overlay_extra_wire_object_selectable_clipped, overlay_extra_wire_object_selectable, drw_clipped)
/* clang-format on */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extra points
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_point_iface)
FLAT(float4, radii)
FLAT(float4, fill_color)
FLAT(float4, outline_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_point_base)
/* TODO(fclem): Move the vertex shader to Overlay engine and remove this bypass. */
DEFINE_VALUE("blender_srgb_to_framebuffer_space(a)", "a")
VERTEX_OUT(overlay_extra_point_iface)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_extra_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_varying_outline_aa_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
TYPEDEF_SOURCE("overlay_shader_shared.hh")
STORAGE_BUF(0, read, VertexData, data_buf[])
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_extra_point, overlay_extra_point_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_extra_point_selectable, overlay_extra_point_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_extra_point_clipped, overlay_extra_point, drw_clipped)
CREATE_INFO_VARIANT(overlay_extra_point_selectable_clipped, overlay_extra_point_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_INTERFACE_INFO(overlay_extra_loose_point_iface)
SMOOTH(float4, final_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_loose_point_base)
VERTEX_OUT(overlay_extra_loose_point_iface)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_extra_loose_point_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_loose_point_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.hh")
STORAGE_BUF(0, read, VertexData, data_buf[])
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_extra_loose_point, overlay_extra_loose_point_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_extra_loose_point_selectable, overlay_extra_loose_point_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_extra_loose_point_clipped, overlay_extra_loose_point, drw_clipped)
CREATE_INFO_VARIANT(overlay_extra_loose_point_selectable_clipped, overlay_extra_loose_point_selectable, drw_clipped)
/* clang-format on */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Motion Path
 * \{ */

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_motion_path_line_iface, interp)
SMOOTH(float4, color)
GPU_SHADER_NAMED_INTERFACE_END(interp)

GPU_SHADER_CREATE_INFO(overlay_motion_path_line)
DO_STATIC_COMPILATION()
STORAGE_BUF_FREQ(0, read, float, pos[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int, gpu_attr_0_len) /* Avoid a warning on Metal. */
PUSH_CONSTANT(int4, mpath_line_settings)
PUSH_CONSTANT(bool, selected)
PUSH_CONSTANT(float3, custom_color_pre)
PUSH_CONSTANT(float3, custom_color_post)
PUSH_CONSTANT(int, line_thickness) /* In pixels. */
PUSH_CONSTANT(float4x4, camera_space_matrix)
VERTEX_OUT(overlay_motion_path_line_iface)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_motion_path_line_vert.glsl")
FRAGMENT_SOURCE("overlay_motion_path_line_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_motion_path_line_clipped, overlay_motion_path_line, drw_clipped)

GPU_SHADER_INTERFACE_INFO(overlay_motion_path_point_iface)
FLAT(float4, final_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_motion_path_point)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, int, flag)
PUSH_CONSTANT(int4, mpath_point_settings)
PUSH_CONSTANT(bool, show_key_frames)
PUSH_CONSTANT(float3, custom_color_pre)
PUSH_CONSTANT(float3, custom_color_post)
PUSH_CONSTANT(float4x4, camera_space_matrix)
VERTEX_OUT(overlay_motion_path_point_iface)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_motion_path_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_motion_path_point_clipped, overlay_motion_path_point, drw_clipped)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Empty
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_image_iface)
SMOOTH(float2, uvs)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_image_base)
PUSH_CONSTANT(bool, depth_set)
PUSH_CONSTANT(bool, is_camera_background)
PUSH_CONSTANT(bool, img_premultiplied)
PUSH_CONSTANT(bool, img_alpha_blend)
PUSH_CONSTANT(float4, ucolor)
VERTEX_IN(0, float3, pos)
VERTEX_OUT(overlay_image_iface)
SAMPLER(0, sampler2D, img_tx)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_image_vert.glsl")
FRAGMENT_SOURCE("overlay_image_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_image, overlay_image_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_image_selectable, overlay_image_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_image_clipped, overlay_image, drw_clipped)
CREATE_INFO_VARIANT(overlay_image_selectable_clipped, overlay_image_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_CREATE_INFO(overlay_image_depth_bias_base)
ADDITIONAL_INFO(overlay_image_base)
DEFINE("DEPTH_BIAS")
PUSH_CONSTANT(float4x4, depth_bias_winmat)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_image_depth_bias, overlay_image_depth_bias_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_image_depth_bias_selectable, overlay_image_depth_bias_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_image_depth_bias_clipped, overlay_image_depth_bias, drw_clipped)
CREATE_INFO_VARIANT(overlay_image_depth_bias_selectable_clipped, overlay_image_depth_bias_selectable, drw_clipped)
/* clang-format on */

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPencil Canvas
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_gpencil_canvas)
DO_STATIC_COMPILATION()
VERTEX_OUT(overlay_extra_iface)
PUSH_CONSTANT(float4, color)
PUSH_CONSTANT(float3, axis_x)
PUSH_CONSTANT(float3, axis_y)
PUSH_CONSTANT(float3, origin)
PUSH_CONSTANT(int, half_line_count)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_edit_gpencil_canvas_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_frag.glsl")
ADDITIONAL_INFO(draw_mesh)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_gpencil_canvas_clipped, overlay_gpencil_canvas, drw_clipped)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particle
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_particle_iface)
FLAT(float4, final_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_particle_dot_base)
SAMPLER(0, sampler1D, weight_tx)
PUSH_CONSTANT(float4, ucolor) /* Draw-size packed in alpha. */
VERTEX_IN(0, float3, part_pos)
VERTEX_IN(1, float4, part_rot)
VERTEX_IN(2, float, part_val)
VERTEX_OUT(overlay_particle_iface)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_particle_vert.glsl")
FRAGMENT_SOURCE("overlay_particle_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_particle_dot, overlay_particle_dot_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_particle_dot_selectable, overlay_particle_dot_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_particle_dot_clipped, overlay_particle_dot, drw_clipped)
CREATE_INFO_VARIANT(overlay_particle_dot_selectable_clipped, overlay_particle_dot_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_CREATE_INFO(overlay_particle_shape_base)
TYPEDEF_SOURCE("overlay_shader_shared.hh")
SAMPLER(0, sampler1D, weight_tx)
PUSH_CONSTANT(float4, ucolor) /* Draw-size packed in alpha. */
PUSH_CONSTANT(int, shape_type)
/* Use first attribute to only bind one buffer. */
STORAGE_BUF_FREQ(0, read, ParticlePointData, part_pos[], GEOMETRY)
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_particle_shape_vert.glsl")
FRAGMENT_SOURCE("overlay_particle_shape_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_particle_shape, overlay_particle_shape_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_particle_shape_selectable, overlay_particle_shape_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_particle_shape_clipped, overlay_particle_shape, drw_clipped)
CREATE_INFO_VARIANT(overlay_particle_shape_selectable_clipped, overlay_particle_shape_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_CREATE_INFO(overlay_particle_hair_base)
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float3, nor)
PUSH_CONSTANT(int, color_type)
PUSH_CONSTANT(bool, is_transform)
PUSH_CONSTANT(bool, use_coloring)
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_particle_hair_vert.glsl")
FRAGMENT_SOURCE("overlay_particle_shape_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_particle_hair, overlay_particle_hair_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_particle_hair_selectable, overlay_particle_hair_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_particle_hair_clipped, overlay_particle_hair, drw_clipped)
CREATE_INFO_VARIANT(overlay_particle_hair_selectable_clipped, overlay_particle_hair_selectable, drw_clipped)
/* clang-format on */

/** \} */
