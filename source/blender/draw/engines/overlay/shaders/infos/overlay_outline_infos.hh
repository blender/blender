/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_object_infos_infos.hh"
#  include "draw_view_infos.hh"
#  include "gpu_shader_fullscreen_infos.hh"

#  include "gpu_index_load_infos.hh"
#  include "gpu_shader_create_info.hh"

#  include "overlay_shader_shared.hh"

#  define CURVES_SHADER
#  define DRW_HAIR_INFO

#  define POINTCLOUD_SHADER
#  define DRW_POINTCLOUD_INFO
#endif

#include "overlay_common_infos.hh"

/* -------------------------------------------------------------------- */
/** \name Outline Pre-pass
 * \{ */

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_outline_prepass_iface, interp)
FLAT(uint, ob_id)
GPU_SHADER_NAMED_INTERFACE_END(interp)

GPU_SHADER_CREATE_INFO(overlay_outline_prepass)
TYPEDEF_SOURCE("overlay_shader_shared.hh")
PUSH_CONSTANT(bool, is_transform)
VERTEX_OUT(overlay_outline_prepass_iface)
/* Using uint because 16bit uint can contain more ids than int. */
FRAGMENT_OUT(0, uint, out_object_id)
FRAGMENT_SOURCE("overlay_outline_prepass_frag.glsl")
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_mesh)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
VERTEX_SOURCE("overlay_outline_prepass_vert.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(overlay_outline_prepass)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_outline_prepass_mesh_clipped,
                    overlay_outline_prepass_mesh,
                    drw_clipped)

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_outline_prepass_wire_iface, vert)
FLAT(float3, pos)
GPU_SHADER_NAMED_INTERFACE_END(vert)

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_curves)
DO_STATIC_COMPILATION()
VERTEX_SOURCE("overlay_outline_prepass_curves_vert.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_curves)
ADDITIONAL_INFO(draw_curves_infos)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(overlay_outline_prepass)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_outline_prepass_curves_clipped,
                    overlay_outline_prepass_curves,
                    drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_wire)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_outline_prepass)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_mesh)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(0, read, float, pos[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
VERTEX_SOURCE("overlay_outline_prepass_wire_vert.glsl")
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_outline_prepass_wire_clipped,
                    overlay_outline_prepass_wire,
                    drw_clipped)

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_outline_prepass_gpencil_flat_iface, gp_interp_flat)
FLAT(float2, aspect)
FLAT(float4, sspos)
FLAT(float4, sspos_adj)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp_flat)
GPU_SHADER_NAMED_INTERFACE_INFO(overlay_outline_prepass_gpencil_noperspective_iface,
                                gp_interp_noperspective)
NO_PERSPECTIVE(float4, thickness)
NO_PERSPECTIVE(float, hardness)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp_noperspective)

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_gpencil)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
PUSH_CONSTANT(bool, is_transform)
VERTEX_OUT(overlay_outline_prepass_iface)
VERTEX_OUT(overlay_outline_prepass_gpencil_flat_iface)
VERTEX_OUT(overlay_outline_prepass_gpencil_noperspective_iface)
VERTEX_SOURCE("overlay_outline_prepass_gpencil_vert.glsl")
PUSH_CONSTANT(bool, gp_stroke_order3d) /* TODO(fclem): Move to a GPencil object UBO. */
PUSH_CONSTANT(float4, gp_depth_plane)  /* TODO(fclem): Move to a GPencil object UBO. */
/* Using uint because 16bit uint can contain more ids than int. */
FRAGMENT_OUT(0, uint, out_object_id)
FRAGMENT_SOURCE("overlay_outline_prepass_gpencil_frag.glsl")
DEPTH_WRITE(DepthWrite::ANY)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_gpencil)
ADDITIONAL_INFO(draw_object_infos)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_outline_prepass_gpencil_clipped,
                    overlay_outline_prepass_gpencil,
                    drw_clipped)

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_pointcloud)
DO_STATIC_COMPILATION()
VERTEX_SOURCE("overlay_outline_prepass_pointcloud_vert.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_pointcloud)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(overlay_outline_prepass)
GPU_SHADER_CREATE_END()

CREATE_INFO_VARIANT(overlay_outline_prepass_pointcloud_clipped,
                    overlay_outline_prepass_pointcloud,
                    drw_clipped)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Outline Rendering
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_outline_detect)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(float, alpha_occlu)
PUSH_CONSTANT(bool, is_xray_wires)
PUSH_CONSTANT(bool, do_anti_aliasing)
PUSH_CONSTANT(bool, do_thick_outlines)
SAMPLER(0, usampler2D, outline_id_tx)
SAMPLER(1, sampler2DDepth, outline_depth_tx)
SAMPLER(2, sampler2DDepth, scene_depth_tx)
FRAGMENT_OUT(0, float4, frag_color)
FRAGMENT_OUT(1, float4, line_output)
FRAGMENT_SOURCE("overlay_outline_detect_frag.glsl")
ADDITIONAL_INFO(gpu_fullscreen)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/** \} */
