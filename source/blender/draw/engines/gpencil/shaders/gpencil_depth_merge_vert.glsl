/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpencil_depth_merge)

#include "draw_view_lib.glsl"

void main()
{
  int v = gl_VertexID % 3;
  float x = -1.0f + float((v & 1) << 2);
  float y = -1.0f + float((v & 2) << 1);
  gl_Position = drw_view().winmat *
                (drw_view().viewmat * (gp_model_matrix * float4(x, y, 0.0f, 1.0f)));
}
