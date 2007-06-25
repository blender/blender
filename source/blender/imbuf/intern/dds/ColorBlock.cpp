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
 * Contributors: Amorilia (amorilia@gamebox.net)
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/*
 * This file is based on a similar file from the NVIDIA texture tools
 * (http://nvidia-texture-tools.googlecode.com/)
 *
 * Original license from NVIDIA follows.
 */

// This code is in the public domain -- castanyo@yahoo.es

#include <ColorBlock.h>
#include <Image.h>
#include <Common.h>

	// Get approximate luminance.
	inline static unsigned int colorLuminance(Color32 c)
	{
		return c.r + c.g + c.b;
	}
	
	// Get the euclidean distance between the given colors.
	inline static unsigned int colorDistance(Color32 c0, Color32 c1)
	{
		return (c0.r - c1.r) * (c0.r - c1.r) + (c0.g - c1.g) * (c0.g - c1.g) + (c0.b - c1.b) * (c0.b - c1.b);
	}
	

/// Default constructor.
ColorBlock::ColorBlock()
{
}

/// Init the color block with the contents of the given block.
ColorBlock::ColorBlock(const ColorBlock & block)
{
	for(unsigned int i = 0; i < 16; i++) {
		color(i) = block.color(i);
	}
}


/// Initialize this color block.
ColorBlock::ColorBlock(const Image * img, unsigned int x, unsigned int y)
{
	init(img, x, y);
}

void ColorBlock::init(const Image * img, unsigned int x, unsigned int y)
{
	const unsigned int bw = min(img->width() - x, 4U);
	const unsigned int bh = min(img->height() - y, 4U);

	static int remainder[] = {
		0, 0, 0, 0,
		0, 1, 0, 1,
		0, 1, 2, 0,
		0, 1, 2, 3,
	};

	// Blocks that are smaller than 4x4 are handled by repeating the pixels.
	// @@ Thats only correct when block size is 1, 2 or 4, but not with 3. :(

	for(unsigned int i = 0; i < 4; i++) {
		//const int by = i % bh;
		const int by = remainder[(bh - 1) * 4 + i];
		for(unsigned int e = 0; e < 4; e++) {
			//const int bx = e % bw;
			const int bx = remainder[(bw - 1) * 4 + e];
			color(e, i) = img->pixel(x + bx, y + by);
		}
	}
}


void ColorBlock::swizzleDXT5n()
{
	for(int i = 0; i < 16; i++)
	{
		Color32 c = m_color[i];
		m_color[i] = Color32(0, c.g, 0, c.r);
	}
}

void ColorBlock::splatX()
{
	for(int i = 0; i < 16; i++)
	{
		unsigned char x = m_color[i].r;
		m_color[i] = Color32(x, x, x, x);
	}
}

void ColorBlock::splatY()
{
	for(int i = 0; i < 16; i++)
	{
		unsigned char y = m_color[i].g;
		m_color[i] = Color32(y, y, y, y);
	}
}


/// Count number of unique colors in this color block.
unsigned int ColorBlock::countUniqueColors() const
{
	unsigned int count = 0;

	// @@ This does not have to be o(n^2)
	for(int i = 0; i < 16; i++)
	{
		bool unique = true;
		for(int j = 0; j < i; j++) {
			if( m_color[i] != m_color[j] ) {
				unique = false;
			}
		}
		
		if( unique ) {
			count++;
		}
	}
	
	return count;
}

/// Get average color of the block.
Color32 ColorBlock::averageColor() const
{
	unsigned int r, g, b, a;
	r = g = b = a = 0;

	for(unsigned int i = 0; i < 16; i++) {
		r += m_color[i].r;
		g += m_color[i].g;
		b += m_color[i].b;
		a += m_color[i].a;
	}
	
	return Color32((unsigned char)(r / 16), (unsigned char)(g / 16), (unsigned char)(b / 16), (unsigned char)(a / 16));
}


/// Get diameter color range.
void ColorBlock::diameterRange(Color32 * start, Color32 * end) const
{
	Color32 c0, c1;
	unsigned int best_dist = 0;
	
	for(int i = 0; i < 16; i++) {
		for (int j = i+1; j < 16; j++) {
			unsigned int dist = colorDistance(m_color[i], m_color[j]);
			if( dist > best_dist ) {
				best_dist = dist;
				c0 = m_color[i];
				c1 = m_color[j];
			}
		}
	}
	
	*start = c0;
	*end = c1;
}

/// Get luminance color range.
void ColorBlock::luminanceRange(Color32 * start, Color32 * end) const
{
	Color32 minColor, maxColor;
	unsigned int minLuminance, maxLuminance;
	
	maxLuminance = minLuminance = colorLuminance(m_color[0]);
	
	for(unsigned int i = 1; i < 16; i++)
	{
		unsigned int luminance = colorLuminance(m_color[i]);
		
		if (luminance > maxLuminance) {
			maxLuminance = luminance;
			maxColor = m_color[i];
		}
		else if (luminance < minLuminance) {
			minLuminance = luminance;
			minColor = m_color[i];
		}
	}

	*start = minColor;
	*end = maxColor;
}

/// Get color range based on the bounding box. 
void ColorBlock::boundsRange(Color32 * start, Color32 * end) const
{
	Color32 minColor(255, 255, 255);
	Color32 maxColor(0, 0, 0);

	for(unsigned int i = 0; i < 16; i++)
	{
		if (m_color[i].r < minColor.r) { minColor.r = m_color[i].r; }
		if (m_color[i].g < minColor.g) { minColor.g = m_color[i].g; }
		if (m_color[i].b < minColor.b) { minColor.b = m_color[i].b; }
		if (m_color[i].r > maxColor.r) { maxColor.r = m_color[i].r; }
		if (m_color[i].g > maxColor.g) { maxColor.g = m_color[i].g; }
		if (m_color[i].b > maxColor.b) { maxColor.b = m_color[i].b; }
	}

	// Offset range by 1/16 of the extents
	Color32 inset;
	inset.r = (maxColor.r - minColor.r) >> 4;
	inset.g = (maxColor.g - minColor.g) >> 4;
	inset.b = (maxColor.b - minColor.b) >> 4;

	minColor.r = (minColor.r + inset.r <= 255) ? minColor.r + inset.r : 255;
	minColor.g = (minColor.g + inset.g <= 255) ? minColor.g + inset.g : 255;
	minColor.b = (minColor.b + inset.b <= 255) ? minColor.b + inset.b : 255;

	maxColor.r = (maxColor.r >= inset.r) ? maxColor.r - inset.r : 0;
	maxColor.g = (maxColor.g >= inset.g) ? maxColor.g - inset.g : 0;
	maxColor.b = (maxColor.b >= inset.b) ? maxColor.b - inset.b : 0;

	*start = minColor;
	*end = maxColor;
}

/// Get color range based on the bounding box. 
void ColorBlock::boundsRangeAlpha(Color32 * start, Color32 * end) const
{
	Color32 minColor(255, 255, 255, 255);
	Color32 maxColor(0, 0, 0, 0);

	for(unsigned int i = 0; i < 16; i++)
	{
		if (m_color[i].r < minColor.r) { minColor.r = m_color[i].r; }
		if (m_color[i].g < minColor.g) { minColor.g = m_color[i].g; }
		if (m_color[i].b < minColor.b) { minColor.b = m_color[i].b; }
		if (m_color[i].a < minColor.a) { minColor.a = m_color[i].a; }
		if (m_color[i].r > maxColor.r) { maxColor.r = m_color[i].r; }
		if (m_color[i].g > maxColor.g) { maxColor.g = m_color[i].g; }
		if (m_color[i].b > maxColor.b) { maxColor.b = m_color[i].b; }
		if (m_color[i].a > maxColor.a) { maxColor.a = m_color[i].a; }
	}

	// Offset range by 1/16 of the extents
	Color32 inset;
	inset.r = (maxColor.r - minColor.r) >> 4;
	inset.g = (maxColor.g - minColor.g) >> 4;
	inset.b = (maxColor.b - minColor.b) >> 4;
	inset.a = (maxColor.a - minColor.a) >> 4;

	minColor.r = (minColor.r + inset.r <= 255) ? minColor.r + inset.r : 255;
	minColor.g = (minColor.g + inset.g <= 255) ? minColor.g + inset.g : 255;
	minColor.b = (minColor.b + inset.b <= 255) ? minColor.b + inset.b : 255;
	minColor.a = (minColor.a + inset.a <= 255) ? minColor.a + inset.a : 255;

	maxColor.r = (maxColor.r >= inset.r) ? maxColor.r - inset.r : 0;
	maxColor.g = (maxColor.g >= inset.g) ? maxColor.g - inset.g : 0;
	maxColor.b = (maxColor.b >= inset.b) ? maxColor.b - inset.b : 0;
	maxColor.a = (maxColor.a >= inset.a) ? maxColor.a - inset.a : 0;
	
	*start = minColor;
	*end = maxColor;
}

/// Sort colors by abosolute value in their 16 bit representation.
void ColorBlock::sortColorsByAbsoluteValue()
{
	// Dummy selection sort.
	for( unsigned int a = 0; a < 16; a++ ) {
		unsigned int max = a;
		Color16 cmax(m_color[a]);
		
		for( unsigned int b = a+1; b < 16; b++ ) {
			Color16 cb(m_color[b]);
			
			if( cb.u > cmax.u ) {
				max = b;
				cmax = cb;
			}
		}
		Color32 tmp;
		swap( m_color[a], m_color[max], tmp );
	}
}


