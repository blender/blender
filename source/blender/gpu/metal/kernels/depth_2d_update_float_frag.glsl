
uniform sampler2D source_data;
uniform int mip;

in vec2 texCoord_interp;

void main()
{
  gl_FragDepth = textureLod(source_data, texCoord_interp, mip).r;
}
