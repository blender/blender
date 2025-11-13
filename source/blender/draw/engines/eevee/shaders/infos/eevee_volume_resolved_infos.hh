/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "draw_view_infos.hh"
#  include "eevee_uniform_infos.hh"

#  define SPHERE_PROBE
#endif

#include "gpu_shader_create_info.hh"

#pragma once

/* Used for shaders that need the final accumulated volume transmittance and scattering. */
GPU_SHADER_CREATE_INFO(eevee_volume_lib)
TYPEDEF_SOURCE("eevee_defines.hh")
ADDITIONAL_INFO(eevee_global_ubo)
ADDITIONAL_INFO(draw_view)
SAMPLER(VOLUME_SCATTERING_TEX_SLOT, sampler3D, volume_scattering_tx)
SAMPLER(VOLUME_TRANSMITTANCE_TEX_SLOT, sampler3D, volume_transmittance_tx)
GPU_SHADER_CREATE_END()
