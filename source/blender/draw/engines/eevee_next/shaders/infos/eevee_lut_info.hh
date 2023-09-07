/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"

GPU_SHADER_CREATE_INFO(eevee_lut)
    .local_group_size(LUT_WORKGROUP_SIZE, LUT_WORKGROUP_SIZE, 1)
    .push_constant(Type::INT, "table_type")
    .push_constant(Type::IVEC3, "table_extent")
    .image(0, GPU_RGBA32F, Qualifier::READ_WRITE, ImageType::FLOAT_3D, "table_img")
    .additional_info("eevee_shared")
    .compute_source("eevee_lut_comp.glsl")
    .do_static_compilation(true);
