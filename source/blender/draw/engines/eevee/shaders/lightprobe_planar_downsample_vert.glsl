
out int instance;
out vec2 vPos;

void main()
{
  int v = gl_VertexID % 3;
  vPos.x = -1.0 + float((v & 1) << 2);
  vPos.y = -1.0 + float((v & 2) << 1);

  instance = gl_VertexID / 3;
}
