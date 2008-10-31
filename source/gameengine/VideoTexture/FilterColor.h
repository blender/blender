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
		unsigned int gray = (28 * ((val >> 16) & 0xFF) + 151 * ((val >> 8) & 0xFF)
			+ 77 * (val & 0xFF)) & 0xFF00;
		// return gray scale value
		return (val & 0xFF000000) | gray << 8 | gray | gray >> 8;
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
	unsigned int calcColor (unsigned int val, short idx)
	{
		return (((m_matrix[idx][0]  * (val & 0xFF) + m_matrix[idx][1] * ((val >> 8) & 0xFF)
			+ m_matrix[idx][2] * ((val >> 16) & 0xFF) + m_matrix[idx][3] * ((val >> 24) & 0xFF)
			+ m_matrix[idx][4]) >> 8) & 0xFF) << (idx << 3);
	}

	/// filter pixel template, source int buffer
	template <class SRC> unsigned int tFilter (SRC src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val)
	{
		// return calculated color
		return calcColor(val, 0) | calcColor(val, 1) | calcColor(val, 2)
			| calcColor(val, 3);
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
typedef unsigned long ColorLevel[4][3];

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
		unsigned int col = val & (0xFF << (idx << 3));
		if (col <= levels[idx][0]) col = 0;
		else if (col >= levels[idx][1]) col = 0xFF << (idx << 3);
		else if (idx < 3) col = (((col - levels[idx][0]) << 8) / levels[idx][2]) & (0xFF << (idx << 3));
		else col = (((col - levels[idx][0]) / levels[idx][2]) << 8) & (0xFF << (idx << 3));
		return col; 
	}

	/// filter pixel template, source int buffer
	template <class SRC> unsigned int tFilter (SRC src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val)
	{
		// return calculated color
		return calcColor(val, 0) | calcColor(val, 1) | calcColor(val, 2)
			| calcColor(val, 3);
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