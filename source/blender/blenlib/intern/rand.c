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

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "BLI_threads.h"
#include "BLI_rand.h"

#ifdef _MSC_VER
typedef unsigned __int64 r_uint64;

#define MULTIPLIER  0x5DEECE66Di64
#define MASK        0x0000FFFFFFFFFFFFi64
#else
typedef unsigned long long r_uint64;

#define MULTIPLIER  0x5DEECE66Dll
#define MASK        0x0000FFFFFFFFFFFFll
#endif

#define ADDEND      0xB

#define LOWSEED     0x330E

extern unsigned char hash[];    // noise.c

/***/

struct RNG {
	r_uint64 X;
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
	rng->X = (((r_uint64) seed) << 16) | LOWSEED;
}

void BLI_rng_srandom(RNG *rng, unsigned int seed)
{
	BLI_rng_seed(rng, seed + hash[seed & 255]);
	seed = BLI_rng_get_int(rng);
	BLI_rng_seed(rng, seed + hash[seed & 255]);
	seed = BLI_rng_get_int(rng);
	BLI_rng_seed(rng, seed + hash[seed & 255]);
}

int BLI_rng_get_int(RNG *rng)
{
	rng->X = (MULTIPLIER * rng->X + ADDEND) & MASK;
	return (int) (rng->X >> 17);
}

double BLI_rng_get_double(RNG *rng)
{
	return (double) BLI_rng_get_int(rng) / 0x80000000;
}

float BLI_rng_get_float(RNG *rng)
{
	return (float) BLI_rng_get_int(rng) / 0x80000000;
}

void BLI_rng_shuffle_array(RNG *rng, void *data, int elemSize, int numElems)
{
	int i = numElems;
	void *temp;

	if (numElems <= 0) {
		return;
	}

	temp = malloc(elemSize);

	/* XXX Shouldn't it rather be "while (i--) {" ?
	 *     Else we have no guaranty first (0) element has a chance to be shuffled... --mont29 */
	while (--i) {
		int j = BLI_rng_get_int(rng) % numElems;
		if (i != j) {
			void *iElem = (unsigned char *)data + i * elemSize;
			void *jElem = (unsigned char *)data + j * elemSize;
			memcpy(temp, iElem, elemSize);
			memcpy(iElem, jElem, elemSize);
			memcpy(jElem, temp, elemSize);
		}
	}

	free(temp);
}

void BLI_rng_skip(RNG *rng, int n)
{
	int i;

	for (i = 0; i < n; i++)
		BLI_rng_get_int(rng);
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

float BLI_hash_frand(unsigned int seed)
{
	RNG rng;

	BLI_rng_srandom(&rng, seed);
	return BLI_rng_get_float(&rng);
}

void BLI_array_randomize(void *data, int elemSize, int numElems, unsigned int seed)
{
	RNG rng;

	BLI_rng_seed(&rng, seed);
	BLI_rng_shuffle_array(&rng, data, elemSize, numElems);
}

/* ********* for threaded random ************** */

static RNG rng_tab[BLENDER_MAX_THREADS];

void BLI_thread_srandom(int thread, unsigned int seed)
{
	if (thread >= BLENDER_MAX_THREADS)
		thread = 0;
	
	BLI_rng_seed(&rng_tab[thread], seed + hash[seed & 255]);
	seed = BLI_rng_get_int(&rng_tab[thread]);
	BLI_rng_seed(&rng_tab[thread], seed + hash[seed & 255]);
	seed = BLI_rng_get_int(&rng_tab[thread]);
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

