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

/** \file string/STR_HashedString.h
 *  \ingroup string
 *
 * Copyright (C) 2001 NaN Technologies B.V.
 * This file was formerly known as: GEN_StdString.cpp.
 * \date	November, 14, 2001
 */

#ifndef __STR_HASHEDSTRING_H__
#define __STR_HASHEDSTRING_H__

#include "STR_String.h"


// Hash Mix utility function, by Bob Jenkins - Mix 3 32-bit values reversibly
//
// - If gHashMix() is run forward or backward, at least 32 bits in a,b,c have at
//   least 1/4 probability of changing.
//
// - If gHashMix() is run forward, every bit of c will change between 1/3 and
//   2/3 of the time.
//
static inline void STR_gHashMix(dword& a, dword& b, dword& c)
{
	a -= b; a -= c; a ^= (c >> 13);
	b -= c; b -= a; b ^= (a << 8);
	c -= a; c -= b; c ^= (b >> 13);
	a -= b; a -= c; a ^= (c >> 12);
	b -= c; b -= a; b ^= (a << 16);
	c -= a; c -= b; c ^= (b >> 5);
	a -= b; a -= c; a ^= (c >> 3);
	b -= c; b -= a; b ^= (a << 10);
	c -= a; c -= b; c ^= (b >> 15);
}

//
// Fast Hashable<int32> functionality
// http://www.concentric.net/~Ttwang/tech/inthash.htm
//
static inline dword         STR_gHash(dword inDWord)
{
	dword key = inDWord;
	key += ~(key << 16);
	key ^=  (key >>  5);
	key +=  (key <<  3);
	key ^=  (key >> 13);
	key += ~(key <<  9);
	key ^=  (key >> 17);
	return key;
}

enum { GOLDEN_RATIO = 0x9e3779b9 }; /* arbitrary value to initialize hash funtion, well not so arbitrary
                                     * as this value is taken from the pigs library (Orange Games/Lost Boys) */



static dword STR_gHash(const void *in, int len, dword init_val)
{
	unsigned int length = len;
	dword a = (dword)GOLDEN_RATIO;
	dword b = (dword)GOLDEN_RATIO;
	dword c = init_val;  /* the previous hash value */
	byte  *p_in = (byte *)in;

	// Do the largest part of the key
	while (length >= 12)
	{
		a += (p_in[0] + ((dword)p_in[1] << 8) + ((dword)p_in[2]  << 16) + ((dword)p_in[3]  << 24));
		b += (p_in[4] + ((dword)p_in[5] << 8) + ((dword)p_in[6]  << 16) + ((dword)p_in[7]  << 24));
		c += (p_in[8] + ((dword)p_in[9] << 8) + ((dword)p_in[10] << 16) + ((dword)p_in[11] << 24));
		STR_gHashMix(a, b, c);
		p_in += 12; length -= 12;
	}

	// Handle the last 11 bytes
	c += len;
	switch (length) {
		case 11: c += ((dword)p_in[10] << 24);
		case 10: c += ((dword)p_in[9]  << 16);
		case  9: c += ((dword)p_in[8]  << 8);  /* the first byte of c is reserved for the length */
		case  8: b += ((dword)p_in[7]  << 24);
		case  7: b += ((dword)p_in[6]  << 16);
		case  6: b += ((dword)p_in[5]  << 8);
		case  5: b += p_in[4];
		case  4: a += ((dword)p_in[3]  << 24);
		case  3: a += ((dword)p_in[2]  << 16);
		case  2: a += ((dword)p_in[1]  << 8);
		case  1: a += p_in[0];
	}
	STR_gHashMix(a, b, c);

	return c;
}




class STR_HashedString : public STR_String
{
public:
	STR_HashedString() : STR_String(), m_Hashed(false) {}
	STR_HashedString(const char *str) : STR_String(str), m_Hashed(false) {}
	STR_HashedString(const STR_String &str) : STR_String(str), m_Hashed(false) {}

	inline dword hash(dword init = 0) const
	{ 
		if (!m_Hashed) 
		{
			const char *str = *this;
			int length = this->Length();
			m_CachedHash = STR_gHash(str, length, init);
			m_Hashed = true;
		} 
		return m_CachedHash;
	}

private:
	mutable bool m_Hashed;
	mutable dword m_CachedHash;
};

#endif //__STR_HASHEDSTRING_H__

