/*  collision.c
*
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
* The Original Code is Copyright (C) Blender Foundation
* All rights reserved.
*
* The Original Code is: all of this file.
*
* Contributor(s): none yet.
*
* ***** END GPL LICENSE BLOCK *****
*/

#include "MEM_guardedalloc.h"

#include "BKE_cloth.h"

#include "DNA_group_types.h"
#include "DNA_object_types.h"
#include "DNA_cloth_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_modifier.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "mydevice.h"

#include "Bullet-C-Api.h"

#include "BLI_kdopbvh.h"
#include "BKE_collision.h"

#ifdef _WIN32
static void start ( void )
{}
static void end ( void )
{
}
static double val()
{
	return 0;
}
#else
#include <sys/time.h>
static void mystart ( struct timeval *start, struct timezone *z )
{
	gettimeofday ( start, z );
}
static void myend ( struct timeval *end, struct timezone *z )
{
	gettimeofday ( end,z );
}
static double myval ( struct timeval *start, struct timeval *end )
{
	double t1, t2;
	t1 = ( double ) start->tv_sec + ( double ) start->tv_usec/ ( 1000*1000 );
	t2 = ( double ) end->tv_sec + ( double ) end->tv_usec/ ( 1000*1000 );
	return t2-t1;
}
#endif

/***********************************
Collision modifier code start
***********************************/

/* step is limited from 0 (frame start position) to 1 (frame end position) */
void collision_move_object ( CollisionModifierData *collmd, float step, float prevstep )
{
	float tv[3] = {0,0,0};
	unsigned int i = 0;

	for ( i = 0; i < collmd->numverts; i++ )
	{
		VECSUB ( tv, collmd->xnew[i].co, collmd->x[i].co );
		VECADDS ( collmd->current_x[i].co, collmd->x[i].co, tv, prevstep );
		VECADDS ( collmd->current_xnew[i].co, collmd->x[i].co, tv, step );
		VECSUB ( collmd->current_v[i].co, collmd->current_xnew[i].co, collmd->current_x[i].co );
	}
	bvhtree_update_from_mvert ( collmd->bvhtree, collmd->mfaces, collmd->numfaces, collmd->current_x, collmd->current_xnew, collmd->numverts, 1 );
}

BVHTree *bvhtree_build_from_mvert ( MFace *mfaces, unsigned int numfaces, MVert *x, unsigned int numverts, float epsilon )
{
	BVHTree *tree;
	float co[12];
	int i;
	MFace *tface = mfaces;

	tree = BLI_bvhtree_new ( numfaces*2, epsilon, 4, 26 );

	// fill tree
	for ( i = 0; i < numfaces; i++, tface++ )
	{
		VECCOPY ( &co[0*3], x[tface->v1].co );
		VECCOPY ( &co[1*3], x[tface->v2].co );
		VECCOPY ( &co[2*3], x[tface->v3].co );
		if ( tface->v4 )
			VECCOPY ( &co[3*3], x[tface->v4].co );

		BLI_bvhtree_insert ( tree, i, co, ( mfaces->v4 ? 4 : 3 ) );
	}

	// balance tree
	BLI_bvhtree_balance ( tree );

	return tree;
}

void bvhtree_update_from_mvert ( BVHTree * bvhtree, MFace *faces, int numfaces, MVert *x, MVert *xnew, int numverts, int moving )
{
	int i;
	MFace *mfaces = faces;
	float co[12], co_moving[12];
	int ret = 0;

	if ( !bvhtree )
		return;

	if ( x )
	{
		for ( i = 0; i < numfaces; i++, mfaces++ )
		{
			VECCOPY ( &co[0*3], x[mfaces->v1].co );
			VECCOPY ( &co[1*3], x[mfaces->v2].co );
			VECCOPY ( &co[2*3], x[mfaces->v3].co );
			if ( mfaces->v4 )
				VECCOPY ( &co[3*3], x[mfaces->v4].co );

			// copy new locations into array
			if ( moving && xnew )
			{
				// update moving positions
				VECCOPY ( &co_moving[0*3], xnew[mfaces->v1].co );
				VECCOPY ( &co_moving[1*3], xnew[mfaces->v2].co );
				VECCOPY ( &co_moving[2*3], xnew[mfaces->v3].co );
				if ( mfaces->v4 )
					VECCOPY ( &co_moving[3*3], xnew[mfaces->v4].co );

				ret = BLI_bvhtree_update_node ( bvhtree, i, co, co_moving, ( mfaces->v4 ? 4 : 3 ) );
			}
			else
			{
				ret = BLI_bvhtree_update_node ( bvhtree, i, co, NULL, ( mfaces->v4 ? 4 : 3 ) );
			}

			// check if tree is already full
			if ( !ret )
				break;
		}

		BLI_bvhtree_update_tree ( bvhtree );
	}
}

/***********************************
Collision modifier code end
***********************************/

/**
 * gsl_poly_solve_cubic -
 *
 * copied from SOLVE_CUBIC.C --> GSL
 */

#define mySWAP(a,b) do { double tmp = b ; b = a ; a = tmp ; } while(0)

int 
		gsl_poly_solve_cubic (double a, double b, double c, 
							  double *x0, double *x1, double *x2)
{
	double q = (a * a - 3 * b);
	double r = (2 * a * a * a - 9 * a * b + 27 * c);

	double Q = q / 9;
	double R = r / 54;

	double Q3 = Q * Q * Q;
	double R2 = R * R;

	double CR2 = 729 * r * r;
	double CQ3 = 2916 * q * q * q;

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

      /* Due to finite precision some double roots may be missed, and
		considered to be a pair of complex roots z = x +/- epsilon i
		close to the real axis. */

		double sqrtQ = sqrt (Q);

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
		double sqrtQ = sqrt (Q);
		double sqrtQ3 = sqrtQ * sqrtQ * sqrtQ;
		double theta = acos (R / sqrtQ3);
		double norm = -2 * sqrtQ;
		*x0 = norm * cos (theta / 3) - a / 3;
		*x1 = norm * cos ((theta + 2.0 * M_PI) / 3) - a / 3;
		*x2 = norm * cos ((theta - 2.0 * M_PI) / 3) - a / 3;
      
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
		double sgnR = (R >= 0 ? 1 : -1);
		double A = -sgnR * pow (fabs (R) + sqrt (R2 - Q3), 1.0/3.0);
		double B = Q / A ;
		*x0 = A + B - a / 3;
		return 1;
	}
}



/**
 * gsl_poly_solve_quadratic
 *
 * copied from GSL
 */
int 
		gsl_poly_solve_quadratic (double a, double b, double c, 
								  double *x0, double *x1)
{
	double disc = b * b - 4 * a * c;

	if (a == 0) /* Handle linear case */
	{
		if (b == 0)
		{
			return 0;
		}
		else
		{
			*x0 = -c / b;
			return 1;
		};
	}

	if (disc > 0)
	{
		if (b == 0)
		{
			double r = fabs (0.5 * sqrt (disc) / a);
			*x0 = -r;
			*x1 =  r;
		}
		else
		{
			double sgnb = (b > 0 ? 1 : -1);
			double temp = -0.5 * (b + sgnb * sqrt (disc));
			double r1 = temp / a ;
			double r2 = c / temp ;

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
int cloth_get_collision_time ( double a[3], double b[3], double c[3], double d[3], double e[3], double f[3], double solution[3] )
{
	int num_sols = 0;

	// x^0 - checked 
	double g = 	a[0] * c[1] * e[2] - a[0] * c[2] * e[1] +
				a[1] * c[2] * e[0] - a[1] * c[0] * e[2] + 
				a[2] * c[0] * e[1] - a[2] * c[1] * e[0];
	
	// x^1
	double h = -b[2] * c[1] * e[0] + b[1] * c[2] * e[0] - a[2] * d[1] * e[0] +
			a[1] * d[2] * e[0] + b[2] * c[0] * e[1] - b[0] * c[2] * e[1] +
			a[2] * d[0] * e[1] - a[0] * d[2] * e[1] - b[1] * c[0] * e[2] +
			b[0] * c[1] * e[2] - a[1] * d[0] * e[2] + a[0] * d[1] * e[2] -
			a[2] * c[1] * f[0] + a[1] * c[2] * f[0] + a[2] * c[0] * f[1] -
			a[0] * c[2] * f[1] - a[1] * c[0] * f[2] + a[0] * c[1] * f[2];

	// x^2
	double i = -b[2] * d[1] * e[0] + b[1] * d[2] * e[0] +
			b[2] * d[0] * e[1] - b[0] * d[2] * e[1] -
			b[1] * d[0] * e[2] + b[0] * d[1] * e[2] -
			b[2] * c[1] * f[0] + b[1] * c[2] * f[0] -
			a[2] * d[1] * f[0] + a[1] * d[2] * f[0] +
			b[2] * c[0] * f[1] - b[0] * c[2] * f[1] + 
			a[2] * d[0] * f[1] - a[0] * d[2] * f[1] -
			b[1] * c[0] * f[2] + b[0] * c[1] * f[2] -
			a[1] * d[0] * f[2] + a[0] * d[1] * f[2];
	
	// x^3 - checked
	double j = -b[2] * d[1] * f[0] + b[1] * d[2] * f[0] +
			b[2] * d[0] * f[1] - b[0] * d[2] * f[1] -
			b[1] * d[0] * f[2] + b[0] * d[1] * f[2];
	
	/*
	printf("r1: %lf\n", a[0] * c[1] * e[2] - a[0] * c[2] * e[1]);
	printf("r2: %lf\n", a[1] * c[2] * e[0] - a[1] * c[0] * e[2]);
	printf("r3: %lf\n", a[2] * c[0] * e[1] - a[2] * c[1] * e[0]);
	
	printf("x1 x: %f, y: %f, z: %f\n", a[0], a[1], a[2]);
	printf("x2 x: %f, y: %f, z: %f\n", c[0], c[1], c[2]);
	printf("x3 x: %f, y: %f, z: %f\n", e[0], e[1], e[2]);
	
	printf("v1 x: %f, y: %f, z: %f\n", b[0], b[1], b[2]);
	printf("v2 x: %f, y: %f, z: %f\n", d[0], d[1], d[2]);
	printf("v3 x: %f, y: %f, z: %f\n", f[0], f[1], f[2]);
	
	printf("t^3: %lf, t^2: %lf, t^1: %lf, t^0: %lf\n", j, i, h, g);
	*/

	// Solve cubic equation to determine times t1, t2, t3, when the collision will occur.
	if ( ABS ( j ) > DBL_EPSILON )
	{
		i /= j;
		h /= j;
		g /= j;
		num_sols = gsl_poly_solve_cubic ( i, h, g, &solution[0], &solution[1], &solution[2] );
	}
	else
	{
		num_sols = gsl_poly_solve_quadratic ( i, h, g, &solution[0], &solution[1] );
		solution[2] = -1.0;
	}

	// printf("num_sols: %d, sol1: %lf, sol2: %lf, sol3: %lf\n", num_sols, solution[0],  solution[1],  solution[2]);

	// Discard negative solutions
	if ( ( num_sols >= 1 ) && ( solution[0] < DBL_EPSILON ) )
	{
		--num_sols;
		solution[0] = solution[num_sols];
	}
	if ( ( num_sols >= 2 ) && ( solution[1] < DBL_EPSILON ) )
	{
		--num_sols;
		solution[1] = solution[num_sols];
	}
	if ( ( num_sols == 3 ) && ( solution[2] < DBL_EPSILON ) )
	{
		--num_sols;
	}

	// Sort
	if ( num_sols == 2 )
	{
		if ( solution[0] > solution[1] )
		{
			double tmp = solution[0];
			solution[0] = solution[1];
			solution[1] = tmp;
		}
	}
	else if ( num_sols == 3 )
	{

		// Bubblesort
		if ( solution[0] > solution[1] )
		{
			double tmp = solution[0]; solution[0] = solution[1]; solution[1] = tmp;
		}
		if ( solution[1] > solution[2] )
		{
			double tmp = solution[1]; solution[1] = solution[2]; solution[2] = tmp;
		}
		if ( solution[0] > solution[1] )
		{
			double tmp = solution[0]; solution[0] = solution[1]; solution[1] = tmp;
		}
	}

	return num_sols;
}

// w3 is not perfect
void collision_compute_barycentric ( float pv[3], float p1[3], float p2[3], float p3[3], float *w1, float *w2, float *w3 )
{
	double	tempV1[3], tempV2[3], tempV4[3];
	double	a,b,c,d,e,f;

	VECSUB ( tempV1, p1, p3 );
	VECSUB ( tempV2, p2, p3 );
	VECSUB ( tempV4, pv, p3 );

	a = INPR ( tempV1, tempV1 );
	b = INPR ( tempV1, tempV2 );
	c = INPR ( tempV2, tempV2 );
	e = INPR ( tempV1, tempV4 );
	f = INPR ( tempV2, tempV4 );

	d = ( a * c - b * b );

	if ( ABS ( d ) < ALMOST_ZERO )
	{
		*w1 = *w2 = *w3 = 1.0 / 3.0;
		return;
	}

	w1[0] = ( float ) ( ( e * c - b * f ) / d );

	if ( w1[0] < 0 )
		w1[0] = 0;

	w2[0] = ( float ) ( ( f - b * ( double ) w1[0] ) / c );

	if ( w2[0] < 0 )
		w2[0] = 0;

	w3[0] = 1.0f - w1[0] - w2[0];
}

DO_INLINE void collision_interpolateOnTriangle ( float to[3], float v1[3], float v2[3], float v3[3], double w1, double w2, double w3 )
{
	to[0] = to[1] = to[2] = 0;
	VECADDMUL ( to, v1, w1 );
	VECADDMUL ( to, v2, w2 );
	VECADDMUL ( to, v3, w3 );
}

int cloth_collision_response_static ( ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair, CollPair *collision_end )
{
	int result = 0;
	Cloth *cloth1;
	float w1, w2, w3, u1, u2, u3;
	float v1[3], v2[3], relativeVelocity[3];
	float magrelVel;
	float epsilon2 = BLI_bvhtree_getepsilon ( collmd->bvhtree );

	cloth1 = clmd->clothObject;

	for ( ; collpair != collision_end; collpair++ )
	{
		// only handle static collisions here
		if ( collpair->flag & COLLISION_IN_FUTURE )
			continue;

		// compute barycentric coordinates for both collision points
		collision_compute_barycentric ( collpair->pa,
		                                cloth1->verts[collpair->ap1].txold,
		                                cloth1->verts[collpair->ap2].txold,
		                                cloth1->verts[collpair->ap3].txold,
		                                &w1, &w2, &w3 );

		// was: txold
		collision_compute_barycentric ( collpair->pb,
		                                collmd->current_x[collpair->bp1].co,
		                                collmd->current_x[collpair->bp2].co,
		                                collmd->current_x[collpair->bp3].co,
		                                &u1, &u2, &u3 );

		// Calculate relative "velocity".
		collision_interpolateOnTriangle ( v1, cloth1->verts[collpair->ap1].tv, cloth1->verts[collpair->ap2].tv, cloth1->verts[collpair->ap3].tv, w1, w2, w3 );

		collision_interpolateOnTriangle ( v2, collmd->current_v[collpair->bp1].co, collmd->current_v[collpair->bp2].co, collmd->current_v[collpair->bp3].co, u1, u2, u3 );

		VECSUB ( relativeVelocity, v2, v1 );

		// Calculate the normal component of the relative velocity (actually only the magnitude - the direction is stored in 'normal').
		magrelVel = INPR ( relativeVelocity, collpair->normal );

		// printf("magrelVel: %f\n", magrelVel);

		// Calculate masses of points.
		// TODO

		// If v_n_mag < 0 the edges are approaching each other.
		if ( magrelVel > ALMOST_ZERO )
		{
			// Calculate Impulse magnitude to stop all motion in normal direction.
			float magtangent = 0, repulse = 0, d = 0;
			double impulse = 0.0;
			float vrel_t_pre[3];
			float temp[3];

			// calculate tangential velocity
			VECCOPY ( temp, collpair->normal );
			VecMulf ( temp, magrelVel );
			VECSUB ( vrel_t_pre, relativeVelocity, temp );

			// Decrease in magnitude of relative tangential velocity due to coulomb friction
			// in original formula "magrelVel" should be the "change of relative velocity in normal direction"
			magtangent = MIN2 ( clmd->coll_parms->friction * 0.01 * magrelVel,sqrt ( INPR ( vrel_t_pre,vrel_t_pre ) ) );

			// Apply friction impulse.
			if ( magtangent > ALMOST_ZERO )
			{
				Normalize ( vrel_t_pre );

				impulse = 2.0 * magtangent / ( 1.0 + w1*w1 + w2*w2 + w3*w3 );
				VECADDMUL ( cloth1->verts[collpair->ap1].impulse, vrel_t_pre, w1 * impulse );
				VECADDMUL ( cloth1->verts[collpair->ap2].impulse, vrel_t_pre, w2 * impulse );
				VECADDMUL ( cloth1->verts[collpair->ap3].impulse, vrel_t_pre, w3 * impulse );
			}

			// Apply velocity stopping impulse
			// I_c = m * v_N / 2.0
			// no 2.0 * magrelVel normally, but looks nicer DG
			impulse =  magrelVel / ( 1.0 + w1*w1 + w2*w2 + w3*w3 );

			VECADDMUL ( cloth1->verts[collpair->ap1].impulse, collpair->normal, w1 * impulse );
			cloth1->verts[collpair->ap1].impulse_count++;

			VECADDMUL ( cloth1->verts[collpair->ap2].impulse, collpair->normal, w2 * impulse );
			cloth1->verts[collpair->ap2].impulse_count++;

			VECADDMUL ( cloth1->verts[collpair->ap3].impulse, collpair->normal, w3 * impulse );
			cloth1->verts[collpair->ap3].impulse_count++;

			// Apply repulse impulse if distance too short
			// I_r = -min(dt*kd, m(0,1d/dt - v_n))
			d = clmd->coll_parms->epsilon*8.0/9.0 + epsilon2*8.0/9.0 - collpair->distance;
			if ( ( magrelVel < 0.1*d*clmd->sim_parms->stepsPerFrame ) && ( d > ALMOST_ZERO ) )
			{
				repulse = MIN2 ( d*1.0/clmd->sim_parms->stepsPerFrame, 0.1*d*clmd->sim_parms->stepsPerFrame - magrelVel );

				// stay on the safe side and clamp repulse
				if ( impulse > ALMOST_ZERO )
					repulse = MIN2 ( repulse, 5.0*impulse );
				repulse = MAX2 ( impulse, repulse );

				impulse = repulse / ( 1.0 + w1*w1 + w2*w2 + w3*w3 ); // original 2.0 / 0.25
				VECADDMUL ( cloth1->verts[collpair->ap1].impulse, collpair->normal,  impulse );
				VECADDMUL ( cloth1->verts[collpair->ap2].impulse, collpair->normal,  impulse );
				VECADDMUL ( cloth1->verts[collpair->ap3].impulse, collpair->normal,  impulse );
			}

			result = 1;
		}
	}
	return result;
}

//Determines collisions on overlap, collisions are writen to collpair[i] and collision+number_collision_found is returned
CollPair* cloth_collision ( ModifierData *md1, ModifierData *md2, BVHTreeOverlap *overlap, CollPair *collpair )
{
	ClothModifierData *clmd = ( ClothModifierData * ) md1;
	CollisionModifierData *collmd = ( CollisionModifierData * ) md2;
	MFace *face1=NULL, *face2 = NULL;
	ClothVertex *verts1 = clmd->clothObject->verts;
	double distance = 0;
	float epsilon1 = clmd->coll_parms->epsilon;
	float epsilon2 = BLI_bvhtree_getepsilon ( collmd->bvhtree );
	int i;

	face1 = & ( clmd->clothObject->mfaces[overlap->indexA] );
	face2 = & ( collmd->mfaces[overlap->indexB] );

	// check all 4 possible collisions
	for ( i = 0; i < 4; i++ )
	{
		if ( i == 0 )
		{
			// fill faceA
			collpair->ap1 = face1->v1;
			collpair->ap2 = face1->v2;
			collpair->ap3 = face1->v3;

			// fill faceB
			collpair->bp1 = face2->v1;
			collpair->bp2 = face2->v2;
			collpair->bp3 = face2->v3;
		}
		else if ( i == 1 )
		{
			if ( face1->v4 )
			{
				// fill faceA
				collpair->ap1 = face1->v1;
				collpair->ap2 = face1->v4;
				collpair->ap3 = face1->v3;

				// fill faceB
				collpair->bp1 = face2->v1;
				collpair->bp2 = face2->v2;
				collpair->bp3 = face2->v3;
			}
			else
				i++;
		}
		if ( i == 2 )
		{
			if ( face2->v4 )
			{
				// fill faceA
				collpair->ap1 = face1->v1;
				collpair->ap2 = face1->v2;
				collpair->ap3 = face1->v3;

				// fill faceB
				collpair->bp1 = face2->v1;
				collpair->bp2 = face2->v4;
				collpair->bp3 = face2->v3;
			}
			else
				break;
		}
		else if ( i == 3 )
		{
			if ( face1->v4 && face2->v4 )
			{
				// fill faceA
				collpair->ap1 = face1->v1;
				collpair->ap2 = face1->v4;
				collpair->ap3 = face1->v3;

				// fill faceB
				collpair->bp1 = face2->v1;
				collpair->bp2 = face2->v4;
				collpair->bp3 = face2->v3;
			}
			else
				break;
		}

#ifdef WITH_BULLET
		// calc distance + normal
		distance = plNearestPoints (
		               verts1[collpair->ap1].txold, verts1[collpair->ap2].txold, verts1[collpair->ap3].txold, collmd->current_x[collpair->bp1].co, collmd->current_x[collpair->bp2].co, collmd->current_x[collpair->bp3].co, collpair->pa,collpair->pb,collpair->vector );
#else
		// just be sure that we don't add anything
		distance = 2.0 * ( epsilon1 + epsilon2 + ALMOST_ZERO );
#endif

		if ( distance <= ( epsilon1 + epsilon2 + ALMOST_ZERO ) )
		{
			VECCOPY ( collpair->normal, collpair->vector );
			Normalize ( collpair->normal );

			collpair->distance = distance;
			collpair->flag = 0;
		}
		else
		{
			// check for collision in the future
			collpair->flag |= COLLISION_IN_FUTURE;
		}
		collpair++;
	}
	return collpair;
}

int cloth_are_edges_adjacent ( ClothModifierData *clmd, CollisionModifierData *collmd, EdgeCollPair *edgecollpair )
{
	Cloth *cloth1 = NULL;
	ClothVertex *verts1 = NULL;
	float temp[3];
	MVert *verts2 = collmd->current_x; // old x

	cloth1 = clmd->clothObject;
	verts1 = cloth1->verts;

	VECSUB ( temp, verts1[edgecollpair->p11].txold, verts2[edgecollpair->p21].co );
	if ( ABS ( INPR ( temp, temp ) ) < ALMOST_ZERO )
		return 1;

	VECSUB ( temp, verts1[edgecollpair->p11].txold, verts2[edgecollpair->p22].co );
	if ( ABS ( INPR ( temp, temp ) ) < ALMOST_ZERO )
		return 1;

	VECSUB ( temp, verts1[edgecollpair->p12].txold, verts2[edgecollpair->p21].co );
	if ( ABS ( INPR ( temp, temp ) ) < ALMOST_ZERO )
		return 1;

	VECSUB ( temp, verts1[edgecollpair->p12].txold, verts2[edgecollpair->p22].co );
	if ( ABS ( INPR ( temp, temp ) ) < ALMOST_ZERO )
		return 1;
	
	VECSUB ( temp, verts1[edgecollpair->p11].txold, verts1[edgecollpair->p12].txold );
	if ( ABS ( INPR ( temp, temp ) ) < ALMOST_ZERO )
		return 1;
	
	VECSUB ( temp, verts2[edgecollpair->p21].co, verts2[edgecollpair->p22].co );
	if ( ABS ( INPR ( temp, temp ) ) < ALMOST_ZERO )
		return 1;
	

	return 0;
}

int cloth_collision_response_moving( ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair, CollPair *collision_end )
{
	int result = 0;
	Cloth *cloth1;
	float w1, w2, w3, u1, u2, u3;
	float v1[3], v2[3], relativeVelocity[3];
	float magrelVel;
	float epsilon2 = BLI_bvhtree_getepsilon ( collmd->bvhtree );

	cloth1 = clmd->clothObject;

	for ( ; collpair != collision_end; collpair++ )
	{
		// only handle static collisions here
		if ( collpair->flag & COLLISION_IN_FUTURE )
			continue;

		// compute barycentric coordinates for both collision points
		collision_compute_barycentric ( collpair->pa,
										cloth1->verts[collpair->ap1].txold,
		  cloth1->verts[collpair->ap2].txold,
	cloth1->verts[collpair->ap3].txold,
 &w1, &w2, &w3 );

		// was: txold
		collision_compute_barycentric ( collpair->pb,
										collmd->current_x[collpair->bp1].co,
		  collmd->current_x[collpair->bp2].co,
	collmd->current_x[collpair->bp3].co,
 &u1, &u2, &u3 );

		// Calculate relative "velocity".
		collision_interpolateOnTriangle ( v1, cloth1->verts[collpair->ap1].tv, cloth1->verts[collpair->ap2].tv, cloth1->verts[collpair->ap3].tv, w1, w2, w3 );

		collision_interpolateOnTriangle ( v2, collmd->current_v[collpair->bp1].co, collmd->current_v[collpair->bp2].co, collmd->current_v[collpair->bp3].co, u1, u2, u3 );

		VECSUB ( relativeVelocity, v2, v1 );

		// Calculate the normal component of the relative velocity (actually only the magnitude - the direction is stored in 'normal').
		magrelVel = INPR ( relativeVelocity, collpair->normal );

		// printf("magrelVel: %f\n", magrelVel);

		// Calculate masses of points.
		// TODO

		// If v_n_mag < 0 the edges are approaching each other.
		if ( magrelVel > ALMOST_ZERO )
		{
			// Calculate Impulse magnitude to stop all motion in normal direction.
			float magtangent = 0, repulse = 0, d = 0;
			double impulse = 0.0;
			float vrel_t_pre[3];
			float temp[3];

			// calculate tangential velocity
			VECCOPY ( temp, collpair->normal );
			VecMulf ( temp, magrelVel );
			VECSUB ( vrel_t_pre, relativeVelocity, temp );

			// Decrease in magnitude of relative tangential velocity due to coulomb friction
			// in original formula "magrelVel" should be the "change of relative velocity in normal direction"
			magtangent = MIN2 ( clmd->coll_parms->friction * 0.01 * magrelVel,sqrt ( INPR ( vrel_t_pre,vrel_t_pre ) ) );

			// Apply friction impulse.
			if ( magtangent > ALMOST_ZERO )
			{
				Normalize ( vrel_t_pre );

				impulse = 2.0 * magtangent / ( 1.0 + w1*w1 + w2*w2 + w3*w3 );
				VECADDMUL ( cloth1->verts[collpair->ap1].impulse, vrel_t_pre, w1 * impulse );
				VECADDMUL ( cloth1->verts[collpair->ap2].impulse, vrel_t_pre, w2 * impulse );
				VECADDMUL ( cloth1->verts[collpair->ap3].impulse, vrel_t_pre, w3 * impulse );
			}

			// Apply velocity stopping impulse
			// I_c = m * v_N / 2.0
			// no 2.0 * magrelVel normally, but looks nicer DG
			impulse =  magrelVel / ( 1.0 + w1*w1 + w2*w2 + w3*w3 );

			VECADDMUL ( cloth1->verts[collpair->ap1].impulse, collpair->normal, w1 * impulse );
			cloth1->verts[collpair->ap1].impulse_count++;

			VECADDMUL ( cloth1->verts[collpair->ap2].impulse, collpair->normal, w2 * impulse );
			cloth1->verts[collpair->ap2].impulse_count++;

			VECADDMUL ( cloth1->verts[collpair->ap3].impulse, collpair->normal, w3 * impulse );
			cloth1->verts[collpair->ap3].impulse_count++;

			// Apply repulse impulse if distance too short
			// I_r = -min(dt*kd, m(0,1d/dt - v_n))
			/*
			d = clmd->coll_parms->epsilon*8.0/9.0 + epsilon2*8.0/9.0 - collpair->distance;
			if ( ( magrelVel < 0.1*d*clmd->sim_parms->stepsPerFrame ) && ( d > ALMOST_ZERO ) )
			{
				repulse = MIN2 ( d*1.0/clmd->sim_parms->stepsPerFrame, 0.1*d*clmd->sim_parms->stepsPerFrame - magrelVel );

				// stay on the safe side and clamp repulse
				if ( impulse > ALMOST_ZERO )
					repulse = MIN2 ( repulse, 5.0*impulse );
				repulse = MAX2 ( impulse, repulse );

				impulse = repulse / ( 1.0 + w1*w1 + w2*w2 + w3*w3 ); // original 2.0 / 0.25
				VECADDMUL ( cloth1->verts[collpair->ap1].impulse, collpair->normal,  impulse );
				VECADDMUL ( cloth1->verts[collpair->ap2].impulse, collpair->normal,  impulse );
				VECADDMUL ( cloth1->verts[collpair->ap3].impulse, collpair->normal,  impulse );
			}
*/
			result = 1;
		}
	}
	return result;
}

int cloth_collision_moving_edges ( ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair )
{
	EdgeCollPair edgecollpair;
	Cloth *cloth1=NULL;
	ClothVertex *verts1=NULL;
	unsigned int i = 0, j = 0, k = 0;
	int numsolutions = 0;
	double x1[3], v1[3], x2[3], v2[3], x3[3], v3[3];
	double solution[3];
	MVert *verts2 = collmd->current_x; // old x
	MVert *velocity2 = collmd->current_v; // velocity
	float mintime = FLT_MAX;
	float distance;
	float triA[3][3], triB[3][3];
	int result = 0;

	cloth1 = clmd->clothObject;
	verts1 = cloth1->verts;
	
	for(i = 0; i < 9; i++)
	{
		// 9 edge - edge possibilities
		
		if(i == 0) // cloth edge: 1-2; coll edge: 1-2
		{
			edgecollpair.p11 = collpair->ap1;
			edgecollpair.p12 = collpair->ap2;
			
			edgecollpair.p21 = collpair->bp1;
			edgecollpair.p22 = collpair->bp2;
		}
		else if(i == 1) // cloth edge: 1-2; coll edge: 2-3
		{
			edgecollpair.p11 = collpair->ap1;
			edgecollpair.p12 = collpair->ap2;
			
			edgecollpair.p21 = collpair->bp2;
			edgecollpair.p22 = collpair->bp3;
		}
		else if(i == 2) // cloth edge: 1-2; coll edge: 1-3
		{
			edgecollpair.p11 = collpair->ap1;
			edgecollpair.p12 = collpair->ap2;
			
			edgecollpair.p21 = collpair->bp1;
			edgecollpair.p22 = collpair->bp3;
		}
		else if(i == 3) // cloth edge: 2-3; coll edge: 1-2
		{
			edgecollpair.p11 = collpair->ap2;
			edgecollpair.p12 = collpair->ap3;
			
			edgecollpair.p21 = collpair->bp1;
			edgecollpair.p22 = collpair->bp2;
		}
		else if(i == 4) // cloth edge: 2-3; coll edge: 2-3
		{
			edgecollpair.p11 = collpair->ap2;
			edgecollpair.p12 = collpair->ap3;
			
			edgecollpair.p21 = collpair->bp2;
			edgecollpair.p22 = collpair->bp3;
		}
		else if(i == 5) // cloth edge: 2-3; coll edge: 1-3
		{
			edgecollpair.p11 = collpair->ap2;
			edgecollpair.p12 = collpair->ap3;
			
			edgecollpair.p21 = collpair->bp1;
			edgecollpair.p22 = collpair->bp3;
		}
		else if(i ==6) // cloth edge: 1-3; coll edge: 1-2
		{
			edgecollpair.p11 = collpair->ap1;
			edgecollpair.p12 = collpair->ap3;
			
			edgecollpair.p21 = collpair->bp1;
			edgecollpair.p22 = collpair->bp2;
		}
		else if(i ==7) // cloth edge: 1-3; coll edge: 2-3
		{
			edgecollpair.p11 = collpair->ap1;
			edgecollpair.p12 = collpair->ap3;
			
			edgecollpair.p21 = collpair->bp2;
			edgecollpair.p22 = collpair->bp3;
		}
		else if(i == 8) // cloth edge: 1-3; coll edge: 1-3
		{
			edgecollpair.p11 = collpair->ap1;
			edgecollpair.p12 = collpair->ap3;
			
			edgecollpair.p21 = collpair->bp1;
			edgecollpair.p22 = collpair->bp3;
		}
		
		if ( !cloth_are_edges_adjacent ( clmd, collmd, &edgecollpair ) )
		{
			// always put coll points in p21/p22
			VECSUB ( x1, verts1[edgecollpair.p12].txold, verts1[edgecollpair.p11].txold );
			VECSUB ( v1, verts1[edgecollpair.p12].tv, verts1[edgecollpair.p11].tv );
			
			VECSUB ( x2, verts2[edgecollpair.p21].co, verts1[edgecollpair.p11].txold );
			VECSUB ( v2, velocity2[edgecollpair.p21].co, verts1[edgecollpair.p11].tv );
			
			VECSUB ( x3, verts2[edgecollpair.p22].co, verts1[edgecollpair.p11].txold );
			VECSUB ( v3, velocity2[edgecollpair.p22].co, verts1[edgecollpair.p11].tv );
			
			numsolutions = cloth_get_collision_time ( x1, v1, x2, v2, x3, v3, solution );
	
			for ( k = 0; k < numsolutions; k++ )
			{
				// printf("sol %d: %lf\n", k, solution[k]);
				if ( ( solution[k] >= DBL_EPSILON ) && ( solution[k] <= 1.0 ) )
				{
					//float out_collisionTime = solution[k];
	
					// TODO: check for collisions
	
					// TODO: put into (edge) collision list
					
					mintime = MIN2(mintime, (float)solution[k]);
					
					result = 1;
					break;
				}
			}
		}
	}
	
	if(result)
	{
		// move triangles to collision point in time
		VECADDS(triA[0], verts1[collpair->ap1].txold, verts1[collpair->ap1].tv, mintime);
		VECADDS(triA[1], verts1[collpair->ap2].txold, verts1[collpair->ap2].tv, mintime);
		VECADDS(triA[2], verts1[collpair->ap3].txold, verts1[collpair->ap3].tv, mintime);
		
		VECADDS(triB[0], collmd->current_x[collpair->bp1].co, collmd->current_v[collpair->bp1].co, mintime);
		VECADDS(triB[1], collmd->current_x[collpair->bp2].co, collmd->current_v[collpair->bp2].co, mintime);
		VECADDS(triB[2], collmd->current_x[collpair->bp3].co, collmd->current_v[collpair->bp3].co, mintime);
		
		// check distance there
		distance = plNearestPoints (triA[0], triA[1], triA[2], triB[0], triB[1], triB[2], collpair->pa,collpair->pb,collpair->vector );
		
		if(distance <= (clmd->coll_parms->epsilon + BLI_bvhtree_getepsilon ( collmd->bvhtree ) + ALMOST_ZERO))
		{
			CollPair *next = collpair;
			next++;
			
			collpair->distance = clmd->coll_parms->epsilon;
			collpair->time = mintime;
			
			VECCOPY ( collpair->normal, collpair->vector );
			Normalize ( collpair->normal );
			
			cloth_collision_response_moving ( clmd, collmd, collpair, next );
		}
	}
	
	return result;
}

/*
void cloth_collision_moving_tris ( ClothModifierData *clmd, ClothModifierData *coll_clmd, CollisionTree *tree1, CollisionTree *tree2 )
{
	CollPair collpair;
	Cloth *cloth1=NULL, *cloth2=NULL;
	MFace *face1=NULL, *face2=NULL;
	ClothVertex *verts1=NULL, *verts2=NULL;
	unsigned int i = 0, j = 0, k = 0;
	int numsolutions = 0;
	float a[3], b[3], c[3], d[3], e[3], f[3];
	double solution[3];

	for ( i = 0; i < 2; i++ )
	{
		cloth1 = clmd->clothObject;
		cloth2 = coll_clmd->clothObject;

		verts1 = cloth1->verts;
		verts2 = cloth2->verts;

		face1 = & ( cloth1->mfaces[tree1->tri_index] );
		face2 = & ( cloth2->mfaces[tree2->tri_index] );

		// check all possible pairs of triangles
		if ( i == 0 )
		{
			collpair.ap1 = face1->v1;
			collpair.ap2 = face1->v2;
			collpair.ap3 = face1->v3;

			collpair.pointsb[0] = face2->v1;
			collpair.pointsb[1] = face2->v2;
			collpair.pointsb[2] = face2->v3;
			collpair.pointsb[3] = face2->v4;
		}

		if ( i == 1 )
		{
			if ( face1->v4 )
			{
				collpair.ap1 = face1->v3;
				collpair.ap2 = face1->v4;
				collpair.ap3 = face1->v1;

				collpair.pointsb[0] = face2->v1;
				collpair.pointsb[1] = face2->v2;
				collpair.pointsb[2] = face2->v3;
				collpair.pointsb[3] = face2->v4;
			}
			else
				i++;
		}

		// calc SIPcode (?)

		if ( i < 2 )
		{
			VECSUB ( a, verts1[collpair.ap2].xold, verts1[collpair.ap1].xold );
			VECSUB ( b, verts1[collpair.ap2].v, verts1[collpair.ap1].v );
			VECSUB ( c, verts1[collpair.ap3].xold, verts1[collpair.ap1].xold );
			VECSUB ( d, verts1[collpair.ap3].v, verts1[collpair.ap1].v );

			for ( j = 0; j < 4; j++ )
			{
				if ( ( j==3 ) && ! ( face2->v4 ) )
					break;

				VECSUB ( e, verts2[collpair.pointsb[j]].xold, verts1[collpair.ap1].xold );
				VECSUB ( f, verts2[collpair.pointsb[j]].v, verts1[collpair.ap1].v );

				numsolutions = cloth_get_collision_time ( a, b, c, d, e, f, solution );

				for ( k = 0; k < numsolutions; k++ )
				{
					if ( ( solution[k] >= ALMOST_ZERO ) && ( solution[k] <= 1.0 ) )
					{
						//float out_collisionTime = solution[k];

						// TODO: check for collisions

						// TODO: put into (point-face) collision list

						// printf("Moving found!\n");

					}
				}

				// TODO: check borders for collisions
			}

		}
	}
}
*/

int cloth_collision_moving ( ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair, CollPair *collision_end )
{
	int result = 0;
	Cloth *cloth1;
	float w1, w2, w3, u1, u2, u3;
	float v1[3], v2[3], relativeVelocity[3];
	float magrelVel;
	float epsilon2 = BLI_bvhtree_getepsilon ( collmd->bvhtree );

	cloth1 = clmd->clothObject;

	for ( ; collpair != collision_end; collpair++ )
	{
		// only handle moving collisions here
		if (!( collpair->flag & COLLISION_IN_FUTURE ))
			continue;
		
		cloth_collision_moving_edges ( clmd, collmd, collpair);
	}
	
	return 1;
}

int cloth_bvh_objcollisions_do ( ClothModifierData * clmd, CollisionModifierData *collmd, float step, float dt )
{
	Cloth *cloth = clmd->clothObject;
	BVHTree *cloth_bvh= ( BVHTree * ) cloth->bvhtree;
	long i=0, j = 0, numfaces = 0, numverts = 0;
	ClothVertex *verts = NULL;
	CollPair *collisions = NULL, *collisions_index = NULL;
	int ret = 0;
	int result = 0;
	float tnull[3] = {0,0,0};
	BVHTreeOverlap *overlap = NULL;


	numfaces = clmd->clothObject->numfaces;
	numverts = clmd->clothObject->numverts;

	verts = cloth->verts;

	if ( collmd->bvhtree )
	{
		/* get pointer to bounding volume hierarchy */
		BVHTree *coll_bvh = collmd->bvhtree;

		/* move object to position (step) in time */
		collision_move_object ( collmd, step + dt, step );

		/* search for overlapping collision pairs */
		overlap = BLI_bvhtree_overlap ( cloth_bvh, coll_bvh, &result );

		collisions = ( CollPair* ) MEM_mallocN ( sizeof ( CollPair ) * result*4, "collision array" ); //*4 since cloth_collision_static can return more than 1 collision
		collisions_index = collisions;

		for ( i = 0; i < result; i++ )
		{
			collisions_index = cloth_collision ( ( ModifierData * ) clmd, ( ModifierData * ) collmd, overlap+i, collisions_index );
		}

		if ( overlap )
			MEM_freeN ( overlap );
	}
	else
	{
		if ( G.rt > 0 )
			printf ( "cloth_bvh_objcollision: found a collision object with clothObject or collData NULL.\n" );
	}

	// process all collisions (calculate impulses, TODO: also repulses if distance too short)
	result = 1;
	for ( j = 0; j < 5; j++ ) // 5 is just a value that ensures convergence
	{
		result = 0;

		if ( collmd->bvhtree )
		{
			result += cloth_collision_response_static ( clmd, collmd, collisions, collisions_index );

			// apply impulses in parallel
			if ( result )
			{
				for ( i = 0; i < numverts; i++ )
				{
					// calculate "velocities" (just xnew = xold + v; no dt in v)
					if ( verts[i].impulse_count )
					{
						VECADDMUL ( verts[i].tv, verts[i].impulse, 1.0f / verts[i].impulse_count );
						VECCOPY ( verts[i].impulse, tnull );
						verts[i].impulse_count = 0;
	
						ret++;
					}
				}
			}
			/*
			result += cloth_collision_moving ( clmd, collmd, collisions, collisions_index );
			
			// apply impulses in parallel
			if ( result )
			{
				for ( i = 0; i < numverts; i++ )
				{
					// calculate "velocities" (just xnew = xold + v; no dt in v)
					if ( verts[i].impulse_count )
					{
						VECADDMUL ( verts[i].tv, verts[i].impulse, 1.0f / verts[i].impulse_count );
						VECCOPY ( verts[i].impulse, tnull );
						verts[i].impulse_count = 0;
	
						ret++;
					}
				}
			}
		*/
		}
	}

	if ( collisions ) MEM_freeN ( collisions );

	return ret;
}

// cloth - object collisions
int cloth_bvh_objcollision ( ClothModifierData * clmd, float step, float dt )
{
	Base *base=NULL;
	CollisionModifierData *collmd=NULL;
	Cloth *cloth=NULL;
	Object *coll_ob=NULL;
	BVHTree *cloth_bvh=NULL;
	long i=0, j = 0, k = 0, numfaces = 0, numverts = 0;
	unsigned int result = 0, rounds = 0; // result counts applied collisions; ic is for debug output;
	ClothVertex *verts = NULL;
	int ret = 0;
	ClothModifierData *tclmd;
	int collisions = 0, count = 0;

	if ( ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_COLLOBJ ) || ! ( ( ( Cloth * ) clmd->clothObject )->bvhtree ) )
	{
		return 0;
	}

	cloth = clmd->clothObject;
	verts = cloth->verts;
	cloth_bvh = ( BVHTree * ) cloth->bvhtree;
	numfaces = clmd->clothObject->numfaces;
	numverts = clmd->clothObject->numverts;

	////////////////////////////////////////////////////////////
	// static collisions
	////////////////////////////////////////////////////////////

	// update cloth bvh
	bvhtree_update_from_cloth ( clmd, 1 ); // 0 means STATIC, 1 means MOVING (see later in this function)
	bvhselftree_update_from_cloth ( clmd, 0 ); // 0 means STATIC, 1 means MOVING (see later in this function)

	do
	{
		result = 0;

		// check all collision objects
		for ( base = G.scene->base.first; base; base = base->next )
		{
			coll_ob = base->object;
			collmd = ( CollisionModifierData * ) modifiers_findByType ( coll_ob, eModifierType_Collision );

			if ( !collmd )
			{
				if ( coll_ob->dup_group )
				{
					GroupObject *go;
					Group *group = coll_ob->dup_group;

					for ( go= group->gobject.first; go; go= go->next )
					{
						coll_ob = go->ob;

						collmd = ( CollisionModifierData * ) modifiers_findByType ( coll_ob, eModifierType_Collision );

						if ( !collmd )
							continue;

						tclmd = ( ClothModifierData * ) modifiers_findByType ( coll_ob, eModifierType_Cloth );
						if ( tclmd == clmd )
							continue;

						ret += cloth_bvh_objcollisions_do ( clmd, collmd, step, dt );
					}
				}
			}
			else
			{
				tclmd = ( ClothModifierData * ) modifiers_findByType ( coll_ob, eModifierType_Cloth );
				if ( tclmd == clmd )
					continue;

				ret += cloth_bvh_objcollisions_do ( clmd, collmd, step, dt );
			}
		}
		rounds++;

		////////////////////////////////////////////////////////////
		// update positions
		// this is needed for bvh_calc_DOP_hull_moving() [kdop.c]
		////////////////////////////////////////////////////////////

		// verts come from clmd
		for ( i = 0; i < numverts; i++ )
		{
			if ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL )
			{
				if ( verts [i].flags & CLOTH_VERT_FLAG_PINNED )
				{
					continue;
				}
			}

			VECADD ( verts[i].tx, verts[i].txold, verts[i].tv );
		}
		////////////////////////////////////////////////////////////


		////////////////////////////////////////////////////////////
		// Test on *simple* selfcollisions
		////////////////////////////////////////////////////////////
		if ( clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_SELF )
		{

			MFace *mface = cloth->mfaces;
			BVHTreeOverlap *overlap = NULL;

			collisions = 1;
			verts = cloth->verts; // needed for openMP

			numfaces = clmd->clothObject->numfaces;
			numverts = clmd->clothObject->numverts;

			verts = cloth->verts;

			if ( cloth->bvhselftree )
			{
				/* search for overlapping collision pairs */
				overlap = BLI_bvhtree_overlap ( cloth->bvhselftree, cloth->bvhselftree, &result );

				for ( k = 0; k < result; k++ )
				{
					float temp[3];
					float length = 0;
					float mindistance;
					
					i = overlap[k].indexA;
					j = overlap[k].indexB;
					
					mindistance = clmd->coll_parms->selfepsilon* ( cloth->verts[i].avg_spring_len + cloth->verts[j].avg_spring_len );

					if ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL )
					{
						if ( ( cloth->verts [i].flags & CLOTH_VERT_FLAG_PINNED )
												 && ( cloth->verts [j].flags & CLOTH_VERT_FLAG_PINNED ) )
						{
							continue;
						}
					}

					VECSUB ( temp, verts[i].tx, verts[j].tx );

					if ( ( ABS ( temp[0] ) > mindistance ) || ( ABS ( temp[1] ) > mindistance ) || ( ABS ( temp[2] ) > mindistance ) ) continue;

					// check for adjacent points (i must be smaller j)
					if ( BLI_edgehash_haskey ( cloth->edgehash, MIN2(i, j), MAX2(i, j) ) )
					{
						continue;
					}

					length = Normalize ( temp );

					if ( length < mindistance )
					{
						float correction = mindistance - length;

						if ( cloth->verts [i].flags & CLOTH_VERT_FLAG_PINNED )
						{
							VecMulf ( temp, -correction );
							VECADD ( verts[j].tx, verts[j].tx, temp );
						}
						else if ( cloth->verts [j].flags & CLOTH_VERT_FLAG_PINNED )
						{
							VecMulf ( temp, correction );
							VECADD ( verts[i].tx, verts[i].tx, temp );
						}
						else
						{
							VecMulf ( temp, -correction*0.5 );
							VECADD ( verts[j].tx, verts[j].tx, temp );

							VECSUB ( verts[i].tx, verts[i].tx, temp );
						}
					}
				}
				
				if ( overlap )
					MEM_freeN ( overlap );
				
			}

			/*
			for ( count = 0; count < clmd->coll_parms->self_loop_count; count++ )
			{
			if ( collisions )
			{
			collisions = 0;
			#pragma omp parallel for private(i,j, collisions) shared(verts, ret)
			for ( i = 0; i < cloth->numverts; i++ )
			{
			for ( j = i + 1; j < cloth->numverts; j++ )
			{
			float temp[3];
			float length = 0;
			float mindistance = clmd->coll_parms->selfepsilon* ( cloth->verts[i].avg_spring_len + cloth->verts[j].avg_spring_len );

			if ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL )
			{
			if ( ( cloth->verts [i].flags & CLOTH_VERT_FLAG_PINNED )
			&& ( cloth->verts [j].flags & CLOTH_VERT_FLAG_PINNED ) )
			{
			continue;
			}
			}

			VECSUB ( temp, verts[i].tx, verts[j].tx );

			if ( ( ABS ( temp[0] ) > mindistance ) || ( ABS ( temp[1] ) > mindistance ) || ( ABS ( temp[2] ) > mindistance ) ) continue;

							// check for adjacent points (i must be smaller j)
			if ( BLI_edgehash_haskey ( cloth->edgehash, i, j ) )
			{
			continue;
			}

			length = Normalize ( temp );

			if ( length < mindistance )
			{
			float correction = mindistance - length;

			if ( cloth->verts [i].flags & CLOTH_VERT_FLAG_PINNED )
			{
			VecMulf ( temp, -correction );
			VECADD ( verts[j].tx, verts[j].tx, temp );
			}
			else if ( cloth->verts [j].flags & CLOTH_VERT_FLAG_PINNED )
			{
			VecMulf ( temp, correction );
			VECADD ( verts[i].tx, verts[i].tx, temp );
			}
			else
			{
			VecMulf ( temp, -correction*0.5 );
			VECADD ( verts[j].tx, verts[j].tx, temp );

			VECSUB ( verts[i].tx, verts[i].tx, temp );
			}

			collisions = 1;

			if ( !ret )
			{
			#pragma omp critical
			{
			ret = 1;
			}
			}
			}
			}
			}
			}
			}
			*/
			////////////////////////////////////////////////////////////

			////////////////////////////////////////////////////////////
			// SELFCOLLISIONS: update velocities
			////////////////////////////////////////////////////////////
			if ( ret )
			{
				for ( i = 0; i < cloth->numverts; i++ )
				{
					if ( ! ( cloth->verts [i].flags & CLOTH_VERT_FLAG_PINNED ) )
						VECSUB ( verts[i].tv, verts[i].tx, verts[i].txold );
				}
			}
			////////////////////////////////////////////////////////////
		}
	}
	while ( result && ( clmd->coll_parms->loop_count>rounds ) );

	return MIN2 ( ret, 1 );
}
