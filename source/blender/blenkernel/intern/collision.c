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
void collision_move_object(CollisionModifierData *collmd, float step, float prevstep)
{
	float tv[3] = {0, 0, 0};
	unsigned int i = 0;

	for ( i = 0; i < collmd->numverts; i++ ) {
		sub_v3_v3v3(tv, collmd->xnew[i].co, collmd->x[i].co);
		VECADDS(collmd->current_x[i].co, collmd->x[i].co, tv, prevstep);
		VECADDS(collmd->current_xnew[i].co, collmd->x[i].co, tv, step);
		sub_v3_v3v3(collmd->current_v[i].co, collmd->current_xnew[i].co, collmd->current_x[i].co);
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
	for ( i = 0; i < numfaces; i++, tface++ ) {
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

void bvhtree_update_from_mvert(BVHTree * bvhtree, MFace *faces, int numfaces, MVert *x, MVert *xnew, int UNUSED(numverts), int moving )
{
	int i;
	MFace *mfaces = faces;
	float co[12], co_moving[12];
	int ret = 0;

	if ( !bvhtree )
		return;

	if ( x ) {
		for ( i = 0; i < numfaces; i++, mfaces++ ) {
			copy_v3_v3 ( &co[0*3], x[mfaces->v1].co );
			copy_v3_v3 ( &co[1*3], x[mfaces->v2].co );
			copy_v3_v3 ( &co[2*3], x[mfaces->v3].co );
			if ( mfaces->v4 )
				copy_v3_v3 ( &co[3*3], x[mfaces->v4].co );

			// copy new locations into array
			if ( moving && xnew ) {
				// update moving positions
				copy_v3_v3 ( &co_moving[0*3], xnew[mfaces->v1].co );
				copy_v3_v3 ( &co_moving[1*3], xnew[mfaces->v2].co );
				copy_v3_v3 ( &co_moving[2*3], xnew[mfaces->v3].co );
				if ( mfaces->v4 )
					copy_v3_v3 ( &co_moving[3*3], xnew[mfaces->v4].co );

				ret = BLI_bvhtree_update_node ( bvhtree, i, co, co_moving, ( mfaces->v4 ? 4 : 3 ) );
			}
			else {
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
#define mySWAP(a, b) do { double tmp = b ; b = a ; a = tmp ; } while (0)


// w3 is not perfect
static void collision_compute_barycentric ( float pv[3], float p1[3], float p2[3], float p3[3], float *w1, float *w2, float *w3 )
{
	double	tempV1[3], tempV2[3], tempV4[3];
	double	a, b, c, d, e, f;

	VECSUB ( tempV1, p1, p3 );
	VECSUB ( tempV2, p2, p3 );
	VECSUB ( tempV4, pv, p3 );

	a = INPR ( tempV1, tempV1 );
	b = INPR ( tempV1, tempV2 );
	c = INPR ( tempV2, tempV2 );
	e = INPR ( tempV1, tempV4 );
	f = INPR ( tempV2, tempV4 );

	d = ( a * c - b * b );

	if ( ABS ( d ) < (double)ALMOST_ZERO ) {
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
	zero_v3(to);
	VECADDMUL(to, v1, w1);
	VECADDMUL(to, v2, w2);
	VECADDMUL(to, v3, w3);
}

static int cloth_collision_response_static ( ClothModifierData *clmd, CollisionModifierData *collmd, CollPair *collpair, CollPair *collision_end )
{
	int result = 0;
	Cloth *cloth1;
	float w1, w2, w3, u1, u2, u3;
	float v1[3], v2[3], relativeVelocity[3];
	float magrelVel;
	float epsilon2 = BLI_bvhtree_getepsilon ( collmd->bvhtree );

	cloth1 = clmd->clothObject;

	for ( ; collpair != collision_end; collpair++ ) {
		float i1[3], i2[3], i3[3];

		zero_v3(i1);
		zero_v3(i2);
		zero_v3(i3);

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

		sub_v3_v3v3(relativeVelocity, v2, v1);

		// Calculate the normal component of the relative velocity (actually only the magnitude - the direction is stored in 'normal').
		magrelVel = dot_v3v3(relativeVelocity, collpair->normal);

		// printf("magrelVel: %f\n", magrelVel);

		// Calculate masses of points.
		// TODO

		// If v_n_mag < 0 the edges are approaching each other.
		if ( magrelVel > ALMOST_ZERO ) {
			// Calculate Impulse magnitude to stop all motion in normal direction.
			float magtangent = 0, repulse = 0, d = 0;
			double impulse = 0.0;
			float vrel_t_pre[3];
			float temp[3], spf;

			// calculate tangential velocity
			copy_v3_v3 ( temp, collpair->normal );
			mul_v3_fl(temp, magrelVel);
			sub_v3_v3v3(vrel_t_pre, relativeVelocity, temp);

			// Decrease in magnitude of relative tangential velocity due to coulomb friction
			// in original formula "magrelVel" should be the "change of relative velocity in normal direction"
			magtangent = MIN2(clmd->coll_parms->friction * 0.01f * magrelVel, sqrtf(dot_v3v3(vrel_t_pre, vrel_t_pre)));

			// Apply friction impulse.
			if ( magtangent > ALMOST_ZERO ) {
				normalize_v3(vrel_t_pre);

				impulse = magtangent / ( 1.0f + w1*w1 + w2*w2 + w3*w3 ); // 2.0 *
				VECADDMUL ( i1, vrel_t_pre, w1 * impulse );
				VECADDMUL ( i2, vrel_t_pre, w2 * impulse );
				VECADDMUL ( i3, vrel_t_pre, w3 * impulse );
			}

			// Apply velocity stopping impulse
			// I_c = m * v_N / 2.0
			// no 2.0 * magrelVel normally, but looks nicer DG
			impulse =  magrelVel / ( 1.0 + w1*w1 + w2*w2 + w3*w3 );

			VECADDMUL ( i1, collpair->normal, w1 * impulse );
			cloth1->verts[collpair->ap1].impulse_count++;

			VECADDMUL ( i2, collpair->normal, w2 * impulse );
			cloth1->verts[collpair->ap2].impulse_count++;

			VECADDMUL ( i3, collpair->normal, w3 * impulse );
			cloth1->verts[collpair->ap3].impulse_count++;

			// Apply repulse impulse if distance too short
			// I_r = -min(dt*kd, m(0, 1d/dt - v_n))
			// DG: this formula ineeds to be changed for this code since we apply impulses/repulses like this:
			// v += impulse; x_new = x + v; 
			// We don't use dt!!
			// DG TODO: Fix usage of dt here!
			spf = (float)clmd->sim_parms->stepsPerFrame / clmd->sim_parms->timescale;

			d = clmd->coll_parms->epsilon*8.0f/9.0f + epsilon2*8.0f/9.0f - collpair->distance;
			if ( ( magrelVel < 0.1f*d*spf ) && ( d > ALMOST_ZERO ) ) {
				repulse = MIN2 ( d*1.0f/spf, 0.1f*d*spf - magrelVel );

				// stay on the safe side and clamp repulse
				if ( impulse > ALMOST_ZERO )
					repulse = MIN2 ( repulse, 5.0*impulse );
				repulse = MAX2 ( impulse, repulse );

				impulse = repulse / ( 1.0f + w1*w1 + w2*w2 + w3*w3 ); // original 2.0 / 0.25
				VECADDMUL ( i1, collpair->normal,  impulse );
				VECADDMUL ( i2, collpair->normal,  impulse );
				VECADDMUL ( i3, collpair->normal,  impulse );
			}

			result = 1;
		}
		else
		{
			// Apply repulse impulse if distance too short
			// I_r = -min(dt*kd, max(0, 1d/dt - v_n))
			// DG: this formula ineeds to be changed for this code since we apply impulses/repulses like this:
			// v += impulse; x_new = x + v; 
			// We don't use dt!!
			float spf = (float)clmd->sim_parms->stepsPerFrame / clmd->sim_parms->timescale;

			float d = clmd->coll_parms->epsilon*8.0f/9.0f + epsilon2*8.0f/9.0f - collpair->distance;
			if ( d > ALMOST_ZERO) {
				// stay on the safe side and clamp repulse
				float repulse = d*1.0f/spf;

				float impulse = repulse / ( 3.0 * ( 1.0f + w1*w1 + w2*w2 + w3*w3 )); // original 2.0 / 0.25 

				VECADDMUL ( i1, collpair->normal,  impulse );
				VECADDMUL ( i2, collpair->normal,  impulse );
				VECADDMUL ( i3, collpair->normal,  impulse );

				cloth1->verts[collpair->ap1].impulse_count++;
				cloth1->verts[collpair->ap2].impulse_count++;
				cloth1->verts[collpair->ap3].impulse_count++;

				result = 1;
			}
		}

		if (result) {
			int i = 0;

			for (i = 0; i < 3; i++) {
				if (cloth1->verts[collpair->ap1].impulse_count > 0 && ABS(cloth1->verts[collpair->ap1].impulse[i]) < ABS(i1[i]))
					cloth1->verts[collpair->ap1].impulse[i] = i1[i];

				if (cloth1->verts[collpair->ap2].impulse_count > 0 && ABS(cloth1->verts[collpair->ap2].impulse[i]) < ABS(i2[i]))
					cloth1->verts[collpair->ap2].impulse[i] = i2[i];

				if (cloth1->verts[collpair->ap3].impulse_count > 0 && ABS(cloth1->verts[collpair->ap3].impulse[i]) < ABS(i3[i]))
					cloth1->verts[collpair->ap3].impulse[i] = i3[i];
			}
		}
	}
	return result;
}

//Determines collisions on overlap, collisions are written to collpair[i] and collision+number_collision_found is returned
static CollPair* cloth_collision(ModifierData *md1, ModifierData *md2,
                                 BVHTreeOverlap *overlap, CollPair *collpair, float UNUSED(dt))
{
	ClothModifierData *clmd = (ClothModifierData *)md1;
	CollisionModifierData *collmd = (CollisionModifierData *) md2;
	/* Cloth *cloth = clmd->clothObject; */ /* UNUSED */
	MFace *face1=NULL, *face2 = NULL;
#ifdef USE_BULLET
	ClothVertex *verts1 = clmd->clothObject->verts;
#endif
	double distance = 0;
	float epsilon1 = clmd->coll_parms->epsilon;
	float epsilon2 = BLI_bvhtree_getepsilon ( collmd->bvhtree );
	int i;

	face1 = & ( clmd->clothObject->mfaces[overlap->indexA] );
	face2 = & ( collmd->mfaces[overlap->indexB] );

	// check all 4 possible collisions
	for ( i = 0; i < 4; i++ ) {
		if ( i == 0 ) {
			// fill faceA
			collpair->ap1 = face1->v1;
			collpair->ap2 = face1->v2;
			collpair->ap3 = face1->v3;

			// fill faceB
			collpair->bp1 = face2->v1;
			collpair->bp2 = face2->v2;
			collpair->bp3 = face2->v3;
		}
		else if ( i == 1 ) {
			if ( face1->v4 ) {
				// fill faceA
				collpair->ap1 = face1->v1;
				collpair->ap2 = face1->v3;
				collpair->ap3 = face1->v4;

				// fill faceB
				collpair->bp1 = face2->v1;
				collpair->bp2 = face2->v2;
				collpair->bp3 = face2->v3;
			}
			else
				i++;
		}
		if ( i == 2 ) {
			if ( face2->v4 ) {
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
		else if ( i == 3 ) {
			if ( face1->v4 && face2->v4 ) {
				// fill faceA
				collpair->ap1 = face1->v1;
				collpair->ap2 = face1->v3;
				collpair->ap3 = face1->v4;

				// fill faceB
				collpair->bp1 = face2->v1;
				collpair->bp2 = face2->v3;
				collpair->bp3 = face2->v4;
			}
			else
				break;
		}
		
#ifdef USE_BULLET
		// calc distance + normal
		distance = plNearestPoints (
			verts1[collpair->ap1].txold, verts1[collpair->ap2].txold, verts1[collpair->ap3].txold, collmd->current_x[collpair->bp1].co, collmd->current_x[collpair->bp2].co, collmd->current_x[collpair->bp3].co, collpair->pa, collpair->pb, collpair->vector );
#else
		// just be sure that we don't add anything
		distance = 2.0 * (double)( epsilon1 + epsilon2 + ALMOST_ZERO );
#endif

		if (distance <= (epsilon1 + epsilon2 + ALMOST_ZERO)) {
			normalize_v3_v3(collpair->normal, collpair->vector);

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

			sub_v3_v3v3(relativeVelocity, v2, v1);

			if (sqrt(dot_v3v3(relativeVelocity, relativeVelocity)) >= distance)
			{
				// check for collision in the future
				collpair->flag |= COLLISION_IN_FUTURE;
				collpair++;
			}
		}*/
	}
	return collpair;
}

static void add_collision_object(Object ***objs, unsigned int *numobj, unsigned int *maxobj, Object *ob, Object *self, int level, unsigned int modifier_type)
{
	CollisionModifierData *cmd= NULL;

	if (ob == self)
		return;

	/* only get objects with collision modifier */
	if (((modifier_type == eModifierType_Collision) && ob->pd && ob->pd->deflect) || (modifier_type != eModifierType_Collision))
		cmd= (CollisionModifierData *)modifiers_findByType(ob, modifier_type);
	
	if (cmd) {	
		/* extend array */
		if (*numobj >= *maxobj) {
			*maxobj *= 2;
			*objs= MEM_reallocN(*objs, sizeof(Object*)*(*maxobj));
		}
		
		(*objs)[*numobj] = ob;
		(*numobj)++;
	}

	/* objects in dupli groups, one level only for now */
	if (ob->dup_group && level == 0) {
		GroupObject *go;
		Group *group= ob->dup_group;

		/* add objects */
		for (go= group->gobject.first; go; go= go->next)
			add_collision_object(objs, numobj, maxobj, go->ob, self, level+1, modifier_type);
	}	
}

// return all collision objects in scene
// collision object will exclude self 
Object **get_collisionobjects(Scene *scene, Object *self, Group *group, unsigned int *numcollobj, unsigned int modifier_type)
{
	Base *base;
	Object **objs;
	GroupObject *go;
	unsigned int numobj= 0, maxobj= 100;
	
	objs= MEM_callocN(sizeof(Object *)*maxobj, "CollisionObjectsArray");

	/* gather all collision objects */
	if (group) {
		/* use specified group */
		for (go= group->gobject.first; go; go= go->next)
			add_collision_object(&objs, &numobj, &maxobj, go->ob, self, 0, modifier_type);
	}
	else {
		Scene *sce_iter;
		/* add objects in same layer in scene */
		for (SETLOOPER(scene, sce_iter, base)) {
			if (base->lay & self->lay)
				add_collision_object(&objs, &numobj, &maxobj, base->object, self, 0, modifier_type);

		}
	}

	*numcollobj= numobj;

	return objs;
}

static void add_collider_cache_object(ListBase **objs, Object *ob, Object *self, int level)
{
	CollisionModifierData *cmd= NULL;
	ColliderCache *col;

	if (ob == self)
		return;

	if (ob->pd && ob->pd->deflect)
		cmd =(CollisionModifierData *)modifiers_findByType(ob, eModifierType_Collision);
	
	if (cmd && cmd->bvhtree) {	
		if (*objs == NULL)
			*objs = MEM_callocN(sizeof(ListBase), "ColliderCache array");

		col = MEM_callocN(sizeof(ColliderCache), "ColliderCache");
		col->ob = ob;
		col->collmd = cmd;
		/* make sure collider is properly set up */
		collision_move_object(cmd, 1.0, 0.0);
		BLI_addtail(*objs, col);
	}

	/* objects in dupli groups, one level only for now */
	if (ob->dup_group && level == 0) {
		GroupObject *go;
		Group *group= ob->dup_group;

		/* add objects */
		for (go= group->gobject.first; go; go= go->next)
			add_collider_cache_object(objs, go->ob, self, level+1);
	}
}

ListBase *get_collider_cache(Scene *scene, Object *self, Group *group)
{
	GroupObject *go;
	ListBase *objs= NULL;
	
	/* add object in same layer in scene */
	if (group) {
		for (go= group->gobject.first; go; go= go->next)
			add_collider_cache_object(&objs, go->ob, self, 0);
	}
	else {
		Scene *sce_iter;
		Base *base;

		/* add objects in same layer in scene */
		for (SETLOOPER(scene, sce_iter, base)) {
			if (!self || (base->lay & self->lay))
				add_collider_cache_object(&objs, base->object, self, 0);

		}
	}

	return objs;
}

void free_collider_cache(ListBase **colliders)
{
	if (*colliders) {
		BLI_freelistN(*colliders);
		MEM_freeN(*colliders);
		*colliders = NULL;
	}
}


static void cloth_bvh_objcollisions_nearcheck ( ClothModifierData * clmd, CollisionModifierData *collmd,
	CollPair **collisions, CollPair **collisions_index, int numresult, BVHTreeOverlap *overlap, double dt)
{
	int i;
	
	*collisions = (CollPair *) MEM_mallocN(sizeof(CollPair) * numresult * 64, "collision array" ); //*4 since cloth_collision_static can return more than 1 collision
	*collisions_index = *collisions;

	for ( i = 0; i < numresult; i++ ) {
		*collisions_index = cloth_collision ( (ModifierData *)clmd, (ModifierData *)collmd,
		                                      overlap+i, *collisions_index, dt );
	}
}

static int cloth_bvh_objcollisions_resolve ( ClothModifierData * clmd, CollisionModifierData *collmd, CollPair *collisions, CollPair *collisions_index)
{
	Cloth *cloth = clmd->clothObject;
	int i=0, j = 0, /*numfaces = 0, */ numverts = 0;
	ClothVertex *verts = NULL;
	int ret = 0;
	int result = 0;
	
	numverts = clmd->clothObject->numverts;
	verts = cloth->verts;
	
	// process all collisions (calculate impulses, TODO: also repulses if distance too short)
	result = 1;
	for ( j = 0; j < 2; j++ ) { /* 5 is just a value that ensures convergence */
		result = 0;

		if ( collmd->bvhtree ) {
			result += cloth_collision_response_static ( clmd, collmd, collisions, collisions_index );

			// apply impulses in parallel
			if (result) {
				for (i = 0; i < numverts; i++) {
					// calculate "velocities" (just xnew = xold + v; no dt in v)
					if (verts[i].impulse_count) {
						// VECADDMUL ( verts[i].tv, verts[i].impulse, 1.0f / verts[i].impulse_count );
						VECADD ( verts[i].tv, verts[i].tv, verts[i].impulse);
						zero_v3(verts[i].impulse);
						verts[i].impulse_count = 0;

						ret++;
					}
				}
			}
		}

		if (!result) {
			break;
		}
	}
	return ret;
}

// cloth - object collisions
int cloth_bvh_objcollision(Object *ob, ClothModifierData * clmd, float step, float dt )
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
	
	collobjs = get_collisionobjects(clmd->scene, ob, clmd->coll_parms->group, &numcollobj, eModifierType_Collision);
	
	if (!collobjs)
		return 0;

	/* move object to position (step) in time */
	for (i = 0; i < numcollobj; i++) {
		Object *collob= collobjs[i];
		CollisionModifierData *collmd = (CollisionModifierData*)modifiers_findByType(collob, eModifierType_Collision);

		if (!collmd->bvhtree)
			continue;

		/* move object to position (step) in time */
		collision_move_object ( collmd, step + dt, step );
	}

	do
	{
		CollPair **collisions, **collisions_index;
		
		ret2 = 0;

		collisions = MEM_callocN(sizeof(CollPair *) *numcollobj, "CollPair");
		collisions_index = MEM_callocN(sizeof(CollPair *) *numcollobj, "CollPair");
		
		// check all collision objects
		for (i = 0; i < numcollobj; i++) {
			Object *collob= collobjs[i];
			CollisionModifierData *collmd = (CollisionModifierData*)modifiers_findByType(collob, eModifierType_Collision);
			BVHTreeOverlap *overlap = NULL;
			unsigned int result = 0;
			
			if (!collmd->bvhtree)
				continue;
			
			/* search for overlapping collision pairs */
			overlap = BLI_bvhtree_overlap ( cloth_bvh, collmd->bvhtree, &result );
				
			// go to next object if no overlap is there
			if ( result && overlap ) {
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
		
		for (i = 0; i < numcollobj; i++) {
			if ( collisions[i] ) MEM_freeN ( collisions[i] );
		}
			
		MEM_freeN(collisions);
		MEM_freeN(collisions_index);

		////////////////////////////////////////////////////////////
		// update positions
		// this is needed for bvh_calc_DOP_hull_moving() [kdop.c]
		////////////////////////////////////////////////////////////

		// verts come from clmd
		for ( i = 0; i < numverts; i++ ) {
			if ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL ) {
				if ( verts [i].flags & CLOTH_VERT_FLAG_PINNED ) {
					continue;
				}
			}

			VECADD ( verts[i].tx, verts[i].txold, verts[i].tv );
		}
		////////////////////////////////////////////////////////////
		
		
		////////////////////////////////////////////////////////////
		// Test on *simple* selfcollisions
		////////////////////////////////////////////////////////////
		if ( clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_SELF ) {
			for (l = 0; l < (unsigned int)clmd->coll_parms->self_loop_count; l++) {
				// TODO: add coll quality rounds again
				BVHTreeOverlap *overlap = NULL;
				unsigned int result = 0;
	
				// collisions = 1;
				verts = cloth->verts; // needed for openMP
	
				/* numfaces = cloth->numfaces; */ /* UNUSED */
				numverts = cloth->numverts;
	
				verts = cloth->verts;
	
				if ( cloth->bvhselftree ) {
					// search for overlapping collision pairs
					overlap = BLI_bvhtree_overlap ( cloth->bvhselftree, cloth->bvhselftree, &result );
	
	// #pragma omp parallel for private(k, i, j) schedule(static)
					for ( k = 0; k < result; k++ ) {
						float temp[3];
						float length = 0;
						float mindistance;
	
						i = overlap[k].indexA;
						j = overlap[k].indexB;
	
						mindistance = clmd->coll_parms->selfepsilon* ( cloth->verts[i].avg_spring_len + cloth->verts[j].avg_spring_len );
	
						if ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL ) {
							if ( ( cloth->verts [i].flags & CLOTH_VERT_FLAG_PINNED ) &&
							     ( cloth->verts [j].flags & CLOTH_VERT_FLAG_PINNED ) )
							{
								continue;
							}
						}

						if( ( cloth->verts[i].flags & CLOTH_VERT_FLAG_NOSELFCOLL ) || 
							( cloth->verts[j].flags & CLOTH_VERT_FLAG_NOSELFCOLL ) )
							continue;
	
						sub_v3_v3v3(temp, verts[i].tx, verts[j].tx);
	
						if ( ( ABS ( temp[0] ) > mindistance ) || ( ABS ( temp[1] ) > mindistance ) || ( ABS ( temp[2] ) > mindistance ) ) continue;
	
						// check for adjacent points (i must be smaller j)
						if ( BLI_edgehash_haskey ( cloth->edgehash, MIN2(i, j), MAX2(i, j) ) ) {
							continue;
						}
	
						length = normalize_v3(temp );
	
						if ( length < mindistance ) {
							float correction = mindistance - length;
	
							if ( cloth->verts [i].flags & CLOTH_VERT_FLAG_PINNED ) {
								mul_v3_fl(temp, -correction);
								VECADD ( verts[j].tx, verts[j].tx, temp );
							}
							else if ( cloth->verts [j].flags & CLOTH_VERT_FLAG_PINNED ) {
								mul_v3_fl(temp, correction);
								VECADD ( verts[i].tx, verts[i].tx, temp );
							}
							else {
								mul_v3_fl(temp, correction * -0.5);
								VECADD ( verts[j].tx, verts[j].tx, temp );
	
								sub_v3_v3v3(verts[i].tx, verts[i].tx, temp);
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
			if ( ret2 ) {
				for ( i = 0; i < cloth->numverts; i++ ) {
					if ( ! ( verts [i].flags & CLOTH_VERT_FLAG_PINNED ) ) {
						sub_v3_v3v3(verts[i].tv, verts[i].tx, verts[i].txold);
					}
				}
			}
			////////////////////////////////////////////////////////////
		}
	}
	while ( ret2 && ( clmd->coll_parms->loop_count>rounds ) );
	
	if (collobjs)
		MEM_freeN(collobjs);

	return 1|MIN2 ( ret, 1 );
}
