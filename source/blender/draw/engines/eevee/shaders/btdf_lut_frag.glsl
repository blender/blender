#pragma BLENDER_REQUIRE(common_utiltex_lib.glsl)
#pragma BLENDER_REQUIRE(bsdf_sampling_lib.glsl)

uniform float sampleCount;
uniform float z;

out vec4 FragColor;

void main()
{
  float x = floor(gl_FragCoord.x) / (LUT_SIZE - 1.0);
  float y = floor(gl_FragCoord.y) / (LUT_SIZE - 1.0);

  float ior = clamp(sqrt(x), 0.05, 0.999);
  /* ior is sin of critical angle. */
  float critical_cos = sqrt(1.0 - saturate(ior * ior));

  y = y * 2.0 - 1.0;
  /* Maximize texture usage on both sides of the critical angle. */
  y *= (y > 0.0) ? (1.0 - critical_cos) : critical_cos;
  /* Center LUT around critical angle to avoid strange interpolation issues when the critical
   * angle is changing. */
  y += critical_cos;
  float NV = clamp(y, 1e-4, 0.9999);

  float a = z * z;
  float a2 = clamp(a * a, 1e-8, 0.9999);

  vec3 V = vec3(sqrt(1.0 - NV * NV), 0.0, NV);

  /* Integrating BTDF */
  float btdf_accum = 0.0;
  float fresnel_accum = 0.0;
  for (float j = 0.0; j < sampleCount; j++) {
    for (float i = 0.0; i < sampleCount; i++) {
      vec3 Xi = (vec3(i, j, 0.0) + 0.5) / sampleCount;
      Xi.yz = vec2(cos(Xi.y * M_2PI), sin(Xi.y * M_2PI));

      /* Microfacet normal. */
      vec3 H = sample_ggx(Xi, a2, V);

      float VH = dot(V, H);

      /* Check if there is total internal reflections. */
      float fresnel = F_eta(ior, VH);

      fresnel_accum += fresnel;

      float eta = 1.0 / ior;
      if (dot(H, V) < 0.0) {
        H = -H;
        eta = ior;
      }

      vec3 L = refract(-V, H, eta);
      float NL = -L.z;

      if ((NL > 0.0) && (fresnel < 0.999)) {
        float LH = dot(L, H);

        /* Balancing the adjustments made in G1_Smith. */
        float G1_l = NL * 2.0 / G1_Smith_GGX_opti(NL, a2);

        /* btdf = abs(VH*LH) * (ior*ior) * D * G(V) * G(L) / (Ht2 * NV)
         * pdf = (VH * abs(LH)) * (ior*ior) * D * G(V) / (Ht2 * NV) */
        float btdf = G1_l * abs(VH * LH) / (VH * abs(LH));

        btdf_accum += btdf;
      }
    }
  }
  btdf_accum /= sampleCount * sampleCount;
  fresnel_accum /= sampleCount * sampleCount;

  if (z == 0.0) {
    /* Perfect mirror. Increased precision because the roughness is clamped. */
    fresnel_accum = F_eta(ior, NV);
  }

  if (x == 0.0) {
    /* Special case. */
    fresnel_accum = 1.0;
    btdf_accum = 0.0;
  }

  /* There is place to put multiscater result (which is a little bit different still)
   * and / or lobe fitting for better sampling of  */
  FragColor = vec4(btdf_accum, fresnel_accum, 0.0, 1.0);
}
