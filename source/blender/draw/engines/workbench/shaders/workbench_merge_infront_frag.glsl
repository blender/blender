
uniform sampler2D depthBuffer;

in vec4 uvcoordsvar;

out vec4 fragColor;

void main()
{
  float depth = texture(depthBuffer, uvcoordsvar.st).r;
  /* Discard background pixels. */
  if (depth == 1.0) {
    discard;
  }
  /* Make this fragment occlude any fragment that will try to
   * render over it in the normal passes. */
  gl_FragDepth = 0.0;
}