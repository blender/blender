#pragma BLENDER_REQUIRE(bsdf_sampling_lib.glsl)

uniform float a2;

out vec4 FragColor;

void main()
{
  vec3 N, T, B, V;

  float x = gl_FragCoord.x / LUT_SIZE;
  float y = gl_FragCoord.y / LUT_SIZE;
  /* There is little variation if ior > 1.0 so we
   * maximize LUT precision for ior < 1.0 */
  x = x * 1.1;
  float ior = (x > 1.0) ? ior_from_f0((x - 1.0) * 10.0) : sqrt(x);
  float NV = (1.0 - (clamp(y, 1e-4, 0.9999)));

  N = vec3(0.0, 0.0, 1.0);
  T = vec3(1.0, 0.0, 0.0);
  B = vec3(0.0, 1.0, 0.0);
  V = vec3(sqrt(1.0 - NV * NV), 0.0, NV);

  setup_noise();

  /* Integrating BTDF */
  float btdf_accum = 0.0;
  for (float i = 0.0; i < sampleCount; i++) {
    vec3 H = sample_ggx(i, a2, N, T, B); /* Microfacet normal */

    float VH = dot(V, H);

    /* Check if there is total internal reflections. */
    float c = abs(VH);
    float g = ior * ior - 1.0 + c * c;

    float eta = 1.0 / ior;
    if (dot(H, V) < 0.0) {
      H = -H;
      eta = ior;
    }

    vec3 L = refract(-V, H, eta);
    float NL = -dot(N, L);

    if ((NL > 0.0) && (g > 0.0)) {
      float LH = dot(L, H);

      float G1_l = NL * 2.0 /
                   G1_Smith_GGX(NL, a2); /* Balancing the adjustments made in G1_Smith */

      /* btdf = abs(VH*LH) * (ior*ior) * D * G(V) * G(L) / (Ht2 * NV)
       * pdf = (VH * abs(LH)) * (ior*ior) * D * G(V) / (Ht2 * NV) */
      float btdf = G1_l * abs(VH * LH) / (VH * abs(LH));

      btdf_accum += btdf;
    }
  }
  btdf_accum /= sampleCount;

  FragColor = vec4(btdf_accum, 0.0, 0.0, 1.0);
}
