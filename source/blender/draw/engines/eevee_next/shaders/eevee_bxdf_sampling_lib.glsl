/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Sampling of Normal Distribution Function for various BxDF.
 */

#pragma BLENDER_REQUIRE(eevee_bxdf_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name Microfacet GGX distribution
 * \{ */

#define GGX_USE_VISIBLE_NORMAL 1

float sample_pdf_ggx_reflect(float NH, float NV, float VH, float G1_V, float alpha)
{
  float a2 = sqr(alpha);
#if GGX_USE_VISIBLE_NORMAL
  return 0.25 * bxdf_ggx_D(NH, a2) * G1_V / NV;
#else
  return NH * bxdf_ggx_D(NH, a2);
#endif
}

float sample_pdf_ggx_refract(
    float NH, float NV, float VH, float LH, float G1_V, float alpha, float eta)
{
  float a2 = sqr(alpha);
  float D = bxdf_ggx_D(NH, a2);
  float Ht2 = sqr(eta * LH + VH);
  return (D * G1_V * abs(VH * LH) * sqr(eta)) / (NV * Ht2);
}

/**
 * Returns a tangent space microfacet normal following the GGX distribution.
 *
 * \param rand: random point on the unit cylinder (result of sample_cylinder).
 * \param alpha: roughness parameter.
 * \param tV: tangent space view vector.
 * \param G1_V: output G1_V factor to be reused.
 */
vec3 sample_ggx(vec3 rand, float alpha, vec3 Vt, out float G1_V)
{
#if GGX_USE_VISIBLE_NORMAL
  /* From:
   * "A Simpler and Exact Sampling Routine for the GGXDistribution of Visible Normals"
   * by Eric Heitz.
   * http://jcgt.org/published/0007/04/01/slides.pdf
   * View vector is expected to be in tangent space. */

  /* Stretch view. */
  vec3 Th, Bh, Vh = normalize(vec3(alpha * Vt.xy, Vt.z));
  make_orthonormal_basis(Vh, Th, Bh);
  /* Sample point with polar coordinates (r, phi). */
  float r = sqrt(rand.x);
  float x = r * rand.y;
  float y = r * rand.z;
  float s = 0.5 * (1.0 + Vh.z);
  G1_V = Vh.z / s;
  y = (1.0 - s) * sqrt(1.0 - x * x) + s * y;
  float z = sqrt(saturate(1.0 - x * x - y * y));
  /* Compute normal. */
  vec3 Hh = x * Th + y * Bh + z * Vh;
  /* Unstretch. */
  vec3 Ht = normalize(vec3(alpha * Hh.xy, saturate(Hh.z)));
  /* Microfacet Normal. */
  return Ht;
#else
  /* Theta is the cone angle. */
  float z = sqrt((1.0 - rand.x) / (1.0 + sqr(alpha) * rand.x - rand.x)); /* cos theta */
  float r = sqrt(max(0.0, 1.0 - z * z));                                 /* sin theta */
  float x = r * rand.y;
  float y = r * rand.z;
  /* Microfacet Normal */
  return vec3(x, y, z);
#endif
}

vec3 sample_ggx(vec3 rand, float alpha, vec3 Vt)
{
  float G1_unused;
  return sample_ggx(rand, alpha, Vt, G1_unused);
}

/**
 * Returns a reflected ray direction following the GGX distribution.
 *
 * \param rand: random point on the unit cylinder (result of sample_cylinder).
 * \param alpha: roughness parameter.
 * \param V: View vector.
 * \param N: Normal vector.
 * \param T: Tangent vector.
 * \param B: Bitangent vector.

 * \return pdf: the pdf of sampling the reflected ray. 0 if ray is invalid.
 */
vec3 sample_ggx_reflect(vec3 rand, float alpha, vec3 V, vec3 N, vec3 T, vec3 B, out float pdf)
{
  float G1_V;
  vec3 tV = world_to_tangent(V, N, T, B);
  vec3 tH = sample_ggx(rand, alpha, tV, G1_V);
  float NH = saturate(tH.z);
  float NV = saturate(tV.z);
  float VH = dot(tV, tH);
  vec3 H = tangent_to_world(tH, N, T, B);

  if (VH > 0.0) {
    vec3 L = reflect(-V, H);
    pdf = sample_pdf_ggx_reflect(NH, NV, VH, G1_V, alpha);
    return L;
  }
  pdf = 0.0;
  return vec3(1.0, 0.0, 0.0);
}

/**
 * Returns a refracted ray direction following the GGX distribution.
 *
 * \param rand: random point on the unit cylinder (result of sample_cylinder).
 * \param alpha: roughness parameter.
 * \param V: View vector.
 * \param N: Normal vector.
 * \param T: Tangent vector.
 * \param B: Bitangent vector.

 * \return pdf: the pdf of sampling the refracted ray. 0 if ray is invalid.
 */
vec3 sample_ggx_refract(
    vec3 rand, float alpha, float ior, vec3 V, vec3 N, vec3 T, vec3 B, out float pdf)
{
  float G1_V;
  vec3 Vt = world_to_tangent(V, N, T, B);
  vec3 Ht = sample_ggx(rand, alpha, Vt, G1_V);
  float NH = saturate(Ht.z);
  float NV = saturate(Vt.z);
  float VH = dot(Vt, Ht);
  vec3 H = tangent_to_world(Ht, N, T, B);

  if (VH > 0.0) {
    vec3 L = refract(-V, H, 1.0 / ior);
    float LH = dot(L, H);
    pdf = sample_pdf_ggx_refract(NH, NV, VH, LH, G1_V, alpha, ior);
    return L;
  }
  pdf = 0.0;
  return vec3(1.0, 0.0, 0.0);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform Hemisphere
 * \{ */

float sample_pdf_uniform_hemisphere()
{
  return 0.5 * M_1_PI;
}

vec3 sample_uniform_hemisphere(vec3 rand)
{
  float z = rand.x;                      /* cos theta */
  float r = sqrt(max(0.0, 1.0 - z * z)); /* sin theta */
  float x = r * rand.y;
  float y = r * rand.z;
  return vec3(x, y, z);
}

vec3 sample_uniform_hemisphere(vec3 rand, vec3 N, vec3 T, vec3 B, out float pdf)
{
  vec3 tH = sample_uniform_hemisphere(rand);
  pdf = sample_pdf_uniform_hemisphere();
  return tH * mat3(N, T, B);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cosine Hemisphere
 * \{ */

float sample_pdf_cosine_hemisphere(float cos_theta)
{
  return cos_theta * M_1_PI;
}

vec3 sample_cosine_hemisphere(vec3 rand)
{
  float z = sqrt(max(1e-16, rand.x));    /* cos theta */
  float r = sqrt(max(0.0, 1.0 - z * z)); /* sin theta */
  float x = r * rand.y;
  float y = r * rand.z;
  return vec3(x, y, z);
}

vec3 sample_cosine_hemisphere(vec3 rand, vec3 N, vec3 T, vec3 B, out float pdf)
{
  vec3 tH = sample_cosine_hemisphere(rand);
  pdf = sample_pdf_cosine_hemisphere(tH.z);
  return tH * mat3(N, T, B);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform Cone sampling
 * \{ */

vec3 sample_uniform_cone(vec3 rand, float angle)
{
  float z = cos(angle * rand.x);         /* cos theta */
  float r = sqrt(max(0.0, 1.0 - z * z)); /* sin theta */
  float x = r * rand.y;
  float y = r * rand.z;
  return vec3(x, y, z);
}

vec3 sample_uniform_cone(vec3 rand, float angle, vec3 N, vec3 T, vec3 B)
{
  vec3 tH = sample_uniform_cone(rand, angle);
  /* TODO: pdf? */
  return tH * mat3(N, T, B);
}

/** \} */
