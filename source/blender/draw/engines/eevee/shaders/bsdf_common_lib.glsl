
#pragma BLENDER_REQUIRE(common_math_lib.glsl)

vec3 diffuse_dominant_dir(vec3 bent_normal)
{
  return bent_normal;
}

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

/* Simplified form of F_eta(eta, 1.0). */
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
  if (g > 0.0) {
    g = sqrt(g);
    float A = (g - c) / (g + c);
    float B = (c * (g + c) - 1.0) / (c * (g - c) + 1.0);
    return 0.5 * A * A * (1.0 + B * B);
  }
  /* Total internal reflections. */
  return 1.0;
}

/* Fresnel color blend base on fresnel factor */
vec3 F_color_blend(float eta, float fresnel, vec3 f0_color)
{
  float f0 = f0_from_ior(eta);
  float fac = saturate((fresnel - f0) / (1.0 - f0));
  return mix(f0_color, vec3(1.0), fac);
}

/* Fresnel split-sum approximation. */
vec3 F_brdf_single_scatter(vec3 f0, vec3 f90, vec2 lut)
{
  /* Unreal specular matching : if specular color is below 2% intensity,
   * treat as shadowning */
  return lut.y * f90 + lut.x * f0;
}

/* Multi-scattering brdf approximation from :
 * "A Multiple-Scattering Microfacet Model for Real-Time Image-based Lighting"
 * by Carmelo J. Fdez-Agüera. */
vec3 F_brdf_multi_scatter(vec3 f0, vec3 f90, vec2 lut)
{
  vec3 FssEss = lut.y * f90 + lut.x * f0;

  float Ess = lut.x + lut.y;
  float Ems = 1.0 - Ess;
  vec3 Favg = f0 + (1.0 - f0) / 21.0;
  vec3 Fms = FssEss * Favg / (1.0 - (1.0 - Ess) * Favg);
  /* We don't do anything special for diffuse surfaces because the principle bsdf
   * does not care about energy conservation of the specular layer for dielectrics. */
  return FssEss + Fms * Ems;
}

/* GGX */
float D_ggx_opti(float NH, float a2)
{
  float tmp = (NH * a2 - NH) * NH + 1.0;
  return M_PI * tmp * tmp; /* Doing RCP and mul a2 at the end */
}

float G1_Smith_GGX_opti(float NX, float a2)
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

  float G = G1_Smith_GGX_opti(NV, a2) * G1_Smith_GGX_opti(NL, a2); /* Doing RCP at the end */
  float D = D_ggx_opti(NH, a2);

  /* Denominator is canceled by G1_Smith */
  /* bsdf = D * G / (4.0 * NL * NV); /* Reference function */
  return NL * a2 / (D * G); /* NL to Fit cycles Equation : line. 345 in bsdf_microfacet.h */
}

void accumulate_light(vec3 light, float fac, inout vec4 accum)
{
  accum += vec4(light, 1.0) * min(fac, (1.0 - accum.a));
}

/* Same thing as Cycles without the comments to make it shorter. */
vec3 ensure_valid_reflection(vec3 Ng, vec3 I, vec3 N)
{
  vec3 R = -reflect(I, N);

  /* Reflection rays may always be at least as shallow as the incoming ray. */
  float threshold = min(0.9 * dot(Ng, I), 0.025);
  if (dot(Ng, R) >= threshold) {
    return N;
  }

  float NdotNg = dot(N, Ng);
  vec3 X = normalize(N - NdotNg * Ng);

  float Ix = dot(I, X), Iz = dot(I, Ng);
  float Ix2 = sqr(Ix), Iz2 = sqr(Iz);
  float a = Ix2 + Iz2;

  float b = sqrt(Ix2 * (a - sqr(threshold)));
  float c = Iz * threshold + a;

  float fac = 0.5 / a;
  float N1_z2 = fac * (b + c), N2_z2 = fac * (-b + c);
  bool valid1 = (N1_z2 > 1e-5) && (N1_z2 <= (1.0 + 1e-5));
  bool valid2 = (N2_z2 > 1e-5) && (N2_z2 <= (1.0 + 1e-5));

  vec2 N_new;
  if (valid1 && valid2) {
    /* If both are possible, do the expensive reflection-based check. */
    vec2 N1 = vec2(sqrt(1.0 - N1_z2), sqrt(N1_z2));
    vec2 N2 = vec2(sqrt(1.0 - N2_z2), sqrt(N2_z2));

    float R1 = 2.0 * (N1.x * Ix + N1.y * Iz) * N1.y - Iz;
    float R2 = 2.0 * (N2.x * Ix + N2.y * Iz) * N2.y - Iz;

    valid1 = (R1 >= 1e-5);
    valid2 = (R2 >= 1e-5);
    if (valid1 && valid2) {
      N_new = (R1 < R2) ? N1 : N2;
    }
    else {
      N_new = (R1 > R2) ? N1 : N2;
    }
  }
  else if (valid1 || valid2) {
    float Nz2 = valid1 ? N1_z2 : N2_z2;
    N_new = vec2(sqrt(1.0 - Nz2), sqrt(Nz2));
  }
  else {
    return Ng;
  }
  return N_new.x * X + N_new.y * Ng;
}

/* ----------- Cone angle Approximation --------- */

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
