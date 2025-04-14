/* SPDX-FileCopyrightText: 2020-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/gpencil_vfx_info.hh"

FRAGMENT_SHADER_CREATE_INFO(gpencil_fx_composite)

#include "gpencil_common_lib.glsl"

float gaussian_weight(float x)
{
  return exp(-x * x / (2.0f * 0.35f * 0.35f));
}

#if defined(COMPOSITE)

void main()
{
  if (isFirstPass) {
    /* Blend mode is multiply. */
    fragColor.rgb = fragRevealage.rgb = texture(revealBuf, uvcoordsvar.xy).rgb;
    fragColor.a = fragRevealage.a = 1.0f;
  }
  else {
    /* Blend mode is additive. */
    fragRevealage = float4(0.0f);
    fragColor.rgb = texture(colorBuf, uvcoordsvar.xy).rgb;
    fragColor.a = 0.0f;
  }
}

#elif defined(COLORIZE)

#  define sepia_mat \
    float3x3(float3(0.393f, 0.349f, 0.272f), \
             float3(0.769f, 0.686f, 0.534f), \
             float3(0.189f, 0.168f, 0.131f))

#  define MODE_GRAYSCALE 0
#  define MODE_SEPIA 1
#  define MODE_DUOTONE 2
#  define MODE_CUSTOM 3
#  define MODE_TRANSPARENT 4

void main()
{
  fragColor = texture(colorBuf, uvcoordsvar.xy);
  fragRevealage = texture(revealBuf, uvcoordsvar.xy);

  float luma = dot(fragColor.rgb, float3(0.2126f, 0.7152f, 0.723f));

  /* No blending. */
  switch (mode) {
    case MODE_GRAYSCALE:
      fragColor.rgb = mix(fragColor.rgb, float3(luma), factor);
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
      fragRevealage.rgb = mix(float3(1.0f), fragRevealage.rgb, factor);
      break;
  }
}

#elif defined(BLUR)

void main()
{
  float2 pixel_size = 1.0f / float2(textureSize(revealBuf, 0).xy);
  float2 ofs = offset * pixel_size;

  fragColor = float4(0.0f);
  fragRevealage = float4(0.0f);

  /* No blending. */
  float weight_accum = 0.0f;
  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount);
    float weight = gaussian_weight(x);
    weight_accum += weight;
    float2 uv = uvcoordsvar.xy + ofs * x;
    fragColor.rgb += texture(colorBuf, uv).rgb * weight;
    fragRevealage.rgb += texture(revealBuf, uv).rgb * weight;
  }

  fragColor /= weight_accum;
  fragRevealage /= weight_accum;
}

#elif defined(TRANSFORM)

void main()
{
  float2 uv = (uvcoordsvar.xy - 0.5f) * axisFlip + 0.5f;

  /* Wave deform. */
  float wave_time = dot(uv, waveDir.xy);
  uv += sin(wave_time + wavePhase) * waveOffset;
  /* Swirl deform. */
  if (swirlRadius > 0.0f) {
    float2 tex_size = float2(textureSize(colorBuf, 0).xy);
    float2 pix_coord = uv * tex_size - swirlCenter;
    float dist = length(pix_coord);
    float percent = clamp((swirlRadius - dist) / swirlRadius, 0.0f, 1.0f);
    float theta = percent * percent * swirlAngle;
    float s = sin(theta);
    float c = cos(theta);
    float2x2 rot = float2x2(float2(c, -s), float2(s, c));
    uv = (rot * pix_coord + swirlCenter) / tex_size;
  }

  fragColor = texture(colorBuf, uv);
  fragRevealage = texture(revealBuf, uv);
}

#elif defined(GLOW)

void main()
{
  float2 pixel_size = 1.0f / float2(textureSize(revealBuf, 0).xy);
  float2 ofs = offset * pixel_size;

  fragColor = float4(0.0f);
  fragRevealage = float4(0.0f);

  float weight_accum = 0.0f;
  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount);
    float weight = gaussian_weight(x);
    weight_accum += weight;
    float2 uv = uvcoordsvar.xy + ofs * x;
    float3 col = texture(colorBuf, uv).rgb;
    float3 rev = texture(revealBuf, uv).rgb;
    if (threshold.x > -1.0f) {
      if (threshold.y > -1.0f) {
        if (any(greaterThan(abs(col - float3(threshold)), float3(threshold.w)))) {
          weight = 0.0f;
        }
      }
      else {
        if (dot(col, float3(1.0f / 3.0f)) < threshold.x) {
          weight = 0.0f;
        }
      }
    }
    fragColor.rgb += col * weight;
    fragRevealage.rgb += (1.0f - rev) * weight;
  }

  if (weight_accum > 0.0f) {
    fragColor *= glowColor.rgbb / weight_accum;
    fragRevealage = fragRevealage / weight_accum;
  }
  fragRevealage = 1.0f - fragRevealage;

  if (glowUnder) {
    if (firstPass) {
      /* In first pass we copy the revealage buffer in the alpha channel.
       * This let us do the alpha under in second pass. */
      float3 original_revealage = texture(revealBuf, uvcoordsvar.xy).rgb;
      fragRevealage.a = clamp(dot(original_revealage.rgb, float3(0.333334f)), 0.0f, 1.0f);
    }
    else {
      /* Recover original revealage. */
      fragRevealage.a = texture(revealBuf, uvcoordsvar.xy).a;
    }
  }

  if (!firstPass) {
    fragColor.a = clamp(1.0f - dot(fragRevealage.rgb, float3(0.333334f)), 0.0f, 1.0f);
    fragRevealage.a *= glowColor.a;
    blend_mode_output(blendMode, fragColor, fragRevealage.a, fragColor, fragRevealage);
  }
}

#elif defined(RIM)

void main()
{
  /* Blur revealage buffer. */
  fragRevealage = float4(0.0f);
  float weight_accum = 0.0f;
  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount);
    float weight = gaussian_weight(x);
    weight_accum += weight;
    float2 uv = uvcoordsvar.xy + blurDir * x + uvOffset;
    float3 col = texture(revealBuf, uv).rgb;
    if (any(not(equal(float2(0.0f), floor(uv))))) {
      col = float3(0.0f);
    }
    fragRevealage.rgb += col * weight;
  }
  fragRevealage /= weight_accum;

  if (isFirstPass) {
    /* In first pass we copy the reveal buffer. This let us do alpha masking in second pass. */
    fragColor = texture(revealBuf, uvcoordsvar.xy);
    /* Also add the masked color to the reveal buffer. */
    float3 col = texture(colorBuf, uvcoordsvar.xy).rgb;
    if (all(lessThan(abs(col - maskColor), float3(0.05f)))) {
      fragColor = float4(1.0f);
    }
  }
  else {
    /* Pre-multiply by foreground alpha (alpha mask). */
    float mask = 1.0f -
                 clamp(dot(float3(0.333334f), texture(colorBuf, uvcoordsvar.xy).rgb), 0.0f, 1.0f);

    /* fragRevealage is blurred shadow. */
    float rim = clamp(dot(float3(0.333334f), fragRevealage.rgb), 0.0f, 1.0f);

    float4 color = float4(rimColor, 1.0f);

    blend_mode_output(blendMode, color, rim * mask, fragColor, fragRevealage);
  }
}

#elif defined(SHADOW)

float2 compute_uvs(float x)
{
  float2 uv = uvcoordsvar.xy;
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
  fragRevealage = float4(0.0f);
  float weight_accum = 0.0f;
  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount);
    float weight = gaussian_weight(x);
    weight_accum += weight;
    float2 uv = compute_uvs(x);
    float3 col = texture(revealBuf, uv).rgb;
    if (any(not(equal(float2(0.0f), floor(uv))))) {
      col = float3(1.0f);
    }
    fragRevealage.rgb += col * weight;
  }
  fragRevealage /= weight_accum;

  /* No blending in first pass, alpha over pre-multiply in second pass. */
  if (isFirstPass) {
    /* In first pass we copy the reveal buffer. This let us do alpha under in second pass. */
    fragColor = texture(revealBuf, uvcoordsvar.xy);
  }
  else {
    /* fragRevealage is blurred shadow. */
    float shadow_fac = 1.0f - clamp(dot(float3(0.333334f), fragRevealage.rgb), 0.0f, 1.0f);
    /* Pre-multiply by foreground revealage (alpha under). */
    float3 original_revealage = texture(colorBuf, uvcoordsvar.xy).rgb;
    shadow_fac *= clamp(dot(float3(0.333334f), original_revealage), 0.0f, 1.0f);
    /* Modulate by opacity */
    shadow_fac *= shadowColor.a;
    /* Apply shadow color. */
    fragColor.rgb = mix(float3(0.0f), shadowColor.rgb, shadow_fac);
    /* Alpha over (mask behind the shadow). */
    fragColor.a = shadow_fac;

    fragRevealage.rgb = original_revealage * (1.0f - shadow_fac);
    /* Replace the whole revealage buffer. */
    fragRevealage.a = 1.0f;
  }
}

#elif defined(PIXELIZE)

void main()
{
  float2 pixel = floor((uvcoordsvar.xy - targetPixelOffset) / targetPixelSize);
  float2 uv = (pixel + 0.5f) * targetPixelSize + targetPixelOffset;

  fragColor = float4(0.0f);
  fragRevealage = float4(0.0f);

  for (int i = -sampCount; i <= sampCount; i++) {
    float x = float(i) / float(sampCount + 1);
    float2 uv_ofs = uv + accumOffset * 0.5f * x;
    fragColor += texture(colorBuf, uv_ofs);
    fragRevealage += texture(revealBuf, uv_ofs);
  }

  fragColor /= float(sampCount) * 2.0f + 1.0f;
  fragRevealage /= float(sampCount) * 2.0f + 1.0f;
}

#endif
