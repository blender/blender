/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * @author	Maarten Gribnau
 * @date	March 7, 2001
 */

#ifndef _H_IMG_Rect
#define _H_IMG_Rect

#include "IMG_Types.h"
#include "IMG_Line.h"

/**
 * Implements rectangle functionality.
 * Used for bounds in images.
 * The four extreme coordinates are stored as left, top, right and bottom.
 * left is assumed to be smaller than or equal to right.
 * top is assumed to be smaller than or equal to bottom.
 * @author	Maarten Gribnau
 * @date	March 6, 2001
 */

class IMG_Rect {
public:

	/**
	 * Constructs a rectangle with the given values.
	 * @param	l	requested left coordinate of the rectangle
	 * @param	t	requested top coordinate of the rectangle
	 * @param	r	requested right coordinate of the rectangle
	 * @param	b	requested bottom coordinate of the rectangle
	 */
	IMG_Rect(TInt32 l=0, TInt32 t=0, TInt32 r=0, TInt32 b=0)
		: m_l(l), m_t(t), m_r(r), m_b(b) {}

	/**
	 * Copy constructor.
	 * @param	r	rectangle to copy
	 */
	IMG_Rect(const IMG_Rect& r)
		: m_l(r.m_l), m_t(r.m_t), m_r(r.m_r), m_b(r.m_b) {}
	
	/**
	 * Destructor.
	 */
	virtual ~IMG_Rect() {};

	/**
	 * Access to rectangle width.
	 * @return	width of the rectangle
	 */
	virtual inline TInt32 getWidth() const;

	/**
	 * Access to rectangle height.
	 * @return	height of the rectangle
	 */
	virtual inline TInt32 getHeight() const;

	/**
	 * Sets all members of the rectangle.
	 * @param	l	requested left coordinate of the rectangle
	 * @param	t	requested top coordinate of the rectangle
	 * @param	r	requested right coordinate of the rectangle
	 * @param	b	requested bottom coordinate of the rectangle
	 */
	virtual inline void set(TInt32 l, TInt32 t, TInt32 r, TInt32 b);

	/**
	 * Returns whether this rectangle is empty.
	 * Empty rectangles are rectangles that have width==0 and/or height==0.
	 * @return	boolean value (true==empty rectangle)
	 */
	virtual inline bool isEmpty() const;

	/**
	 * Returns whether the point is inside this rectangle.
	 * Point on the boundary is considered inside.
	 * @param x	x-coordinate of point to test.
	 * @param y y-coordinate of point to test.
	 * @return boolean value (true if point is inside).
	 */
	virtual inline bool isInside(TInt32 x, TInt32 y) const;

	/**
	 * Returns whether the rectangle is inside this rectangle.
	 * @param	r	rectangle to test.
	 * @return	visibility (not, partially or fully visible).
	 */
	virtual TVisibility getVisibility(IMG_Rect& r) const;

	/**
	 * Returns whether the line is inside this rectangle.
	 * @param l	line to test.
	 * @return	visibility (not, partially or fully visible).
	 */
	virtual TVisibility getVisibility(IMG_Line& l) const;

	/**
	 * Sets rectangle members.
	 * Sets rectangle members such that it is centered at the given location.
	 * @param	cx	requested center x-coordinate of the rectangle
	 * @param	cy	requested center y-coordinate of the rectangle
	 */
	virtual	void setCenter(TInt32 cx, TInt32 cy);

	/**
	 * Sets rectangle members.
	 * Sets rectangle members such that it is centered at the given location,
	 * with the width requested.
	 * @param	cx	requested center x-coordinate of the rectangle
	 * @param	cy	requested center y-coordinate of the rectangle
	 * @param	w	requested width of the rectangle
	 * @param	h	requested height of the rectangle
	 */
	virtual	void setCenter(TInt32 cx, TInt32 cy, TInt32 w, TInt32 h);

	/**
	 * Clips a rectangle.
	 * Updates the rectangle given such that it will fit within this one.
	 * This can result in an empty rectangle.
	 * @param	r	the rectangle to clip
	 * @return	whether clipping has occurred
	 */
	virtual	bool clip(IMG_Rect& r) const;

	/**
	 * Clips a line.
	 * Updates the line given such that it will fit within this rectangle.
	 * This can result in an empty line.
	 * @param	l	the line to clip
	 * @return	whether clipping has occurred
	 */
	virtual	bool clip(IMG_Line& l) const;

	/** Left coordinate of the rectangle */
	TInt32 m_l;
	/** Top coordinate of the rectangle */
	TInt32 m_t;
	/** Right coordinate of the rectangle */
	TInt32 m_r;
	/** Bottom coordinate of the rectangle */
	TInt32 m_b;
};


inline TInt32 IMG_Rect::getWidth() const
{
	return m_r - m_l;
}

inline TInt32 IMG_Rect::getHeight() const
{
	return m_b - m_t;
}

inline void IMG_Rect::set(TInt32 l, TInt32 t, TInt32 r, TInt32 b)
{
	m_l = l; m_t = t; m_r = r; m_b = b;
}

inline bool	IMG_Rect::isEmpty() const
{
	return (getWidth() == 0) || (getHeight() == 0);
}

inline bool IMG_Rect::isInside(TInt32 x, TInt32 y) const
{
	return (x >= m_l) && (x <= m_r) && (y >= m_t) && (y <= m_b);
}

#endif // _H_IMG_Rect

