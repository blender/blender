/*
 * pixelshading.h
 *
 * These functions determine what actual colour a pixel will have.
 *
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
 */

#ifndef PIXELSHADING_H
#define PIXELSHADING_H

#include "render.h"
#include "vanillaRenderPipe_types.h"

/**
 * Render the pixel at (x,y) for object ap. Apply the jitter mask. 
 * Output is given in float collector[4]. The type vector:
 * t[0] - min. distance
 * t[1] - face/halo index
 * t[2] - jitter mask                     
 * t[3] - type ZB_POLY or ZB_HALO
 * t[4] - max. distance
 * mask is pixel coverage in bits
 * @return pointer to the object
 */
void *renderPixel(float x, float y, int *t, int mask);

void *renderHaloPixel(float x, float y, int haloNr) ;


void setSkyBlendingMode(enum RE_SkyAlphaBlendingType mode);
void shadeHaloFloat(HaloRen *har, 
					float *col, unsigned int zz, 
					float dist, float xn, 
					float yn, short flarec);

/**
 * Get the sky blending mode.
 */
enum RE_SkyAlphaBlendingType getSkyBlendingMode(void);
/**
 * Render the sky at pixel (x, y).
 */
void renderSkyPixelFloat(float x, float y);

/* used by shadeSkyPixel: */
void shadeSkyPixelFloat(float y, float *view);
void renderSpotHaloPixel(float x, float y, float *target);
void shadeSkyPixel(float fx, float fy);
void fillBackgroundImage(float x, float y);

/* ------------------------------------------------------------------------- */

#endif

