
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

vec3 specular_dominant_dir(vec3 N, vec3 V, float roughness)
{
  vec3 R = -reflect(V, N);
  float smoothness = 1.0 - roughness;
  float fac = smoothness * (sqrt(smoothness) + roughness);
  return normalize(mix(N, R, fac));
}

float ior_from_f0(float f0)
{
  float f = sqrt(f0);
  return (-f - 1.0) / (f - 1.0);
}

float f0_from_ior(float eta)
{
  float A = (eta - 1.0) / (eta + 1.0);
  return A * A;
}

vec3 refraction_dominant_dir(vec3 N, vec3 V, float roughness, float ior)
{
  /* TODO: This a bad approximation. Better approximation should fit
   * the refracted vector and roughness into the best prefiltered reflection
   * lobe. */
  /* Correct the IOR for ior < 1.0 to not see the abrupt delimitation or the TIR */
  ior = (ior < 1.0) ? mix(ior, 1.0, roughness) : ior;
  float eta = 1.0 / ior;

  float NV = dot(N, -V);

  /* Custom Refraction. */
  float k = 1.0 - eta * eta * (1.0 - NV * NV);
  k = max(0.0, k); /* Only this changes. */
  vec3 R = eta * -V - (eta * NV + sqrt(k)) * N;

  return R;
}

/* Fresnel monochromatic, perfect mirror */
float F_eta(float eta, float cos_theta)
{
  /* compute fresnel reflectance without explicitly computing
   * the refracted direction */
  float c = abs(cos_theta);
  float g = eta * eta - 1.0 + c * c;
  float result;

  if (g > 0.0) {
    g = sqrt(g);
    vec2 g_c = vec2(g) + vec2(c, -c);
    float A = g_c.y / g_c.x;
    A *= A;
    g_c *= c;
    float B = (g_c.y - 1.0) / (g_c.x + 1.0);
    B *= B;
    result = 0.5 * A * (1.0 + B);
  }
  else {
    result = 1.0; /* TIR (no refracted component) */
  }

  return result;
}

/* Fresnel color blend base on fresnel factor */
vec3 F_color_blend(float eta, float fresnel, vec3 f0_color)
{
  float f0 = F_eta(eta, 1.0);
  float fac = saturate((fresnel - f0) / max(1e-8, 1.0 - f0));
  return mix(f0_color, vec3(1.0), fac);
}

/* Fresnel */
vec3 F_schlick(vec3 f0, float cos_theta)
{
  float fac = 1.0 - cos_theta;
  float fac2 = fac * fac;
  fac = fac2 * fac2 * fac;

  /* Unreal specular matching : if specular color is below 2% intensity,
   * (using green channel for intensity) treat as shadowning */
  return saturate(50.0 * dot(f0, vec3(0.3, 0.6, 0.1))) * fac + (1.0 - fac) * f0;
}

/* Fresnel approximation for LTC area lights (not MRP) */
vec3 F_area(vec3 f0, vec3 f90, vec2 lut)
{
  /* Unreal specular matching : if specular color is below 2% intensity,
   * treat as shadowning */
  return saturate(50.0 * dot(f0, vec3(0.3, 0.6, 0.1))) * lut.y * f90 + lut.x * f0;
}

/* Fresnel approximation for IBL */
vec3 F_ibl(vec3 f0, vec3 f90, vec2 lut)
{
  /* Unreal specular matching : if specular color is below 2% intensity,
   * treat as shadowning */
  return saturate(50.0 * dot(f0, vec3(0.3, 0.6, 0.1))) * lut.y * f90 + lut.x * f0;
}

/* GGX */
float D_ggx_opti(float NH, float a2)
{
  float tmp = (NH * a2 - NH) * NH + 1.0;
  return M_PI * tmp * tmp; /* Doing RCP and mul a2 at the end */
}

float G1_Smith_GGX(float NX, float a2)
{
  /* Using Brian Karis approach and refactoring by NX/NX
   * this way the (2*NL)*(2*NV) in G = G1(V) * G1(L) gets canceled by the brdf denominator 4*NL*NV
   * Rcp is done on the whole G later
   * Note that this is not convenient for the transmission formula */
  return NX + sqrt(NX * (NX - NX * a2) + a2);
  /* return 2 / (1 + sqrt(1 + a2 * (1 - NX*NX) / (NX*NX) ) ); /* Reference function */
}

float bsdf_ggx(vec3 N, vec3 L, vec3 V, float roughness)
{
  float a = roughness;
  float a2 = a * a;

  vec3 H = normalize(L + V);
  float NH = max(dot(N, H), 1e-8);
  float NL = max(dot(N, L), 1e-8);
  float NV = max(dot(N, V), 1e-8);

  float G = G1_Smith_GGX(NV, a2) * G1_Smith_GGX(NL, a2); /* Doing RCP at the end */
  float D = D_ggx_opti(NH, a2);

  /* Denominator is canceled by G1_Smith */
  /* bsdf = D * G / (4.0 * NL * NV); /* Reference function */
  return NL * a2 / (D * G); /* NL to Fit cycles Equation : line. 345 in bsdf_microfacet.h */
}

void accumulate_light(vec3 light, float fac, inout vec4 accum)
{
  accum += vec4(light, 1.0) * min(fac, (1.0 - accum.a));
}

/* ----------- Cone Aperture Approximation --------- */

/* Return a fitted cone angle given the input roughness */
float cone_cosine(float r)
{
  /* Using phong gloss
   * roughness = sqrt(2/(gloss+2)) */
  float gloss = -2 + 2 / (r * r);
  /* Drobot 2014 in GPUPro5 */
  // return cos(2.0 * sqrt(2.0 / (gloss + 2)));
  /* Uludag 2014 in GPUPro5 */
  // return pow(0.244, 1 / (gloss + 1));
  /* Jimenez 2016 in Practical Realtime Strategies for Accurate Indirect Occlusion*/
  return exp2(-3.32193 * r * r);
}
