/* SPDX-FileCopyrightText: 2022 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup gpu
 */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(gpu_shader_2D_image_overlays_stereo_merge)
    .vertex_in(0, Type::VEC2, "pos")
    .fragment_out(0, Type::VEC4, "overlayColor")
    .fragment_out(1, Type::VEC4, "imageColor")
    .sampler(0, ImageType::FLOAT_2D, "imageTexture")
    .sampler(1, ImageType::FLOAT_2D, "overlayTexture")
    .push_constant(Type::MAT4, "ModelViewProjectionMatrix")
    .push_constant(Type::INT, "stereoDisplaySettings")
    .vertex_source("gpu_shader_2D_vert.glsl")
    .fragment_source("gpu_shader_image_overlays_stereo_merge_frag.glsl")
    .do_static_compilation(true);
