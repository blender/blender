
void main()
{
  normal = normalize(NormalMatrix * nor);
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
}
