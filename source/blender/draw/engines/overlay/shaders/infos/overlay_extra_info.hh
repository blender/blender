/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

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
/* Instance attributes. */
VERTEX_IN(2, VEC4, color)
VERTEX_IN(3, MAT4, inst_obmat)
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_extra_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_select)
DO_STATIC_COMPILATION()
DEFINE("SELECT_EDGES")
ADDITIONAL_INFO(overlay_extra)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_extra)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_select_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_extra_select)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Irradiance Grid
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_grid_iface)
FLAT(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_grid)
DO_STATIC_COMPILATION()
SAMPLER(0, DEPTH_2D, depthBuffer)
PUSH_CONSTANT(MAT4, gridModelMatrix)
PUSH_CONSTANT(BOOL, isTransform)
VERTEX_OUT(overlay_extra_grid_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_extra_lightprobe_grid_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_grid_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_extra_grid)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ground-lines
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_extra_groundline)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
/* Instance attributes. */
VERTEX_IN(1, VEC3, inst_pos)
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_extra_groundline_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_groundline_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_extra_groundline)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extra wires
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_wire_iface)
NO_PERSPECTIVE(VEC2, stipple_coord)
FLAT(VEC2, stipple_start)
FLAT(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_wire)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC4, color)
/* If colorid is equal to 0 (i.e: Not specified) use color attribute and stippling. */
VERTEX_IN(2, INT, colorid)
VERTEX_OUT(overlay_extra_wire_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_extra_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_wire_frag.glsl")
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_wire_select)
DO_STATIC_COMPILATION()
DEFINE("SELECT_EDGES")
ADDITIONAL_INFO(overlay_extra_wire)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_wire_object)
DO_STATIC_COMPILATION()
DEFINE("OBJECT_WIRE")
ADDITIONAL_INFO(overlay_extra_wire)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_wire_select_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_extra_wire_select)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_wire_object_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_extra_wire_object)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_wire_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_extra_wire)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extra points
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_point_iface)
FLAT(VEC4, radii)
FLAT(VEC4, fillColor)
FLAT(VEC4, outlineColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_point)
DO_STATIC_COMPILATION()
/* TODO(fclem): Move the vertex shader to Overlay engine and remove this bypass. */
DEFINE_VALUE("blender_srgb_to_framebuffer_space(a)", "a")
VERTEX_IN(0, VEC3, pos)
PUSH_CONSTANT(VEC4, ucolor)
VERTEX_OUT(overlay_extra_point_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_extra_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_varying_outline_aa_frag.glsl")
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_point_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_extra_point)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(overlay_extra_loose_point_iface)
SMOOTH(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_loose_point)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC4, vertex_color)
VERTEX_OUT(overlay_extra_loose_point_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
VERTEX_SOURCE("overlay_extra_loose_point_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_loose_point_frag.glsl")
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_loose_point_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_extra_loose_point)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Motion Path
 * \{ */

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_motion_path_line_iface, interp)
SMOOTH(VEC4, color)
GPU_SHADER_NAMED_INTERFACE_END(interp)
GPU_SHADER_NAMED_INTERFACE_INFO(overlay_motion_path_line_flat_iface, interp_flat)
FLAT(VEC2, ss_pos)
GPU_SHADER_NAMED_INTERFACE_END(interp_flat)

GPU_SHADER_CREATE_INFO(overlay_motion_path_line)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
PUSH_CONSTANT(IVEC4, mpathLineSettings)
PUSH_CONSTANT(BOOL, selected)
PUSH_CONSTANT(VEC3, customColorPre)
PUSH_CONSTANT(VEC3, customColorPost)
PUSH_CONSTANT(INT, lineThickness) /* In pixels. */
PUSH_CONSTANT(MAT4, camera_space_matrix)
VERTEX_OUT(overlay_motion_path_line_iface)
VERTEX_OUT(overlay_motion_path_line_flat_iface)
GEOMETRY_OUT(overlay_motion_path_line_iface)
GEOMETRY_LAYOUT(PrimitiveIn::LINES, PrimitiveOut::TRIANGLE_STRIP, 4)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_motion_path_line_vert.glsl")
GEOMETRY_SOURCE("overlay_motion_path_line_geom.glsl")
FRAGMENT_SOURCE("overlay_motion_path_line_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_motion_path_line_no_geom)
METAL_BACKEND_ONLY()
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
PUSH_CONSTANT(IVEC4, mpathLineSettings)
PUSH_CONSTANT(BOOL, selected)
PUSH_CONSTANT(VEC3, customColorPre)
PUSH_CONSTANT(VEC3, customColorPost)
PUSH_CONSTANT(INT, lineThickness) /* In pixels. */
VERTEX_OUT(overlay_motion_path_line_iface)
FRAGMENT_OUT(0, VEC4, fragColor)
VERTEX_SOURCE("overlay_motion_path_line_vert_no_geom.glsl")
FRAGMENT_SOURCE("overlay_motion_path_line_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_motion_path_line_next)
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
VERTEX_SOURCE("overlay_motion_path_line_next_vert.glsl")
FRAGMENT_SOURCE("overlay_motion_path_line_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_motion_path_line_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_motion_path_line)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_motion_path_line_clipped_no_geom)
METAL_BACKEND_ONLY()
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_motion_path_line_no_geom)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

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

GPU_SHADER_CREATE_INFO(overlay_motion_path_point_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_motion_path_point)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Image Empty
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_image_iface)
SMOOTH(VEC2, uvs)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_image)
DO_STATIC_COMPILATION()
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
ADDITIONAL_INFO(draw_mesh)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_image_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_image)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

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

GPU_SHADER_CREATE_INFO(overlay_gpencil_canvas_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_gpencil_canvas)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particle
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_particle_iface)
FLAT(VEC4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_particle)
SAMPLER(0, FLOAT_1D, weightTex)
PUSH_CONSTANT(VEC4, ucolor) /* Draw-size packed in alpha. */
VERTEX_IN(0, VEC3, part_pos)
VERTEX_IN(1, VEC4, part_rot)
VERTEX_IN(2, FLOAT, part_val)
VERTEX_OUT(overlay_particle_iface)
VERTEX_SOURCE("overlay_particle_vert.glsl")
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_particle_dot)
DO_STATIC_COMPILATION()
DEFINE("USE_DOTS")
DEFINE_VALUE("vclass", "0")
DEFINE_VALUE("pos", "vec3(0.0)")
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
FRAGMENT_SOURCE("overlay_particle_frag.glsl")
ADDITIONAL_INFO(overlay_particle)
ADDITIONAL_INFO(draw_mesh)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_particle_dot_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_particle_dot)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_particle_shape)
DO_STATIC_COMPILATION()
/* Instantiated Attrs. */
VERTEX_IN(3, VEC3, pos)
VERTEX_IN(4, INT, vclass)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_SOURCE("overlay_varying_color.glsl")
ADDITIONAL_INFO(overlay_particle)
ADDITIONAL_INFO(draw_modelmat)
ADDITIONAL_INFO(draw_resource_id_uniform)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_particle_shape_next)
DO_STATIC_COMPILATION()
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
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_particle_hair_next)
DO_STATIC_COMPILATION()
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
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_object_infos_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_particle_shape_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_particle_shape)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */
