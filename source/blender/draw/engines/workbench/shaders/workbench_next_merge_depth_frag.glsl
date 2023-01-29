
void main()
{
  float depth = texture(depth_tx, uvcoordsvar.xy).r;
  if (depth != 1.0) {
    gl_FragDepth = depth;
  }
  else {
    discard;
  }
}
