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

#include "kernel/kernel_jitter.h"

CCL_NAMESPACE_BEGIN

#ifdef __SOBOL__

/* Skip initial numbers that are not as well distributed, especially the
 * first sequence is just 0 everywhere, which can be problematic for e.g.
 * path termination.
 */
#define SOBOL_SKIP 64

/* High Dimensional Sobol. */

/* Van der Corput radical inverse. */
ccl_device uint van_der_corput(uint bits)
{
	bits = (bits << 16) | (bits >> 16);
	bits = ((bits & 0x00ff00ff) << 8) | ((bits & 0xff00ff00) >> 8);
	bits = ((bits & 0x0f0f0f0f) << 4) | ((bits & 0xf0f0f0f0) >> 4);
	bits = ((bits & 0x33333333) << 2) | ((bits & 0xcccccccc) >> 2);
	bits = ((bits & 0x55555555) << 1) | ((bits & 0xaaaaaaaa) >> 1);
	return bits;
}

/* Sobol radical inverse. */
ccl_device uint sobol(uint i)
{
	uint r = 0;
	for(uint v = 1U << 31; i; i >>= 1, v ^= v >> 1) {
		if(i & 1) {
			r ^= v;
		}
	}
	return r;
}

/* Inverse of sobol radical inverse. */
ccl_device uint sobol_inverse(uint i)
{
	const uint msb = 1U << 31;
	uint r = 0;
	for(uint v = 1; i; i <<= 1, v ^= v << 1) {
		if(i & msb) {
			r ^= v;
		}
	}
	return r;
}

/* Multidimensional sobol with generator matrices
 * dimension 0 and 1 are equal to van_der_corput() and sobol() respectively.
 */
ccl_device uint sobol_dimension(KernelGlobals *kg, int index, int dimension)
{
	uint result = 0;
	uint i = index;
	for(uint j = 0; i; i >>= 1, j++) {
		if(i & 1) {
			result ^= kernel_tex_fetch(__sobol_directions, 32*dimension + j);
		}
	}
	return result;
}

/* Lookup index and x/y coordinate, assumes m is a power of two. */
ccl_device uint sobol_lookup(const uint m,
                             const uint frame,
                             const uint ex,
                             const uint ey,
                             uint *x, uint *y)
{
	/* Shift is constant per frame. */
	const uint shift = frame << (m << 1);
	const uint sobol_shift = sobol(shift);
	/* Van der Corput is its own inverse. */
	const uint lower = van_der_corput(ex << (32 - m));
	/* Need to compensate for ey difference and shift. */
	const uint sobol_lower = sobol(lower);
	const uint mask = ~-(1 << m) << (32 - m);  /* Only m upper bits. */
	const uint delta = ((ey << (32 - m)) ^ sobol_lower ^ sobol_shift) & mask;
	/* Only use m upper bits for the index (m is a power of two). */
	const uint sobol_result = delta | (delta >> m);
	const uint upper = sobol_inverse(sobol_result);
	const uint index = shift | upper | lower;
	*x = van_der_corput(index);
	*y = sobol_shift ^ sobol_result ^ sobol_lower;
	return index;
}

ccl_device_forceinline float path_rng_1D(KernelGlobals *kg,
                                         RNG *rng,
                                         int sample, int num_samples,
                                         int dimension)
{
#ifdef __CMJ__
	if(kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_CMJ) {
		/* Correlated multi-jitter. */
		int p = *rng + dimension;
		return cmj_sample_1D(sample, num_samples, p);
	}
#endif

#ifdef __SOBOL_FULL_SCREEN__
	uint result = sobol_dimension(kg, *rng, dimension);
	float r = (float)result * (1.0f/(float)0xFFFFFFFF);
	return r;
#else
	/* Compute sobol sequence value using direction vectors. */
	uint result = sobol_dimension(kg, sample + SOBOL_SKIP, dimension);
	float r = (float)result * (1.0f/(float)0xFFFFFFFF);

	/* Cranly-Patterson rotation using rng seed */
	float shift;

	/* Hash rng with dimension to solve correlation issues.
	 * See T38710, T50116.
	 */
	RNG tmp_rng = cmj_hash_simple(dimension, *rng);
	shift = tmp_rng * (1.0f/(float)0xFFFFFFFF);

	return r + shift - floorf(r + shift);
#endif
}

ccl_device_forceinline void path_rng_2D(KernelGlobals *kg,
                                        RNG *rng,
                                        int sample, int num_samples,
                                        int dimension,
                                        float *fx, float *fy)
{
#ifdef __CMJ__
	if(kernel_data.integrator.sampling_pattern == SAMPLING_PATTERN_CMJ) {
		/* Correlated multi-jitter. */
		int p = *rng + dimension;
		cmj_sample_2D(sample, num_samples, p, fx, fy);
	}
	else
#endif
	{
		/* Sobol. */
		*fx = path_rng_1D(kg, rng, sample, num_samples, dimension);
		*fy = path_rng_1D(kg, rng, sample, num_samples, dimension + 1);
	}
}

ccl_device_inline void path_rng_init(KernelGlobals *kg,
                                     ccl_global uint *rng_state,
                                     int sample, int num_samples,
                                     RNG *rng,
                                     int x, int y,
                                     float *fx, float *fy)
{
#ifdef __SOBOL_FULL_SCREEN__
	uint px, py;
	uint bits = 16; /* limits us to 65536x65536 and 65536 samples */
	uint size = 1 << bits;
	uint frame = sample;

	*rng = sobol_lookup(bits, frame, x, y, &px, &py);

	*rng ^= kernel_data.integrator.seed;

	if(sample == 0) {
		*fx = 0.5f;
		*fy = 0.5f;
	}
	else {
		*fx = size * (float)px * (1.0f/(float)0xFFFFFFFF) - x;
		*fy = size * (float)py * (1.0f/(float)0xFFFFFFFF) - y;
	}
#else
	*rng = *rng_state;

	*rng ^= kernel_data.integrator.seed;

	if(sample == 0) {
		*fx = 0.5f;
		*fy = 0.5f;
	}
	else {
		path_rng_2D(kg, rng, sample, num_samples, PRNG_FILTER_U, fx, fy);
	}
#endif
}

ccl_device void path_rng_end(KernelGlobals *kg,
                             ccl_global uint *rng_state,
                             RNG rng)
{
	/* nothing to do */
}

#else  /* __SOBOL__ */

/* Linear Congruential Generator */

ccl_device_forceinline float path_rng_1D(KernelGlobals *kg,
                                         RNG *rng,
                                         int sample, int num_samples,
                                         int dimension)
{
	/* implicit mod 2^32 */
	*rng = (1103515245*(*rng) + 12345);
	return (float)*rng * (1.0f/(float)0xFFFFFFFF);
}

ccl_device_inline void path_rng_2D(KernelGlobals *kg,
                                   RNG *rng,
                                   int sample, int num_samples,
                                   int dimension,
                                   float *fx, float *fy)
{
	*fx = path_rng_1D(kg, rng, sample, num_samples, dimension);
	*fy = path_rng_1D(kg, rng, sample, num_samples, dimension + 1);
}

ccl_device void path_rng_init(KernelGlobals *kg,
                              ccl_global uint *rng_state,
                              int sample, int num_samples,
                              RNG *rng,
                              int x, int y,
                              float *fx, float *fy)
{
	/* load state */
	*rng = *rng_state;

	*rng ^= kernel_data.integrator.seed;

	if(sample == 0) {
		*fx = 0.5f;
		*fy = 0.5f;
	}
	else {
		path_rng_2D(kg, rng, sample, num_samples, PRNG_FILTER_U, fx, fy);
	}
}

ccl_device void path_rng_end(KernelGlobals *kg,
                             ccl_global uint *rng_state,
                             RNG rng)
{
	/* store state for next sample */
	*rng_state = rng;
}

#endif  /* __SOBOL__ */

/* Linear Congruential Generator */

ccl_device uint lcg_step_uint(uint *rng)
{
	/* implicit mod 2^32 */
	*rng = (1103515245*(*rng) + 12345);
	return *rng;
}

ccl_device float lcg_step_float(uint *rng)
{
	/* implicit mod 2^32 */
	*rng = (1103515245*(*rng) + 12345);
	return (float)*rng * (1.0f/(float)0xFFFFFFFF);
}

ccl_device uint lcg_init(uint seed)
{
	uint rng = seed;
	lcg_step_uint(&rng);
	return rng;
}

/* Path Tracing Utility Functions
 *
 * For each random number in each step of the path we must have a unique
 * dimension to avoid using the same sequence twice.
 *
 * For branches in the path we must be careful not to reuse the same number
 * in a sequence and offset accordingly.
 */

ccl_device_inline float path_state_rng_1D(KernelGlobals *kg,
                                          RNG *rng,
                                          const ccl_addr_space PathState *state,
                                          int dimension)
{
	return path_rng_1D(kg,
	                   rng,
	                   state->sample, state->num_samples,
	                   state->rng_offset + dimension);
}

ccl_device_inline float path_state_rng_1D_for_decision(
        KernelGlobals *kg,
        RNG *rng,
        const ccl_addr_space PathState *state,
        int dimension)
{
	/* The rng_offset is not increased for transparent bounces. if we do then
	 * fully transparent objects can become subtly visible by the different
	 * sampling patterns used where the transparent object is.
	 *
	 * however for some random numbers that will determine if we next bounce
	 * is transparent we do need to increase the offset to avoid always making
	 * the same decision. */
	const int rng_offset = state->rng_offset + state->transparent_bounce * PRNG_BOUNCE_NUM;
	return path_rng_1D(kg,
	                   rng,
	                   state->sample, state->num_samples,
	                   rng_offset + dimension);
}

ccl_device_inline void path_state_rng_2D(KernelGlobals *kg,
                                         RNG *rng,
                                         const ccl_addr_space PathState *state,
                                         int dimension,
                                         float *fx, float *fy)
{
	path_rng_2D(kg,
	            rng,
	            state->sample, state->num_samples,
	            state->rng_offset + dimension,
	            fx, fy);
}

ccl_device_inline float path_branched_rng_1D(
        KernelGlobals *kg,
        RNG *rng,
        const ccl_addr_space PathState *state,
        int branch,
        int num_branches,
        int dimension)
{
	return path_rng_1D(kg,
	                   rng,
	                   state->sample * num_branches + branch,
	                   state->num_samples * num_branches,
	                   state->rng_offset + dimension);
}

ccl_device_inline float path_branched_rng_1D_for_decision(
        KernelGlobals *kg,
        RNG *rng,
        const ccl_addr_space PathState *state,
        int branch,
        int num_branches,
        int dimension)
{
	const int rng_offset = state->rng_offset + state->transparent_bounce * PRNG_BOUNCE_NUM;
	return path_rng_1D(kg,
	                   rng,
	                   state->sample * num_branches + branch,
	                   state->num_samples * num_branches,
	                   rng_offset + dimension);
}

ccl_device_inline void path_branched_rng_2D(
        KernelGlobals *kg,
        RNG *rng,
        const ccl_addr_space PathState *state,
        int branch,
        int num_branches,
        int dimension,
        float *fx, float *fy)
{
	path_rng_2D(kg,
	            rng,
	            state->sample * num_branches + branch,
	            state->num_samples * num_branches,
	            state->rng_offset + dimension,
	            fx, fy);
}

/* Utitility functions to get light termination value,
 * since it might not be needed in many cases.
 */
ccl_device_inline float path_state_rng_light_termination(
        KernelGlobals *kg,
        RNG *rng,
        const ccl_addr_space PathState *state)
{
	if(kernel_data.integrator.light_inv_rr_threshold > 0.0f) {
		return path_state_rng_1D_for_decision(kg, rng, state, PRNG_LIGHT_TERMINATE);
	}
	return 0.0f;
}

ccl_device_inline float path_branched_rng_light_termination(
        KernelGlobals *kg,
        RNG *rng,
        const ccl_addr_space PathState *state,
        int branch,
        int num_branches)
{
	if(kernel_data.integrator.light_inv_rr_threshold > 0.0f) {
		return path_branched_rng_1D_for_decision(kg,
		                                         rng,
		                                         state,
		                                         branch,
		                                         num_branches,
		                                         PRNG_LIGHT_TERMINATE);
	}
	return 0.0f;
}

ccl_device_inline void path_state_branch(ccl_addr_space PathState *state,
                                         int branch,
                                         int num_branches)
{
	/* path is splitting into a branch, adjust so that each branch
	 * still gets a unique sample from the same sequence */
	state->rng_offset += PRNG_BOUNCE_NUM;
	state->sample = state->sample*num_branches + branch;
	state->num_samples = state->num_samples*num_branches;
}

ccl_device_inline uint lcg_state_init(RNG *rng,
                                      int rng_offset,
                                      int sample,
                                      uint scramble)
{
	return lcg_init(*rng + rng_offset + sample*scramble);
}

ccl_device float lcg_step_float_addrspace(ccl_addr_space uint *rng)
{
	/* Implicit mod 2^32 */
	*rng = (1103515245*(*rng) + 12345);
	return (float)*rng * (1.0f/(float)0xFFFFFFFF);
}

CCL_NAMESPACE_END

