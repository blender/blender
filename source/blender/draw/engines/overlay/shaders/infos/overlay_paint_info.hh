/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "overlay_common_info.hh"

/* -------------------------------------------------------------------- */
/** \name OVERLAY_shader_paint_face.
 *
 * Used for face selection mode in Weight, Vertex and Texture Paint.
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_paint_face)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC4, nor) /* Select flag on the 4th component. */
PUSH_CONSTANT(VEC4, ucolor)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_paint_face_vert.glsl")
FRAGMENT_SOURCE("overlay_uniform_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_paint_face)

/** \} */

/* -------------------------------------------------------------------- */
/** \name OVERLAY_shader_paint_point.
 *
 * Used for vertex selection mode in Weight and Vertex Paint.
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_overlay_paint_point_iface)
SMOOTH(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_paint_point)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC4, nor) /* Select flag on the 4th component. */
VERTEX_OUT(overlay_overlay_paint_point_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_paint_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_paint_point)

/** \} */

/* -------------------------------------------------------------------- */
/** \name OVERLAY_shader_paint_texture.
 *
 * Used in Texture Paint mode for the Stencil Image Masking.
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_paint_texture_iface)
SMOOTH(VEC2, uv_interp)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_paint_texture)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC2, mu) /* Masking uv map. */
VERTEX_OUT(overlay_paint_texture_iface)
SAMPLER(0, FLOAT_2D, maskImage)
PUSH_CONSTANT(VEC3, maskColor)
PUSH_CONSTANT(FLOAT, opacity) /* `1.0` by default. */
PUSH_CONSTANT(BOOL, maskInvertStencil)
PUSH_CONSTANT(BOOL, maskImagePremultiplied)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_paint_texture_vert.glsl")
FRAGMENT_SOURCE("overlay_paint_texture_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_paint_texture)

/** \} */

/* -------------------------------------------------------------------- */
/** \name OVERLAY_shader_paint_weight.
 *
 * Used to display Vertex Weights.
 * `overlay paint weight` is for wireframe display mode.
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_paint_weight_iface)
SMOOTH(VEC2, weight_interp) /* (weight, alert) */
SMOOTH(FLOAT, color_fac)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_paint_weight)
DO_STATIC_COMPILATION()
VERTEX_IN(0, FLOAT, weight)
VERTEX_IN(1, VEC3, pos)
VERTEX_IN(2, VEC3, nor)
VERTEX_OUT(overlay_paint_weight_iface)
SAMPLER(0, FLOAT_1D, colorramp)
PUSH_CONSTANT(FLOAT, opacity)     /* `1.0` by default. */
PUSH_CONSTANT(BOOL, drawContours) /* `false` by default. */
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_paint_weight_vert.glsl")
FRAGMENT_SOURCE("overlay_paint_weight_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_paint_weight)

GPU_SHADER_CREATE_INFO(overlay_paint_weight_fake_shading)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_paint_weight)
DEFINE("FAKE_SHADING")
PUSH_CONSTANT(VEC3, light_dir)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_paint_weight_fake_shading)

/** \} */

/* -------------------------------------------------------------------- */
/** \name OVERLAY_shader_paint_wire.
 *
 * Used in face selection mode to display edges of selected faces in Weight, Vertex and Texture
 * paint modes.
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_paint_wire_iface)
FLAT(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_paint_wire)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC4, nor) /* flag stored in w */
VERTEX_OUT(overlay_paint_wire_iface)
PUSH_CONSTANT(BOOL, useSelect)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_paint_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_paint_wire)

/** \} */
