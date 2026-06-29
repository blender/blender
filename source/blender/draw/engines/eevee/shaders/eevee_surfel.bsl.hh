/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "eevee_lightprobe_shared.hh"

struct SurfelData {
  [[storage(SURFEL_BUF_SLOT, read_write)]] Surfel (&surfel_buf)[];
  [[storage(CAPTURE_BUF_SLOT, read)]] const CaptureInfoData &capture_info_buf;

  /**
   * Return true if link from `surfel[a]` to `surfel[b]` is valid.
   * WARNING: this function is not commutative : `f(a, b) != f(b, a)`
   */
  bool is_valid_surfel_link(int a, int b) const
  {
    float3 link_vector = normalize(surfel_buf[b].position - surfel_buf[a].position);
    float link_angle_cos = dot(surfel_buf[a].normal, link_vector);
    bool is_coplanar = abs(link_angle_cos) < 0.05f;
    return !is_coplanar;
  }
};
