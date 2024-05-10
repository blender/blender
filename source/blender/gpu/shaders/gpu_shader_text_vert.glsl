/* SPDX-FileCopyrightText: 2016-2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

void main()
{
  color_flat = col;
  glyph_offset = offset;
  glyph_dim = glyph_size;
  glyph_flags = flags;

  /* Depending on shadow outline / blur level, we might need to expand the quad. */
  uint shadow_type = flags & 0xF;
  int interp_size = shadow_type > 4 ? 2 : (shadow_type > 0 ? 1 : 0);

  /* Quad expansion using instanced rendering. */
  float x = float(gl_VertexID % 2);
  float y = float(gl_VertexID / 2);
  vec2 quad = vec2(x, y);

  vec2 interp_offset = float(interp_size) / abs(pos.zw - pos.xy);
  texCoord_interp = mix(-interp_offset, 1.0 + interp_offset, quad) * vec2(glyph_dim) + vec2(0.5);

  vec2 final_pos = mix(vec2(ivec2(pos.xy) + ivec2(-interp_size, interp_size)),
                       vec2(ivec2(pos.zw) + ivec2(interp_size, -interp_size)),
                       quad);

  gl_Position = ModelViewProjectionMatrix * vec4(final_pos, 0.0, 1.0);
}
