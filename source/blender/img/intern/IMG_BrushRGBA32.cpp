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

#include "IMG_BrushRGBA32.h"
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

IMG_BrushRGBA32::IMG_BrushRGBA32(TUns32 w, TUns32 h, const IMG_ColorRGB& c, float a)
	: IMG_PixmapRGBA32(w, h), m_color(c), m_alpha(a)
{
	m_ro = w < h ? w/2 : h/2;
	m_ri = m_ro >> 1;
	updateImage();
}


void IMG_BrushRGBA32::setColor(const IMG_ColorRGB& c)
{
	m_color = c;
	updateImage();
}


void IMG_BrushRGBA32::setTransparency(float a)
{
	m_alpha = a;
	if (m_alpha > 1.f) m_alpha = 1.f;
	if (m_alpha < 0.f) m_alpha = 0.f;
	updateImage();
}


void IMG_BrushRGBA32::setRadii(TUns32 rI, TUns32 rO)
{
	if ((rI < 2) || (rO < 2)) return;
	m_ri = rI;
	m_ro = rO;

    TUns32 w_2 = m_width >> 1;
    TUns32 h_2 = m_height >> 1;

	/*
	 * Make the brush size smaller than half of the minimum
	 * width or height of the pixmap. Make sure that inner
	 * radius <= outer radius.
	 */
	if (m_ro > w_2) m_ro = w_2;
	if (m_ro > h_2) m_ro = h_2;
	if (m_ri > m_ro) m_ri = m_ro;

	updateImage();
}


void IMG_BrushRGBA32::updateImage()
{
    TUns32 cx = m_width >> 1;
    TUns32 cy = m_height >> 1;
	
	// Prepare pixel values for this pixmap
	IMG_ColorRGBA c (m_color.m_r, m_color.m_g, m_color.m_b, 0.f);
	TPixelRGBA32 pOut = getPixelValue(c);
	c.m_a = m_alpha;
	TPixelRGBA32 pIn = getPixelValue(c);
	TPixelRGBA32 p = getPixelValue(c);
	TUns8* pa = & (((TUns8*)&p)[bi_a]);

	// Set the pixels in the destination rectangle
	for (TUns32 y = 0; y < m_height; y++) {
		// Park pixel pointer at the start pixels
		TPixelPtr desPtr = getPixelPtr(0, y);
		for (TUns32 x = 0; x < m_width; x++) {
			// Calculate the distance between current pixel and center
            float dX = (float)((TInt32)x) - ((TInt32)cx);
            float dY = (float)((TInt32)y) - ((TInt32)cy);
            float d = (float) ::sqrt(dX*dX + dY*dY);
			float a;

			if (d <= m_ri) {
				*desPtr = pIn;
			}
			else if ((d < m_ro) && (m_ri < m_ro)) {
				// Calculate alpha, linear
                a = (d - m_ri) / (m_ro - m_ri);
				// Now: 0 <= a <= 1
				//a = m_alpha + a * (1.f - m_alpha);
				a = (1.f - a) * m_alpha;
				// Now: m_alpha <= a <= 1
#if 0
                a = (float)::pow(a, 0.2);
#endif
				// Store pixel
				*pa = (TUns8)(a * ((float)0xFF));
				*desPtr = p;
			}
			else {
				*desPtr = pOut;
			}
			desPtr++;
		}
	}
}
