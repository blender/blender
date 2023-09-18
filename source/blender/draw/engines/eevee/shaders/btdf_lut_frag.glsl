/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(bsdf_sampling_lib.glsl)

/* Generate BSDF LUT for `IOR < 1`. Returns the integrated BTDF and BRDF, multiplied by the cosine
 * foreshortening factor. */
void main()
{
  /* Make sure coordinates are covering the whole [0..1] range at texel center. */
  float x = floor(gl_FragCoord.x) / (LUT_SIZE - 1.0);
  float y = floor(gl_FragCoord.y) / (LUT_SIZE - 1.0);

  float ior = sqrt(x);
  /* ior is sin of critical angle. */
  float critical_cos = sqrt(1.0 - saturate(ior * ior));

  y = y * 2.0 - 1.0;
  /* Maximize texture usage on both sides of the critical angle. */
  y *= (y > 0.0) ? (1.0 - critical_cos) : critical_cos;
  /* Center LUT around critical angle to avoid strange interpolation issues when the critical
   * angle is changing. */
  y += critical_cos;
  float NV = clamp(y, 1e-4, 0.9999);

  /* Squaring for perceptually linear roughness, see [Physically Based Shading at Disney]
   * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
   * Section 5.4. */
  float roughness = z_factor * z_factor;
  float roughness_sq = roughness * roughness;

  vec3 V = vec3(sqrt(1.0 - NV * NV), 0.0, NV);

  /* Integrating BSDF */
  float btdf = 0.0;
  float brdf = 0.0;
  for (float j = 0.0; j < sampleCount; j++) {
    for (float i = 0.0; i < sampleCount; i++) {
      vec3 Xi = (vec3(i, j, 0.0) + 0.5) / sampleCount;
      Xi.yz = vec2(cos(Xi.y * M_2PI), sin(Xi.y * M_2PI));

      /* Microfacet normal. */
      vec3 H = sample_ggx(Xi, roughness, V);
      float fresnel = F_eta(ior, dot(V, H));

      /* Reflection. */
      vec3 R = -reflect(V, H);
      float NR = R.z;
      if (NR > 0.0) {
        /* Assuming sample visible normals, accumulating `brdf * NV / pdf.` */
        brdf += fresnel * bxdf_ggx_smith_G1(NR, roughness_sq);
      }

      /* Refraction. */
      vec3 T = refract(-V, H, ior);
      float NT = T.z;
      /* In the case of TIR, `T == vec3(0)`. */
      if (NT < 0.0) {
        /* Assuming sample visible normals, accumulating `btdf * NV / pdf.` */
        btdf += (1.0 - fresnel) * bxdf_ggx_smith_G1(NT, roughness_sq);
      }
    }
  }
  btdf /= sampleCount * sampleCount;
  brdf /= sampleCount * sampleCount;

  /* There is place to put multi-scatter result (which is a little bit different still)
   * and / or lobe fitting for better sampling of. */
  FragColor = vec4(btdf, brdf, 0.0, 1.0);
}
