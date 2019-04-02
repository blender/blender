
flat in vec4 finalColor;

#ifndef USE_WIRE
in vec2 texCoord_interp;
#endif

out vec4 fragColor;

#ifndef USE_WIRE
uniform sampler2D image;
uniform bool imagePremultiplied;
#endif

uniform int depthMode;
uniform bool useAlphaTest;

void main()
{
#ifdef USE_WIRE
  fragColor = finalColor;
#else
  vec4 tex_col = texture_read_as_srgb(image, imagePremultiplied, texCoord_interp);
  fragColor = finalColor * tex_col;

  if (useAlphaTest) {
    /* Arbitrary discard anything below 5% opacity.
     * Note that this could be exposed to the User. */
    if (tex_col.a < 0.05) {
      discard;
    }
    else {
      fragColor.a = 1.0;
    }
  }
#endif

  if (depthMode == DEPTH_BACK) {
    gl_FragDepth = 0.999999;
  }
  else if (depthMode == DEPTH_FRONT) {
    gl_FragDepth = 0.000001;
  }
  else if (depthMode == DEPTH_UNCHANGED) {
    gl_FragDepth = gl_FragCoord.z;
  }
}
