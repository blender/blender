
void main()
{
  float depth = texture(depthBuffer, uvcoordsvar.xy).r;
  /* Fix issues with Intel drivers (see #80023). */
  fragColor = vec4(0.0);
  /* Discard background pixels. */
  if (depth == 1.0) {
    discard;
  }
  /* Make this fragment occlude any fragment that will try to
   * render over it in the normal passes. */
  gl_FragDepth = 0.0;
}
