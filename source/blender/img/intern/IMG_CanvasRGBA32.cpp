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

#include "IMG_CanvasRGBA32.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

IMG_CanvasRGBA32::IMG_CanvasRGBA32(TUns32 width, TUns32 height)
	: IMG_PixmapRGBA32(width, height)
{
}

IMG_CanvasRGBA32::IMG_CanvasRGBA32(void* image, TUns32 width, TUns32 height, TUns32 rowBytes)
	: IMG_PixmapRGBA32(image, width, height, rowBytes)
{
}


void IMG_CanvasRGBA32::blendPixmap(
	TUns32 xStart, TUns32 yStart, TUns32 xEnd, TUns32 yEnd,
	const IMG_PixmapRGBA32& pixmap)
{
	// Determine visibility of the line
	IMG_Line l (xStart, yStart, xEnd, yEnd);	// Line used for blending
	IMG_Rect bnds (0, 0, m_width, m_height);	// Bounds of this pixmap
	TVisibility v = bnds.getVisibility(l);
	if (v == kNotVisible) return;
	if (v == kPartiallyVisible) {
		bnds.clip(l);
	}

	float numSteps = (((float)l.getLength()) / ((float)pixmap.getWidth() / 4));
	//numSteps *= 4;
	numSteps = numSteps ? numSteps : 1;
	float step = 0.f, stepSize = 1.f / ((float)numSteps);
	TInt32 x, y;
    for (TUns32 s = 0; s < numSteps; s++) {
		l.getPoint(step, x, y);
		IMG_PixmapRGBA32::blendPixmap((TUns32)x, (TUns32)y, pixmap);
		step += stepSize;
	}
}


void IMG_CanvasRGBA32::blendPixmap(
	float uStart, float vStart, float uEnd, float vEnd,
	const IMG_PixmapRGBA32& pixmap)
{
	TUns32 xStart, yStart, xEnd, yEnd;
	getPixelAddress(uStart, vStart, xStart, yStart);
	getPixelAddress(uEnd, vEnd, xEnd, yEnd);
	blendPixmap(xStart, yStart, xEnd, yEnd, pixmap);
}

