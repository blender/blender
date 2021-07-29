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
 *
 */

/** \file container/CTR_HashedPtr.h
 *  \ingroup ctr
 */

#ifndef __CTR_HASHEDPTR_H__
#define __CTR_HASHEDPTR_H__

#include <stdlib.h>

inline unsigned int CTR_Hash(void *inDWord)
{
	size_t key = (size_t)inDWord;
	return (unsigned int)(key ^ (key >> 4));
}

class CTR_HashedPtr
{
	void *m_valptr;
public:
	CTR_HashedPtr(void *val) : m_valptr(val) {
	}
	unsigned int hash() const {
		return CTR_Hash(m_valptr);
	}
	inline friend bool operator ==(const CTR_HashedPtr & rhs, const CTR_HashedPtr & lhs) {
		return rhs.m_valptr == lhs.m_valptr;
	}
	void *getValue() const {
		return m_valptr;
	}
};

#endif  /* __CTR_HASHEDPTR_H__ */

