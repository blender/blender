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
 * along with this program; if not, write to the Free Software  Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Copyright (c) 2007 The Zdeno Ash Miklas
 *
 * This source file is part of blendTex library
 *
 * Contributor(s):
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file FilterNormal.h
 *  \ingroup bgevideotex
 */

#ifndef __FILTERNORMAL_H__
#define __FILTERNORMAL_H__

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
	unsigned short getColor (void) { return m_colIdx; }
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

	/// color index, 0=red, 1=green, 2=blue, 3=alpha
	unsigned short m_colIdx;

	/// filter pixel, source int buffer
	template <class SRC> unsigned int tFilter (SRC *src, short x, short y,
	                                           short * size, unsigned int pixSize, unsigned int val = 0)
	{
		// get value of required color
		int actPix = int(VT_C(val,m_colIdx));
		int upPix = actPix;
		int leftPix = actPix;
		// get upper and left pixel from actual pixel
		if (y > 0)
		{
			val = convertPrevious(src - pixSize * size[0], x, y - 1, size, pixSize);
			upPix = VT_C(val,m_colIdx);
		}
		if (x > 0)
		{
			val = convertPrevious(src - pixSize, x - 1, y, size, pixSize);
			leftPix = VT_C(val,m_colIdx);
		}
		// height differences (from blue color)
		float dx = (actPix - leftPix) * m_depthScale;
		float dy = (actPix - upPix) * m_depthScale;
		// normalize vector
		float dz = float(normScaleKoef / sqrt(dx * dx + dy * dy + 1.0));
		dx = dx * dz + normScaleKoef;
		dy = dy * dz + normScaleKoef;
		dz += normScaleKoef;
		// return normal vector converted to color
		VT_RGBA(val, dx, dy, dz, 0xFF);
		return val;
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
