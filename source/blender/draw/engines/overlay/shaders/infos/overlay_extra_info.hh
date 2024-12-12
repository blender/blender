/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "overlay_common_info.hh"

/* -------------------------------------------------------------------- */
/** \name Extra shapes
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_iface)
NO_PERSPECTIVE(VEC2, edgePos)
FLAT(VEC2, edgeStart)
FLAT(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.h")
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, INT, vclass)
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_extra_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, READ, ExtraInstanceData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_extra)

GPU_SHADER_CREATE_INFO(overlay_extra_spot_cone)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_extra)
DEFINE("IS_SPOT_CONE")
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_extra_spot_cone)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Irradiance Grid
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_grid_iface)
FLAT(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_grid_base)
SAMPLER(0, DEPTH_2D, depthBuffer)
PUSH_CONSTANT(MAT4, gridModelMatrix)
PUSH_CONSTANT(BOOL, isTransform)
VERTEX_OUT(overlay_extra_grid_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_extra_lightprobe_grid_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_extra_grid, overlay_extra_grid_base)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ground-lines
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_extra_groundline)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
/* Instance attributes. */
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_extra_groundline_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, READ, vec4, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_extra_groundline)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extra wires
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_wire_iface)
NO_PERSPECTIVE(VEC2, stipple_coord)
FLAT(VEC2, stipple_start)
FLAT(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_wire_base)
VERTEX_OUT(overlay_extra_wire_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_extra_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_wire_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.h")
STORAGE_BUF(0, READ, VertexData, data_buf[])
PUSH_CONSTANT(INT, colorid)
DEFINE_VALUE("pos", "data_buf[gl_VertexID].pos_.xyz")
DEFINE_VALUE("color", "data_buf[gl_VertexID].color_")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_extra_wire, overlay_extra_wire_base)

GPU_SHADER_CREATE_INFO(overlay_extra_wire_object_base)
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC4, color)
/* If colorid is equal to 0 (i.e: Not specified) use color attribute and stippling. */
VERTEX_IN(2, INT, colorid)
VERTEX_OUT(overlay_extra_wire_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_extra_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_wire_frag.glsl")
DEFINE("OBJECT_WIRE")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_extra_wire_object, overlay_extra_wire_object_base)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extra points
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_point_iface)
FLAT(VEC4, radii)
FLAT(VEC4, fillColor)
FLAT(VEC4, outlineColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_point_base)
/* TODO(fclem): Move the vertex shader to Overlay engine and remove this bypass. */
DEFINE_VALUE("blender_srgb_to_framebuffer_space(a)", "a")
VERTEX_OUT(overlay_extra_point_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_extra_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_varying_outline_aa_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
TYPEDEF_SOURCE("overlay_shader_shared.h")
STORAGE_BUF(0, READ, VertexData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_extra_point, overlay_extra_point_base)

GPU_SHADER_INTERFACE_INFO(overlay_extra_loose_point_iface)
SMOOTH(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_loose_point_base)
VERTEX_OUT(overlay_extra_loose_point_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_extra_loose_point_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_loose_point_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.h")
STORAGE_BUF(0, READ, VertexData, data_buf[])
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_extra_loose_point, overlay_extra_loose_point_base)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Motion Path
 * \{ */

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_motion_path_line_iface, interp)
SMOOTH(VEC4, color)
GPU_SHADER_NAMED_INTERFACE_END(interp)

GPU_SHADER_CREATE_INFO(overlay_motion_path_line)
DO_STATIC_COMPILATION()
STORAGE_BUF_FREQ(0, READ, float, pos[], GEOMETRY)
PUSH_CONSTANT(IVEC2, gpu_attr_0)
PUSH_CONSTANT(IVEC4, mpathLineSettings)
PUSH_CONSTANT(BOOL, selected)
PUSH_CONSTANT(VEC3, customColorPre)
PUSH_CONSTANT(VEC3, customColorPost)
PUSH_CONSTANT(INT, lineThickness) /* In pixels. */
PUSH_CONSTANT(MAT4, camera_space_matrix)
VERTEX_OUT(overlay_motion_path_line_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_motion_path_line_vert.glsl")
FRAGMENT_SOURCE("overlay_motion_path_line_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_motion_path_line)

GPU_SHADER_INTERFACE_INFO(overlay_motion_path_point_iface)
FLAT(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_motion_path_point)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.h")
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, INT, flag)
PUSH_CONSTANT(IVEC4, mpathPointSettings)
PUSH_CONSTANT(BOOL, showKeyFrames)
PUSH_CONSTANT(VEC3, customColorPre)
PUSH_CONSTANT(VEC3, customColorPost)
PUSH_CONSTANT(MAT4, camera_space_matrix)
VERTEX_OUT(overlay_motion_path_point_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_motion_path_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_motion_path_point)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Empty
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_image_iface)
SMOOTH(VEC2, uvs)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_image_base)
PUSH_CONSTANT(BOOL, depthSet)
PUSH_CONSTANT(BOOL, isCameraBackground)
PUSH_CONSTANT(BOOL, imgPremultiplied)
PUSH_CONSTANT(BOOL, imgAlphaBlend)
PUSH_CONSTANT(VEC4, ucolor)
VERTEX_IN(0, VEC3, pos)
VERTEX_OUT(overlay_image_iface)
SAMPLER(0, FLOAT_2D, imgTexture)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_image_vert.glsl")
FRAGMENT_SOURCE("overlay_image_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_image, overlay_image_base)

GPU_SHADER_CREATE_INFO(overlay_image_depth_bias_base)
ADDITIONAL_INFO(overlay_image_base)
DEFINE("DEPTH_BIAS")
PUSH_CONSTANT(MAT4, depth_bias_winmat)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_image_depth_bias, overlay_image_depth_bias_base)

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPencil Canvas
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_gpencil_canvas)
DO_STATIC_COMPILATION()
VERTEX_OUT(overlay_extra_iface)
PUSH_CONSTANT(VEC4, color)
PUSH_CONSTANT(VEC3, xAxis)
PUSH_CONSTANT(VEC3, yAxis)
PUSH_CONSTANT(VEC3, origin)
PUSH_CONSTANT(INT, halfLineCount)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_edit_gpencil_canvas_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_frag.glsl")
ADDITIONAL_INFO(draw_mesh)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_gpencil_canvas)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particle
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_particle_iface)
FLAT(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_particle_dot_base)
SAMPLER(0, FLOAT_1D, weightTex)
PUSH_CONSTANT(VEC4, ucolor) /* Draw-size packed in alpha. */
VERTEX_IN(0, VEC3, part_pos)
VERTEX_IN(1, VEC4, part_rot)
VERTEX_IN(2, FLOAT, part_val)
VERTEX_OUT(overlay_particle_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_particle_vert.glsl")
FRAGMENT_SOURCE("overlay_particle_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_particle_dot, overlay_particle_dot_base)

GPU_SHADER_CREATE_INFO(overlay_particle_shape_base)
TYPEDEF_SOURCE("overlay_shader_shared.h")
SAMPLER(0, FLOAT_1D, weightTex)
PUSH_CONSTANT(VEC4, ucolor) /* Draw-size packed in alpha. */
PUSH_CONSTANT(INT, shape_type)
/* Use first attribute to only bind one buffer. */
STORAGE_BUF_FREQ(0, READ, ParticlePointData, part_pos[], GEOMETRY)
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_particle_shape_vert.glsl")
FRAGMENT_SOURCE("overlay_particle_shape_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_particle_shape, overlay_particle_shape_base)

GPU_SHADER_CREATE_INFO(overlay_particle_hair_base)
TYPEDEF_SOURCE("overlay_shader_shared.h")
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC3, nor)
PUSH_CONSTANT(INT, colorType)
PUSH_CONSTANT(BOOL, isTransform)
PUSH_CONSTANT(BOOL, useColoring)
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_particle_hair_vert.glsl")
FRAGMENT_SOURCE("overlay_particle_shape_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_object_infos_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_particle_hair, overlay_particle_hair_base)

/** \} */
