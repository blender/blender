in vec2 uv_interp;

out vec4 fragColor;

uniform float opacity = 1.0;

uniform sampler2D maskImage;
uniform bool maskImagePremultiplied;
uniform vec3 maskColor;
uniform bool maskInvertStencil;

void main()
{
  vec4 mask = vec4(texture_read_as_srgb(maskImage, maskImagePremultiplied, uv_interp).rgb, 1.0);
  if (maskInvertStencil) {
    mask.rgb = 1.0 - mask.rgb;
  }
  float mask_step = smoothstep(0.0, 3.0, mask.r + mask.g + mask.b);
  mask.rgb *= maskColor;
  mask.a = mask_step * opacity;

  fragColor = mask;
}
