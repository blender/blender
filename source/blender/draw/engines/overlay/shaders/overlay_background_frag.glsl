

#pragma BLENDER_REQUIRE(common_math_lib.glsl)

/* 4x4 bayer matrix prepared for 8bit UNORM precision error. */
#define P(x) (((x + 0.5) * (1.0 / 16.0) - 0.5) * (1.0 / 255.0))

float dither(void)
{
  /* NOTE(Metal): Declaring constant array in function scope to avoid increasing local shader
   * memory pressure. */
  const vec4 dither_mat4x4[4] = vec4[4](vec4(P(0.0), P(8.0), P(2.0), P(10.0)),
                                        vec4(P(12.0), P(4.0), P(14.0), P(6.0)),
                                        vec4(P(3.0), P(11.0), P(1.0), P(9.0)),
                                        vec4(P(15.0), P(7.0), P(13.0), P(5.0)));

  ivec2 co = ivec2(gl_FragCoord.xy) % 4;
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

  vec3 bg_col;
  vec3 col_high;
  vec3 col_low;

  /* BG_SOLID_CHECKER selects BG_SOLID when no pixel has been drawn otherwise use the BG_CHERKER.
   */
  int bg_type = bgType == BG_SOLID_CHECKER ? (depth == 1.0 ? BG_SOLID : BG_CHECKER) : bgType;

  switch (bg_type) {
    case BG_SOLID:
      bg_col = colorBackground.rgb;
      break;
    case BG_GRADIENT:
      /* XXX do interpolation in a non-linear space to have a better visual result. */
      col_high = pow(colorBackground.rgb, vec3(1.0 / 2.2));
      col_low = pow(colorBackgroundGradient.rgb, vec3(1.0 / 2.2));
      bg_col = mix(col_low, col_high, uvcoordsvar.y);
      /* Convert back to linear. */
      bg_col = pow(bg_col, vec3(2.2));
      /*  Dither to hide low precision buffer. (Could be improved) */
      bg_col += dither();
      break;
    case BG_RADIAL: {
      /* Do interpolation in a non-linear space to have a better visual result. */
      col_high = pow(colorBackground.rgb, vec3(1.0 / 2.2));
      col_low = pow(colorBackgroundGradient.rgb, vec3(1.0 / 2.2));

      vec2 uv_n = uvcoordsvar.xy - 0.5;
      bg_col = mix(col_high, col_low, length(uv_n) * M_SQRT2);

      /* Convert back to linear. */
      bg_col = pow(bg_col, vec3(2.2));
      /*  Dither to hide low precision buffer. (Could be improved) */
      bg_col += dither();
      break;
    }
    case BG_CHECKER: {
      float size = sizeChecker * sizePixel;
      ivec2 p = ivec2(floor(gl_FragCoord.xy / size));
      bool check = mod(p.x, 2) == mod(p.y, 2);
      bg_col = (check) ? colorCheckerPrimary.rgb : colorCheckerSecondary.rgb;
      break;
    }
    case BG_MASK:
      fragColor = vec4(vec3(1.0 - alpha), 0.0);
      return;
  }

  bg_col = mix(bg_col, colorOverride.rgb, colorOverride.a);

  /* Mimic alpha under behavior. Result is premultiplied. */
  fragColor = vec4(bg_col, 1.0) * (1.0 - alpha);

  /* Special case: If the render is not transparent, do not clear alpha values. */
  if (depth == 1.0 && alpha == 1.0) {
    fragColor.a = 1.0;
  }
}
