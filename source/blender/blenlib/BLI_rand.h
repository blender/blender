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
 
#ifndef __BLI_RAND_H__
#define __BLI_RAND_H__

/** \file BLI_rand.h
 *  \ingroup bli
 *  \brief Random number functions.
 */

/* RNG is an abstract random number generator type that avoids using globals.
 * Always use this instead of the global RNG unless you have a good reason,
 * the global RNG is not thread safe and will not give repeatable results.
 */
struct RNG;
typedef struct RNG RNG;

struct RNG *BLI_rng_new(unsigned int seed);
struct RNG *BLI_rng_new_srandom(unsigned int seed);
void        BLI_rng_free(struct RNG *rng);

void        BLI_rng_seed(struct RNG *rng, unsigned int seed);
void        BLI_rng_srandom(struct RNG *rng, unsigned int seed);
int         BLI_rng_get_int(struct RNG *rng);
unsigned int BLI_rng_get_uint(struct RNG *rng);
double      BLI_rng_get_double(struct RNG *rng);
float       BLI_rng_get_float(struct RNG *rng);
void        BLI_rng_get_float_unit_v3(struct RNG *rng, float v[3]);
void        BLI_rng_shuffle_array(struct RNG *rng, void *data, unsigned int elem_size_i, unsigned int elem_tot);

/** Note that skipping is as slow as generating n numbers! */
void        BLI_rng_skip(struct RNG *rng, int n);

/** Seed for the random number generator, using noise.c hash[] */
void    BLI_srandom(unsigned int seed);

/** Return a pseudo-random number N where 0<=N<(2^31) */
int     BLI_rand(void);

/** Return a pseudo-random number N where 0.0f<=N<1.0f */
float   BLI_frand(void);
void    BLI_frand_unit_v3(float v[3]);

/** Return a pseudo-random (hash) float from an integer value */
float	BLI_hash_frand(unsigned int seed);

/** Shuffle an array randomly using the given seed.
 * contents. This routine does not use nor modify
 * the state of the BLI random number generator.
 */
void    BLI_array_randomize(void *data, unsigned int elem_size, unsigned int elem_tot, unsigned int seed);


/** Better seed for the random number generator, using noise.c hash[] */
/** Allows up to BLENDER_MAX_THREADS threads to address */
void    BLI_thread_srandom(int thread, unsigned int seed);

/** Return a pseudo-random number N where 0<=N<(2^31) */
/** Allows up to BLENDER_MAX_THREADS threads to address */
int     BLI_thread_rand(int thread);

/** Return a pseudo-random number N where 0.0f<=N<1.0f */
/** Allows up to BLENDER_MAX_THREADS threads to address */
float   BLI_thread_frand(int thread);

#endif  /* __BLI_RAND_H__ */
