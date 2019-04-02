in vec2 texCoord_interp;
out vec4 fragColor;

uniform sampler2D image;
uniform float alpha;
uniform bool imagePremultiplied;

void main()
{
#ifdef DRW_STATE_DO_COLOR_MANAGEMENT
  /* render engine has already applied the view transform. We sample the
   * camera images as srgb*/
  vec4 color = texture_read_as_srgb(image, imagePremultiplied, texCoord_interp);

#else
  /* Render engine renders in linearrgb. We sample the camera images as
   * linearrgb */
  vec4 color = texture_read_as_linearrgb(image, imagePremultiplied, texCoord_interp);
#endif

  color.a *= alpha;
  fragColor = color;
}
