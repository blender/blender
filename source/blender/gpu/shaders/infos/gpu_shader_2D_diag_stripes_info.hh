/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2022 Blender Foundation */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_diag_stripes)
    .vertex_in(0, Type::VEC2, "pos")
    .fragment_out(0, Type::VEC4, "fragColor")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::VEC4, "color1")
    .push_constant(Type::VEC4, "color2")
    .push_constant(Type::INT, "size1")
    .push_constant(Type::INT, "size2")
    .vertex_source("gpu_shader_2D_vert.glsl")
    .fragment_source("gpu_shader_diag_stripes_frag.glsl")
    .do_static_compilation(true);
