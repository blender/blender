/*
 * rendercore_ext.h
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

#ifndef RENDERCORE_H
#define RENDERCORE_H 
#include "render_types.h"

struct HaloRen;

typedef struct ShadeResult 
{
	float diff[3];
	float spec[3];
	float alpha;

} ShadeResult;


float   mistfactor(float *co);	/* dist en hoogte, return alpha */
void renderspothalo(unsigned short *col);
void render_lighting_halo(struct HaloRen *har, float *colf);
unsigned int    calchalo_z(struct HaloRen *har, unsigned int zz);
void shade_lamp_loop(int mask, ShadeResult *shr);
float fresnel_fac(float *view, float *vn, float fresnel, float falloff);

float spec(float inp, int hard);

void add_halo_flare(void);

/**
 * Apply the background (sky). Depending on the active alphamode and
 * worldmode, different filling strategies are applied.
 * Active alphamode = R.r.alphamode
 * Active worldmode = R.wrld.mode
 * <LI>
 * <IT> R_ALPHAPREMUL - do not fill sky, but apply alpha to colours
 * <IT> R_ALPHAKEY    - do not fill sky, do not apply alpha to colours
 * <IT> R_ADDSKY      - fill skycolour in the background, blend
 *                      transparent colours with the background
 * (there's also a world dependency here?
 *   <LI>
 *   <IT> R.wrld.mode == WO_MIST
 *   <IT> R.r.bufflag == 1, R.flag == R_SEC_FIELD
 *   <IT> R.wrld.skytype == ( WO_SKYBLEND ^ WO_SKYTEX)
 *   <IT>   R.wrld.skytype == WO_SKYPAPER
 *   <IT> R.r.mode == R_PANORAMA )
 *   </LI>
 * </LI>
 * @param rect
 * @param y 
 */
void scanlinesky(char *rect, int y);

/**
 * Do z buffer stuff.
 */
void zbufshade(void);

/**
 * Insert transparent faces into the z buffer?
 */
void zbufshadeDA(void);	/* Delta Accum Pixel Struct */

/**
 * Also called in: zbuf.c
 */
void shadepixel(float x, float y, int vlaknr, int mask);

/**
 * Shade the pixel at xn, yn for halo har, and write the result to col. 
 * Also called in: previewrender.c
 * @param har    The halo to be rendered on this location
 * @param col    [unsigned int 3] The destination colour vector 
 * @param zz     Some kind of distance
 * @param dist   Square of the distance of this coordinate to the halo's center
 * @param x      [f] Pixel x relative to center
 * @param y      [f] Pixel y relative to center
 * @param flarec Flare counter? Always har->flarec...
 */
/*  void shadehalo(struct HaloRen *har, char *col, unsigned int zz, float dist, float x, float y, short flarec); */

/**
 * A cryptic but very efficient way of counting the number of bits that 
 * is set in the unsigned short.
 */
int count_mask(unsigned short mask);

/* These defines are only used internally :) */
/* dirty hack: pointers are negative, indices positive */
/* pointers should be converted to positive numbers */
	
#define IS_A_POINTER_CODE(a)		((a)<0)
#define POINTER_FROM_CODE(a)		((void *)(-(a)))
#define POINTER_TO_CODE(a)			(-(long)(a))

#endif /* RENDER_EXT_H */

