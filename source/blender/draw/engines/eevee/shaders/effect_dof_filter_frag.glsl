
/**
 * Gather Filter pass: Filter the gather pass result to reduce noise.
 *
 * This is a simple 3x3 median filter to avoid dilating highlights with a 3x3 max filter even if
 * cheaper.
 **/

#pragma BLENDER_REQUIRE(effect_dof_lib.glsl)

uniform sampler2D colorBuffer;
uniform sampler2D weightBuffer;

in vec4 uvcoordsvar;

layout(location = 0) out vec4 outColor;
layout(location = 1) out float outWeight;

/* From:
 * Implementing Median Filters in XC4000E FPGAs
 * JOHN L. SMITH, Univision Technologies Inc., Billerica, MA
 * http://users.utcluj.ro/~baruch/resources/Image/xl23_16.pdf
 * Figure 1 */

/* Outputs low median and high value of a triple. */
void lmh(vec4 s1, vec4 s2, vec4 s3, out vec4 l, out vec4 m, out vec4 h)
{
  /* From diagram, with nodes numbered from top to bottom. */
  vec4 h1 = max(s2, s3);
  vec4 l1 = min(s2, s3);

  vec4 h2 = max(s1, l1);
  vec4 l2 = min(s1, l1);

  vec4 h3 = max(h2, h1);
  vec4 l3 = min(h2, h1);

  l = l2;
  m = l3;
  h = h3;
}

vec4 median_filter(sampler2D tex, vec2 uv)
{
  vec2 texel_size = 1.0 / vec2(textureSize(tex, 0).xy);
  vec4 samples[9];
  int s = 0;

  const vec2 ofs[9] = vec2[9](vec2(-1, -1),
                              vec2(0, -1),
                              vec2(1, -1),
                              vec2(-1, 0),
                              vec2(0, 0),
                              vec2(1, 0),
                              vec2(-1, 1),
                              vec2(0, 1),
                              vec2(1, 1));

  for (int s = 0; s < 9; s++) {
    samples[s] = textureLod(tex, uv + ofs[s] * texel_size, 0.0);
  }

  if (no_gather_filtering) {
    return samples[4];
  }

  for (int s = 0; s < 9; s += 3) {
    lmh(samples[s], samples[s + 1], samples[s + 2], samples[s], samples[s + 1], samples[s + 2]);
  }
  /* Some aliases to better understand what's happening. */
  vec4 L123 = samples[0 + 0], L456 = samples[3 + 0], L789 = samples[6 + 0];
  vec4 M123 = samples[0 + 1], M456 = samples[3 + 1], M789 = samples[6 + 1];
  vec4 H123 = samples[0 + 2], H456 = samples[3 + 2], H789 = samples[6 + 2];
  vec4 dummy, l, m, h;
  /* Left nodes. */
  h = max(max(L123, L456), L789);
  /* Right nodes. */
  l = min(min(H123, H456), H789);
  /* Center nodes. */
  lmh(M123, M456, M789, dummy, m, dummy);
  /* Last bottom nodes. */
  lmh(l, m, h, dummy, m, dummy);

  return m;
}

void main()
{
  /* OPTI(fclem) Could early return on some tiles. */

  outColor = median_filter(colorBuffer, uvcoordsvar.xy);
  outWeight = median_filter(weightBuffer, uvcoordsvar.xy).r;
}