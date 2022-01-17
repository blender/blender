#ifndef USE_GPU_SHADER_CREATE_INFO
uniform mat4 ModelViewProjectionMatrix;

uniform vec4 rect;
uniform int cornerLen;
uniform float scale;

in vec2 pos;

out vec2 uv;
#endif

void main()
{
  int corner_id = (gl_VertexID / cornerLen) % 4;

  vec2 final_pos = pos * scale;

  if (corner_id == 0) {
    uv = pos + vec2(1.0, 1.0);
    final_pos += rect.yw; /* top right */
  }
  else if (corner_id == 1) {
    uv = pos + vec2(-1.0, 1.0);
    final_pos += rect.xw; /* top left */
  }
  else if (corner_id == 2) {
    uv = pos + vec2(-1.0, -1.0);
    final_pos += rect.xz; /* bottom left */
  }
  else {
    uv = pos + vec2(1.0, -1.0);
    final_pos += rect.yz; /* bottom right */
  }

  gl_Position = (ModelViewProjectionMatrix * vec4(final_pos, 0.0, 1.0));
}
