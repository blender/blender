
uniform mat4 ModelViewProjectionMatrix;

in vec4 pos; /* rect */
in vec4 tex; /* rect */
in vec4 col;

flat out vec4 color_flat;
noperspective out vec2 texCoord_interp;

void main()
{
  /* Quad expension using instanced rendering. */
  float x = float(gl_VertexID % 2);
  float y = float(gl_VertexID / 2);
  vec2 quad = vec2(x, y);

  gl_Position = ModelViewProjectionMatrix * vec4(mix(pos.xy, pos.zw, quad), 0.0, 1.0);
  texCoord_interp = mix(abs(tex.xy), abs(tex.zw), quad) * sign(tex.xw);
  color_flat = col;
}
