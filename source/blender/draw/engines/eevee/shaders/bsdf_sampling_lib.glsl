
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)
#pragma BLENDER_REQUIRE(bsdf_common_lib.glsl)

uniform sampler1D texHammersley;

vec3 tangent_to_world(vec3 vector, vec3 N, vec3 T, vec3 B)
{
  return T * vector.x + B * vector.y + N * vector.z;
}

vec3 world_to_tangent(vec3 vector, vec3 N, vec3 T, vec3 B)
{
  return vec3(dot(T, vector), dot(B, vector), dot(N, vector));
}

#ifdef HAMMERSLEY_SIZE
vec3 hammersley_3d(float i, float invsamplenbr)
{
  vec3 Xi; /* Theta, cos(Phi), sin(Phi) */

  Xi.x = i * invsamplenbr;
  Xi.yz = texelFetch(texHammersley, int(i), 0).rg;

  return Xi;
}
#endif

/* -------------- BSDFS -------------- */

#define USE_VISIBLE_NORMAL 1

float pdf_ggx_reflect(float NH, float NV, float VH, float alpha)
{
  float a2 = sqr(alpha);
#if USE_VISIBLE_NORMAL
  float D = a2 / D_ggx_opti(NH, a2);
  float G1 = NV * 2.0 / G1_Smith_GGX_opti(NV, a2);
  return G1 * VH * D / NV;
#else
  return NH * a2 / D_ggx_opti(NH, a2);
#endif
}

float pdf_hemisphere()
{
  return 0.5 * M_1_PI;
}

vec3 sample_ggx(vec3 rand, float alpha, vec3 Vt)
{
#if USE_VISIBLE_NORMAL
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

vec3 sample_ggx(vec3 rand, float alpha, vec3 V, vec3 N, vec3 T, vec3 B, out float pdf)
{
  vec3 Vt = world_to_tangent(V, N, T, B);
  vec3 Ht = sample_ggx(rand, alpha, Vt);
  float NH = saturate(Ht.z);
  float NV = saturate(Vt.z);
  float VH = saturate(dot(Vt, Ht));
  pdf = pdf_ggx_reflect(NH, NV, VH, alpha);
  return tangent_to_world(Ht, N, T, B);
}

vec3 sample_hemisphere(vec3 rand)
{
  /* Theta is the cone angle. */
  float z = rand.x;                      /* cos theta */
  float r = sqrt(max(0.0, 1.0 - z * z)); /* sin theta */
  float x = r * rand.y;
  float y = r * rand.z;

  return vec3(x, y, z);
}

vec3 sample_hemisphere(vec3 rand, vec3 N, vec3 T, vec3 B)
{
  vec3 Ht = sample_hemisphere(rand);
  return tangent_to_world(Ht, N, T, B);
}

#ifdef HAMMERSLEY_SIZE
vec3 sample_ggx(float nsample,
                float inv_sample_count,
                float alpha,
                vec3 V,
                vec3 N,
                vec3 T,
                vec3 B,
                out float pdf)
{
  vec3 Xi = hammersley_3d(nsample, inv_sample_count);
  return sample_ggx(Xi, alpha, V, N, T, B, pdf);
}

vec3 sample_hemisphere(float nsample, float inv_sample_count, vec3 N, vec3 T, vec3 B)
{
  vec3 Xi = hammersley_3d(nsample, inv_sample_count);
  return sample_hemisphere(Xi, N, T, B);
}

vec3 sample_cone(float nsample, float inv_sample_count, float angle, vec3 N, vec3 T, vec3 B)
{
  vec3 Xi = hammersley_3d(nsample, inv_sample_count);

  float z = cos(angle * Xi.x);           /* cos theta */
  float r = sqrt(max(0.0, 1.0 - z * z)); /* sin theta */
  float x = r * Xi.y;
  float y = r * Xi.z;

  vec3 Ht = vec3(x, y, z);

  return tangent_to_world(Ht, N, T, B);
}
#endif
