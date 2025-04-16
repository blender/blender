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

#  include "overlay_shader_shared.hh"

#  define HAIR_SHADER
#  define DRW_HAIR_INFO

#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO
#endif

#include "overlay_common_info.hh"

GPU_SHADER_INTERFACE_INFO(overlay_edit_flat_color_iface)
FLAT(float4, finalColor)
GPU_SHADER_INTERFACE_END()
GPU_SHADER_INTERFACE_INFO(overlay_edit_smooth_color_iface)
SMOOTH(float4, finalColor)
GPU_SHADER_INTERFACE_END()
GPU_SHADER_INTERFACE_INFO(overlay_edit_nopersp_color_iface)
NO_PERSPECTIVE(float4, finalColor)
GPU_SHADER_INTERFACE_END()

/* -------------------------------------------------------------------- */
/** \name Edit Mesh
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_common)
DEFINE_VALUE("blender_srgb_to_framebuffer_space(a)", "a")
SAMPLER(0, DEPTH_2D, depthTex)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
/* Per view factor. */
PUSH_CONSTANT(float, ndc_offset_factor)
/* Per pass factor. */
PUSH_CONSTANT(float, ndc_offset)
PUSH_CONSTANT(bool, wireShading)
PUSH_CONSTANT(bool, selectFace)
PUSH_CONSTANT(bool, selectEdge)
PUSH_CONSTANT(float, alpha)
PUSH_CONSTANT(float, retopologyOffset)
PUSH_CONSTANT(int4, dataMask)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_depth)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
PUSH_CONSTANT(float, retopologyOffset)
VERTEX_SOURCE("overlay_edit_mesh_depth_vert.glsl")
FRAGMENT_SOURCE("overlay_depth_only_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_mesh_depth)

GPU_SHADER_INTERFACE_INFO(overlay_edit_mesh_vert_iface)
SMOOTH(float4, finalColor)
SMOOTH(float, vertexCrease)
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

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_mesh_vert)

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_edit_mesh_edge_geom_iface, geometry_out)
SMOOTH(float4, finalColor)
GPU_SHADER_NAMED_INTERFACE_END(geometry_out)
GPU_SHADER_NAMED_INTERFACE_INFO(overlay_edit_mesh_edge_geom_flat_iface, geometry_flat_out)
FLAT(float4, finalColorOuter)
GPU_SHADER_NAMED_INTERFACE_END(geometry_flat_out)
GPU_SHADER_NAMED_INTERFACE_INFO(overlay_edit_mesh_edge_geom_noperspective_iface,
                                geometry_noperspective_out)
NO_PERSPECTIVE(float, edgeCoord)
GPU_SHADER_NAMED_INTERFACE_END(geometry_noperspective_out)

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_edge)
DO_STATIC_COMPILATION()
DEFINE("EDGE")
STORAGE_BUF_FREQ(0, READ, float, pos[], GEOMETRY)
STORAGE_BUF_FREQ(1, READ, uint, vnor[], GEOMETRY)
STORAGE_BUF_FREQ(2, READ, uint, data[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
PUSH_CONSTANT(int2, gpu_attr_2)
PUSH_CONSTANT(bool, do_smooth_wire)
PUSH_CONSTANT(bool, use_vertex_selection)
VERTEX_OUT(overlay_edit_mesh_edge_geom_iface)
VERTEX_OUT(overlay_edit_mesh_edge_geom_flat_iface)
VERTEX_OUT(overlay_edit_mesh_edge_geom_noperspective_iface)
VERTEX_SOURCE("overlay_edit_mesh_edge_vert.glsl")
FRAGMENT_SOURCE("overlay_edit_mesh_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(overlay_edit_mesh_common)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_mesh_edge)

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

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_mesh_face)

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

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_mesh_facedot)

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_normal)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
SAMPLER(0, DEPTH_2D, depthTex)
PUSH_CONSTANT(float, normalSize)
PUSH_CONSTANT(float, normalScreenSize)
PUSH_CONSTANT(float, alpha)
PUSH_CONSTANT(bool, isConstantScreenSizeNormals)
VERTEX_OUT(overlay_edit_flat_color_iface)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
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
STORAGE_BUF_FREQ(1, READ, float, pos[], GEOMETRY)
DEFINE("FACE_NORMAL")
PUSH_CONSTANT(bool, hq_normals)
STORAGE_BUF_FREQ(0, READ, uint, norAndFlag[], GEOMETRY)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_mesh_face_normal)

GPU_SHADER_CREATE_INFO(overlay_mesh_face_normal_subdiv)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_edit_mesh_normal)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(1, READ, float, pos[], GEOMETRY)
DEFINE("FACE_NORMAL")
DEFINE("FLOAT_NORMAL")
STORAGE_BUF_FREQ(0, READ, float4, norAndFlag[], GEOMETRY)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_mesh_face_normal_subdiv)

GPU_SHADER_CREATE_INFO(overlay_mesh_loop_normal)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_edit_mesh_normal)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(1, READ, float, pos[], GEOMETRY)
DEFINE("LOOP_NORMAL")
PUSH_CONSTANT(bool, hq_normals)
STORAGE_BUF_FREQ(0, READ, uint, lnor[], GEOMETRY)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_mesh_loop_normal)

GPU_SHADER_CREATE_INFO(overlay_mesh_loop_normal_subdiv)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_edit_mesh_normal)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(1, READ, float, pos[], GEOMETRY)
DEFINE("LOOP_NORMAL")
DEFINE("FLOAT_NORMAL")
STORAGE_BUF_FREQ(0, READ, float4, lnor[], GEOMETRY)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_mesh_loop_normal_subdiv)

GPU_SHADER_CREATE_INFO(overlay_mesh_vert_normal)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_edit_mesh_normal)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(1, READ, float, pos[], GEOMETRY)
DEFINE("VERT_NORMAL")
STORAGE_BUF_FREQ(0, READ, uint, vnor[], GEOMETRY)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_mesh_vert_normal)

GPU_SHADER_CREATE_INFO(overlay_mesh_vert_normal_subdiv)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_edit_mesh_normal)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(1, READ, float, pos[], GEOMETRY)
DEFINE("VERT_NORMAL")
DEFINE("FLOAT_NORMAL")
STORAGE_BUF_FREQ(0, READ, float, vnor[], GEOMETRY)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_mesh_vert_normal_subdiv)

GPU_SHADER_INTERFACE_INFO(overlay_edit_mesh_analysis_iface)
SMOOTH(float4, weightColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_analysis)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float, weight)
SAMPLER(0, FLOAT_1D, weightTex)
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_OUT(overlay_edit_mesh_analysis_iface)
VERTEX_SOURCE("overlay_edit_mesh_analysis_vert.glsl")
FRAGMENT_SOURCE("overlay_edit_mesh_analysis_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_mesh_analysis)

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_skin_root)
DO_STATIC_COMPILATION()
VERTEX_OUT(overlay_edit_flat_color_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_edit_mesh_skin_root_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
/* TODO(fclem): Use correct vertex format. For now we read the format manually. */
STORAGE_BUF_FREQ(0, READ, float, size[], GEOMETRY)
DEFINE("VERTEX_PULL")
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_mesh_skin_root)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit UV
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_edit_uv_iface)
SMOOTH(float, selectionFac)
FLAT(float2, stippleStart)
NO_PERSPECTIVE(float, edgeCoord)
NO_PERSPECTIVE(float2, stipplePos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_edges)
DO_STATIC_COMPILATION()
STORAGE_BUF_FREQ(0, READ, float, au[], GEOMETRY)
STORAGE_BUF_FREQ(1, READ, uint, data[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
PUSH_CONSTANT(int, lineStyle)
PUSH_CONSTANT(bool, doSmoothWire)
PUSH_CONSTANT(float, alpha)
PUSH_CONSTANT(float, dashLength)
SPECIALIZATION_CONSTANT(bool, use_edge_select, false)
VERTEX_OUT(overlay_edit_uv_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_edit_uv_edges_vert.glsl")
FRAGMENT_SOURCE("overlay_edit_uv_edges_frag.glsl")
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
PUSH_CONSTANT(float, uvOpacity)
VERTEX_OUT(overlay_edit_flat_color_iface)
FRAGMENT_OUT(0, float4, fragColor)
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
PUSH_CONSTANT(float, pointSize)
VERTEX_OUT(overlay_edit_flat_color_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_edit_uv_face_dots_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(overlay_edit_uv_vert_iface)
SMOOTH(float4, fillColor)
SMOOTH(float4, outlineColor)
SMOOTH(float4, radii)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_verts)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float2, au)
VERTEX_IN(1, uint, flag)
PUSH_CONSTANT(float, pointSize)
PUSH_CONSTANT(float, outlineWidth)
PUSH_CONSTANT(float4, color)
VERTEX_OUT(overlay_edit_uv_vert_iface)
FRAGMENT_OUT(0, float4, fragColor)
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
FRAGMENT_OUT(0, float4, fragColor)
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
SAMPLER(0, FLOAT_2D, imgTexture)
PUSH_CONSTANT(bool, imgPremultiplied)
PUSH_CONSTANT(bool, imgAlphaBlend)
PUSH_CONSTANT(float4, ucolor)
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_SOURCE("overlay_image_frag.glsl")
PUSH_CONSTANT(float2, brush_offset)
PUSH_CONSTANT(float2, brush_scale)
ADDITIONAL_INFO(draw_view);
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_mask_image)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_OUT(edit_uv_image_iface)
SAMPLER(0, FLOAT_2D, imgTexture)
PUSH_CONSTANT(float4, color)
PUSH_CONSTANT(float, opacity)
FRAGMENT_OUT(0, float4, fragColor)
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
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_edit_uv_stretching_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_edit_uv_stretching_area)
DO_STATIC_COMPILATION()
VERTEX_IN(1, float, ratio)
PUSH_CONSTANT(float, totalAreaRatio)
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
STORAGE_BUF_FREQ(0, READ, float, pos[], GEOMETRY)
STORAGE_BUF_FREQ(1, READ, uint, data[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
VERTEX_OUT(overlay_edit_smooth_color_iface)
PUSH_CONSTANT(bool, showCurveHandles)
PUSH_CONSTANT(int, curveHandleDisplay)
PUSH_CONSTANT(float, alpha)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_edit_curve_handle_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_curve_handle)

GPU_SHADER_CREATE_INFO(overlay_edit_curve_point)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, uint, data)
VERTEX_OUT(overlay_edit_flat_color_iface)
PUSH_CONSTANT(bool, showCurveHandles)
PUSH_CONSTANT(int, curveHandleDisplay)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_edit_curve_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_curve_point)

GPU_SHADER_CREATE_INFO(overlay_edit_curve_wire)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float3, nor)
VERTEX_IN(2, float3, tangent)
VERTEX_IN(3, float, rad)
PUSH_CONSTANT(float, normalSize)
VERTEX_OUT(overlay_edit_flat_color_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_edit_curve_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_curve_wire)

GPU_SHADER_CREATE_INFO(overlay_edit_curve_normals)
DO_STATIC_COMPILATION()
STORAGE_BUF_FREQ(0, READ, float, pos[], GEOMETRY)
STORAGE_BUF_FREQ(1, READ, float, rad[], GEOMETRY)
STORAGE_BUF_FREQ(2, READ, uint, nor[], GEOMETRY)
STORAGE_BUF_FREQ(3, READ, uint, tangent[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
PUSH_CONSTANT(int2, gpu_attr_2)
PUSH_CONSTANT(int2, gpu_attr_3)
PUSH_CONSTANT(float, normalSize)
PUSH_CONSTANT(bool, use_hq_normals)
VERTEX_OUT(overlay_edit_flat_color_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_edit_curve_normals_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_curve_normals)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Curves
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_edit_curves_handle)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
STORAGE_BUF_FREQ(0, READ, float, pos[], GEOMETRY)
STORAGE_BUF_FREQ(1, READ, uint, data[], GEOMETRY)
STORAGE_BUF_FREQ(2, READ, float, selection[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int2, gpu_attr_1)
PUSH_CONSTANT(int2, gpu_attr_2)
VERTEX_OUT(overlay_edit_smooth_color_iface)
PUSH_CONSTANT(int, curveHandleDisplay)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_edit_curves_handle_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_curves_handle)

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
PUSH_CONSTANT(bool, doStrokeEndpoints)
#endif
VERTEX_OUT(overlay_edit_flat_color_iface)
SAMPLER(0, FLOAT_1D, weightTex)
PUSH_CONSTANT(bool, useWeight)
PUSH_CONSTANT(bool, useGreasePencil)
PUSH_CONSTANT(int, curveHandleDisplay)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_edit_particle_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_curves_point)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Lattice
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_edit_lattice_point_base)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, uint, data)
VERTEX_OUT(overlay_edit_flat_color_iface)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_edit_lattice_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_edit_lattice_point, overlay_edit_lattice_point_base)

GPU_SHADER_CREATE_INFO(overlay_edit_lattice_wire_base)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float, weight)
SAMPLER(0, FLOAT_1D, weightTex)
VERTEX_OUT(overlay_edit_smooth_color_iface)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_edit_lattice_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_edit_lattice_wire, overlay_edit_lattice_wire_base)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit Particle
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_edit_particle_strand)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float, selection)
SAMPLER(0, FLOAT_1D, weightTex)
PUSH_CONSTANT(bool, useWeight)
PUSH_CONSTANT(bool, useGreasePencil)
VERTEX_OUT(overlay_edit_smooth_color_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_edit_particle_strand_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_particle_strand)

GPU_SHADER_CREATE_INFO(overlay_edit_particle_point)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float, selection)
VERTEX_OUT(overlay_edit_flat_color_iface)
SAMPLER(0, FLOAT_1D, weightTex)
PUSH_CONSTANT(bool, useWeight)
PUSH_CONSTANT(bool, useGreasePencil)
FRAGMENT_OUT(0, float4, fragColor)
#if 1 /* TODO(fclem): Required for legacy gpencil overlay. To be moved to specialized shader. */
TYPEDEF_SOURCE("gpencil_shader_shared.hh")
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(3, uint, vflag)
PUSH_CONSTANT(bool, doStrokeEndpoints)
#endif
VERTEX_SOURCE("overlay_edit_particle_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_edit_particle_point)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Edit PointCloud
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_edit_pointcloud_base)
VERTEX_IN(0, float4, pos_rad)
VERTEX_OUT(overlay_edit_flat_color_iface)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_edit_pointcloud_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_edit_pointcloud, overlay_edit_pointcloud_base)

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

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_depth_mesh, overlay_depth_mesh_base)

GPU_SHADER_CREATE_INFO(overlay_depth_mesh_conservative_base)
STORAGE_BUF_FREQ(0, READ, float, pos[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
VERTEX_SOURCE("overlay_depth_only_mesh_conservative_vert.glsl")
FRAGMENT_SOURCE("overlay_depth_only_frag.glsl")
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(gpu_index_buffer_load)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_depth_mesh_conservative,
                                 overlay_depth_mesh_conservative_base)

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_depth_only_gpencil_flat_iface, gp_interp_flat)
FLAT(float2, aspect)
FLAT(float4, sspos)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp_flat)
GPU_SHADER_NAMED_INTERFACE_INFO(overlay_depth_only_gpencil_noperspective_iface,
                                gp_interp_noperspective)
NO_PERSPECTIVE(float2, thickness)
NO_PERSPECTIVE(float, hardness)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp_noperspective)

GPU_SHADER_CREATE_INFO(overlay_depth_gpencil_base)
TYPEDEF_SOURCE("gpencil_shader_shared.hh")
VERTEX_OUT(overlay_depth_only_gpencil_flat_iface)
VERTEX_OUT(overlay_depth_only_gpencil_noperspective_iface)
VERTEX_SOURCE("overlay_depth_only_gpencil_vert.glsl")
FRAGMENT_SOURCE("overlay_depth_only_gpencil_frag.glsl")
DEPTH_WRITE(DepthWrite::ANY)
PUSH_CONSTANT(bool, gpStrokeOrder3d) /* TODO(fclem): Move to a GPencil object UBO. */
PUSH_CONSTANT(float4, gpDepthPlane)  /* TODO(fclem): Move to a GPencil object UBO. */
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_gpencil)
ADDITIONAL_INFO(draw_object_infos)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_depth_gpencil, overlay_depth_gpencil_base)

GPU_SHADER_CREATE_INFO(overlay_depth_pointcloud_base)
VERTEX_SOURCE("overlay_depth_only_pointcloud_vert.glsl")
FRAGMENT_SOURCE("overlay_depth_only_frag.glsl")
ADDITIONAL_INFO(draw_pointcloud)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_depth_pointcloud, overlay_depth_pointcloud_base)

GPU_SHADER_CREATE_INFO(overlay_depth_curves_base)
VERTEX_SOURCE("overlay_depth_only_curves_vert.glsl")
FRAGMENT_SOURCE("overlay_depth_only_frag.glsl")
ADDITIONAL_INFO(draw_hair)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_depth_curves, overlay_depth_curves_base)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform color
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_uniform_color)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
PUSH_CONSTANT(float4, ucolor)
DEFINE("LINE_OUTPUT")
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_depth_only_vert.glsl")
FRAGMENT_SOURCE("overlay_uniform_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_modelmat)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_uniform_color)

/** \} */
