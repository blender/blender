/* SPDX-FileCopyrightText: 2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  float4 rect = float4(offset.x, offset.y, offset.x + extent.x, offset.y + extent.y);
  rect /= float4(size, size);
  float4 tex = rect;
  rect = rect * 2.0f - 1.0f;

  /* QUAD */
  if (pos.x == 0.0f && pos.y == 0.0f) {
    rect.xy = rect.xy;
    texCoord_interp = tex.xy;
  }
  else if (pos.x == 0.0f && pos.y == 1.0f) {
    rect.xy = rect.xw;
    texCoord_interp = tex.xw;
  }
  else if (pos.x == 1.0f && pos.y == 1.0f) {
    rect.xy = rect.zw;
    texCoord_interp = tex.zw;
  }
  else {
    rect.xy = rect.zy;
    texCoord_interp = tex.zy;
  }
  gl_Position = float4(rect.xy, 0.0f, 1.0f);
}
