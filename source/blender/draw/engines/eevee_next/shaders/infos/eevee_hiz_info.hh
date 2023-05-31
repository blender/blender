/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_hiz_data)
    .sampler(15, ImageType::FLOAT_2D, "hiz_tx")
    .uniform_buf(5, "HiZData", "hiz_buf");

GPU_SHADER_CREATE_INFO(eevee_hiz_update)
    .do_static_compilation(true)
    .local_group_size(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
    .storage_buf(0, Qualifier::READ_WRITE, "uint", "finished_tile_counter")
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .image(0, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_mip_0")
    .image(1, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_mip_1")
    .image(2, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_mip_2")
    .image(3, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_mip_3")
    .image(4, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_mip_4")
    .image(5, GPU_R32F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "out_mip_5")
    .image(6, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_mip_6")
    .image(7, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_mip_7")
    .push_constant(Type::BOOL, "update_mip_0")
    .compute_source("eevee_hiz_update_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_hiz_debug)
    .do_static_compilation(true)
    .fragment_out(0, Type::VEC4, "out_debug_color_add", DualBlend::SRC_0)
    .fragment_out(0, Type::VEC4, "out_debug_color_mul", DualBlend::SRC_1)
    .fragment_source("eevee_hiz_debug_frag.glsl")
    .additional_info("eevee_shared", "eevee_hiz_data", "draw_fullscreen");
