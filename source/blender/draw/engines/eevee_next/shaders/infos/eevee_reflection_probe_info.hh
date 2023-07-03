/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

/* -------------------------------------------------------------------- */
/** \name Shared
 * \{ */

GPU_SHADER_CREATE_INFO(eevee_reflection_probe_data)
    .sampler(REFLECTION_PROBE_TEX_SLOT, ImageType::FLOAT_CUBE_ARRAY, "reflectionProbes");

/** \} */
