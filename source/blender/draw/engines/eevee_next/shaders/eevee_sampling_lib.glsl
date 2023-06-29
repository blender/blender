
/**
 * Sampling data accessors and random number generators.
 * Also contains some sample mapping functions.
 **/

#pragma BLENDER_REQUIRE(common_math_lib.glsl)
#pragma BLENDER_REQUIRE(common_math_geom_lib.glsl)

/* -------------------------------------------------------------------- */
/** \name Sampling data.
 *
 * Return a random values from Low Discrepancy Sequence in [0..1) range.
 * This value is uniform (constant) for the whole scene sample.
 * You might want to couple it with a noise function.
 * \{ */

#ifdef EEVEE_SAMPLING_DATA

float sampling_rng_1D_get(const eSamplingDimension dimension)
{
  return sampling_buf.dimensions[dimension];
}

vec2 sampling_rng_2D_get(const eSamplingDimension dimension)
{
  return vec2(sampling_buf.dimensions[dimension], sampling_buf.dimensions[dimension + 1u]);
}

vec3 sampling_rng_3D_get(const eSamplingDimension dimension)
{
  return vec3(sampling_buf.dimensions[dimension],
              sampling_buf.dimensions[dimension + 1u],
              sampling_buf.dimensions[dimension + 2u]);
}

#endif

/** \} */

/* -------------------------------------------------------------------- */
/** \name Random Number Generators.
 * \{ */

/* Interleaved gradient noise by Jorge Jimenez
 * http://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare
 * Seeding found by Epic Game. */
float interlieved_gradient_noise(vec2 pixel, float seed, float offset)
{
  pixel += seed * (vec2(47, 17) * 0.695);
  return fract(offset + 52.9829189 * fract(0.06711056 * pixel.x + 0.00583715 * pixel.y));
}

/* From: http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html */
float van_der_corput_radical_inverse(uint bits)
{
#if 0 /* Reference */
  bits = (bits << 16u) | (bits >> 16u);
  bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
  bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
  bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
  bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
#else
  bits = bitfieldReverse(bits);
#endif
  /* Same as dividing by 0x100000000. */
  return float(bits) * 2.3283064365386963e-10;
}

vec2 hammersley_2d(float i, float sample_count)
{
  vec2 rand;
  rand.x = i / sample_count;
  rand.y = van_der_corput_radical_inverse(uint(i));
  return rand;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Distribution mapping.
 *
 * Functions mapping input random numbers to sampling shapes (i.e: hemisphere).
 * \{ */

/* Given 2 random number in [0..1] range, return a random unit disk sample. */
vec2 sample_disk(vec2 noise)
{
  float angle = noise.x * M_2PI;
  return vec2(cos(angle), sin(angle)) * sqrt(noise.y);
}

/* This transform a 2d random sample (in [0..1] range) to a sample located on a cylinder of the
 * same range. This is because the sampling functions expect such a random sample which is
 * normally precomputed. */
vec3 sample_cylinder(vec2 rand)
{
  float theta = rand.x;
  float phi = (rand.y - 0.5) * M_2PI;
  float cos_phi = cos(phi);
  float sin_phi = sqrt(1.0 - sqr(cos_phi)) * sign(phi);
  return vec3(theta, cos_phi, sin_phi);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Microfacet GGX distribution
 * \{ */

#define USE_VISIBLE_NORMAL 1

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

/** \} */
