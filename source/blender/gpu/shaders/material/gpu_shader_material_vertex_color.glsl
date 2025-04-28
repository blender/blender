/* SPDX-FileCopyrightText: 2019 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void node_vertex_color(float4 vertexColor, out float4 outColor, out float outAlpha)
{
  outColor = vertexColor;
  outAlpha = vertexColor.a;
}
