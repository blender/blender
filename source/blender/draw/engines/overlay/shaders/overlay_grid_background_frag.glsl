
void main()
{
  fragColor = color;
  float scene_depth = texelFetch(depthBuffer, ivec2(gl_FragCoord.xy), 0).r;
  fragColor.a = (scene_depth == 1.0) ? 1.0 : 0.0;
}
