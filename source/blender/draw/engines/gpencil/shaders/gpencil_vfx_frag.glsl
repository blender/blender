
uniform sampler2D colorBuf;
uniform sampler2D revealBuf;

in vec4 uvcoordsvar;

/* Reminder: This is considered SRC color in blend equations.
 * Same operation on all buffers. */
layout(location = 0) out vec4 fragColor;
layout(location = 1) out vec4 fragRevealage;

float gaussian_weight(float x)
{
  return exp(-x * x / (2.0 * 0.35 * 0.35));
}

#if defined(COMPOSITE)

uniform bool isFirstPass;

void main()
{
  if (isFirstPass) {
    /* Blend mode is multiply. */
    fragColor.rgb = fragRevealage.rgb = texture(revealBuf, uvcoordsvar.xy).rgb;
    fragColor.a = fragRevealage.a = 1.0;
  }
  else {
    /* Blend mode is additive. */
    fragRevealage = vec4(0.0);
    fragColor.rgb = texture(colorBuf, uvcoordsvar.xy).rgb;
    fragColor.a = 0.0;
  }
}

#elif defined(COLORIZE)

uniform vec3 lowColor;
uniform vec3 highColor;
uniform float factor;
uniform int mode;

const mat3 sepia_mat = mat3(
    vec3(0.393, 0.349, 0.272), vec3(0.769, 0.686, 0.534), vec3(0.189, 0.168, 0.131));

#  define MODE_GRAYSCALE 0
#  define MODE_SEPIA 1
#  define MODE_DUOTONE 2
#  define MODE_CUSTOM 3
#  define MODE_TRANSPARENT 4

void main()
{
  fragColor = texture(colorBuf, uvcoordsvar.xy);
  fragRevealage = texture(revealBuf, uvcoordsvar.xy);

  float luma = dot(fragColor.rgb, vec3(0.2126, 0.7152, 0.723));

  /* No blending. */
  switch (mode) {
    case MODE_GRAYSCALE:
      fragColor.rgb = mix(fragColor.rgb, vec3(luma), factor);
      break;
    case MODE_SEPIA:
      fragColor.rgb = mix(fragColor.rgb, sepia_mat * fragColor.rgb, factor);
      break;
    case MODE_DUOTONE:
      fragColor.rgb = luma * ((luma <= factor) ? lowColor : highColor);
      break;
    case MODE_CUSTOM:
      fragColor.rgb = mix(fragColor.rgb, luma * lowColor, factor);
      break;
    case MODE_TRANSPARENT:
    default:
      fragColor.rgb *= factor;
      fragRevealage.rgb = mix(vec3(1.0), fragRevealage.rgb, factor);
      break;
  }
}

#elif defined(BLUR)

uniform vec2 offset;
uniform int sampCount;

void main()
{
  vec2 pixel_size = 1.0 / vec2(textureSize(revealBuf, 0).xy);
  vec2 ofs = offset * pixel_size;

  fragColor = vec4(0.0);
  fragRevealage = vec4(0.0);

  /* No blending. */
  float weight_accum = 0.0;
  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount);
    float weight = gaussian_weight(x);
    weight_accum += weight;
    vec2 uv = uvcoordsvar.xy + ofs * x;
    fragColor.rgb += texture(colorBuf, uv).rgb * weight;
    fragRevealage.rgb += texture(revealBuf, uv).rgb * weight;
  }

  fragColor /= weight_accum;
  fragRevealage /= weight_accum;
}

#elif defined(TRANSFORM)

uniform vec2 axisFlip = vec2(1.0);
uniform vec2 waveDir = vec2(0.0);
uniform vec2 waveOffset = vec2(0.0);
uniform float wavePhase = 0.0;
uniform vec2 swirlCenter = vec2(0.0);
uniform float swirlAngle = 0.0;
uniform float swirlRadius = 0.0;

void main()
{
  vec2 uv = (uvcoordsvar.xy - 0.5) * axisFlip + 0.5;

  /* Wave deform. */
  float wave_time = dot(uv, waveDir.xy);
  uv += sin(wave_time + wavePhase) * waveOffset;
  /* Swirl deform. */
  if (swirlRadius > 0.0) {
    vec2 tex_size = vec2(textureSize(colorBuf, 0).xy);
    vec2 pix_coord = uv * tex_size - swirlCenter;
    float dist = length(pix_coord);
    float percent = clamp((swirlRadius - dist) / swirlRadius, 0.0, 1.0);
    float theta = percent * percent * swirlAngle;
    float s = sin(theta);
    float c = cos(theta);
    mat2 rot = mat2(vec2(c, -s), vec2(s, c));
    uv = (rot * pix_coord + swirlCenter) / tex_size;
  }

  fragColor = texture(colorBuf, uv);
  fragRevealage = texture(revealBuf, uv);
}

#elif defined(GLOW)

uniform vec4 glowColor;
uniform vec2 offset;
uniform int sampCount;
uniform vec4 threshold;
uniform bool firstPass;
uniform bool glowUnder;
uniform int blendMode;

void main()
{
  vec2 pixel_size = 1.0 / vec2(textureSize(revealBuf, 0).xy);
  vec2 ofs = offset * pixel_size;

  fragColor = vec4(0.0);
  fragRevealage = vec4(0.0);

  float weight_accum = 0.0;
  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount);
    float weight = gaussian_weight(x);
    weight_accum += weight;
    vec2 uv = uvcoordsvar.xy + ofs * x;
    vec3 col = texture(colorBuf, uv).rgb;
    vec3 rev = texture(revealBuf, uv).rgb;
    if (threshold.x > -1.0) {
      if (threshold.y > -1.0) {
        if (any(greaterThan(abs(col - vec3(threshold)), vec3(threshold.w)))) {
          weight = 0.0;
        }
      }
      else {
        if (dot(col, vec3(1.0 / 3.0)) < threshold.x) {
          weight = 0.0;
        }
      }
    }
    fragColor.rgb += col * weight;
    fragRevealage.rgb += (1.0 - rev) * weight;
  }

  if (weight_accum > 0.0) {
    fragColor *= glowColor.rgbb / weight_accum;
    fragRevealage = fragRevealage / weight_accum;
  }
  fragRevealage = 1.0 - fragRevealage;

  if (glowUnder) {
    if (firstPass) {
      /* In first pass we copy the revealage buffer in the alpha channel.
       * This let us do the alpha under in second pass. */
      vec3 original_revealage = texture(revealBuf, uvcoordsvar.xy).rgb;
      fragRevealage.a = clamp(dot(original_revealage.rgb, vec3(0.333334)), 0.0, 1.0);
    }
    else {
      /* Recover original revealage. */
      fragRevealage.a = texture(revealBuf, uvcoordsvar.xy).a;
    }
  }

  if (!firstPass) {
    fragColor.a = clamp(1.0 - dot(fragRevealage.rgb, vec3(0.333334)), 0.0, 1.0);
    fragRevealage.a *= glowColor.a;
    blend_mode_output(blendMode, fragColor, fragRevealage.a, fragColor, fragRevealage);
  }
}

#elif defined(RIM)

uniform vec2 blurDir;
uniform vec2 uvOffset;
uniform vec3 rimColor;
uniform vec3 maskColor;
uniform int sampCount;
uniform int blendMode;
uniform bool isFirstPass;

void main()
{
  /* Blur revealage buffer. */
  fragRevealage = vec4(0.0);
  float weight_accum = 0.0;
  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount);
    float weight = gaussian_weight(x);
    weight_accum += weight;
    vec2 uv = uvcoordsvar.xy + blurDir * x + uvOffset;
    vec3 col = texture(revealBuf, uv).rgb;
    if (any(not(equal(vec2(0.0), floor(uv))))) {
      col = vec3(0.0);
    }
    fragRevealage.rgb += col * weight;
  }
  fragRevealage /= weight_accum;

  if (isFirstPass) {
    /* In first pass we copy the reveal buffer. This let us do alpha masking in second pass. */
    fragColor = texture(revealBuf, uvcoordsvar.xy);
    /* Also add the masked color to the reveal buffer. */
    vec3 col = texture(colorBuf, uvcoordsvar.xy).rgb;
    if (all(lessThan(abs(col - maskColor), vec3(0.05)))) {
      fragColor = vec4(1.0);
    }
  }
  else {
    /* Premult by foreground alpha (alpha mask). */
    float mask = 1.0 - clamp(dot(vec3(0.333334), texture(colorBuf, uvcoordsvar.xy).rgb), 0.0, 1.0);

    /* fragRevealage is blurred shadow. */
    float rim = clamp(dot(vec3(0.333334), fragRevealage.rgb), 0.0, 1.0);

    vec4 color = vec4(rimColor, 1.0);

    blend_mode_output(blendMode, color, rim * mask, fragColor, fragRevealage);
  }
}

#elif defined(SHADOW)

uniform vec4 shadowColor;
uniform vec2 uvRotX;
uniform vec2 uvRotY;
uniform vec2 uvOffset;
uniform vec2 blurDir;
uniform vec2 waveDir;
uniform vec2 waveOffset;
uniform float wavePhase;
uniform int sampCount;
uniform bool isFirstPass;

vec2 compute_uvs(float x)
{
  vec2 uv = uvcoordsvar.xy;
  /* Transform UV (loc, rot, scale) */
  uv = uv.x * uvRotX + uv.y * uvRotY + uvOffset;
  uv += blurDir * x;
  /* Wave deform. */
  float wave_time = dot(uv, waveDir.xy);
  uv += sin(wave_time + wavePhase) * waveOffset;
  return uv;
}

void main()
{
  /* Blur revealage buffer. */
  fragRevealage = vec4(0.0);
  float weight_accum = 0.0;
  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount);
    float weight = gaussian_weight(x);
    weight_accum += weight;
    vec2 uv = compute_uvs(x);
    vec3 col = texture(revealBuf, uv).rgb;
    if (any(not(equal(vec2(0.0), floor(uv))))) {
      col = vec3(1.0);
    }
    fragRevealage.rgb += col * weight;
  }
  fragRevealage /= weight_accum;

  /* No blending in first pass, alpha over premult in second pass. */
  if (isFirstPass) {
    /* In first pass we copy the reveal buffer. This let us do alpha under in second pass. */
    fragColor = texture(revealBuf, uvcoordsvar.xy);
  }
  else {
    /* fragRevealage is blurred shadow. */
    float shadow_fac = 1.0 - clamp(dot(vec3(0.333334), fragRevealage.rgb), 0.0, 1.0);
    /* Premult by foreground revealage (alpha under). */
    vec3 original_revealage = texture(colorBuf, uvcoordsvar.xy).rgb;
    shadow_fac *= clamp(dot(vec3(0.333334), original_revealage), 0.0, 1.0);
    /* Modulate by opacity */
    shadow_fac *= shadowColor.a;
    /* Apply shadow color. */
    fragColor.rgb = mix(vec3(0.0), shadowColor.rgb, shadow_fac);
    /* Alpha over (mask behind the shadow). */
    fragColor.a = shadow_fac;

    fragRevealage.rgb = original_revealage * (1.0 - shadow_fac);
    /* Replace the whole revealage buffer. */
    fragRevealage.a = 1.0;
  }
}

#elif defined(PIXELIZE)

uniform vec2 targetPixelSize;
uniform vec2 targetPixelOffset;
uniform vec2 accumOffset;
uniform int sampCount;

void main()
{
  vec2 pixel = floor((uvcoordsvar.xy - targetPixelOffset) / targetPixelSize);
  vec2 uv = (pixel + 0.5) * targetPixelSize + targetPixelOffset;

  fragColor = vec4(0.0);
  fragRevealage = vec4(0.0);

  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount + 1);
    vec2 uv_ofs = uv + accumOffset * 0.5 * x;
    fragColor += texture(colorBuf, uv_ofs);
    fragRevealage += texture(revealBuf, uv_ofs);
  }

  fragColor /= float(sampCount) * 2.0 + 1.0;
  fragRevealage /= float(sampCount) * 2.0 + 1.0;
}

#endif
