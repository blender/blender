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
 * Pixmap of kPixelTypeRGBA32 type.
 * A pixmap with 24 bits per pixel in ABGR format.
 * Provides methods to fill rectangular areas in this image with a color.
 * Provides methods to paste or blend other pixmaps into this pixmap.
 * @author	Maarten Gribnau
 * @date	March 6, 2001
 */


#ifndef _H_IMG_PixmapRGBA32
#define _H_IMG_PixmapRGBA32

#include "IMG_Pixmap.h"
#include "IMG_MemPtr.h"
#include "IMG_Color.h"


class IMG_PixmapRGBA32 : public IMG_Pixmap {
public:
	/** The pixel type in this pixmap. */
	typedef GEN_TUns32 TPixelRGBA32;

	/** The pixel pointer type of this pixmap. */
	typedef TPixelRGBA32* TPixelPtr;

	/** Indices of color component byes within a pixel. */
	typedef enum {
		bi_r = 0,
		bi_g = 1,
		bi_b = 2,
		bi_a = 3
	} TPixelIndex;

	/**
	 * Constructor.
	 * Creates a new pixmap of the kPixelTypeRGBA32 type with the requested dimensions.
	 * @throw <IMG_MemPtr::Size>	when an invalid width and/or height is passed.
	 * @throw <IMG_MemPtr::Memory>	when a there is not enough memory to allocate the image.
	 * @param	width	the width in pixels of the image.
	 * @param	height	the height in pixels of the image.
	 */
	IMG_PixmapRGBA32(GEN_TUns32 width, GEN_TUns32 height);

	/**
	 * Constructor.
	 * Creates a new pixmap of the kPixelTypeRGBA32 from a pointer to image data.
	 * The image data will not be freed upon destruction of this object.
	 * The owner of this object is reponsible for that.
	 * @throw <Size>	when an invalid width and/or height is passed.
	 * @param	image	pointer to the image data.
	 * @param	width	the width in pixels of the image.
	 * @param	height	the height in pixels of the image.
	 */
	IMG_PixmapRGBA32(void* image, GEN_TUns32 width, GEN_TUns32 height, GEN_TUns32 rowBytes);

#if 0
	/**
	 * Destructor.
	 */
	virtual	~IMG_PixmapRGBA32();
#endif

	/**
	 * Fills the rectangle given with the color given.
	 * Retains the alpha values.
	 * Performs a bounds check.
	 * @param	r	requested bounds rectangle in the image
	 * @param	c	color to use
	 */
	virtual void fillRect(const GEN_Rect& r, const IMG_ColorRGB& c);

	/**
	 * Fills the rectangle given with the color given.
	 * Sets the alpha values from the color.
	 * Performs a bounds check.
	 * @param	r	requested bounds rectangle in the image
	 * @param	c	color to use
	 */
	virtual void fillRect(const GEN_Rect& r, const IMG_ColorRGBA& c);

	/**
	 * Pastes an area of a pixmap into this pixmap.
	 * Pastes an area of the given pixmap centered at the given coordinates into this pixmap.
	 * Ignores the alpha information, this is pasted as well.
	 * Performs a bounds check.
	 * @param	src			the pixmap to paste.
	 * @param	srcBnds		the bounds of the area of the pixmap to paste.
	 * @param	destBnds	the destination area for the paste.
	 */
	virtual	void setPixmap(const IMG_PixmapRGBA32& src, const GEN_Rect& srcBnds, const GEN_Rect& destBnds);

	/**
	 * Blends an area of a pixmap into this pixmap.
	 * Blends an area of the given pixmap centered at the given coordinates into this pixmap.
	 * The alpha information in the given image is used to blend.
	 * Performs a bounds check.
	 * @param	src			the pixmap to paste.
	 * @param	srcBnds		the bounds of the area of the pixmap to paste.
	 * @param	destBnds	the destination area for the paste.
	 */
	virtual void blendPixmap(const IMG_PixmapRGBA32& src, const GEN_Rect& srcBnds, const GEN_Rect& destBnds);

protected:
	/**
	 * Returns pointer to the pixel.
	 * Returns a pointer of TPixelPtr type to the pixel at the requested coordinates.
	 * Does not perform a bounds check!
	 * @param	x	column address of the pixel.
	 * @param	y	row address of the pixel.
	 * @return	the pointer calculated.
	 */
	inline TPixelPtr getPixelPtr(GEN_TUns32 x, GEN_TUns32 y) const;

	/**
	 * Returns the pixel value of a color.
	 * @param	c	the color to convert
	 * @return	the pixel value calculated
	 */
	inline TPixelRGBA32	getPixelValue(const IMG_ColorRGBA& c) const;

	/**
	 * Returns the color of from a pixel value.
	 * @param	p	the pixel value
	 * @param	c	the color calculated
	 */
	inline void	getColor(TPixelRGBA32 p, IMG_ColorRGBA& c) const;

protected:
	/** "Save" memory pointer. */
	IMG_MemPtr<TPixelRGBA32> m_pixels;

	/** Pointer to the first pixel. */
	TPixelPtr m_pixelsBase;
};


inline IMG_PixmapRGBA32::TPixelPtr IMG_PixmapRGBA32::getPixelPtr(GEN_TUns32 x, GEN_TUns32 y) const
{
	return (TPixelPtr) (((GEN_TUns8*)m_pixelsBase) + (y*m_rowBytes) + (x<<2));
}


inline IMG_PixmapRGBA32::TPixelRGBA32 IMG_PixmapRGBA32::getPixelValue(const IMG_ColorRGBA& c) const
{
#if 0
	// Obtain pixel value through shifting
	TPixelRGBA32 p = ((TPixelRGBA32) (((float) 0xFF) * c.m_a)) << 24;
	p |= ((TPixelRGBA32) (((float) 0xFF) * c.m_b)) << 16;
	p |= ((TPixelRGBA32) (((float) 0xFF) * c.m_g)) << 8;
	p |= ((TPixelRGBA32) (((float) 0xFF) * c.m_r));
	return p;
#else
	// Obtain pixel value through byte indexing
	TPixelRGBA32 pixel;
	GEN_TUns8* bytes = (GEN_TUns8*)&pixel;
	bytes[bi_r] = (GEN_TUns8)(((float) 0xFF) * c.m_r);
	bytes[bi_g] = (GEN_TUns8)(((float) 0xFF) * c.m_g);
	bytes[bi_b] = (GEN_TUns8)(((float) 0xFF) * c.m_b);
	bytes[bi_a] = (GEN_TUns8)(((float) 0xFF) * c.m_a);
	return pixel;
#endif
}

inline void	IMG_PixmapRGBA32::getColor(TPixelRGBA32 p, IMG_ColorRGBA& c) const
{
#if 0
	// Obtain color value through shifting
	c.m_a = ((float) ((p >> 24) & 0x00FF)) / ((float) 0xFF);
	c.m_b = ((float) ((p >> 16) & 0x00FF)) / ((float) 0xFF);
	c.m_g = ((float) ((p >>  8) & 0x00FF)) / ((float) 0xFF);
	c.m_r = ((float) ( p        & 0x00FF)) / ((float) 0xFF);
#else
	// Obtain color value through byte indexing
	GEN_TUns8* bytes = (GEN_TUns8*)&p;
	c.m_r = ((float)bytes[bi_r]) / ((float) 0xFF);
	c.m_g = ((float)bytes[bi_g]) / ((float) 0xFF);
	c.m_b = ((float)bytes[bi_b]) / ((float) 0xFF);
	c.m_a = ((float)bytes[bi_a]) / ((float) 0xFF);
#endif
}

#endif // _H_IMG_PixmapRGBA32