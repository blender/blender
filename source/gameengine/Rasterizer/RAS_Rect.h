/**
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
 */

#ifndef _RAS_RECT
#define _RAS_RECT

#ifdef WITH_CXX_GUARDEDALLOC
#include "MEM_guardedalloc.h"
#endif

/** 
 * @section interface class.
 * RAS_Rect just encodes a simple rectangle.
 * Should be part of a generic library
 */

class RAS_Rect
{
public:  // todo: make a decent class, and make private
	int m_x1, m_y1;
	int m_x2, m_y2;

public:
	RAS_Rect() : m_x1(0), m_y1(0), m_x2(0), m_y2(0) {}
	int GetWidth(
	) const {
		return m_x2 - m_x1;
	}
	int GetHeight(
	) const {
		return m_y2 - m_y1;
	}
	int GetLeft(
	) const {
		return m_x1;
	}
	int GetRight(
	) const {
		return m_x2;
	}
	int GetBottom(
	) const {
		return m_y1;
	}
	int GetTop(
	) const {
		return m_y2;
	}

	void SetLeft(
		int x1)
	{
		m_x1 = x1;
	}
	void SetBottom(
		int y1)
	{
		m_y1 = y1;
	}
	void SetRight(
		int x2)
	{
		m_x2 = x2;
	}
	void SetTop(
		int y2)
	{
		m_y2 = y2;
	}
	
#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new( unsigned int num_bytes) { return MEM_mallocN(num_bytes, "GE:RAS_Rect"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};

#endif // _RAS_RECT

