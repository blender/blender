
void main()
{
  gl_FragDepth = textureLod(source_data, texCoord_interp, mip).r;
}
