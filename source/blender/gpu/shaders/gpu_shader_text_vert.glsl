#ifndef USE_GPU_SHADER_CREATE_INFO
uniform mat4 ModelViewProjectionMatrix;

in vec4 pos; /* rect */
in vec4 col;
in int offset;
in ivec2 glyph_size;

flat out vec4 color_flat;
noperspective out vec2 texCoord_interp;
flat out int glyph_offset;
flat out ivec2 glyph_dim;
flat out int interp_size;
#endif

void main()
{
  color_flat = col;
  glyph_offset = offset;
  glyph_dim = abs(glyph_size);
  interp_size = int(glyph_size.x < 0) + int(glyph_size.y < 0);

  /* Quad expension using instanced rendering. */
  float x = float(gl_VertexID % 2);
  float y = float(gl_VertexID / 2);
  vec2 quad = vec2(x, y);

  vec2 interp_offset = float(interp_size) / abs(pos.zw - pos.xy);
  texCoord_interp = mix(-interp_offset, 1.0 + interp_offset, quad);

  vec2 final_pos = mix(
      pos.xy + ivec2(-interp_size, interp_size), pos.zw + ivec2(interp_size, -interp_size), quad);

  gl_Position = ModelViewProjectionMatrix * vec4(final_pos, 0.0, 1.0);
}
