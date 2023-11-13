/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_clipping_lib.glsl)
#pragma BLENDER_REQUIRE(common_view_lib.glsl)

void main()
{
  GPU_INTEL_VERTEX_SHADER_WORKAROUND

  vec2 pos;
  pos.x = float(gl_VertexID % 2);
  pos.y = float(gl_VertexID / 2) / float(halfLineCount - 1);

  if (pos.y > 1.0) {
    pos.xy = pos.yx;
    pos.x -= 1.0 + 1.0 / float(halfLineCount - 1);
  }

  pos -= 0.5;

  vec3 world_pos = xAxis * pos.x + yAxis * pos.y + origin;

  gl_Position = point_world_to_ndc(world_pos);

  view_clipping_distances(world_pos);

  finalColor = color;

  /* Convert to screen position [0..sizeVp]. */
  edgePos = edgeStart = ((gl_Position.xy / gl_Position.w) * 0.5 + 0.5) * sizeViewport.xy;
}
