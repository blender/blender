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
 * Abstract base class for pixmaps of different types.
 * @author	Maarten Gribnau
 * @date	March 6, 2001
 */

#ifndef _H_IMG_Pixmap
#define _H_IMG_Pixmap

#include "GEN_Types.h"
#include "GEN_Rect.h"


class IMG_Pixmap
{
public:
	/** The type of pixels that are stored in this pixmap. */
        typedef enum {
           kPixelTypeUnknown       = 0,    /**< R:8, G:8, B:8, Ignore:8  */
/*         kPixelTypeRGB32         = 1, */ /*< R:8, G:8, B:8, Ignore:8   */
           kPixelTypeRGBA32        = 2,    /**< R:8, G:8, B:8, Alpha:8   */
/*         kPixelTypeRGB16         = 3, */ /**< Ignore:1, R:5, G:5, B:5  */
/*         kPixelTypeRGBA16        = 4, */ /**< Alpha:1,  R:5, G:5, B:5  */
/*         kPixelTypeRGB16_565     = 5, */ /**<           R:5, G:6, B:5  */
/*         kPixelTypeRGB24         = 6  */ /**< R:8, G:8, B:8            */
        } TPixelType;

	/**
	 * Default constructor.
	 */
	IMG_Pixmap();

	/**
	 * Constructs a pixmap with the requested width and height.
	 * @param width The width of the pixmap created.
	 * @param height The height of the pixmap created.
	 */
	IMG_Pixmap(GEN_TUns32 width, GEN_TUns32 height);

	/**
	 * Destructor.
	 */
	virtual	~IMG_Pixmap();

	/**
	 * Access to image data
	 * @return	pointer to the image data
	 */
	virtual void* getDataPtr() const = 0;

	/**
	 * Access to image width.
	 * @return	width of the image
	 */
	inline GEN_TUns32 getWidth() const;

	/**
	 * Access to image height.
	 * @return	height of the image
	 */
	inline GEN_TUns32 getHeight() const;

	/**
	 * Returns the bounds of the pixmap in a rectangle.
	 * @param	bounds of the image
	 */
	inline void getBounds(GEN_Rect& r) const;

	/**
	 * Access to pixel type.
	 * @return	the pixel type
	 */
	inline TPixelType getPixelType() const;

	/**
	 * Clamps coordinates inside (0, 0) and (width, height).
	 * @param	x	requested x-coordinate
	 * @param	y	requested y-coordinate
	 */
	inline void clamp(GEN_TInt32& x, GEN_TInt32& y) const;

	/**
	 * Clamps u, v coordinates between 0 and 1.
	 * @param	u	requested u-coordinate
	 * @param	v	requested v-coordinate
	 */
	inline void clamp(float& u, float& v) const;

	/**
	 * Converts (u,v) coordinates to pixel addresses.
	 * Assumes that (u,v) coordinates are in the [0,1] range.
	 * @param	u	requested u-coordinate in the image
	 * @param	v	requested v-coordinate in the image
	 * @param	x	calculated x-coordinate in the image
	 * @param	y	calculated y-coordinate in the image 
	 */
	inline void	getPixelAddress(float u, float v, GEN_TInt32& x, GEN_TInt32& y) const;

	/**
	 * Fills the rectangle given with the color given.
	 * Performs a bounds check.
	 * @param	r	requested bounds rectangle in the image
	 * @param	c	color to use
	 */
	/*virtual void fillRect(const GEN_Rect& r, const IMG_ColorRGBA& c) = 0; */

protected:
	/** Width of the image in pixels */
	GEN_TUns32 m_width;
	/** Height of the image in pixels */
	GEN_TUns32 m_height;
	/** Number of bytes for one row in the image. */
	GEN_TUns32 m_rowBytes;
	/** Size in bits for one pixel */
	GEN_TUns32 m_pixelSize;
	/** Type of pixels in this image. */
	TPixelType m_pixelType;
/*	TEndian m_bitOrder;
	TEndian m_byteOrder;
*/
};

inline GEN_TUns32 IMG_Pixmap::getWidth() const
{
	return m_width;
}

inline GEN_TUns32 IMG_Pixmap::getHeight() const
{
	return m_height;
}

inline void IMG_Pixmap::getBounds(GEN_Rect& r) const
{
	r.set(0, 0, m_width, m_height);
}

inline IMG_Pixmap::TPixelType IMG_Pixmap::getPixelType() const
{
	return m_pixelType;
}

inline void IMG_Pixmap::clamp(GEN_TInt32& x, GEN_TInt32& y) const
{
	if (x < 0) x = 0;
	if (x > (GEN_TInt32)m_width) x = (GEN_TInt32)m_width;
	if (y < 0) y = 0;
	if (y > (GEN_TInt32)m_height) y = (GEN_TInt32)m_height;
}

inline void IMG_Pixmap::clamp(float& u, float& v) const
{
	if (u < 0.f) u = 0.f;
	if (u > 1.f) u = 1.f;
	if (v < 0.f) v = 0.f;
	if (v > 1.f) v = 1.f;
}

inline void	IMG_Pixmap::getPixelAddress(float u, float v, GEN_TInt32& x, GEN_TInt32& y) const
{
	x = (GEN_TInt32)(((float)m_width) * u);
	y = (GEN_TInt32)(((float)m_height) * v);
}

#endif /* _H_IMG_Pixmap */

