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

#  define CURVES_SHADER
#  define DRW_HAIR_INFO

#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO
#endif

#include "overlay_common_infos.hh"

GPU_SHADER_INTERFACE_INFO(overlay_edit_flat_wire_iface)
NO_PERSPECTIVE(float2, edge_pos)
FLAT(float2, edge_start)
FLAT(float4, final_color)
GPU_SHADER_INTERFACE_END()
GPU_SHADER_INTERFACE_INFO(overlay_edit_flat_color_iface)
FLAT(float4, final_color)
GPU_SHADER_INTERFACE_END()
GPU_SHADER_INTERFACE_INFO(overlay_edit_smooth_color_iface)
SMOOTH(float4, final_color)
GPU_SHADER_INTERFACE_END()
GPU_SHADER_INTERFACE_INFO(overlay_edit_nopersp_color_iface)
NO_PERSPECTIVE(float4, final_color)
GPU_SHADER_INTERFACE_END()

/* -------------------------------------------------------------------- */
/** \name Edit Mesh
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_common)
DEFINE_VALUE("blender_srgb_to_framebuffer_space(a)", "a")
SAMPLER(0, sampler2DDepth, depth_tx)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
/* Per view factor. */
PUSH_CONSTANT(float, ndc_offset_factor)
/* Per pass factor. */
PUSH_CONSTANT(float, ndc_offset)
PUSH_CONSTANT(bool, wire_shading)
PUSH_CONSTANT(bool, select_face)
PUSH_CONSTANT(bool, select_edge)
PUSH_CONSTANT(float, alpha)
PUSH_CONSTANT(float, retopology_offset)
PUSH_CONSTANT(int4, data_mask)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_depth)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
PUSH_CONSTANT(float, retopology_offset)
VERTEX_SOURCE("overlay_edit_mesh_depth_vert.glsl")
FRAGMENT_SOURCE("overlay_depth_only_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_mesh_depth_clipped, overlay_edit_mesh_depth, drw_clipped)

GPU_SHADER_INTERFACE_INFO(overlay_edit_mesh_vert_iface)
SMOOTH(float4, final_color)
SMOOTH(float, vertex_crease)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_vert)
DO_STATIC_COMPILATION()
BUILTINS(BuiltinBits::POINT_SIZE)
DEFINE("VERT")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, uint4, data)
VERTEX_IN(2, float3, vnor)
VERTEX_SOURCE("overlay_edit_mesh_vert.glsl")
VERTEX_OUT(overlay_edit_mesh_vert_iface)
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(overlay_edit_mesh_common)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_mesh_vert_clipped, overlay_edit_mesh_vert, drw_clipped)

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_edit_mesh_edge_geom_iface, geometry_out)
SMOOTH(float4, final_color)
GPU_SHADER_NAMED_INTERFACE_END(geometry_out)
GPU_SHADER_NAMED_INTERFACE_INFO(overlay_edit_mesh_edge_geom_flat_iface, geometry_flat_out)
FLAT(float4, final_color_outer)
GPU_SHADER_NAMED_INTERFACE_END(geometry_flat_out)
GPU_SHADER_NAMED_INTERFACE_INFO(overlay_edit_mesh_edge_geom_noperspective_iface,
                                geometry_noperspective_out)
NO_PERSPECTIVE(float, edge_coord)
GPU_SHADER_NAMED_INTERFACE_END(geometry_noperspective_out)

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_edge)
DO_STATIC_COMPILATION()
DEFINE("EDGE")
STORAGE_BUF_FREQ(0, read, float, pos[], GEOMETRY)
STORAGE_BUF_FREQ(1, read, uint, vnor[], GEOMETRY)
STORAGE_BUF_FREQ(2, read, uint, data[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
PUSH_CONSTANT(int2, gpu_attr_2)
PUSH_CONSTANT(bool, do_smooth_wire)
PUSH_CONSTANT(bool, use_vertex_selection)
VERTEX_OUT(overlay_edit_mesh_edge_geom_iface)
VERTEX_OUT(overlay_edit_mesh_edge_geom_flat_iface)
VERTEX_OUT(overlay_edit_mesh_edge_geom_noperspective_iface)
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_SOURCE("overlay_edit_mesh_edge_vert.glsl")
FRAGMENT_SOURCE("overlay_edit_mesh_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(overlay_edit_mesh_common)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_mesh_edge_clipped, overlay_edit_mesh_edge, drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_face)
DO_STATIC_COMPILATION()
DEFINE("FACE")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, uint4, data)
VERTEX_SOURCE("overlay_edit_mesh_vert.glsl")
VERTEX_OUT(overlay_edit_flat_color_iface)
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(overlay_edit_mesh_common)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_mesh_face_clipped, overlay_edit_mesh_face, drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_facedot)
DO_STATIC_COMPILATION()
DEFINE("FACEDOT")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, uint4, data)
VERTEX_IN(2, float4, norAndFlag)
VERTEX_SOURCE("overlay_edit_mesh_facedot_vert.glsl")
VERTEX_OUT(overlay_edit_flat_color_iface)
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(overlay_edit_mesh_common)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_mesh_facedot_clipped, overlay_edit_mesh_facedot, drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_normal)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
SAMPLER(0, sampler2DDepth, depth_tx)
PUSH_CONSTANT(float, normal_size)
PUSH_CONSTANT(float, normal_screen_size)
PUSH_CONSTANT(float, alpha)
PUSH_CONSTANT(bool, is_constant_screen_size_normals)
VERTEX_OUT(overlay_edit_flat_color_iface)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_edit_mesh_normal_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_mesh_face_normal)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_edit_mesh_normal)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(1, read, float, pos[], GEOMETRY)
DEFINE("FACE_NORMAL")
PUSH_CONSTANT(bool, hq_normals)
STORAGE_BUF_FREQ(0, read, uint, norAndFlag[], GEOMETRY)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_mesh_face_normal_clipped, overlay_mesh_face_normal, drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_mesh_face_normal_subdiv)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_edit_mesh_normal)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(1, read, float, pos[], GEOMETRY)
DEFINE("FACE_NORMAL")
DEFINE("FLOAT_NORMAL")
STORAGE_BUF_FREQ(0, read, float4, norAndFlag[], GEOMETRY)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_mesh_face_normal_subdiv_clipped,
                    overlay_mesh_face_normal_subdiv,
                    drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_mesh_loop_normal)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_edit_mesh_normal)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(1, read, float, pos[], GEOMETRY)
DEFINE("LOOP_NORMAL")
PUSH_CONSTANT(bool, hq_normals)
STORAGE_BUF_FREQ(0, read, uint, lnor[], GEOMETRY)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_mesh_loop_normal_clipped, overlay_mesh_loop_normal, drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_mesh_loop_normal_subdiv)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_edit_mesh_normal)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(1, read, float, pos[], GEOMETRY)
DEFINE("LOOP_NORMAL")
DEFINE("FLOAT_NORMAL")
STORAGE_BUF_FREQ(0, read, float4, lnor[], GEOMETRY)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_mesh_loop_normal_subdiv_clipped,
                    overlay_mesh_loop_normal_subdiv,
                    drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_mesh_vert_normal)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_edit_mesh_normal)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(1, read, float, pos[], GEOMETRY)
DEFINE("VERT_NORMAL")
STORAGE_BUF_FREQ(0, read, uint, vnor[], GEOMETRY)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_mesh_vert_normal_clipped, overlay_mesh_vert_normal, drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_mesh_vert_normal_subdiv)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_edit_mesh_normal)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(1, read, float, pos[], GEOMETRY)
DEFINE("VERT_NORMAL")
DEFINE("FLOAT_NORMAL")
STORAGE_BUF_FREQ(0, read, float, vnor[], GEOMETRY)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_mesh_vert_normal_subdiv_clipped,
                    overlay_mesh_vert_normal_subdiv,
                    drw_clipped)

GPU_SHADER_INTERFACE_INFO(overlay_edit_mesh_analysis_iface)
SMOOTH(float4, weight_color)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_analysis)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float, weight)
SAMPLER(0, sampler1D, weight_tx)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_OUT(overlay_edit_mesh_analysis_iface)
VERTEX_SOURCE("overlay_edit_mesh_analysis_vert.glsl")
FRAGMENT_SOURCE("overlay_edit_mesh_analysis_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_mesh_analysis_clipped, overlay_edit_mesh_analysis, drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_skin_root)
DO_STATIC_COMPILATION()
VERTEX_OUT(overlay_edit_flat_color_iface)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_edit_mesh_skin_root_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
/* TODO(fclem): Use correct vertex format. For now we read the format manually. */
STORAGE_BUF_FREQ(0, read, float, size[], GEOMETRY)
DEFINE("VERTEX_PULL")
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_mesh_skin_root_clipped, overlay_edit_mesh_skin_root, drw_clipped)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit UV
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_edit_uv_iface)
SMOOTH(float, selection_fac)
FLAT(float2, stipple_start)
NO_PERSPECTIVE(float, edge_coord)
NO_PERSPECTIVE(float2, stipple_pos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_edges)
DO_STATIC_COMPILATION()
STORAGE_BUF_FREQ(0, read, float, au[], GEOMETRY)
STORAGE_BUF_FREQ(1, read, uint, data[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
PUSH_CONSTANT(int, line_style)
PUSH_CONSTANT(bool, do_smooth_wire)
PUSH_CONSTANT(float, alpha)
PUSH_CONSTANT(float, dash_length)
SPECIALIZATION_CONSTANT(bool, use_edge_select, false)
VERTEX_OUT(overlay_edit_uv_iface)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_edit_uv_edges_vert.glsl")
FRAGMENT_SOURCE("overlay_edit_uv_edges_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.hh")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_faces)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float2, au)
VERTEX_IN(1, uint, flag)
PUSH_CONSTANT(float, uv_opacity)
VERTEX_OUT(overlay_edit_flat_color_iface)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_edit_uv_faces_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_resource_id_varying)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_face_dots)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float2, au)
VERTEX_IN(1, uint, flag)
PUSH_CONSTANT(float, dot_size)
VERTEX_OUT(overlay_edit_flat_color_iface)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_edit_uv_face_dots_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(overlay_edit_uv_vert_iface)
SMOOTH(float4, fill_color)
SMOOTH(float4, outline_color)
SMOOTH(float4, radii)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_verts)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float2, au)
VERTEX_IN(1, uint, flag)
PUSH_CONSTANT(float, dot_size)
PUSH_CONSTANT(float, outline_width)
PUSH_CONSTANT(float4, color)
VERTEX_OUT(overlay_edit_uv_vert_iface)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_edit_uv_verts_vert.glsl")
FRAGMENT_SOURCE("overlay_edit_uv_verts_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_tiled_image_borders)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
PUSH_CONSTANT(float4, ucolor)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_edit_uv_tiled_image_borders_vert.glsl")
FRAGMENT_SOURCE("overlay_uniform_color_frag.glsl")
PUSH_CONSTANT(float3, tile_pos)
DEFINE_VALUE("tile_scale", "float3(1.0f)")
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(edit_uv_image_iface)
SMOOTH(float2, uvs)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_stencil_image)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_OUT(edit_uv_image_iface)
VERTEX_SOURCE("overlay_edit_uv_image_vert.glsl")
SAMPLER(0, sampler2D, img_tx)
PUSH_CONSTANT(bool, img_premultiplied)
PUSH_CONSTANT(bool, img_alpha_blend)
PUSH_CONSTANT(float4, ucolor)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_SOURCE("overlay_image_frag.glsl")
PUSH_CONSTANT(float2, brush_offset)
PUSH_CONSTANT(float2, brush_scale)
ADDITIONAL_INFO(draw_view);
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_mask_image)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_OUT(edit_uv_image_iface)
SAMPLER(0, sampler2D, img_tx)
PUSH_CONSTANT(float4, color)
PUSH_CONSTANT(float, opacity)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_edit_uv_image_vert.glsl")
FRAGMENT_SOURCE("overlay_edit_uv_image_mask_frag.glsl")
PUSH_CONSTANT(float2, brush_offset)
PUSH_CONSTANT(float2, brush_scale)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name UV Stretching
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_edit_uv_stretching)
VERTEX_IN(0, float2, pos)
PUSH_CONSTANT(float2, aspect)
PUSH_CONSTANT(float, stretch_opacity)
VERTEX_OUT(overlay_edit_nopersp_color_iface)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_edit_uv_stretching_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_stretching_area)
DO_STATIC_COMPILATION()
VERTEX_IN(1, float, ratio)
PUSH_CONSTANT(float, total_area_ratio)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(overlay_edit_uv_stretching)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_stretching_angle)
DO_STATIC_COMPILATION()
DEFINE("STRETCH_ANGLE")
VERTEX_IN(1, float2, uv_angles)
VERTEX_IN(2, float, angle)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(overlay_edit_uv_stretching)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Curve
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_edit_curve_handle)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
STORAGE_BUF_FREQ(0, read, float, pos[], GEOMETRY)
STORAGE_BUF_FREQ(1, read, uint, data[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
VERTEX_OUT(overlay_edit_smooth_color_iface)
PUSH_CONSTANT(bool, show_curve_handles)
PUSH_CONSTANT(int, curve_handle_display)
PUSH_CONSTANT(float, alpha)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_edit_curve_handle_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_curve_handle_clipped, overlay_edit_curve_handle, drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_edit_curve_point)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, uint, data)
VERTEX_OUT(overlay_edit_flat_color_iface)
PUSH_CONSTANT(bool, show_curve_handles)
PUSH_CONSTANT(int, curve_handle_display)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_edit_curve_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_curve_point_clipped, overlay_edit_curve_point, drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_edit_curve_wire)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float3, nor)
VERTEX_IN(2, float3, tangent)
VERTEX_IN(3, float, rad)
PUSH_CONSTANT(float, normal_size)
VERTEX_OUT(overlay_edit_flat_wire_iface)
DEFINE("LINE_OUTPUT_NO_DUMMY") /* TODO(fclem): Should be the default. */
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_edit_curve_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_curve_wire_clipped, overlay_edit_curve_wire, drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_edit_curve_normals)
DO_STATIC_COMPILATION()
STORAGE_BUF_FREQ(0, read, float, pos[], GEOMETRY)
STORAGE_BUF_FREQ(1, read, float, rad[], GEOMETRY)
STORAGE_BUF_FREQ(2, read, uint, nor[], GEOMETRY)
STORAGE_BUF_FREQ(3, read, uint, tangent[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
PUSH_CONSTANT(int2, gpu_attr_2)
PUSH_CONSTANT(int2, gpu_attr_3)
PUSH_CONSTANT(float, normal_size)
PUSH_CONSTANT(bool, use_hq_normals)
VERTEX_OUT(overlay_edit_flat_color_iface)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_edit_curve_normals_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_curve_normals_clipped, overlay_edit_curve_normals, drw_clipped)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Curves
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_edit_curves_handle)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
STORAGE_BUF_FREQ(0, read, float, pos[], GEOMETRY)
STORAGE_BUF_FREQ(1, read, uint, data[], GEOMETRY)
STORAGE_BUF_FREQ(2, read, float, selection[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
PUSH_CONSTANT(int2, gpu_attr_2)
VERTEX_OUT(overlay_edit_smooth_color_iface)
PUSH_CONSTANT(int, curve_handle_display)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_edit_curves_handle_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_curves_handle_clipped, overlay_edit_curves_handle, drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_edit_curves_point)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
DEFINE("CURVES_POINT")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, uint, data)
VERTEX_IN(2, float, selection)
#if 1 /* TODO(fclem): Required for legacy gpencil overlay. To be moved to specialized shader. */
TYPEDEF_SOURCE("gpencil_shader_shared.hh")
VERTEX_IN(3, uint, vflag)
PUSH_CONSTANT(bool, do_stroke_endpoints)
#endif
VERTEX_OUT(overlay_edit_flat_color_iface)
SAMPLER(0, sampler1D, weight_tx)
PUSH_CONSTANT(bool, use_weight)
PUSH_CONSTANT(bool, use_grease_pencil)
PUSH_CONSTANT(int, curve_handle_display)
FRAGMENT_OUT(0, float4, frag_color)
VERTEX_SOURCE("overlay_edit_particle_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_curves_point_clipped, overlay_edit_curves_point, drw_clipped)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Lattice
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_edit_lattice_point)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, uint, data)
VERTEX_OUT(overlay_edit_flat_color_iface)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_edit_lattice_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_modelmat)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_lattice_point_clipped, overlay_edit_lattice_point, drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_edit_lattice_wire)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float, weight)
SAMPLER(0, sampler1D, weight_tx)
VERTEX_OUT(overlay_edit_smooth_color_iface)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_edit_lattice_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_modelmat)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_lattice_wire_clipped, overlay_edit_lattice_wire, drw_clipped)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Particle
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_edit_particle_strand)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float, selection)
SAMPLER(0, sampler1D, weight_tx)
PUSH_CONSTANT(bool, use_weight)
PUSH_CONSTANT(bool, use_grease_pencil)
VERTEX_OUT(overlay_edit_smooth_color_iface)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
DEFINE("LINE_OUTPUT")
VERTEX_SOURCE("overlay_edit_particle_strand_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_particle_strand_clipped,
                    overlay_edit_particle_strand,
                    drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_edit_particle_point)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float, selection)
VERTEX_OUT(overlay_edit_flat_color_iface)
SAMPLER(0, sampler1D, weight_tx)
PUSH_CONSTANT(bool, use_weight)
PUSH_CONSTANT(bool, use_grease_pencil)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
DEFINE("LINE_OUTPUT")
#if 1 /* TODO(fclem): Required for legacy gpencil overlay. To be moved to specialized shader. */
TYPEDEF_SOURCE("gpencil_shader_shared.hh")
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(3, uint, vflag)
PUSH_CONSTANT(bool, do_stroke_endpoints)
#endif
VERTEX_SOURCE("overlay_edit_particle_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_particle_point_clipped, overlay_edit_particle_point, drw_clipped)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit PointCloud
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_edit_pointcloud)
VERTEX_IN(0, float4, pos_rad)
VERTEX_OUT(overlay_edit_flat_color_iface)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_edit_pointcloud_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_modelmat)
DO_STATIC_COMPILATION()
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_edit_pointcloud_clipped, overlay_edit_pointcloud, drw_clipped)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Depth Only Shader
 *
 * Used to occlude edit geometry which might not be rendered by the render engine.
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_depth_mesh_base)
VERTEX_IN(0, float3, pos)
VERTEX_SOURCE("overlay_depth_only_vert.glsl")
FRAGMENT_SOURCE("overlay_depth_only_frag.glsl")
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_depth_mesh, overlay_depth_mesh_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_depth_mesh_selectable, overlay_depth_mesh_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_depth_mesh_clipped, overlay_depth_mesh, drw_clipped)
CREATE_INFO_VARIANT(overlay_depth_mesh_selectable_clipped, overlay_depth_mesh_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_CREATE_INFO(overlay_depth_mesh_conservative_base)
STORAGE_BUF_FREQ(0, read, float, pos[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
VERTEX_SOURCE("overlay_depth_only_mesh_conservative_vert.glsl")
FRAGMENT_SOURCE("overlay_depth_only_frag.glsl")
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(gpu_index_buffer_load)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_depth_mesh_conservative, overlay_depth_mesh_conservative_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_depth_mesh_conservative_selectable, overlay_depth_mesh_conservative_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_depth_mesh_conservative_clipped, overlay_depth_mesh_conservative, drw_clipped)
CREATE_INFO_VARIANT(overlay_depth_mesh_conservative_selectable_clipped, overlay_depth_mesh_conservative_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_depth_only_gpencil_flat_iface, gp_interp_flat)
FLAT(float2, aspect)
FLAT(float4, sspos)
FLAT(float4, sspos_adj)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp_flat)
GPU_SHADER_NAMED_INTERFACE_INFO(overlay_depth_only_gpencil_noperspective_iface,
                                gp_interp_noperspective)
NO_PERSPECTIVE(float4, thickness)
NO_PERSPECTIVE(float, hardness)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp_noperspective)

GPU_SHADER_CREATE_INFO(overlay_depth_gpencil_base)
TYPEDEF_SOURCE("gpencil_shader_shared.hh")
VERTEX_OUT(overlay_depth_only_gpencil_flat_iface)
VERTEX_OUT(overlay_depth_only_gpencil_noperspective_iface)
VERTEX_SOURCE("overlay_depth_only_gpencil_vert.glsl")
FRAGMENT_SOURCE("overlay_depth_only_gpencil_frag.glsl")
DEPTH_WRITE(DepthWrite::ANY)
PUSH_CONSTANT(bool, gp_stroke_order3d) /* TODO(fclem): Move to a GPencil object UBO. */
PUSH_CONSTANT(float4, gp_depth_plane)  /* TODO(fclem): Move to a GPencil object UBO. */
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_gpencil)
ADDITIONAL_INFO(draw_object_infos)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_depth_gpencil, overlay_depth_gpencil_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_depth_gpencil_selectable, overlay_depth_gpencil_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_depth_gpencil_clipped, overlay_depth_gpencil, drw_clipped)
CREATE_INFO_VARIANT(overlay_depth_gpencil_selectable_clipped, overlay_depth_gpencil_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_CREATE_INFO(overlay_depth_pointcloud_base)
VERTEX_SOURCE("overlay_depth_only_pointcloud_vert.glsl")
FRAGMENT_SOURCE("overlay_depth_only_frag.glsl")
ADDITIONAL_INFO(draw_pointcloud)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_depth_pointcloud, overlay_depth_pointcloud_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_depth_pointcloud_selectable, overlay_depth_pointcloud_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_depth_pointcloud_clipped, overlay_depth_pointcloud, drw_clipped)
CREATE_INFO_VARIANT(overlay_depth_pointcloud_selectable_clipped, overlay_depth_pointcloud_selectable, drw_clipped)
/* clang-format on */

GPU_SHADER_CREATE_INFO(overlay_depth_curves_base)
VERTEX_SOURCE("overlay_depth_only_curves_vert.glsl")
FRAGMENT_SOURCE("overlay_depth_only_frag.glsl")
ADDITIONAL_INFO(draw_curves)
ADDITIONAL_INFO(draw_curves_infos)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

/* clang-format off */
CREATE_INFO_VARIANT(overlay_depth_curves, overlay_depth_curves_base, draw_modelmat)
CREATE_INFO_VARIANT(overlay_depth_curves_selectable, overlay_depth_curves_base, draw_modelmat_with_custom_id, overlay_select)
CREATE_INFO_VARIANT(overlay_depth_curves_clipped, overlay_depth_curves, drw_clipped)
CREATE_INFO_VARIANT(overlay_depth_curves_selectable_clipped, overlay_depth_curves_selectable, drw_clipped)
/* clang-format on */

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform color
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_uniform_color)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
PUSH_CONSTANT(float4, ucolor)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
VERTEX_SOURCE("overlay_depth_only_vert.glsl")
FRAGMENT_SOURCE("overlay_uniform_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_modelmat)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_uniform_color_clipped, overlay_uniform_color, drw_clipped)

/** \} */
