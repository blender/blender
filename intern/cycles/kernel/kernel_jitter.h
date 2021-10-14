/*
 * Copyright 2011-2013 Blender Foundation
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
CCL_NAMESPACE_BEGIN

ccl_device_inline uint32_t laine_karras_permutation(uint32_t x, uint32_t seed)
{
  x += seed;
  x ^= (x * 0x6c50b47cu);
  x ^= x * 0xb82f1e52u;
  x ^= x * 0xc7afe638u;
  x ^= x * 0x8d22f6e6u;

  return x;
}

ccl_device_inline uint32_t nested_uniform_scramble(uint32_t x, uint32_t seed)
{
  x = reverse_integer_bits(x);
  x = laine_karras_permutation(x, seed);
  x = reverse_integer_bits(x);

  return x;
}

ccl_device_inline uint cmj_hash(uint i, uint p)
{
  i ^= p;
  i ^= i >> 17;
  i ^= i >> 10;
  i *= 0xb36534e5;
  i ^= i >> 12;
  i ^= i >> 21;
  i *= 0x93fc4795;
  i ^= 0xdf6e307f;
  i ^= i >> 17;
  i *= 1 | p >> 18;

  return i;
}

ccl_device_inline uint cmj_hash_simple(uint i, uint p)
{
  i = (i ^ 61) ^ p;
  i += i << 3;
  i ^= i >> 4;
  i *= 0x27d4eb2d;
  return i;
}

ccl_device_inline float cmj_randfloat(uint i, uint p)
{
  return cmj_hash(i, p) * (1.0f / 4294967808.0f);
}

ccl_device_inline float cmj_randfloat_simple(uint i, uint p)
{
  return cmj_hash_simple(i, p) * (1.0f / (float)0xFFFFFFFF);
}

ccl_device float pmj_sample_1D(ccl_global const KernelGlobals *kg,
                               uint sample,
                               uint rng_hash,
                               uint dimension)
{
  /* Perform Owen shuffle of the sample number to reorder the samples. */
#ifdef _SIMPLE_HASH_
  const uint rv = cmj_hash_simple(dimension, rng_hash);
#else /* Use a _REGULAR_HASH_. */
  const uint rv = cmj_hash(dimension, rng_hash);
#endif
#ifdef _XOR_SHUFFLE_
#  warning "Using XOR shuffle."
  const uint s = sample ^ rv;
#else /* Use _OWEN_SHUFFLE_ for reordering. */
  const uint s = nested_uniform_scramble(sample, rv);
#endif

  /* Based on the sample number a sample pattern is selected and offset by the dimension. */
  const uint sample_set = s / NUM_PMJ_SAMPLES;
  const uint d = (dimension + sample_set);
  const uint dim = d % NUM_PMJ_PATTERNS;

  /* The PMJ sample sets contain a sample with (x,y) with NUM_PMJ_SAMPLES so for 1D
   *  the x part is used for even dims and the y for odd. */
  int index = 2 * ((dim >> 1) * NUM_PMJ_SAMPLES + (s % NUM_PMJ_SAMPLES)) + (dim & 1);

  float fx = kernel_tex_fetch(__sample_pattern_lut, index);

#ifndef _NO_CRANLEY_PATTERSON_ROTATION_
  /* Use Cranley-Patterson rotation to displace the sample pattern. */
#  ifdef _SIMPLE_HASH_
  float dx = cmj_randfloat_simple(d, rng_hash);
#  else
  float dx = cmj_randfloat(d, rng_hash);
#  endif
  /* Jitter sample locations and map back into [0 1]. */
  fx = fx + dx;
  fx = fx - floorf(fx);
#else
#  warning "Not using Cranley-Patterson Rotation."
#endif

  return fx;
}

ccl_device void pmj_sample_2D(ccl_global const KernelGlobals *kg,
                              uint sample,
                              uint rng_hash,
                              uint dimension,
                              ccl_private float *x,
                              ccl_private float *y)
{
  /* Perform a shuffle on the sample number to reorder the samples. */
#ifdef _SIMPLE_HASH_
  const uint rv = cmj_hash_simple(dimension, rng_hash);
#else /* Use a _REGULAR_HASH_. */
  const uint rv = cmj_hash(dimension, rng_hash);
#endif
#ifdef _XOR_SHUFFLE_
#  warning "Using XOR shuffle."
  const uint s = sample ^ rv;
#else /* Use _OWEN_SHUFFLE_ for reordering. */
  const uint s = nested_uniform_scramble(sample, rv);
#endif

  /* Based on the sample number a sample pattern is selected and offset by the dimension. */
  const uint sample_set = s / NUM_PMJ_SAMPLES;
  const uint d = (dimension + sample_set);
  uint dim = d % NUM_PMJ_PATTERNS;
  int index = 2 * (dim * NUM_PMJ_SAMPLES + (s % NUM_PMJ_SAMPLES));

  float fx = kernel_tex_fetch(__sample_pattern_lut, index);
  float fy = kernel_tex_fetch(__sample_pattern_lut, index + 1);

#ifndef _NO_CRANLEY_PATTERSON_ROTATION_
  /* Use Cranley-Patterson rotation to displace the sample pattern. */
#  ifdef _SIMPLE_HASH_
  float dx = cmj_randfloat_simple(d, rng_hash);
  float dy = cmj_randfloat_simple(d + 1, rng_hash);
#  else
  float dx = cmj_randfloat(d, rng_hash);
  float dy = cmj_randfloat(d + 1, rng_hash);
#  endif
  /* Jitter sample locations and map back to the unit square [0 1]x[0 1]. */
  float sx = fx + dx;
  float sy = fy + dy;
  sx = sx - floorf(sx);
  sy = sy - floorf(sy);
#else
#  warning "Not using Cranley Patterson Rotation."
#endif

  (*x) = sx;
  (*y) = sy;
}

CCL_NAMESPACE_END
