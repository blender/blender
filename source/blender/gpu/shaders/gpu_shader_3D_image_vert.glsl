
void main()
{
  gl_Position = ModelViewProjectionMatrix * vec4(pos.xyz, 1.0f);
  texCoord_interp = texCoord;
}
