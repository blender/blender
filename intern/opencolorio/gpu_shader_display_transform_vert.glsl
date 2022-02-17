
void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos.xy, 0.0f, 1.0f);
  texCoord_interp = texCoord;
}
