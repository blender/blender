/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
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
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenlib/intern/rand.c
 *  \ingroup bli
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BLI_threads.h"
#include "BLI_rand.h"
#include "BLI_math.h"

#include "BLI_sys_types.h"
#include "BLI_strict_flags.h"

#define MULTIPLIER  0x5DEECE66Dll
#define MASK        0x0000FFFFFFFFFFFFll

#define ADDEND      0xB
#define LOWSEED     0x330E

extern unsigned char hash[];    // noise.c

/**
 * Random Number Generator.
 */
struct RNG {
	uint64_t X;
};

RNG *BLI_rng_new(unsigned int seed)
{
	RNG *rng = MEM_mallocN(sizeof(*rng), "rng");

	BLI_rng_seed(rng, seed);

	return rng;
}

RNG *BLI_rng_new_srandom(unsigned int seed)
{
	RNG *rng = MEM_mallocN(sizeof(*rng), "rng");

	BLI_rng_srandom(rng, seed);

	return rng;
}

void BLI_rng_free(RNG *rng)
{
	MEM_freeN(rng);
}

void BLI_rng_seed(RNG *rng, unsigned int seed)
{
	rng->X = (((uint64_t) seed) << 16) | LOWSEED;
}

void BLI_rng_srandom(RNG *rng, unsigned int seed)
{
	BLI_rng_seed(rng, seed + hash[seed & 255]);
	seed = BLI_rng_get_uint(rng);
	BLI_rng_seed(rng, seed + hash[seed & 255]);
	seed = BLI_rng_get_uint(rng);
	BLI_rng_seed(rng, seed + hash[seed & 255]);
}

BLI_INLINE void rng_step(RNG *rng)
{
	rng->X = (MULTIPLIER * rng->X + ADDEND) & MASK;
}

int BLI_rng_get_int(RNG *rng)
{
	rng_step(rng);
	return (int) (rng->X >> 17);
}

unsigned int BLI_rng_get_uint(RNG *rng)
{
	rng_step(rng);
	return (unsigned int) (rng->X >> 17);
}

double BLI_rng_get_double(RNG *rng)
{
	return (double) BLI_rng_get_int(rng) / 0x80000000;
}

float BLI_rng_get_float(RNG *rng)
{
	return (float) BLI_rng_get_int(rng) / 0x80000000;
}

void BLI_rng_get_float_unit_v2(RNG *rng, float v[2])
{
	float a = (float)(M_PI * 2.0) * BLI_rng_get_float(rng);
	v[0] = cosf(a);
	v[1] = sinf(a);
}

void BLI_rng_get_float_unit_v3(RNG *rng, float v[3])
{
	float r;
	v[2] = (2.0f * BLI_rng_get_float(rng)) - 1.0f;
	if ((r = 1.0f - (v[2] * v[2])) > 0.0f) {
		float a = (float)(M_PI * 2.0) * BLI_rng_get_float(rng);
		r = sqrtf(r);
		v[0] = r * cosf(a);
		v[1] = r * sinf(a);
	}
	else {
		v[2] = 1.0f;
	}
}

/**
 * Generate a random point inside given tri.
 */
void BLI_rng_get_tri_sample_float_v2(
        RNG *rng, const float v1[2], const float v2[2], const float v3[2],
        float r_pt[2])
{
	float u = BLI_rng_get_float(rng);
	float v = BLI_rng_get_float(rng);

	float side_u[2], side_v[2];

	if ((u + v) > 1.0f) {
		u = 1.0f - u;
		v = 1.0f - v;
	}

	sub_v2_v2v2(side_u, v2, v1);
	sub_v2_v2v2(side_v, v3, v1);

	copy_v2_v2(r_pt, v1);
	madd_v2_v2fl(r_pt, side_u, u);
	madd_v2_v2fl(r_pt, side_v, v);
}

void BLI_rng_shuffle_array(RNG *rng, void *data, unsigned int elem_size_i, unsigned int elem_tot)
{
	const size_t elem_size = (unsigned int)elem_size_i;
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

void BLI_rng_skip(RNG *rng, int n)
{
	while (n--) {
		rng_step(rng);
	}
}

/***/

/* initialize with some non-zero seed */
static RNG theBLI_rng = {611330372042337130};

/* using hash table to create better seed */
void BLI_srandom(unsigned int seed)
{
	BLI_rng_srandom(&theBLI_rng, seed);
}

int BLI_rand(void)
{
	return BLI_rng_get_int(&theBLI_rng);
}

float BLI_frand(void)
{
	return BLI_rng_get_float(&theBLI_rng);
}

void BLI_frand_unit_v3(float v[3])
{
	BLI_rng_get_float_unit_v3(&theBLI_rng, v);
}

float BLI_hash_frand(unsigned int seed)
{
	RNG rng;

	BLI_rng_srandom(&rng, seed);
	return BLI_rng_get_float(&rng);
}

void BLI_array_randomize(void *data, unsigned int elem_size, unsigned int elem_tot, unsigned int seed)
{
	RNG rng;

	BLI_rng_seed(&rng, seed);
	BLI_rng_shuffle_array(&rng, data, elem_size, elem_tot);
}

/* ********* for threaded random ************** */

static RNG rng_tab[BLENDER_MAX_THREADS];

void BLI_thread_srandom(int thread, unsigned int seed)
{
	if (thread >= BLENDER_MAX_THREADS)
		thread = 0;
	
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
	RNG_THREAD_ARRAY *rngarr = MEM_mallocN(sizeof(RNG_THREAD_ARRAY), "random_array");
	
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

