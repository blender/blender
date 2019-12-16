
out vec2 uvs;

void main()
{
  int v = gl_VertexID % 3;
  float x = float((v & 1) << 2);
  float y = float((v & 2) << 1);
  gl_Position = vec4(x - 1.0, y - 1.0, 1.0, 1.0);
  uvs = vec2(x, y) * 0.5;
}
