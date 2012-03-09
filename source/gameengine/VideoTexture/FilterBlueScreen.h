/*
-----------------------------------------------------------------------------
This source file is part of blendTex library

Copyright (c) 2007 The Zdeno Ash Miklas

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place - Suite 330, Boston, MA 02111-1307, USA, or go to
http://www.gnu.org/copyleft/lesser.txt.
-----------------------------------------------------------------------------
*/

/** \file FilterBlueScreen.h
 *  \ingroup bgevideotex
 */
 
#ifndef FILTERBLUESCREEN_H
#define FILTERBLUESCREEN_H

#include "Common.h"

#include "FilterBase.h"


/// pixel filter for blue screen
class FilterBlueScreen : public FilterBase
{
public:
	/// constructor
	FilterBlueScreen (void);
	/// destructor
	virtual ~FilterBlueScreen (void) {}

	/// get color
	unsigned char * getColor (void) { return m_color; }
	/// set color
	void setColor (unsigned char red, unsigned char green, unsigned char blue);

	/// get limits for color variation
	unsigned short * getLimits (void) { return m_limits; }
	/// set limits for color variation
	void setLimits (unsigned short minLimit, unsigned short maxLimit);

protected:
	///  blue screen color (red component first)
	unsigned char m_color[3];
	/// limits for color variation - first defines, where ends fully transparent
	/// color, second defines, where begins fully opaque color
	unsigned short m_limits[2];
	/// squared limits for color variation
	unsigned int m_squareLimits[2];
	/// distance between squared limits
	unsigned int m_limitDist;

	/// filter pixel template, source int buffer
	template <class SRC> unsigned int tFilter (SRC src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val)
	{
		// calculate differences
		int difRed = int(VT_R(val)) - int(m_color[0]);
		int difGreen = int(VT_G(val)) - int(m_color[1]);
		int difBlue = int(VT_B(val)) - int(m_color[2]);
		// calc distance from "blue screen" color
		unsigned int dist = (unsigned int)(difRed * difRed + difGreen * difGreen
			+ difBlue * difBlue);
		// condition for fully transparent color
		if (m_squareLimits[0] >= dist) 
			// return color with zero alpha
			VT_A(val) = 0;
		// condition for fully opaque color
		else if (m_squareLimits[1] <= dist)
			// return normal color
			VT_A(val) = 0xFF;
		// otherwise calc alpha
		else
			VT_A(val) = (((dist - m_squareLimits[0]) << 8) / m_limitDist);
		return val;
	}

	/// virtual filtering function for byte source
	virtual unsigned int filter (unsigned char * src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val = 0)
	{ return tFilter(src, x, y, size, pixSize, val); }
	/// virtual filtering function for unsigned int source
	virtual unsigned int filter (unsigned int * src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val = 0)
	{ return tFilter(src, x, y, size, pixSize, val); }
};


#endif
