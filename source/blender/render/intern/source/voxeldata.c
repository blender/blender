/**
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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Raul Fernandez Hernandez (Farsthary), Matt Ebb.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "MEM_guardedalloc.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "BKE_global.h"
#include "BKE_main.h"

#include "DNA_texture_types.h"
#include "render_types.h"
#include "renderdatabase.h"
#include "texture.h"
#include "voxeldata.h"

#if defined( _MSC_VER ) && !defined( __cplusplus )
# define inline __inline
#endif // defined( _MSC_VER ) && !defined( __cplusplus )

/*---------------------------Utils----------------------------------------*/
inline int _I(int x, int y, int z, int *n)
{
	return (z*(n[1])+y)*(n[2])+x;
}

float Linear(float xx, float yy, float zz, float *x0, int *n)
{
	float sx1,sx0,sy1,sy0,sz1,sz0,v0,v1;
	int i0,i1,j0,j1,k0,k1;
	
	if (xx<0.5) xx=0.5f; if (xx>n[0]+0.5) xx=n[0]+0.5f; i0=(int)xx; i1=i0+1;
	if (yy<0.5) yy=0.5f; if (yy>n[1]+0.5) yy=n[1]+0.5f; j0=(int)yy; j1=j0+1;
	if (zz<0.5) zz=0.5f; if (zz>n[2]+0.5) zz=n[2]+0.5f; k0=(int)zz; k1=k0+1;
	
	sx1 = xx-i0; sx0 = 1-sx1;
	sy1 = yy-j0; sy0 = 1-sy1;
	sz1 = zz-k0; sz0 = 1-sz1;
	v0 = sx0*(sy0*x0[_I(i0,j0,k0,n)]+sy1*x0[_I(i0,j1,k0,n)])+sx1*(sy0*x0[_I(i1,j0,k0,n)]+sy1*x0[_I(i1,j1,k0,n)]);
	v1 = sx0*(sy0*x0[_I(i0,j0,k1,n)]+sy1*x0[_I(i0,j1,k1,n)])+sx1*(sy0*x0[_I(i1,j0,k1,n)]+sy1*x0[_I(i1,j1,k1,n)]);
	return sz0*v0 + sz1*v1;
}


static float D(float *data, int *res, int x, int y, int z)
{
	CLAMP(x, 0, res[0]-1);
	CLAMP(y, 0, res[1]-1);
	CLAMP(z, 0, res[2]-1);
	return data[ _I(x, y, z, res) ];
}

static inline float lerp(float t, float v1, float v2) {
	return (1.f - t) * v1 + t * v2;
}

/* trilinear interpolation */
static float trilinear(float *data, int *res, float *co)
{
	float voxx, voxy, voxz;
	int vx, vy, vz;
	float dx, dy, dz;
	float d00, d10, d01, d11, d0, d1, d_final;

	if (!data) return 0.f;
	
	voxx = co[0] * res[0] - 0.5f;
	voxy = co[1] * res[1] - 0.5f;
	voxz = co[2] * res[2] - 0.5f;
	
	vx = (int)voxx; vy = (int)voxy; vz = (int)voxz;
	
	dx = voxx - vx; dy = voxy - vy; dz = voxz - vz;
	
	d00 = lerp(dx, D(data, res, vx, vy, vz), 		D(data, res, vx+1, vy, vz));
	d10 = lerp(dx, D(data, res, vx, vy+1, vz), 		D(data, res, vx+1, vy+1, vz));
	d01 = lerp(dx, D(data, res, vx, vy, vz+1), 		D(data, res, vx+1, vy, vz+1));
	d11 = lerp(dx, D(data, res, vx, vy+1, vz+1), 	D(data, res, vx+1, vy+1, vz+1));
	d0 = lerp(dy, d00, d10);
	d1 = lerp(dy, d01, d11);
	d_final = lerp(dz, d0, d1);
	
	return d_final;
}


int C[64][64] = {
	{ 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{-3, 3, 0, 0, 0, 0, 0, 0,-2,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 2,-2, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{-3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2, 0,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0,-3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2, 0,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 9,-9,-9, 9, 0, 0, 0, 0, 6, 3,-6,-3, 0, 0, 0, 0, 6,-6, 3,-3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{-6, 6, 6,-6, 0, 0, 0, 0,-3,-3, 3, 3, 0, 0, 0, 0,-4, 4,-2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2,-2,-1,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 2, 0,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 2, 0,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{-6, 6, 6,-6, 0, 0, 0, 0,-4,-2, 4, 2, 0, 0, 0, 0,-3, 3,-3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2,-1,-2,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 4,-4,-4, 4, 0, 0, 0, 0, 2, 2,-2,-2, 0, 0, 0, 0, 2,-2, 2,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3, 3, 0, 0, 0, 0, 0, 0,-2,-1, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,-2, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2, 0,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2, 0,-1, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9,-9,-9, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 3,-6,-3, 0, 0, 0, 0, 6,-6, 3,-3, 0, 0, 0, 0, 4, 2, 2, 1, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-6, 6, 6,-6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3,-3, 3, 3, 0, 0, 0, 0,-4, 4,-2, 2, 0, 0, 0, 0,-2,-2,-1,-1, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-6, 6, 6,-6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-4,-2, 4, 2, 0, 0, 0, 0,-3, 3,-3, 3, 0, 0, 0, 0,-2,-1,-2,-1, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,-4,-4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2,-2,-2, 0, 0, 0, 0, 2,-2, 2,-2, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 0, 0},
	{-3, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2, 0, 0, 0,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0,-3, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2, 0, 0, 0,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 9,-9, 0, 0,-9, 9, 0, 0, 6, 3, 0, 0,-6,-3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6,-6, 0, 0, 3,-3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 2, 0, 0, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{-6, 6, 0, 0, 6,-6, 0, 0,-3,-3, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-4, 4, 0, 0,-2, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2,-2, 0, 0,-1,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2, 0, 0, 0,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3, 0, 0, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2, 0, 0, 0,-1, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 9,-9, 0, 0,-9, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 3, 0, 0,-6,-3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6,-6, 0, 0, 3,-3, 0, 0, 4, 2, 0, 0, 2, 1, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-6, 6, 0, 0, 6,-6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3,-3, 0, 0, 3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-4, 4, 0, 0,-2, 2, 0, 0,-2,-2, 0, 0,-1,-1, 0, 0},
	{ 9, 0,-9, 0,-9, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 3, 0,-6, 0,-3, 0, 6, 0,-6, 0, 3, 0,-3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 2, 0, 2, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 9, 0,-9, 0,-9, 0, 9, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 6, 0, 3, 0,-6, 0,-3, 0, 6, 0,-6, 0, 3, 0,-3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4, 0, 2, 0, 2, 0, 1, 0},
	{-27,27,27,-27,27,-27,-27,27,-18,-9,18, 9,18, 9,-18,-9,-18,18,-9, 9,18,-18, 9,-9,-18,18,18,-18,-9, 9, 9,-9,-12,-6,-6,-3,12, 6, 6, 3,-12,-6,12, 6,-6,-3, 6, 3,-12,12,-6, 6,-6, 6,-3, 3,-8,-4,-4,-2,-4,-2,-2,-1},
	{18,-18,-18,18,-18,18,18,-18, 9, 9,-9,-9,-9,-9, 9, 9,12,-12, 6,-6,-12,12,-6, 6,12,-12,-12,12, 6,-6,-6, 6, 6, 6, 3, 3,-6,-6,-3,-3, 6, 6,-6,-6, 3, 3,-3,-3, 8,-8, 4,-4, 4,-4, 2,-2, 4, 4, 2, 2, 2, 2, 1, 1},
	{-6, 0, 6, 0, 6, 0,-6, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3, 0,-3, 0, 3, 0, 3, 0,-4, 0, 4, 0,-2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2, 0,-2, 0,-1, 0,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0,-6, 0, 6, 0, 6, 0,-6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3, 0,-3, 0, 3, 0, 3, 0,-4, 0, 4, 0,-2, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2, 0,-2, 0,-1, 0,-1, 0},
	{18,-18,-18,18,-18,18,18,-18,12, 6,-12,-6,-12,-6,12, 6, 9,-9, 9,-9,-9, 9,-9, 9,12,-12,-12,12, 6,-6,-6, 6, 6, 3, 6, 3,-6,-3,-6,-3, 8, 4,-8,-4, 4, 2,-4,-2, 6,-6, 6,-6, 3,-3, 3,-3, 4, 2, 4, 2, 2, 1, 2, 1},
	{-12,12,12,-12,12,-12,-12,12,-6,-6, 6, 6, 6, 6,-6,-6,-6, 6,-6, 6, 6,-6, 6,-6,-8, 8, 8,-8,-4, 4, 4,-4,-3,-3,-3,-3, 3, 3, 3, 3,-4,-4, 4, 4,-2,-2, 2, 2,-4, 4,-4, 4,-2, 2,-2, 2,-2,-2,-2,-2,-1,-1,-1,-1},
	{ 2, 0, 0, 0,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{-6, 6, 0, 0, 6,-6, 0, 0,-4,-2, 0, 0, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3, 3, 0, 0,-3, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2,-1, 0, 0,-2,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 4,-4, 0, 0,-4, 4, 0, 0, 2, 2, 0, 0,-2,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,-2, 0, 0, 2,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 0, 0,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-6, 6, 0, 0, 6,-6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-4,-2, 0, 0, 4, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-3, 3, 0, 0,-3, 3, 0, 0,-2,-1, 0, 0,-2,-1, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 4,-4, 0, 0,-4, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 2, 0, 0,-2,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2,-2, 0, 0, 2,-2, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0},
	{-6, 0, 6, 0, 6, 0,-6, 0, 0, 0, 0, 0, 0, 0, 0, 0,-4, 0,-2, 0, 4, 0, 2, 0,-3, 0, 3, 0,-3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2, 0,-1, 0,-2, 0,-1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0,-6, 0, 6, 0, 6, 0,-6, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,-4, 0,-2, 0, 4, 0, 2, 0,-3, 0, 3, 0,-3, 0, 3, 0, 0, 0, 0, 0, 0, 0, 0, 0,-2, 0,-1, 0,-2, 0,-1, 0},
	{18,-18,-18,18,-18,18,18,-18,12, 6,-12,-6,-12,-6,12, 6,12,-12, 6,-6,-12,12,-6, 6, 9,-9,-9, 9, 9,-9,-9, 9, 8, 4, 4, 2,-8,-4,-4,-2, 6, 3,-6,-3, 6, 3,-6,-3, 6,-6, 3,-3, 6,-6, 3,-3, 4, 2, 2, 1, 4, 2, 2, 1},
	{-12,12,12,-12,12,-12,-12,12,-6,-6, 6, 6, 6, 6,-6,-6,-8, 8,-4, 4, 8,-8, 4,-4,-6, 6, 6,-6,-6, 6, 6,-6,-4,-4,-2,-2, 4, 4, 2, 2,-3,-3, 3, 3,-3,-3, 3, 3,-4, 4,-2, 2,-4, 4,-2, 2,-2,-2,-1,-1,-2,-2,-1,-1},
	{ 4, 0,-4, 0,-4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0,-2, 0,-2, 0, 2, 0,-2, 0, 2, 0,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	{ 0, 0, 0, 0, 0, 0, 0, 0, 4, 0,-4, 0,-4, 0, 4, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 2, 0,-2, 0,-2, 0, 2, 0,-2, 0, 2, 0,-2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0},
	{-12,12,12,-12,12,-12,-12,12,-8,-4, 8, 4, 8, 4,-8,-4,-6, 6,-6, 6, 6,-6, 6,-6,-6, 6, 6,-6,-6, 6, 6,-6,-4,-2,-4,-2, 4, 2, 4, 2,-4,-2, 4, 2,-4,-2, 4, 2,-3, 3,-3, 3,-3, 3,-3, 3,-2,-1,-2,-1,-2,-1,-2,-1},
	{ 8,-8,-8, 8,-8, 8, 8,-8, 4, 4,-4,-4,-4,-4, 4, 4, 4,-4, 4,-4,-4, 4,-4, 4, 4,-4,-4, 4, 4,-4,-4, 4, 2, 2, 2, 2,-2,-2,-2,-2, 2, 2,-2,-2, 2, 2,-2,-2, 2,-2, 2,-2, 2,-2, 2,-2, 1, 1, 1, 1, 1, 1, 1, 1}};

int ijk2n(int i, int j, int k) {
	return(i+4*j+16*k);
}

void tricubic_get_coeff_stacked(float a[64], float x[64]) {
	int i,j;
	for (i=0;i<64;i++) {
		a[i]=(float)(0.0);
		for (j=0;j<64;j++) {
			a[i]+=C[i][j]*x[j];
		}
	}
}

void point2xyz(int p, int *x, int *y, int *z) {
	switch (p) {
		case 0: *x=0; *y=0; *z=0; break;
		case 1: *x=1; *y=0; *z=0; break;
		case 2: *x=0; *y=1; *z=0; break;
		case 3: *x=1; *y=1; *z=0; break;
		case 4: *x=0; *y=0; *z=1; break;
		case 5: *x=1; *y=0; *z=1; break;
		case 6: *x=0; *y=1; *z=1; break;
		case 7: *x=1; *y=1; *z=1; break;
		default:*x=0; *y=0; *z=0;
	}
}


void tricubic_get_coeff(float a[64], float f[8], float dfdx[8], float dfdy[8], float dfdz[8], float d2fdxdy[8], float d2fdxdz[8], float d2fdydz[8], float d3fdxdydz[8]) {
	int i;
	float x[64];
	for (i=0;i<8;i++) {
		x[0+i]=f[i];
		x[8+i]=dfdx[i];
		x[16+i]=dfdy[i];
		x[24+i]=dfdz[i];
		x[32+i]=d2fdxdy[i];
		x[40+i]=d2fdxdz[i];
		x[48+i]=d2fdydz[i];
		x[56+i]=d3fdxdydz[i];
	}
	tricubic_get_coeff_stacked(a,x);
}

float tricubic_eval(float a[64], float x, float y, float z) {
	int i,j,k;
	float ret=(float)(0.0);
	
	for (i=0;i<4;i++) {
		for (j=0;j<4;j++) {
			for (k=0;k<4;k++) {
				ret+=a[ijk2n(i,j,k)]*pow(x,i)*pow(y,j)*pow(z,k);
			}
		}
	}
	return(ret);
}


float tricubic(float xx, float yy, float zz, float *heap, int *n)
{
	
	int xi,yi,zi;
	float dx,dy,dz;
	float a[64];

	if (xx<0.5) xx=0.5f; if (xx>n[0]+0.5) xx=n[0]+0.5f; xi=(int)xx;
	if (yy<0.5) yy=0.5f; if (yy>n[1]+0.5) yy=n[1]+0.5f; yi=(int)yy;
	if (zz<0.5) zz=0.5f; if (zz>n[2]+0.5) zz=n[2]+0.5f; zi=(int)zz;
	
	{
	float fval[8]={heap[_I(xi,yi,zi,n)],heap[_I(xi+1,yi,zi,n)],heap[_I(xi,yi+1,zi,n)],heap[_I(xi+1,yi+1,zi,n)],heap[_I(xi,yi,zi+1,n)],heap[_I(xi+1,yi,zi+1,n)],heap[_I(xi,yi+1,zi+1,n)],heap[_I(xi+1,yi+1,zi+1,n)]}; 
	
	float dfdxval[8]={0.5f*(heap[_I(xi+1,yi,zi,n)]-heap[_I(xi-1,yi,zi,n)]),0.5f*(heap[_I(xi+2,yi,zi,n)]-heap[_I(xi,yi,zi,n)]),
		0.5f*(heap[_I(xi+1,yi+1,zi,n)]-heap[_I(xi-1,yi+1,zi,n)]),0.5f*(heap[_I(xi+2,yi+1,zi,n)]-heap[_I(xi,yi+1,zi,n)]),
		0.5f*(heap[_I(xi+1,yi,zi+1,n)]-heap[_I(xi-1,yi,zi+1,n)]),0.5f*(heap[_I(xi+2,yi,zi+1,n)]-heap[_I(xi,yi,zi+1,n)]),
		0.5f*(heap[_I(xi+1,yi+1,zi+1,n)]-heap[_I(xi-1,yi+1,zi+1,n)]),
	0.5f*(heap[_I(xi+2,yi+1,zi+1,n)]-heap[_I(xi,yi+1,zi+1,n)])};						
	
	float dfdyval[8]={0.5f*(heap[_I(xi,yi+1,zi,n)]-heap[_I(xi,yi-1,zi,n)]),0.5f*(heap[_I(xi+1,yi+1,zi,n)]-heap[_I(xi+1,yi-1,zi,n)]),
		0.5f*(heap[_I(xi,yi+2,zi,n)]-heap[_I(xi,yi,zi,n)]),0.5f*(heap[_I(xi+1,yi+2,zi,n)]-heap[_I(xi+1,yi,zi,n)]),
		0.5f*(heap[_I(xi,yi+1,zi+1,n)]-heap[_I(xi,yi-1,zi+1,n)]),0.5f*(heap[_I(xi+1,yi+1,zi+1,n)]-heap[_I(xi+1,yi-1,zi+1,n)]),
		0.5f*(heap[_I(xi,yi+2,zi+1,n)]-heap[_I(xi,yi,zi+1,n)]),
	0.5f*(heap[_I(xi+1,yi+2,zi+1,n)]-heap[_I(xi+1,yi,zi+1,n)])};						 
	
	float dfdzval[8]={0.5f*(heap[_I(xi,yi,zi+1,n)]-heap[_I(xi,yi,zi-1,n)]),0.5f*(heap[_I(xi+1,yi,zi+1,n)]-heap[_I(xi+1,yi,zi-1,n)]),
		0.5f*(heap[_I(xi,yi+1,zi+1,n)]-heap[_I(xi,yi+1,zi-1,n)]),0.5f*(heap[_I(xi+1,yi+1,zi+1,n)]-heap[_I(xi+1,yi+1,zi-1,n)]),
		0.5f*(heap[_I(xi,yi,zi+2,n)]-heap[_I(xi,yi,zi,n)]),0.5f*(heap[_I(xi+1,yi,zi+2,n)]-heap[_I(xi+1,yi,zi,n)]),
		0.5f*(heap[_I(xi,yi+1,zi+2,n)]-heap[_I(xi,yi+1,zi,n)]),
	0.5f*(heap[_I(xi+1,yi+1,zi+2,n)]-heap[_I(xi+1,yi+1,zi,n)])};						 
	
	float d2fdxdyval[8]={0.25*(heap[_I(xi+1,yi+1,zi,n)]-heap[_I(xi-1,yi+1,zi,n)]-heap[_I(xi+1,yi-1,zi,n)]+heap[_I(xi-1,yi-1,zi,n)]),
		0.25*(heap[_I(xi+2,yi+1,zi,n)]-heap[_I(xi,yi+1,zi,n)]-heap[_I(xi+2,yi-1,zi,n)]+heap[_I(xi,yi-1,zi,n)]),
		0.25*(heap[_I(xi+1,yi+2,zi,n)]-heap[_I(xi-1,yi+2,zi,n)]-heap[_I(xi+1,yi,zi,n)]+heap[_I(xi-1,yi,zi,n)]),
		0.25*(heap[_I(xi+2,yi+2,zi,n)]-heap[_I(xi,yi+2,zi,n)]-heap[_I(xi+2,yi,zi,n)]+heap[_I(xi,yi,zi,n)]),
		0.25*(heap[_I(xi+1,yi+1,zi+1,n)]-heap[_I(xi-1,yi+1,zi+1,n)]-heap[_I(xi+1,yi-1,zi+1,n)]+heap[_I(xi-1,yi-1,zi+1,n)]),
		0.25*(heap[_I(xi+2,yi+1,zi+1,n)]-heap[_I(xi,yi+1,zi+1,n)]-heap[_I(xi+2,yi-1,zi+1,n)]+heap[_I(xi,yi-1,zi+1,n)]),
		0.25*(heap[_I(xi+1,yi+2,zi+1,n)]-heap[_I(xi-1,yi+2,zi+1,n)]-heap[_I(xi+1,yi,zi+1,n)]+heap[_I(xi-1,yi,zi+1,n)]),
	0.25*(heap[_I(xi+2,yi+2,zi+1,n)]-heap[_I(xi,yi+2,zi+1,n)]-heap[_I(xi+2,yi,zi+1,n)]+heap[_I(xi,yi,zi+1,n)])};						 
	
	float d2fdxdzval[8]={0.25f*(heap[_I(xi+1,yi,zi+1,n)]-heap[_I(xi-1,yi,zi+1,n)]-heap[_I(xi+1,yi,zi-1,n)]+heap[_I(xi-1,yi,zi-1,n)]),
		0.25f*(heap[_I(xi+2,yi,zi+1,n)]-heap[_I(xi,yi,zi+1,n)]-heap[_I(xi+2,yi,zi-1,n)]+heap[_I(xi,yi,zi-1,n)]),
		0.25f*(heap[_I(xi+1,yi+1,zi+1,n)]-heap[_I(xi-1,yi+1,zi+1,n)]-heap[_I(xi+1,yi+1,zi-1,n)]+heap[_I(xi-1,yi+1,zi-1,n)]),
		0.25f*(heap[_I(xi+2,yi+1,zi+1,n)]-heap[_I(xi,yi+1,zi+1,n)]-heap[_I(xi+2,yi+1,zi-1,n)]+heap[_I(xi,yi+1,zi-1,n)]),
		0.25f*(heap[_I(xi+1,yi,zi+2,n)]-heap[_I(xi-1,yi,zi+2,n)]-heap[_I(xi+1,yi,zi,n)]+heap[_I(xi-1,yi,zi,n)]),
		0.25f*(heap[_I(xi+2,yi,zi+2,n)]-heap[_I(xi,yi,zi+2,n)]-heap[_I(xi+2,yi,zi,n)]+heap[_I(xi,yi,zi,n)]),
		0.25f*(heap[_I(xi+1,yi+1,zi+2,n)]-heap[_I(xi-1,yi+1,zi+2,n)]-heap[_I(xi+1,yi+1,zi,n)]+heap[_I(xi-1,yi+1,zi,n)]),
	0.25f*(heap[_I(xi+2,yi+1,zi+2,n)]-heap[_I(xi,yi+1,zi+2,n)]-heap[_I(xi+2,yi+1,zi,n)]+heap[_I(xi,yi+1,zi,n)])};
	
	
	float d2fdydzval[8]={0.25f*(heap[_I(xi,yi+1,zi+1,n)]-heap[_I(xi,yi-1,zi+1,n)]-heap[_I(xi,yi+1,zi-1,n)]+heap[_I(xi,yi-1,zi-1,n)]),
		0.25f*(heap[_I(xi+1,yi+1,zi+1,n)]-heap[_I(xi+1,yi-1,zi+1,n)]-heap[_I(xi+1,yi+1,zi-1,n)]+heap[_I(xi+1,yi-1,zi-1,n)]),
		0.25f*(heap[_I(xi,yi+2,zi+1,n)]-heap[_I(xi,yi,zi+1,n)]-heap[_I(xi,yi+2,zi-1,n)]+heap[_I(xi,yi,zi-1,n)]),
		0.25f*(heap[_I(xi+1,yi+2,zi+1,n)]-heap[_I(xi+1,yi,zi+1,n)]-heap[_I(xi+1,yi+2,zi-1,n)]+heap[_I(xi+1,yi,zi-1,n)]),
		0.25f*(heap[_I(xi,yi+1,zi+2,n)]-heap[_I(xi,yi-1,zi+2,n)]-heap[_I(xi,yi+1,zi,n)]+heap[_I(xi,yi-1,zi,n)]),
		0.25f*(heap[_I(xi+1,yi+1,zi+2,n)]-heap[_I(xi+1,yi-1,zi+2,n)]-heap[_I(xi+1,yi+1,zi,n)]+heap[_I(xi+1,yi-1,zi,n)]),
		0.25f*(heap[_I(xi,yi+2,zi+2,n)]-heap[_I(xi,yi,zi+2,n)]-heap[_I(xi,yi+2,zi,n)]+heap[_I(xi,yi,zi,n)]),
	0.25f*(heap[_I(xi+1,yi+2,zi+2,n)]-heap[_I(xi+1,yi,zi+2,n)]-heap[_I(xi+1,yi+2,zi,n)]+heap[_I(xi+1,yi,zi,n)])};
	
	
	float d3fdxdydzval[8]={0.125f*(heap[_I(xi+1,yi+1,zi+1,n)]-heap[_I(xi-1,yi+1,zi+1,n)]-heap[_I(xi+1,yi-1,zi+1,n)]+heap[_I(xi-1,yi-1,zi+1,n)]-heap[_I(xi+1,yi+1,zi-1,n)]+heap[_I(xi-1,yi+1,zi-1,n)]+heap[_I(xi+1,yi-1,zi-1,n)]-heap[_I(xi-1,yi-1,zi-1,n)]),
		0.125f*(heap[_I(xi+2,yi+1,zi+1,n)]-heap[_I(xi,yi+1,zi+1,n)]-heap[_I(xi+2,yi-1,zi+1,n)]+heap[_I(xi,yi-1,zi+1,n)]-heap[_I(xi+2,yi+1,zi-1,n)]+heap[_I(xi,yi+1,zi-1,n)]+heap[_I(xi+2,yi-1,zi-1,n)]-heap[_I(xi,yi-1,zi-1,n)]),
		0.125f*(heap[_I(xi+1,yi+2,zi+1,n)]-heap[_I(xi-1,yi+2,zi+1,n)]-heap[_I(xi+1,yi,zi+1,n)]+heap[_I(xi-1,yi,zi+1,n)]-heap[_I(xi+1,yi+2,zi-1,n)]+heap[_I(xi-1,yi+2,zi-1,n)]+heap[_I(xi+1,yi,zi-1,n)]-heap[_I(xi-1,yi,zi-1,n)]),
		0.125f*(heap[_I(xi+2,yi+2,zi+1,n)]-heap[_I(xi,yi+2,zi+1,n)]-heap[_I(xi+2,yi,zi+1,n)]+heap[_I(xi,yi,zi+1,n)]-heap[_I(xi+2,yi+2,zi-1,n)]+heap[_I(xi,yi+2,zi-1,n)]+heap[_I(xi+2,yi,zi-1,n)]-heap[_I(xi,yi,zi-1,n)]),
		0.125f*(heap[_I(xi+1,yi+1,zi+2,n)]-heap[_I(xi-1,yi+1,zi+2,n)]-heap[_I(xi+1,yi-1,zi+2,n)]+heap[_I(xi-1,yi-1,zi+2,n)]-heap[_I(xi+1,yi+1,zi,n)]+heap[_I(xi-1,yi+1,zi,n)]+heap[_I(xi+1,yi-1,zi,n)]-heap[_I(xi-1,yi-1,zi,n)]),
		0.125f*(heap[_I(xi+2,yi+1,zi+2,n)]-heap[_I(xi,yi+1,zi+2,n)]-heap[_I(xi+2,yi-1,zi+2,n)]+heap[_I(xi,yi-1,zi+2,n)]-heap[_I(xi+2,yi+1,zi,n)]+heap[_I(xi,yi+1,zi,n)]+heap[_I(xi+2,yi-1,zi,n)]-heap[_I(xi,yi-1,zi,n)]),
		0.125f*(heap[_I(xi+1,yi+2,zi+2,n)]-heap[_I(xi-1,yi+2,zi+2,n)]-heap[_I(xi+1,yi,zi+2,n)]+heap[_I(xi-1,yi,zi+2,n)]-heap[_I(xi+1,yi+2,zi,n)]+heap[_I(xi-1,yi+2,zi,n)]+heap[_I(xi+1,yi,zi,n)]-heap[_I(xi-1,yi,zi,n)]),
	0.125f*(heap[_I(xi+2,yi+2,zi+2,n)]-heap[_I(xi,yi+2,zi+2,n)]-heap[_I(xi+2,yi,zi+2,n)]+heap[_I(xi,yi,zi+2,n)]-heap[_I(xi+2,yi+2,zi,n)]+heap[_I(xi,yi+2,zi,n)]+heap[_I(xi+2,yi,zi,n)]-heap[_I(xi,yi,zi,n)])};

	
	tricubic_get_coeff(a,fval,dfdxval,dfdyval,dfdzval,d2fdxdyval,d2fdxdzval,d2fdydzval,d3fdxdydzval);
	}
		
	dx = xx-xi;
	dy = yy-yi;
	dz = zz-zi;
	
	return tricubic_eval(a,dx,dy,dz);
	
}

void load_frame (FILE *fp, float *F, int size, int frame, int offset)
{	
	fseek(fp,frame*size*sizeof(float)+offset,0);
	fread(F,sizeof(float),size,fp);
}

void write_voxeldata_header(struct VoxelDataHeader *h, FILE *fp)
{
	fwrite(h,sizeof(struct VoxelDataHeader),1,fp);
}

void read_voxeldata_header(FILE *fp, struct VoxelData *vd)
{
	VoxelDataHeader *h=(VoxelDataHeader *)MEM_mallocN(sizeof(VoxelDataHeader), "voxel data header");
	
	rewind(fp);
	fread(h,sizeof(VoxelDataHeader),1,fp);
	
	vd->resolX=h->resolX;
	vd->resolY=h->resolY;
	vd->resolZ=h->resolZ;

	MEM_freeN(h);
}

void cache_voxeldata(struct Render *re,Tex *tex)
{	
	VoxelData *vd = tex->vd;
	FILE *fp;
	int size;
	
	if (!vd) return;
	
	if (!BLI_exists(vd->source_path)) return;
	fp = fopen(vd->source_path,"rb");
	if (!fp) return;
	
	read_voxeldata_header(fp, vd);
	size = (vd->resolX)*(vd->resolY)*(vd->resolZ);
	vd->dataset = MEM_mallocN(sizeof(float)*size, "voxel dataset");
	
	//here improve the dataset loading function for more dataset types
	if (vd->still) load_frame(fp, vd->dataset, size, vd->still_frame, sizeof(VoxelDataHeader));
	else load_frame(fp, vd->dataset, size, re->r.cfra, sizeof(VoxelDataHeader));
	
	fclose(fp);
}

void make_voxeldata(struct Render *re)
{
    Tex *tex;
	
	if(re->scene->r.scemode & R_PREVIEWBUTS)
		return;
	
	re->i.infostr= "Loading voxel datasets";
	re->stats_draw(&re->i);
	
	for (tex= G.main->tex.first; tex; tex= tex->id.next) {
		if(tex->id.us && tex->type==TEX_VOXELDATA) {
			cache_voxeldata(re, tex);
		}
	}
	
	re->i.infostr= NULL;
	re->stats_draw(&re->i);
	
}

static void free_voxeldata_one(Render *re, Tex *tex)
{
	VoxelData *vd = tex->vd;
	
	if (vd->dataset) {
		MEM_freeN(vd->dataset);
		vd->dataset = NULL;
	}
}


void free_voxeldata(Render *re)
{
	Tex *tex;
	
	if(re->scene->r.scemode & R_PREVIEWBUTS)
		return;
	
	for (tex= G.main->tex.first; tex; tex= tex->id.next) {
		if(tex->id.us && tex->type==TEX_VOXELDATA) {
			free_voxeldata_one(re, tex);
		}
	}
}

int voxeldatatex(struct Tex *tex, float *texvec, struct TexResult *texres)
{	 
    int retval = TEX_INT;
	VoxelData *vd = tex->vd;	
	float vec[3] = {0.0, 0.0, 0.0};	
	float co[3];
	float dx, dy, dz;
	int xi, yi, zi;
	float xf, yf, zf;
	int i=0, fail=0;
	int resol[3];
	
	if ((!vd) || (vd->dataset==NULL)) {
		texres->tin = 0.0f;
		return 0;
	}
	
	resol[0] = vd->resolX;
	resol[1] = vd->resolY;
	resol[2] = vd->resolZ;
	
	VECCOPY(co, texvec);	
	
	dx=1.0f/(resol[0]);
	dy=1.0f/(resol[1]);
	dz=1.0f/(resol[2]);
	
	xi=co[0]/dx;
	yi=co[1]/dy;
	zi=co[2]/dz;
		
	xf=co[0]/dx;
	yf=co[1]/dy;
	zf=co[2]/dz;
	
	if (xi>1 && xi<resol[0])
	{
		if (yi>1 && yi<resol[1])
		{
			if (zi>1 && zi<resol[2])
			{
				switch (vd->interp_type)
				{
					case TEX_VD_NEARESTNEIGHBOR:
					{
						texres->tin = vd->dataset[_I(xi,yi,zi,resol)];
						break;  
					}
					case TEX_VD_LINEAR:
					{
						texres->tin = trilinear(vd->dataset, resol, co);
						break;					
					}
					case TEX_VD_TRICUBIC:
					{
						texres->tin = tricubic(xf, yf, zf, vd->dataset, resol);
						break;
					}
				}				  				   
			} else fail++;
		} else fail++;
	} else fail++;
	
	if (fail) texres->tin=0.0f;

	texres->tin *= vd->int_multiplier;
	
	BRICONT;
	
	texres->tr = texres->tin;
	texres->tg = texres->tin;
	texres->tb = texres->tin;
	texres->ta = texres->tin;
	BRICONTRGB;
	
	return retval;	
}


