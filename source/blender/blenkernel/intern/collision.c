/*  collision.c      
* 
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
* The Original Code is Copyright (C) Blender Foundation
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): none yet.
*
* ***** END GPL/BL DUAL LICENSE BLOCK *****
*/

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "MEM_guardedalloc.h"
/* types */
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_cloth_types.h"	
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_lattice_types.h"
#include "DNA_scene_types.h"
#include "DNA_modifier_types.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_edgehash.h"
#include "BLI_linklist.h"
#include "BKE_collisions.h"
#include "BKE_curve.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_cloth.h"
#include "BKE_modifier.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "DNA_screen_types.h"
#include "BSE_headerbuttons.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "mydevice.h"

#include "Bullet-C-Api.h"

// step is limited from 0 (frame start position) to 1 (frame end position)
void collision_move_object(CollisionModifierData *collmd, float step, float prevstep)
{
	float tv[3] = {0,0,0};
	unsigned int i = 0;
	
	for ( i = 0; i < collmd->numverts; i++ )
	{
		VECSUB(tv, collmd->xnew[i].co, collmd->x[i].co);
		VECADDS(collmd->current_x[i].co, collmd->x[i].co, tv, prevstep);
		VECADDS(collmd->current_xnew[i].co, collmd->x[i].co, tv, step);
		VECSUB(collmd->current_v[i].co, collmd->current_xnew[i].co, collmd->current_x[i].co);
	}
}


/**
 * gsl_poly_solve_cubic -
 *
 * copied from SOLVE_CUBIC.C --> GSL
 */
#define mySWAP(a,b) { float tmp = b ; b = a ; a = tmp ; }

int gsl_poly_solve_cubic (float a, float b, float c, float *x0, float *x1, float *x2)
{
	float q = (a * a - 3 * b);
	float r = (2 * a * a * a - 9 * a * b + 27 * c);

	float Q = q / 9;
	float R = r / 54;

	float Q3 = Q * Q * Q;
	float R2 = R * R;

	float CR2 = 729 * r * r;
	float CQ3 = 2916 * q * q * q;

	if (R == 0 && Q == 0)
	{
		*x0 = - a / 3 ;
		*x1 = - a / 3 ;
		*x2 = - a / 3 ;
		return 3 ;
	}
	else if (CR2 == CQ3) 
	{
	  /* this test is actually R2 == Q3, written in a form suitable
		for exact computation with integers */

	  /* Due to finite precision some float roots may be missed, and
		considered to be a pair of complex roots z = x +/- epsilon i
		close to the real axis. */

		float sqrtQ = sqrtf (Q);

		if (R > 0)
		{
			*x0 = -2 * sqrtQ  - a / 3;
			*x1 = sqrtQ - a / 3;
			*x2 = sqrtQ - a / 3;
		}
		else
		{
			*x0 = - sqrtQ  - a / 3;
			*x1 = - sqrtQ - a / 3;
			*x2 = 2 * sqrtQ - a / 3;
		}
		return 3 ;
	}
	else if (CR2 < CQ3) /* equivalent to R2 < Q3 */
	{
		float sqrtQ = sqrtf (Q);
		float sqrtQ3 = sqrtQ * sqrtQ * sqrtQ;
		float theta = acosf (R / sqrtQ3);
		float norm = -2 * sqrtQ;
		*x0 = norm * cosf (theta / 3) - a / 3;
		*x1 = norm * cosf ((theta + 2.0 * M_PI) / 3) - a / 3;
		*x2 = norm * cosf ((theta - 2.0 * M_PI) / 3) - a / 3;
      
		/* Sort *x0, *x1, *x2 into increasing order */

		if (*x0 > *x1)
			mySWAP(*x0, *x1) ;
      
		if (*x1 > *x2)
		{
			mySWAP(*x1, *x2) ;
          
			if (*x0 > *x1)
				mySWAP(*x0, *x1) ;
		}
      
		return 3;
	}
	else
	{
		float sgnR = (R >= 0 ? 1 : -1);
		float A = -sgnR * powf (fabs (R) + sqrtf (R2 - Q3), 1.0/3.0);
		float B = Q / A ;
		*x0 = A + B - a / 3;
		return 1;
	}
}


/**
 * gsl_poly_solve_quadratic
 *
 * copied from GSL
 */
int gsl_poly_solve_quadratic (float a, float b, float c,  float *x0, float *x1)
{
	float disc = b * b - 4 * a * c;

	if (disc > 0)
	{
		if (b == 0)
		{
			float r = fabs (0.5 * sqrtf (disc) / a);
			*x0 = -r;
			*x1 =  r;
		}
		else
		{
			float sgnb = (b > 0 ? 1 : -1);
			float temp = -0.5 * (b + sgnb * sqrtf (disc));
			float r1 = temp / a ;
			float r2 = c / temp ;

			if (r1 < r2) 
			{
				*x0 = r1 ;
				*x1 = r2 ;
			} 
			else 
			{
				*x0 = r2 ;
				*x1 = r1 ;
			}
		}
		return 2;
	}
	else if (disc == 0) 
	{
		*x0 = -0.5 * b / a ;
		*x1 = -0.5 * b / a ;
		return 2 ;
	}
	else
	{
		return 0;
	}
}



/*
 * See Bridson et al. "Robust Treatment of Collision, Contact and Friction for Cloth Animation"
 *     page 4, left column
 */

int collisions_get_collision_time(float a[3], float b[3], float c[3], float d[3], float e[3], float f[3], float solution[3]) 
{
	int num_sols = 0;
	
	float g = -a[2] * c[1] * e[0] + a[1] * c[2] * e[0] +
			a[2] * c[0] * e[1] - a[0] * c[2] * e[1] -
			a[1] * c[0] * e[2] + a[0] * c[1] * e[2];

	float h = -b[2] * c[1] * e[0] + b[1] * c[2] * e[0] - a[2] * d[1] * e[0] +
			a[1] * d[2] * e[0] + b[2] * c[0] * e[1] - b[0] * c[2] * e[1] +
			a[2] * d[0] * e[1] - a[0] * d[2] * e[1] - b[1] * c[0] * e[2] +
			b[0] * c[1] * e[2] - a[1] * d[0] * e[2] + a[0] * d[1] * e[2] -
			a[2] * c[1] * f[0] + a[1] * c[2] * f[0] + a[2] * c[0] * f[1] -
			a[0] * c[2] * f[1] - a[1] * c[0] * f[2] + a[0] * c[1] * f[2];

	float i = -b[2] * d[1] * e[0] + b[1] * d[2] * e[0] +
			b[2] * d[0] * e[1] - b[0] * d[2] * e[1] -
			b[1] * d[0] * e[2] + b[0] * d[1] * e[2] -
			b[2] * c[1] * f[0] + b[1] * c[2] * f[0] -
			a[2] * d[1] * f[0] + a[1] * d[2] * f[0] +
			b[2] * c[0] * f[1] - b[0] * c[2] * f[1] + 
			a[2] * d[0] * f[1] - a[0] * d[2] * f[1] -
			b[1] * c[0] * f[2] + b[0] * c[1] * f[2] -
			a[1] * d[0] * f[2] + a[0] * d[1] * f[2];

	float j = -b[2] * d[1] * f[0] + b[1] * d[2] * f[0] +
			b[2] * d[0] * f[1] - b[0] * d[2] * f[1] -
			b[1] * d[0] * f[2] + b[0] * d[1] * f[2];

	// Solve cubic equation to determine times t1, t2, t3, when the collision will occur.
	if(ABS(j) > ALMOST_ZERO)
	{
		i /= j;
		h /= j;
		g /= j;
		
		num_sols = gsl_poly_solve_cubic(i, h, g, &solution[0], &solution[1], &solution[2]);
	}
	else if(ABS(i) > ALMOST_ZERO)
	{	
		num_sols = gsl_poly_solve_quadratic(i, h, g, &solution[0], &solution[1]);
		solution[2] = -1.0;
	}
	else if(ABS(h) > ALMOST_ZERO)
	{
		solution[0] = -g / h;
		solution[1] = solution[2] = -1.0;
		num_sols = 1;
	}
	else if(ABS(g) > ALMOST_ZERO)
	{
		solution[0] = 0;
		solution[1] = solution[2] = -1.0;
		num_sols = 1;
	}

	// Discard negative solutions
	if ((num_sols >= 1) && (solution[0] < 0)) 
	{
		--num_sols;
		solution[0] = solution[num_sols];
	}
	if ((num_sols >= 2) && (solution[1] < 0)) 
	{
		--num_sols;
		solution[1] = solution[num_sols];
	}
	if ((num_sols == 3) && (solution[2] < 0)) 
	{
		--num_sols;
	}

	// Sort
	if (num_sols == 2) 
	{
		if (solution[0] > solution[1]) 
		{
			double tmp = solution[0];
			solution[0] = solution[1];
			solution[1] = tmp;
		}
	}
	else if (num_sols == 3) 
	{

		// Bubblesort
		if (solution[0] > solution[1]) {
			double tmp = solution[0]; solution[0] = solution[1]; solution[1] = tmp;
		}
		if (solution[1] > solution[2]) {
			double tmp = solution[1]; solution[1] = solution[2]; solution[2] = tmp;
		}
		if (solution[0] > solution[1]) {
			double tmp = solution[0]; solution[0] = solution[1]; solution[1] = tmp;
		}
	}

	return num_sols;
}

// w3 is not perfect
void collisions_compute_barycentric (float pv[3], float p1[3], float p2[3], float p3[3], float *w1, float *w2, float *w3)
{
	double	tempV1[3], tempV2[3], tempV4[3];
	double	a,b,c,d,e,f;

	VECSUB (tempV1, p1, p3);	
	VECSUB (tempV2, p2, p3);	
	VECSUB (tempV4, pv, p3);	
	
	a = INPR (tempV1, tempV1);	
	b = INPR (tempV1, tempV2);	
	c = INPR (tempV2, tempV2);	
	e = INPR (tempV1, tempV4);	
	f = INPR (tempV2, tempV4);	
	
	d = (a * c - b * b);
	
	if (ABS(d) < ALMOST_ZERO) {
		*w1 = *w2 = *w3 = 1.0 / 3.0;
		return;
	}
	
	w1[0] = (float)((e * c - b * f) / d);
	
	if(w1[0] < 0)
		w1[0] = 0;
	
	w2[0] = (float)((f - b * (double)w1[0]) / c);
	
	if(w2[0] < 0)
		w2[0] = 0;
	
	w3[0] = 1.0f - w1[0] - w2[0];
}

DO_INLINE void interpolateOnTriangle(float to[3], float v1[3], float v2[3], float v3[3], double w1, double w2, double w3) 
{
	to[0] = to[1] = to[2] = 0;
	VECADDMUL(to, v1, w1);
	VECADDMUL(to, v2, w2);
	VECADDMUL(to, v3, w3);
}

