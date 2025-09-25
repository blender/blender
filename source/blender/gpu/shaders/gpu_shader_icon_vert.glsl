/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Simple shader that just draw one icon at the specified location
 * does not need any vertex input (producing less call to immBegin/End)
 */

#include "infos/gpu_shader_icon_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_icon)

void main()
{
  float2 uv;
  float2 co;

  if (gl_VertexID == 0) {
    co = rect_geom.xw;
    uv = rect_icon.xw;
    mask_coord_interp = float2(0, 1);
  }
  else if (gl_VertexID == 1) {
    co = rect_geom.xy;
    uv = rect_icon.xy;
    mask_coord_interp = float2(0, 0);
  }
  else if (gl_VertexID == 2) {
    co = rect_geom.zw;
    uv = rect_icon.zw;
    mask_coord_interp = float2(1, 1);
  }
  else {
    co = rect_geom.zy;
    uv = rect_icon.zy;
    mask_coord_interp = float2(1, 0);
  }

  /* Put origin in lower right corner. */
  mask_coord_interp.x -= 1;

  gl_Position = ModelViewProjectionMatrix * float4(co, 0.0f, 1.0f);
  texCoord_interp = uv;
  final_color = finalColor;
}
