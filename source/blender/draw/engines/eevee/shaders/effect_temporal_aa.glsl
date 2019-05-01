
uniform sampler2D colorHistoryBuffer;
uniform sampler2D velocityBuffer;

out vec4 FragColor;

vec4 safe_color(vec4 c)
{
  /* Clamp to avoid black square artifacts if a pixel goes NaN. */
  return clamp(c, vec4(0.0), vec4(1e20)); /* 1e20 arbitrary. */
}

#ifdef USE_REPROJECTION

/**
 * Adapted from https://casual-effects.com/g3d/G3D10/data-files/shader/Film/Film_temporalAA.pix
 * which is adapted from
 * https://github.com/gokselgoktas/temporal-anti-aliasing/blob/master/Assets/Resources/Shaders/TemporalAntiAliasing.cginc
 * which is adapted from https://github.com/playdeadgames/temporal
 * Optimization by Stubbesaurus and epsilon adjustment to avoid division by zero.
 *
 * This can cause 3x3 blocks of color when there is a thin edge of a similar color that
 * is varying in intensity.
 */
vec3 clip_to_aabb(vec3 color, vec3 minimum, vec3 maximum, vec3 average)
{
  /* note: only clips towards aabb center (but fast!) */
  vec3 center = 0.5 * (maximum + minimum);
  vec3 extents = 0.5 * (maximum - minimum);
  vec3 dist = color - center;
  vec3 ts = abs(extents) / max(abs(dist), vec3(0.0001));
  float t = saturate(min_v3(ts));
  return center + dist * t;
}

/**
 * Vastly based on https://github.com/playdeadgames/temporal
 */
void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);
  float depth = texelFetch(depthBuffer, texel, 0).r;
  vec2 motion = texelFetch(velocityBuffer, texel, 0).rg;

  /* Decode from unsigned normalized 16bit texture. */
  motion = motion * 2.0 - 1.0;

  /* Compute pixel position in previous frame. */
  vec2 screen_res = vec2(textureSize(colorBuffer, 0).xy);
  vec2 uv = gl_FragCoord.xy / screen_res;
  vec2 uv_history = uv - motion;

  ivec2 texel_history = ivec2(uv_history * screen_res);
  vec4 color_history = textureLod(colorHistoryBuffer, uv_history, 0.0);

  /* Color bounding box clamping. 3x3 neighborhood. */
  vec4 c02 = texelFetchOffset(colorBuffer, texel, 0, ivec2(-1, 1));
  vec4 c12 = texelFetchOffset(colorBuffer, texel, 0, ivec2(0, 1));
  vec4 c22 = texelFetchOffset(colorBuffer, texel, 0, ivec2(1, 1));
  vec4 c01 = texelFetchOffset(colorBuffer, texel, 0, ivec2(-1, 0));
  vec4 c11 = texelFetchOffset(colorBuffer, texel, 0, ivec2(0, 0));
  vec4 c21 = texelFetchOffset(colorBuffer, texel, 0, ivec2(1, 0));
  vec4 c00 = texelFetchOffset(colorBuffer, texel, 0, ivec2(-1, -1));
  vec4 c10 = texelFetchOffset(colorBuffer, texel, 0, ivec2(0, -1));
  vec4 c20 = texelFetchOffset(colorBuffer, texel, 0, ivec2(1, -1));

  vec4 color = c11;

  /* AABB minmax */
  vec4 min_col = min9(c02, c12, c22, c01, c11, c21, c00, c10, c20);
  vec4 max_col = max9(c02, c12, c22, c01, c11, c21, c00, c10, c20);
  vec4 avg_col = avg9(c02, c12, c22, c01, c11, c21, c00, c10, c20);

  /* bias the color aabb toward the center (rounding the shape) */
  vec4 min_center = min5(c12, c01, c11, c21, c10);
  vec4 max_center = max5(c12, c01, c11, c21, c10);
  vec4 avg_center = avg5(c12, c01, c11, c21, c10);
  min_col = (min_col + min_center) * 0.5;
  max_col = (max_col + max_center) * 0.5;
  avg_col = (avg_col + avg_center) * 0.5;

  /* Clip color toward the center of the neighborhood colors AABB box. */
  color_history.rgb = clip_to_aabb(color_history.rgb, min_col.rgb, max_col.rgb, avg_col.rgb);

  /* Luminance weighting. */
  /* TODO correct luminance */
  float lum0 = dot(color.rgb, vec3(0.333));
  float lum1 = dot(color_history.rgb, vec3(0.333));
  float diff = abs(lum0 - lum1) / max(lum0, max(lum1, 0.2));
  float weight = 1.0 - diff;
  float alpha = mix(0.04, 0.12, weight * weight);

  color_history = mix(color_history, color, alpha);

  bool out_of_view = any(greaterThanEqual(abs(uv_history - 0.5), vec2(0.5)));
  color_history = (out_of_view) ? color : color_history;

  FragColor = safe_color(color_history);
}

#else

uniform float alpha;

void main()
{
  ivec2 texel = ivec2(gl_FragCoord.xy);
  vec4 color = texelFetch(colorBuffer, texel, 0);
  vec4 color_history = texelFetch(colorHistoryBuffer, texel, 0);
  FragColor = safe_color(mix(color_history, color, alpha));
}
#endif
