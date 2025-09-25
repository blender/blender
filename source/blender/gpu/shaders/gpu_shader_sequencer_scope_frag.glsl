/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_sequencer_infos.hh"

FRAGMENT_SHADER_CREATE_INFO(gpu_shader_sequencer_scope_resolve)

void main()
{
  /* Compute shader based scope point rasterizer produced screen-sized
   * raster buffer, with accumulated R,G,B,A values in fixed point.
   * Fetch data from that buffer and calculate resulting color. */
  int2 view_pos = int2(gl_FragCoord.xy);
  fragColor = float4(0.0f);
  if (any(lessThan(view_pos, int2(0))) ||
      any(greaterThanEqual(view_pos, int2(view_width, view_height))))
  {
    return;
  }

  int view_index = view_pos.y * view_width + view_pos.x;
  SeqScopeRasterData data = raster_buf[view_index];
  if (data.col_a != 0) {
    float4 pix = float4(data.col_r, data.col_g, data.col_b, data.col_a) / 255.0f;
    fragColor.rgb = pix.rgb / pix.a;

    /* Use tonemap-like curve to map amount of points to transparency. */
    fragColor.w = 1.0f - exp(-pix.a * alpha_exponent);
  }
}
