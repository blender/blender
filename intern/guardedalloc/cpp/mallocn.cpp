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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file guardedalloc/cpp/mallocn.cpp
 *  \ingroup MEM
 */


#include <new>
#include "../MEM_guardedalloc.h"

/* not default but can be used when needing to set a string */
void *operator new(size_t size, const char *str)
{
	return MEM_mallocN(size, str);
}
void *operator new[](size_t size, const char *str)
{
	return MEM_mallocN(size, str);
}


void *operator new(size_t size)
{
	return MEM_mallocN(size, "C++/anonymous");
}
void *operator new[](size_t size)
{
	return MEM_mallocN(size, "C++/anonymous[]");
}


void operator delete(void *p)
{
	/* delete NULL is valid in c++ */
	if (p)
		MEM_freeN(p);
}
void operator delete[](void *p)
{
	/* delete NULL is valid in c++ */
	if (p)
		MEM_freeN(p);
}
