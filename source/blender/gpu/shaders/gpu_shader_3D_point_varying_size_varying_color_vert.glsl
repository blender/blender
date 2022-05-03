
void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
  gl_PointSize = size;
  finalColor = color;
}
