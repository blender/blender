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
#endif

#include "overlay_common_info.hh"

/* -------------------------------------------------------------------- */
/** \name Extra shapes
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_iface)
NO_PERSPECTIVE(float2, edgePos)
FLAT(float2, edgeStart)
FLAT(float4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, int, vclass)
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
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
FLAT(float4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_grid_base)
SAMPLER(0, DEPTH_2D, depthBuffer)
PUSH_CONSTANT(float4x4, gridModelMatrix)
PUSH_CONSTANT(bool, isTransform)
VERTEX_OUT(overlay_extra_grid_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_extra_lightprobe_grid_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_extra_grid, overlay_extra_grid_base)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Ground-lines
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_extra_groundline)
DO_STATIC_COMPILATION()
VERTEX_IN(0, float3, pos)
/* Instance attributes. */
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_extra_groundline_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
STORAGE_BUF(0, READ, float4, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS(overlay_extra_groundline)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extra wires
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_wire_iface)
NO_PERSPECTIVE(float2, stipple_coord)
FLAT(float2, stipple_start)
FLAT(float4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_wire_base)
VERTEX_OUT(overlay_extra_wire_iface)
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_extra_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_wire_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.hh")
STORAGE_BUF(0, READ, VertexData, data_buf[])
PUSH_CONSTANT(int, colorid)
DEFINE_VALUE("pos", "data_buf[gl_VertexID].pos_.xyz")
DEFINE_VALUE("color", "data_buf[gl_VertexID].color_")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_extra_wire, overlay_extra_wire_base)

GPU_SHADER_CREATE_INFO(overlay_extra_wire_object_base)
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float4, color)
/* If colorid is equal to 0 (i.e: Not specified) use color attribute and stippling. */
VERTEX_IN(2, int, colorid)
VERTEX_OUT(overlay_extra_wire_iface)
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_extra_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_wire_frag.glsl")
DEFINE("OBJECT_WIRE")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_extra_wire_object, overlay_extra_wire_object_base)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Extra points
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_extra_point_iface)
FLAT(float4, radii)
FLAT(float4, fillColor)
FLAT(float4, outlineColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_point_base)
/* TODO(fclem): Move the vertex shader to Overlay engine and remove this bypass. */
DEFINE_VALUE("blender_srgb_to_framebuffer_space(a)", "a")
VERTEX_OUT(overlay_extra_point_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_extra_point_vert.glsl")
FRAGMENT_SOURCE("overlay_point_varying_color_varying_outline_aa_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
TYPEDEF_SOURCE("overlay_shader_shared.hh")
STORAGE_BUF(0, READ, VertexData, data_buf[])
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_extra_point, overlay_extra_point_base)

GPU_SHADER_INTERFACE_INFO(overlay_extra_loose_point_iface)
SMOOTH(float4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_extra_loose_point_base)
VERTEX_OUT(overlay_extra_loose_point_iface)
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_extra_loose_point_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_loose_point_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.hh")
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
SMOOTH(float4, color)
GPU_SHADER_NAMED_INTERFACE_END(interp)

GPU_SHADER_CREATE_INFO(overlay_motion_path_line)
DO_STATIC_COMPILATION()
STORAGE_BUF_FREQ(0, READ, float, pos[], GEOMETRY)
PUSH_CONSTANT(int2, gpu_attr_0)
PUSH_CONSTANT(int4, mpathLineSettings)
PUSH_CONSTANT(bool, selected)
PUSH_CONSTANT(float3, customColorPre)
PUSH_CONSTANT(float3, customColorPost)
PUSH_CONSTANT(int, lineThickness) /* In pixels. */
PUSH_CONSTANT(float4x4, camera_space_matrix)
VERTEX_OUT(overlay_motion_path_line_iface)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_motion_path_line_vert.glsl")
FRAGMENT_SOURCE("overlay_motion_path_line_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_motion_path_line)

GPU_SHADER_INTERFACE_INFO(overlay_motion_path_point_iface)
FLAT(float4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_motion_path_point)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, int, flag)
PUSH_CONSTANT(int4, mpathPointSettings)
PUSH_CONSTANT(bool, showKeyFrames)
PUSH_CONSTANT(float3, customColorPre)
PUSH_CONSTANT(float3, customColorPost)
PUSH_CONSTANT(float4x4, camera_space_matrix)
VERTEX_OUT(overlay_motion_path_point_iface)
FRAGMENT_OUT(0, float4, fragColor)
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
SMOOTH(float2, uvs)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_image_base)
PUSH_CONSTANT(bool, depthSet)
PUSH_CONSTANT(bool, isCameraBackground)
PUSH_CONSTANT(bool, imgPremultiplied)
PUSH_CONSTANT(bool, imgAlphaBlend)
PUSH_CONSTANT(float4, ucolor)
VERTEX_IN(0, float3, pos)
VERTEX_OUT(overlay_image_iface)
SAMPLER(0, FLOAT_2D, imgTexture)
FRAGMENT_OUT(0, float4, fragColor)
VERTEX_SOURCE("overlay_image_vert.glsl")
FRAGMENT_SOURCE("overlay_image_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_image, overlay_image_base)

GPU_SHADER_CREATE_INFO(overlay_image_depth_bias_base)
ADDITIONAL_INFO(overlay_image_base)
DEFINE("DEPTH_BIAS")
PUSH_CONSTANT(float4x4, depth_bias_winmat)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_image_depth_bias, overlay_image_depth_bias_base)

/** \} */

/* -------------------------------------------------------------------- */
/** \name GPencil Canvas
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_gpencil_canvas)
DO_STATIC_COMPILATION()
VERTEX_OUT(overlay_extra_iface)
PUSH_CONSTANT(float4, color)
PUSH_CONSTANT(float3, xAxis)
PUSH_CONSTANT(float3, yAxis)
PUSH_CONSTANT(float3, origin)
PUSH_CONSTANT(int, halfLineCount)
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_edit_gpencil_canvas_vert.glsl")
FRAGMENT_SOURCE("overlay_extra_frag.glsl")
ADDITIONAL_INFO(draw_mesh)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_gpencil_canvas)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Particle
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_particle_iface)
FLAT(float4, finalColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_particle_dot_base)
SAMPLER(0, FLOAT_1D, weightTex)
PUSH_CONSTANT(float4, ucolor) /* Draw-size packed in alpha. */
VERTEX_IN(0, float3, part_pos)
VERTEX_IN(1, float4, part_rot)
VERTEX_IN(2, float, part_val)
VERTEX_OUT(overlay_particle_iface)
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_particle_vert.glsl")
FRAGMENT_SOURCE("overlay_particle_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_particle_dot, overlay_particle_dot_base)

GPU_SHADER_CREATE_INFO(overlay_particle_shape_base)
TYPEDEF_SOURCE("overlay_shader_shared.hh")
SAMPLER(0, FLOAT_1D, weightTex)
PUSH_CONSTANT(float4, ucolor) /* Draw-size packed in alpha. */
PUSH_CONSTANT(int, shape_type)
/* Use first attribute to only bind one buffer. */
STORAGE_BUF_FREQ(0, READ, ParticlePointData, part_pos[], GEOMETRY)
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_particle_shape_vert.glsl")
FRAGMENT_SOURCE("overlay_particle_shape_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_particle_shape, overlay_particle_shape_base)

GPU_SHADER_CREATE_INFO(overlay_particle_hair_base)
TYPEDEF_SOURCE("overlay_shader_shared.hh")
VERTEX_IN(0, float3, pos)
VERTEX_IN(1, float3, nor)
PUSH_CONSTANT(int, colorType)
PUSH_CONSTANT(bool, isTransform)
PUSH_CONSTANT(bool, useColoring)
VERTEX_OUT(overlay_extra_iface)
FRAGMENT_OUT(0, float4, fragColor)
FRAGMENT_OUT(1, float4, lineOutput)
VERTEX_SOURCE("overlay_particle_hair_vert.glsl")
FRAGMENT_SOURCE("overlay_particle_shape_frag.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_object_infos)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_VARIATIONS_MODELMAT(overlay_particle_hair, overlay_particle_hair_base)

/** \} */
