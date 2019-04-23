uniform int color_type;
uniform sampler2D myTexture;

uniform float gradient_f;

uniform vec4 colormix;
uniform float mix_stroke_factor;
uniform int shading_type[2];

in vec4 mColor;
in vec2 mTexCoord;
in vec2 uvfac;

out vec4 fragColor;

#define texture2D texture

/* keep this list synchronized with list in gpencil_engine.h */
#define GPENCIL_COLOR_SOLID 0
#define GPENCIL_COLOR_TEXTURE 1
#define GPENCIL_COLOR_PATTERN 2

#define ENDCAP 1.0

#define OB_SOLID 3
#define V3D_SHADING_TEXTURE_COLOR 3

bool no_texture = (shading_type[0] == OB_SOLID) && (shading_type[1] != V3D_SHADING_TEXTURE_COLOR);

float linearrgb_to_srgb(float c)
{
  if (c < 0.0031308) {
    return (c < 0.0) ? 0.0 : c * 12.92;
  }
  else {
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
  }
}

vec4 texture_read_as_srgb(sampler2D tex, vec2 co)
{
  /* By convention image textures return scene linear colors, but
   * grease pencil still works in srgb. */
  vec4 color = texture2D(tex, co);
  color.r = linearrgb_to_srgb(color.r);
  color.g = linearrgb_to_srgb(color.g);
  color.b = linearrgb_to_srgb(color.b);
  return color;
}

void main()
{

  vec4 tColor = vec4(mColor);
  /* if uvfac[1]  == 1, then encap */
  if (uvfac[1] == ENDCAP) {
    vec2 center = vec2(uvfac[0], 0.5);
    float dist = length(mTexCoord - center);
    if (dist > 0.50) {
      discard;
    }
  }

  if ((color_type == GPENCIL_COLOR_SOLID) || (no_texture)) {
    fragColor = tColor;
  }

  /* texture for endcaps */
  vec4 text_color;
  if (uvfac[1] == ENDCAP) {
    text_color = texture_read_as_srgb(myTexture, vec2(mTexCoord.x, mTexCoord.y));
  }
  else {
    text_color = texture_read_as_srgb(myTexture, mTexCoord);
  }

  /* texture */
  if ((color_type == GPENCIL_COLOR_TEXTURE) && (!no_texture)) {
    if (mix_stroke_factor > 0.0) {
      fragColor.rgb = mix(text_color.rgb, colormix.rgb, mix_stroke_factor);
      fragColor.a = text_color.a;
    }
    else {
      fragColor = text_color;
    }

    /* mult both alpha factor to use strength factor */
    fragColor.a = min(fragColor.a * tColor.a, fragColor.a);
  }
  /* pattern */
  if ((color_type == GPENCIL_COLOR_PATTERN) && (!no_texture)) {
    fragColor = tColor;
    /* mult both alpha factor to use strength factor with color alpha limit */
    fragColor.a = min(text_color.a * tColor.a, tColor.a);
  }

  /* gradient */
  /* keep this disabled while the line glitch bug exists
  if (gradient_f < 1.0) {
    float d = abs(mTexCoord.y - 0.5)  * (1.1 - gradient_f);
    float alpha = 1.0 - clamp((fragColor.a - (d * 2.0)), 0.03, 1.0);
    fragColor.a = smoothstep(fragColor.a, 0.0, alpha);

  }
  */

  if (fragColor.a < 0.0035)
    discard;
}
