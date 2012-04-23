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

#if defined(WIN32) && !defined(FREE_WINDOWS)
typedef unsigned __int64	r_uint64;

#define MULTIPLIER	0x5DEECE66Di64 
#define MASK		0x0000FFFFFFFFFFFFi64
#else
typedef unsigned long long	r_uint64;

#define MULTIPLIER	0x5DEECE66Dll
#define MASK		0x0000FFFFFFFFFFFFll
#endif

#define ADDEND		0xB

#define LOWSEED		0x330E

extern unsigned char hash[];	// noise.c

/***/

struct RNG {
	r_uint64 X;
};

RNG	*rng_new(unsigned int seed)
{
	RNG *rng = MEM_mallocN(sizeof(*rng), "rng");

	rng_seed(rng, seed);

	return rng;
}

void rng_free(RNG* rng)
{
	MEM_freeN(rng);
}

void rng_seed(RNG *rng, unsigned int seed)
{
	rng->X= (((r_uint64) seed)<<16) | LOWSEED;
}

void rng_srandom(RNG *rng, unsigned int seed)
{
	rng_seed(rng, seed + hash[seed & 255]);
	seed= rng_getInt(rng);
	rng_seed(rng, seed + hash[seed & 255]);
	seed= rng_getInt(rng);
	rng_seed(rng, seed + hash[seed & 255]);
}

int rng_getInt(RNG *rng)
{
	rng->X= (MULTIPLIER*rng->X + ADDEND)&MASK;
	return (int) (rng->X>>17);
}

double rng_getDouble(RNG *rng)
{
	return (double) rng_getInt(rng)/0x80000000;
}

float rng_getFloat(RNG *rng)
{
	return (float) rng_getInt(rng)/0x80000000;
}

void rng_shuffleArray(RNG *rng, void *data, int elemSize, int numElems)
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
		int j = rng_getInt(rng)%numElems;
		if (i!=j) {
			void *iElem = (unsigned char*)data + i*elemSize;
			void *jElem = (unsigned char*)data + j*elemSize;
			memcpy(temp, iElem, elemSize);
			memcpy(iElem, jElem, elemSize);
			memcpy(jElem, temp, elemSize);
		}
	}

	free(temp);
}

void rng_skip(RNG *rng, int n)
{
	int i;

	for (i=0; i<n; i++)
		rng_getInt(rng);
}

/***/

static RNG theBLI_rng = {0};

/* note, this one creates periodical patterns */
void BLI_srand(unsigned int seed)
{
	rng_seed(&theBLI_rng, seed);
}

/* using hash table to create better seed */
void BLI_srandom(unsigned int seed)
{
	rng_srandom(&theBLI_rng, seed);
}

int BLI_rand(void)
{
	return rng_getInt(&theBLI_rng);
}

double BLI_drand(void)
{
	return rng_getDouble(&theBLI_rng);
}

float BLI_frand(void)
{
	return rng_getFloat(&theBLI_rng);
}

void BLI_fillrand(void *addr, int len)
{
	RNG rng;
	unsigned char *p= addr;

	rng_seed(&rng, (unsigned int) (PIL_check_seconds_timer()*0x7FFFFFFF));
	while (len--) *p++= rng_getInt(&rng)&0xFF;
}

void BLI_array_randomize(void *data, int elemSize, int numElems, unsigned int seed)
{
	RNG rng;

	rng_seed(&rng, seed);
	rng_shuffleArray(&rng, data, elemSize, numElems);
}

/* ********* for threaded random ************** */

static RNG rng_tab[BLENDER_MAX_THREADS];

void BLI_thread_srandom(int thread, unsigned int seed)
{
	if (thread >= BLENDER_MAX_THREADS)
		thread= 0;
	
	rng_seed(&rng_tab[thread], seed + hash[seed & 255]);
	seed= rng_getInt(&rng_tab[thread]);
	rng_seed(&rng_tab[thread], seed + hash[seed & 255]);
	seed= rng_getInt(&rng_tab[thread]);
	rng_seed(&rng_tab[thread], seed + hash[seed & 255]);
}

int BLI_thread_rand(int thread)
{
	return rng_getInt(&rng_tab[thread]);
}

float BLI_thread_frand(int thread)
{
	return rng_getFloat(&rng_tab[thread]);
}

