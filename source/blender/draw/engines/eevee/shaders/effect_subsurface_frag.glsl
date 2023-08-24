/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_view_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(common_uniforms_lib.glsl)

/* Based on Separable SSS. by Jorge Jimenez and Diego Gutierrez */

void main(void)
{
  vec2 pixel_size = 1.0 / vec2(textureSize(depthBuffer, 0).xy); /* TODO: precompute. */
  vec2 uvs = gl_FragCoord.xy * pixel_size;
  vec3 sss_irradiance = texture(sssIrradiance, uvs).rgb;
  float sss_radius = texture(sssRadius, uvs).r * radii_max_radius.w * avg_inv_radius;
  float depth = texture(depthBuffer, uvs).r;
  float depth_view = get_view_z_from_depth(depth);

  float rand = texelfetch_noise_tex(gl_FragCoord.xy).r;
#ifdef FIRST_PASS
  float angle = M_2PI * rand + M_PI_2;
  vec2 dir = vec2(1.0, 0.0);
#else /* SECOND_PASS */
  float angle = M_2PI * rand;
  vec2 dir = vec2(0.0, 1.0);
#endif
  vec2 dir_rand = vec2(cos(angle), sin(angle));

  /* Compute kernel bounds in 2D. */
  float homcoord = ProjectionMatrix[2][3] * depth_view + ProjectionMatrix[3][3];
  vec2 scale = vec2(ProjectionMatrix[0][0], ProjectionMatrix[1][1]) * sss_radius / homcoord;
  vec2 finalStep = scale * 0.5; /* samples range -1..1 */

  float sss_radius_inv = 1.0 / max(1e-8, sss_radius);

  /* Center sample */
  vec3 accum = sss_irradiance * sss_kernel[0].rgb;

  for (int i = 1; i < sss_samples && i < MAX_SSS_SAMPLES; i++) {
    vec2 sample_uv = uvs + sss_kernel[i].a * finalStep *
                               ((abs(sss_kernel[i].a) > sssJitterThreshold) ? dir : dir_rand);
    vec3 color = texture(sssIrradiance, sample_uv).rgb;
    float sample_depth = texture(depthBuffer, sample_uv).r;
    sample_depth = get_view_z_from_depth(sample_depth);
    /* Depth correction factor. See Real Time Realistic Skin Translucency 2010
     * by Jimenez, eqs. 2 and 9, and D9740.
     * Coefficient -2 follows from gaussian_profile() from gpu_material.c and
     * from the definition of finalStep. */
    float depth_delta = (depth_view - sample_depth) * sss_radius_inv;
    float s = exp(-2.0 * sqr(depth_delta));
    /* Out of view samples. */
    if (any(lessThan(sample_uv, vec2(0.0))) || any(greaterThan(sample_uv, vec2(1.0)))) {
      s = 0.0;
    }
    /* Mix with first sample in failure case and apply sss_kernel color. */
    accum += sss_kernel[i].rgb * mix(sss_irradiance, color, s);
  }

#if defined(FIRST_PASS)
  sssRadiance = vec4(accum, 1.0);
#else /* SECOND_PASS */
  sssRadiance = vec4(accum * texture(sssAlbedo, uvs).rgb, 1.0);
#endif
}
