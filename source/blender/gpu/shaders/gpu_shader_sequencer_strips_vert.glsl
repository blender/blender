/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_sequencer_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_sequencer_strips)

void main()
{
  int id = gl_InstanceID;
  strip_id = id;
  int vid = gl_VertexID;
  SeqStripDrawData strip = strip_data[id];
  float4 rect = float4(strip.left_handle, strip.bottom, strip.right_handle, strip.top);
  /* Expand by 1px to fit pixel grid rounding. */
  float2 expand = float2(1.0f, 1.0f);
  rect.xy -= expand;
  rect.zw += expand;

  float2 co;
  if (vid == 0) {
    co = rect.xw;
  }
  else if (vid == 1) {
    co = rect.xy;
  }
  else if (vid == 2) {
    co = rect.zw;
  }
  else {
    co = rect.zy;
  }

  co_interp = co;
  gl_Position = ModelViewProjectionMatrix * float4(co, 0.0f, 1.0f);
}
