/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_film)
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .sampler(1, ImageType::FLOAT_2D, "combined_tx")
    .sampler(2, ImageType::FLOAT_2D, "vector_tx")
    .sampler(3, ImageType::FLOAT_2D_ARRAY, "rp_color_tx")
    .sampler(4, ImageType::FLOAT_2D_ARRAY, "rp_value_tx")
    /* Color History for TAA needs to be sampler to leverage bilinear sampling. */
    .sampler(5, ImageType::FLOAT_2D, "in_combined_tx")
    .sampler(6, ImageType::FLOAT_2D, "cryptomatte_tx")
    .image(0, GPU_R32F, Qualifier::READ, ImageType::FLOAT_2D_ARRAY, "in_weight_img")
    .image(1, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D_ARRAY, "out_weight_img")
    /* Color History for TAA needs to be sampler to leverage bilinear sampling. */
    //.image(2, GPU_RGBA16F, Qualifier::READ, ImageType::FLOAT_2D, "in_combined_img")
    .image(3, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_combined_img")
    .image(4, GPU_R32F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "depth_img")
    .image(5, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D_ARRAY, "color_accum_img")
    .image(6, GPU_R16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D_ARRAY, "value_accum_img")
    .image(7, GPU_RGBA32F, Qualifier::READ_WRITE, ImageType::FLOAT_2D_ARRAY, "cryptomatte_img")
    .specialization_constant(Type::INT, "enabled_categories", 0)
    .specialization_constant(Type::INT, "samples_len", 0)
    .specialization_constant(Type::BOOL, "use_reprojection", false)
    .additional_info("eevee_shared")
    .additional_info("eevee_global_ubo")
    .additional_info("eevee_velocity_camera")
    .additional_info("draw_view");

GPU_SHADER_CREATE_INFO(eevee_film_frag)
    .do_static_compilation(true)
    .fragment_out(0, Type::VEC4, "out_color")
    .fragment_source("eevee_film_frag.glsl")
    .additional_info("draw_fullscreen", "eevee_film")
    .depth_write(DepthWrite::ANY);

GPU_SHADER_CREATE_INFO(eevee_film_comp)
    .do_static_compilation(true)
    .local_group_size(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
    .compute_source("eevee_film_comp.glsl")
    .additional_info("eevee_film");

GPU_SHADER_CREATE_INFO(eevee_film_cryptomatte_post)
    .do_static_compilation(true)
    .image(0, GPU_RGBA32F, Qualifier::READ_WRITE, ImageType::FLOAT_2D_ARRAY, "cryptomatte_img")
    .image(1, GPU_R32F, Qualifier::READ, ImageType::FLOAT_2D_ARRAY, "weight_img")
    .push_constant(Type::INT, "cryptomatte_layer_len")
    .push_constant(Type::INT, "cryptomatte_samples_per_layer")
    .local_group_size(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
    .compute_source("eevee_film_cryptomatte_post_comp.glsl")
    .additional_info("eevee_shared");
