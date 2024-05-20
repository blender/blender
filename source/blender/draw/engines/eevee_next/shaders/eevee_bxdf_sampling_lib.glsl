/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/**
 * Sampling of Normal Distribution Function for various BxDF.
 */

#pragma BLENDER_REQUIRE(eevee_bxdf_lib.glsl)
#pragma BLENDER_REQUIRE(draw_math_geom_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name Microfacet GGX distribution
 * \{ */

float sample_pdf_ggx_refract_ex(
    float NH, float NV, float VH, float LH, float G1_V, float alpha, float eta)
{
  float a2 = square(alpha);
  float D = bxdf_ggx_D(NH, a2);
  float Ht2 = square(eta * LH + VH);
  return (D * G1_V * abs(VH * LH) * square(eta)) / (NV * Ht2);
}

/* All inputs must be in tangent space. */
float sample_pdf_ggx_refract(vec3 Vt, vec3 Lt, float alpha, float ior, bool is_second_event)
{
  /* Inverse of `refract(-V, H, 1.0 / ior)` with `Lt * ior + Vt` equivalent to `Lt + Vt / ior`. */
  vec3 Ht = normalize(-(Lt * ior + Vt));
  /* FIXME(fclem): Why is this necessary? */
  Ht = is_second_event ? -Ht : Ht;
  float NH = Ht.z;
  float NV = Vt.z;
  float VH = dot(Vt, Ht);
  float LH = dot(Lt, Ht);

  if (VH > 0.0) {
    vec3 Vh = normalize(vec3(alpha * Vt.xy, Vt.z));
    float G1_V = 2.0 * Vh.z / (1.0 + Vh.z);
    return sample_pdf_ggx_refract_ex(NH, NV, VH, LH, G1_V, alpha, ior);
  }
  return 0.0;
}

/**
 * Returns a tangent space microfacet normal following the GGX distribution.
 *
 * \param rand: random point on the unit cylinder (result of sample_cylinder).
 * \param alpha: roughness parameter.
 * \param Vt: tangent space view vector.
 * \param G1_V: output G1_V factor to be reused.
 */
vec3 sample_ggx(vec3 rand, float alpha, vec3 Vt, out float G1_V)
{
  /* Sampling Visible GGX Normals with Spherical Caps.
   * Jonathan Dupuy and Anis Benyoub, HPG Vol. 42, No. 8, 2023.
   * https://diglib.eg.org/bitstream/handle/10.1111/cgf14867/v42i8_03_14867.pdf
   * View vector is expected to be in tangent space. */

  /* Transforming the view direction to the hemisphere configuration. */
  vec3 Vh = normalize(vec3(alpha * Vt.xy, Vt.z));

  /* Visibility term. */
  G1_V = 2.0 * Vh.z / (1.0 + Vh.z);

  /* Sample a spherical cap in (-Vh.z, 1]. */
  float cos_theta = mix(-Vh.z, 1.0, 1.0 - rand.x);
  float sin_theta = sqrt(saturate(1.0 - square(cos_theta)));
  vec3 Lh = vec3(sin_theta * rand.yz, cos_theta);

  /* Compute unnormalized halfway direction. */
  vec3 Hh = Vh + Lh;

  /* Transforming the normal back to the ellipsoid configuration. */
  return normalize(vec3(alpha * Hh.xy, max(0.0, Hh.z)));
}

vec3 sample_ggx(vec3 rand, float alpha, vec3 Vt)
{
  float G1_unused;
  return sample_ggx(rand, alpha, Vt, G1_unused);
}

/* All inputs must be in tangent space. */
float sample_pdf_ggx_bounded(vec3 Vt, vec3 Lt, float alpha)
{
  /**
   * Eto, Kenta, and Yusuke Tokuyoshi. "Bounded VNDF Sampling for Smith-GGX Reflections."
   * SIGGRAPH Asia 2023 Technical Communications. 2023. 1-4.
   * https://gpuopen.com/download/publications/Bounded_VNDF_Sampling_for_Smith-GGX_Reflections.pdf
   * Listing 2.
   */
  vec3 Ht = normalize(Vt + Lt);
  float a2 = square(alpha);
  float s2 = square(1.0 + length(Vt.xy));
  float D = bxdf_ggx_D(Ht.z, a2);
  float len_ai_sqr = length_squared(alpha * Vt.xy);
  float t = sqrt(len_ai_sqr + square(Vt.z));
  if (Vt.z >= 0.0) {
    float k = (1.0 - a2) * s2 / (s2 + a2 * square(Vt.z));
    return D / (2.0 * (k * Vt.z + t));
  }
  return D * (t - Vt.z) / (2.0 * len_ai_sqr);
}

/* Similar as `sample_ggx()`, but reduces the number or rejected samples due to reflection in the
 * lower hemisphere, and returns `pdf` instead of `G1_V`. Only used for reflection.
 *
 * Sampling visible GGX normals with bounded spherical caps.
 * Eto, Kenta, and Yusuke Tokuyoshi. "Bounded VNDF Sampling for Smith-GGX Reflections."
 * SIGGRAPH Asia 2023 Technical Communications. 2023. 1-4.
 * https://gpuopen.com/download/publications/Bounded_VNDF_Sampling_for_Smith-GGX_Reflections.pdf */
vec3 sample_ggx_bounded(vec3 rand, float alpha, vec3 Vt, out float pdf)
{
  /* Transforming the view direction to the hemisphere configuration. */
  vec3 Vh = vec3(alpha * Vt.xy, Vt.z);
  float norm = length(Vh);
  Vh = Vh / norm;

  /* Compute the bounded cap. */
  float a2 = square(alpha);
  float s2 = square(1.0 + length(Vt.xy));
  float k = (1.0 - a2) * s2 / (s2 + a2 * square(Vt.z));

  /* Sample a spherical cap in (-Vh.z * k, 1]. */
  float cos_theta = mix(-Vh.z * k, 1.0, 1.0 - rand.x);
  float sin_theta = sqrt(saturate(1.0 - square(cos_theta)));
  vec3 Lh = vec3(sin_theta * rand.yz, cos_theta);

  /* Compute unnormalized halfway direction. */
  vec3 Hh = Vh + Lh;

  /* Transforming the normal back to the ellipsoid configuration. */
  vec3 Ht = normalize(vec3(alpha * Hh.xy, max(0.0, Hh.z)));

  pdf = bxdf_ggx_D(saturate(Ht.z), a2) / (2.0 * (k * Vt.z + norm));

  return Ht;
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
 *
 * \return pdf: the pdf of sampling the reflected ray. 0 if ray is invalid.
 */
vec3 sample_ggx_reflect(vec3 rand, float alpha, vec3 V, vec3 N, vec3 T, vec3 B, out float pdf)
{
  vec3 Vt = world_to_tangent(V, N, T, B);
  vec3 Ht = sample_ggx_bounded(rand, alpha, Vt, pdf);
  vec3 H = tangent_to_world(Ht, N, T, B);

  if (dot(V, H) > 0.0) {
    vec3 L = reflect(-V, H);
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
 *
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
    pdf = sample_pdf_ggx_refract_ex(NH, NV, VH, LH, G1_V, alpha, ior);
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
  return mat3(T, B, N) * tH;
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
  return mat3(T, B, N) * tH;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Cosine Hemisphere
 * \{ */

float sample_pdf_uniform_sphere()
{
  return 1.0 / (4.0 * M_PI);
}

vec3 sample_uniform_sphere(vec3 rand)
{
  float cos_theta = rand.x * 2.0 - 1.0;
  float sin_theta = safe_sqrt(1.0 - cos_theta * cos_theta);
  return vec3(sin_theta * rand.yz, cos_theta);
}

vec3 sample_uniform_sphere(vec3 rand, vec3 N, vec3 T, vec3 B, out float pdf)
{
  pdf = sample_pdf_uniform_sphere();
  return mat3(T, B, N) * sample_uniform_sphere(rand);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Uniform Cone sampling
 * \{ */

vec3 sample_uniform_cone(vec3 rand, float angle)
{
  float z = mix(cos(angle), 1.0, rand.x); /* cos theta */
  float r = sqrt(max(0.0, 1.0 - z * z));  /* sin theta */
  float x = r * rand.y;
  float y = r * rand.z;
  return vec3(x, y, z);
}

vec3 sample_uniform_cone(vec3 rand, float angle, vec3 N, vec3 T, vec3 B)
{
  vec3 tH = sample_uniform_cone(rand, angle);
  /* TODO: pdf? */
  return mat3(T, B, N) * tH;
}

/** \} */
