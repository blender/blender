/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

/**

 * $Id$
 * Copyright (C) 2001 NaN Technologies B.V.
 * Base class for pixmaps.
 * @author	Maarten Gribnau
 * @date	March 6, 2001
 */


#include "../extern/IMG_PixmapRGBA32.h"

IMG_PixmapRGBA32::IMG_PixmapRGBA32(GEN_TUns32 width, GEN_TUns32 height)
	: IMG_Pixmap(), m_pixels(width * height)
{
	m_pixelsBase = m_pixels;
	m_width = width;
	m_height = height;
	m_rowBytes = width * sizeof(TPixelRGBA32);
	m_pixelSize = 32;
	m_pixelType = kPixelTypeRGBA32;
}


IMG_PixmapRGBA32::IMG_PixmapRGBA32(void* image, GEN_TUns32 width, GEN_TUns32 height, GEN_TUns32 rowBytes)
	: IMG_Pixmap(), m_pixels(image, width * rowBytes)
{
	m_pixelsBase = m_pixels;
	m_width = width;
	m_height = height;
	m_rowBytes = rowBytes;
	m_pixelSize = 32;
	m_pixelType = kPixelTypeRGBA32;
}


void IMG_PixmapRGBA32::fillRect(const GEN_Rect& r, const IMG_ColorRGB& c)
{
	GEN_Rect t_bnds (0, 0, m_width, m_height);	// Bounds of this pixmap
	GEN_Rect r_bnds (r);						// Area to set

	// Determine visibility
	GEN_TVisibility v = t_bnds.getVisibility(r_bnds);
	if (v == GEN_kNotVisible) return;
	if (v == GEN_kPartiallyVisible) {
		// Clip the destination rectangle to the bounds of this pixmap
		t_bnds.clip(r_bnds);
		if (r_bnds.isEmpty()) {
			return;
		}
	}

#if 0
	// Set new pixels using shifting
	// Prepare the pixel value to set
	IMG_ColorRGBA ca (c);
	TPixelRGBA32 pv = getPixelValue(c);
	// Mask off the alpha bits
	pv &= 0x00FFFFFF; //0xFFFFFF00;

	// Set the pixels in the destination rectangle
	for (GEN_TInt32 y = r.m_t; y < r.m_b; y++) {
		// Park pixel pointer at the start pixels
		TPixelPtr desPtr = getPixelPtr(r_bnds.m_l, y);
		for (GEN_TInt32 x = r.m_l; x < r.m_r; x++) {
			// Set the new pixel value (retain current alpha)
			*(desPtr++) = pv | ((*desPtr) & 0xFF000000); //0x000000FF);
		}
	}
#else
	// Set new values using byte indexing
	//IMG_ColorRGBA ca (c);
	TPixelRGBA32 src = getPixelValue(c);
	TPixelPtr desPtr;
	GEN_TUns8* srcBytes = (GEN_TUns8*) &src;
	
	// Set the pixels in the destination rectangle
	for (GEN_TInt32 y = r.m_t; y < r.m_b; y++) {
		// Park pixel pointer at the start pixels
		desPtr = getPixelPtr(r_bnds.m_l, y);
		for (GEN_TInt32 x = r.m_l; x < r.m_r; x++) {
			// Set the new pixel value (retain current alpha)
			((GEN_TUns8*)desPtr)[bi_r] = srcBytes[bi_r];
			((GEN_TUns8*)desPtr)[bi_g] = srcBytes[bi_g];
			((GEN_TUns8*)desPtr)[bi_b] = srcBytes[bi_b];
			desPtr++;
		}
	}
#endif
}


void IMG_PixmapRGBA32::fillRect(const GEN_Rect& r, const IMG_ColorRGBA& c)
{
	GEN_Rect t_bnds (0, 0, m_width, m_height);	// Bounds of this pixmap
	GEN_Rect r_bnds (r);						// Area to set

	// Determine visibility
	GEN_TVisibility v = t_bnds.getVisibility(r_bnds);
	if (v == GEN_kNotVisible) return;
	if (v == GEN_kPartiallyVisible) {
		// Clip the destination rectangle to the bounds of this pixmap
		t_bnds.clip(r_bnds);
		if (r_bnds.isEmpty()) {
			return;
		}
	}

	// Set the pixels in the destination rectangle
	TPixelRGBA32 pixel = getPixelValue(c);
	for (GEN_TInt32 y = r.m_t; y < r.m_b; y++) {
		// Park pixel pointer at the start pixels
		TPixelPtr desPtr = getPixelPtr(r_bnds.m_l, y);
		for (GEN_TInt32 x = r.m_l; x < r.m_r; x++) {
			*(desPtr++) = pixel;
		}
	}
}


void IMG_PixmapRGBA32::setPixmap(const IMG_PixmapRGBA32& src, const GEN_Rect& srcBnds, const GEN_Rect& destBnds)
{
	GEN_Rect i_bnds (srcBnds);					// Bounds of input pixmap
	GEN_Rect t_bnds (0, 0, m_width, m_height);	// Bounds of this pixmap
	GEN_Rect p_bnds (destBnds);					// Bounds of the paste area
	
	// The next check could be removed if the caller is made responsible for handing us non-empty rectangles
	if (i_bnds.isEmpty()) {
		// Nothing to do
		return;
	}

	// Determine visibility of the paste area
	GEN_TVisibility v = t_bnds.getVisibility(p_bnds);
	if (v == GEN_kNotVisible) return;
	if (v == GEN_kPartiallyVisible) {
		// Clipping is needed
		if (p_bnds.m_l < 0) {
			i_bnds.m_l += -p_bnds.m_l;
			p_bnds.m_l = 0;
		}
		if (p_bnds.m_t < 0) {
			i_bnds.m_t += -p_bnds.m_t;
			p_bnds.m_t = 0;
		}
		GEN_TInt32 d = t_bnds.getWidth();
		if (p_bnds.m_r > d) {
			i_bnds.m_r -= d - p_bnds.m_r;
			p_bnds.m_r = d;
		}
		d = t_bnds.getHeight();
		if (p_bnds.m_b > d) {
			i_bnds.m_b -= d - p_bnds.m_b;
			p_bnds.m_b = d;
		}
	}

	// Iterate through the rows
	for (GEN_TInt32 r = 0; r < p_bnds.getHeight(); r++) {
		// Park pixel pointers at the start pixels
		TPixelPtr srcPtr = src.getPixelPtr(i_bnds.m_l, i_bnds.m_t + r);
		TPixelPtr desPtr = getPixelPtr(p_bnds.m_l, p_bnds.m_t + r);
		// Iterate through the columns
		for (int c = 0; c < p_bnds.getWidth(); c++) {
			*(desPtr++) = *(srcPtr++);
		}
	}
}


void IMG_PixmapRGBA32::blendPixmap(const IMG_PixmapRGBA32& src, const GEN_Rect& srcBnds, const GEN_Rect& destBnds)
{
	GEN_Rect i_bnds (srcBnds);					// Bounds of input pixmap
	GEN_Rect t_bnds (0, 0, m_width, m_height);	// Bounds of this pixmap
	GEN_Rect p_bnds (destBnds);					// Bounds of the paste area
	
	// The next check could be removed if the caller is made responsible for handing us non-empty rectangles
	if (i_bnds.isEmpty()) {
		// Nothing to do
		return;
	}

	// Determine visibility of the paste area
	GEN_TVisibility v = t_bnds.getVisibility(p_bnds);
	if (v == GEN_kNotVisible) return;
	if (v == GEN_kPartiallyVisible) {
		// Clipping is needed
		if (p_bnds.m_l < 0) {
			i_bnds.m_l += -p_bnds.m_l;
			p_bnds.m_l = 0;
		}
		if (p_bnds.m_t < 0) {
			i_bnds.m_t += -p_bnds.m_t;
			p_bnds.m_t = 0;
		}
		GEN_TInt32 d = t_bnds.getWidth();
		if (p_bnds.m_r > d) {
			i_bnds.m_r -= p_bnds.m_r - d;
			p_bnds.m_r = d;
		}
		d = t_bnds.getHeight();
		if (p_bnds.m_b > d) {
			i_bnds.m_b -= p_bnds.m_b - d;
			p_bnds.m_b = d;
		}
	}

	IMG_ColorRGBA srcColor;
	IMG_ColorRGBA desColor;

	// Iterate through the rows
	for (int r = 0; r < p_bnds.getHeight(); r++) {
		// Park pixel pointers at the start pixels
		TPixelPtr srcPtr = src.getPixelPtr(i_bnds.m_l, i_bnds.m_t + r);
		TPixelPtr desPtr = getPixelPtr(p_bnds.m_l, p_bnds.m_t + r);
		// Iterate through the columns
		for (int c = 0; c < p_bnds.getWidth(); c++) {
			// Retrieve colors from source and destination pixmaps
			getColor(*srcPtr, srcColor);
			getColor(*desPtr, desColor);
			// Blend the colors
			desColor.blendColor(srcColor);
			// Write color back to destination pixmap
			*desPtr = getPixelValue(desColor);
			srcPtr++;
			desPtr++;
		}
	}
}
