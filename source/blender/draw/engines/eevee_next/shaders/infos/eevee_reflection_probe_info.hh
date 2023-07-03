/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Shared
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_reflection_probe_data)
    .sampler(REFLECTION_PROBE_TEX_SLOT, ImageType::FLOAT_2D_ARRAY, "reflectionProbes");

/* Sample cubemap and remap into an octahedral texture. */
GPU_SHADER_CREATE_INFO(eevee_reflection_probe_remap)
    .local_group_size(REFLECTION_PROBE_GROUP_SIZE, REFLECTION_PROBE_GROUP_SIZE)
    .sampler(0, ImageType::FLOAT_CUBE, "cubemap_tx")
    .image(0, GPU_RGBA16F, Qualifier::WRITE, ImageType::FLOAT_2D_ARRAY, "octahedral_img")
    .compute_source("eevee_reflection_probe_remap_comp.glsl")
    .do_static_compilation(true);

/** \} */
