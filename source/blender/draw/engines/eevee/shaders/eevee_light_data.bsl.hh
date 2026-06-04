/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "gpu_shader_compat.hh"

#include "eevee_light_shared.hh"

namespace eevee {

struct LightRenderData {
  [[storage(LIGHT_CULL_BUF_SLOT, read)]] const LightCullingData &light_cull_buf;
  [[storage(LIGHT_BUF_SLOT, read)]] const LightData (&light_buf)[];
  [[storage(LIGHT_ZBIN_BUF_SLOT, read)]] const uint (&light_zbin_buf)[];
  [[storage(LIGHT_TILE_BUF_SLOT, read)]] const uint (&light_tile_buf)[];
};

}  // namespace eevee
