/* SPDX-FileCopyrightText: 2022-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Shader that down-sample depth buffer, creating a Hierarchical-Z buffer.
 * Saves max value of each 2x2 texel in the mipmap above the one we are
 * rendering to. Adapted from
 * http://rastergrid.com/blog/2010/10/hierarchical-z-map-based-occlusion-culling/
 *
 * Major simplification has been made since we pad the buffer to always be
 * bigger than input to avoid mipmapping misalignment.
 *
 * Start by copying the base level by quad loading the depth.
 * Then each thread compute it's local depth for level 1.
 * After that we use shared variables to do inter thread communication and
 * downsample to max level.
 */

#pragma once

#include "gpu_shader_compat.hh"

namespace eevee {

/* CurrentLayerHiZ. */
struct HiZ {
  [[sampler(HIZ_TEX_SLOT)]] sampler2D hiz_tx;
};

struct PreviousLayerHiZ {
  [[sampler(HIZ_PREVIOUS_LAYER_TEX_SLOT)]] sampler2D hiz_prev_tx;
};

struct PreviousLayerRadiance {
  [[sampler(RADIANCE_PREVIOUS_LAYER_TEX_SLOT)]] sampler2D previous_layer_radiance_tx;
};

}  // namespace eevee
