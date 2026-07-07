/* SPDX-FileCopyrightText: 2016-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpu_shader_text_infos.hh"

VERTEX_SHADER_CREATE_INFO(gpu_shader_text)

void main()
{
  int glyph_index = gl_InstanceID;

  color_flat = glyphs[glyph_index].glyph_color;
  glyph_offset = glyphs[glyph_index].offset;
  glyph_dim = glyphs[glyph_index].glyph_size;
  glyph_flags = glyphs[glyph_index].flags;

  /* Depending on shadow outline / blur level, we might need to expand the quad. */
  uint shadow_type = glyph_flags & 0xFu;
  int interp_size = shadow_type > 4 ? 2 : (shadow_type > 0 ? 1 : 0);

  /* Quad expansion using instanced rendering. */
  float x = float(gl_VertexID % 2);
  float y = float(gl_VertexID / 2);
  float2 quad = float2(x, y);

  float4 pos = float4(glyphs[glyph_index].position);
  float2 interp_offset = float(interp_size) / abs(pos.zw - pos.xy);
  texCoord_interp = mix(-interp_offset, 1.0f + interp_offset, quad) * float2(glyph_dim) +
                    float2(0.5f);

  float2 final_pos = mix(float2(int2(pos.xy) + int2(-interp_size, interp_size)),
                         float2(int2(pos.zw) + int2(interp_size, -interp_size)),
                         quad);

  gl_Position = ModelViewProjectionMatrix * float4(final_pos, 0.0f, 1.0f);
}
