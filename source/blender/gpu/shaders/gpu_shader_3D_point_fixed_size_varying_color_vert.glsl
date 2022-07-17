
void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
  finalColor = color;
}
