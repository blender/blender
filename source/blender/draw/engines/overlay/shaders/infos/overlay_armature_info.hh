/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(overlay_frag_output)
FRAGMENT_OUT(0, VEC4, fragColor)
FRAGMENT_OUT(1, VEC4, lineOutput)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(overlay_armature_wire_iface)
FLAT(VEC4, finalColor)
FLAT(VEC2, edgeStart)
NO_PERSPECTIVE(VEC2, edgePos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_common)
PUSH_CONSTANT(FLOAT, alpha)
ADDITIONAL_INFO(draw_view)
GPU_SHADER_CREATE_END()

/* -------------------------------------------------------------------- */
/** \name Armature Sphere
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_sphere_outline)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC2, pos)
/* Per instance. */
VERTEX_IN(1, MAT4, inst_obmat)
VERTEX_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_sphere_outline_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_sphere_outline_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_sphere_outline)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(overlay_armature_sphere_solid_iface)
FLAT(VEC3, finalStateColor)
FLAT(VEC3, finalBoneColor)
FLAT(MAT4, sphereMatrix)
SMOOTH(VEC3, viewPosition)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_sphere_solid)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC2, pos)
/* Per instance. */
VERTEX_IN(1, VEC4, color)
VERTEX_IN(2, MAT4, inst_obmat)
DEPTH_WRITE(DepthWrite::GREATER)
VERTEX_OUT(overlay_armature_sphere_solid_iface)
VERTEX_SOURCE("overlay_armature_sphere_solid_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_sphere_solid_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
DEPTH_WRITE(DepthWrite::ANY)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_sphere_solid_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_sphere_solid)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Shapes
 * \{ */

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_armature_shape_outline_iface, geom_in)
SMOOTH(VEC4, pPos)
SMOOTH(VEC3, vPos)
SMOOTH(VEC2, ssPos)
SMOOTH(VEC4, vColSize)
GPU_SHADER_NAMED_INTERFACE_END(geom_in)
GPU_SHADER_NAMED_INTERFACE_INFO(overlay_armature_shape_outline_flat_iface, geom_flat_in)
FLAT(INT, inverted)
GPU_SHADER_NAMED_INTERFACE_END(geom_flat_in)

GPU_SHADER_INTERFACE_INFO(overlay_armature_shape_outline_no_geom_iface)
FLAT(VEC4, finalColor)
FLAT(VEC2, edgeStart)
NO_PERSPECTIVE(VEC2, edgePos)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_outline)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
/* Per instance. */
VERTEX_IN(3, MAT4, inst_obmat)
VERTEX_OUT(overlay_armature_shape_outline_iface)
VERTEX_OUT(overlay_armature_shape_outline_flat_iface)
GEOMETRY_LAYOUT(PrimitiveIn::LINES_ADJACENCY, PrimitiveOut::LINE_STRIP, 2)
GEOMETRY_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_shape_outline_vert.glsl")
GEOMETRY_SOURCE("overlay_armature_shape_outline_geom.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_outline_no_geom)
METAL_BACKEND_ONLY()
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
/* Per instance. */
VERTEX_IN(3, MAT4, inst_obmat)
VERTEX_OUT(overlay_armature_shape_outline_no_geom_iface)
VERTEX_SOURCE("overlay_armature_shape_outline_vert_no_geom.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_outline_next)
DO_STATIC_COMPILATION()
STORAGE_BUF_FREQ(0, READ, float, pos[], GEOMETRY)
STORAGE_BUF(1, READ, mat4, data_buf[])
PUSH_CONSTANT(IVEC2, gpu_attr_0)
VERTEX_OUT(overlay_armature_shape_outline_no_geom_iface)
VERTEX_SOURCE("overlay_armature_shape_outline_next_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_outline_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_shape_outline)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_outline_clipped_no_geom)
METAL_BACKEND_ONLY()
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_shape_outline_no_geom)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(overlay_armature_shape_solid_iface)
SMOOTH(VEC4, finalColor)
FLAT(INT, inverted)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_solid)
DO_STATIC_COMPILATION()
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC3, nor)
/* Per instance. */
VERTEX_IN(2, MAT4, inst_obmat)
DEPTH_WRITE(DepthWrite::GREATER)
VERTEX_OUT(overlay_armature_shape_solid_iface)
VERTEX_SOURCE("overlay_armature_shape_solid_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_shape_solid_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_solid_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_shape_solid)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(overlay_armature_shape_wire_next_iface)
FLAT(VEC4, finalColor)
FLAT(FLOAT, wire_width)
NO_PERSPECTIVE(FLOAT, edgeCoord)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_armature_shape_wire_iface, geometry_in)
FLAT(VEC4, finalColor)
FLAT(FLOAT, wire_width)
GPU_SHADER_NAMED_INTERFACE_END(geometry_in)

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_armature_shape_wire_geom_iface, geometry_out)
FLAT(VEC4, finalColor)
FLAT(FLOAT, wire_width)
GPU_SHADER_NAMED_INTERFACE_END(geometry_out)

GPU_SHADER_NAMED_INTERFACE_INFO(overlay_armature_shape_wire_geom_noperspective_iface,
                                geometry_noperspective_out)
NO_PERSPECTIVE(FLOAT, edgeCoord)
GPU_SHADER_NAMED_INTERFACE_END(geometry_noperspective_out)

GPU_SHADER_CREATE_INFO(overlay_armature_shape_wire)
DO_STATIC_COMPILATION()
PUSH_CONSTANT(BOOL, do_smooth_wire)
VERTEX_IN(0, VEC3, pos)
/* Per instance. */
VERTEX_IN(2, MAT4, inst_obmat)
VERTEX_OUT(overlay_armature_shape_wire_iface)
VERTEX_SOURCE("overlay_armature_shape_wire_vert.glsl")
GEOMETRY_OUT(overlay_armature_shape_wire_geom_iface)
GEOMETRY_OUT(overlay_armature_shape_wire_geom_noperspective_iface)
GEOMETRY_LAYOUT(PrimitiveIn::LINES, PrimitiveOut::TRIANGLE_STRIP, 4)
GEOMETRY_SOURCE("overlay_armature_shape_wire_geom.glsl")
FRAGMENT_SOURCE("overlay_armature_shape_wire_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.h")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_shape_wire_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_shape_wire)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

#ifdef WITH_METAL_BACKEND
GPU_SHADER_CREATE_INFO(overlay_armature_shape_wire_no_geom)
METAL_BACKEND_ONLY()
DO_STATIC_COMPILATION()
PUSH_CONSTANT(BOOL, do_smooth_wire)
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(2, MAT4, inst_obmat)
VERTEX_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_shape_wire_vert_no_geom.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.h")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()
#endif

GPU_SHADER_CREATE_INFO(overlay_armature_shape_wire_next)
DO_STATIC_COMPILATION()
DEFINE("NO_GEOM")
PUSH_CONSTANT(BOOL, do_smooth_wire)
STORAGE_BUF_FREQ(0, READ, float, pos[], GEOMETRY)
STORAGE_BUF(1, READ, mat4, data_buf[])
PUSH_CONSTANT(IVEC2, gpu_attr_0)
DEFINE_VALUE("inst_obmat", "data_buf[gl_InstanceID]")
VERTEX_OUT(overlay_armature_shape_wire_next_iface)
VERTEX_SOURCE("overlay_armature_shape_wire_next_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_shape_wire_frag.glsl")
TYPEDEF_SOURCE("overlay_shader_shared.h")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(gpu_index_buffer_load)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Envelope
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_envelope_outline)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.h")
VERTEX_IN(0, VEC2, pos0)
VERTEX_IN(1, VEC2, pos1)
VERTEX_IN(2, VEC2, pos2)
/* Per instance. */
VERTEX_IN(3, VEC4, headSphere)
VERTEX_IN(4, VEC4, tailSphere)
VERTEX_IN(5, VEC4, outlineColorSize)
VERTEX_IN(6, VEC3, xAxis)
VERTEX_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_envelope_outline_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_envelope_outline_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_envelope_outline)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

GPU_SHADER_INTERFACE_INFO(overlay_armature_envelope_solid_iface)
FLAT(VEC3, finalStateColor)
FLAT(VEC3, finalBoneColor)
SMOOTH(VEC3, normalView)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_envelope_solid)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.h")
VERTEX_IN(0, VEC3, pos)
/* Per instance. Assumed to be in world coordinate already. */
VERTEX_IN(1, VEC4, headSphere)
VERTEX_IN(2, VEC4, tailSphere)
VERTEX_IN(3, VEC3, xAxis)
VERTEX_IN(4, VEC3, stateColor)
VERTEX_IN(5, VEC3, boneColor)
VERTEX_OUT(overlay_armature_envelope_solid_iface)
PUSH_CONSTANT(BOOL, isDistance)
VERTEX_SOURCE("overlay_armature_envelope_solid_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_envelope_solid_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_envelope_solid_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_envelope_solid)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Stick
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_armature_stick_iface)
NO_PERSPECTIVE(FLOAT, colorFac)
FLAT(VEC4, finalWireColor)
FLAT(VEC4, finalInnerColor)
GPU_SHADER_INTERFACE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_stick)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.h")
/* Bone aligned screen space. */
VERTEX_IN(0, VEC2, pos)
VERTEX_IN(1, UINT, flag)
/* Per instance. Assumed to be in world coordinate already. */
VERTEX_IN(2, VEC3, boneStart)
VERTEX_IN(3, VEC3, boneEnd)
/* alpha encode if we do wire. If 0.0 we don't. */
VERTEX_IN(4, VEC4, wireColor)
VERTEX_IN(5, VEC4, boneColor)
VERTEX_IN(6, VEC4, headColor)
VERTEX_IN(7, VEC4, tailColor)
DEFINE_VALUE("do_wire", "(wireColor.a > 0.0)")
VERTEX_OUT(overlay_armature_stick_iface)
VERTEX_SOURCE("overlay_armature_stick_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_stick_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_stick_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_stick)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Degrees of Freedom
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_dof)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.h")
VERTEX_IN(0, VEC2, pos)
/* Per instance. Assumed to be in world coordinate already. */
VERTEX_IN(1, VEC4, color)
VERTEX_IN(2, MAT4, inst_obmat)
VERTEX_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_dof_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_dof_solid_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(overlay_armature_common)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_dof_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_dof)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Wire
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_wire)
DO_STATIC_COMPILATION()
TYPEDEF_SOURCE("overlay_shader_shared.h")
VERTEX_IN(0, VEC3, pos)
VERTEX_IN(1, VEC4, color)
PUSH_CONSTANT(FLOAT, alpha)
VERTEX_OUT(overlay_armature_wire_iface)
VERTEX_SOURCE("overlay_armature_wire_vert.glsl")
FRAGMENT_SOURCE("overlay_armature_wire_frag.glsl")
ADDITIONAL_INFO(overlay_frag_output)
ADDITIONAL_INFO(draw_mesh)
ADDITIONAL_INFO(draw_globals)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(overlay_armature_wire_clipped)
DO_STATIC_COMPILATION()
ADDITIONAL_INFO(overlay_armature_wire)
ADDITIONAL_INFO(drw_clipped)
GPU_SHADER_CREATE_END()

/** \} */
