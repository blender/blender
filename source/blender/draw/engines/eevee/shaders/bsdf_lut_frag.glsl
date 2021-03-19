#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(bsdf_sampling_lib.glsl)

uniform float sampleCount;

out vec2 FragColor;

void main()
{
  /* Make sure coordinates are covering the whole [0..1] range at texel center. */
  float y = floor(gl_FragCoord.y) / (LUT_SIZE - 1);
  float x = floor(gl_FragCoord.x) / (LUT_SIZE - 1);

  float NV = clamp(1.0 - y * y, 1e-4, 0.9999);
  float a = x * x;
  float a2 = clamp(a * a, 1e-4, 0.9999);

  vec3 V = vec3(sqrt(1.0 - NV * NV), 0.0, NV);

  /* Integrating BRDF */
  float brdf_accum = 0.0;
  float fresnel_accum = 0.0;
  for (float j = 0.0; j < sampleCount; j++) {
    for (float i = 0.0; i < sampleCount; i++) {
      vec3 Xi = (vec3(i, j, 0.0) + 0.5) / sampleCount;
      Xi.yz = vec2(cos(Xi.y * M_2PI), sin(Xi.y * M_2PI));

      /* Microfacet normal */
      vec3 H = sample_ggx(Xi, a, V);
      vec3 L = -reflect(V, H);
      float NL = L.z;

      if (NL > 0.0) {
        float NH = max(H.z, 0.0);
        float VH = max(dot(V, H), 0.0);

        float G1_v = G1_Smith_GGX_opti(NV, a2);
        float G1_l = G1_Smith_GGX_opti(NL, a2);
        /* See G1_Smith_GGX_opti for explanations. */
        float G_smith = 4.0 * NV * NL / (G1_v * G1_l);

        float brdf = (G_smith * VH) / (NH * NV);

        /* Follow maximum specular value for principled bsdf. */
        const float specular = 1.0;
        const float eta = (2.0 / (1.0 - sqrt(0.08 * specular))) - 1.0;
        float fresnel = F_eta(eta, VH);
        float Fc = F_color_blend(eta, fresnel, vec3(0)).r;

        brdf_accum += (1.0 - Fc) * brdf;
        fresnel_accum += Fc * brdf;
      }
    }
  }
  brdf_accum /= sampleCount * sampleCount;
  fresnel_accum /= sampleCount * sampleCount;

  FragColor = vec2(brdf_accum, fresnel_accum);
}
