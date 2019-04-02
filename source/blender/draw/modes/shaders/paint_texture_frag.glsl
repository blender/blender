
in vec2 uv_interp;
#ifdef TEXTURE_PAINT_MASK
in vec2 masking_uv_interp;
#endif

out vec4 fragColor;

uniform sampler2D image;
uniform bool imagePremultiplied;
uniform float alpha = 1.0;
uniform bool nearestInterp;

#ifdef TEXTURE_PAINT_MASK
uniform sampler2D maskingImage;
uniform bool maskingImagePremultiplied;
uniform vec3 maskingColor;
uniform bool maskingInvertStencil;
#endif

void main()
{
  vec2 uv = uv_interp;
  if (nearestInterp) {
    vec2 tex_size = vec2(textureSize(image, 0).xy);
    uv = (floor(uv_interp * tex_size) + 0.5) / tex_size;
  }

  vec4 color = texture_read_as_srgb(image, imagePremultiplied, uv);
  color.a *= alpha;

#ifdef TEXTURE_PAINT_MASK
  vec4 mask = vec4(
      texture_read_as_srgb(maskingImage, maskingImagePremultiplied, masking_uv_interp).rgb, 1.0);
  if (maskingInvertStencil) {
    mask.rgb = 1.0 - mask.rgb;
  }
  float mask_step = smoothstep(0, 3.0, mask.r + mask.g + mask.b);
  mask.rgb *= maskingColor;
  color = mix(color, mask, mask_step);
#endif

  fragColor = color;
}
