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

/** \file FilterSource.h
 *  \ingroup bgevideotex
 */

#ifndef __FILTERSOURCE_H__
#define __FILTERSOURCE_H__

#include "Common.h"

#include "FilterBase.h"

/// class for RGB24 conversion
class FilterRGB24 : public FilterBase
{
public:
	/// constructor
	FilterRGB24 (void) {}
	/// destructor
	virtual ~FilterRGB24 (void) {}

	/// get source pixel size
	virtual unsigned int getPixelSize (void) { return 3; }

protected:
	/// filter pixel, source byte buffer
	virtual unsigned int filter (unsigned char *src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val)
	{ VT_RGBA(val,src[0],src[1],src[2],0xFF); return val; }
};

/// class for RGBA32 conversion
class FilterRGBA32 : public FilterBase
{
public:
	/// constructor
	FilterRGBA32 (void) {}
	/// destructor
	virtual ~FilterRGBA32 (void) {}

	/// get source pixel size
	virtual unsigned int getPixelSize (void) { return 4; }

protected:
	/// filter pixel, source byte buffer
	virtual unsigned int filter (unsigned char *src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val)
	{ 
		if ((intptr_t(src)&0x3) == 0) 
			return *(unsigned int*)src;
		else 
		{
			VT_RGBA(val,src[0],src[1],src[2],src[3]); 
			return val; 
		}
	}
};

/// class for BGR24 conversion
class FilterBGR24 : public FilterBase
{
public:
	/// constructor
	FilterBGR24 (void) {}
	/// destructor
	virtual ~FilterBGR24 (void) {}

	/// get source pixel size
	virtual unsigned int getPixelSize (void) { return 3; }

protected:
	/// filter pixel, source byte buffer
	virtual unsigned int filter (unsigned char *src, short x, short y,
	                             short * size, unsigned int pixSize, unsigned int val)
	{ VT_RGBA(val,src[2],src[1],src[0],0xFF); return val; }
};

/// class for Z_buffer conversion
class FilterZZZA : public FilterBase
{
public:
	/// constructor
	FilterZZZA (void) {}
	/// destructor
	virtual ~FilterZZZA (void) {}

	/// get source pixel size
	virtual unsigned int getPixelSize (void) { return 1; }

protected:
	/// filter pixel, source float buffer
	virtual unsigned int filter (float *src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val)
	{
		// calculate gray value
		// convert float to unsigned char
		unsigned int depth = int(src[0] * 255);
		// return depth scale value
		VT_R(val) = depth;
		VT_G(val) = depth;
		VT_B(val) = depth;
		VT_A(val) = 0xFF;

		return val;
	}
};


/// class for Z_buffer conversion
class FilterDEPTH : public FilterBase
{
public:
	/// constructor
	FilterDEPTH (void) {}
	/// destructor
	virtual ~FilterDEPTH (void) {}

	/// get source pixel size
	virtual unsigned int getPixelSize (void) { return 1; }

protected:
	/// filter pixel, source float buffer
	virtual unsigned int filter (float *src, short x, short y,
	                             short *size, unsigned int pixSize, unsigned int val)
	{
		/* Copy the float value straight away
		 * The user can retrieve the original float value by using
		 * 'F' mode in BGL buffer */
		memcpy(&val, src, sizeof (unsigned int));
		return val;
	}
};




/// class for YV12 conversion
class FilterYV12 : public FilterBase
{
public:
	/// constructor
	FilterYV12 (void): m_buffV(NULL), m_buffU(NULL), m_pitchUV(0) {}
	/// destructor
	virtual ~FilterYV12 (void) {}

	/// get source pixel size
	virtual unsigned int getPixelSize (void) { return 1; }

	/// set pointers to color buffers
	void setBuffs (unsigned char * buff, short * size)
	{
		unsigned int buffSize = size[0] * size[1];
		m_buffV = buff + buffSize;
		m_buffU = m_buffV + (buffSize >> 2);
		m_pitchUV = size[0] >> 1;
	}

protected:
	/// begin of V buffer
	unsigned char * m_buffV;
	/// begin of U buffer
	unsigned char * m_buffU;
	/// pitch for V & U buffers
	short m_pitchUV;

	/// interpolation function
	int interpol (int a, int b, int c, int d)
	{ return (9 * (b + c) - a - d + 8) >> 4; }

	/// common horizontal interpolation
	int interpolH (unsigned char *src)
	{ return interpol(*(src-1), *src, *(src+1), *(src+2)); }

	/// common vertical interpolation
	int interpolV (unsigned char *src)
	{ return interpol(*(src-m_pitchUV), *src, *(src+m_pitchUV), *(src+2*m_pitchUV)); }

	/// common joined vertical and horizontal interpolation
	int interpolVH (unsigned char *src)
	{
		return interpol(interpolV(src-1), interpolV(src), interpolV(src+1),
		                interpolV(src+2));
	}

	/// is pixel on edge
	bool isEdge (short x, short y, const short size[2])
	{ return x <= 1 || x >= size[0] - 4 || y <= 1 || y >= size[1] - 4; }

	/// get the first parameter on the low edge
	unsigned char * interParA (unsigned char *src, short x, short size, short shift)
	{ return x > 1 ? src - shift : src; }
	/// get the third parameter on the high edge
	unsigned char * interParC (unsigned char *src, short x, short size, short shift)
	{ return x < size - 2 ? src + shift : src; }
	/// get the fourth parameter on the high edge
	unsigned char * interParD (unsigned char *src, short x, short size, short shift)
	{ return x < size - 4 ? src + 2 * shift : x < size - 2 ? src + shift : src; }

	/// horizontal interpolation on edges
	int interpolEH (unsigned char *src, short x, short size)
	{
		return interpol(*interParA(src, x, size, 1), *src,
		                *interParC(src, x, size, 1), *interParD(src, x, size, 1));
	}

	/// vertical interpolation on edges
	int interpolEV (unsigned char *src, short y, short size)
	{
		return interpol(*interParA(src, y, size, m_pitchUV), *src,
		                *interParC(src, y, size, m_pitchUV), *interParD(src, y, size, m_pitchUV));
	}

	/// joined vertical and horizontal interpolation on edges
	int interpolEVH (unsigned char *src, short x, short y, short * size)
	{
		return interpol(interpolEV(interParA(src, x, size[0], 1), y, size[1]),
		        interpolEV(src, y, size[1]), interpolEV(interParC(src, x, size[0], 1), y, size[1]),
		        interpolEV(interParD(src, x, size[0], 1), y, size[1]));
	}


	/// filter pixel, source byte buffer
	virtual unsigned int filter (unsigned char *src, short x, short y,
		short * size, unsigned int pixSize, unsigned int val)
	{
		// V & U offset
		long offset = (x >> 1) + m_pitchUV * (y >> 1);
		// get modified YUV -> CDE: C = Y - 16; D = U - 128; E = V - 128
		int c = *src - 16;
		int d = m_buffU[offset] - 128;
		int e = m_buffV[offset] - 128;
		// if horizontal interpolation is needed
		if ((x & 1) == 1) {
			// if vertical interpolation is needed too
			if ((y & 1) == 1)
			{
				// if this pixel is on the edge
				if (isEdge(x, y, size))
				{
					// get U & V from edge
					d = interpolEVH(m_buffU + offset, x, y, size) - 128;
					e = interpolEVH(m_buffV + offset, x, y, size) - 128;
				}
				// otherwise get U & V from inner range
				else
				{
					d = interpolVH(m_buffU + offset) - 128;
					e = interpolVH(m_buffV + offset) - 128;
				}
				// otherwise use horizontal interpolation only
			}
			else {
				// if this pixel is on the edge
				if (isEdge(x, y, size))
				{
					// get U & V from edge
					d = interpolEH(m_buffU + offset, x, size[0]) - 128;
					e = interpolEH(m_buffV + offset, x, size[0]) - 128;
				}
				// otherwise get U & V from inner range
				else
				{
					d = interpolH(m_buffU + offset) - 128;
					e = interpolH(m_buffV + offset) - 128;
				}
				// otherwise if only vertical interpolation is needed
			}
		}
		else if ((y & 1) == 1) {
			// if this pixel is on the edge
			if (isEdge(x, y, size))
			{
				// get U & V from edge
				d = interpolEV(m_buffU + offset, y, size[1]) - 128;
				e = interpolEV(m_buffV + offset, y, size[1]) - 128;
			}
			// otherwise get U & V from inner range
			else
			{
				d = interpolV(m_buffU + offset) - 128;
				e = interpolV(m_buffV + offset) - 128;
			}
		}
		// convert to RGB
		// R = clip(( 298 * C           + 409 * E + 128) >> 8)
		// G = clip(( 298 * C - 100 * D - 208 * E + 128) >> 8)
		// B = clip(( 298 * C + 516 * D           + 128) >> 8)
		int red = (298 * c + 409 * e + 128) >> 8;
		if (red >= 0x100) red = 0xFF;
		else if (red < 0) red = 0;
		int green = (298 * c - 100 * d - 208 * e) >> 8;
		if (green >= 0x100) green = 0xFF;
		else if (green < 0) green = 0;
		int blue = (298 * c + 516 * d + 128) >> 8;
		if (blue >= 0x100) blue = 0xFF;
		else if (blue < 0) blue = 0;
		// return result
		VT_RGBA(val, red, green, blue, 0xFF);
		return val;
	}
};

#endif  /* __FILTERSOURCE_H__ */
