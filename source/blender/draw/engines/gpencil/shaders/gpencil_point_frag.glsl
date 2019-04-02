uniform int color_type;
uniform int mode;
uniform sampler2D myTexture;
uniform bool myTexturePremultiplied;

uniform float gradient_f;
uniform vec2 gradient_s;

uniform vec4 colormix;
uniform float mix_stroke_factor;
uniform int shading_type[2];

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

#define OB_SOLID 3
#define V3D_SHADING_TEXTURE_COLOR 3

bool no_texture = (shading_type[0] == OB_SOLID) && (shading_type[1] != V3D_SHADING_TEXTURE_COLOR);

/* Function to check the point inside ellipse */
float check_ellipse_point(vec2 pt, vec2 radius)
{
  float p = (pow(pt.x, 2) / pow(radius.x, 2)) + (pow(pt.y, 2) / pow(radius.y, 2));

  return p;
}

/* Function to check the point inside box */
vec2 check_box_point(vec2 pt, vec2 radius)
{
  vec2 rtn;
  rtn.x = abs(pt.x) / radius.x;
  rtn.y = abs(pt.y) / radius.y;

  return rtn;
}

void main()
{
  vec2 centered = mTexCoord - vec2(0.5);
  float ellip = check_ellipse_point(centered, vec2(gradient_s / 2.0));
  vec2 box;

  if (mode != GPENCIL_MODE_BOX) {
    if (ellip > 1.0) {
      discard;
    }
  }
  else {
    box = check_box_point(centered, vec2(gradient_s / 2.0));
    if ((box.x > 1.0) || (box.y > 1.0)) {
      discard;
    }
  }

  /* Solid */
  if ((color_type == GPENCIL_COLOR_SOLID) || (no_texture)) {
    fragColor = mColor;
  }
  /* texture */
  if ((color_type == GPENCIL_COLOR_TEXTURE) && (!no_texture)) {
    vec4 text_color = texture_read_as_srgb(myTexture, myTexturePremultiplied, mTexCoord);
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
  if ((color_type == GPENCIL_COLOR_PATTERN) && (!no_texture)) {
    vec4 text_color = texture_read_as_srgb(myTexture, myTexturePremultiplied, mTexCoord);
    fragColor = mColor;
    /* mult both alpha factor to use strength factor with color alpha limit */
    fragColor.a = min(text_color.a * mColor.a, mColor.a);
  }

  if (gradient_f < 1.0) {
    float dist = length(centered) * 2.0;
    float decay = dist * (1.0 - gradient_f) * fragColor.a;
    fragColor.a = clamp(fragColor.a - decay, 0.0, 1.0);
    if (mode == GPENCIL_MODE_DOTS) {
      fragColor.a = fragColor.a * (1.0 - ellip);
    }
  }

  if (fragColor.a < 0.0035) {
    discard;
  }
}
