/** \file gameengine/GameLogic/SCA_RandomNumberGenerator.cpp
 *  \ingroup gamelogic
 */
/**
 * Generate random numbers that can be used by other components. We 
 * convert to different types/distributions elsewhere. This just 
 * delivers a clean, random bitvector.
 *
 */

/* A C-program for MT19937: Real number version                */
/*   genrand() generates one pseudorandom real number (double) */
/* which is uniformly distributed on [0,1]-interval, for each  */
/* call. sgenrand(seed) set initial values to the working area */
/* of 624 words. Before genrand(), sgenrand(seed) must be      */
/* called once. (seed is any 32-bit integer except for 0).     */
/* Integer generator is obtained by modifying two lines.       */
/*   Coded by Takuji Nishimura, considering the suggestions by */
/* Topher Cooper and Marc Rieffel in July-Aug. 1997.           */

/* This library is free software; you can redistribute it and/or   */
/* modify it under the terms of the GNU Library General Public     */
/* License as published by the Free Software Foundation; either    */
/* version 2 of the License, or (at your option) any later         */
/* version.                                                        */
/* This library is distributed in the hope that it will be useful, */
/* but WITHOUT ANY WARRANTY; without even the implied warranty of  */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.            */
/* See the GNU Library General Public License for more details.    */
/* You should have received a copy of the GNU Library General      */
/* Public License along with this library; if not, write to the    */
/* Free Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA   */ 
/* 02110-1301, USA                                                 */

/* Copyright (C) 1997 Makoto Matsumoto and Takuji Nishimura.       */
/* When you use this, send an email to: matumoto@math.keio.ac.jp   */
/* with an appropriate reference to your work.                     */

#include <limits.h>
#include "SCA_RandomNumberGenerator.h"

/* Period parameters */  
#define N 624
#define M 397
#define MATRIX_A 0x9908b0df   /* constant vector a */
#define UPPER_MASK 0x80000000 /* most significant w-r bits */
#define LOWER_MASK 0x7fffffff /* least significant r bits */

/* Tempering parameters */   
#define TEMPERING_MASK_B 0x9d2c5680
#define TEMPERING_MASK_C 0xefc60000
#define TEMPERING_SHIFT_U(y)  (y >> 11)
#define TEMPERING_SHIFT_S(y)  (y << 7)
#define TEMPERING_SHIFT_T(y)  (y << 15)
#define TEMPERING_SHIFT_L(y)  (y >> 18)

SCA_RandomNumberGenerator::SCA_RandomNumberGenerator(long seed)
{
	// int mti = N + 1; /*unused*/
	m_seed = seed;
	m_refcount = 1;
	SetStartVector();
}

SCA_RandomNumberGenerator::~SCA_RandomNumberGenerator()
{
	/* intentionally empty */
}

void SCA_RandomNumberGenerator::SetStartVector(void)
{
	/* setting initial seeds to mt[N] using         */
	/* the generator Line 25 of Table 1 in          */
	/* [KNUTH 1981, The Art of Computer Programming */
	/*    Vol. 2 (2nd Ed.), pp102]                  */
	mt[0] = m_seed & 0xffffffff;
	for (mti = 1; mti < N; mti++)
		mt[mti] = (69069 * mt[mti-1]) & 0xffffffff;
}

long SCA_RandomNumberGenerator::GetSeed() { return m_seed; }
void SCA_RandomNumberGenerator::SetSeed(long newseed) 
{ 
	m_seed = newseed;
	SetStartVector();
}

/**
 * This is the important part: copied verbatim :)
 */
unsigned long SCA_RandomNumberGenerator::Draw()
{
	static unsigned long mag01[2] = { 0x0, MATRIX_A };
	/* mag01[x] = x * MATRIX_A  for x=0,1 */

	unsigned long y;

	if (mti >= N) { /* generate N words at one time */
		int kk;

		/* I set this in the constructor, so it is always satisfied ! */
		//          if (mti == N+1)   /* if sgenrand() has not been called, */
		//              GEN_srand(4357); /* a default initial seed is used   */

		for (kk = 0; kk < N - M; kk++) {
			y = (mt[kk] & UPPER_MASK) | (mt[kk+1] & LOWER_MASK);
			mt[kk] = mt[kk+M] ^ (y >> 1) ^ mag01[y & 0x1];
		}
		for (; kk < N-1; kk++) {
			y = (mt[kk] & UPPER_MASK) | (mt[kk+1] & LOWER_MASK);
			mt[kk] = mt[kk+(M-N)] ^ (y >> 1) ^ mag01[y & 0x1];
		}
		y = (mt[N-1] & UPPER_MASK) | (mt[0] & LOWER_MASK);
		mt[N-1] = mt[M-1] ^ (y >> 1) ^ mag01[y & 0x1];

		mti = 0;
	}

	y = mt[mti++];
	y ^= TEMPERING_SHIFT_U(y);
	y ^= TEMPERING_SHIFT_S(y) & TEMPERING_MASK_B;
	y ^= TEMPERING_SHIFT_T(y) & TEMPERING_MASK_C;
	y ^= TEMPERING_SHIFT_L(y);

	return y;
}

float SCA_RandomNumberGenerator::DrawFloat()
{
	return ( (float) Draw()/ (unsigned long) 0xffffffff );
}

/* eof */
