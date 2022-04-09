/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Edit Mesh
 * \{ */

GPU_SHADER_INTERFACE_INFO(overlay_edit_mesh_color_iface, "").flat(Type::VEC4, "finalColor");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_common)
    .define("blender_srgb_to_framebuffer_space(a)", "a")
    .sampler(0, ImageType::DEPTH_2D, "depthTex")
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::BOOL, "selectFaces")
    .push_constant(Type::BOOL, "selectEdges")
    .push_constant(Type::FLOAT, "alpha")
    .push_constant(Type::IVEC4, "dataMask")
    .vertex_source("edit_mesh_vert.glsl")
    .additional_info("draw_modelmat", "draw_globals");

GPU_SHADER_INTERFACE_INFO(overlay_edit_mesh_vert_iface, "")
    .smooth(Type::VEC4, "finalColor")
    .smooth(Type::FLOAT, "vertexCrease");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_vert)
    .do_static_compilation(true)
    .builtins(BuiltinBits::POINT_SIZE)
    .define("srgbTarget", "false") /* Colors are already in linear space. */
    .define("VERT")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::IVEC4, "data")
    .vertex_in(2, Type::VEC3, "vnor")
    .vertex_out(overlay_edit_mesh_vert_iface)
    .fragment_source("gpu_shader_point_varying_color_frag.glsl")
    .additional_info("overlay_edit_mesh_common");

GPU_SHADER_INTERFACE_INFO(overlay_edit_mesh_edge_iface, "geometry_in")
    .smooth(Type::VEC4, "finalColor_")
    .smooth(Type::VEC4, "finalColorOuter_")
    .smooth(Type::INT, "selectOverride_");

GPU_SHADER_INTERFACE_INFO(overlay_edit_mesh_edge_geom_iface, "geometry_out")
    .smooth(Type::VEC4, "finalColor")
    .flat(Type::VEC4, "finalColorOuter")
    .no_perspective(Type::FLOAT, "edgeCoord");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_edge)
    .do_static_compilation(true)
    .define("EDGE")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::IVEC4, "data")
    .vertex_in(2, Type::VEC3, "vnor")
    .push_constant(Type::BOOL, "do_smooth_wire")
    .vertex_out(overlay_edit_mesh_edge_iface)
    .geometry_out(overlay_edit_mesh_edge_geom_iface)
    .geometry_layout(PrimitiveIn::LINES, PrimitiveOut::TRIANGLE_STRIP, 4)
    .geometry_source("edit_mesh_geom.glsl")
    .fragment_source("edit_mesh_frag.glsl")
    .additional_info("overlay_edit_mesh_common");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_edge_flat)
    .do_static_compilation(true)
    .define("FLAT")
    .additional_info("overlay_edit_mesh_edge");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_face)
    .do_static_compilation(true)
    .define("srgbTarget", "false") /* Colors are already in linear space. */
    .define("FACE")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::IVEC4, "data")
    .vertex_in(2, Type::VEC3, "vnor")
    .vertex_out(overlay_edit_mesh_color_iface)
    .fragment_source("gpu_shader_3D_smooth_color_frag.glsl")
    .additional_info("overlay_edit_mesh_common");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_facedot)
    .do_static_compilation(true)
    .define("FACEDOT")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::IVEC4, "data")
    .vertex_in(2, Type::VEC4, "norAndFlag")
    .define("vnor", "norAndFlag.xyz")
    .vertex_out(overlay_edit_mesh_color_iface)
    .fragment_source("gpu_shader_point_varying_color_frag.glsl")
    .additional_info("overlay_edit_mesh_common");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_normal)
    .do_static_compilation(true)
    .define("srgbTarget", "false") /* Colors are already in linear space. */
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::VEC4, "lnor")
    .vertex_in(2, Type::VEC4, "vnor")
    .vertex_in(3, Type::VEC4, "norAndFlag")
    .sampler(0, ImageType::DEPTH_2D, "depthTex")
    .push_constant(Type::FLOAT, "normalSize")
    .push_constant(Type::FLOAT, "normalScreenSize")
    .push_constant(Type::FLOAT, "alpha")
    .push_constant(Type::BOOL, "isConstantScreenSizeNormals")
    .vertex_out(overlay_edit_mesh_color_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("edit_mesh_normal_vert.glsl")
    .fragment_source("gpu_shader_flat_color_frag.glsl")
    .additional_info("draw_modelmat_instanced_attr", "draw_globals");

GPU_SHADER_INTERFACE_INFO(overlay_edit_mesh_analysis_iface, "").smooth(Type::VEC4, "weightColor");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_analysis)
    .do_static_compilation(true)
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::FLOAT, "weight")
    .sampler(0, ImageType::FLOAT_1D, "weightTex")
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_out(overlay_edit_mesh_analysis_iface)
    .vertex_source("edit_mesh_analysis_vert.glsl")
    .fragment_source("edit_mesh_analysis_frag.glsl")
    .additional_info("draw_modelmat");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_skin_root)
    .do_static_compilation(true)
    .define("srgbTarget", "false") /* Colors are already in linear space. */
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::FLOAT, "size")
    .vertex_in(2, Type::VEC3, "local_pos")
    .vertex_out(overlay_edit_mesh_color_iface)
    .fragment_out(0, Type::VEC4, "fragColor")
    .vertex_source("edit_mesh_skin_root_vert.glsl")
    .fragment_source("gpu_shader_flat_color_frag.glsl")
    .additional_info("draw_modelmat_instanced_attr", "draw_globals");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_vert_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_edit_mesh_vert", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_edge_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_edit_mesh_edge", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_edge_flat_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_edit_mesh_edge_flat", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_face_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_edit_mesh_face", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_facedot_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_edit_mesh_facedot", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_normal_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_edit_mesh_normal", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_analysis_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_edit_mesh_analysis", "drw_clipped");

GPU_SHADER_CREATE_INFO(overlay_edit_mesh_skin_root_clipped)
    .do_static_compilation(true)
    .additional_info("overlay_edit_mesh_skin_root", "drw_clipped");

/** \} */
