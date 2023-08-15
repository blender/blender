/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(overlay_frag_output)
    .fragment_out(0, Type::VEC4, "fragColor")
    .fragment_out(1, Type::VEC4, "lineOutput");

GPU_SHADER_INTERFACE_INFO(overlay_armature_wire_iface, "")
    .flat(Type::VEC4, "finalColor")
    .flat(Type::VEC2, "edgeStart")
    .no_perspective(Type::VEC2, "edgePos");

GPU_SHADER_CREATE_INFO(overlay_armature_common)
    .push_constant(Type::FLOAT, "alpha")
    .additional_info("draw_view");

/* -------------------------------------------------------------------- */
/** \name Armature Sphere
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_sphere_outline)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC2, "pos")
    /* Per instance. */
    .vertex_in(1, Type::MAT4, "inst_obmat")
    .vertex_out(overlay_armature_wire_iface)
    .vertex_source("overlay_armature_sphere_outline_vert.glsl")
    .fragment_source("overlay_armature_wire_frag.glsl")
    .additional_info("overlay_frag_output", "overlay_armature_common", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_armature_sphere_outline_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_armature_sphere_outline", "drw_clipped");

GPU_SHADER_INTERFACE_INFO(overlay_armature_sphere_solid_iface, "")
    .flat(Type::VEC3, "finalStateColor")
    .flat(Type::VEC3, "finalBoneColor")
    .flat(Type::MAT4, "sphereMatrix")
    .smooth(Type::VEC3, "viewPosition");

GPU_SHADER_CREATE_INFO(overlay_armature_sphere_solid)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC2, "pos")
    /* Per instance. */
    .vertex_in(1, Type::VEC4, "color")
    .vertex_in(2, Type::MAT4, "inst_obmat")
    .depth_write(DepthWrite::GREATER)
    .vertex_out(overlay_armature_sphere_solid_iface)
    .vertex_source("overlay_armature_sphere_solid_vert.glsl")
    .fragment_source("overlay_armature_sphere_solid_frag.glsl")
    .additional_info("overlay_frag_output", "overlay_armature_common", "draw_globals")
    .depth_write(DepthWrite::ANY);

GPU_SHADER_CREATE_INFO(overlay_armature_sphere_solid_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_armature_sphere_solid", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Shapes
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_armature_shape_outline_iface, "geom_in")
    .smooth(Type::VEC4, "pPos")
    .smooth(Type::VEC3, "vPos")
    .smooth(Type::VEC2, "ssPos")
    .smooth(Type::VEC2, "ssNor")
    .smooth(Type::VEC4, "vColSize")
    .flat(Type::INT, "inverted");

GPU_SHADER_INTERFACE_INFO(overlay_armature_shape_outline_no_geom_iface, "")
    .flat(Type::VEC4, "finalColor")
    .flat(Type::VEC2, "edgeStart")
    .no_perspective(Type::VEC2, "edgePos");

GPU_SHADER_CREATE_INFO(overlay_armature_shape_outline)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "snor")
    /* Per instance. */
    .vertex_in(2, Type::VEC4, "color")
    .vertex_in(3, Type::MAT4, "inst_obmat")
    .vertex_out(overlay_armature_shape_outline_iface)
    .geometry_layout(PrimitiveIn::LINES_ADJACENCY, PrimitiveOut::LINE_STRIP, 2)
    .geometry_out(overlay_armature_wire_iface)
    .vertex_source("overlay_armature_shape_outline_vert.glsl")
    .geometry_source("overlay_armature_shape_outline_geom.glsl")
    .fragment_source("overlay_armature_wire_frag.glsl")
    .additional_info("overlay_frag_output", "overlay_armature_common", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_armature_shape_outline_no_geom)
    .metal_backend_only(true)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "snor")
    /* Per instance. */
    .vertex_in(2, Type::VEC4, "color")
    .vertex_in(3, Type::MAT4, "inst_obmat")
    .vertex_out(overlay_armature_shape_outline_no_geom_iface)
    .vertex_source("overlay_armature_shape_outline_vert_no_geom.glsl")
    .fragment_source("overlay_armature_wire_frag.glsl")
    .additional_info("overlay_frag_output", "overlay_armature_common", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_armature_shape_outline_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_armature_shape_outline", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_armature_shape_outline_clipped_no_geom)
    .metal_backend_only(true)
    .do_static_compilation(true)
    .additional_info("overlay_armature_shape_outline_no_geom", "drw_clipped");

GPU_SHADER_INTERFACE_INFO(overlay_armature_shape_solid_iface, "")
    .smooth(Type::VEC4, "finalColor")
    .flat(Type::INT, "inverted");

GPU_SHADER_CREATE_INFO(overlay_armature_shape_solid)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "nor")
    /* Per instance. */
    .vertex_in(2, Type::MAT4, "inst_obmat")
    .depth_write(DepthWrite::GREATER)
    .vertex_out(overlay_armature_shape_solid_iface)
    .vertex_source("overlay_armature_shape_solid_vert.glsl")
    .fragment_source("overlay_armature_shape_solid_frag.glsl")
    .additional_info("overlay_frag_output", "overlay_armature_common", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_armature_shape_solid_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_armature_shape_solid", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_armature_shape_wire)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC3, "nor")
    /* Per instance. */
    .vertex_in(2, Type::MAT4, "inst_obmat")
    .vertex_out(overlay_armature_wire_iface)
    .vertex_source("overlay_armature_shape_wire_vert.glsl")
    .fragment_source("overlay_armature_wire_frag.glsl")
    .additional_info("overlay_frag_output", "overlay_armature_common", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_armature_shape_wire_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_armature_shape_wire", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Envelope
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_envelope_outline)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC2, "pos0")
    .vertex_in(1, Type::VEC2, "pos1")
    .vertex_in(2, Type::VEC2, "pos2")
    /* Per instance. */
    .vertex_in(3, Type::VEC4, "headSphere")
    .vertex_in(4, Type::VEC4, "tailSphere")
    .vertex_in(5, Type::VEC4, "outlineColorSize")
    .vertex_in(6, Type::VEC3, "xAxis")
    .vertex_out(overlay_armature_wire_iface)
    .vertex_source("overlay_armature_envelope_outline_vert.glsl")
    .fragment_source("overlay_armature_wire_frag.glsl")
    .additional_info("overlay_frag_output", "overlay_armature_common", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_armature_envelope_outline_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_armature_envelope_outline", "drw_clipped");

GPU_SHADER_INTERFACE_INFO(overlay_armature_envelope_solid_iface, "")
    .flat(Type::VEC3, "finalStateColor")
    .flat(Type::VEC3, "finalBoneColor")
    .smooth(Type::VEC3, "normalView");

GPU_SHADER_CREATE_INFO(overlay_armature_envelope_solid)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    /* Per instance. Assumed to be in world coordinate already. */
    .vertex_in(1, Type::VEC4, "headSphere")
    .vertex_in(2, Type::VEC4, "tailSphere")
    .vertex_in(3, Type::VEC3, "xAxis")
    .vertex_in(4, Type::VEC3, "stateColor")
    .vertex_in(5, Type::VEC3, "boneColor")
    .vertex_out(overlay_armature_envelope_solid_iface)
    .push_constant(Type::BOOL, "isDistance")
    .vertex_source("overlay_armature_envelope_solid_vert.glsl")
    .fragment_source("overlay_armature_envelope_solid_frag.glsl")
    .additional_info("overlay_frag_output", "overlay_armature_common");

GPU_SHADER_CREATE_INFO(overlay_armature_envelope_solid_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_armature_envelope_solid", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Stick
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_armature_stick_iface, "")
    .no_perspective(Type::FLOAT, "colorFac")
    .flat(Type::VEC4, "finalWireColor")
    .flat(Type::VEC4, "finalInnerColor");

GPU_SHADER_CREATE_INFO(overlay_armature_stick)
    .do_static_compilation(true)
    /* Bone aligned screen space. */
    .vertex_in(0, Type::VEC2, "pos")
    .vertex_in(1, Type::UINT, "flag")
    /* Per instance. Assumed to be in world coordinate already. */
    .vertex_in(2, Type::VEC3, "boneStart")
    .vertex_in(3, Type::VEC3, "boneEnd")
    /* alpha encode if we do wire. If 0.0 we don't. */
    .vertex_in(4, Type::VEC4, "wireColor")
    .vertex_in(5, Type::VEC4, "boneColor")
    .vertex_in(6, Type::VEC4, "headColor")
    .vertex_in(7, Type::VEC4, "tailColor")
    .define("do_wire", "(wireColor.a > 0.0)")
    .vertex_out(overlay_armature_stick_iface)
    .vertex_source("overlay_armature_stick_vert.glsl")
    .fragment_source("overlay_armature_stick_frag.glsl")
    .additional_info("overlay_frag_output", "overlay_armature_common", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_armature_stick_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_armature_stick", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Degrees of Freedom
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_dof)
    .vertex_in(0, Type::VEC2, "pos")
    /* Per instance. Assumed to be in world coordinate already. */
    .vertex_in(1, Type::VEC4, "color")
    .vertex_in(2, Type::MAT4, "inst_obmat")
    .vertex_out(overlay_armature_wire_iface)
    .vertex_source("overlay_armature_dof_vert.glsl")
    .additional_info("overlay_frag_output", "overlay_armature_common", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_armature_dof_wire)
    .do_static_compilation(true)
    .fragment_source("overlay_armature_dof_solid_frag.glsl")
    .additional_info("overlay_armature_dof");

GPU_SHADER_CREATE_INFO(overlay_armature_dof_wire_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_armature_dof_wire", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_armature_dof_solid)
    .do_static_compilation(true)
    .fragment_source("overlay_armature_dof_solid_frag.glsl")
    .additional_info("overlay_armature_dof");

GPU_SHADER_CREATE_INFO(overlay_armature_dof_solid_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_armature_dof_solid", "drw_clipped");

/** \} */

/* -------------------------------------------------------------------- */
/** \name Armature Wire
 * \{ */

GPU_SHADER_CREATE_INFO(overlay_armature_wire)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC4, "color")
    .push_constant(Type::FLOAT, "alpha")
    .vertex_out(overlay_armature_wire_iface)
    .vertex_source("overlay_armature_wire_vert.glsl")
    .fragment_source("overlay_armature_wire_frag.glsl")
    .additional_info("overlay_frag_output", "draw_mesh", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_armature_wire_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_armature_wire", "drw_clipped");

/** \} */
