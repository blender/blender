/* SPDX-FileCopyrightText: 2020-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "infos/overlay_background_info.hh"

FRAGMENT_SHADER_CREATE_INFO(overlay_background)

#include "gpu_shader_math_base_lib.glsl"

/* 4x4 bayer matrix prepared for 8bit UNORM precision error. */
#define P(x) (((x + 0.5f) * (1.0f / 16.0f) - 0.5f) * (1.0f / 255.0f))

float dither()
{
  /* NOTE(Metal): Declaring constant array in function scope to avoid increasing local shader
   * memory pressure. */
  const float4 dither_mat4x4[4] = float4_array(float4(P(0.0f), P(8.0f), P(2.0f), P(10.0f)),
                                               float4(P(12.0f), P(4.0f), P(14.0f), P(6.0f)),
                                               float4(P(3.0f), P(11.0f), P(1.0f), P(9.0f)),
                                               float4(P(15.0f), P(7.0f), P(13.0f), P(5.0f)));

  int2 co = int2(gl_FragCoord.xy) % 4;
  return dither_mat4x4[co.x][co.y];
}

void main()
{
  /* The blend equation is:
   * `result.rgb = SRC.rgb * (1 - DST.a) + DST.rgb * (SRC.a)`
   * `result.a = SRC.a * 0 + DST.a * SRC.a`
   * This removes the alpha channel and put the background behind reference images
   * while masking the reference images by the render alpha.
   */
  float alpha = texture(colorBuffer, uvcoordsvar.xy).a;
  float depth = texture(depthBuffer, uvcoordsvar.xy).r;

  float3 bg_col;
  float3 col_high;
  float3 col_low;

  /* BG_SOLID_CHECKER selects BG_SOLID when no pixel has been drawn otherwise use the BG_CHERKER.
   */
  int bg_type = bgType == BG_SOLID_CHECKER ? (depth == 1.0f ? BG_SOLID : BG_CHECKER) : bgType;

  switch (bg_type) {
    case BG_SOLID:
      bg_col = colorBackground.rgb;
      break;
    case BG_GRADIENT:
      /* XXX do interpolation in a non-linear space to have a better visual result. */
      col_high = pow(colorBackground.rgb, float3(1.0f / 2.2f));
      col_low = pow(colorBackgroundGradient.rgb, float3(1.0f / 2.2f));
      bg_col = mix(col_low, col_high, uvcoordsvar.y);
      /* Convert back to linear. */
      bg_col = pow(bg_col, float3(2.2f));
      /*  Dither to hide low precision buffer. (Could be improved) */
      bg_col += dither();
      break;
    case BG_RADIAL: {
      /* Do interpolation in a non-linear space to have a better visual result. */
      col_high = pow(colorBackground.rgb, float3(1.0f / 2.2f));
      col_low = pow(colorBackgroundGradient.rgb, float3(1.0f / 2.2f));

      float2 uv_n = uvcoordsvar.xy - 0.5f;
      bg_col = mix(col_high, col_low, length(uv_n) * M_SQRT2);

      /* Convert back to linear. */
      bg_col = pow(bg_col, float3(2.2f));
      /*  Dither to hide low precision buffer. (Could be improved) */
      bg_col += dither();
      break;
    }
    case BG_CHECKER: {
      float size = sizeChecker * sizePixel;
      int2 p = int2(floor(gl_FragCoord.xy / size));
      bool check = mod(p.x, 2) == mod(p.y, 2);
      bg_col = (check) ? colorCheckerPrimary.rgb : colorCheckerSecondary.rgb;
      break;
    }
    case BG_MASK:
      fragColor = float4(float3(1.0f - alpha), 0.0f);
      return;
  }

  bg_col = mix(bg_col, colorOverride.rgb, colorOverride.a);

  /* Mimic alpha under behavior. Result is premultiplied. */
  fragColor = float4(bg_col, 1.0f) * (1.0f - alpha);

  /* Special case: If the render is not transparent, do not clear alpha values. */
  if (depth == 1.0f && alpha == 1.0f) {
    fragColor.a = 1.0f;
  }
}
