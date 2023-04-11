/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Select ID for Edit Mesh Selection
 * \{ */

GPU_SHADER_INTERFACE_INFO(select_id_iface, "").flat(Type::INT, "id");

GPU_SHADER_CREATE_INFO(select_id_flat)
    .push_constant(Type::FLOAT, "sizeVertex")
    .push_constant(Type::INT, "offset")
    .push_constant(Type::FLOAT, "retopologyOffset")
    .vertex_in(0, Type::VEC3, "pos")
    .vertex_in(1, Type::INT, "index")
    .vertex_out(select_id_iface)
    .fragment_out(0, Type::UINT, "fragColor")
    .vertex_source("select_id_vert.glsl")
    .fragment_source("select_id_frag.glsl")
    .additional_info("draw_modelmat")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(select_id_uniform)
    .define("UNIFORM_ID")
    .push_constant(Type::FLOAT, "sizeVertex")
    .push_constant(Type::INT, "id")
    .push_constant(Type::FLOAT, "retopologyOffset")
    .vertex_in(0, Type::VEC3, "pos")
    .fragment_out(0, Type::UINT, "fragColor")
    .vertex_source("select_id_vert.glsl")
    .fragment_source("select_id_frag.glsl")
    .additional_info("draw_modelmat")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(select_id_flat_clipped)
    .additional_info("select_id_flat")
    .additional_info("drw_clipped")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(select_id_uniform_clipped)
    .additional_info("select_id_uniform")
    .additional_info("drw_clipped")
    .do_static_compilation(true);
/** \} */
