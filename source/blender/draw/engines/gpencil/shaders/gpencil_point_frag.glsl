uniform int color_type;
uniform int mode;
uniform sampler2D myTexture;

uniform float gradient_f;
uniform vec2 gradient_s;

uniform vec4 colormix;
uniform float mix_stroke_factor;

in vec4 mColor;
in vec2 mTexCoord;
out vec4 fragColor;

#define texture2D texture

#define GPENCIL_MODE_LINE 0
#define GPENCIL_MODE_DOTS 1
#define GPENCIL_MODE_BOX 2

/* keep this list synchronized with list in gpencil_engine.h */
#define GPENCIL_COLOR_SOLID 0
#define GPENCIL_COLOR_TEXTURE 1
#define GPENCIL_COLOR_PATTERN 2

/* Function to check the point inside ellipse */
float checkpoint(vec2 pt, vec2 radius)
{
  float p = (pow(pt.x, 2) / pow(radius.x, 2)) + (pow(pt.y, 2) / pow(radius.y, 2));

  return p;
}

void main()
{
  vec2 centered = mTexCoord - vec2(0.5);
  float ellip = checkpoint(centered, vec2(gradient_s / 2.0));

  if (mode != GPENCIL_MODE_BOX) {
    if (ellip > 1.0) {
      discard;
    }
  }

  vec4 tmp_color = texture2D(myTexture, mTexCoord);

  /* Solid */
  if (color_type == GPENCIL_COLOR_SOLID) {
    fragColor = mColor;
  }
  /* texture */
  if (color_type == GPENCIL_COLOR_TEXTURE) {
    vec4 text_color = texture2D(myTexture, mTexCoord);
    if (mix_stroke_factor > 0.0) {
      fragColor.rgb = mix(text_color.rgb, colormix.rgb, mix_stroke_factor);
      fragColor.a = text_color.a;
    }
    else {
      fragColor = text_color;
    }

    /* mult both alpha factor to use strength factor with texture */
    fragColor.a = min(fragColor.a * mColor.a, fragColor.a);
  }
  /* pattern */
  if (color_type == GPENCIL_COLOR_PATTERN) {
    vec4 text_color = texture2D(myTexture, mTexCoord);
    fragColor = mColor;
    /* mult both alpha factor to use strength factor with color alpha limit */
    fragColor.a = min(text_color.a * mColor.a, mColor.a);
  }

  if ((mode == GPENCIL_MODE_DOTS) && (gradient_f < 1.0)) {
    float dist = length(centered) * 2;
    float decay = dist * (1.0 - gradient_f) * fragColor.a;
    fragColor.a = clamp(fragColor.a - decay, 0.0, 1.0);
    fragColor.a = fragColor.a * (1.0 - ellip);
  }

  if (fragColor.a < 0.0035) {
    discard;
  }
}
