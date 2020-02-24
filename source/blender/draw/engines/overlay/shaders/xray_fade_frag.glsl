
uniform sampler2D depthTex;
uniform sampler2D xrayDepthTex;
uniform float opacity;

in vec4 uvcoordsvar;

out vec4 fragColor;

void main()
{
  float depth = texture(depthTex, uvcoordsvar.xy).r;
  float depth_xray = texture(xrayDepthTex, uvcoordsvar.xy).r;
  fragColor = vec4((depth < 1.0 && depth > depth_xray) ? opacity : 1.0);
}
