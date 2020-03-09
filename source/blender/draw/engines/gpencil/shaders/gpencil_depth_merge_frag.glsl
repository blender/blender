
uniform sampler2D depthBuf;
uniform float strokeDepth2d;
uniform bool strokeOrder3d;

noperspective in vec4 uvcoordsvar;

void main()
{
  float depth = textureLod(depthBuf, uvcoordsvar.xy, 0).r;
  if (strokeOrder3d) {
    gl_FragDepth = depth;
  }
  else {
    gl_FragDepth = (depth != 0.0) ? gl_FragCoord.z : 1.0;
  }
}
