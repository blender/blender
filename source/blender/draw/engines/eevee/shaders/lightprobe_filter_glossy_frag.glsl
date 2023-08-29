/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(random_lib.glsl)
#pragma BLENDER_REQUIRE(bsdf_sampling_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)

vec3 octahedral_to_cubemap_proj(vec2 co)
{
  co = co * 2.0 - 1.0;

  vec2 abs_co = abs(co);
  vec3 v = vec3(co, 1.0 - (abs_co.x + abs_co.y));

  if (abs_co.x + abs_co.y > 1.0) {
    v.xy = (abs(co.yx) - 1.0) * -sign(co.xy);
  }

  return v;
}

void main()
{
  vec3 N, T, B, V;

  vec3 R = normalize(geom_iface.worldPosition);

  /* Isotropic assumption */
  N = V = R;

  make_orthonormal_basis(N, T, B); /* Generate tangent space */

  /* Integrating environment-map. */
  float weight = 0.0;
  vec3 out_radiance = vec3(0.0);
  for (float i = 0; i < sampleCount; i++) {
    vec3 Xi = rand2d_to_cylinder(hammersley_2d(i, sampleCount));

    float pdf;
    /* Microfacet normal */
    vec3 H = sample_ggx(Xi, probe_roughness, V, N, T, B, pdf);
    vec3 L = -reflect(V, H);
    float NL = dot(N, L);

    if (NL > 0.0) {
      float NH = max(1e-8, dot(N, H)); /* cosTheta */

      /* Coarse Approximation of the mapping distortion
       * Unit Sphere -> Cube-map Face. */
      const float dist = 4.0 * M_PI / 6.0;
      /* http://http.developer.nvidia.com/GPUGems3/gpugems3_ch20.html : Equation 13 */
      float lod = clamp(lodFactor - 0.5 * log2(pdf * dist), 0.0, lodMax);

      vec3 l_col = textureLod(probeHdr, L, lod).rgb;

      /* Clamped brightness. */
      float luma = max(1e-8, max_v3(l_col));
      l_col *= 1.0 - max(0.0, luma - fireflyFactor) / luma;

      out_radiance += l_col * NL;
      weight += NL;
    }
  }

  FragColor = vec4(intensityFac * out_radiance / weight, 1.0);
}
