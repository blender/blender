/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(draw_hair_refine_compute)
    .local_group_size(1, 1)
    .storage_buf(0, Qualifier::WRITE, "vec4", "posTime[]")
    .sampler(0, ImageType::FLOAT_BUFFER, "hairPointBuffer")
    .sampler(1, ImageType::UINT_BUFFER, "hairStrandBuffer")
    .sampler(2, ImageType::UINT_BUFFER, "hairStrandSegBuffer")
    .push_constant(Type::MAT4, "hairDupliMatrix")
    .push_constant(Type::BOOL, "hairCloseTip")
    .push_constant(Type::FLOAT, "hairRadShape")
    .push_constant(Type::FLOAT, "hairRadTip")
    .push_constant(Type::FLOAT, "hairRadRoot")
    .push_constant(Type::INT, "hairThicknessRes")
    .push_constant(Type::INT, "hairStrandsRes")
    .push_constant(Type::INT, "hairStrandOffset")
    .compute_source("common_hair_refine_comp.glsl")
    .define("HAIR_PHASE_SUBDIV")
    .define("HAIR_SHADER")
    .define("DRW_HAIR_INFO")
    .do_static_compilation(true);
