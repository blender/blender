
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

float linearrgb_to_srgb(float c)
{
  if (c < 0.0031308) {
    return (c < 0.0) ? 0.0 : c * 12.92;
  }
  else {
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
  }
}

vec4 texture_read_as_srgb(sampler2D tex, bool premultiplied, vec2 co)
{
  /* By convention image textures return scene linear colors, but
   * overlays still assume srgb. */
  vec4 color = texture(tex, co);
  /* Unpremultiply if stored multiplied, since straight alpha is expected by shaders. */
  if (premultiplied && !(color.a == 0.0 || color.a == 1.0)) {
    color.rgb = color.rgb / color.a;
  }
  color.r = linearrgb_to_srgb(color.r);
  color.g = linearrgb_to_srgb(color.g);
  color.b = linearrgb_to_srgb(color.b);
  return color;
}

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
