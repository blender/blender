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
PUSH_CONSTANT(float4x4, camera_object_persinv)
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
PUSH_CONSTANT(float4x4, camera_object_persinv)
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
