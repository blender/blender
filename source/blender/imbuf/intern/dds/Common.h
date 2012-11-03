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
 * Contributors: Amorilia (amorilia@users.sourceforge.net)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/dds/Common.h
 *  \ingroup imbdds
 */


#ifndef __COMMON_H__
#define __COMMON_H__

#ifndef min
#define min(a,b) ((a) <= (b) ? (a) : (b))
#endif
#ifndef max
#define max(a,b) ((a) >= (b) ? (a) : (b))
#endif
#ifndef clamp
#define clamp(x,a,b) min(max((x), (a)), (b))
#endif

template<typename T>
inline void
swap(T & a, T & b)
{
	T tmp = a;
	a = b;
	b = tmp;
}

typedef unsigned char      uint8;
typedef unsigned short     uint16;
typedef unsigned int       uint;
typedef unsigned int       uint32;
typedef unsigned long long uint64;

// copied from nvtt src/nvimage/nvimage.h
inline uint computePitch(uint w, uint bitsize, uint alignment)
{
	return ((w * bitsize +  8 * alignment - 1) / (8 * alignment)) * alignment;
}

#endif
