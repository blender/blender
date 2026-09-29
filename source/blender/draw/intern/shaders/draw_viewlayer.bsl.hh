/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "gpu_shader_compat.hh"

#include "draw_shader_shared.hh"

namespace draw {

struct ViewLayerAttributes {
  [[uniform(DRW_LAYER_ATTR_UBO_SLOT)]] const LayerAttribute (
      &drw_layer_attrs)[DRW_RESOURCE_CHUNK_LEN];

  float4 get(const uint attr_hash) const
  {
    /* The first record of the buffer stores the length. */
    uint left = 0, right = drw_layer_attrs[0].buffer_length;

    while (left < right) {
      uint mid = (left + right) / 2;
      uint hash = drw_layer_attrs[mid].hash_code;

      if (hash < attr_hash) {
        left = mid + 1;
      }
      else if (hash > attr_hash) {
        right = mid;
      }
      else {
        return drw_layer_attrs[mid].data;
      }
    }
    return float4(0);
  }
};

}  // namespace draw
