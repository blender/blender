
uniform vec4 gpModelMatrix[4];

void main()
{
  mat4 model_matrix = mat4(gpModelMatrix[0], gpModelMatrix[1], gpModelMatrix[2], gpModelMatrix[3]);
  int v = gl_VertexID % 3;
  float x = -1.0 + float((v & 1) << 2);
  float y = -1.0 + float((v & 2) << 1);
  gl_Position = ViewProjectionMatrix * (model_matrix * vec4(x, y, 0.0, 1.0));
}
