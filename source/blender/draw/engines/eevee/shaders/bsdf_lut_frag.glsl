/* SPDX-FileCopyrightText: 2017-2022 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(bsdf_sampling_lib.glsl)

/* Generate BRDF LUT following "Real shading in unreal engine 4" by Brian Karis
 * https://cdn2.unrealengine.com/Resources/files/2013SiggraphPresentationsNotes-26915738.pdf
 * Parametrizing with `x = roughness` and `y = sqrt(1.0 - cos(theta))`.
 * The result is interpreted as: `integral = f0 * scale + f90 * bias`. */
void main()
{
  /* Make sure coordinates are covering the whole [0..1] range at texel center. */
  float x = floor(gl_FragCoord.x) / (LUT_SIZE - 1);
  float y = floor(gl_FragCoord.y) / (LUT_SIZE - 1);

  /* Squaring for perceptually linear roughness, see [Physically Based Shading at Disney]
   * (https://media.disneyanimation.com/uploads/production/publication_asset/48/asset/s2012_pbs_disney_brdf_notes_v3.pdf)
   * Section 5.4. */
  float roughness = x * x;
  float roughness_sq = roughness * roughness;

  float NV = clamp(1.0 - y * y, 1e-4, 0.9999);
  vec3 V = vec3(sqrt(1.0 - NV * NV), 0.0, NV);

  /* Integrating BRDF */
  float scale = 0.0;
  float bias = 0.0;
  for (float j = 0.0; j < sampleCount; j++) {
    for (float i = 0.0; i < sampleCount; i++) {
      vec3 Xi = (vec3(i, j, 0.0) + 0.5) / sampleCount;
      Xi.yz = vec2(cos(Xi.y * M_2PI), sin(Xi.y * M_2PI));

      /* Microfacet normal */
      vec3 H = sample_ggx(Xi, roughness, V);
      vec3 L = -reflect(V, H);
      float NL = L.z;

      if (NL > 0.0) {
        /* Assuming sample visible normals, `weight = brdf * NV / (pdf * fresnel).` */
        float weight = bxdf_ggx_smith_G1(NL, roughness_sq);

        /* Schlick's Fresnel. */
        float s = pow(1.0 - saturate(dot(V, H)), 5.0);

        scale += (1.0 - s) * weight;
        bias += s * weight;
      }
    }
  }
  scale /= sampleCount * sampleCount;
  bias /= sampleCount * sampleCount;

  FragColor = vec2(scale, bias);
}
