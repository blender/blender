/* SPDX-FileCopyrightText: 2017-2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

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
  return f0 * lut.x + f90 * lut.y;
}

/* Multi-scattering brdf approximation from
 * "A Multiple-Scattering Microfacet Model for Real-Time Image-based Lighting"
 * https://jcgt.org/published/0008/01/03/paper.pdf by Carmelo J. Fdez-Agüera. */
vec3 F_brdf_multi_scatter(vec3 f0, vec3 f90, vec2 lut)
{
  vec3 FssEss = F_brdf_single_scatter(f0, f90, lut);

  float Ess = lut.x + lut.y;
  float Ems = 1.0 - Ess;
  vec3 Favg = f0 + (1.0 - f0) / 21.0;

  /* The original paper uses `FssEss * radiance + Fms*Ems * irradiance`, but
   * "A Journey Through Implementing Multiscattering BRDFs and Area Lights" by Steve McAuley
   * suggests to use `FssEss * radiance + Fms*Ems * radiance` which results in comparible quality.
   * We handle `radiance` outside of this function, so the result simplifies to:
   * `FssEss + Fms*Ems = FssEss * (1 + Ems*Favg / (1 - Ems*Favg)) = FssEss / (1 - Ems*Favg)`.
   * This is a simple albedo scaling very similar to the approach used by Cycles:
   * "Practical multiple scattering compensation for microfacet model". */
  return FssEss / (1.0 - Ems * Favg);
}

/* GGX */
float bxdf_ggx_D(float NH, float a2)
{
  return a2 / (M_PI * sqr((a2 - 1.0) * (NH * NH) + 1.0));
}

float D_ggx_opti(float NH, float a2)
{
  float tmp = (NH * a2 - NH) * NH + 1.0;
  return M_PI * tmp * tmp; /* Doing RCP and multiply a2 at the end. */
}

float bxdf_ggx_smith_G1(float NX, float a2)
{
  return 2.0 / (1.0 + sqrt(1.0 + a2 * (1.0 / (NX * NX) - 1.0)));
}

float G1_Smith_GGX_opti(float NX, float a2)
{
  /* Using Brian Karis approach and refactoring by NX/NX
   * this way the (2*NL)*(2*NV) in G = G1(V) * G1(L) gets canceled by the brdf denominator 4*NL*NV
   * RCP is done on the whole G later
   * Note that this is not convenient for the transmission formula */
  return NX + sqrt(NX * (NX - NX * a2) + a2);
  // return 2 / (1 + sqrt(1 + a2 * (1 - NX*NX) / (NX*NX) ) ); /* Reference function. */
}

/* Compute the GGX BRDF without the Fresnel term, multiplied by the cosine foreshortening term. */
float bsdf_ggx(vec3 N, vec3 L, vec3 V, float roughness)
{
  float a = roughness;
  float a2 = a * a;

  vec3 H = normalize(L + V);
  float NH = max(dot(N, H), 1e-8);
  float NL = max(dot(N, L), 1e-8);
  float NV = max(dot(N, V), 1e-8);

  float G = bxdf_ggx_smith_G1(NV, a2) * bxdf_ggx_smith_G1(NL, a2);
  float D = bxdf_ggx_D(NH, a2);

  /* brdf * NL =  `((D * G) / (4 * NV * NL)) * NL`. */
  return (0.25 * D * G) / NV;
}

void accumulate_light(vec3 light, float fac, inout vec4 accum)
{
  accum += vec4(light, 1.0) * min(fac, (1.0 - accum.a));
}

/* Same thing as Cycles without the comments to make it shorter. */
vec3 ensure_valid_specular_reflection(vec3 Ng, vec3 I, vec3 N)
{
  vec3 R = -reflect(I, N);

  float Iz = dot(I, Ng);

  /* Reflection rays may always be at least as shallow as the incoming ray. */
  float threshold = min(0.9 * Iz, 0.025);
  if (dot(Ng, R) >= threshold) {
    return N;
  }

  vec3 X = normalize(N - dot(N, Ng) * Ng);
  float Ix = dot(I, X);

  float a = sqr(Ix) + sqr(Iz);
  float b = 2.0 * (a + Iz * threshold);
  float c = sqr(threshold + Iz);

  float Nz2 = (Ix < 0.0) ? 0.25 * (b + safe_sqrt(sqr(b) - 4.0 * a * c)) / a :
                           0.25 * (b - safe_sqrt(sqr(b) - 4.0 * a * c)) / a;

  float Nx = safe_sqrt(1.0 - Nz2);
  float Nz = safe_sqrt(Nz2);

  return Nx * X + Nz * Ng;
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
  /* Jimenez 2016 in Practical Realtime Strategies for Accurate Indirect Occlusion. */
  return exp2(-3.32193 * r * r);
}
