/*
 * $Id$
 *
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
#include "GEN_HashedPtr.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "BLO_sys_types.h" // for intptr_t support

//
// Build hash index from pointer.  Even though the final result
// is a 32-bit integer, use all the bits of the pointer as long
// as possible.
//
#if 1
unsigned int GEN_Hash(void * inDWord)
{
	uintptr_t key = (uintptr_t)inDWord;
#if 0
	// this is way too complicated
	key += ~(key << 16);
	key ^=  (key >>  5);
	key +=  (key <<  3);
	key ^=  (key >> 13);
	key += ~(key <<  9);
	key ^=  (key >> 17);

  	return (unsigned int)(key & 0xffffffff);
#else
	return (unsigned int)(key ^ (key>>4));
#endif
}
#endif