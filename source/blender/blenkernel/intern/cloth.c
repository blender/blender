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
 * Contributor(s): Daniel Genrich
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/cloth.c
 *  \ingroup bke
 */


#include "MEM_guardedalloc.h"

#include "DNA_cloth_types.h"
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_math.h"
#include "BLI_edgehash.h"
#include "BLI_linklist.h"

#include "BKE_cdderivedmesh.h"
#include "BKE_cloth.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_modifier.h"
#include "BKE_pointcache.h"

// #include "PIL_time.h"  /* timing for debug prints */

/* Our available solvers. */
// 255 is the magic reserved number, so NEVER try to put 255 solvers in here!
// 254 = MAX!
static CM_SOLVER_DEF	solvers [] =
{
	{ "Implicit", CM_IMPLICIT, implicit_init, implicit_solver, implicit_free },
		// { "Implicit C++", CM_IMPLICITCPP, implicitcpp_init, implicitcpp_solver, implicitcpp_free },
};

/* ********** cloth engine ******* */
/* Prototypes for internal functions.
 */
static void cloth_to_object (Object *ob,  ClothModifierData *clmd, float (*vertexCos)[3]);
static void cloth_from_mesh ( ClothModifierData *clmd, DerivedMesh *dm );
static int cloth_from_object(Object *ob, ClothModifierData *clmd, DerivedMesh *dm, float framenr, int first);
static void cloth_update_springs( ClothModifierData *clmd );
static int cloth_build_springs ( ClothModifierData *clmd, DerivedMesh *dm );
static void cloth_apply_vgroup ( ClothModifierData *clmd, DerivedMesh *dm );


/******************************************************************************
 *
 * External interface called by modifier.c clothModifier functions.
 *
 ******************************************************************************/
/**
 * cloth_init - creates a new cloth simulation.
 *
 * 1. create object
 * 2. fill object with standard values or with the GUI settings if given
 */
void cloth_init(ClothModifierData *clmd )
{	
	/* Initialize our new data structure to reasonable values. */
	clmd->sim_parms->gravity[0] = 0.0;
	clmd->sim_parms->gravity[1] = 0.0;
	clmd->sim_parms->gravity[2] = -9.81;
	clmd->sim_parms->structural = 15.0;
	clmd->sim_parms->shear = 15.0;
	clmd->sim_parms->bending = 0.5;
	clmd->sim_parms->Cdis = 5.0; 
	clmd->sim_parms->Cvi = 1.0;
	clmd->sim_parms->mass = 0.3f;
	clmd->sim_parms->stepsPerFrame = 5;
	clmd->sim_parms->flags = 0;
	clmd->sim_parms->solver_type = 0;
	clmd->sim_parms->preroll = 0;
	clmd->sim_parms->maxspringlen = 10;
	clmd->sim_parms->vgroup_mass = 0;
	clmd->sim_parms->vgroup_shrink = 0;
	clmd->sim_parms->shrink_min = 0.0f; /* min amount the fabric will shrink by 0.0 = no shrinking, 1.0 = shrink to nothing*/
	clmd->sim_parms->avg_spring_len = 0.0;
	clmd->sim_parms->presets = 2; /* cotton as start setting */
	clmd->sim_parms->timescale = 1.0f; /* speed factor, describes how fast cloth moves */
	clmd->sim_parms->reset = 0;
	clmd->sim_parms->vel_damping = 1.0f; /* 1.0 = no damping, 0.0 = fully dampened */
	
	clmd->coll_parms->self_friction = 5.0;
	clmd->coll_parms->friction = 5.0;
	clmd->coll_parms->loop_count = 2;
	clmd->coll_parms->epsilon = 0.015f;
	clmd->coll_parms->flags = CLOTH_COLLSETTINGS_FLAG_ENABLED;
	clmd->coll_parms->collision_list = NULL;
	clmd->coll_parms->self_loop_count = 1.0;
	clmd->coll_parms->selfepsilon = 0.75;
	clmd->coll_parms->vgroup_selfcol = 0;

	/* These defaults are copied from softbody.c's
	 * softbody_calc_forces() function.
	 */
	clmd->sim_parms->eff_force_scale = 1000.0;
	clmd->sim_parms->eff_wind_scale = 250.0;

	// also from softbodies
	clmd->sim_parms->maxgoal = 1.0f;
	clmd->sim_parms->mingoal = 0.0f;
	clmd->sim_parms->defgoal = 0.0f;
	clmd->sim_parms->goalspring = 1.0f;
	clmd->sim_parms->goalfrict = 0.0f;
	clmd->sim_parms->velocity_smooth = 0.0f;

	if (!clmd->sim_parms->effector_weights)
		clmd->sim_parms->effector_weights = BKE_add_effector_weights(NULL);

	if (clmd->point_cache)
		clmd->point_cache->step = 1;
}

static BVHTree *bvhselftree_build_from_cloth (ClothModifierData *clmd, float epsilon)
{
	unsigned int i;
	BVHTree *bvhtree;
	Cloth *cloth;
	ClothVertex *verts;
	float co[12];

	if (!clmd)
		return NULL;

	cloth = clmd->clothObject;

	if (!cloth)
		return NULL;
	
	verts = cloth->verts;
	
	// in the moment, return zero if no faces there
	if (!cloth->numverts)
		return NULL;
	
	// create quadtree with k=26
	bvhtree = BLI_bvhtree_new(cloth->numverts, epsilon, 4, 6);
	
	// fill tree
	for (i = 0; i < cloth->numverts; i++, verts++) {
		copy_v3_v3(&co[0*3], verts->xold);
		
		BLI_bvhtree_insert(bvhtree, i, co, 1);
	}
	
	// balance tree
	BLI_bvhtree_balance(bvhtree);
	
	return bvhtree;
}

static BVHTree *bvhtree_build_from_cloth (ClothModifierData *clmd, float epsilon)
{
	unsigned int i;
	BVHTree *bvhtree;
	Cloth *cloth;
	ClothVertex *verts;
	MFace *mfaces;
	float co[12];

	if (!clmd)
		return NULL;

	cloth = clmd->clothObject;

	if (!cloth)
		return NULL;
	
	verts = cloth->verts;
	mfaces = cloth->mfaces;
	
	/* in the moment, return zero if no faces there */
	if (!cloth->numfaces)
		return NULL;

	/* create quadtree with k=26 */
	bvhtree = BLI_bvhtree_new(cloth->numfaces, epsilon, 4, 26);

	/* fill tree */
	for (i = 0; i < cloth->numfaces; i++, mfaces++) {
		copy_v3_v3(&co[0*3], verts[mfaces->v1].xold);
		copy_v3_v3(&co[1*3], verts[mfaces->v2].xold);
		copy_v3_v3(&co[2*3], verts[mfaces->v3].xold);

		if (mfaces->v4)
			copy_v3_v3(&co[3*3], verts[mfaces->v4].xold);

		BLI_bvhtree_insert(bvhtree, i, co, (mfaces->v4 ? 4 : 3));
	}

	/* balance tree */
	BLI_bvhtree_balance(bvhtree);
	
	return bvhtree;
}

void bvhtree_update_from_cloth(ClothModifierData *clmd, int moving)
{	
	unsigned int i = 0;
	Cloth *cloth = clmd->clothObject;
	BVHTree *bvhtree = cloth->bvhtree;
	ClothVertex *verts = cloth->verts;
	MFace *mfaces;
	float co[12], co_moving[12];
	bool ret = false;
	
	if (!bvhtree)
		return;
	
	mfaces = cloth->mfaces;
	
	// update vertex position in bvh tree
	if (verts && mfaces) {
		for (i = 0; i < cloth->numfaces; i++, mfaces++) {
			copy_v3_v3(&co[0*3], verts[mfaces->v1].txold);
			copy_v3_v3(&co[1*3], verts[mfaces->v2].txold);
			copy_v3_v3(&co[2*3], verts[mfaces->v3].txold);
			
			if (mfaces->v4)
				copy_v3_v3(&co[3*3], verts[mfaces->v4].txold);
		
			// copy new locations into array
			if (moving) {
				// update moving positions
				copy_v3_v3(&co_moving[0*3], verts[mfaces->v1].tx);
				copy_v3_v3(&co_moving[1*3], verts[mfaces->v2].tx);
				copy_v3_v3(&co_moving[2*3], verts[mfaces->v3].tx);
				
				if (mfaces->v4)
					copy_v3_v3(&co_moving[3*3], verts[mfaces->v4].tx);
				
				ret = BLI_bvhtree_update_node(bvhtree, i, co, co_moving, (mfaces->v4 ? 4 : 3));
			}
			else {
				ret = BLI_bvhtree_update_node(bvhtree, i, co, NULL, (mfaces->v4 ? 4 : 3));
			}
			
			// check if tree is already full
			if (!ret)
				break;
		}
		
		BLI_bvhtree_update_tree(bvhtree);
	}
}

void bvhselftree_update_from_cloth(ClothModifierData *clmd, int moving)
{	
	unsigned int i = 0;
	Cloth *cloth = clmd->clothObject;
	BVHTree *bvhtree = cloth->bvhselftree;
	ClothVertex *verts = cloth->verts;
	MFace *mfaces;
	float co[12], co_moving[12];
	int ret = 0;
	
	if (!bvhtree)
		return;
	
	mfaces = cloth->mfaces;

	// update vertex position in bvh tree
	if (verts && mfaces) {
		for (i = 0; i < cloth->numverts; i++, verts++) {
			copy_v3_v3(&co[0*3], verts->txold);

			// copy new locations into array
			if (moving) {
				// update moving positions
				copy_v3_v3(&co_moving[0*3], verts->tx);

				ret = BLI_bvhtree_update_node(bvhtree, i, co, co_moving, 1);
			}
			else {
				ret = BLI_bvhtree_update_node(bvhtree, i, co, NULL, 1);
			}

			// check if tree is already full
			if (!ret)
				break;
		}
		
		BLI_bvhtree_update_tree(bvhtree);
	}
}

void cloth_clear_cache(Object *ob, ClothModifierData *clmd, float framenr)
{
	PTCacheID pid;
	
	BKE_ptcache_id_from_cloth(&pid, ob, clmd);

	// don't do anything as long as we're in editmode!
	if (pid.cache->edit && ob->mode & OB_MODE_PARTICLE_EDIT)
		return;
	
	BKE_ptcache_id_clear(&pid, PTCACHE_CLEAR_AFTER, framenr);
}

static int do_init_cloth(Object *ob, ClothModifierData *clmd, DerivedMesh *result, int framenr)
{
	PointCache *cache;

	cache= clmd->point_cache;

	/* initialize simulation data if it didn't exist already */
	if (clmd->clothObject == NULL) {
		if (!cloth_from_object(ob, clmd, result, framenr, 1)) {
			BKE_ptcache_invalidate(cache);
			modifier_setError(&(clmd->modifier), "Can't initialize cloth");
			return 0;
		}
	
		if (clmd->clothObject == NULL) {
			BKE_ptcache_invalidate(cache);
			modifier_setError(&(clmd->modifier), "Null cloth object");
			return 0;
		}
	
		implicit_set_positions(clmd);

		clmd->clothObject->last_frame= MINFRAME-1;
	}

	return 1;
}

static int do_step_cloth(Object *ob, ClothModifierData *clmd, DerivedMesh *result, int framenr)
{
	ClothVertex *verts = NULL;
	Cloth *cloth;
	ListBase *effectors = NULL;
	MVert *mvert;
	unsigned int i = 0;
	int ret = 0;

	/* simulate 1 frame forward */
	cloth = clmd->clothObject;
	verts = cloth->verts;
	mvert = result->getVertArray(result);

	/* force any pinned verts to their constrained location. */
	for (i = 0; i < clmd->clothObject->numverts; i++, verts++) {
		/* save the previous position. */
		copy_v3_v3(verts->xold, verts->xconst);
		copy_v3_v3(verts->txold, verts->x);

		/* Get the current position. */
		copy_v3_v3(verts->xconst, mvert[i].co);
		mul_m4_v3(ob->obmat, verts->xconst);
	}

	effectors = pdInitEffectors(clmd->scene, ob, NULL, clmd->sim_parms->effector_weights, true);

	/* Support for dynamic vertex groups, changing from frame to frame */
	cloth_apply_vgroup ( clmd, result );
	cloth_update_springs( clmd );
	
	// TIMEIT_START(cloth_step)

	/* call the solver. */
	if (solvers [clmd->sim_parms->solver_type].solver)
		ret = solvers[clmd->sim_parms->solver_type].solver(ob, framenr, clmd, effectors);

	// TIMEIT_END(cloth_step)

	pdEndEffectors(&effectors);

	// printf ( "%f\n", ( float ) tval() );
	
	return ret;
}

#if 0
static DerivedMesh *cloth_to_triangles(DerivedMesh *dm)
{
	DerivedMesh *result = NULL;
	unsigned int i = 0, j = 0;
	unsigned int quads = 0, numfaces = dm->getNumTessFaces(dm);
	MFace *mface = dm->getTessFaceArray(dm);
	MFace *mface2 = NULL;

	/* calc faces */
	for (i = 0; i < numfaces; i++) {
		if (mface[i].v4) {
			quads++;
		}
	}
		
	result = CDDM_from_template(dm, dm->getNumVerts(dm), 0, numfaces + quads, 0, 0);

	DM_copy_vert_data(dm, result, 0, 0, dm->getNumVerts(dm));
	DM_copy_tessface_data(dm, result, 0, 0, numfaces);

	DM_ensure_tessface(result);
	mface2 = result->getTessFaceArray(result);

	for (i = 0, j = numfaces; i < numfaces; i++) {
		// DG TODO: is this necessary?
		mface2[i].v1 = mface[i].v1;
		mface2[i].v2 = mface[i].v2;
		mface2[i].v3 = mface[i].v3;

		mface2[i].v4 = 0;
		//test_index_face(&mface2[i], &result->faceData, i, 3);

		if (mface[i].v4) {
			DM_copy_tessface_data(dm, result, i, j, 1);

			mface2[j].v1 = mface[i].v1;
			mface2[j].v2 = mface[i].v3;
			mface2[j].v3 = mface[i].v4;
			mface2[j].v4 = 0;
			//test_index_face(&mface2[j], &result->faceData, j, 3);

			j++;
		}
	}

	CDDM_calc_edges_tessface(result);
	CDDM_tessfaces_to_faces(result); /* builds ngon faces from tess (mface) faces */

	return result;
}
#endif

/************************************************
 * clothModifier_do - main simulation function
 ************************************************/
void clothModifier_do(ClothModifierData *clmd, Scene *scene, Object *ob, DerivedMesh *dm, float (*vertexCos)[3])
{
	PointCache *cache;
	PTCacheID pid;
	float timescale;
	int framenr, startframe, endframe;
	int cache_result;

	clmd->scene= scene;	/* nice to pass on later :) */
	framenr= (int)scene->r.cfra;
	cache= clmd->point_cache;

	BKE_ptcache_id_from_cloth(&pid, ob, clmd);
	BKE_ptcache_id_time(&pid, scene, framenr, &startframe, &endframe, &timescale);
	clmd->sim_parms->timescale= timescale;

	if (clmd->sim_parms->reset ||
	    (framenr == (startframe - clmd->sim_parms->preroll) && clmd->sim_parms->preroll != 0) ||
	    (clmd->clothObject && dm->getNumVerts(dm) != clmd->clothObject->numverts))
	{
		clmd->sim_parms->reset = 0;
		cache->flag |= PTCACHE_OUTDATED;
		BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
		BKE_ptcache_validate(cache, 0);
		cache->last_exact= 0;
		cache->flag &= ~PTCACHE_REDO_NEEDED;
	}
	
	// unused in the moment, calculated separately in implicit.c
	clmd->sim_parms->dt = clmd->sim_parms->timescale / clmd->sim_parms->stepsPerFrame;

	/* handle continuous simulation with the play button */
	if ((clmd->sim_parms->preroll > 0) && (framenr > startframe - clmd->sim_parms->preroll) && (framenr < startframe)) {
		BKE_ptcache_invalidate(cache);

		/* do simulation */
		if (!do_init_cloth(ob, clmd, dm, framenr))
			return;

		do_step_cloth(ob, clmd, dm, framenr);
		cloth_to_object(ob, clmd, vertexCos);

		clmd->clothObject->last_frame= framenr;

		return;
	}

	/* simulation is only active during a specific period */
	if (framenr < startframe) {
		BKE_ptcache_invalidate(cache);
		return;
	}
	else if (framenr > endframe) {
		framenr= endframe;
	}

	/* initialize simulation data if it didn't exist already */
	if (!do_init_cloth(ob, clmd, dm, framenr))
		return;

	if ((framenr == startframe) && (clmd->sim_parms->preroll == 0)) {
		BKE_ptcache_id_reset(scene, &pid, PTCACHE_RESET_OUTDATED);
		do_init_cloth(ob, clmd, dm, framenr);
		BKE_ptcache_validate(cache, framenr);
		cache->flag &= ~PTCACHE_REDO_NEEDED;
		clmd->clothObject->last_frame= framenr;
		return;
	}

	/* try to read from cache */
	cache_result = BKE_ptcache_read(&pid, (float)framenr+scene->r.subframe);

	if (cache_result == PTCACHE_READ_EXACT || cache_result == PTCACHE_READ_INTERPOLATED) {
		implicit_set_positions(clmd);
		cloth_to_object (ob, clmd, vertexCos);

		BKE_ptcache_validate(cache, framenr);

		if (cache_result == PTCACHE_READ_INTERPOLATED && cache->flag & PTCACHE_REDO_NEEDED)
			BKE_ptcache_write(&pid, framenr);

		clmd->clothObject->last_frame= framenr;

		return;
	}
	else if (cache_result==PTCACHE_READ_OLD) {
		implicit_set_positions(clmd);
	}
	else if ( /*ob->id.lib ||*/ (cache->flag & PTCACHE_BAKED)) { /* 2.4x disabled lib, but this can be used in some cases, testing further - campbell */
		/* if baked and nothing in cache, do nothing */
		BKE_ptcache_invalidate(cache);
		return;
	}

	if (framenr!=clmd->clothObject->last_frame+1)
		return;

	/* if on second frame, write cache for first frame */
	if (cache->simframe == startframe && (cache->flag & PTCACHE_OUTDATED || cache->last_exact==0))
		BKE_ptcache_write(&pid, startframe);

	clmd->sim_parms->timescale *= framenr - cache->simframe;

	/* do simulation */
	BKE_ptcache_validate(cache, framenr);

	if (!do_step_cloth(ob, clmd, dm, framenr)) {
		BKE_ptcache_invalidate(cache);
	}
	else
		BKE_ptcache_write(&pid, framenr);

	cloth_to_object (ob, clmd, vertexCos);
	clmd->clothObject->last_frame= framenr;
}

/* frees all */
void cloth_free_modifier(ClothModifierData *clmd )
{
	Cloth	*cloth = NULL;
	
	if ( !clmd )
		return;

	cloth = clmd->clothObject;

	
	if ( cloth ) {
		// If our solver provides a free function, call it
		if ( solvers [clmd->sim_parms->solver_type].free ) {
			solvers [clmd->sim_parms->solver_type].free ( clmd );
		}

		// Free the verts.
		if ( cloth->verts != NULL )
			MEM_freeN ( cloth->verts );

		cloth->verts = NULL;
		cloth->numverts = 0;

		// Free the springs.
		if ( cloth->springs != NULL ) {
			LinkNode *search = cloth->springs;
			while (search) {
				ClothSpring *spring = search->link;
						
				MEM_freeN ( spring );
				search = search->next;
			}
			BLI_linklist_free(cloth->springs, NULL);
		
			cloth->springs = NULL;
		}

		cloth->springs = NULL;
		cloth->numsprings = 0;

		// free BVH collision tree
		if ( cloth->bvhtree )
			BLI_bvhtree_free ( cloth->bvhtree );
		
		if ( cloth->bvhselftree )
			BLI_bvhtree_free ( cloth->bvhselftree );

		// we save our faces for collision objects
		if ( cloth->mfaces )
			MEM_freeN ( cloth->mfaces );
		
		if (cloth->edgeset)
			BLI_edgeset_free(cloth->edgeset);
		
		
		/*
		if (clmd->clothObject->facemarks)
		MEM_freeN(clmd->clothObject->facemarks);
		*/
		MEM_freeN ( cloth );
		clmd->clothObject = NULL;
	}
}

/* frees all */
void cloth_free_modifier_extern(ClothModifierData *clmd )
{
	Cloth	*cloth = NULL;
	if (G.debug_value > 0)
		printf("cloth_free_modifier_extern\n");
	
	if ( !clmd )
		return;

	cloth = clmd->clothObject;
	
	if ( cloth ) {
		if (G.debug_value > 0)
			printf("cloth_free_modifier_extern in\n");

		// If our solver provides a free function, call it
		if ( solvers [clmd->sim_parms->solver_type].free ) {
			solvers [clmd->sim_parms->solver_type].free ( clmd );
		}

		// Free the verts.
		if ( cloth->verts != NULL )
			MEM_freeN ( cloth->verts );

		cloth->verts = NULL;
		cloth->numverts = 0;

		// Free the springs.
		if ( cloth->springs != NULL ) {
			LinkNode *search = cloth->springs;
			while (search) {
				ClothSpring *spring = search->link;

				MEM_freeN ( spring );
				search = search->next;
			}
			BLI_linklist_free(cloth->springs, NULL);

			cloth->springs = NULL;
		}

		cloth->springs = NULL;
		cloth->numsprings = 0;

		// free BVH collision tree
		if ( cloth->bvhtree )
			BLI_bvhtree_free ( cloth->bvhtree );
		
		if ( cloth->bvhselftree )
			BLI_bvhtree_free ( cloth->bvhselftree );

		// we save our faces for collision objects
		if ( cloth->mfaces )
			MEM_freeN ( cloth->mfaces );

		if (cloth->edgeset)
			BLI_edgeset_free(cloth->edgeset);


		/*
		if (clmd->clothObject->facemarks)
		MEM_freeN(clmd->clothObject->facemarks);
		*/
		MEM_freeN ( cloth );
		clmd->clothObject = NULL;
	}
}

/******************************************************************************
 *
 * Internal functions.
 *
 ******************************************************************************/

/**
 * cloth_to_object - copies the deformed vertices to the object.
 *
 **/
static void cloth_to_object (Object *ob,  ClothModifierData *clmd, float (*vertexCos)[3])
{
	unsigned int	i = 0;
	Cloth *cloth = clmd->clothObject;

	if (clmd->clothObject) {
		/* inverse matrix is not uptodate... */
		invert_m4_m4(ob->imat, ob->obmat);

		for (i = 0; i < cloth->numverts; i++) {
			copy_v3_v3 (vertexCos[i], cloth->verts[i].x);
			mul_m4_v3(ob->imat, vertexCos[i]);	/* cloth is in global coords */
		}
	}
}


int cloth_uses_vgroup(ClothModifierData *clmd)
{
	return (((clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_SCALING ) || 
		(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL ) ||
		(clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_SELF)) && 
		((clmd->sim_parms->vgroup_mass>0) || 
		(clmd->sim_parms->vgroup_struct>0)||
		(clmd->sim_parms->vgroup_bend>0)  ||
		(clmd->sim_parms->vgroup_shrink>0) ||
		(clmd->coll_parms->vgroup_selfcol>0)));
}

/**
 * cloth_apply_vgroup - applies a vertex group as specified by type
 *
 **/
/* can be optimized to do all groups in one loop */
static void cloth_apply_vgroup ( ClothModifierData *clmd, DerivedMesh *dm )
{
	int i = 0;
	int j = 0;
	MDeformVert *dvert = NULL;
	Cloth *clothObj = NULL;
	int numverts;
	/* float goalfac = 0; */ /* UNUSED */
	ClothVertex *verts = NULL;

	if (!clmd || !dm) return;

	clothObj = clmd->clothObject;

	numverts = dm->getNumVerts (dm);

	verts = clothObj->verts;
	
	if (cloth_uses_vgroup(clmd)) {
		for ( i = 0; i < numverts; i++, verts++ ) {

			/* Reset Goal values to standard */
			if ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL )
				verts->goal= clmd->sim_parms->defgoal;
			else
				verts->goal= 0.0f;

			/* Reset vertex flags */
			verts->flags &= ~CLOTH_VERT_FLAG_PINNED;
			verts->flags &= ~CLOTH_VERT_FLAG_NOSELFCOLL;

			dvert = dm->getVertData ( dm, i, CD_MDEFORMVERT );
			if ( dvert ) {
				for ( j = 0; j < dvert->totweight; j++ ) {
					if (( dvert->dw[j].def_nr == (clmd->sim_parms->vgroup_mass-1)) && (clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL )) {
						verts->goal = dvert->dw [j].weight;

						/* goalfac= 1.0f; */ /* UNUSED */
						
						// Kicking goal factor to simplify things...who uses that anyway?
						// ABS ( clmd->sim_parms->maxgoal - clmd->sim_parms->mingoal );
						
						verts->goal  = pow4f(verts->goal);
						if ( verts->goal >= SOFTGOALSNAP )
							verts->flags |= CLOTH_VERT_FLAG_PINNED;
					}
					
					if (clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_SCALING ) {
						if ( dvert->dw[j].def_nr == (clmd->sim_parms->vgroup_struct-1)) {
							verts->struct_stiff = dvert->dw [j].weight;
							verts->shear_stiff = dvert->dw [j].weight;
						}
						
						if ( dvert->dw[j].def_nr == (clmd->sim_parms->vgroup_bend-1)) {
							verts->bend_stiff = dvert->dw [j].weight;
						}
					}

					if (clmd->coll_parms->flags & CLOTH_COLLSETTINGS_FLAG_SELF ) {
						if ( dvert->dw[j].def_nr == (clmd->coll_parms->vgroup_selfcol-1)) {
							if (dvert->dw [j].weight > 0.0f) {
								verts->flags |= CLOTH_VERT_FLAG_NOSELFCOLL;
							}
						}

					if (clmd->sim_parms->vgroup_shrink > 0 )
					{
						if ( dvert->dw[j].def_nr == (clmd->sim_parms->vgroup_shrink-1))
						{
							verts->shrink_factor = clmd->sim_parms->shrink_min*(1.0f-dvert->dw[j].weight)+clmd->sim_parms->shrink_max*dvert->dw [j].weight; // linear interpolation between min and max shrink factor based on weight
						}
					}
					else {
						verts->shrink_factor = clmd->sim_parms->shrink_min;
					}

					}
				}
			}
		}
	}
}


static int cloth_from_object(Object *ob, ClothModifierData *clmd, DerivedMesh *dm, float UNUSED(framenr), int first)
{
	int i = 0;
	MVert *mvert = NULL;
	ClothVertex *verts = NULL;
	float (*shapekey_rest)[3] = NULL;
	float tnull[3] = {0, 0, 0};
	Cloth *cloth = NULL;
	float maxdist = 0;

	// If we have a clothObject, free it. 
	if ( clmd->clothObject != NULL ) {
		cloth_free_modifier ( clmd );
		if (G.debug_value > 0)
			printf("cloth_free_modifier cloth_from_object\n");
	}

	// Allocate a new cloth object.
	clmd->clothObject = MEM_callocN ( sizeof ( Cloth ), "cloth" );
	if ( clmd->clothObject ) {
		clmd->clothObject->old_solver_type = 255;
		// clmd->clothObject->old_collision_type = 255;
		cloth = clmd->clothObject;
		clmd->clothObject->edgeset = NULL;
	}
	else if (!clmd->clothObject) {
		modifier_setError(&(clmd->modifier), "Out of memory on allocating clmd->clothObject");
		return 0;
	}

	// mesh input objects need DerivedMesh
	if ( !dm )
		return 0;

	cloth_from_mesh ( clmd, dm );

	// create springs
	clmd->clothObject->springs = NULL;
	clmd->clothObject->numsprings = -1;

	if ( clmd->sim_parms->shapekey_rest )
		shapekey_rest = dm->getVertDataArray ( dm, CD_CLOTH_ORCO );

	mvert = dm->getVertArray (dm);

	verts = clmd->clothObject->verts;

	// set initial values
	for ( i = 0; i < dm->getNumVerts(dm); i++, verts++ ) {
		if (first) {
			copy_v3_v3(verts->x, mvert[i].co);

			mul_m4_v3(ob->obmat, verts->x);

			if ( shapekey_rest ) {
				verts->xrest= shapekey_rest[i];
				mul_m4_v3(ob->obmat, verts->xrest);
			}
			else
				verts->xrest = verts->x;
		}
		
		/* no GUI interface yet */
		verts->mass = clmd->sim_parms->mass; 
		verts->impulse_count = 0;

		if ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL )
			verts->goal= clmd->sim_parms->defgoal;
		else
			verts->goal= 0.0f;

		verts->flags = 0;
		copy_v3_v3 ( verts->xold, verts->x );
		copy_v3_v3 ( verts->xconst, verts->x );
		copy_v3_v3 ( verts->txold, verts->x );
		copy_v3_v3 ( verts->tx, verts->x );
		mul_v3_fl(verts->v, 0.0f);

		verts->impulse_count = 0;
		copy_v3_v3 ( verts->impulse, tnull );
	}
	
	// apply / set vertex groups
	// has to be happen before springs are build!
	cloth_apply_vgroup (clmd, dm);

	if ( !cloth_build_springs ( clmd, dm ) ) {
		cloth_free_modifier ( clmd );
		modifier_setError(&(clmd->modifier), "Cannot build springs");
		printf("cloth_free_modifier cloth_build_springs\n");
		return 0;
	}
	
	for ( i = 0; i < dm->getNumVerts(dm); i++) {
		if ((!(cloth->verts[i].flags & CLOTH_VERT_FLAG_PINNED)) && (cloth->verts[i].goal > ALMOST_ZERO)) {
			cloth_add_spring (clmd, i, i, 0.0, CLOTH_SPRING_TYPE_GOAL);
		}
	}
	
	// init our solver
	if ( solvers [clmd->sim_parms->solver_type].init ) {
		solvers [clmd->sim_parms->solver_type].init ( ob, clmd );
	}
	
	if (!first)
		implicit_set_positions(clmd);

	clmd->clothObject->bvhtree = bvhtree_build_from_cloth ( clmd, MAX2(clmd->coll_parms->epsilon, clmd->coll_parms->distance_repel) );
	
	for (i = 0; i < dm->getNumVerts(dm); i++) {
		maxdist = MAX2(maxdist, clmd->coll_parms->selfepsilon* ( cloth->verts[i].avg_spring_len*2.0f));
	}
	
	clmd->clothObject->bvhselftree = bvhselftree_build_from_cloth ( clmd, maxdist );

	return 1;
}

static void cloth_from_mesh ( ClothModifierData *clmd, DerivedMesh *dm )
{
	unsigned int numverts = dm->getNumVerts (dm);
	unsigned int numfaces = dm->getNumTessFaces (dm);
	MFace *mface = dm->getTessFaceArray(dm);
	unsigned int i = 0;

	/* Allocate our vertices. */
	clmd->clothObject->numverts = numverts;
	clmd->clothObject->verts = MEM_callocN ( sizeof ( ClothVertex ) * clmd->clothObject->numverts, "clothVertex" );
	if ( clmd->clothObject->verts == NULL ) {
		cloth_free_modifier ( clmd );
		modifier_setError(&(clmd->modifier), "Out of memory on allocating clmd->clothObject->verts");
		printf("cloth_free_modifier clmd->clothObject->verts\n");
		return;
	}

	// save face information
	clmd->clothObject->numfaces = numfaces;
	clmd->clothObject->mfaces = MEM_callocN ( sizeof ( MFace ) * clmd->clothObject->numfaces, "clothMFaces" );
	if ( clmd->clothObject->mfaces == NULL ) {
		cloth_free_modifier ( clmd );
		modifier_setError(&(clmd->modifier), "Out of memory on allocating clmd->clothObject->mfaces");
		printf("cloth_free_modifier clmd->clothObject->mfaces\n");
		return;
	}
	for ( i = 0; i < numfaces; i++ )
		memcpy ( &clmd->clothObject->mfaces[i], &mface[i], sizeof ( MFace ) );

	/* Free the springs since they can't be correct if the vertices
	 * changed.
	 */
	if ( clmd->clothObject->springs != NULL )
		MEM_freeN ( clmd->clothObject->springs );

}

/***************************************************************************************
 * SPRING NETWORK BUILDING IMPLEMENTATION BEGIN
 ***************************************************************************************/

BLI_INLINE void spring_verts_ordered_set(ClothSpring *spring, int v0, int v1)
{
	if (v0 < v1) {
		spring->ij = v0;
		spring->kl = v1;
	}
	else {
		spring->ij = v1;
		spring->kl = v0;
	}
}

// be careful: implicit solver has to be resettet when using this one!
// --> only for implicit handling of this spring!
int cloth_add_spring(ClothModifierData *clmd, unsigned int indexA, unsigned int indexB, float restlength, int spring_type)
{
	Cloth *cloth = clmd->clothObject;
	ClothSpring *spring = NULL;
	
	if (cloth) {
		// TODO: look if this spring is already there
		
		spring = (ClothSpring *)MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );
		
		if (!spring)
			return 0;
		
		spring->ij = indexA;
		spring->kl = indexB;
		spring->restlen =  restlength;
		spring->type = spring_type;
		spring->flags = 0;
		spring->stiffness = 0;
		
		cloth->numsprings++;
	
		BLI_linklist_prepend ( &cloth->springs, spring );
		
		return 1;
	}
	return 0;
}

static void cloth_free_edgelist(LinkNode **edgelist, unsigned int numverts)
{
	if (edgelist) {
		unsigned int i;
		for (i = 0; i < numverts; i++) {
			BLI_linklist_free(edgelist[i], NULL);
		}

		MEM_freeN(edgelist);
	}
}

static void cloth_free_errorsprings(Cloth *cloth,  LinkNode **edgelist)
{
	if ( cloth->springs != NULL ) {
		LinkNode *search = cloth->springs;
		while (search) {
			ClothSpring *spring = search->link;
						
			MEM_freeN ( spring );
			search = search->next;
		}
		BLI_linklist_free(cloth->springs, NULL);
		
		cloth->springs = NULL;
	}

	cloth_free_edgelist(edgelist, cloth->numverts);
	
	if (cloth->edgeset) {
		BLI_edgeset_free(cloth->edgeset);
		cloth->edgeset = NULL;
	}
}

/* update stiffness if vertex group values are changing from frame to frame */
static void cloth_update_springs( ClothModifierData *clmd )
{
	Cloth *cloth = clmd->clothObject;
	LinkNode *search = NULL;

	search = cloth->springs;
	while (search) {
		ClothSpring *spring = search->link;

		spring->stiffness = 0.0f;

		if (spring->type == CLOTH_SPRING_TYPE_STRUCTURAL) {
			spring->stiffness = (cloth->verts[spring->kl].struct_stiff + cloth->verts[spring->ij].struct_stiff) / 2.0f;
		}
		else if (spring->type == CLOTH_SPRING_TYPE_SHEAR) {
			spring->stiffness = (cloth->verts[spring->kl].shear_stiff + cloth->verts[spring->ij].shear_stiff) / 2.0f;
		}
		else if (spring->type == CLOTH_SPRING_TYPE_BENDING) {
			spring->stiffness = (cloth->verts[spring->kl].bend_stiff + cloth->verts[spring->ij].bend_stiff) / 2.0f;
		}
		else if (spring->type == CLOTH_SPRING_TYPE_GOAL) {
			/* Warning: Appending NEW goal springs does not work because implicit solver would need reset! */

			/* Activate / Deactivate existing springs */
			if ((!(cloth->verts[spring->ij].flags & CLOTH_VERT_FLAG_PINNED)) &&
			    (cloth->verts[spring->ij].goal > ALMOST_ZERO))
			{
				spring->flags &= ~CLOTH_SPRING_FLAG_DEACTIVATE;
			}
			else {
				spring->flags |= CLOTH_SPRING_FLAG_DEACTIVATE;
			}
		}
		
		search = search->next;
	}


}

static int cloth_build_springs ( ClothModifierData *clmd, DerivedMesh *dm )
{
	Cloth *cloth = clmd->clothObject;
	ClothSpring *spring = NULL, *tspring = NULL, *tspring2 = NULL;
	unsigned int struct_springs = 0, shear_springs=0, bend_springs = 0;
	unsigned int i = 0;
	unsigned int numverts = (unsigned int)dm->getNumVerts (dm);
	unsigned int numedges = (unsigned int)dm->getNumEdges (dm);
	unsigned int numfaces = (unsigned int)dm->getNumTessFaces (dm);
	float shrink_factor;
	MEdge *medge = dm->getEdgeArray (dm);
	MFace *mface = dm->getTessFaceArray (dm);
	int index2 = 0; // our second vertex index
	LinkNode **edgelist = NULL;
	EdgeSet *edgeset = NULL;
	LinkNode *search = NULL, *search2 = NULL;
	
	// error handling
	if ( numedges==0 )
		return 0;

	/* NOTE: handling ownership of springs and edgeset is quite sloppy
	 * currently they are never initialized but assert just to be sure */
	BLI_assert(cloth->springs == NULL);
	BLI_assert(cloth->edgeset == NULL);

	cloth->springs = NULL;
	cloth->edgeset = NULL;

	edgelist = MEM_callocN ( sizeof (LinkNode *) * numverts, "cloth_edgelist_alloc" );
	
	if (!edgelist)
		return 0;

	// structural springs
	for ( i = 0; i < numedges; i++ ) {
		spring = (ClothSpring *)MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );

		if ( spring ) {
			spring_verts_ordered_set(spring, medge[i].v1, medge[i].v2);
			if (clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_SEW && medge[i].flag & ME_LOOSEEDGE) {
				// handle sewing (loose edges will be pulled together)
				spring->restlen = 0.0f;
				spring->stiffness = 1.0f;
				spring->type = CLOTH_SPRING_TYPE_SEWING;
			}
			else {
				if (clmd->sim_parms->vgroup_shrink > 0)
					shrink_factor = 1.0f - ((cloth->verts[spring->ij].shrink_factor + cloth->verts[spring->kl].shrink_factor) / 2.0f);
				else
					shrink_factor = 1.0f - clmd->sim_parms->shrink_min;
				spring->restlen = len_v3v3(cloth->verts[spring->kl].xrest, cloth->verts[spring->ij].xrest) * shrink_factor;
				spring->stiffness = (cloth->verts[spring->kl].struct_stiff + cloth->verts[spring->ij].struct_stiff) / 2.0f;
				spring->type = CLOTH_SPRING_TYPE_STRUCTURAL;
			}
			clmd->sim_parms->avg_spring_len += spring->restlen;
			cloth->verts[spring->ij].avg_spring_len += spring->restlen;
			cloth->verts[spring->kl].avg_spring_len += spring->restlen;
			cloth->verts[spring->ij].spring_count++;
			cloth->verts[spring->kl].spring_count++;
			spring->flags = 0;
			struct_springs++;
			
			BLI_linklist_prepend ( &cloth->springs, spring );
		}
		else {
			cloth_free_errorsprings(cloth, edgelist);
			return 0;
		}
	}

	if (struct_springs > 0)
		clmd->sim_parms->avg_spring_len /= struct_springs;
	
	for (i = 0; i < numverts; i++) {
		cloth->verts[i].avg_spring_len = cloth->verts[i].avg_spring_len * 0.49f / ((float)cloth->verts[i].spring_count);
	}

	// shear springs
	for ( i = 0; i < numfaces; i++ ) {
		// triangle faces already have shear springs due to structural geometry
		if ( !mface[i].v4 )
			continue;

		spring = (ClothSpring *)MEM_callocN(sizeof(ClothSpring), "cloth spring");
		
		if (!spring) {
			cloth_free_errorsprings(cloth, edgelist);
			return 0;
		}

		spring_verts_ordered_set(spring, mface[i].v1, mface[i].v3);
		if (clmd->sim_parms->vgroup_shrink > 0)
			shrink_factor = 1.0f - ((cloth->verts[spring->ij].shrink_factor + cloth->verts[spring->kl].shrink_factor) / 2.0f);
		else
			shrink_factor = 1.0f - clmd->sim_parms->shrink_min;
		spring->restlen = len_v3v3(cloth->verts[spring->kl].xrest, cloth->verts[spring->ij].xrest) * shrink_factor;
		spring->type = CLOTH_SPRING_TYPE_SHEAR;
		spring->stiffness = (cloth->verts[spring->kl].shear_stiff + cloth->verts[spring->ij].shear_stiff) / 2.0f;

		BLI_linklist_append ( &edgelist[spring->ij], spring );
		BLI_linklist_append ( &edgelist[spring->kl], spring );
		shear_springs++;

		BLI_linklist_prepend ( &cloth->springs, spring );

		
		// if ( mface[i].v4 ) --> Quad face
		spring = (ClothSpring *)MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );
		
		if (!spring) {
			cloth_free_errorsprings(cloth, edgelist);
			return 0;
		}

		spring_verts_ordered_set(spring, mface[i].v2, mface[i].v4);
		if (clmd->sim_parms->vgroup_shrink > 0)
			shrink_factor = 1.0f - ((cloth->verts[spring->ij].shrink_factor + cloth->verts[spring->kl].shrink_factor) / 2.0f);
		else
			shrink_factor = 1.0f - clmd->sim_parms->shrink_min;
		spring->restlen = len_v3v3(cloth->verts[spring->kl].xrest, cloth->verts[spring->ij].xrest) * shrink_factor;
		spring->type = CLOTH_SPRING_TYPE_SHEAR;
		spring->stiffness = (cloth->verts[spring->kl].shear_stiff + cloth->verts[spring->ij].shear_stiff) / 2.0f;

		BLI_linklist_append ( &edgelist[spring->ij], spring );
		BLI_linklist_append ( &edgelist[spring->kl], spring );
		shear_springs++;

		BLI_linklist_prepend ( &cloth->springs, spring );
	}

	edgeset = BLI_edgeset_new_ex(__func__, numedges);
	cloth->edgeset = edgeset;

	if (numfaces) {
		// bending springs
		search2 = cloth->springs;
		for ( i = struct_springs; i < struct_springs+shear_springs; i++ ) {
			if ( !search2 )
				break;

			tspring2 = search2->link;
			search = edgelist[tspring2->kl];
			while ( search ) {
				tspring = search->link;
				index2 = ( ( tspring->ij==tspring2->kl ) ? ( tspring->kl ) : ( tspring->ij ) );

				// check for existing spring
				// check also if startpoint is equal to endpoint
				if ((index2 != tspring2->ij) &&
				    !BLI_edgeset_haskey(edgeset, tspring2->ij, index2))
				{
					spring = (ClothSpring *)MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );

					if (!spring) {
						cloth_free_errorsprings(cloth, edgelist);
						return 0;
					}

					spring_verts_ordered_set(spring, tspring2->ij, index2);
					spring->restlen = len_v3v3(cloth->verts[spring->kl].xrest, cloth->verts[spring->ij].xrest);
					spring->type = CLOTH_SPRING_TYPE_BENDING;
					spring->stiffness = (cloth->verts[spring->kl].bend_stiff + cloth->verts[spring->ij].bend_stiff) / 2.0f;
					BLI_edgeset_insert(edgeset, spring->ij, spring->kl);
					bend_springs++;

					BLI_linklist_prepend ( &cloth->springs, spring );
				}
				search = search->next;
			}
			search2 = search2->next;
		}
	}
	else if (struct_springs > 2) {
		/* bending springs for hair strands */
		/* The current algorightm only goes through the edges in order of the mesh edges list	*/
		/* and makes springs between the outer vert of edges sharing a vertice. This works just */
		/* fine for hair, but not for user generated string meshes. This could/should be later	*/
		/* extended to work with non-ordered edges so that it can be used for general "rope		*/
		/* dynamics" without the need for the vertices or edges to be ordered through the length*/
		/* of the strands. -jahka */
		search = cloth->springs;
		search2 = search->next;
		while (search && search2) {
			tspring = search->link;
			tspring2 = search2->link;

			if (tspring->ij == tspring2->kl) {
				spring = (ClothSpring *)MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );
				
				if (!spring) {
					cloth_free_errorsprings(cloth, edgelist);
					return 0;
				}

				spring->ij = tspring2->ij;
				spring->kl = tspring->kl;
				spring->restlen = len_v3v3(cloth->verts[spring->kl].xrest, cloth->verts[spring->ij].xrest);
				spring->type = CLOTH_SPRING_TYPE_BENDING;
				spring->stiffness = (cloth->verts[spring->kl].bend_stiff + cloth->verts[spring->ij].bend_stiff) / 2.0f;
				bend_springs++;

				BLI_linklist_prepend ( &cloth->springs, spring );
			}
			
			search = search->next;
			search2 = search2->next;
		}
	}
	
	/* note: the edges may already exist so run reinsert */

	/* insert other near springs in edgeset AFTER bending springs are calculated (for selfcolls) */
	for (i = 0; i < numedges; i++) { /* struct springs */
		BLI_edgeset_add(edgeset, medge[i].v1, medge[i].v2);
	}

	for (i = 0; i < numfaces; i++) { /* edge springs */
		if (mface[i].v4) {
			BLI_edgeset_add(edgeset, mface[i].v1, mface[i].v3);
			
			BLI_edgeset_add(edgeset, mface[i].v2, mface[i].v4);
		}
	}
	
	
	cloth->numsprings = struct_springs + shear_springs + bend_springs;
	
	cloth_free_edgelist(edgelist, numverts);

#if 0
	if (G.debug_value > 0)
		printf("avg_len: %f\n", clmd->sim_parms->avg_spring_len);
#endif

	return 1;

} /* cloth_build_springs */
/***************************************************************************************
 * SPRING NETWORK BUILDING IMPLEMENTATION END
 ***************************************************************************************/

