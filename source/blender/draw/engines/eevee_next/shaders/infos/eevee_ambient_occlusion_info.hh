/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_ambient_occlusion_pass)
    .compute_source("eevee_ambient_occlusion_pass_comp.glsl")
    .local_group_size(AMBIENT_OCCLUSION_PASS_TILE_SIZE, AMBIENT_OCCLUSION_PASS_TILE_SIZE)
    .image(0, GPU_RGBA16F, Qualifier::READ, ImageType::FLOAT_2D_ARRAY, "in_normal_img")
    .push_constant(Type::INT, "in_normal_img_layer_index")
    .image(1, GPU_RG16F, Qualifier::WRITE, ImageType::FLOAT_2D_ARRAY, "out_ao_img")
    .push_constant(Type::INT, "out_ao_img_layer_index")
    .additional_info("draw_view",
                     "eevee_shared",
                     "eevee_hiz_data",
                     "eevee_sampling_data",
                     "eevee_utility_texture",
                     "eevee_global_ubo")
    .do_static_compilation(true);
