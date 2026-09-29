/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include "draw_curves.bsl.hh"

namespace curves::Test {

struct Test {
  [[storage(0, write)]] float (&result_pos_buf)[];
  [[storage(1, write)]] int4 (&result_indices_buf)[];
};

[[vertex]]
void vert([[resource_table]] Test &srt,
          [[resource_table]] draw::Curves &curves,
          [[vertex_id]] const int vert_id,
          [[position]] float4 &out_pos)
{
  draw::curves::Point pt = curves.point_get(uint(vert_id));

  srt.result_pos_buf[vert_id] = pt.P.x;
  srt.result_indices_buf[vert_id].x = pt.point_id;
  srt.result_indices_buf[vert_id].y = pt.curve_id;
  srt.result_indices_buf[vert_id].z = pt.curve_segment;
  srt.result_indices_buf[vert_id].w = int(pt.azimuthal_offset);

  out_pos = float4(0);
}

[[fragment]]
void frag()
{
}

}  // namespace curves::Test

PipelineGraphic draw_curves_test(curves::Test::vert, curves::Test::frag);
