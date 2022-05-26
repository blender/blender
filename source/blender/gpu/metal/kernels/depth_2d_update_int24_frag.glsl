
uniform isampler2D source_data;
uniform int mip;

in vec2 texCoord_interp;

void main()
{
  uint val = textureLod(source_data, texCoord_interp, mip).r;
  uint stencil = (val >> 24) & 0xFFu;
  uint depth = (val)&0xFFFFFFu;
  gl_FragDepth = float(depth) / float(0xFFFFFFu);
}
