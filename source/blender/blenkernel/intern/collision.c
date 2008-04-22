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
#include "BKE_cloth.h"
#include "BKE_modifier.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "mydevice.h"

#include "Bullet-C-Api.h"

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
	bvh_update_from_mvert ( collmd->bvh, collmd->current_x, collmd->numverts, collmd->current_xnew, 1 );
}

/* build bounding volume hierarchy from mverts (see kdop.c for whole BVH code) */
BVH *bvh_build_from_mvert ( MFace *mfaces, unsigned int numfaces, MVert *x, unsigned int numverts, float epsilon )
{
	BVH *bvh=NULL;

	bvh = MEM_callocN ( sizeof ( BVH ), "BVH" );
	if ( bvh == NULL )
	{
		printf ( "bvh: Out of memory.\n" );
		return NULL;
	}

	// in the moment, return zero if no faces there
	if ( !numfaces )
		return NULL;

	bvh->epsilon = epsilon;
	bvh->numfaces = numfaces;
	bvh->mfaces = mfaces;

	// we have no faces, we save seperate points
	if ( !mfaces )
	{
		bvh->numfaces = numverts;
	}

	bvh->numverts = numverts;
	bvh->current_x = MEM_dupallocN ( x );

	bvh_build ( bvh );

	return bvh;
}

void bvh_update_from_mvert ( BVH * bvh, MVert *x, unsigned int numverts, MVert *xnew, int moving )
{
	if ( !bvh )
		return;

	if ( numverts!=bvh->numverts )
		return;

	if ( x )
		memcpy ( bvh->current_xold, x, sizeof ( MVert ) * numverts );

	if ( xnew )
		memcpy ( bvh->current_x, xnew, sizeof ( MVert ) * numverts );

	bvh_update ( bvh, moving );
}

/***********************************
Collision modifier code end
***********************************/

/**
 * gsl_poly_solve_cubic -
 *
 * copied from SOLVE_CUBIC.C --> GSL
 */

/* DG: debug hint! don't forget that all functions were "fabs", "sinf", etc before */
#define mySWAP(a,b) { float tmp = b ; b = a ; a = tmp ; }

int gsl_poly_solve_cubic ( float a, float b, float c, float *x0, float *x1, float *x2 )
{
	float q = ( a * a - 3 * b );
	float r = ( 2 * a * a * a - 9 * a * b + 27 * c );

	float Q = q / 9;
	float R = r / 54;

	float Q3 = Q * Q * Q;
	float R2 = R * R;

	float CR2 = 729 * r * r;
	float CQ3 = 2916 * q * q * q;

	if ( R == 0 && Q == 0 )
	{
		*x0 = - a / 3 ;
		*x1 = - a / 3 ;
		*x2 = - a / 3 ;
		return 3 ;
	}
	else if ( CR2 == CQ3 )
	{
		/* this test is actually R2 == Q3, written in a form suitable
		  for exact computation with integers */

		/* Due to finite precision some float roots may be missed, and
		  considered to be a pair of complex roots z = x +/- epsilon i
		  close to the real axis. */

		float sqrtQ = sqrt ( Q );

		if ( R > 0 )
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
	else if ( CR2 < CQ3 ) /* equivalent to R2 < Q3 */
	{
		float sqrtQ = sqrt ( Q );
		float sqrtQ3 = sqrtQ * sqrtQ * sqrtQ;
		float theta = acos ( R / sqrtQ3 );
		float norm = -2 * sqrtQ;
		*x0 = norm * cos ( theta / 3 ) - a / 3;
		*x1 = norm * cos ( ( theta + 2.0 * M_PI ) / 3 ) - a / 3;
		*x2 = norm * cos ( ( theta - 2.0 * M_PI ) / 3 ) - a / 3;

		/* Sort *x0, *x1, *x2 into increasing order */

		if ( *x0 > *x1 )
			mySWAP ( *x0, *x1 ) ;

		if ( *x1 > *x2 )
		{
			mySWAP ( *x1, *x2 ) ;

			if ( *x0 > *x1 )
				mySWAP ( *x0, *x1 ) ;
		}

		return 3;
	}
	else
	{
		float sgnR = ( R >= 0 ? 1 : -1 );
		float A = -sgnR * pow ( ABS ( R ) + sqrt ( R2 - Q3 ), 1.0/3.0 );
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
int gsl_poly_solve_quadratic ( float a, float b, float c,  float *x0, float *x1 )
{
	float disc = b * b - 4 * a * c;

	if ( disc > 0 )
	{
		if ( b == 0 )
		{
			float r = ABS ( 0.5 * sqrt ( disc ) / a );
			*x0 = -r;
			*x1 =  r;
		}
		else
		{
			float sgnb = ( b > 0 ? 1 : -1 );
			float temp = -0.5 * ( b + sgnb * sqrt ( disc ) );
			float r1 = temp / a ;
			float r2 = c / temp ;

			if ( r1 < r2 )
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
	else if ( disc == 0 )
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

int cloth_get_collision_time ( float a[3], float b[3], float c[3], float d[3], float e[3], float f[3], float solution[3] )
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
	if ( ABS ( j ) > ALMOST_ZERO )
	{
		i /= j;
		h /= j;
		g /= j;

		num_sols = gsl_poly_solve_cubic ( i, h, g, &solution[0], &solution[1], &solution[2] );
	}
	else if ( ABS ( i ) > ALMOST_ZERO )
	{
		num_sols = gsl_poly_solve_quadratic ( i, h, g, &solution[0], &solution[1] );
		solution[2] = -1.0;
	}
	else if ( ABS ( h ) > ALMOST_ZERO )
	{
		solution[0] = -g / h;
		solution[1] = solution[2] = -1.0;
		num_sols = 1;
	}
	else if ( ABS ( g ) > ALMOST_ZERO )
	{
		solution[0] = 0;
		solution[1] = solution[2] = -1.0;
		num_sols = 1;
	}

	// Discard negative solutions
	if ( ( num_sols >= 1 ) && ( solution[0] < 0 ) )
	{
		--num_sols;
		solution[0] = solution[num_sols];
	}
	if ( ( num_sols >= 2 ) && ( solution[1] < 0 ) )
	{
		--num_sols;
		solution[1] = solution[num_sols];
	}
	if ( ( num_sols == 3 ) && ( solution[2] < 0 ) )
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

int cloth_collision_response_static ( ClothModifierData *clmd, CollisionModifierData *collmd )
{
	int result = 0;
	LinkNode *search = NULL;
	CollPair *collpair = NULL;
	Cloth *cloth1;
	float w1, w2, w3, u1, u2, u3;
	float v1[3], v2[3], relativeVelocity[3];
	float magrelVel;
	float epsilon2 = collmd->bvh->epsilon;

	cloth1 = clmd->clothObject;

	search = clmd->coll_parms->collision_list;

	while ( search )
	{
		collpair = search->link;

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

		search = search->next;
	}


	return result;
}

int cloth_collision_response_moving_tris ( ClothModifierData *clmd, ClothModifierData *coll_clmd )
{
	return 1;
}


int cloth_collision_response_moving_edges ( ClothModifierData *clmd, ClothModifierData *coll_clmd )
{
	return 1;
}

void cloth_collision_static ( ModifierData *md1, ModifierData *md2, CollisionTree *tree1, CollisionTree *tree2 )
{
	ClothModifierData *clmd = ( ClothModifierData * ) md1;
	CollisionModifierData *collmd = ( CollisionModifierData * ) md2;
	CollPair *collpair = NULL;
	Cloth *cloth1=NULL;
	MFace *face1=NULL, *face2=NULL;
	ClothVertex *verts1=NULL;
	double distance = 0;
	float epsilon = clmd->coll_parms->epsilon;
	float epsilon2 = ( ( CollisionModifierData * ) md2 )->bvh->epsilon;
	unsigned int i = 0;

	for ( i = 0; i < 4; i++ )
	{
		collpair = ( CollPair * ) MEM_callocN ( sizeof ( CollPair ), "cloth coll pair" );

		cloth1 = clmd->clothObject;

		verts1 = cloth1->verts;

		face1 = & ( cloth1->mfaces[tree1->tri_index] );
		face2 = & ( collmd->mfaces[tree2->tri_index] );

		// check all possible pairs of triangles
		if ( i == 0 )
		{
			collpair->ap1 = face1->v1;
			collpair->ap2 = face1->v2;
			collpair->ap3 = face1->v3;

			collpair->bp1 = face2->v1;
			collpair->bp2 = face2->v2;
			collpair->bp3 = face2->v3;

		}

		if ( i == 1 )
		{
			if ( face1->v4 )
			{
				collpair->ap1 = face1->v3;
				collpair->ap2 = face1->v4;
				collpair->ap3 = face1->v1;

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
				collpair->ap1 = face1->v1;
				collpair->ap2 = face1->v2;
				collpair->ap3 = face1->v3;

				collpair->bp1 = face2->v3;
				collpair->bp2 = face2->v4;
				collpair->bp3 = face2->v1;
			}
			else
				i+=2;
		}

		if ( i == 3 )
		{
			if ( ( face1->v4 ) && ( face2->v4 ) )
			{
				collpair->ap1 = face1->v3;
				collpair->ap2 = face1->v4;
				collpair->ap3 = face1->v1;

				collpair->bp1 = face2->v3;
				collpair->bp2 = face2->v4;
				collpair->bp3 = face2->v1;
			}
			else
				i++;
		}

		// calc SIPcode (?)

		if ( i < 4 )
		{
			// calc distance + normal
#ifdef WITH_BULLET
			distance = plNearestPoints (
			               verts1[collpair->ap1].txold, verts1[collpair->ap2].txold, verts1[collpair->ap3].txold, collmd->current_x[collpair->bp1].co, collmd->current_x[collpair->bp2].co, collmd->current_x[collpair->bp3].co, collpair->pa,collpair->pb,collpair->vector );
#else
			// just be sure that we don't add anything
			distance = 2.0 * ( epsilon + epsilon2 + ALMOST_ZERO );
#endif
			if ( distance <= ( epsilon + epsilon2 + ALMOST_ZERO ) )
			{
				// printf("dist: %f\n", (float)distance);

				// collpair->face1 = tree1->tri_index;
				// collpair->face2 = tree2->tri_index;

				VECCOPY ( collpair->normal, collpair->vector );
				Normalize ( collpair->normal );

				collpair->distance = distance;
				BLI_linklist_prepend ( &clmd->coll_parms->collision_list, collpair );

			}
			else
			{
				MEM_freeN ( collpair );
			}
		}
		else
		{
			MEM_freeN ( collpair );
		}
	}
}

int cloth_are_edges_adjacent ( ClothModifierData *clmd, ClothModifierData *coll_clmd, EdgeCollPair *edgecollpair )
{
	Cloth *cloth1 = NULL, *cloth2 = NULL;
	ClothVertex *verts1 = NULL, *verts2 = NULL;
	float temp[3];

	cloth1 = clmd->clothObject;
	cloth2 = coll_clmd->clothObject;

	verts1 = cloth1->verts;
	verts2 = cloth2->verts;

	VECSUB ( temp, verts1[edgecollpair->p11].xold, verts2[edgecollpair->p21].xold );
	if ( ABS ( INPR ( temp, temp ) ) < ALMOST_ZERO )
		return 1;

	VECSUB ( temp, verts1[edgecollpair->p11].xold, verts2[edgecollpair->p22].xold );
	if ( ABS ( INPR ( temp, temp ) ) < ALMOST_ZERO )
		return 1;

	VECSUB ( temp, verts1[edgecollpair->p12].xold, verts2[edgecollpair->p21].xold );
	if ( ABS ( INPR ( temp, temp ) ) < ALMOST_ZERO )
		return 1;

	VECSUB ( temp, verts1[edgecollpair->p12].xold, verts2[edgecollpair->p22].xold );
	if ( ABS ( INPR ( temp, temp ) ) < ALMOST_ZERO )
		return 1;

	return 0;
}

void cloth_collision_moving_edges ( ClothModifierData *clmd, ClothModifierData *coll_clmd, CollisionTree *tree1, CollisionTree *tree2 )
{
	EdgeCollPair edgecollpair;
	Cloth *cloth1=NULL, *cloth2=NULL;
	MFace *face1=NULL, *face2=NULL;
	ClothVertex *verts1=NULL, *verts2=NULL;
	unsigned int i = 0, j = 0, k = 0;
	int numsolutions = 0;
	float a[3], b[3], c[3], d[3], e[3], f[3], solution[3];

	cloth1 = clmd->clothObject;
	cloth2 = coll_clmd->clothObject;

	verts1 = cloth1->verts;
	verts2 = cloth2->verts;

	face1 = & ( cloth1->mfaces[tree1->tri_index] );
	face2 = & ( cloth2->mfaces[tree2->tri_index] );

	for ( i = 0; i < 5; i++ )
	{
		if ( i == 0 )
		{
			edgecollpair.p11 = face1->v1;
			edgecollpair.p12 = face1->v2;
		}
		else if ( i == 1 )
		{
			edgecollpair.p11 = face1->v2;
			edgecollpair.p12 = face1->v3;
		}
		else if ( i == 2 )
		{
			if ( face1->v4 )
			{
				edgecollpair.p11 = face1->v3;
				edgecollpair.p12 = face1->v4;
			}
			else
			{
				edgecollpair.p11 = face1->v3;
				edgecollpair.p12 = face1->v1;
				i+=5; // get out of here after this edge pair is handled
			}
		}
		else if ( i == 3 )
		{
			if ( face1->v4 )
			{
				edgecollpair.p11 = face1->v4;
				edgecollpair.p12 = face1->v1;
			}
			else
				continue;
		}
		else
		{
			edgecollpair.p11 = face1->v3;
			edgecollpair.p12 = face1->v1;
		}


		for ( j = 0; j < 5; j++ )
		{
			if ( j == 0 )
			{
				edgecollpair.p21 = face2->v1;
				edgecollpair.p22 = face2->v2;
			}
			else if ( j == 1 )
			{
				edgecollpair.p21 = face2->v2;
				edgecollpair.p22 = face2->v3;
			}
			else if ( j == 2 )
			{
				if ( face2->v4 )
				{
					edgecollpair.p21 = face2->v3;
					edgecollpair.p22 = face2->v4;
				}
				else
				{
					edgecollpair.p21 = face2->v3;
					edgecollpair.p22 = face2->v1;
				}
			}
			else if ( j == 3 )
			{
				if ( face2->v4 )
				{
					edgecollpair.p21 = face2->v4;
					edgecollpair.p22 = face2->v1;
				}
				else
					continue;
			}
			else
			{
				edgecollpair.p21 = face2->v3;
				edgecollpair.p22 = face2->v1;
			}


			if ( !cloth_are_edges_adjacent ( clmd, coll_clmd, &edgecollpair ) )
			{
				VECSUB ( a, verts1[edgecollpair.p12].xold, verts1[edgecollpair.p11].xold );
				VECSUB ( b, verts1[edgecollpair.p12].v, verts1[edgecollpair.p11].v );
				VECSUB ( c, verts1[edgecollpair.p21].xold, verts1[edgecollpair.p11].xold );
				VECSUB ( d, verts1[edgecollpair.p21].v, verts1[edgecollpair.p11].v );
				VECSUB ( e, verts2[edgecollpair.p22].xold, verts1[edgecollpair.p11].xold );
				VECSUB ( f, verts2[edgecollpair.p22].v, verts1[edgecollpair.p11].v );

				numsolutions = cloth_get_collision_time ( a, b, c, d, e, f, solution );

				for ( k = 0; k < numsolutions; k++ )
				{
					if ( ( solution[k] >= 0.0 ) && ( solution[k] <= 1.0 ) )
					{
						//float out_collisionTime = solution[k];

						// TODO: check for collisions

						// TODO: put into (edge) collision list

						// printf("Moving edge found!\n");
					}
				}
			}
		}
	}
}

void cloth_collision_moving_tris ( ClothModifierData *clmd, ClothModifierData *coll_clmd, CollisionTree *tree1, CollisionTree *tree2 )
{
	CollPair collpair;
	Cloth *cloth1=NULL, *cloth2=NULL;
	MFace *face1=NULL, *face2=NULL;
	ClothVertex *verts1=NULL, *verts2=NULL;
	unsigned int i = 0, j = 0, k = 0;
	int numsolutions = 0;
	float a[3], b[3], c[3], d[3], e[3], f[3], solution[3];

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
					if ( ( solution[k] >= 0.0 ) && ( solution[k] <= 1.0 ) )
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

void cloth_collision_moving ( ClothModifierData *clmd, ClothModifierData *coll_clmd, CollisionTree *tree1, CollisionTree *tree2 )
{
	// TODO: check for adjacent
	cloth_collision_moving_edges ( clmd, coll_clmd, tree1, tree2 );

	cloth_collision_moving_tris ( clmd, coll_clmd, tree1, tree2 );
	cloth_collision_moving_tris ( coll_clmd, clmd, tree2, tree1 );
}

void cloth_free_collision_list ( ClothModifierData *clmd )
{
	// free collision list
	if ( clmd->coll_parms->collision_list )
	{
		LinkNode *search = clmd->coll_parms->collision_list;
		while ( search )
		{
			CollPair *coll_pair = search->link;

			MEM_freeN ( coll_pair );
			search = search->next;
		}
		BLI_linklist_free ( clmd->coll_parms->collision_list,NULL );

		clmd->coll_parms->collision_list = NULL;
	}
}

int cloth_bvh_objcollisions_do ( ClothModifierData * clmd, CollisionModifierData *collmd, float step, float dt )
{
	Cloth *cloth = clmd->clothObject;
	BVH *cloth_bvh= ( BVH * ) cloth->tree;
	long i=0, j = 0, numfaces = 0, numverts = 0;
	ClothVertex *verts = NULL;
	int ret = 0;
	unsigned int result = 0;
	float tnull[3] = {0,0,0};

	numfaces = clmd->clothObject->numfaces;
	numverts = clmd->clothObject->numverts;

	verts = cloth->verts;

	if ( collmd->bvh )
	{
		/* get pointer to bounding volume hierarchy */
		BVH *coll_bvh = collmd->bvh;

		/* move object to position (step) in time */
		collision_move_object ( collmd, step + dt, step );

		/* search for overlapping collision pairs */
		bvh_traverse ( ( ModifierData * ) clmd, ( ModifierData * ) collmd, cloth_bvh->root, coll_bvh->root, step, cloth_collision_static, 0 );
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

		if ( collmd->bvh )
			result += cloth_collision_response_static ( clmd, collmd );

		// apply impulses in parallel
		if ( result )
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

		if ( !result )
			break;
	}

	cloth_free_collision_list ( clmd );

	return ret;
}

// cloth - object collisions
int cloth_bvh_objcollision ( ClothModifierData * clmd, float step, float dt )
{
	Base *base=NULL;
	CollisionModifierData *collmd=NULL;
	Cloth *cloth=NULL;
	Object *coll_ob=NULL;
	BVH *cloth_bvh=NULL;
	long i=0, j = 0, numfaces = 0, numverts = 0;
	unsigned int result = 0, rounds = 0; // result counts applied collisions; ic is for debug output;
	ClothVertex *verts = NULL;
	int ret = 0;
	ClothModifierData *tclmd;
	int collisions = 0, count = 0;

	if ( ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_COLLOBJ ) || ! ( ( ( Cloth * ) clmd->clothObject )->tree ) )
	{
		return 0;
	}

	cloth = clmd->clothObject;
	verts = cloth->verts;
	cloth_bvh = ( BVH * ) cloth->tree;
	numfaces = clmd->clothObject->numfaces;
	numverts = clmd->clothObject->numverts;

	////////////////////////////////////////////////////////////
	// static collisions
	////////////////////////////////////////////////////////////

	// update cloth bvh
	bvh_update_from_cloth ( clmd, 0 ); // 0 means STATIC, 1 means MOVING (see later in this function)

	do
	{
		result = 0;
		clmd->coll_parms->collision_list = NULL;

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
			collisions = 1;
			verts = cloth->verts; // needed for openMP

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
