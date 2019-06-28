in vec4 uvcoordsvar;

out vec4 FragColor;

uniform sampler2D strokeColor;
uniform sampler2D strokeDepth;
uniform sampler2D blendColor;
uniform sampler2D blendDepth;
uniform int mode;
uniform int mask_layer;
uniform int tonemapping;

#define ON 1
#define OFF 0

#define MODE_REGULAR 0
#define MODE_OVERLAY 1
#define MODE_ADD 2
#define MODE_SUB 3
#define MODE_MULTIPLY 4
#define MODE_DIVIDE 5

float overlay_color(float a, float b)
{
  float rtn;
  if (a < 0.5) {
    rtn = 2.0 * a * b;
  }
  else {
    rtn = 1.0 - 2.0 * (1.0 - a) * (1.0 - b);
  }

  return rtn;
}

vec4 get_blend_color(int mode, vec4 src_color, vec4 mix_color)
{
  vec4 outcolor;

  if (mix_color.a == 0) {
    return src_color;
  }

  switch (mode) {
    case MODE_REGULAR: {
      /* premult */
      src_color = vec4(vec3(src_color.rgb / src_color.a), src_color.a);
      mix_color = vec4(vec3(mix_color.rgb / mix_color.a), mix_color.a);

      outcolor = vec4(mix(src_color.rgb, mix_color.rgb, mix_color.a), src_color.a);
      break;
    }
    case MODE_OVERLAY: {
      src_color = vec4(vec3(src_color.rgb / src_color.a), src_color.a);
      mix_color = vec4(vec3(mix_color.rgb / mix_color.a), mix_color.a);

      mix_color.rgb = mix(src_color.rgb, mix_color.rgb, mix_color.a);
      outcolor.r = overlay_color(src_color.r, mix_color.r);
      outcolor.g = overlay_color(src_color.g, mix_color.g);
      outcolor.b = overlay_color(src_color.b, mix_color.b);
      outcolor.a = src_color.a;
      break;
    }
    case MODE_ADD: {
      mix_color.rgb = mix(src_color.rgb, mix_color.rgb, mix_color.a);
      outcolor = src_color + mix_color;
      outcolor.a = src_color.a;
      break;
    }
    case MODE_SUB: {
      mix_color.rgb = mix(src_color.rgb, mix_color.rgb, mix_color.a);
      outcolor = src_color - mix_color;
      outcolor.a = clamp(src_color.a - mix_color.a, 0.0, 1.0);
      break;
    }
    case MODE_MULTIPLY: {
      src_color = vec4(vec3(src_color.rgb / src_color.a), src_color.a);
      mix_color = vec4(vec3(mix_color.rgb / mix_color.a), mix_color.a);

      mix_color.rgb = mix(src_color.rgb, mix_color.rgb, mix_color.a);
      outcolor = src_color * mix_color;
      outcolor.a = src_color.a;
      break;
    }
    case MODE_DIVIDE: {
      mix_color.rgb = mix(src_color.rgb, mix_color.rgb, mix_color.a);
      outcolor = src_color / mix_color;
      outcolor.a = src_color.a;
      break;
    }
    default: {
      outcolor = mix_color;
      outcolor.a = src_color.a;
      break;
    }
  }
  return clamp(outcolor, 0.0, 1.0);
}

float linearrgb_to_srgb(float c)
{
  if (c < 0.0031308) {
    return (c < 0.0) ? 0.0 : c * 12.92;
  }
  else {
    return 1.055 * pow(c, 1.0 / 2.4) - 0.055;
  }
}

vec4 tone(vec4 stroke_color)
{
  if (tonemapping == 1) {
    vec4 color = vec4(0, 0, 0, stroke_color.a);
    color.r = linearrgb_to_srgb(stroke_color.r);
    color.g = linearrgb_to_srgb(stroke_color.g);
    color.b = linearrgb_to_srgb(stroke_color.b);
    return color;
  }
  else {
    return stroke_color;
  }
}

void main()
{
  vec4 outcolor;
  ivec2 uv = ivec2(gl_FragCoord.xy);
  vec4 stroke_color = texelFetch(strokeColor, uv, 0).rgba;
  float stroke_depth = texelFetch(strokeDepth, uv, 0).r;

  vec4 mix_color = texelFetch(blendColor, uv, 0).rgba;
  float mix_depth = texelFetch(blendDepth, uv, 0).r;

  if (stroke_color.a > 0) {
    if (mix_color.a > 0) {
      /* apply blend mode */
      FragColor = get_blend_color(mode, stroke_color, mix_color);
    }
    else {
      FragColor = stroke_color;
    }
    gl_FragDepth = min(stroke_depth, mix_depth);
  }
  else {
    if (mask_layer == ON) {
      discard;
    }
    else {
      /* if not using mask, return mix color */
      FragColor = mix_color;
      gl_FragDepth = mix_depth;
    }
  }

  /* apply tone mapping */
  FragColor = tone(FragColor);
}
