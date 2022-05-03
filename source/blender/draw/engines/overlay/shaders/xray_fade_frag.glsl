
void main()
{
  float depth = texture(depthTex, uvcoordsvar.xy).r;
  float depth_xray = texture(xrayDepthTex, uvcoordsvar.xy).r;
  fragColor = vec4((depth < 1.0 && depth > depth_xray) ? opacity : 1.0);
}
