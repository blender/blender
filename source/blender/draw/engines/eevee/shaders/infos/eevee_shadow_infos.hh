/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#ifdef GPU_SHADER
#  pragma once
#  include "gpu_shader_compat.hh"

#  include "eevee_shadow_shared.hh"
#endif

#include "eevee_defines.hh"
#include "gpu_shader_create_info.hh"

GPU_SHADER_CREATE_INFO(eevee_shadow_data)
TYPEDEF_SOURCE("eevee_shadow_shared.hh")
/* SHADOW_READ_ATOMIC macro indicating shadow functions should use `usampler2DArrayAtomic` as
 * the atlas type. */
DEFINE("SHADOW_READ_ATOMIC")
BUILTINS(BuiltinBits::TEXTURE_ATOMIC)
SAMPLER(SHADOW_ATLAS_TEX_SLOT, usampler2DArrayAtomic, shadow_atlas_tx)
SAMPLER(SHADOW_TILEMAPS_TEX_SLOT, usampler2D, shadow_tilemaps_tx)
GPU_SHADER_CREATE_END()

GPU_SHADER_CREATE_INFO(eevee_shadow_data_non_atomic)
TYPEDEF_SOURCE("eevee_shadow_shared.hh")
SAMPLER(SHADOW_ATLAS_TEX_SLOT, usampler2DArray, shadow_atlas_tx)
SAMPLER(SHADOW_TILEMAPS_TEX_SLOT, usampler2D, shadow_tilemaps_tx)
GPU_SHADER_CREATE_END()
