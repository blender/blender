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

/** \file gameengine/Expressions/KX_HashedPtr.cpp
 *  \ingroup expressions
 */
#ifdef __MINGW64__
#include <basetsd.h>
#endif

#include "KX_HashedPtr.h"

unsigned int KX_Hash(void * inDWord)
{
#ifdef _WIN64
	unsigned __int64 key = (unsigned __int64)inDWord;
#else
	unsigned long key = (unsigned long)inDWord;
#endif

	key += ~(key << 16);
	key ^=  (key >>  5);
	key +=  (key <<  3);
	key ^=  (key >> 13);
	key += ~(key <<  9);
	key ^=  (key >> 17);

	return (unsigned int)(key & 0xffffffff);
}


CHashedPtr::CHashedPtr(void* val) : m_valptr(val)
{
}



unsigned int CHashedPtr::hash() const
{
	return KX_Hash(m_valptr);
}
