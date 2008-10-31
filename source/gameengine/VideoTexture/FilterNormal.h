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

#if !defined FILTERNORMAL_H
#define FILTERNORMAL_H

#include "Common.h"

#include "FilterBase.h"


// scale constants for normals
const float depthScaleKoef = 255.0;
const float normScaleKoef = float(depthScaleKoef / 2.0);


/// pixel filter for normal mapping
class FilterNormal : public FilterBase
{
public:
	/// constructor
	FilterNormal (void);
	/// destructor
	virtual ~FilterNormal (void) {}

	/// get index of color used to calculate normals
	unsigned short getColor (void) { return m_colShift >> 3; }
	/// set index of color used to calculate normals
	void setColor (unsigned short colIdx);

	/// get depth
	float getDepth (void) { return m_depth; }
	/// set depth
	void setDepth (float depth);

protected:
	/// depth of normal relief
	float m_depth;
	/// scale to calculate normals
	float m_depthScale;

	/// shift to used color component
	unsigned short m_colShift;

	/// filter pixel, source int buffer
	template <class SRC> unsigned int tFilter (SRC * src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val = 0)
	{
		// get value of required color
		int actPix = int((val >> m_colShift) & 0xFF);
		// get upper and left pixel from actual pixel
		int upPix = y > 0 ? int((convertPrevious(src - pixSize * size[0], x, y - 1,
			size, pixSize) >> m_colShift) & 0xFF) : actPix;
		int leftPix = x > 0 ? int((convertPrevious(src - pixSize, x - 1, y, size, pixSize)
			>> m_colShift) & 0xFF) : actPix;
		// height differences (from blue color)
		float dx = (actPix - leftPix) * m_depthScale;
		float dy = (actPix - upPix) * m_depthScale;
		// normalize vector
		float dz = float(normScaleKoef / sqrt(dx * dx + dy * dy + 1.0));
		dx = dx * dz + normScaleKoef;
		dy = dy * dz + normScaleKoef;
		dz += normScaleKoef;
		// return normal vector converted to color
		return 0xFF000000 | int(dz) << 16 | int(dy) << 8 | int(dx);
	}

	/// filter pixel, source byte buffer
	virtual unsigned int filter (unsigned char * src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val = 0)
	{ return tFilter(src, x, y, size, pixSize, val); }
	/// filter pixel, source int buffer
	virtual unsigned int filter (unsigned int * src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val = 0)
	{ return tFilter(src, x, y, size, pixSize, val); }
};


#endif