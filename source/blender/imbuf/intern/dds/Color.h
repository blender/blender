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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributors: Amorilia (amorilia@gamebox.net)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*
 * This file is based on a similar file from the NVIDIA texture tools
 * (http://nvidia-texture-tools.googlecode.com/)
 *
 * Original license from NVIDIA follows.
 */

// This code is in the public domain -- castanyo@yahoo.es

#ifndef _DDS_COLOR_H
#define _DDS_COLOR_H

/// 32 bit color stored as BGRA.
class Color32
{
public:
	Color32() { }
	Color32(const Color32 & c) : u(c.u) { }
	Color32(unsigned char R, unsigned char G, unsigned char B) { setRGBA(R, G, B, 0xFF); }
	Color32(unsigned char R, unsigned char G, unsigned char B, unsigned char A) { setRGBA( R, G, B, A); }
	//Color32(unsigned char c[4]) { setRGBA(c[0], c[1], c[2], c[3]); }
	//Color32(float R, float G, float B) { setRGBA(uint(R*255), uint(G*255), uint(B*255), 0xFF); }
	//Color32(float R, float G, float B, float A) { setRGBA(uint(R*255), uint(G*255), uint(B*255), uint(A*255)); }
	Color32(unsigned int U) : u(U) { }

	void setRGBA(unsigned char R, unsigned char G, unsigned char B, unsigned char A)
	{
		r = R;
		g = G;
		b = B;
		a = A;
	}

	void setBGRA(unsigned char B, unsigned char G, unsigned char R, unsigned char A = 0xFF)
	{
		r = R;
		g = G;
		b = B;
		a = A;
	}

	operator unsigned int () const {
		return u;
	}
	
	union {
		struct {
			unsigned char b, g, r, a;
		};
		unsigned int u;
	};
};

/// 16 bit 565 BGR color.
class Color16
{
public:
	Color16() { }
	Color16(const Color16 & c) : u(c.u) { }
	explicit Color16(unsigned short U) : u(U) { }
	
	union {
		struct {
			unsigned short b : 5;
			unsigned short g : 6;
			unsigned short r : 5;
		};
		unsigned short u;
	};
};

#endif // _DDS_COLOR_H
