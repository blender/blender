/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_film)
    .uniform_buf(1, "FilmData", "film_buf")
    .sampler(0, ImageType::DEPTH_2D, "depth_tx")
    .sampler(1, ImageType::FLOAT_2D, "combined_tx")
    .sampler(2, ImageType::FLOAT_2D, "normal_tx")
    .sampler(3, ImageType::FLOAT_2D, "vector_tx")
    .sampler(4, ImageType::FLOAT_2D, "diffuse_light_tx")
    .sampler(5, ImageType::FLOAT_2D, "diffuse_color_tx")
    .sampler(6, ImageType::FLOAT_2D, "specular_light_tx")
    .sampler(7, ImageType::FLOAT_2D, "specular_color_tx")
    .sampler(8, ImageType::FLOAT_2D, "volume_light_tx")
    .sampler(9, ImageType::FLOAT_2D, "emission_tx")
    .sampler(10, ImageType::FLOAT_2D, "environment_tx")
    .sampler(11, ImageType::FLOAT_2D, "shadow_tx")
    .sampler(12, ImageType::FLOAT_2D, "ambient_occlusion_tx")
    .sampler(13, ImageType::FLOAT_2D_ARRAY, "aov_color_tx")
    .sampler(14, ImageType::FLOAT_2D_ARRAY, "aov_value_tx")
    // .sampler(15, ImageType::FLOAT_2D, "cryptomatte_tx") /* TODO */
    .image(0, GPU_R32F, Qualifier::READ, ImageType::FLOAT_2D_ARRAY, "in_weight_img")
    .image(1, GPU_R32F, Qualifier::WRITE, ImageType::FLOAT_2D_ARRAY, "out_weight_img")
    .image(2, GPU_RGBA16F, Qualifier::READ, ImageType::FLOAT_2D, "in_combined_img")
    .image(3, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D, "out_combined_img")
    .image(4, GPU_R32F, Qualifier::READ_WRITE, ImageType::FLOAT_2D, "depth_img")
    .image(5, GPU_RGBA16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D_ARRAY, "color_accum_img")
    .image(6, GPU_R16F, Qualifier::READ_WRITE, ImageType::FLOAT_2D_ARRAY, "value_accum_img")
    .additional_info("eevee_shared")
    .additional_info("draw_view");

GPU_SHADER_CREATE_INFO(eevee_film_frag)
    .do_static_compilation(true)
    .fragment_out(0, Type::VEC4, "out_color")
    .fragment_source("eevee_film_frag.glsl")
    .additional_info("draw_fullscreen", "eevee_film");

GPU_SHADER_CREATE_INFO(eevee_film_comp)
    .do_static_compilation(true)
    .local_group_size(FILM_GROUP_SIZE, FILM_GROUP_SIZE)
    .compute_source("eevee_film_comp.glsl")
    .additional_info("eevee_film");
