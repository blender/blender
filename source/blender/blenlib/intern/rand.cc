/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup bli
 */

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_rand.hh"
#include "BLI_threads.h"

/* defines BLI_INLINE */
#include "BLI_compiler_compat.h"

#include "BLI_strict_flags.h"
#include "BLI_sys_types.h"

extern "C" unsigned char BLI_noise_hash_uchar_512[512]; /* noise.c */
#define hash BLI_noise_hash_uchar_512

/**
 * Random Number Generator.
 */
struct RNG {
  blender::RandomNumberGenerator rng;

  MEM_CXX_CLASS_ALLOC_FUNCS("RNG")
};

RNG *BLI_rng_new(unsigned int seed)
{
  RNG *rng = new RNG();
  rng->rng.seed(seed);
  return rng;
}

/**
 * A version of #BLI_rng_new that hashes the seed.
 */
RNG *BLI_rng_new_srandom(unsigned int seed)
{
  RNG *rng = new RNG();
  rng->rng.seed_random(seed);
  return rng;
}

RNG *BLI_rng_copy(RNG *rng)
{
  return new RNG(*rng);
}

void BLI_rng_free(RNG *rng)
{
  delete rng;
}

void BLI_rng_seed(RNG *rng, unsigned int seed)
{
  rng->rng.seed(seed);
}

/**
 * Use a hash table to create better seed.
 */
void BLI_rng_srandom(RNG *rng, unsigned int seed)
{
  rng->rng.seed_random(seed);
}

void BLI_rng_get_char_n(RNG *rng, char *bytes, size_t bytes_len)
{
  rng->rng.get_bytes(blender::MutableSpan(bytes, static_cast<int64_t>(bytes_len)));
}

int BLI_rng_get_int(RNG *rng)
{
  return rng->rng.get_int32();
}

unsigned int BLI_rng_get_uint(RNG *rng)
{
  return rng->rng.get_uint32();
}

/**
 * \return Random value (0..1), but never 1.0.
 */
double BLI_rng_get_double(RNG *rng)
{
  return rng->rng.get_double();
}

/**
 * \return Random value (0..1), but never 1.0.
 */
float BLI_rng_get_float(RNG *rng)
{
  return rng->rng.get_float();
}

void BLI_rng_get_float_unit_v2(RNG *rng, float v[2])
{
  copy_v2_v2(v, rng->rng.get_unit_float2());
}

void BLI_rng_get_float_unit_v3(RNG *rng, float v[3])
{
  copy_v3_v3(v, rng->rng.get_unit_float3());
}

/**
 * Generate a random point inside given tri.
 */
void BLI_rng_get_tri_sample_float_v2(
    RNG *rng, const float v1[2], const float v2[2], const float v3[2], float r_pt[2])
{
  copy_v2_v2(r_pt, rng->rng.get_triangle_sample(v1, v2, v3));
}

void BLI_rng_get_tri_sample_float_v3(
    RNG *rng, const float v1[3], const float v2[3], const float v3[3], float r_pt[3])
{
  copy_v3_v3(r_pt, rng->rng.get_triangle_sample_3d(v1, v2, v3));
}

void BLI_rng_shuffle_array(RNG *rng, void *data, unsigned int elem_size_i, unsigned int elem_tot)
{
  const uint elem_size = elem_size_i;
  unsigned int i = elem_tot;
  void *temp;

  if (elem_tot <= 1) {
    return;
  }

  temp = malloc(elem_size);

  while (i--) {
    unsigned int j = BLI_rng_get_uint(rng) % elem_tot;
    if (i != j) {
      void *iElem = (unsigned char *)data + i * elem_size_i;
      void *jElem = (unsigned char *)data + j * elem_size_i;
      memcpy(temp, iElem, elem_size);
      memcpy(iElem, jElem, elem_size);
      memcpy(jElem, temp, elem_size);
    }
  }

  free(temp);
}

/**
 * Simulate getting \a n random values.
 *
 * \note Useful when threaded code needs consistent values, independent of task division.
 */
void BLI_rng_skip(RNG *rng, int n)
{
  rng->rng.skip((uint)n);
}

/***/

/* fill an array with random numbers */
void BLI_array_frand(float *ar, int count, unsigned int seed)
{
  RNG rng;

  BLI_rng_srandom(&rng, seed);

  for (int i = 0; i < count; i++) {
    ar[i] = BLI_rng_get_float(&rng);
  }
}

float BLI_hash_frand(unsigned int seed)
{
  RNG rng;

  BLI_rng_srandom(&rng, seed);
  return BLI_rng_get_float(&rng);
}

void BLI_array_randomize(void *data,
                         unsigned int elem_size,
                         unsigned int elem_tot,
                         unsigned int seed)
{
  RNG rng;

  BLI_rng_seed(&rng, seed);
  BLI_rng_shuffle_array(&rng, data, elem_size, elem_tot);
}

/* ********* for threaded random ************** */

static RNG rng_tab[BLENDER_MAX_THREADS];

void BLI_thread_srandom(int thread, unsigned int seed)
{
  if (thread >= BLENDER_MAX_THREADS) {
    thread = 0;
  }

  BLI_rng_seed(&rng_tab[thread], seed + hash[seed & 255]);
  seed = BLI_rng_get_uint(&rng_tab[thread]);
  BLI_rng_seed(&rng_tab[thread], seed + hash[seed & 255]);
  seed = BLI_rng_get_uint(&rng_tab[thread]);
  BLI_rng_seed(&rng_tab[thread], seed + hash[seed & 255]);
}

int BLI_thread_rand(int thread)
{
  return BLI_rng_get_int(&rng_tab[thread]);
}

float BLI_thread_frand(int thread)
{
  return BLI_rng_get_float(&rng_tab[thread]);
}

struct RNG_THREAD_ARRAY {
  RNG rng_tab[BLENDER_MAX_THREADS];
};

RNG_THREAD_ARRAY *BLI_rng_threaded_new(void)
{
  unsigned int i;
  RNG_THREAD_ARRAY *rngarr = (RNG_THREAD_ARRAY *)MEM_mallocN(sizeof(RNG_THREAD_ARRAY),
                                                             "random_array");

  for (i = 0; i < BLENDER_MAX_THREADS; i++) {
    BLI_rng_srandom(&rngarr->rng_tab[i], (unsigned int)clock());
  }

  return rngarr;
}

void BLI_rng_threaded_free(struct RNG_THREAD_ARRAY *rngarr)
{
  MEM_freeN(rngarr);
}

int BLI_rng_thread_rand(RNG_THREAD_ARRAY *rngarr, int thread)
{
  return BLI_rng_get_int(&rngarr->rng_tab[thread]);
}

/* ********* Low-discrepancy sequences ************** */

/* incremental halton sequence generator, from:
 * "Instant Radiosity", Keller A. */
BLI_INLINE double halton_ex(double invprimes, double *offset)
{
  double e = fabs((1.0 - *offset) - 1e-10);

  if (invprimes >= e) {
    double lasth;
    double h = invprimes;

    do {
      lasth = h;
      h *= invprimes;
    } while (h >= e);

    *offset += ((lasth + h) - 1.0);
  }
  else {
    *offset += invprimes;
  }

  return *offset;
}

void BLI_halton_1d(unsigned int prime, double offset, int n, double *r)
{
  const double invprime = 1.0 / (double)prime;

  *r = 0.0;

  for (int s = 0; s < n; s++) {
    *r = halton_ex(invprime, &offset);
  }
}

void BLI_halton_2d(const unsigned int prime[2], double offset[2], int n, double *r)
{
  const double invprimes[2] = {1.0 / (double)prime[0], 1.0 / (double)prime[1]};

  r[0] = r[1] = 0.0;

  for (int s = 0; s < n; s++) {
    for (int i = 0; i < 2; i++) {
      r[i] = halton_ex(invprimes[i], &offset[i]);
    }
  }
}

void BLI_halton_3d(const unsigned int prime[3], double offset[3], int n, double *r)
{
  const double invprimes[3] = {
      1.0 / (double)prime[0], 1.0 / (double)prime[1], 1.0 / (double)prime[2]};

  r[0] = r[1] = r[2] = 0.0;

  for (int s = 0; s < n; s++) {
    for (int i = 0; i < 3; i++) {
      r[i] = halton_ex(invprimes[i], &offset[i]);
    }
  }
}

void BLI_halton_2d_sequence(const unsigned int prime[2], double offset[2], int n, double *r)
{
  const double invprimes[2] = {1.0 / (double)prime[0], 1.0 / (double)prime[1]};

  for (int s = 0; s < n; s++) {
    for (int i = 0; i < 2; i++) {
      r[s * 2 + i] = halton_ex(invprimes[i], &offset[i]);
    }
  }
}

/* From "Sampling with Hammersley and Halton Points" TT Wong
 * Appendix: Source Code 1 */
BLI_INLINE double radical_inverse(unsigned int n)
{
  double u = 0;

  /* This reverse the bit-wise representation
   * around the decimal point. */
  for (double p = 0.5; n; p *= 0.5, n >>= 1) {
    if (n & 1) {
      u += p;
    }
  }

  return u;
}

void BLI_hammersley_1d(unsigned int n, double *r)
{
  *r = radical_inverse(n);
}

void BLI_hammersley_2d_sequence(unsigned int n, double *r)
{
  for (unsigned int s = 0; s < n; s++) {
    r[s * 2 + 0] = (double)(s + 0.5) / (double)n;
    r[s * 2 + 1] = radical_inverse(s);
  }
}

namespace blender {

/**
 * Set a randomized hash of the value as seed.
 */
void RandomNumberGenerator::seed_random(uint32_t seed)
{
  this->seed(seed + hash[seed & 255]);
  seed = this->get_uint32();
  this->seed(seed + hash[seed & 255]);
  seed = this->get_uint32();
  this->seed(seed + hash[seed & 255]);
}

float2 RandomNumberGenerator::get_unit_float2()
{
  float a = (float)(M_PI * 2.0) * this->get_float();
  return {cosf(a), sinf(a)};
}

float3 RandomNumberGenerator::get_unit_float3()
{
  float z = (2.0f * this->get_float()) - 1.0f;
  float r = 1.0f - z * z;
  if (r > 0.0f) {
    float a = (float)(M_PI * 2.0) * this->get_float();
    r = sqrtf(r);
    float x = r * cosf(a);
    float y = r * sinf(a);
    return {x, y, z};
  }
  return {0.0f, 0.0f, 1.0f};
}

/**
 * Generate a random point inside the given triangle.
 */
float2 RandomNumberGenerator::get_triangle_sample(float2 v1, float2 v2, float2 v3)
{
  float u = this->get_float();
  float v = this->get_float();

  if (u + v > 1.0f) {
    u = 1.0f - u;
    v = 1.0f - v;
  }

  float2 side_u = v2 - v1;
  float2 side_v = v3 - v1;

  float2 sample = v1;
  sample += side_u * u;
  sample += side_v * v;
  return sample;
}

float3 RandomNumberGenerator::get_triangle_sample_3d(float3 v1, float3 v2, float3 v3)
{
  float u = this->get_float();
  float v = this->get_float();

  if (u + v > 1.0f) {
    u = 1.0f - u;
    v = 1.0f - v;
  }

  float3 side_u = v2 - v1;
  float3 side_v = v3 - v1;

  float3 sample = v1;
  sample += side_u * u;
  sample += side_v * v;
  return sample;
}

void RandomNumberGenerator::get_bytes(MutableSpan<char> r_bytes)
{
  constexpr int64_t mask_bytes = 2;
  constexpr int64_t rand_stride = static_cast<int64_t>(sizeof(x_)) - mask_bytes;

  int64_t last_len = 0;
  int64_t trim_len = r_bytes.size();

  if (trim_len > rand_stride) {
    last_len = trim_len % rand_stride;
    trim_len = trim_len - last_len;
  }
  else {
    trim_len = 0;
    last_len = r_bytes.size();
  }

  const char *data_src = (const char *)&x_;
  int64_t i = 0;
  while (i != trim_len) {
    BLI_assert(i < trim_len);
#ifdef __BIG_ENDIAN__
    for (int64_t j = (rand_stride + mask_bytes) - 1; j != mask_bytes - 1; j--)
#else
    for (int64_t j = 0; j != rand_stride; j++)
#endif
    {
      r_bytes[i++] = data_src[j];
    }
    this->step();
  }
  if (last_len) {
    for (int64_t j = 0; j != last_len; j++) {
      r_bytes[i++] = data_src[j];
    }
  }
}

}  // namespace blender
