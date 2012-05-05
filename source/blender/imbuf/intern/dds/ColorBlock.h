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
 * Contributors: Amorilia (amorilia@users.sourceforge.net)
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/imbuf/intern/dds/ColorBlock.h
 *  \ingroup imbdds
 */


/*
 * This file is based on a similar file from the NVIDIA texture tools
 * (http://nvidia-texture-tools.googlecode.com/)
 *
 * Original license from NVIDIA follows.
 */

// This code is in the public domain -- castanyo@yahoo.es

#ifndef __COLORBLOCK_H__
#define __COLORBLOCK_H__

#include <Color.h>
#include <Image.h>

/// Uncompressed 4x4 color block.
struct ColorBlock
{
	ColorBlock();
	ColorBlock(const uint * linearImage);
	ColorBlock(const ColorBlock & block);
	ColorBlock(const Image * img, uint x, uint y);
	
	void init(const Image * img, uint x, uint y);
	void init(uint w, uint h, const uint * data, uint x, uint y);
	void init(uint w, uint h, const float * data, uint x, uint y);
	
	void swizzle(uint x, uint y, uint z, uint w); // 0=r, 1=g, 2=b, 3=a, 4=0xFF, 5=0
	
	bool isSingleColor(Color32 mask = Color32(0xFF, 0xFF, 0xFF, 0x00)) const;
	bool hasAlpha() const;
	
	
	// Accessors
	const Color32 * colors() const;

	Color32 color(uint i) const;
	Color32 & color(uint i);
	
	Color32 color(uint x, uint y) const;
	Color32 & color(uint x, uint y);
	
private:
	
	Color32 m_color[4*4];
	
};


/// Get pointer to block colors.
inline const Color32 * ColorBlock::colors() const
{
	return m_color;
}

/// Get block color.
inline Color32 ColorBlock::color(uint i) const
{
	return m_color[i];
}

/// Get block color.
inline Color32 & ColorBlock::color(uint i)
{
	return m_color[i];
}

/// Get block color.
inline Color32 ColorBlock::color(uint x, uint y) const
{
	return m_color[y * 4 + x];
}

/// Get block color.
inline Color32 & ColorBlock::color(uint x, uint y)
{
	return m_color[y * 4 + x];
}

#endif // __COLORBLOCK_H__
