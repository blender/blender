
/* Display a linear image texture into sRGB space */

uniform sampler2D image;

in vec2 texCoord_interp;

out vec4 fragColor;

float linearrgb_to_srgb(float c)
{
  if (c < 0.0031308) {
    return (c < 0.0) ? 0.0 : c * 12.92;
  }
  else {
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
  }
}

void linearrgb_to_srgb(vec4 col_from, out vec4 col_to)
{
  col_to.r = linearrgb_to_srgb(col_from.r);
  col_to.g = linearrgb_to_srgb(col_from.g);
  col_to.b = linearrgb_to_srgb(col_from.b);
  col_to.a = col_from.a;
}

void main()
{
  fragColor = texture(image, texCoord_interp.st);

  linearrgb_to_srgb(fragColor, fragColor);
}
