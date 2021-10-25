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

/** \file blender/imbuf/intern/dds/ColorBlock.cpp
 *  \ingroup imbdds
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

#if 0
	// Get approximate luminance.
	inline static uint colorLuminance(Color32 c)
	{
		return c.r + c.g + c.b;
	}
	
	// Get the euclidean distance between the given colors.
	inline static uint colorDistance(Color32 c0, Color32 c1)
	{
		return (c0.r - c1.r) * (c0.r - c1.r) + (c0.g - c1.g) * (c0.g - c1.g) + (c0.b - c1.b) * (c0.b - c1.b);
	}
#endif
	

/// Default constructor.
ColorBlock::ColorBlock()
{
}

/// Init the color block from an array of colors.
ColorBlock::ColorBlock(const uint *linearImage)
{
	for (uint i = 0; i < 16; i++) {
		color(i) = Color32(linearImage[i]);
	}
}

/// Init the color block with the contents of the given block.
ColorBlock::ColorBlock(const ColorBlock & block)
{
	for (uint i = 0; i < 16; i++) {
		color(i) = block.color(i);
	}
}


/// Initialize this color block.
ColorBlock::ColorBlock(const Image *img, uint x, uint y)
{
	init(img, x, y);
}

void ColorBlock::init(const Image *img, uint x, uint y)
{
	init(img->width(), img->height(), (const uint *)img->pixels(), x, y);
}

void ColorBlock::init(uint w, uint h, const uint *data, uint x, uint y)
{
	const uint bw = MIN(w - x, 4U);
	const uint bh = MIN(h - y, 4U);

	// Blocks that are smaller than 4x4 are handled by repeating the pixels.
	// @@ Thats only correct when block size is 1, 2 or 4, but not with 3. :(
	// @@ Ideally we should zero the weights of the pixels out of range.

	for (uint i = 0; i < 4; i++) {
		const int by = i % bh;

		for (uint e = 0; e < 4; e++) {
			const int bx = e % bw;
			const uint idx = (y + by) * w + x + bx;

			color(e, i).u = data[idx];
		}
	}
}

void ColorBlock::init(uint w, uint h, const float *data, uint x, uint y)
{
	const uint bw = MIN(w - x, 4U);
	const uint bh = MIN(h - y, 4U);

	// Blocks that are smaller than 4x4 are handled by repeating the pixels.
	// @@ Thats only correct when block size is 1, 2 or 4, but not with 3. :(
	// @@ Ideally we should zero the weights of the pixels out of range.

	uint srcPlane = w * h;

	for (uint i = 0; i < 4; i++) {
		const uint by = i % bh;
		
		for (uint e = 0; e < 4; e++) {
			const uint bx = e % bw;
			const uint idx = ((y + by) * w + x + bx);
			
			Color32 & c = color(e, i);
			c.r = uint8(255 * CLAMP(data[idx + 0 * srcPlane], 0.0f, 1.0f)); // @@ Is this the right way to quantize floats to bytes?
			c.g = uint8(255 * CLAMP(data[idx + 1 * srcPlane], 0.0f, 1.0f));
			c.b = uint8(255 * CLAMP(data[idx + 2 * srcPlane], 0.0f, 1.0f));
			c.a = uint8(255 * CLAMP(data[idx + 3 * srcPlane], 0.0f, 1.0f));
		}
	}
}

static inline uint8 component(Color32 c, uint i)
{
	if (i == 0) return c.r;
	if (i == 1) return c.g;
	if (i == 2) return c.b;
	if (i == 3) return c.a;
	if (i == 4) return 0xFF;
	return 0;
}

void ColorBlock::swizzle(uint x, uint y, uint z, uint w)
{
	for (int i = 0; i < 16; i++) {
		Color32 c = m_color[i];
		m_color[i].r = component(c, x);
		m_color[i].g = component(c, y);
		m_color[i].b = component(c, z);
		m_color[i].a = component(c, w);
	}
}


/// Returns true if the block has a single color.
bool ColorBlock::isSingleColor(Color32 mask/*= Color32(0xFF, 0xFF, 0xFF, 0x00)*/) const
{
	uint u = m_color[0].u & mask.u;
	
	for (int i = 1; i < 16; i++) {
		if (u != (m_color[i].u & mask.u)) {
			return false;
		}
	}
	
	return true;
}

/*
/// Returns true if the block has a single color, ignoring transparent pixels.
bool ColorBlock::isSingleColorNoAlpha() const
{
	Color32 c;
	int i;
	for (i = 0; i < 16; i++)
	{
		if (m_color[i].a != 0) c = m_color[i];
	}

	Color32 mask(0xFF, 0xFF, 0xFF, 0x00);
	uint u = c.u & mask.u;

	for (; i < 16; i++)
	{
		if (u != (m_color[i].u & mask.u))
		{
			return false;
		}
	}
	
	return true;
}
*/

/// Count number of unique colors in this color block.
/*uint ColorBlock::countUniqueColors() const
{
	uint count = 0;

	// @@ This does not have to be o(n^2)
	for (int i = 0; i < 16; i++)
	{
		bool unique = true;
		for (int j = 0; j < i; j++) {
			if ( m_color[i] != m_color[j] ) {
				unique = false;
			}
		}
		
		if ( unique ) {
			count++;
		}
	}
	
	return count;
}*/

/*/// Get average color of the block.
Color32 ColorBlock::averageColor() const
{
	uint r, g, b, a;
	r = g = b = a = 0;

	for (uint i = 0; i < 16; i++) {
		r += m_color[i].r;
		g += m_color[i].g;
		b += m_color[i].b;
		a += m_color[i].a;
	}
	
	return Color32(uint8(r / 16), uint8(g / 16), uint8(b / 16), uint8(a / 16));
}*/

/// Return true if the block is not fully opaque.
bool ColorBlock::hasAlpha() const
{
	for (uint i = 0; i < 16; i++) {
		if (m_color[i].a != 255) return true;
	}
	return false;
}

#if 0

/// Get diameter color range.
void ColorBlock::diameterRange(Color32 *start, Color32 *end) const
{
	Color32 c0, c1;
	uint best_dist = 0;
	
	for (int i = 0; i < 16; i++) {
		for (int j = i+1; j < 16; j++) {
			uint dist = colorDistance(m_color[i], m_color[j]);
			if ( dist > best_dist ) {
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
void ColorBlock::luminanceRange(Color32 *start, Color32 *end) const
{
	Color32 minColor, maxColor;
	uint minLuminance, maxLuminance;
	
	maxLuminance = minLuminance = colorLuminance(m_color[0]);
	
	for (uint i = 1; i < 16; i++)
	{
		uint luminance = colorLuminance(m_color[i]);
		
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
void ColorBlock::boundsRange(Color32 *start, Color32 *end) const
{
	Color32 minColor(255, 255, 255);
	Color32 maxColor(0, 0, 0);

	for (uint i = 0; i < 16; i++)
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
void ColorBlock::boundsRangeAlpha(Color32 *start, Color32 *end) const
{
	Color32 minColor(255, 255, 255, 255);
	Color32 maxColor(0, 0, 0, 0);

	for (uint i = 0; i < 16; i++)
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
#endif

/*/// Sort colors by abosolute value in their 16 bit representation.
void ColorBlock::sortColorsByAbsoluteValue()
{
	// Dummy selection sort.
	for ( uint a = 0; a < 16; a++ ) {
		uint max = a;
		Color16 cmax(m_color[a]);
		
		for ( uint b = a+1; b < 16; b++ ) {
			Color16 cb(m_color[b]);
			
			if ( cb.u > cmax.u ) {
				max = b;
				cmax = cb;
			}
		}
		swap( m_color[a], m_color[max] );
	}
}*/


/*/// Find extreme colors in the given axis.
void ColorBlock::computeRange(Vector3::Arg axis, Color32 *start, Color32 *end) const
{
	
	int mini, maxi;
	mini = maxi = 0;
	
	float min, max;
	min = max = dot(Vector3(m_color[0].r, m_color[0].g, m_color[0].b), axis);

	for (uint i = 1; i < 16; i++)
	{
		const Vector3 vec(m_color[i].r, m_color[i].g, m_color[i].b);
		
		float val = dot(vec, axis);
		if ( val < min ) {
			mini = i;
			min = val;
		}
		else if ( val > max ) {
			maxi = i;
			max = val;
		}
	}
	
	*start = m_color[mini];
	*end = m_color[maxi];
}*/


/*/// Sort colors in the given axis.
void ColorBlock::sortColors(const Vector3 & axis)
{
	float luma_array[16];
	
	for (uint i = 0; i < 16; i++) {
		const Vector3 vec(m_color[i].r, m_color[i].g, m_color[i].b);
		luma_array[i] = dot(vec, axis);
	}
	
	// Dummy selection sort.
	for ( uint a = 0; a < 16; a++ ) {
		uint min = a;
		for ( uint b = a+1; b < 16; b++ ) {
			if ( luma_array[b] < luma_array[min] ) {
				min = b;
			}
		}
		swap( luma_array[a], luma_array[min] );
		swap( m_color[a], m_color[min] );
	}
}*/


/*/// Get the volume of the color block.
float ColorBlock::volume() const
{
	Box bounds;
	bounds.clearBounds();
	
	for (int i = 0; i < 16; i++) {
		const Vector3 point(m_color[i].r, m_color[i].g, m_color[i].b);
		bounds.addPointToBounds(point);
	}
	
	return bounds.volume();
}
*/

