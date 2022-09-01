
void main()
{
  uint val = textureLod(source_data, texCoord_interp, mip).r;
  uint depth = (val) & (0xFFFFFFFFu);
  gl_FragDepth = float(depth) / float(0xFFFFFFFFu);
}
