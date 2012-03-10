/*
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
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

/** \file blender/blenkernel/intern/collision.c
 *  \ingroup bke
 */


#include "MEM_guardedalloc.h"

#include "BKE_cloth.h"

#include "DNA_cloth_types.h"
#include "DNA_group_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_scene_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_rand.h"

#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_scene.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_modifier.h"

#include "BKE_DerivedMesh.h"
#ifdef USE_BULLET
#include "Bullet-C-Api.h"
#endif
#include "BLI_kdopbvh.h"
#include "BKE_collision.h"

#ifdef WITH_ELTOPO
#include "eltopo-capi.h"
#endif


/***********************************
Collision modifier code start
***********************************/

/* step is limited from 0 (frame start position) to 1 (frame end position) */
void collision_move_object ( CollisionModifierData *collmd, float step, float prevstep )
{
	float tv[3] = {0, 0, 0};
	unsigned int i = 0;

	for ( i = 0; i < collmd->numverts; i++ )
	{
		sub_v3_v3v3 ( tv, collmd->xnew[i].co, collmd->x[i].co );
		VECADDS ( collmd->current_x[i].co, collmd->x[i].co, tv, prevstep );
		VECADDS ( collmd->current_xnew[i].co, collmd->x[i].co, tv, step );
		sub_v3_v3v3 ( collmd->current_v[i].co, collmd->current_xnew[i].co, collmd->current_x[i].co );
	}

	bvhtree_update_from_mvert ( collmd->bvhtree, collmd->mfaces, collmd->numfaces, collmd->current_x, collmd->current_xnew, collmd->numverts, 1 );
}

BVHTree *bvhtree_build_from_mvert ( MFace *mfaces, unsigned int numfaces, MVert *x, unsigned int UNUSED(numverts), float epsilon )
{
	BVHTree *tree;
	float co[12];
	unsigned int i;
	MFace *tface = mfaces;

	tree = BLI_bvhtree_new ( numfaces*2, epsilon, 4, 26 );

	// fill tree
	for ( i = 0; i < numfaces; i++, tface++ )
	{
		copy_v3_v3 ( &co[0*3], x[tface->v1].co );
		copy_v3_v3 ( &co[1*3], x[tface->v2].co );
		copy_v3_v3 ( &co[2*3], x[tface->v3].co );
		if ( tface->v4 )
			copy_v3_v3 ( &co[3*3], x[tface->v4].co );

		BLI_bvhtree_insert ( tree, i, co, ( mfaces->v4 ? 4 : 3 ) );
	}

	// balance tree
	BLI_bvhtree_balance ( tree );

	return tree;
}

void bvhtree_update_from_mvert ( BVHTree * bvhtree, MFace *faces, int numfaces, MVert *x, MVert *xnew, int UNUSED(numverts), int moving )
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
			copy_v3_v3 ( &co[0*3], x[mfaces->v1].co );
			copy_v3_v3 ( &co[1*3], x[mfaces->v2].co );
			copy_v3_v3 ( &co[2*3], x[mfaces->v3].co );
			if ( mfaces->v4 )
				copy_v3_v3 ( &co[3*3], x[mfaces->v4].co );

			// copy new locations into array
			if ( moving && xnew )
			{
				// update moving positions
				copy_v3_v3 ( &co_moving[0*3], xnew[mfaces->v1].co );
				copy_v3_v3 ( &co_moving[1*3], xnew[mfaces->v2].co );
				copy_v3_v3 ( &co_moving[2*3], xnew[mfaces->v3].co );
				if ( mfaces->v4 )
					copy_v3_v3 ( &co_moving[3*3], xnew[mfaces->v4].co );

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
#if 0 /* UNUSED */
static int 
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
		*x0 = - a / 3;
		*x1 = - a / 3;
		*x2 = - a / 3;
		return 3;
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
		return 3;
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
			mySWAP(*x0, *x1);

		if (*x1 > *x2)
		{
			mySWAP(*x1, *x2);

			if (*x0 > *x1)
				mySWAP(*x0, *x1);
		}

		return 3;
	}
	else
	{
		double sgnR = (R >= 0 ? 1 : -1);
		double A = -sgnR * pow (fabs (R) + sqrt (R2 - Q3), 1.0/3.0);
		double B = Q / A;
		*x0 = A + B - a / 3;
		return 1;
	}
}



/**
* gsl_poly_solve_quadratic
*
* copied from GSL
*/
static int 
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
			double r1 = temp / a;
			double r2 = c / temp;

			if (r1 < r2) 
			{
				*x0 = r1;
				*x1 = r2;
			} 
			else 
			{
				*x0 = r2;
				*x1 = r1;
			}
		}
		return 2;
	}
	else if (disc == 0) 
	{
		*x0 = -0.5 * b / a;
		*x1 = -0.5 * b / a;
		return 2;
	}
	else
	{
		return 0;
	}
}
#endif /* UNUSED */



/*
* See Bridson et al. "Robust Treatment of Collision, Contact and Friction for Cloth Animation"
*     page 4, left column
*/
#if 0
static int cloth_get_collision_time ( double a[3], double b[3], double c[3], double d[3], double e[3], double f[3], double solution[3] )
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
#endif


// w3 is not perfect
static void collision_compute_barycentric ( float pv[3], float p1[3], float p2[3], float p3[3], float *w1, float *w2, float *w3 )
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

	if ( ABS ( d ) < (double)ALMOST_ZERO )
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

#ifndef WITH_ELTOPO
static int cloth_collision_response_static ( ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair, CollPair *collision_end )
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

		sub_v3_v3v3 ( relativeVelocity, v2, v1 );

		// Calculate the normal component of the relative velocity (actually only the magnitude - the direction is stored in 'normal').
		magrelVel = dot_v3v3( relativeVelocity, collpair->normal );

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
			float temp[3], spf;

			// calculate tangential velocity
			copy_v3_v3 ( temp, collpair->normal );
			mul_v3_fl( temp, magrelVel );
			sub_v3_v3v3 ( vrel_t_pre, relativeVelocity, temp );

			// Decrease in magnitude of relative tangential velocity due to coulomb friction
			// in original formula "magrelVel" should be the "change of relative velocity in normal direction"
			magtangent = MIN2 ( clmd->coll_parms->friction * 0.01f * magrelVel, sqrtf( dot_v3v3( vrel_t_pre,vrel_t_pre ) ) );

			// Apply friction impulse.
			if ( magtangent > ALMOST_ZERO )
			{
				normalize_v3( vrel_t_pre );

				impulse = magtangent / ( 1.0f + w1*w1 + w2*w2 + w3*w3 ); // 2.0 *
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
			spf = (float)clmd->sim_parms->stepsPerFrame / clmd->sim_parms->timescale;

			d = clmd->coll_parms->epsilon*8.0f/9.0f + epsilon2*8.0f/9.0f - collpair->distance;
			if ( ( magrelVel < 0.1f*d*spf ) && ( d > ALMOST_ZERO ) )
			{
				repulse = MIN2 ( d*1.0f/spf, 0.1f*d*spf - magrelVel );

				// stay on the safe side and clamp repulse
				if ( impulse > ALMOST_ZERO )
					repulse = MIN2 ( repulse, 5.0*impulse );
				repulse = MAX2 ( impulse, repulse );

				impulse = repulse / ( 1.0f + w1*w1 + w2*w2 + w3*w3 ); // original 2.0 / 0.25
				VECADDMUL ( cloth1->verts[collpair->ap1].impulse, collpair->normal,  impulse );
				VECADDMUL ( cloth1->verts[collpair->ap2].impulse, collpair->normal,  impulse );
				VECADDMUL ( cloth1->verts[collpair->ap3].impulse, collpair->normal,  impulse );
			}

			result = 1;
		}
	}
	return result;
}
#endif /* !WITH_ELTOPO */

#ifdef WITH_ELTOPO
typedef struct edgepairkey {
	int a1, a2, b1, b2;
} edgepairkey;

unsigned int edgepair_hash(void *vkey)
{
	edgepairkey *key = vkey;
	int keys[4] = {key->a1, key->a2, key->b1, key->b2};
	int i, j;
	
	for (i=0; i<4; i++) {
		for (j=0; j<3; j++) {
			if (keys[j] >= keys[j+1]) {
				SWAP(int, keys[j], keys[j+1]);
			}
		}
	}
	
	return keys[0]*101 + keys[1]*72 + keys[2]*53 + keys[3]*34;
}

int edgepair_cmp(const void *va, const void *vb)
{
	edgepairkey *a = va, *b = vb;
	int keysa[4] = {a->a1, a->a2, a->b1, a->b2};
	int keysb[4] = {b->a1, b->a2, b->b1, b->b2};
	int i;
	
	for (i=0; i<4; i++) {
		int j, ok=0;
		for (j=0; j<4; j++) {
			if (keysa[i] == keysa[j]) {
				ok = 1;
				break;
			}
		}
		if (!ok)
			return -1;
	}
	
	return 0;
}

static void get_edgepairkey(edgepairkey *key, int a1, int a2, int b1, int b2)
{
	key->a1 = a1;
	key->a2 = a2;
	key->b1 = b1;
	key->b2 = b2;
}

/*an immense amount of duplication goes on here. . .a major performance hit, I'm sure*/
static CollPair* cloth_edge_collision ( ModifierData *md1, ModifierData *md2, 
										BVHTreeOverlap *overlap, CollPair *collpair,
										GHash *visithash, MemArena *arena)
{
	ClothModifierData *clmd = ( ClothModifierData * ) md1;
	CollisionModifierData *collmd = ( CollisionModifierData * ) md2;
	MFace *face1=NULL, *face2 = NULL;
	ClothVertex *verts1 = clmd->clothObject->verts;
	double distance = 0;
	edgepairkey *key, tstkey;
	float epsilon1 = clmd->coll_parms->epsilon;
	float epsilon2 = BLI_bvhtree_getepsilon ( collmd->bvhtree );
	float no[3], uv[3], t, relnor;
	int i, i1, i2, i3, i4, i5, i6;
	Cloth *cloth = clmd->clothObject;
	float n1[3], n2[3], off[3], v1[2][3], v2[2][3], v3[2][3], v4[2][3], v5[2][3], v6[2][3];
	void **verts[] = {v1, v2, v3, v4, v5, v6};
	int j, ret, bp1, bp2, bp3, ap1, ap2, ap3, table[6];
	
	face1 = & ( clmd->clothObject->mfaces[overlap->indexA] );
	face2 = & ( collmd->mfaces[overlap->indexB] );

	// check all 4 possible collisions
	for ( i = 0; i < 4; i++ )
	{
		if ( i == 0 )
		{
			// fill faceA
			ap1 = face1->v1;
			ap2 = face1->v2;
			ap3 = face1->v3;

			// fill faceB
			bp1 = face2->v1;
			bp2 = face2->v2;
			bp3 = face2->v3;
		}
		else if ( i == 1 )
		{
			if ( face1->v4 )
			{
				// fill faceA
				ap1 = face1->v1;
				ap2 = face1->v3;
				ap3 = face1->v4;

				// fill faceB
				bp1 = face2->v1;
				bp2 = face2->v2;
				bp3 = face2->v3;
			}
			else {
				continue;
			}
		}
		if ( i == 2 )
		{
			if ( face2->v4 )
			{
				// fill faceA
				ap1 = face1->v1;
				ap2 = face1->v2;
				ap3 = face1->v3;

				// fill faceB
				bp1 = face2->v1;
				bp2 = face2->v3;
				bp3 = face2->v4;
			}
			else {
				continue;
			}
		}
		else if ( i == 3 )
		{
			if ( face1->v4 && face2->v4 )
			{
				// fill faceA
				ap1 = face1->v1;
				ap2 = face1->v3;
				ap3 = face1->v4;

				// fill faceB
				bp1 = face2->v1;
				bp2 = face2->v3;
				bp3 = face2->v4;
			}
			else {
				continue;
			}
		}
		
		copy_v3_v3(v1[0], cloth->verts[ap1].txold); 
		copy_v3_v3(v1[1], cloth->verts[ap1].tx);
		copy_v3_v3(v2[0], cloth->verts[ap2].txold);
		copy_v3_v3(v2[1], cloth->verts[ap2].tx);
		copy_v3_v3(v3[0], cloth->verts[ap3].txold);
		copy_v3_v3(v3[1], cloth->verts[ap3].tx);
		
		copy_v3_v3(v4[0], collmd->current_x[bp1].co);
		copy_v3_v3(v4[1], collmd->current_xnew[bp1].co);
		copy_v3_v3(v5[0], collmd->current_x[bp2].co);
		copy_v3_v3(v5[1], collmd->current_xnew[bp2].co);
		copy_v3_v3(v6[0], collmd->current_x[bp3].co);
		copy_v3_v3(v6[1], collmd->current_xnew[bp3].co);
		
		normal_tri_v3(n2, v4[1], v5[1], v6[1]);

		/*offset new positions a bit, to account for margins*/
		i1 = ap1; i2 = ap2; i3 = ap3;
		i4 = bp1; i5 = bp2; i6 = bp3;

		for (j=0; j<3; j++) {
			int collp1, collp2, k, j2 = (j+1)%3;
			
			table[0] = ap1; table[1] = ap2; table[2] = ap3;
			table[3] = bp1; table[4] = bp2; table[5] = bp3;
			for (k=0; k<3; k++) {
				float p1[3], p2[3];
				int k2 = (k+1)%3;
				
				get_edgepairkey(&tstkey, table[j], table[j2], table[k+3], table[k2+3]);
				//if (BLI_ghash_haskey(visithash, &tstkey))
				//	continue;
				
				key = BLI_memarena_alloc(arena, sizeof(edgepairkey));
				*key = tstkey;
				BLI_ghash_insert(visithash, key, NULL);

				sub_v3_v3v3(p1, verts[j], verts[j2]);
				sub_v3_v3v3(p2, verts[k+3], verts[k2+3]);
				
				cross_v3_v3v3(off, p1, p2);
				normalize_v3(off);

				if (dot_v3v3(n2, off) < 0.0)
					negate_v3(off);
				
				mul_v3_fl(off,  epsilon1 + epsilon2 + ALMOST_ZERO);
				copy_v3_v3(p1, verts[k+3]);
				copy_v3_v3(p2, verts[k2+3]);
				add_v3_v3(p1, off);
				add_v3_v3(p2, off);
				
				ret = eltopo_line_line_moving_isect_v3v3_f(verts[j], table[j], verts[j2], table[j2], 
														   p1, table[k+3], p2, table[k2+3], 
														   no, uv, &t, &relnor);
				/*cloth vert versus coll face*/
				if (ret) {
					collpair->ap1 = table[j]; collpair->ap2 = table[j2]; 
					collpair->bp1 = table[k+3]; collpair->bp2 = table[k2+3];
					
					/*I'm not sure if this is correct, but hopefully it's 
					  better then simply ignoring back edges*/
					if (dot_v3v3(n2, no) < 0.0) {
						negate_v3(no);
					}
					
					copy_v3_v3(collpair->normal, no);
					mul_v3_v3fl(collpair->vector, collpair->normal, relnor);
					collpair->distance = relnor;
					collpair->time = t;
					
					copy_v2_v2(collpair->bary, uv);
					
					collpair->flag = COLLISION_IS_EDGES;
					collpair++;
				}
			}
		}
	}
	
	return collpair;
}

static int cloth_edge_collision_response_moving ( ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair, CollPair *collision_end )
{
	int result = 0;
	Cloth *cloth1;
	float w1, w2;
	float v1[3], v2[3], relativeVelocity[3];
	float magrelVel, pimpulse[3];

	cloth1 = clmd->clothObject;

	for ( ; collpair != collision_end; collpair++ )
	{
		if (!(collpair->flag & COLLISION_IS_EDGES))
			continue;
		
		// was: txold
		w1 = collpair->bary[0]; w2 = collpair->bary[1];			
		
		// Calculate relative "velocity".
		VECADDFAC(v1, cloth1->verts[collpair->ap1].tv, cloth1->verts[collpair->ap2].tv, w1);
		VECADDFAC(v2, collmd->current_v[collpair->bp1].co, collmd->current_v[collpair->bp2].co, w2);
		
		sub_v3_v3v3 ( relativeVelocity, v2, v1);
		
		// Calculate the normal component of the relative velocity (actually only the magnitude - the direction is stored in 'normal').
		magrelVel = dot_v3v3 ( relativeVelocity, collpair->normal );

		// If v_n_mag < 0 the edges are approaching each other.
		if ( magrelVel > ALMOST_ZERO )
		{
			// Calculate Impulse magnitude to stop all motion in normal direction.
			float magtangent = 0, repulse = 0, d = 0;
			double impulse = 0.0;
			float vrel_t_pre[3];
			float temp[3], spf;
			
			zero_v3(pimpulse);
			
			// calculate tangential velocity
			copy_v3_v3 ( temp, collpair->normal );
			mul_v3_fl( temp, magrelVel );
			sub_v3_v3v3 ( vrel_t_pre, relativeVelocity, temp );

			// Decrease in magnitude of relative tangential velocity due to coulomb friction
			// in original formula "magrelVel" should be the "change of relative velocity in normal direction"
			magtangent = MIN2 ( clmd->coll_parms->friction * 0.01 * magrelVel,sqrt ( dot_v3v3 ( vrel_t_pre,vrel_t_pre ) ) );

			// Apply friction impulse.
			if ( magtangent > ALMOST_ZERO )
			{
				normalize_v3( vrel_t_pre );

				impulse = magtangent; 
				VECADDMUL ( pimpulse, vrel_t_pre, impulse);
			}

			// Apply velocity stopping impulse
			// I_c = m * v_N / 2.0
			// no 2.0 * magrelVel normally, but looks nicer DG
			impulse =  magrelVel;
			
			mul_v3_fl(collpair->normal, 0.5);
			VECADDMUL ( pimpulse, collpair->normal, impulse);

			// Apply repulse impulse if distance too short
			// I_r = -min(dt*kd, m(0,1d/dt - v_n))
			spf = (float)clmd->sim_parms->stepsPerFrame / clmd->sim_parms->timescale;

			d = collpair->distance;
			if ( ( magrelVel < 0.1*d*spf && ( d > ALMOST_ZERO ) ) )
			{
				repulse = MIN2 ( d*1.0/spf, 0.1*d*spf - magrelVel );

				// stay on the safe side and clamp repulse
				if ( impulse > ALMOST_ZERO )
					repulse = MIN2 ( repulse, 5.0*impulse );
				repulse = MAX2 ( impulse, repulse );

				impulse = repulse / ( 5.0 ); // original 2.0 / 0.25
				VECADDMUL ( pimpulse, collpair->normal, impulse);
			}
			
			w2 = 1.0f-w1;
			if (w1 < 0.5)
				w1 *= 2.0;
			else
				w2 *= 2.0;
			
			VECADDFAC(cloth1->verts[collpair->ap1].impulse, cloth1->verts[collpair->ap1].impulse, pimpulse, w1*2.0);
			VECADDFAC(cloth1->verts[collpair->ap2].impulse, cloth1->verts[collpair->ap2].impulse, pimpulse, w2*2.0);
			
			cloth1->verts[collpair->ap1].impulse_count++;
			cloth1->verts[collpair->ap2].impulse_count++;
			
			result = 1;
		}
	} 
	
	return result;
}

static int cloth_collision_response_moving ( ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair, CollPair *collision_end )
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
		if (collpair->flag & COLLISION_IS_EDGES)
			continue;
		
		if ( collpair->flag & COLLISION_USE_COLLFACE ) {
			// was: txold
			w1 = collpair->bary[0]; w2 = collpair->bary[1]; w3 = collpair->bary[2];			

			// Calculate relative "velocity".
			collision_interpolateOnTriangle ( v1, collmd->current_v[collpair->bp1].co, collmd->current_v[collpair->bp2].co, collmd->current_v[collpair->bp3].co, w1, w2, w3);
			
			sub_v3_v3v3 ( relativeVelocity, v1, cloth1->verts[collpair->collp].tv);
			
			// Calculate the normal component of the relative velocity (actually only the magnitude - the direction is stored in 'normal').
			magrelVel = dot_v3v3 ( relativeVelocity, collpair->normal );
	
			// If v_n_mag < 0 the edges are approaching each other.
			if ( magrelVel > ALMOST_ZERO )
			{
				// Calculate Impulse magnitude to stop all motion in normal direction.
				float magtangent = 0, repulse = 0, d = 0;
				double impulse = 0.0;
				float vrel_t_pre[3];
				float temp[3], spf;
	
				// calculate tangential velocity
				copy_v3_v3 ( temp, collpair->normal );
				mul_v3_fl( temp, magrelVel );
				sub_v3_v3v3 ( vrel_t_pre, relativeVelocity, temp );
	
				// Decrease in magnitude of relative tangential velocity due to coulomb friction
				// in original formula "magrelVel" should be the "change of relative velocity in normal direction"
				magtangent = MIN2 ( clmd->coll_parms->friction * 0.01 * magrelVel,sqrt ( dot_v3v3 ( vrel_t_pre,vrel_t_pre ) ) );
	
				// Apply friction impulse.
				if ( magtangent > ALMOST_ZERO )
				{
					normalize_v3( vrel_t_pre );
	
					impulse = magtangent; // 2.0 * 
					VECADDMUL ( cloth1->verts[collpair->collp].impulse, vrel_t_pre, impulse);
				}
	
				// Apply velocity stopping impulse
				// I_c = m * v_N / 2.0
				// no 2.0 * magrelVel normally, but looks nicer DG
				impulse =  magrelVel/2.0;
	
				VECADDMUL ( cloth1->verts[collpair->collp].impulse, collpair->normal, impulse);
				cloth1->verts[collpair->collp].impulse_count++;
	
				// Apply repulse impulse if distance too short
				// I_r = -min(dt*kd, m(0,1d/dt - v_n))
				spf = (float)clmd->sim_parms->stepsPerFrame / clmd->sim_parms->timescale;
	
				d = -collpair->distance;
				if ( ( magrelVel < 0.1*d*spf ) && ( d > ALMOST_ZERO ) )
				{
					repulse = MIN2 ( d*1.0/spf, 0.1*d*spf - magrelVel );
	
					// stay on the safe side and clamp repulse
					if ( impulse > ALMOST_ZERO )
						repulse = MIN2 ( repulse, 5.0*impulse );
					repulse = MAX2 ( impulse, repulse );
	
					impulse = repulse / ( 5.0 ); // original 2.0 / 0.25
					VECADDMUL ( cloth1->verts[collpair->collp].impulse, collpair->normal, impulse);
				}
	
				result = 1;
			}
		} else {	
			w1 = collpair->bary[0]; w2 = collpair->bary[1]; w3 = collpair->bary[2];			

			// Calculate relative "velocity".
			collision_interpolateOnTriangle ( v1, cloth1->verts[collpair->ap1].tv, cloth1->verts[collpair->ap2].tv, cloth1->verts[collpair->ap3].tv, w1, w2, w3 );
	
			sub_v3_v3v3 ( relativeVelocity, collmd->current_v[collpair->collp].co, v1);
			
			// Calculate the normal component of the relative velocity (actually only the magnitude - the direction is stored in 'normal').
			magrelVel = dot_v3v3 ( relativeVelocity, collpair->normal );
	
			// If v_n_mag < 0 the edges are approaching each other.
			if ( magrelVel > ALMOST_ZERO )
			{
				// Calculate Impulse magnitude to stop all motion in normal direction.
				float magtangent = 0, repulse = 0, d = 0;
				double impulse = 0.0;
				float vrel_t_pre[3], pimpulse[3] = {0.0f, 0.0f, 0.0f};
				float temp[3], spf;
	
				// calculate tangential velocity
				copy_v3_v3 ( temp, collpair->normal );
				mul_v3_fl( temp, magrelVel );
				sub_v3_v3v3 ( vrel_t_pre, relativeVelocity, temp );
	
				// Decrease in magnitude of relative tangential velocity due to coulomb friction
				// in original formula "magrelVel" should be the "change of relative velocity in normal direction"
				magtangent = MIN2 ( clmd->coll_parms->friction * 0.01 * magrelVel,sqrt ( dot_v3v3 ( vrel_t_pre,vrel_t_pre ) ) );
	
				// Apply friction impulse.
				if ( magtangent > ALMOST_ZERO )
				{
					normalize_v3( vrel_t_pre );
	
					impulse = magtangent; // 2.0 * 
					VECADDMUL ( pimpulse, vrel_t_pre, impulse);
				}
	
				// Apply velocity stopping impulse
				// I_c = m * v_N / 2.0
				// no 2.0 * magrelVel normally, but looks nicer DG
				impulse =  magrelVel/2.0;
	
				VECADDMUL ( pimpulse, collpair->normal, impulse);
	
				// Apply repulse impulse if distance too short
				// I_r = -min(dt*kd, m(0,1d/dt - v_n))
				spf = (float)clmd->sim_parms->stepsPerFrame / clmd->sim_parms->timescale;
	
				d = -collpair->distance;
				if ( ( magrelVel < 0.1*d*spf ) && ( d > ALMOST_ZERO ) )
				{
					repulse = MIN2 ( d*1.0/spf, 0.1*d*spf - magrelVel );
	
					// stay on the safe side and clamp repulse
					if ( impulse > ALMOST_ZERO )
						repulse = MIN2 ( repulse, 5.0*impulse );
					repulse = MAX2 ( impulse, repulse );
	
					impulse = repulse / ( 2.0 ); // original 2.0 / 0.25
					VECADDMUL ( pimpulse, collpair->normal, impulse);
				}
				
				if (w1 < 0.5) w1 *= 2.0;
				if (w2 < 0.5) w2 *= 2.0;
				if (w3 < 0.5) w3 *= 2.0;
				
				VECADDMUL(cloth1->verts[collpair->ap1].impulse, pimpulse, w1*2.0);
				VECADDMUL(cloth1->verts[collpair->ap2].impulse, pimpulse, w2*2.0);
				VECADDMUL(cloth1->verts[collpair->ap3].impulse, pimpulse, w3*2.0);
				cloth1->verts[collpair->ap1].impulse_count++;
				cloth1->verts[collpair->ap2].impulse_count++;
				cloth1->verts[collpair->ap3].impulse_count++;
				
				result = 1;
			}
		}
	} 
	
	return result;
}


typedef struct tripairkey {
	int p, a1, a2, a3;
} tripairkey;

unsigned int tripair_hash(void *vkey)
{
	tripairkey *key = vkey;
	int keys[4] = {key->p, key->a1, key->a2, key->a3};
	int i, j;
	
	for (i=0; i<4; i++) {
		for (j=0; j<3; j++) {
			if (keys[j] >= keys[j+1]) {
				SWAP(int, keys[j], keys[j+1]);
			}
		}
	}
	
	return keys[0]*101 + keys[1]*72 + keys[2]*53 + keys[3]*34;
}

int tripair_cmp(const void *va, const void *vb)
{
	tripairkey *a = va, *b = vb;
	int keysa[4] = {a->p, a->a1, a->a2, a->a3};
	int keysb[4] = {b->p, b->a1, b->a2, b->a3};
	int i;
	
	for (i=0; i<4; i++) {
		int j, ok=0;
		for (j=0; j<4; j++) {
			if (keysa[i] == keysa[j]) {
				ok = 1;
				break;
			}
		}
		if (!ok)
			return -1;
	}
	
	return 0;
}

static void get_tripairkey(tripairkey *key, int p, int a1, int a2, int a3)
{
	key->a1 = a1;
	key->a2 = a2;
	key->a3 = a3;
	key->p = p;
}

static int checkvisit(MemArena *arena, GHash *gh, int p, int a1, int a2, int a3)
{
	tripairkey key, *key2;
	
	get_tripairkey(&key, p, a1, a2, a3);
	if (BLI_ghash_haskey(gh, &key))
		return 1;
	
	key2 = BLI_memarena_alloc(arena, sizeof(*key2));
	*key2 = key;
	BLI_ghash_insert(gh, key2, NULL);
	
	return 0;
}

int cloth_point_tri_moving_v3v3_f(float v1[2][3], int i1, float v2[2][3], int i2,
                                   float v3[2][3],  int i3, float v4[2][3], int i4,
                                   float normal[3], float bary[3], float *t, 
								   float *relnor, GHash *gh, MemArena *arena)
{
	if (checkvisit(arena, gh, i1, i2, i3, i4))
		return 0;
	
	return eltopo_point_tri_moving_v3v3_f(v1, i1, v2, i2, v3, i3, v4, i4, normal, bary, t, relnor);
}

static CollPair* cloth_collision ( ModifierData *md1, ModifierData *md2, BVHTreeOverlap *overlap, 
								   CollPair *collpair, double dt, GHash *gh, MemArena *arena)
{
	ClothModifierData *clmd = ( ClothModifierData * ) md1;
	CollisionModifierData *collmd = ( CollisionModifierData * ) md2;
	MFace *face1=NULL, *face2 = NULL;
	ClothVertex *verts1 = clmd->clothObject->verts;
	double distance = 0;
	float epsilon1 = clmd->coll_parms->epsilon;
	float epsilon2 = BLI_bvhtree_getepsilon ( collmd->bvhtree );
	float no[3], uv[3], t, relnor;
	int i, i1, i2, i3, i4, i5, i6;
	Cloth *cloth = clmd->clothObject;
	float n1[3], sdis, p[3], l, n2[3], off[3], v1[2][3], v2[2][3], v3[2][3], v4[2][3], v5[2][3], v6[2][3];
	int j, ret, bp1, bp2, bp3, ap1, ap2, ap3;
	
	face1 = & ( clmd->clothObject->mfaces[overlap->indexA] );
	face2 = & ( collmd->mfaces[overlap->indexB] );

	// check all 4 possible collisions
	for ( i = 0; i < 4; i++ )
	{
		if ( i == 0 )
		{
			// fill faceA
			ap1 = face1->v1;
			ap2 = face1->v2;
			ap3 = face1->v3;

			// fill faceB
			bp1 = face2->v1;
			bp2 = face2->v2;
			bp3 = face2->v3;
		}
		else if ( i == 1 )
		{
			if ( face1->v4 )
			{
				// fill faceA
				ap1 = face1->v1;
				ap2 = face1->v3;
				ap3 = face1->v4;

				// fill faceB
				bp1 = face2->v1;
				bp2 = face2->v2;
				bp3 = face2->v3;
			}
			else {
				continue;
			}
		}
		if ( i == 2 )
		{
			if ( face2->v4 )
			{
				// fill faceA
				ap1 = face1->v1;
				ap2 = face1->v2;
				ap3 = face1->v3;

				// fill faceB
				bp1 = face2->v1;
				bp2 = face2->v3;
				bp3 = face2->v4;
			}
			else {
				continue;
			}
		}
		else if ( i == 3 )
		{
			if ( face1->v4 && face2->v4 )
			{
				// fill faceA
				ap1 = face1->v1;
				ap2 = face1->v3;
				ap3 = face1->v4;

				// fill faceB
				bp1 = face2->v1;
				bp2 = face2->v3;
				bp3 = face2->v4;
			}
			else {
				continue;
			}
		}
		
		copy_v3_v3(v1[0], cloth->verts[ap1].txold); 
		copy_v3_v3(v1[1], cloth->verts[ap1].tx);
		copy_v3_v3(v2[0], cloth->verts[ap2].txold);
		copy_v3_v3(v2[1], cloth->verts[ap2].tx);
		copy_v3_v3(v3[0], cloth->verts[ap3].txold);
		copy_v3_v3(v3[1], cloth->verts[ap3].tx);
		
		copy_v3_v3(v4[0], collmd->current_x[bp1].co);
		copy_v3_v3(v4[1], collmd->current_xnew[bp1].co);
		copy_v3_v3(v5[0], collmd->current_x[bp2].co);
		copy_v3_v3(v5[1], collmd->current_xnew[bp2].co);
		copy_v3_v3(v6[0], collmd->current_x[bp3].co);
		copy_v3_v3(v6[1], collmd->current_xnew[bp3].co);
		
		normal_tri_v3(n2, v4[1], v5[1], v6[1]);
		
		sdis = clmd->coll_parms->distance_repel + epsilon2 + FLT_EPSILON;

		/*apply a repulsion force, to help the solver along*/
		copy_v3_v3(off, n2);
		negate_v3(off);
		if (isect_ray_plane_v3(v1[1], off, v4[1], v5[1], v6[1], &l, 0)) {
			if (l >= 0.0 && l < sdis) {
				mul_v3_fl(off, (l-sdis)*cloth->verts[ap1].mass*dt*clmd->coll_parms->repel_force*0.1);

				add_v3_v3(cloth->verts[ap1].tv, off);
				add_v3_v3(cloth->verts[ap2].tv, off);
				add_v3_v3(cloth->verts[ap3].tv, off);
			}
		}

		/*offset new positions a bit, to account for margins*/
		copy_v3_v3(off, n2);
		mul_v3_fl(off,  epsilon1 + epsilon2 + ALMOST_ZERO);
		add_v3_v3(v4[1], off); add_v3_v3(v5[1], off); add_v3_v3(v6[1], off);
		
		i1 = ap1; i2 = ap2; i3 = ap3;
		i4 = bp1+cloth->numverts; i5 = bp2+cloth->numverts; i6 = bp3+cloth->numverts;
		
		for (j=0; j<6; j++) {
			int collp;

			switch (j) {
			case 0:
				ret = cloth_point_tri_moving_v3v3_f(v1, i1, v4, i4, v5, i5, v6, i6, no, uv, &t, &relnor, gh, arena);
				collp = ap1;
				break;
			case 1:
				collp = ap2;
				ret = cloth_point_tri_moving_v3v3_f(v2, i2, v4, i4, v5, i5, v6, i6, no, uv, &t, &relnor, gh, arena);
				break;
			case 2:
				collp = ap3;
				ret = cloth_point_tri_moving_v3v3_f(v3, i3, v4, i4, v5, i5, v6, i6, no, uv, &t, &relnor, gh, arena);
				break;
			case 3:
				collp = bp1;
				ret = cloth_point_tri_moving_v3v3_f(v4, i4, v1, i1, v2, i2, v3, i3, no, uv, &t, &relnor, gh, arena);
				break;
			case 4:
				collp = bp2;				
				ret = cloth_point_tri_moving_v3v3_f(v5, i5, v1, i1, v2, i2, v3, i3, no, uv, &t, &relnor, gh, arena);
				break;
			case 5:
				collp = bp3;
				ret = cloth_point_tri_moving_v3v3_f(v6, i6, v1, i1, v2, i2, v3, i3, no, uv, &t, &relnor, gh, arena);
				break;
			}
			
			/*cloth vert versus coll face*/
			if (ret && j < 3) {
				collpair->bp1 = bp1; collpair->bp2 = bp2; collpair->bp3 = bp3;
				collpair->collp = collp;
				
				copy_v3_v3(collpair->normal, no);
				mul_v3_v3fl(collpair->vector, collpair->normal, relnor);
				collpair->distance = relnor;
				collpair->time = t;
				
				copy_v3_v3(collpair->bary, uv);
				
				collpair->flag = COLLISION_USE_COLLFACE;
				collpair++;
			} else if (ret && j >= 3) { /*coll vert versus cloth face*/
				collpair->ap1 = ap1; collpair->ap2 = ap2; collpair->ap3 = ap3;
				collpair->collp = collp;
				
				copy_v3_v3(collpair->normal, no);
				mul_v3_v3fl(collpair->vector, collpair->normal, relnor);
				collpair->distance = relnor;
				collpair->time = t;
				
				copy_v3_v3(collpair->bary, uv);

				collpair->flag = 0;
				collpair++;
			}
		}
	}
	
	return collpair;
}

static void machine_epsilon_offset(Cloth *cloth)
{
	ClothVertex *cv;
	int i, j;
	
	cv = cloth->verts;
	for (i=0; i<cloth->numverts; i++, cv++) {
		/*aggrevatingly enough, it's necessary to offset the coordinates
		 by a multiple of the 32-bit floating point epsilon when switching
		 into doubles*/
		#define RNDSIGN (float)(-1*(BLI_rand()%2==0)|1)
		for (j=0; j<3; j++) {
			cv->tx[j] += FLT_EPSILON*30.0f*RNDSIGN;
			cv->txold[j] += FLT_EPSILON*30.0f*RNDSIGN;
			cv->tv[j] += FLT_EPSILON*30.0f*RNDSIGN;
		}		
	}
}

#else /* !WITH_ELTOPO */

//Determines collisions on overlap, collisions are written to collpair[i] and collision+number_collision_found is returned
static CollPair* cloth_collision ( ModifierData *md1, ModifierData *md2, 
	BVHTreeOverlap *overlap, CollPair *collpair, float dt )
{
	ClothModifierData *clmd = ( ClothModifierData * ) md1;
	CollisionModifierData *collmd = ( CollisionModifierData * ) md2;
	Cloth *cloth = clmd->clothObject;
	MFace *face1=NULL, *face2 = NULL;
#ifdef USE_BULLET
	ClothVertex *verts1 = clmd->clothObject->verts;
#endif
	double distance = 0;
	float epsilon1 = clmd->coll_parms->epsilon;
	float epsilon2 = BLI_bvhtree_getepsilon ( collmd->bvhtree );
	float n2[3], sdis, l;
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
		
		normal_tri_v3(n2, collmd->current_xnew[collpair->bp1].co, 
			collmd->current_xnew[collpair->bp2].co, 
			collmd->current_xnew[collpair->bp3].co);
		
		sdis = clmd->coll_parms->distance_repel + epsilon2 + FLT_EPSILON;
		
		/* apply a repulsion force, to help the solver along.
		 * this is kindof crude, it only tests one vert of the triangle */
		if (isect_ray_plane_v3(cloth->verts[collpair->ap1].tx, n2, collmd->current_xnew[collpair->bp1].co, 
			collmd->current_xnew[collpair->bp2].co,
			collmd->current_xnew[collpair->bp3].co, &l, 0))
		{
			if (l >= 0.0f && l < sdis) {
				mul_v3_fl(n2, (l-sdis)*cloth->verts[collpair->ap1].mass*dt*clmd->coll_parms->repel_force*0.1f);

				add_v3_v3(cloth->verts[collpair->ap1].tv, n2);
				add_v3_v3(cloth->verts[collpair->ap2].tv, n2);
				add_v3_v3(cloth->verts[collpair->ap3].tv, n2);
			}
		}
		
#ifdef USE_BULLET
		// calc distance + normal
		distance = plNearestPoints (
			verts1[collpair->ap1].txold, verts1[collpair->ap2].txold, verts1[collpair->ap3].txold, collmd->current_x[collpair->bp1].co, collmd->current_x[collpair->bp2].co, collmd->current_x[collpair->bp3].co, collpair->pa,collpair->pb,collpair->vector );
#else
		// just be sure that we don't add anything
		distance = 2.0 * (double)( epsilon1 + epsilon2 + ALMOST_ZERO );
#endif

		if ( distance <= ( epsilon1 + epsilon2 + ALMOST_ZERO ) )
		{
			normalize_v3_v3( collpair->normal, collpair->vector );

			collpair->distance = distance;
			collpair->flag = 0;
			collpair++;
		}/*
		else
		{
			float w1, w2, w3, u1, u2, u3;
			float v1[3], v2[3], relativeVelocity[3];

			// calc relative velocity
			
			// compute barycentric coordinates for both collision points
			collision_compute_barycentric ( collpair->pa,
			verts1[collpair->ap1].txold,
			verts1[collpair->ap2].txold,
			verts1[collpair->ap3].txold,
			&w1, &w2, &w3 );

			// was: txold
			collision_compute_barycentric ( collpair->pb,
			collmd->current_x[collpair->bp1].co,
			collmd->current_x[collpair->bp2].co,
			collmd->current_x[collpair->bp3].co,
			&u1, &u2, &u3 );

			// Calculate relative "velocity".
			collision_interpolateOnTriangle ( v1, verts1[collpair->ap1].tv, verts1[collpair->ap2].tv, verts1[collpair->ap3].tv, w1, w2, w3 );

			collision_interpolateOnTriangle ( v2, collmd->current_v[collpair->bp1].co, collmd->current_v[collpair->bp2].co, collmd->current_v[collpair->bp3].co, u1, u2, u3 );

			sub_v3_v3v3 ( relativeVelocity, v2, v1 );

			if(sqrt(dot_v3v3(relativeVelocity, relativeVelocity)) >= distance)
			{
				// check for collision in the future
				collpair->flag |= COLLISION_IN_FUTURE;
				collpair++;
			}
		}*/
	}
	return collpair;
}
#endif /* WITH_ELTOPO */


#if 0
static int cloth_collision_response_moving( ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair, CollPair *collision_end )
{
	int result = 0;
	Cloth *cloth1;
	float w1, w2, w3, u1, u2, u3;
	float v1[3], v2[3], relativeVelocity[3];
	float magrelVel;

	cloth1 = clmd->clothObject;

	for ( ; collpair != collision_end; collpair++ )
	{
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

		sub_v3_v3v3 ( relativeVelocity, v2, v1 );

		// Calculate the normal component of the relative velocity (actually only the magnitude - the direction is stored in 'normal').
		magrelVel = dot_v3v3 ( relativeVelocity, collpair->normal );

		// printf("magrelVel: %f\n", magrelVel);

		// Calculate masses of points.
		// TODO

		// If v_n_mag < 0 the edges are approaching each other.
		if ( magrelVel > ALMOST_ZERO )
		{
			// Calculate Impulse magnitude to stop all motion in normal direction.
			float magtangent = 0;
			double impulse = 0.0;
			float vrel_t_pre[3];
			float temp[3];

			// calculate tangential velocity
			copy_v3_v3 ( temp, collpair->normal );
			mul_v3_fl( temp, magrelVel );
			sub_v3_v3v3 ( vrel_t_pre, relativeVelocity, temp );

			// Decrease in magnitude of relative tangential velocity due to coulomb friction
			// in original formula "magrelVel" should be the "change of relative velocity in normal direction"
			magtangent = MIN2 ( clmd->coll_parms->friction * 0.01 * magrelVel,sqrt ( dot_v3v3 ( vrel_t_pre,vrel_t_pre ) ) );

			// Apply friction impulse.
			if ( magtangent > ALMOST_ZERO )
			{
				normalize_v3( vrel_t_pre );

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
#endif

#if 0
static float projectPointOntoLine(float *p, float *a, float *b) 
{
	float ba[3], pa[3];
	sub_v3_v3v3(ba, b, a);
	sub_v3_v3v3(pa, p, a);
	return dot_v3v3(pa, ba) / dot_v3v3(ba, ba);
}

static void calculateEENormal(float *np1, float *np2, float *np3, float *np4,float *out_normal) 
{
	float line1[3], line2[3];
	float length;

	sub_v3_v3v3(line1, np2, np1);
	sub_v3_v3v3(line2, np3, np1);

	// printf("l1: %f, l1: %f, l2: %f, l2: %f\n", line1[0], line1[1], line2[0], line2[1]);

	cross_v3_v3v3(out_normal, line1, line2);

	

	length = normalize_v3(out_normal);
	if (length <= FLT_EPSILON)
	{ // lines are collinear
		sub_v3_v3v3(out_normal, np2, np1);
		normalize_v3(out_normal);
	}
}

static void findClosestPointsEE(float *x1, float *x2, float *x3, float *x4, float *w1, float *w2)
{
	float temp[3], temp2[3];
	
	double a, b, c, e, f; 

	sub_v3_v3v3(temp, x2, x1);
	a = dot_v3v3(temp, temp);

	sub_v3_v3v3(temp2, x4, x3);
	b = -dot_v3v3(temp, temp2);

	c = dot_v3v3(temp2, temp2);

	sub_v3_v3v3(temp2, x3, x1);
	e = dot_v3v3(temp, temp2);

	sub_v3_v3v3(temp, x4, x3);
	f = -dot_v3v3(temp, temp2);

	*w1 = (e * c - b * f) / (a * c - b * b);
	*w2 = (f - b * *w1) / c;

}

// calculates the distance of 2 edges
static float edgedge_distance(float np11[3], float np12[3], float np21[3], float np22[3], float *out_a1, float *out_a2, float *out_normal)
{
	float line1[3], line2[3], cross[3];
	float length;
	float temp[3], temp2[3];
	float dist_a1, dist_a2;
	
	sub_v3_v3v3(line1, np12, np11);
	sub_v3_v3v3(line2, np22, np21);

	cross_v3_v3v3(cross, line1, line2);
	length = dot_v3v3(cross, cross);

	if (length < FLT_EPSILON) 
	{
		*out_a2 = projectPointOntoLine(np11, np21, np22);
		if ((*out_a2 >= -FLT_EPSILON) && (*out_a2 <= 1.0 + FLT_EPSILON)) 
		{
			*out_a1 = 0;
			calculateEENormal(np11, np12, np21, np22, out_normal);
			sub_v3_v3v3(temp, np22, np21);
			mul_v3_fl(temp, *out_a2);
			VECADD(temp2, temp, np21);
			VECADD(temp2, temp2, np11);
			return dot_v3v3(temp2, temp2);
		}

		CLAMP(*out_a2, 0.0, 1.0);
		if (*out_a2 > .5) 
		{ // == 1.0
			*out_a1 = projectPointOntoLine(np22, np11, np12);
			if ((*out_a1 >= -FLT_EPSILON) && (*out_a1 <= 1.0 + FLT_EPSILON)) 
			{
				calculateEENormal(np11, np12, np21, np22, out_normal);

				// return (np22 - (np11 + (np12 - np11) * out_a1)).lengthSquared();
				sub_v3_v3v3(temp, np12, np11);
				mul_v3_fl(temp, *out_a1);
				VECADD(temp2, temp, np11);
				sub_v3_v3v3(temp2, np22, temp2);
				return dot_v3v3(temp2, temp2);
			}
		} 
		else 
		{ // == 0.0
			*out_a1 = projectPointOntoLine(np21, np11, np12);
			if ((*out_a1 >= -FLT_EPSILON) && (*out_a1 <= 1.0 + FLT_EPSILON)) 
			{
				calculateEENormal(np11, np11, np21, np22, out_normal);

				// return (np21 - (np11 + (np12 - np11) * out_a1)).lengthSquared();
				sub_v3_v3v3(temp, np12, np11);
				mul_v3_fl(temp, *out_a1);
				VECADD(temp2, temp, np11);
				sub_v3_v3v3(temp2, np21, temp2);
				return dot_v3v3(temp2, temp2);
			}
		}

		CLAMP(*out_a1, 0.0, 1.0);
		calculateEENormal(np11, np12, np21, np22, out_normal);
		if(*out_a1 > .5)
		{
			if(*out_a2 > .5)
			{
				sub_v3_v3v3(temp, np12, np22);
			}
			else
			{
				sub_v3_v3v3(temp, np12, np21);
			}
		}
		else
		{
			if(*out_a2 > .5)
			{
				sub_v3_v3v3(temp, np11, np22);
			}
			else
			{
				sub_v3_v3v3(temp, np11, np21);
			}
		}

		return dot_v3v3(temp, temp);
	}
	else
	{
		
		// If the lines aren't parallel (but coplanar) they have to intersect

		findClosestPointsEE(np11, np12, np21, np22, out_a1, out_a2);

		// If both points are on the finite edges, we're done.
		if (*out_a1 >= 0.0 && *out_a1 <= 1.0 && *out_a2 >= 0.0 && *out_a2 <= 1.0) 
		{
			float p1[3], p2[3];
			
			// p1= np11 + (np12 - np11) * out_a1;
			sub_v3_v3v3(temp, np12, np11);
			mul_v3_fl(temp, *out_a1);
			VECADD(p1, np11, temp);
			
			// p2 = np21 + (np22 - np21) * out_a2;
			sub_v3_v3v3(temp, np22, np21);
			mul_v3_fl(temp, *out_a2);
			VECADD(p2, np21, temp);

			calculateEENormal(np11, np12, np21, np22, out_normal);
			sub_v3_v3v3(temp, p1, p2);
			return dot_v3v3(temp, temp);
		}

		
		/*
		* Clamp both points to the finite edges.
		* The one that moves most during clamping is one part of the solution.
		*/
		dist_a1 = *out_a1;
		CLAMP(dist_a1, 0.0, 1.0);
		dist_a2 = *out_a2;
		CLAMP(dist_a2, 0.0, 1.0);

		// Now project the "most clamped" point on the other line.
		if (dist_a1 > dist_a2) 
		{ 
			/* keep out_a1 */
			float p1[3];

			// p1 = np11 + (np12 - np11) * out_a1;
			sub_v3_v3v3(temp, np12, np11);
			mul_v3_fl(temp, *out_a1);
			VECADD(p1, np11, temp);

			*out_a2 = projectPointOntoLine(p1, np21, np22);
			CLAMP(*out_a2, 0.0, 1.0);

			calculateEENormal(np11, np12, np21, np22, out_normal);

			// return (p1 - (np21 + (np22 - np21) * out_a2)).lengthSquared();
			sub_v3_v3v3(temp, np22, np21);
			mul_v3_fl(temp, *out_a2);
			VECADD(temp, temp, np21);
			sub_v3_v3v3(temp, p1, temp);
			return dot_v3v3(temp, temp);
		} 
		else 
		{	
			/* keep out_a2 */
			float p2[3];
			
			// p2 = np21 + (np22 - np21) * out_a2;
			sub_v3_v3v3(temp, np22, np21);
			mul_v3_fl(temp, *out_a2);
			VECADD(p2, np21, temp);

			*out_a1 = projectPointOntoLine(p2, np11, np12);
			CLAMP(*out_a1, 0.0, 1.0);

			calculateEENormal(np11, np12, np21, np22, out_normal);
			
			// return ((np11 + (np12 - np11) * out_a1) - p2).lengthSquared();
			sub_v3_v3v3(temp, np12, np11);
			mul_v3_fl(temp, *out_a1);
			VECADD(temp, temp, np11);
			sub_v3_v3v3(temp, temp, p2);
			return dot_v3v3(temp, temp);
		}
	}
	
	printf("Error in edgedge_distance: end of function\n");
	return 0;
}

static int cloth_collision_moving_edges ( ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair )
{
	EdgeCollPair edgecollpair;
	Cloth *cloth1=NULL;
	ClothVertex *verts1=NULL;
	unsigned int i = 0, k = 0;
	int numsolutions = 0;
	double x1[3], v1[3], x2[3], v2[3], x3[3], v3[3];
	double solution[3], solution2[3];
	MVert *verts2 = collmd->current_x; // old x
	MVert *velocity2 = collmd->current_v; // velocity
	float distance = 0;
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
		/*
		if((edgecollpair.p11 == 3) && (edgecollpair.p12 == 16))
			printf("Ahier!\n");
		if((edgecollpair.p11 == 16) && (edgecollpair.p12 == 3))
			printf("Ahier!\n");
		*/

		// if ( !cloth_are_edges_adjacent ( clmd, collmd, &edgecollpair ) )
		{
			// always put coll points in p21/p22
			sub_v3_v3v3 ( x1, verts1[edgecollpair.p12].txold, verts1[edgecollpair.p11].txold );
			sub_v3_v3v3 ( v1, verts1[edgecollpair.p12].tv, verts1[edgecollpair.p11].tv );

			sub_v3_v3v3 ( x2, verts2[edgecollpair.p21].co, verts1[edgecollpair.p11].txold );
			sub_v3_v3v3 ( v2, velocity2[edgecollpair.p21].co, verts1[edgecollpair.p11].tv );

			sub_v3_v3v3 ( x3, verts2[edgecollpair.p22].co, verts1[edgecollpair.p11].txold );
			sub_v3_v3v3 ( v3, velocity2[edgecollpair.p22].co, verts1[edgecollpair.p11].tv );

			numsolutions = cloth_get_collision_time ( x1, v1, x2, v2, x3, v3, solution );

			if((edgecollpair.p11 == 3 && edgecollpair.p12==16)|| (edgecollpair.p11==16 && edgecollpair.p12==3))
			{
				if(edgecollpair.p21==6 || edgecollpair.p22 == 6)
				{
					printf("dist: %f, sol[k]: %f, sol2[k]: %f\n", distance, solution[k], solution2[k]);
					printf("a1: %f, a2: %f, b1: %f, b2: %f\n", x1[0], x2[0], x3[0], v1[0]);
					printf("b21: %d, b22: %d\n", edgecollpair.p21, edgecollpair.p22);
				}
			}

			for ( k = 0; k < numsolutions; k++ )
			{
				// printf("sol %d: %lf\n", k, solution[k]);
				if ( ( solution[k] >= ALMOST_ZERO ) && ( solution[k] <= 1.0 ) && ( solution[k] >  ALMOST_ZERO))
				{
					float a,b;
					float out_normal[3];
					float distance;
					float impulse = 0;
					float I_mag;

					// move verts
					VECADDS(triA[0], verts1[edgecollpair.p11].txold, verts1[edgecollpair.p11].tv, solution[k]);
					VECADDS(triA[1], verts1[edgecollpair.p12].txold, verts1[edgecollpair.p12].tv, solution[k]);

					VECADDS(triB[0], collmd->current_x[edgecollpair.p21].co, collmd->current_v[edgecollpair.p21].co, solution[k]);
					VECADDS(triB[1], collmd->current_x[edgecollpair.p22].co, collmd->current_v[edgecollpair.p22].co, solution[k]);

					// TODO: check for collisions
					distance = edgedge_distance(triA[0], triA[1], triB[0], triB[1], &a, &b, out_normal);
					
					if ((distance <= clmd->coll_parms->epsilon + BLI_bvhtree_getepsilon ( collmd->bvhtree ) + ALMOST_ZERO) && (dot_v3v3(out_normal, out_normal) > 0))
					{
						float vrel_1_to_2[3], temp[3], temp2[3], out_normalVelocity;
						float desiredVn;

						copy_v3_v3(vrel_1_to_2, verts1[edgecollpair.p11].tv);
						mul_v3_fl(vrel_1_to_2, 1.0 - a);
						copy_v3_v3(temp, verts1[edgecollpair.p12].tv);
						mul_v3_fl(temp, a);

						VECADD(vrel_1_to_2, vrel_1_to_2, temp);

						copy_v3_v3(temp, verts1[edgecollpair.p21].tv);
						mul_v3_fl(temp, 1.0 - b);
						copy_v3_v3(temp2, verts1[edgecollpair.p22].tv);
						mul_v3_fl(temp2, b);
						VECADD(temp, temp, temp2);

						sub_v3_v3v3(vrel_1_to_2, vrel_1_to_2, temp);

						out_normalVelocity = dot_v3v3(vrel_1_to_2, out_normal);
/*
						// this correction results in wrong normals sometimes?
						if(out_normalVelocity < 0.0)
						{
							out_normalVelocity*= -1.0;
							negate_v3(out_normal);
						}
*/
						/* Inelastic repulsion impulse. */

						// Calculate which normal velocity we need. 
						desiredVn = (out_normalVelocity * (float)solution[k] - (.1 * (clmd->coll_parms->epsilon + BLI_bvhtree_getepsilon ( collmd->bvhtree )) - sqrt(distance)) - ALMOST_ZERO);

						// Now calculate what impulse we need to reach that velocity. 
						I_mag = (out_normalVelocity - desiredVn) / 2.0; // / (1/m1 + 1/m2);

						// Finally apply that impulse. 
						impulse = (2.0 * -I_mag) / (a*a + (1.0-a)*(1.0-a) + b*b + (1.0-b)*(1.0-b));

						VECADDMUL ( verts1[edgecollpair.p11].impulse, out_normal, (1.0-a) * impulse );
						verts1[edgecollpair.p11].impulse_count++;

						VECADDMUL ( verts1[edgecollpair.p12].impulse, out_normal, a * impulse );
						verts1[edgecollpair.p12].impulse_count++;

						// return true;
						result = 1;
						break;
					}
					else
					{
						// missing from collision.hpp
					}
					// mintime = MIN2(mintime, (float)solution[k]);

					break;
				}
			}
		}
	}
	return result;
}

static int cloth_collision_moving ( ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair, CollPair *collision_end )
{
	Cloth *cloth1;
	cloth1 = clmd->clothObject;

	for ( ; collpair != collision_end; collpair++ )
	{
		// only handle moving collisions here
		if (!( collpair->flag & COLLISION_IN_FUTURE ))
			continue;

		cloth_collision_moving_edges ( clmd, collmd, collpair);
		// cloth_collision_moving_tris ( clmd, collmd, collpair);
	}

	return 1;
}
#endif

static void add_collision_object(Object ***objs, unsigned int *numobj, unsigned int *maxobj, Object *ob, Object *self, int level)
{
	CollisionModifierData *cmd= NULL;

	if(ob == self)
		return;

	/* only get objects with collision modifier */
	if(ob->pd && ob->pd->deflect)
		cmd= (CollisionModifierData *)modifiers_findByType(ob, eModifierType_Collision);
	
	if(cmd) {	
		/* extend array */
		if(*numobj >= *maxobj) {
			*maxobj *= 2;
			*objs= MEM_reallocN(*objs, sizeof(Object*)*(*maxobj));
		}
		
		(*objs)[*numobj] = ob;
		(*numobj)++;
	}

	/* objects in dupli groups, one level only for now */
	if(ob->dup_group && level == 0) {
		GroupObject *go;
		Group *group= ob->dup_group;

		/* add objects */
		for(go= group->gobject.first; go; go= go->next)
			add_collision_object(objs, numobj, maxobj, go->ob, self, level+1);
	}	
}

// return all collision objects in scene
// collision object will exclude self 
Object **get_collisionobjects(Scene *scene, Object *self, Group *group, unsigned int *numcollobj)
{
	Base *base;
	Object **objs;
	GroupObject *go;
	unsigned int numobj= 0, maxobj= 100;
	
	objs= MEM_callocN(sizeof(Object *)*maxobj, "CollisionObjectsArray");

	/* gather all collision objects */
	if(group) {
		/* use specified group */
		for(go= group->gobject.first; go; go= go->next)
			add_collision_object(&objs, &numobj, &maxobj, go->ob, self, 0);
	}
	else {
		Scene *sce_iter;
		/* add objects in same layer in scene */
		for(SETLOOPER(scene, sce_iter, base)) {
			if(base->lay & self->lay)
				add_collision_object(&objs, &numobj, &maxobj, base->object, self, 0);

		}
	}

	*numcollobj= numobj;

	return objs;
}

static void add_collider_cache_object(ListBase **objs, Object *ob, Object *self, int level)
{
	CollisionModifierData *cmd= NULL;
	ColliderCache *col;

	if(ob == self)
		return;

	if(ob->pd && ob->pd->deflect)
		cmd =(CollisionModifierData *)modifiers_findByType(ob, eModifierType_Collision);
	
	if(cmd && cmd->bvhtree) {	
		if(*objs == NULL)
			*objs = MEM_callocN(sizeof(ListBase), "ColliderCache array");

		col = MEM_callocN(sizeof(ColliderCache), "ColliderCache");
		col->ob = ob;
		col->collmd = cmd;
		/* make sure collider is properly set up */
		collision_move_object(cmd, 1.0, 0.0);
		BLI_addtail(*objs, col);
	}

	/* objects in dupli groups, one level only for now */
	if(ob->dup_group && level == 0) {
		GroupObject *go;
		Group *group= ob->dup_group;

		/* add objects */
		for(go= group->gobject.first; go; go= go->next)
			add_collider_cache_object(objs, go->ob, self, level+1);
	}
}

ListBase *get_collider_cache(Scene *scene, Object *self, Group *group)
{
	GroupObject *go;
	ListBase *objs= NULL;
	
	/* add object in same layer in scene */
	if(group) {
		for(go= group->gobject.first; go; go= go->next)
			add_collider_cache_object(&objs, go->ob, self, 0);
	}
	else {
		Scene *sce_iter;
		Base *base;

		/* add objects in same layer in scene */
		for(SETLOOPER(scene, sce_iter, base)) {
			if(!self || (base->lay & self->lay))
				add_collider_cache_object(&objs, base->object, self, 0);

		}
	}

	return objs;
}

void free_collider_cache(ListBase **colliders)
{
	if(*colliders) {
		BLI_freelistN(*colliders);
		MEM_freeN(*colliders);
		*colliders = NULL;
	}
}


static void cloth_bvh_objcollisions_nearcheck ( ClothModifierData * clmd, CollisionModifierData *collmd,
	CollPair **collisions, CollPair **collisions_index, int numresult, BVHTreeOverlap *overlap, double dt)
{
	int i;
#ifdef WITH_ELTOPO
	GHash *visithash = BLI_ghash_new(edgepair_hash, edgepair_cmp, "visthash, collision.c");
	GHash *tri_visithash = BLI_ghash_new(tripair_hash, tripair_cmp, "tri_visthash, collision.c");
	MemArena *arena = BLI_memarena_new(1<<16, "edge hash arena, collision.c");
#endif
	
	*collisions = ( CollPair* ) MEM_mallocN ( sizeof ( CollPair ) * numresult * 64, "collision array" ); //*4 since cloth_collision_static can return more than 1 collision
	*collisions_index = *collisions;
	
#ifdef WITH_ELTOPO
	machine_epsilon_offset(clmd->clothObject);

	for ( i = 0; i < numresult; i++ )
	{
		*collisions_index = cloth_collision ( ( ModifierData * ) clmd, ( ModifierData * ) collmd,
											  overlap+i, *collisions_index, dt, tri_visithash, arena );
	}

	for ( i = 0; i < numresult; i++ )
	{
		*collisions_index = cloth_edge_collision ( ( ModifierData * ) clmd, ( ModifierData * ) collmd,
												   overlap+i, *collisions_index, visithash, arena );
	}
	BLI_ghash_free(visithash, NULL, NULL);
	BLI_ghash_free(tri_visithash, NULL, NULL);
	BLI_memarena_free(arena);
#else /* WITH_ELTOPO */
	for ( i = 0; i < numresult; i++ )
	{
		*collisions_index = cloth_collision ( ( ModifierData * ) clmd, ( ModifierData * ) collmd,
											  overlap+i, *collisions_index, dt );
	}
#endif /* WITH_ELTOPO */

}

static int cloth_bvh_objcollisions_resolve ( ClothModifierData * clmd, CollisionModifierData *collmd, CollPair *collisions, CollPair *collisions_index)
{
	Cloth *cloth = clmd->clothObject;
	int i=0, j = 0, /*numfaces = 0,*/ numverts = 0;
	ClothVertex *verts = NULL;
	int ret = 0;
	int result = 0;
	float tnull[3] = {0,0,0};
	
	/*numfaces = clmd->clothObject->numfaces;*/ /*UNUSED*/
	numverts = clmd->clothObject->numverts;
 
	verts = cloth->verts;
	
	// process all collisions (calculate impulses, TODO: also repulses if distance too short)
	result = 1;
	for ( j = 0; j < 5; j++ ) // 5 is just a value that ensures convergence
	{
		result = 0;

		if ( collmd->bvhtree )
		{
#ifdef WITH_ELTOPO
			result += cloth_collision_response_moving(clmd, collmd, collisions, collisions_index);
			result += cloth_edge_collision_response_moving(clmd, collmd, collisions, collisions_index);
#else
			result += cloth_collision_response_static ( clmd, collmd, collisions, collisions_index );
#endif
#ifdef WITH_ELTOPO
			{
#else
			// apply impulses in parallel
			if ( result )
			{
#endif
				for ( i = 0; i < numverts; i++ )
				{
					// calculate "velocities" (just xnew = xold + v; no dt in v)
					if ( verts[i].impulse_count )
					{
						VECADDMUL ( verts[i].tv, verts[i].impulse, 1.0f / verts[i].impulse_count );
						copy_v3_v3 ( verts[i].impulse, tnull );
						verts[i].impulse_count = 0;

						ret++;
					}
				}
			}
		}
	}
	return ret;
}

// cloth - object collisions
int cloth_bvh_objcollision (Object *ob, ClothModifierData * clmd, float step, float dt )
{
	Cloth *cloth= clmd->clothObject;
	BVHTree *cloth_bvh= cloth->bvhtree;
	unsigned int i=0, /* numfaces = 0, */ /* UNUSED */ numverts = 0, k, l, j;
	int rounds = 0; // result counts applied collisions; ic is for debug output;
	ClothVertex *verts = NULL;
	int ret = 0, ret2 = 0;
	Object **collobjs = NULL;
	unsigned int numcollobj = 0;

	if ((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_COLLOBJ) || cloth_bvh==NULL)
		return 0;
	
	verts = cloth->verts;
	/* numfaces = cloth->numfaces; */ /* UNUSED */
	numverts = cloth->numverts;

	////////////////////////////////////////////////////////////
	// static collisions
	////////////////////////////////////////////////////////////

	// update cloth bvh
	bvhtree_update_from_cloth ( clmd, 1 ); // 0 means STATIC, 1 means MOVING (see later in this function)
	bvhselftree_update_from_cloth ( clmd, 0 ); // 0 means STATIC, 1 means MOVING (see later in this function)
	
	collobjs = get_collisionobjects(clmd->scene, ob, clmd->coll_parms->group, &numcollobj);
	
	if(!collobjs)
		return 0;

	do
	{
		CollPair **collisions, **collisions_index;
		
		ret2 = 0;

		collisions = MEM_callocN(sizeof(CollPair *) *numcollobj , "CollPair");
		collisions_index = MEM_callocN(sizeof(CollPair *) *numcollobj , "CollPair");
		
		// check all collision objects
		for(i = 0; i < numcollobj; i++)
		{
			Object *collob= collobjs[i];
			CollisionModifierData *collmd = (CollisionModifierData*)modifiers_findByType(collob, eModifierType_Collision);
			BVHTreeOverlap *overlap = NULL;
			unsigned int result = 0;
			
			if(!collmd->bvhtree)
				continue;
			
			/* move object to position (step) in time */
			
			collision_move_object ( collmd, step + dt, step );
			
			/* search for overlapping collision pairs */
			overlap = BLI_bvhtree_overlap ( cloth_bvh, collmd->bvhtree, &result );
				
			// go to next object if no overlap is there
			if( result && overlap ) {
				/* check if collisions really happen (costly near check) */
				cloth_bvh_objcollisions_nearcheck ( clmd, collmd, &collisions[i], 
					&collisions_index[i], result, overlap, dt/(float)clmd->coll_parms->loop_count);
			
				// resolve nearby collisions
				ret += cloth_bvh_objcollisions_resolve ( clmd, collmd, collisions[i],  collisions_index[i]);
				ret2 += ret;
			}

			if ( overlap )
				MEM_freeN ( overlap );
		}
		rounds++;
		
		for(i = 0; i < numcollobj; i++)
		{
			if ( collisions[i] ) MEM_freeN ( collisions[i] );
		}
			
		MEM_freeN(collisions);
		MEM_freeN(collisions_index);

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
			for(l = 0; l < (unsigned int)clmd->coll_parms->self_loop_count; l++)
			{
				// TODO: add coll quality rounds again
				BVHTreeOverlap *overlap = NULL;
				unsigned int result = 0;
	
				// collisions = 1;
				verts = cloth->verts; // needed for openMP
	
				/* numfaces = cloth->numfaces; */ /* UNUSED */
				numverts = cloth->numverts;
	
				verts = cloth->verts;
	
				if ( cloth->bvhselftree )
				{
					// search for overlapping collision pairs 
					overlap = BLI_bvhtree_overlap ( cloth->bvhselftree, cloth->bvhselftree, &result );
	
	// #pragma omp parallel for private(k, i, j) schedule(static)
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
	
						sub_v3_v3v3 ( temp, verts[i].tx, verts[j].tx );
	
						if ( ( ABS ( temp[0] ) > mindistance ) || ( ABS ( temp[1] ) > mindistance ) || ( ABS ( temp[2] ) > mindistance ) ) continue;
	
						// check for adjacent points (i must be smaller j)
						if ( BLI_edgehash_haskey ( cloth->edgehash, MIN2(i, j), MAX2(i, j) ) )
						{
							continue;
						}
	
						length = normalize_v3( temp );
	
						if ( length < mindistance )
						{
							float correction = mindistance - length;
	
							if ( cloth->verts [i].flags & CLOTH_VERT_FLAG_PINNED )
							{
								mul_v3_fl( temp, -correction );
								VECADD ( verts[j].tx, verts[j].tx, temp );
							}
							else if ( cloth->verts [j].flags & CLOTH_VERT_FLAG_PINNED )
							{
								mul_v3_fl( temp, correction );
								VECADD ( verts[i].tx, verts[i].tx, temp );
							}
							else
							{
								mul_v3_fl( temp, correction * -0.5 );
								VECADD ( verts[j].tx, verts[j].tx, temp );
	
								sub_v3_v3v3 ( verts[i].tx, verts[i].tx, temp );
							}
							ret = 1;
							ret2 += ret;
						}
						else {
							// check for approximated time collisions
						}
					}
	
					if ( overlap )
						MEM_freeN ( overlap );
	
				}
			}
			////////////////////////////////////////////////////////////

			////////////////////////////////////////////////////////////
			// SELFCOLLISIONS: update velocities
			////////////////////////////////////////////////////////////
			if ( ret2 )
			{
				for ( i = 0; i < cloth->numverts; i++ )
				{
					if ( ! ( verts [i].flags & CLOTH_VERT_FLAG_PINNED ) )
					{
						sub_v3_v3v3 ( verts[i].tv, verts[i].tx, verts[i].txold );
					}
				}
			}
			////////////////////////////////////////////////////////////
		}
	}
	while ( ret2 && ( clmd->coll_parms->loop_count>rounds ) );
	
	if(collobjs)
		MEM_freeN(collobjs);

	return 1|MIN2 ( ret, 1 );
}
