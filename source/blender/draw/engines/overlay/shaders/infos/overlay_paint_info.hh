/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name OVERLAY_shader_paint_face.
 *
 * Used for face selection mode in Weight, Vertex and Texture Paint.
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_paint_face)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC4, "nor") /* Select flag on the 4th component. */
    .push_constant(Type::VEC4, "ucolor")
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_paint_face_vert.glsl")
    .fragment_source("overlay_uniform_color_frag.glsl")
    .additional_info("draw_modelmat");

GPU_SHADER_CREATE_INFO(overlay_paint_face_clipped)
    .additional_info("overlay_paint_face")
    .additional_info("drw_clipped")
    .do_static_compilation(true);

/** \} */

/* -------------------------------------------------------------------- */
/** \name OVERLAY_shader_paint_point.
 *
 * Used for vertex selection mode in Weight and Vertex Paint.
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_overlay_paint_point_iface, "").smooth(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_paint_point)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC4, "nor") /* Select flag on the 4th component. */
    .vertex_out(overlay_overlay_paint_point_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_paint_point_vert.glsl")
    .fragment_source("overlay_point_varying_color_frag.glsl")
    .additional_info("draw_modelmat", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_paint_point_clipped)
    .additional_info("overlay_paint_point")
    .additional_info("drw_clipped")
    .do_static_compilation(true);

/** \} */

/* -------------------------------------------------------------------- */
/** \name OVERLAY_shader_paint_texture.
 *
 * Used in Texture Paint mode for the Stencil Image Masking.
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_paint_texture_iface, "").smooth(Type::VEC2, "uv_interp");

GPU_SHADER_CREATE_INFO(overlay_paint_texture)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC2, "mu") /* Masking uv map. */
    .vertex_out(overlay_paint_texture_iface)
    .sampler(0, ImageType::FLOAT_2D, "maskImage")
    .push_constant(Type::VEC3, "maskColor")
    .push_constant(Type::FLOAT, "opacity") /* `1.0` by default. */
    .push_constant(Type::BOOL, "maskInvertStencil")
    .push_constant(Type::BOOL, "maskImagePremultiplied")
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_paint_texture_vert.glsl")
    .fragment_source("overlay_paint_texture_frag.glsl")
    .additional_info("draw_modelmat");

GPU_SHADER_CREATE_INFO(overlay_paint_texture_clipped)
    .additional_info("overlay_paint_texture")
    .additional_info("drw_clipped")
    .do_static_compilation(true);

/** \} */

/* -------------------------------------------------------------------- */
/** \name OVERLAY_shader_paint_vertcol.
 *
 * It should be used to draw a Vertex Paint overlay. But it is currently unreachable.
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_paint_vertcol_iface, "").smooth(Type::VEC3, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_paint_vertcol)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "ac") /* Active color. */
    .vertex_out(overlay_paint_vertcol_iface)
    .push_constant(Type::FLOAT, "opacity")      /* `1.0` by default. */
    .push_constant(Type::BOOL, "useAlphaBlend") /* `false` by default. */
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_paint_vertcol_vert.glsl")
    .fragment_source("overlay_paint_vertcol_frag.glsl")
    .additional_info("draw_modelmat");

GPU_SHADER_CREATE_INFO(overlay_paint_vertcol_clipped)
    .additional_info("overlay_paint_vertcol")
    .additional_info("drw_clipped")
    .do_static_compilation(true);

/** \} */

/* -------------------------------------------------------------------- */
/** \name OVERLAY_shader_paint_weight.
 *
 * Used to display Vertex Weights.
 * `overlay paint weight` is for wireframe display mode.
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_paint_weight_iface, "")
    .smooth(Type::VEC2, "weight_interp") /* (weight, alert) */
    .smooth(Type::FLOAT, "color_fac");

GPU_SHADER_CREATE_INFO(overlay_paint_weight)
    .do_static_compilation(true)
    .vertex_in(0, Type::FLOAT, "weight")
    .vertex_in(1, Type::VEC3, "pos")
    .vertex_in(2, Type::VEC3, "nor")
    .vertex_out(overlay_paint_weight_iface)
    .sampler(0, ImageType::FLOAT_1D, "colorramp")
    .push_constant(Type::FLOAT, "opacity")     /* `1.0` by default. */
    .push_constant(Type::BOOL, "drawContours") /* `false` by default. */
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_paint_weight_vert.glsl")
    .fragment_source("overlay_paint_weight_frag.glsl")
    .additional_info("draw_modelmat", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_paint_weight_fake_shading)
    .additional_info("overlay_paint_weight")
    .define("FAKE_SHADING")
    .push_constant(Type::VEC3, "light_dir")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(overlay_paint_weight_clipped)
    .additional_info("overlay_paint_weight")
    .additional_info("drw_clipped")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(overlay_paint_weight_fake_shading_clipped)
    .additional_info("overlay_paint_weight_fake_shading")
    .additional_info("drw_clipped")
    .do_static_compilation(true);

/** \} */

/* -------------------------------------------------------------------- */
/** \name OVERLAY_shader_paint_wire.
 *
 * Used in face selection mode to display edges of selected faces in Weight, Vertex and Texture
 * paint modes.
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_paint_wire_iface, "").flat(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_paint_wire)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC4, "nor") /* flag stored in w */
    .vertex_out(overlay_paint_wire_iface)
    .push_constant(Type::BOOL, "useSelect")
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("overlay_paint_wire_vert.glsl")
    .fragment_source("overlay_varying_color.glsl")
    .additional_info("draw_modelmat", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_paint_wire_clipped)
    .additional_info("overlay_paint_vertcol")
    .additional_info("drw_clipped")
    .do_static_compilation(true);

/** \} */
