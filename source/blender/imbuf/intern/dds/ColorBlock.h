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

#ifndef _DDS_COLORBLOCK_H
#define _DDS_COLORBLOCK_H

#include <Color.h>
#include <Image.h>

/// Uncompressed 4x4 color block.
struct ColorBlock
{
	ColorBlock();
	ColorBlock(const ColorBlock & block);
	ColorBlock(const Image * img, unsigned int x, unsigned int y);
	
	void init(const Image * img, unsigned int x, unsigned int y);
	
	void swizzleDXT5n();
	void splatX();
	void splatY();
	
	unsigned int countUniqueColors() const;
	Color32 averageColor() const;
	
	void diameterRange(Color32 * start, Color32 * end) const;
	void luminanceRange(Color32 * start, Color32 * end) const;
	void boundsRange(Color32 * start, Color32 * end) const;
	void boundsRangeAlpha(Color32 * start, Color32 * end) const;
	void bestFitRange(Color32 * start, Color32 * end) const;
	
	void sortColorsByAbsoluteValue();

	float volume() const;

	// Accessors
	const Color32 * colors() const;

	Color32 color(unsigned int i) const;
	Color32 & color(unsigned int i);
	
	Color32 color(unsigned int x, unsigned int y) const;
	Color32 & color(unsigned int x, unsigned int y);
	
private:
	
	Color32 m_color[4*4];
	
};


/// Get pointer to block colors.
inline const Color32 * ColorBlock::colors() const
{
	return m_color;
}

/// Get block color.
inline Color32 ColorBlock::color(unsigned int i) const
{
	return m_color[i];
}

/// Get block color.
inline Color32 & ColorBlock::color(unsigned int i)
{
	return m_color[i];
}

/// Get block color.
inline Color32 ColorBlock::color(unsigned int x, unsigned int y) const
{
	return m_color[y * 4 + x];
}

/// Get block color.
inline Color32 & ColorBlock::color(unsigned int x, unsigned int y)
{
	return m_color[y * 4 + x];
}

#endif // _DDS_COLORBLOCK_H
