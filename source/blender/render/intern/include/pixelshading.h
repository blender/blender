/*
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): 2004-2006, Blender Foundation, full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/* pixelshading.h
*
* These functions determine what actual color a pixel will have.
*/


#ifndef PIXELSHADING_H
#define PIXELSHADING_H

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
int shadeHaloFloat(HaloRen *har, 
					float *col, int zz, 
					float dist, float xn, 
					float yn, short flarec);

/**
 * Render the sky at pixel (x, y).
 */
void shadeSkyPixel(float *collector, float fx, float fy);
void shadeSkyView(float *colf, float *rco, float *view, float *dxyview);
void shadeAtmPixel(struct SunSky *sunsky, float *collector, float fx, float fy, float distance);

/* ------------------------------------------------------------------------- */

#endif

