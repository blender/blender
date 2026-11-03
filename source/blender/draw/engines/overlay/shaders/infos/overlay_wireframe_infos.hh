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
#endif

#ifdef GLSL_CPP_STUBS
#  define CUSTOM_DEPTH_BIAS_CONST
#endif

#include "gpu_shader_create_info.hh"

GPU_SHADER_INTERFACE_INFO(overlay_edit_uv_iface_wireframe)
SMOOTH(float, selection_fac)
FLAT(float2, stipple_start)
NO_PERSPECTIVE(float, edge_coord)
NO_PERSPECTIVE(float2, stipple_pos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_wireframe_uv)
DO_STATIC_COMPILATION()
DEFINE("WIREFRAME")
STORAGE_BUF_FREQ(0, read, float, au[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
COMPILATION_CONSTANT(uint, line_style, 4u /* OVERLAY_UV_LINE_STYLE_SHADOW */)
COMPILATION_CONSTANT(uint, dash_length, 1 /* Not use by this style. */)
COMPILATION_CONSTANT(bool, use_edge_select, false)
PUSH_CONSTANT(bool, do_smooth_wire)
PUSH_CONSTANT(float, alpha)
VERTEX_OUT(overlay_edit_uv_iface_wireframe)
FRAGMENT_OUT(0, float4, frag_color)
/* Note: Reuse edit mode shader as it is mostly the same. */
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
