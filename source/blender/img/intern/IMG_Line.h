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
 * @date	March 12, 2001
 */

#ifndef _H_IMG_Line
#define _H_IMG_Line

#include "IMG_Types.h"
#include <math.h>

/**
 * A line from a start to an end point.
 * Used for drawing lines in images.
 * @author	Maarten Gribnau
 * @date	March 6, 2001
 */

class IMG_Line {
public:

	/**
	 * Constructs a line with the given values.
	 * @param	xs	start point x-coordinate
	 * @param	ys	start point y-coordinate
	 * @param	xe	end point x-coordinate
	 * @param	ye	end point y-coordinate
	 */
	IMG_Line(TInt32 xs=0, TInt32 ys=0, TInt32 xe=0, TInt32 ye=0)
		: m_xs(xs), m_ys(ys), m_xe(xe), m_ye(ye) {}

	/**
	 * Copy constructor.
	 * @param	l	line to copy
	 */
	IMG_Line(const IMG_Line& l)
		: m_xs(l.m_xs), m_ys(l.m_ys), m_xe(l.m_xe), m_ye(l.m_ye) {}
	
	/**
	 * Destructor.
	 */
	virtual ~IMG_Line() {};

	/**
	 * Access to line length.
	 * @return	length of the line.
	 */
	virtual inline float getLength() const;

	/**
	 * Sets all members of the line.
	 * @param	xs	start point x-coordinate
	 * @param	ys	start point y-coordinate
	 * @param	xe	end point x-coordinate
	 * @param	ye	end point y-coordinate
	 */
	virtual inline void set(TInt32 xs, TInt32 ys, TInt32 xe, TInt32 ye);

	/**
	 * Returns whether this line is empty.
	 * Empty line are lines that have length==0.
	 * @return	boolean value (true==empty line)
	 */
	virtual inline bool isEmpty() const;

	/**
	 * Returns point at given value for line parameter.
	 * Calculates the coordinates of a point on the line.
	 * @param	t	line parameter value (0<=t<=1) of the point requested.
	 * @param	x	x-coordinate of the point.
	 * @param	y	y-coordinate of the point.
	 */
	virtual inline void getPoint(float t, TInt32& x, TInt32& y) const;

	/** Start point x-coordinate */
	TInt32 m_xs;
	/** Start point y-coordinate */
	TInt32 m_ys;
	/** End point x-coordinate */
	TInt32 m_xe;
	/** End point y-coordinate */
	TInt32 m_ye;
};


inline float IMG_Line::getLength() const
{
	TInt32 dx = m_xe - m_xs;
	TInt32 dy = m_ye - m_ys;
	return ((float)::sqrt(((float)dx)*dx + ((float)dy)*dy));
}


inline void IMG_Line::set(TInt32 xs, TInt32 ys, TInt32 xe, TInt32 ye)
{
	m_xs = xs; m_ys = ys; m_xe = xe; m_ye = ye;
}


inline bool	IMG_Line::isEmpty() const
{
	return (getLength() <= 0);
}


inline void IMG_Line::getPoint(float t, TInt32& x, TInt32& y) const
{
	x = (TInt32) (((float)m_xs) + (t * (m_xe - m_xs)));
	y = (TInt32) (((float)m_ys) + (t * (m_ye - m_ys)));
}

#endif // _H_IMG_Line

