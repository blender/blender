/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_legacy_bloom_common)
    .push_constant(Type::VEC2, "sourceBufferTexelSize")
    .push_constant(Type::VEC4, "curveThreshold")
    .push_constant(Type::FLOAT, "clampIntensity")
    .push_constant(Type::VEC2, "baseBufferTexelSize")
    .push_constant(Type::FLOAT, "sampleScale")
    .push_constant(Type::VEC3, "bloomColor")
    .push_constant(Type::BOOL, "bloomAddBase")
    .sampler(0, ImageType::FLOAT_2D, "sourceBuffer")
    .sampler(1, ImageType::FLOAT_2D, "baseBuffer")
    .fragment_out(0, Type::VEC4, "FragColor")
    .additional_info("draw_fullscreen")
    .fragment_source("effect_bloom_frag.glsl");

GPU_SHADER_CREATE_INFO(eevee_legacy_bloom_blit)
    .define("STEP_BLIT")
    .additional_info("eevee_legacy_bloom_common")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_bloom_blit_hq)
    .define("HIGH_QUALITY")
    .additional_info("eevee_legacy_bloom_blit")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_bloom_downsample)
    .define("STEP_DOWNSAMPLE")
    .additional_info("eevee_legacy_bloom_common")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_bloom_downsample_hq)
    .define("HIGH_QUALITY")
    .additional_info("eevee_legacy_bloom_downsample")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_bloom_upsample)
    .define("STEP_UPSAMPLE")
    .additional_info("eevee_legacy_bloom_common")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_bloom_upsample_hq)
    .define("HIGH_QUALITY")
    .additional_info("eevee_legacy_bloom_upsample")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_bloom_resolve)
    .define("STEP_RESOLVE")
    .additional_info("eevee_legacy_bloom_common")
    .do_static_compilation(true);

GPU_SHADER_CREATE_INFO(eevee_legacy_bloom_resolve_hq)
    .define("HIGH_QUALITY")
    .additional_info("eevee_legacy_bloom_resolve")
    .do_static_compilation(true);
