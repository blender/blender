/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Outline Pre-pass
 * \{ */

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_outline_prepass_iface, interp)
FLAT(UINT, ob_id)
GPU_SHADER_NAMED_INTERFACE_END(interp)

GPU_SHADER_CREATE_INFO(overlay_outline_prepass)
PUSH_CONSTANT(BOOL, isTransform)
VERTEX_OUT(overlay_outline_prepass_iface)
/* Using uint because 16bit uint can contain more ids than int. */
FRAGMENT_OUT(0, UINT, out_object_id)
FRAGMENT_SOURCE("overlay_outline_prepass_frag.glsl")
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_mesh)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
VERTEX_SOURCE("overlay_outline_prepass_vert.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_object_infos_new)
ADDITIONAL_INFO(overlay_outline_prepass)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_outline_prepass_mesh)

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_outline_prepass_wire_iface, vert)
FLAT(VEC3, pos)
GPU_SHADER_NAMED_INTERFACE_END(vert)

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_curves)
DO_STATIC_COMPILATION()
VERTEX_SOURCE("overlay_outline_prepass_curves_vert.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_hair_new)
ADDITIONAL_INFO(draw_object_infos_new)
ADDITIONAL_INFO(overlay_outline_prepass)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_outline_prepass_curves)

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_wire)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_outline_prepass)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_mesh_new)
ADDITIONAL_INFO(draw_object_infos_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(gpu_index_buffer_load)
STORAGE_BUF_FREQ(0, READ, float, pos[], GEOMETRY)
PUSH_CONSTANT(IVEC2, gpu_attr_0)
VERTEX_SOURCE("overlay_outline_prepass_wire_vert.glsl")
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_outline_prepass_wire)

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_outline_prepass_gpencil_flat_iface, gp_interp_flat)
FLAT(VEC2, aspect)
FLAT(VEC4, sspos)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp_flat)
GPU_SHADER_NAMED_INTERFACE_INFO(overlay_outline_prepass_gpencil_noperspective_iface,
                                gp_interp_noperspective)
NO_PERSPECTIVE(VEC2, thickness)
NO_PERSPECTIVE(FLOAT, hardness)
GPU_SHADER_NAMED_INTERFACE_END(gp_interp_noperspective)

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_gpencil)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(BOOL, isTransform)
VERTEX_OUT(overlay_outline_prepass_iface)
VERTEX_OUT(overlay_outline_prepass_gpencil_flat_iface)
VERTEX_OUT(overlay_outline_prepass_gpencil_noperspective_iface)
VERTEX_SOURCE("overlay_outline_prepass_gpencil_vert.glsl")
PUSH_CONSTANT(BOOL, gpStrokeOrder3d) /* TODO(fclem): Move to a GPencil object UBO. */
PUSH_CONSTANT(VEC4, gpDepthPlane)    /* TODO(fclem): Move to a GPencil object UBO. */
/* Using uint because 16bit uint can contain more ids than int. */
FRAGMENT_OUT(0, UINT, out_object_id)
FRAGMENT_SOURCE("overlay_outline_prepass_gpencil_frag.glsl")
DEPTH_WRITE(DepthWrite::ANY)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_gpencil_new)
ADDITIONAL_INFO(draw_object_infos_new)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_outline_prepass_gpencil)

GPU_SHADER_CREATE_INFO(overlay_outline_prepass_pointcloud)
DO_STATIC_COMPILATION()
VERTEX_SOURCE("overlay_outline_prepass_pointcloud_vert.glsl")
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_modelmat_new)
ADDITIONAL_INFO(draw_resource_handle_new)
ADDITIONAL_INFO(draw_globals)
ADDITIONAL_INFO(draw_pointcloud_new)
ADDITIONAL_INFO(draw_object_infos_new)
ADDITIONAL_INFO(overlay_outline_prepass)
GPU_SHADER_CREATE_END()

OVERLAY_INFO_CLIP_VARIATION(overlay_outline_prepass_pointcloud)

/** \} */

/* -------------------------------------------------------------------- */
/** \name Outline Rendering
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_outline_detect)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(FLOAT, alphaOcclu)
PUSH_CONSTANT(BOOL, isXrayWires)
PUSH_CONSTANT(BOOL, doAntiAliasing)
PUSH_CONSTANT(BOOL, doThickOutlines)
SAMPLER(0, UINT_2D, outlineId)
SAMPLER(1, DEPTH_2D, outlineDepth)
SAMPLER(2, DEPTH_2D, sceneDepth)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
FRAGMENT_SOURCE("overlay_outline_detect_frag.glsl")
ADDITIONAL_INFO(draw_fullscreen)
ADDITIONAL_INFO(draw_view)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/** \} */
