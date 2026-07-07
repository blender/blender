/* SPDX-FileCopyrightText: 2018-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_2D_node_socket_infos.hh"

#include "gpu_shader_math_base_lib.glsl"

VERTEX_SHADER_CREATE_INFO(gpu_shader_2D_node_socket_inst)

#define rect parameters[widgetID * MAX_SOCKET_PARAMETERS + 0]
#define colorInner parameters[widgetID * MAX_SOCKET_PARAMETERS + 1]
#define colorOutline parameters[widgetID * MAX_SOCKET_PARAMETERS + 2]
#define outlineThickness parameters[widgetID * MAX_SOCKET_PARAMETERS + 3].x
#define outlineOffset parameters[widgetID * MAX_SOCKET_PARAMETERS + 3].y
#define shape parameters[widgetID * MAX_SOCKET_PARAMETERS + 3].z
#define aspect parameters[widgetID * MAX_SOCKET_PARAMETERS + 3].w

void main()
{
  /* Scale the original rectangle to accommodate the diagonal of the diamond shape. */
  float2 originalRectSize = rect.yw - rect.xz;
  float offset = 0.125f * min(originalRectSize.x, originalRectSize.y) +
                 outlineOffset * outlineThickness;
  float2 ofs = float2(offset, -offset);
  float2 pos;
  switch (gl_VertexID) {
    default:
    case 0: {
      pos = rect.xz + ofs.yy;
      break;
    }
    case 1: {
      pos = rect.xw + ofs.yx;
      break;
    }
    case 2: {
      pos = rect.yz + ofs.xy;
      break;
    }
    case 3: {
      pos = rect.yw + ofs.xx;
      break;
    }
  }

  gl_Position = ModelViewProjectionMatrix * float4(pos, 0.0f, 1.0f);

  float2 rectSize = rect.yw - rect.xz + 2.0f * float2(outlineOffset, outlineOffset);
  float minSize = min(rectSize.x, rectSize.y);

  float2 centeredCoordinates = pos - ((rect.xz + rect.yw) / 2.0f);
  uv = centeredCoordinates / minSize;

  /* Calculate the necessary "extrusion" of the coordinates to draw the middle part of
   * multi sockets. */
  float ratio = rectSize.x / rectSize.y;
  extrusion = (ratio > 1.0f) ? float2((ratio - 1.0f) / 2.0f, 0.0f) :
                               float2(0.0f, ((1.0f / ratio) - 1.0f) / 2.0f);

  /* Shape parameters. */
  finalShape = int(shape);
  finalOutlineThickness = outlineThickness / minSize;
  finalDotRadius = outlineThickness / minSize;
  AAsize = 1.0f * aspect / minSize;

  /* Pass through parameters. */
  finalColor = colorInner;
  finalOutlineColor = colorOutline;
}
