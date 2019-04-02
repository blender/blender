float linearrgb_to_srgb(float c)
{
  if (c < 0.0031308) {
    return (c < 0.0) ? 0.0 : c * 12.92;
  }
  else {
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
  }
}

vec4 texture_read_as_linearrgb(sampler2D tex, bool premultiplied, vec2 co)
{
  /* By convention image textures return scene linear colors, but
   * overlays still assume srgb. */
  vec4 color = texture(tex, co);
  /* Unpremultiply if stored multiplied, since straight alpha is expected by shaders. */
  if (premultiplied && !(color.a == 0.0 || color.a == 1.0)) {
    color.rgb = color.rgb / color.a;
  }
  return color;
}

vec4 texture_read_as_srgb(sampler2D tex, bool premultiplied, vec2 co)
{
  vec4 color = texture_read_as_linearrgb(tex, premultiplied, co);
  color.r = linearrgb_to_srgb(color.r);
  color.g = linearrgb_to_srgb(color.g);
  color.b = linearrgb_to_srgb(color.b);
  return color;
}
