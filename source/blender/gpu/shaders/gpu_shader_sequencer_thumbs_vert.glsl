/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_sequencer_info.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_sequencer_thumbs)

void main()
{
  int id = gl_InstanceID;
  thumb_id = id;
  int vid = gl_VertexID;
  SeqStripThumbData thumb = thumb_data[id];
  vec4 coords = vec4(thumb.x1, thumb.y1, thumb.x2, thumb.y2);
  vec4 uvs = vec4(thumb.u1, thumb.v1, thumb.u2, thumb.v2);

  vec2 co;
  vec2 uv;
  if (vid == 0) {
    co = coords.xw;
    uv = uvs.xw;
  }
  else if (vid == 1) {
    co = coords.xy;
    uv = uvs.xy;
  }
  else if (vid == 2) {
    co = coords.zw;
    uv = uvs.zw;
  }
  else {
    co = coords.zy;
    uv = uvs.zy;
  }

  pos_interp = co;
  texCoord_interp = uv;
  gl_Position = ModelViewProjectionMatrix * vec4(co, 0.0f, 1.0f);
}
