/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_subsurface_setup)
    .do_static_compilation(true)
    .local_group_size(SUBSURFACE_GROUP_SIZE, SUBSURFACE_GROUP_SIZE)
    .typedef_source("draw_shader_shared.hh")
    .additional_info("draw_view", "eevee_shared", "eevee_gbuffer_data")
    .sampler(2, ImageType::DEPTH_2D, "depth_tx")
    .image(0, DEFERRED_RADIANCE_FORMAT, Qualifier::READ, ImageType::UINT_2D, "direct_light_img")
    .image(1, RAYTRACE_RADIANCE_FORMAT, Qualifier::READ, ImageType::FLOAT_2D, "indirect_light_img")
    .image(2, SUBSURFACE_OBJECT_ID_FORMAT, Qualifier::WRITE, ImageType::UINT_2D, "object_id_img")
    .image(3, SUBSURFACE_RADIANCE_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "radiance_img")
    .storage_buf(0, Qualifier::WRITE, "uint", "convolve_tile_buf[]")
    .storage_buf(1, Qualifier::READ_WRITE, "DispatchCommand", "convolve_dispatch_buf")
    .compute_source("eevee_subsurface_setup_comp.glsl");

GPU_SHADER_CREATE_INFO(eevee_subsurface_convolve)
    .do_static_compilation(true)
    .local_group_size(SUBSURFACE_GROUP_SIZE, SUBSURFACE_GROUP_SIZE)
    .additional_info("draw_view", "eevee_shared", "eevee_gbuffer_data", "eevee_global_ubo")
    .sampler(2, ImageType::FLOAT_2D, "radiance_tx")
    .sampler(3, ImageType::DEPTH_2D, "depth_tx")
    .sampler(4, ImageType::UINT_2D, "object_id_tx")
    .storage_buf(0, Qualifier::READ, "uint", "tiles_coord_buf[]")
    .image(0, DEFERRED_RADIANCE_FORMAT, Qualifier::WRITE, ImageType::UINT_2D, "out_direct_img")
    .image(1, RAYTRACE_RADIANCE_FORMAT, Qualifier::WRITE, ImageType::FLOAT_2D, "out_indirect_img")
    .compute_source("eevee_subsurface_convolve_comp.glsl");
