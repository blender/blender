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
struct ShadeResult;

/* ------------------------------------------------------------------------- */

/* to make passing on variables to shadepixel() easier */
typedef struct ShadePixelInfo {
	int thread;
	int layflag, passflag;
	unsigned int lay;
	ShadeResult shr;
} ShadePixelInfo;

typedef struct PixStr
{
	struct PixStr *next;
	int facenr, z;
	unsigned short mask, amount;
} PixStr;

typedef struct PixStrMain
{
	struct PixStrMain *next, *prev;
	struct PixStr *ps;
	int counter;
} PixStrMain;

/* ------------------------------------------------------------------------- */


void	calc_view_vector(float *view, float x, float y);
float   mistfactor(float zcor, float *co);	/* dist and height, return alpha */

void	add_halo_flare(void);

void	shade_input_set_coords(ShadeInput *shi, float u, float v, int i1, int i2, int i3);

void	shade_color(struct ShadeInput *shi, ShadeResult *shr);
void	shade_lamp_loop(struct ShadeInput *shi, ShadeResult *shr);

float	fresnel_fac(float *view, float *vn, float fresnel, float fac);
void	calc_R_ref(struct ShadeInput *shi);

/* for nodes */
void shade_material_loop(struct ShadeInput *shi, struct ShadeResult *shr);

void zbufshade(void);
void zbufshadeDA(void);	/* Delta Accum Pixel Struct */

void *shadepixel(ShadePixelInfo *shpi, float x, float y, int z, volatile int facenr, int mask, float *rco);
int count_mask(unsigned short mask);

void zbufshade_tile(struct RenderPart *pa);
void zbufshadeDA_tile(struct RenderPart *pa);

/* -------- ray.c ------- */

extern void freeoctree(Render *re);
extern void makeoctree(Render *re);

extern void ray_shadow(ShadeInput *, LampRen *, float *);
extern void ray_trace(ShadeInput *, ShadeResult *);
extern void ray_ao(ShadeInput *, float *);
extern void init_jitter_plane(LampRen *lar);
extern void init_ao_sphere(float *sphere, int tot, int iter);

#endif /* RENDER_EXT_H */

