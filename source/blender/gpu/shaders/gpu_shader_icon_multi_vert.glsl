/* SPDX-FileCopyrightText: 2018-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Simple shader that just draw multiple icons at the specified locations
 * does not need any vertex input (producing less call to immBegin/End)
 */

#include "infos/gpu_shader_icon_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_icon_multi)

void main()
{
  float4 rect = multi_icon_data.calls_data[gl_InstanceID * 3];
  float4 tex = multi_icon_data.calls_data[gl_InstanceID * 3 + 1];
  final_color = multi_icon_data.calls_data[gl_InstanceID * 3 + 2];

  /* Use pos to select the right swizzle (instead of gl_VertexID)
   * in order to workaround an OSX driver bug. */
  if (all(equal(pos, float2(0.0f, 0.0f)))) {
    rect.xy = rect.xz;
    tex.xy = tex.xz;
  }
  else if (all(equal(pos, float2(0.0f, 1.0f)))) {
    rect.xy = rect.xw;
    tex.xy = tex.xw;
  }
  else if (all(equal(pos, float2(1.0f, 1.0f)))) {
    rect.xy = rect.yw;
    tex.xy = tex.yw;
  }
  else {
    rect.xy = rect.yz;
    tex.xy = tex.yz;
  }

  gl_Position = float4(rect.xy, 0.0f, 1.0f);
  texCoord_interp = tex.xy;
}
