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


/* vector defines */

#define CROSS(dest, a, b)		dest[0]= a[1] * b[2] - a[2] * b[1]; dest[1]= a[2] * b[0] - a[0] * b[2]; dest[2]= a[0] * b[1] - a[1] * b[0]
#define VECMUL(dest, f)			dest[0]*= f; dest[1]*= f; dest[2]*= f

struct HaloRen;
struct ShadeInput;

typedef struct ShadeResult 
{
	float diff[3];
	float spec[3];
	float alpha;

} ShadeResult;

typedef struct PixStr
{
	struct PixStr *next;
	int vlak0, vlak;
	unsigned int z;
	unsigned int mask;
	short aantal, ronde;
} PixStr;

/* ------------------------------------------------------------------------- */

typedef struct PixStrMain
{
	struct PixStr *ps;
	struct PixStrMain *next;
} PixStrMain;


float   mistfactor(float zcor, float *co);	/* dist and height, return alpha */

void	add_halo_flare(void);

void	shade_input_set_coords(ShadeInput *shi, float u, float v, int i1, int i2, int i3);

void	shade_color(struct ShadeInput *shi, ShadeResult *shr);
void	shade_lamp_loop(struct ShadeInput *shi, ShadeResult *shr);

float	fresnel_fac(float *view, float *vn, float fresnel, float fac);
void	calc_R_ref(struct ShadeInput *shi);
float	spec(float inp, int hard);

/* -------- ray.c ------- */

extern void ray_shadow(ShadeInput *, LampRen *, float *);
extern void ray_trace(ShadeInput *, ShadeResult *);
extern void ray_ao(ShadeInput *, World *, float *);

/**
 * Do z buffer and shade
 */
void zbufshade(void);

/**
 * zbuffer and shade, anti aliased
 */
void zbufshadeDA(void);	/* Delta Accum Pixel Struct */

/**
 * Also called in: zbuf.c
 */
void *shadepixel(float x, float y, int vlaknr, int mask, float *col);

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

