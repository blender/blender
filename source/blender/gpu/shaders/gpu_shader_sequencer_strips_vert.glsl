/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  int id = gl_InstanceID;
  strip_id = id;
  int vid = gl_VertexID;
  SeqStripDrawData strip = strip_data[id];
  vec4 rect = vec4(strip.left_handle, strip.bottom, strip.right_handle, strip.top);
  /* Expand by 2px to fit possible pixel grid rounding. */
  vec2 expand = vec2(context_data.pixelx, context_data.pixely);
  rect.xy -= expand;
  rect.zw += expand;

  vec2 co;
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
  gl_Position = ModelViewProjectionMatrix * vec4(co, 0.0f, 1.0f);
}
