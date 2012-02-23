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

/** \file SCA_RandomNumberGenerator.h
 *  \ingroup gamelogic
 *  \brief Generate random numbers that can be used by other components. Each
 * generator needs its own generator, so that the seed can be set
 * on a per-generator basis.
 */

#ifndef __SCA_RANDOMNUMBERGENERATOR_H__
#define __SCA_RANDOMNUMBERGENERATOR_H__

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

class SCA_RandomNumberGenerator {

	/* reference counted for memleak */
	int m_refcount;

	/** base seed */
	long m_seed;

	/* A bit silly.. The N parameter is a define in the .cpp file */
	/** the array for the state vector  */
	/* unsigned long mt[N]; */
	unsigned long mt[624];

	/** mti==N+1 means mt[KX_MT_VectorLenght] is not initialized */
	int mti; /* initialised in the cpp file */

	/** Calculate a start vector */
	void SetStartVector(void);
 public:
	SCA_RandomNumberGenerator(long seed);
	~SCA_RandomNumberGenerator();
	unsigned long Draw();
	float DrawFloat();
	long GetSeed();
	void SetSeed(long newseed);
	SCA_RandomNumberGenerator* AddRef()
	{
		++m_refcount;
		return this;
	}
	void Release()
	{
		if (--m_refcount == 0)
			delete this;
	}
	
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GE:SCA_RandomNumberGenerator"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif /* __SCA_RANDOMNUMBERGENERATOR_H__ */

