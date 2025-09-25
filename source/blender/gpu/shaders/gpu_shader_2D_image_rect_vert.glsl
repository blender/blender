/* SPDX-FileCopyrightText: 2018-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Simple shader that just draw one icon at the specified location
 * does not need any vertex input (producing less call to immBegin/End)
 */

#include "infos/gpu_shader_2D_image_rect_color_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_image_rect_color)

void main()
{
  float2 uv;
  float2 co;

  if (gl_VertexID == 0) {
    co = rect_geom.xw;
    uv = rect_icon.xw;
  }
  else if (gl_VertexID == 1) {
    co = rect_geom.xy;
    uv = rect_icon.xy;
  }
  else if (gl_VertexID == 2) {
    co = rect_geom.zw;
    uv = rect_icon.zw;
  }
  else {
    co = rect_geom.zy;
    uv = rect_icon.zy;
  }

  gl_Position = ModelViewProjectionMatrix * float4(co, 0.0f, 1.0f);
  texCoord_interp = uv;
}
