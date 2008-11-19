/* $Id$
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

#if !defined FILTERCOLOR_H
#define FILTERCOLOR_H

#include "Common.h"

#include "FilterBase.h"


/// pixel filter for gray scale
class FilterGray : public FilterBase
{
public:
	/// constructor
	FilterGray (void) {}
	/// destructor
	virtual ~FilterGray (void) {}

protected:
	/// filter pixel template, source int buffer
	template <class SRC> unsigned int tFilter (SRC src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val)
	{
		// calculate gray value
		unsigned int gray = (28 * (VT_B(val)) + 151 * (VT_G(val))
			+ 77 * (VT_R(val))) >> 8;
		// return gray scale value
		VT_R(val) = gray;
		VT_G(val) = gray;
		VT_B(val) = gray;
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


/// type for color matrix
typedef short ColorMatrix[4][5];

/// pixel filter for color calculation
class FilterColor : public FilterBase
{
public:
	/// constructor
	FilterColor (void);
	/// destructor
	virtual ~FilterColor (void) {}

	/// get color matrix
	ColorMatrix & getMatrix (void) { return m_matrix; }
	/// set color matrix
	void setMatrix (ColorMatrix & mat);

protected:
	///  color calculation matrix
	ColorMatrix m_matrix;

	/// calculate one color component
	unsigned char calcColor (unsigned int val, short idx)
	{
		return (((m_matrix[idx][0]  * (VT_R(val)) + m_matrix[idx][1] * (VT_G(val))
			+ m_matrix[idx][2] * (VT_B(val)) + m_matrix[idx][3] * (VT_A(val))
			+ m_matrix[idx][4]) >> 8) & 0xFF);
	}

	/// filter pixel template, source int buffer
	template <class SRC> unsigned int tFilter (SRC src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val)
	{
		// return calculated color
		int color;
		VT_RGBA(color, calcColor(val, 0), calcColor(val, 1), calcColor(val, 2), calcColor(val, 3));
		return color;
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


/// type for color levels
typedef unsigned short ColorLevel[4][3];

/// pixel filter for color calculation
class FilterLevel : public FilterBase
{
public:
	/// constructor
	FilterLevel (void);
	/// destructor
	virtual ~FilterLevel (void) {}

	/// get color matrix
	ColorLevel & getLevels (void) { return levels; }
	/// set color matrix
	void setLevels (ColorLevel & lev);

protected:
	///  color calculation matrix
	ColorLevel levels;

	/// calculate one color component
	unsigned int calcColor (unsigned int val, short idx)
	{
		unsigned int col = VT_C(val,idx);;
		if (col <= levels[idx][0]) col = 0;
		else if (col >= levels[idx][1]) col = 0xFF;
		else col = (((col - levels[idx][0]) << 8) / levels[idx][2]) & 0xFF;
		return col; 
	}

	/// filter pixel template, source int buffer
	template <class SRC> unsigned int tFilter (SRC src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val)
	{
		// return calculated color
		int color;
		VT_RGBA(color, calcColor(val, 0), calcColor(val, 1), calcColor(val, 2), calcColor(val, 3));
		return color;
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
