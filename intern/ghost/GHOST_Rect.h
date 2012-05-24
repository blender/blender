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

/** \file ghost/intern/GHOST_Debug.h
 *  \ingroup GHOST
 * Macro's used in GHOST debug target.
 */

#ifndef __GHOST_RECT_H__
#define __GHOST_RECT_H__

#include "GHOST_Types.h"


/**
 * Implements rectangle functionality.
 * The four extreme coordinates are stored as left, top, right and bottom.
 * To be valid, a rectangle should have a left coordinate smaller than or equal to right.
 * To be valid, a rectangle should have a top coordinate smaller than or equal to bottom.
 * @author	Maarten Gribnau
 * @date	May 10, 2001
 */

class GHOST_Rect {
public:

	/**
	 * Constructs a rectangle with the given values.
	 * @param	l	requested left coordinate of the rectangle
	 * @param	t	requested top coordinate of the rectangle
	 * @param	r	requested right coordinate of the rectangle
	 * @param	b	requested bottom coordinate of the rectangle
	 */
	GHOST_Rect(GHOST_TInt32 l = 0, GHOST_TInt32 t = 0, GHOST_TInt32 r = 0, GHOST_TInt32 b = 0)
		: m_l(l), m_t(t), m_r(r), m_b(b)
	{}

	/**
	 * Copy constructor.
	 * @param	r	rectangle to copy
	 */
	GHOST_Rect(const GHOST_Rect& r)
		: m_l(r.m_l), m_t(r.m_t), m_r(r.m_r), m_b(r.m_b)
	{}
	
	/**
	 * Destructor.
	 */
	virtual ~GHOST_Rect()
	{};

	/**
	 * Access to rectangle width.
	 * @return	width of the rectangle
	 */
	virtual inline GHOST_TInt32 getWidth() const;

	/**
	 * Access to rectangle height.
	 * @return	height of the rectangle
	 */
	virtual inline GHOST_TInt32 getHeight() const;

	/**
	 * Sets all members of the rectangle.
	 * @param	l	requested left coordinate of the rectangle
	 * @param	t	requested top coordinate of the rectangle
	 * @param	r	requested right coordinate of the rectangle
	 * @param	b	requested bottom coordinate of the rectangle
	 */
	virtual inline void set(GHOST_TInt32 l, GHOST_TInt32 t, GHOST_TInt32 r, GHOST_TInt32 b);

	/**
	 * Returns whether this rectangle is empty.
	 * Empty rectangles are rectangles that have width==0 and/or height==0.
	 * @return	boolean value (true==empty rectangle)
	 */
	virtual inline bool isEmpty() const;

	/**
	 * Returns whether this rectangle is valid.
	 * Valid rectangles are rectangles that have m_l <= m_r and m_t <= m_b. Thus, emapty rectangles are valid.
	 * @return	boolean value (true==valid rectangle)
	 */
	virtual inline bool isValid() const;

	/**
	 * Grows (or shrinks the rectangle).
	 * The method avoids negative insets making the rectangle invalid
	 * @param	i	The amount of offset given to each extreme (negative values shrink the rectangle).
	 */
	virtual void inset(GHOST_TInt32 i);

	/**
	 * Does a union of the rectangle given and this rectangle.
	 * The result is stored in this rectangle.
	 * @param	r	The rectangle that is input for the union operation.
	 */
	virtual inline void unionRect(const GHOST_Rect& r);

	/**
	 * Grows the rectangle to included a point.
	 * @param	x	The x-coordinate of the point.
	 * @param	y	The y-coordinate of the point.
	 */
	virtual inline void unionPoint(GHOST_TInt32 x, GHOST_TInt32 y);

	/**
	 * Grows the rectangle to included a point.
	 * @param	x	The x-coordinate of the point.
	 * @param	y	The y-coordinate of the point.
	 */
	virtual inline void wrapPoint(GHOST_TInt32 &x, GHOST_TInt32 &y, GHOST_TInt32 ofs);

	/**
	 * Returns whether the point is inside this rectangle.
	 * Point on the boundary is considered inside.
	 * @param x	x-coordinate of point to test.
	 * @param y y-coordinate of point to test.
	 * @return boolean value (true if point is inside).
	 */
	virtual inline bool isInside(GHOST_TInt32 x, GHOST_TInt32 y) const;

	/**
	 * Returns whether the rectangle is inside this rectangle.
	 * @param	r	rectangle to test.
	 * @return	visibility (not, partially or fully visible).
	 */
	virtual GHOST_TVisibility getVisibility(GHOST_Rect& r) const;

	/**
	 * Sets rectangle members.
	 * Sets rectangle members such that it is centered at the given location.
	 * @param	cx	requested center x-coordinate of the rectangle
	 * @param	cy	requested center y-coordinate of the rectangle
	 */
	virtual void setCenter(GHOST_TInt32 cx, GHOST_TInt32 cy);

	/**
	 * Sets rectangle members.
	 * Sets rectangle members such that it is centered at the given location,
	 * with the width requested.
	 * @param	cx	requested center x-coordinate of the rectangle
	 * @param	cy	requested center y-coordinate of the rectangle
	 * @param	w	requested width of the rectangle
	 * @param	h	requested height of the rectangle
	 */
	virtual void setCenter(GHOST_TInt32 cx, GHOST_TInt32 cy, GHOST_TInt32 w, GHOST_TInt32 h);

	/**
	 * Clips a rectangle.
	 * Updates the rectangle given such that it will fit within this one.
	 * This can result in an empty rectangle.
	 * @param	r	the rectangle to clip
	 * @return	whether clipping has occurred
	 */
	virtual bool clip(GHOST_Rect& r) const;

	/** Left coordinate of the rectangle */
	GHOST_TInt32 m_l;
	/** Top coordinate of the rectangle */
	GHOST_TInt32 m_t;
	/** Right coordinate of the rectangle */
	GHOST_TInt32 m_r;
	/** Bottom coordinate of the rectangle */
	GHOST_TInt32 m_b;

#ifdef WITH_CXX_GUARDEDALLOC
public:
	void *operator new(size_t num_bytes) { return MEM_mallocN(num_bytes, "GHOST:GHOST_Rect"); }
	void operator delete( void *mem ) { MEM_freeN(mem); }
#endif
};


inline GHOST_TInt32 GHOST_Rect::getWidth() const
{
	return m_r - m_l;
}

inline GHOST_TInt32 GHOST_Rect::getHeight() const
{
	return m_b - m_t;
}

inline void GHOST_Rect::set(GHOST_TInt32 l, GHOST_TInt32 t, GHOST_TInt32 r, GHOST_TInt32 b)
{
	m_l = l; m_t = t; m_r = r; m_b = b;
}

inline bool GHOST_Rect::isEmpty() const
{
	return (getWidth() == 0) || (getHeight() == 0);
}

inline bool GHOST_Rect::isValid() const
{
	return (m_l <= m_r) && (m_t <= m_b);
}

inline void GHOST_Rect::unionRect(const GHOST_Rect& r)
{
	if (r.m_l < m_l) m_l = r.m_l;
	if (r.m_r > m_r) m_r = r.m_r;
	if (r.m_t < m_t) m_t = r.m_t;
	if (r.m_b > m_b) m_b = r.m_b;
}

inline void GHOST_Rect::unionPoint(GHOST_TInt32 x, GHOST_TInt32 y)
{
	if (x < m_l) m_l = x;
	if (x > m_r) m_r = x;
	if (y < m_t) m_t = y;
	if (y > m_b) m_b = y;
}
#include <stdio.h>
inline void GHOST_Rect::wrapPoint(GHOST_TInt32 &x, GHOST_TInt32 &y, GHOST_TInt32 ofs)
{
	GHOST_TInt32 w = getWidth();
	GHOST_TInt32 h = getHeight();

	/* highly unlikely but avoid eternal loop */
	if (w - ofs * 2 <= 0 || h - ofs * 2 <= 0) {
		return;
	}

	while (x - ofs < m_l) x += w - (ofs * 2);
	while (y - ofs < m_t) y += h - (ofs * 2);
	while (x + ofs > m_r) x -= w - (ofs * 2);
	while (y + ofs > m_b) y -= h - (ofs * 2);
}

inline bool GHOST_Rect::isInside(GHOST_TInt32 x, GHOST_TInt32 y) const
{
	return (x >= m_l) && (x <= m_r) && (y >= m_t) && (y <= m_b);
}

#endif // __GHOST_RECT_H__

