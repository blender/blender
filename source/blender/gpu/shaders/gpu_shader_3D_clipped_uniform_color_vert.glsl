
void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos, 1.0);
  gl_ClipDistance[0] = dot(ModelMatrix * vec4(pos, 1.0), ClipPlane);
}
