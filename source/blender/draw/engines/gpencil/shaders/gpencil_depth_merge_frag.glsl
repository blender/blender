
uniform sampler2D depthBuf;
uniform bool strokeOrder3d;

void main()
{
  float depth = textureLod(depthBuf, gl_FragCoord.xy / vec2(textureSize(depthBuf, 0)), 0).r;
  if (strokeOrder3d) {
    gl_FragDepth = depth;
  }
  else {
    gl_FragDepth = (depth != 0.0) ? gl_FragCoord.z : 1.0;
  }
}
