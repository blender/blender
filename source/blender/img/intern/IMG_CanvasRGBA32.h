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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 * @author	Maarten Gribnau
 * @date	March 6, 2001
 */

#ifndef _H_IMG_CanvasRGBA32
#define _H_IMG_CanvasRGBA32

#include "IMG_PixmapRGBA32.h"
#include "IMG_BrushRGBA32.h"

/**
 * A IMG_PixmapRGBA32 pixmap that allows for drawing with a IMG_BrushRGBA32.
 * @author	Maarten Gribnau
 * @date	March 6, 2001
 */

class IMG_CanvasRGBA32 : public IMG_PixmapRGBA32 {
public:
	/**
	 * Constructor.
	 * @throw <IMG_MemPtr::Size>	when an invalid width and/or height is passed.
	 * @throw <IMG_MemPtr::Memory>	when a there is not enough memory to allocate the image.
	 * @param	width	the width in pixels of the image.
	 * @param	height	the height in pixels of the image.
	 */
	IMG_CanvasRGBA32(TUns32 width, TUns32 height);

	/**
	 * Constructor.
	 * The image data will not be freed upon destruction of this object.
	 * The owner of this object is reponsible for that.
	 * @throw <Size>	when an invalid width and/or height is passed.
	 * @param	image	pointer to the image data.
	 * @param	width	the width in pixels of the image.
	 * @param	height	the height in pixels of the image.
	 */
	IMG_CanvasRGBA32(void* image, TUns32 width, TUns32 height, TUns32 rowBytes);

	/**
	 * Blends a pixmap into this pixmap over a line.
	 * Repeatedly pastes the given pixmap centered at the given line into this pixmap.
	 * The alpha information in the given image is used to blend.
	 * @todo	implement wrapping modes when the pixmap does not fit within the bounds.
	 * @todo	update the drawing algorithm.
	 * @param	x		x-coordinate of the center location of the image.
	 * @param	y		y-coordinate of the center location of the image.
	 * @param	pixmap	the pixmap to blend
	 */
	virtual void blendPixmap(TUns32 xStart, TUns32 yStart, TUns32 xEnd, TUns32 yEnd, const IMG_PixmapRGBA32& pixmap);

	/**
	 * Blends a pixmap into this pixmap over a line in (u,v) coordinates.
	 * Pastes the given pixmap centered at the given line into this pixmap.
	 * The alpha information in the given image is used to blend.
	 * @see		IMG_PixmapRGBA32::blendPixmap(TUns32 xStart, TUns32 yStart, TUns32 yStart, TUns32 yEnd, const IMG_PixmapRGBA32& pixmap)
	 * @todo	implement wrapping modes when the pixmap does not fit within the bounds.
	 * @param	u		u-coordinate of the center location of the image.
	 * @param	v		v-coordinate of the center location of the image.
	 * @param	pixmap	the pixmap to blend
	 */
	virtual void blendPixmap(float uStart, float vStart, float uEnd, float vEnd, const IMG_PixmapRGBA32& pixmap);
};


#endif // _H_IMG_CanvasRGBA32
