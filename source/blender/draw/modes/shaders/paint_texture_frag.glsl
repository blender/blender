in vec2 masking_uv_interp;

out vec4 fragColor;

uniform float alpha = 1.0;

uniform sampler2D maskingImage;
uniform bool maskingImagePremultiplied;
uniform vec3 maskingColor;
uniform bool maskingInvertStencil;

void main()
{

  vec4 mask = vec4(
      texture_read_as_srgb(maskingImage, maskingImagePremultiplied, masking_uv_interp).rgb, 1.0);
  if (maskingInvertStencil) {
    mask.rgb = 1.0 - mask.rgb;
  }
  float mask_step = smoothstep(0, 3.0, mask.r + mask.g + mask.b);
  mask.rgb *= maskingColor;
  mask.a = mask_step * alpha;

  fragColor = mask;
}
