/*  cloth.c
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

#include "BKE_curve.h"
#include "BKE_deform.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_effect.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_cloth.h"
#include "BKE_modifier.h"
#include "BKE_utildefines.h"
#include "BKE_DerivedMesh.h"
#include "BIF_editdeform.h"
#include "BIF_editkey.h"
#include "DNA_screen_types.h"
#include "BSE_headerbuttons.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "mydevice.h"

#include "BKE_pointcache.h"

#ifdef _WIN32
void tstart ( void )
{}
void tend ( void )
{
}
double tval()
{
	return 0;
}
#else
#include <sys/time.h>
static struct timeval _tstart, _tend;
static struct timezone tz;
void tstart ( void )
{
	gettimeofday ( &_tstart, &tz );
}
void tend ( void )
{
	gettimeofday ( &_tend,&tz );
}
double tval()
{
	double t1, t2;
	t1 = ( double ) _tstart.tv_sec + ( double ) _tstart.tv_usec/ ( 1000*1000 );
	t2 = ( double ) _tend.tv_sec + ( double ) _tend.tv_usec/ ( 1000*1000 );
	return t2-t1;
}
#endif

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
static void cloth_to_object ( Object *ob, ClothModifierData *clmd, float ( *vertexCos ) [3], unsigned int numverts );
static void cloth_from_mesh ( Object *ob, ClothModifierData *clmd, DerivedMesh *dm );
static int cloth_from_object ( Object *ob, ClothModifierData *clmd, DerivedMesh *dm, float ( *vertexCos ) [3], unsigned int numverts, float framenr );
static int collobj_from_object ( Object *ob, ClothModifierData *clmd, DerivedMesh *dm, float ( *vertexCos ) [3], unsigned int numverts );
int cloth_build_springs ( Cloth *cloth, DerivedMesh *dm );
static void cloth_apply_vgroup ( ClothModifierData *clmd, DerivedMesh *dm, short vgroup );


/******************************************************************************
*
* External interface called by modifier.c clothModifier functions.
*
******************************************************************************/
/**
* cloth_init -  creates a new cloth simulation.
*
* 1. create object
* 2. fill object with standard values or with the GUI settings if given
*/
void cloth_init ( ClothModifierData *clmd )
{	
	/* Initialize our new data structure to reasonable values. */
	clmd->sim_parms->gravity [0] = 0.0;
	clmd->sim_parms->gravity [1] = 0.0;
	clmd->sim_parms->gravity [2] = -9.81;
	clmd->sim_parms->structural = 100.0;
	clmd->sim_parms->shear = 100.0;
	clmd->sim_parms->bending = 1.0;
	clmd->sim_parms->Cdis = 5.0;
	clmd->sim_parms->Cvi = 1.0;
	clmd->sim_parms->mass = 1.0f;
	clmd->sim_parms->stepsPerFrame = 5;
	clmd->sim_parms->sim_time = 1.0;
	clmd->sim_parms->flags = CLOTH_SIMSETTINGS_FLAG_RESET;
	clmd->sim_parms->solver_type = 0;
	clmd->sim_parms->preroll = 0;
	clmd->sim_parms->maxspringlen = 10;
	clmd->sim_parms->firstframe = 1;
	clmd->sim_parms->lastframe = 250;
	clmd->sim_parms->vgroup_mass = 0;
	clmd->coll_parms->self_friction = 5.0;
	clmd->coll_parms->friction = 10.0;
	clmd->coll_parms->loop_count = 1;
	clmd->coll_parms->epsilon = 0.01f;
	clmd->coll_parms->flags = 0;

	/* These defaults are copied from softbody.c's
	* softbody_calc_forces() function.
	*/
	clmd->sim_parms->eff_force_scale = 1000.0;
	clmd->sim_parms->eff_wind_scale = 250.0;

	// also from softbodies
	clmd->sim_parms->maxgoal = 1.0f;
	clmd->sim_parms->mingoal = 0.0f;
	clmd->sim_parms->defgoal = 0.0f;
	clmd->sim_parms->goalspring = 100.0f;
	clmd->sim_parms->goalfrict = 0.0f;

	clmd->sim_parms->cache = NULL;
}


BVH *bvh_build_from_cloth (ClothModifierData *clmd, float epsilon)
{
	unsigned int i = 0;
	BVH	*bvh=NULL;
	Cloth *cloth = clmd->clothObject;
	ClothVertex *verts = NULL;

	if(!clmd)
		return NULL;

	cloth = clmd->clothObject;

	if(!cloth)
		return NULL;
	
	verts = cloth->verts;
	
	bvh = MEM_callocN(sizeof(BVH), "BVH");
	if (bvh == NULL) 
	{
		printf("bvh: Out of memory.\n");
		return NULL;
	}
	
	// springs = cloth->springs;
	// numsprings = cloth->numsprings;
	
	bvh->flags = 0;
	bvh->leaf_tree = NULL;
	bvh->leaf_root = NULL;
	bvh->tree = NULL;

	bvh->epsilon = epsilon;
	bvh->numfaces = cloth->numfaces;
	bvh->mfaces = cloth->mfaces;

	bvh->numverts = cloth->numverts;
	
	bvh->current_x = MEM_callocN ( sizeof ( MVert ) * bvh->numverts, "bvh->current_x" );
	bvh->current_xold = MEM_callocN ( sizeof ( MVert ) * bvh->numverts, "bvh->current_xold" );
	
	for(i = 0; i < bvh->numverts; i++)
	{
		VECCOPY(bvh->current_x[i].co, verts[i].tx);
		VECCOPY(bvh->current_xold[i].co, verts[i].txold);
	}
	
	bvh_build (bvh);
	
	return bvh;
}

void bvh_update_from_cloth(ClothModifierData *clmd, int moving)
{
	unsigned int i = 0;
	Cloth *cloth = clmd->clothObject;
	BVH *bvh = cloth->tree;
	ClothVertex *verts = cloth->verts;
	
	if(!bvh)
		return;
	
	if(cloth->numverts!=bvh->numverts)
		return;
	
	if(cloth->verts)
	{
		for(i = 0; i < bvh->numverts; i++)
		{
			VECCOPY(bvh->current_x[i].co, verts[i].tx);
			VECCOPY(bvh->current_xold[i].co, verts[i].txold);
		}
	}
	
	bvh_update(bvh, moving);
}

// unused in the moment, cloth needs quads from mesh
DerivedMesh *CDDM_convert_to_triangle ( DerivedMesh *dm )
{
	DerivedMesh *result = NULL;
	int i;
	int numverts = dm->getNumVerts ( dm );
	int numedges = dm->getNumEdges ( dm );
	int numfaces = dm->getNumFaces ( dm );

	MVert *mvert = CDDM_get_verts ( dm );
	MEdge *medge = CDDM_get_edges ( dm );
	MFace *mface = CDDM_get_faces ( dm );

	MVert *mvert2;
	MFace *mface2;
	unsigned int numtris=0;
	unsigned int numquads=0;
	int a = 0;
	int random = 0;
	int firsttime = 0;
	float vec1[3], vec2[3], vec3[3], vec4[3], vec5[3];
	float mag1=0, mag2=0;

	for ( i = 0; i < numfaces; i++ )
	{
		if ( mface[i].v4 )
			numquads++;
		else
			numtris++;
	}

	result = CDDM_from_template ( dm, numverts, 0, numtris + 2*numquads );

	if ( !result )
		return NULL;

	// do verts
	mvert2 = CDDM_get_verts ( result );
	for ( a=0; a<numverts; a++ )
	{
		MVert *inMV;
		MVert *mv = &mvert2[a];

		inMV = &mvert[a];

		DM_copy_vert_data ( dm, result, a, a, 1 );
		*mv = *inMV;
	}


	// do faces
	mface2 = CDDM_get_faces ( result );
	for ( a=0, i=0; a<numfaces; a++ )
	{
		MFace *mf = &mface2[i];
		MFace *inMF;
		inMF = &mface[a];

		/*
		DM_copy_face_data(dm, result, a, i, 1);

		*mf = *inMF;
		*/

		if ( mface[a].v4 && random==1 )
		{
			mf->v1 = mface[a].v2;
			mf->v2 = mface[a].v3;
			mf->v3 = mface[a].v4;
		}
		else
		{
			mf->v1 = mface[a].v1;
			mf->v2 = mface[a].v2;
			mf->v3 = mface[a].v3;
		}

		mf->v4 = 0;
		mf->flag |= ME_SMOOTH;

		test_index_face ( mf, NULL, 0, 3 );

		if ( mface[a].v4 )
		{
			MFace *mf2;

			i++;

			mf2 = &mface2[i];
			/*
			DM_copy_face_data(dm, result, a, i, 1);

			*mf2 = *inMF;
			*/

			if ( random==1 )
			{
				mf2->v1 = mface[a].v1;
				mf2->v2 = mface[a].v2;
				mf2->v3 = mface[a].v4;
			}
			else
			{
				mf2->v1 = mface[a].v4;
				mf2->v2 = mface[a].v1;
				mf2->v3 = mface[a].v3;
			}
			mf2->v4 = 0;
			mf2->flag |= ME_SMOOTH;

			test_index_face ( mf2, NULL, 0, 3 );
		}

		i++;
	}

	CDDM_calc_edges ( result );
	CDDM_calc_normals ( result );

	return result;

}


DerivedMesh *CDDM_create_tearing ( ClothModifierData *clmd, DerivedMesh *dm )
{
	DerivedMesh *result = NULL;
	unsigned int i = 0, a = 0, j=0;
	int numverts = dm->getNumVerts ( dm );
	int numedges = dm->getNumEdges ( dm );
	int numfaces = dm->getNumFaces ( dm );

	MVert *mvert = CDDM_get_verts ( dm );
	MEdge *medge = CDDM_get_edges ( dm );
	MFace *mface = CDDM_get_faces ( dm );

	MVert *mvert2;
	MFace *mface2;
	unsigned int numtris=0;
	unsigned int numquads=0;
	EdgeHash *edgehash = NULL;
	Cloth *cloth = clmd->clothObject;
	ClothSpring *springs = cloth->springs;
	unsigned int numsprings = cloth->numsprings;

	// create spring tearing hash
	edgehash = BLI_edgehash_new();

	for ( i = 0; i < numsprings; i++ )
	{
		if ( ( springs[i].flags & CLOTH_SPRING_FLAG_DEACTIVATE )
		        && ( !BLI_edgehash_haskey ( edgehash, springs[i].ij, springs[i].kl ) ) )
		{
			BLI_edgehash_insert ( edgehash, springs[i].ij, springs[i].kl, NULL );
			BLI_edgehash_insert ( edgehash, springs[i].kl, springs[i].ij, NULL );
			j++;
		}
	}

	// printf("found %d tears\n", j);

	result = CDDM_from_template ( dm, numverts, 0, numfaces );

	if ( !result )
		return NULL;

	// do verts
	mvert2 = CDDM_get_verts ( result );
	for ( a=0; a<numverts; a++ )
	{
		MVert *inMV;
		MVert *mv = &mvert2[a];

		inMV = &mvert[a];

		DM_copy_vert_data ( dm, result, a, a, 1 );
		*mv = *inMV;
	}


	// do faces
	mface2 = CDDM_get_faces ( result );
	for ( a=0, i=0; a<numfaces; a++ )
	{
		MFace *mf = &mface2[i];
		MFace *inMF;
		inMF = &mface[a];

		/*
		DM_copy_face_data(dm, result, a, i, 1);

		*mf = *inMF;
		*/

		if ( ( !BLI_edgehash_haskey ( edgehash, mface[a].v1, mface[a].v2 ) )
		        && ( !BLI_edgehash_haskey ( edgehash, mface[a].v2, mface[a].v3 ) )
		        && ( !BLI_edgehash_haskey ( edgehash, mface[a].v3, mface[a].v4 ) )
		        && ( !BLI_edgehash_haskey ( edgehash, mface[a].v4, mface[a].v1 ) ) )
		{
			mf->v1 = mface[a].v1;
			mf->v2 = mface[a].v2;
			mf->v3 = mface[a].v3;
			mf->v4 = mface[a].v4;

			test_index_face ( mf, NULL, 0, 4 );

			i++;
		}
	}

	CDDM_lower_num_faces ( result, i );
	CDDM_calc_edges ( result );
	CDDM_calc_normals ( result );

	BLI_edgehash_free ( edgehash, NULL );

	return result;
}



int modifiers_indexInObject(Object *ob, ModifierData *md_seek);

void cloth_clear_cache(Object *ob, ClothModifierData *clmd, float framenr)
{
	int stack_index = -1;
	
	if(!(clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_CCACHE_PROTECT))
	{
		stack_index = modifiers_indexInObject(ob, (ModifierData *)clmd);
		
		BKE_ptcache_id_clear((ID *)ob, PTCACHE_CLEAR_AFTER, framenr, stack_index);
	}
}
static void cloth_write_cache(Object *ob, ClothModifierData *clmd, float framenr)
{
	FILE *fp = NULL;
	int stack_index = -1;
	unsigned int a;
	Cloth *cloth = clmd->clothObject;
	
	if(!cloth)
		return;
	
	stack_index = modifiers_indexInObject(ob, (ModifierData *)clmd);
	
	fp = BKE_ptcache_id_fopen((ID *)ob, 'w', framenr, stack_index);
	if(!fp) return;
	
	for(a = 0; a < cloth->numverts; a++)
	{
		fwrite(&cloth->verts[a].x, sizeof(float),3,fp);
		fwrite(&cloth->verts[a].xconst, sizeof(float),3,fp);
		fwrite(&cloth->verts[a].v, sizeof(float),3,fp);
	}
	
	fclose(fp);
}
static int cloth_read_cache(Object *ob, ClothModifierData *clmd, float framenr)
{
	FILE *fp = NULL;
	int stack_index = -1;
	unsigned int a, ret = 1;
	Cloth *cloth = clmd->clothObject;
	
	if(!cloth)
		return 0;
	
	stack_index = modifiers_indexInObject(ob, (ModifierData *)clmd);
	
	fp = BKE_ptcache_id_fopen((ID *)ob, 'r', framenr, stack_index);
	if(!fp)
		ret = 0;
	else {
		for(a = 0; a < cloth->numverts; a++)
		{
			if(fread(&cloth->verts[a].x, sizeof(float), 3, fp) != 3) 
			{
				ret = 0;
				break;
			}
			if(fread(&cloth->verts[a].xconst, sizeof(float), 3, fp) != 3) 
			{
				ret = 0;
				break;
			}
			if(fread(&cloth->verts[a].v, sizeof(float), 3, fp) != 3) 
			{
				ret = 0;
				break;
			}
		}
		
		fclose(fp);
	}
	
	if(clmd->sim_parms->solver_type == 0)
		implicit_set_positions(clmd);
		
	return ret;
}


/**
* cloth_deform_verts - simulates one step, framenr is in frames.
*
**/
void clothModifier_do ( ClothModifierData *clmd, Object *ob, DerivedMesh *dm,
                        float ( *vertexCos ) [3], int numverts )
{
	unsigned int i;
	unsigned int numedges = -1;
	unsigned int numfaces = -1;
	MVert *mvert = NULL;
	MEdge *medge = NULL;
	MFace *mface = NULL;
	DerivedMesh *result = NULL, *result2 = NULL;
	Cloth *cloth = clmd->clothObject;
	unsigned int framenr = ( float ) G.scene->r.cfra;
	float current_time = bsystem_time ( ob, ( float ) G.scene->r.cfra, 0.0 );
	ListBase	*effectors = NULL;
	ClothVertex *newframe= NULL, *verts;
	Frame *frame = NULL;
	LinkNode *search = NULL;
	float deltaTime = current_time - clmd->sim_parms->sim_time;
	
	clmd->sim_parms->ob = ob;


	// only be active during a specific period:
	// that's "first frame" and "last frame" on GUI
	/*
	if ( ! ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_COLLOBJ ) )
	{
		if ( clmd->clothObject )
		{
			if ( clmd->sim_parms->cache )
			{
				if ( current_time < clmd->sim_parms->firstframe )
				{
					int frametime = cloth_cache_first_frame ( clmd );
					if ( cloth_cache_search_frame ( clmd, frametime ) )
					{
						cloth_cache_get_frame ( clmd, frametime );
						cloth_to_object ( ob, clmd, vertexCos, numverts );
					}
					return;
				}
				else if ( current_time > clmd->sim_parms->lastframe )
				{
					int frametime = cloth_cache_last_frame ( clmd );
					if ( cloth_cache_search_frame ( clmd, frametime ) )
					{
						cloth_cache_get_frame ( clmd, frametime );
						cloth_to_object ( ob, clmd, vertexCos, numverts );
					}
					return;
				}
				else if ( ABS ( deltaTime ) >= 2.0f ) // no timewarps allowed
				{
					if ( cloth_cache_search_frame ( clmd, framenr ) )
					{
						cloth_cache_get_frame ( clmd, framenr );
						cloth_to_object ( ob, clmd, vertexCos, numverts );
					}
					clmd->sim_parms->sim_time = current_time;
					return;
				}
			}

		}
	}
	*/

	// unused in the moment, calculated seperately in implicit.c
	clmd->sim_parms->dt = 1.0f / clmd->sim_parms->stepsPerFrame;

	clmd->sim_parms->sim_time = current_time;

	// check if cloth object was some collision object before and needs freeing now
	if ( ! ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_COLLOBJ ) && ( clmd->clothObject != NULL ) && ( clmd->clothObject->old_solver_type == 255 ) )
	{
		// temporary set CSIMSETT_FLAG_COLLOBJ flag for proper freeing
		clmd->sim_parms->flags |= CLOTH_SIMSETTINGS_FLAG_COLLOBJ;
		cloth_free_modifier ( clmd );
		clmd->sim_parms->flags &= ~CLOTH_SIMSETTINGS_FLAG_COLLOBJ;
	}

	// This is for collisions objects: check special case CSIMSETT_FLAG_COLLOBJ
	if ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_COLLOBJ )
	{
		// save next position + time
		if ( ( clmd->clothObject == NULL ) || ( numverts != clmd->clothObject->numverts ) )
		{
			if ( !collobj_from_object ( ob, clmd, dm, vertexCos, framenr ) )
			{
				clmd->sim_parms->flags |= CLOTH_SIMSETTINGS_FLAG_COLLOBJ;
				cloth_free_modifier ( clmd );
				return;
			}

			if ( clmd->clothObject == NULL )
				return;

			cloth = clmd->clothObject;
		}

		// Save old position
		clmd->sim_parms->sim_time_old = clmd->sim_parms->sim_time;
		clmd->sim_parms->sim_time = current_time;

		verts = cloth->verts;

		for ( i = 0; i < clmd->clothObject->numverts; i++, verts++ )
		{
			// Save the previous position.
			VECCOPY ( verts->xold, verts->x );
			VECCOPY ( verts->txold, verts->x );

			// Get the current position.
			VECCOPY ( verts->x, vertexCos[i] );
			Mat4MulVecfl ( ob->obmat, verts->x );

			// Compute the vertices "velocity".
			// (no dt correction here because of float error)
			VECSUB ( verts->v, verts->x, verts->xold );
		}

		return;
	}

	if ( deltaTime == 1.0f )
	{
		if ( ( clmd->clothObject == NULL ) || ( numverts != clmd->clothObject->numverts ) )
		{
			cloth_clear_cache(ob, clmd, 0);
			
			if ( !cloth_from_object ( ob, clmd, dm, vertexCos, numverts, framenr ) )
				return;

			if ( clmd->clothObject == NULL )
				return;

			cloth = clmd->clothObject;
		}

		clmd->clothObject->old_solver_type = clmd->sim_parms->solver_type;

		// Insure we have a clmd->clothObject, in case allocation failed.
		if ( clmd->clothObject != NULL )
		{
			if(!cloth_read_cache(ob, clmd, framenr))
			{
				verts = cloth->verts;

				// Force any pinned verts to their constrained location.
				for ( i = 0; i < clmd->clothObject->numverts; i++, verts++ )
				{
					// Save the previous position.
					VECCOPY ( verts->xold, verts->xconst );
					VECCOPY ( verts->txold, verts->x );

					// Get the current position.
					VECCOPY ( verts->xconst, vertexCos[i] );
					Mat4MulVecfl ( ob->obmat, verts->xconst );
				}

				tstart();

				// Call the solver.
				if ( solvers [clmd->sim_parms->solver_type].solver )
					solvers [clmd->sim_parms->solver_type].solver ( ob, framenr, clmd, effectors );

				tend();
				// printf ( "Cloth simulation time: %f\n", ( float ) tval() );

				cloth_write_cache(ob, clmd, framenr);

			}
			else // just retrieve the cached frame
			{
				cloth_read_cache(ob, clmd, framenr);
			}

			// Copy the result back to the object.
			cloth_to_object ( ob, clmd, vertexCos, numverts );

			// bvh_free(clmd->clothObject->tree);
			// clmd->clothObject->tree = bvh_build(clmd, clmd->coll_parms->epsilon);
		}

	}
	else if ( ( deltaTime <= 0.0f ) || ( deltaTime > 1.0f ) )
	{
		if ( clmd->clothObject != NULL )
		{
			if(cloth_read_cache(ob, clmd, framenr))
				cloth_to_object ( ob, clmd, vertexCos, numverts );
		}
		else
		{
			cloth_clear_cache(ob, clmd, 0);
		}
	}

}

/* frees all */
void cloth_free_modifier ( ClothModifierData *clmd )
{
	Cloth	*cloth = NULL;
	Object *ob = clmd->sim_parms->ob;
	
	if ( !clmd )
		return;

	cloth = clmd->clothObject;

	if ( ! ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_CCACHE_PROTECT ) )
	{
		if ( cloth )
		{
			// free our frame cache, TODO: but get to first position before
			if(ob)
				cloth_clear_cache ( ob, clmd, 0 );
			
			// If our solver provides a free function, call it
			if ( cloth->old_solver_type < 255 && solvers [cloth->old_solver_type].free )
			{
				solvers [cloth->old_solver_type].free ( clmd );
			}

			// Free the verts.
			if ( cloth->verts != NULL )
				MEM_freeN ( cloth->verts );

			cloth->verts = NULL;
			cloth->numverts = 0;

			// Free the springs.
			if ( cloth->springs != NULL )
			{
				LinkNode *search = cloth->springs;
				while(search)
				{
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
			if ( cloth->tree )
				bvh_free ( ( BVH * ) cloth->tree );

			// we save our faces for collision objects
			if ( cloth->mfaces )
				MEM_freeN ( cloth->mfaces );
			/*
			if(clmd->clothObject->facemarks)
				MEM_freeN(clmd->clothObject->facemarks);
			*/
			MEM_freeN ( cloth );
			clmd->clothObject = NULL;
		}
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
* This function is a modified version of the softbody.c:softbody_to_object() function.
**/
static void cloth_to_object ( Object *ob, ClothModifierData *clmd, float ( *vertexCos ) [3], unsigned int numverts )
{
	ClothVertex	*verts = NULL;
	unsigned int	i = 0;

	if ( clmd->clothObject )
	{
		verts = clmd->clothObject->verts;

		/* inverse matrix is not uptodate... */
		Mat4Invert ( ob->imat, ob->obmat );

		for ( i = 0; i < numverts; i++, verts++ )
		{
			VECCOPY ( vertexCos[i], verts->x );
			Mat4MulVecfl ( ob->imat, vertexCos[i] );	/* softbody is in global coords */
		}
	}
}


/**
* cloth_apply_vgroup - applies a vertex group as specified by type
*
**/
static void cloth_apply_vgroup ( ClothModifierData *clmd, DerivedMesh *dm, short vgroup )
{
	unsigned int i = 0;
	unsigned int j = 0;
	MDeformVert *dvert = NULL;
	Cloth *clothObj = NULL;
	unsigned int numverts = dm->getNumVerts ( dm );
	float goalfac = 0;
	ClothVertex *verts = NULL;

	clothObj = clmd->clothObject;

	if ( !dm )
		return;

	numverts = dm->getNumVerts ( dm );

	/* vgroup is 1 based, decrement so we can match the right group. */
	--vgroup;

	verts = clothObj->verts;

	for ( i = 0; i < numverts; i++, verts++ )
	{
		// LATER ON, support also mass painting here
		if ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL )
		{
			dvert = dm->getVertData ( dm, i, CD_MDEFORMVERT );
			if ( dvert )
			{
				for ( j = 0; j < dvert->totweight; j++ )
				{
					if ( dvert->dw[j].def_nr == vgroup )
					{
						verts->goal = dvert->dw [j].weight;

						goalfac= 1.0f;
						
						/*
						// Kicking goal factor to simplify things...who uses that anyway?
						// ABS ( clmd->sim_parms->maxgoal - clmd->sim_parms->mingoal );
						*/
						
						verts->goal  = ( float ) pow ( verts->goal , 4.0f );

						if ( dvert->dw [j].weight >=SOFTGOALSNAP )
						{
							verts->flags |= CVERT_FLAG_PINNED;
						}

						// TODO enable mass painting here, for the moment i let "goals" go first

						break;
					}
				}
			}
		}
	}
}

// only meshes supported at the moment
/* collision objects */
static int collobj_from_object ( Object *ob, ClothModifierData *clmd, DerivedMesh *dm, float ( *vertexCos ) [3], unsigned int numverts )
{
	unsigned int i;
	MVert *mvert = NULL;
	ClothVertex *verts = NULL;
	float tnull[3] = {0,0,0};

	/* If we have a clothObject, free it. */
	if ( clmd->clothObject != NULL )
		cloth_free_modifier ( clmd );

	/* Allocate a new cloth object. */
	clmd->clothObject = MEM_callocN ( sizeof ( Cloth ), "cloth" );
	if ( clmd->clothObject )
	{
		clmd->clothObject->old_solver_type = 255;
		// clmd->clothObject->old_collision_type = 255;
	}
	else if ( clmd->clothObject == NULL )
	{
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject." );
		return 0;
	}

	switch ( ob->type )
	{
		case OB_MESH:

			// mesh input objects need DerivedMesh
			if ( !dm )
				return 0;

			cloth_from_mesh ( ob, clmd, dm );

			if ( clmd->clothObject != NULL )
			{
				if ( !dm ) return 0;
				if ( !dm->getNumVerts ( dm ) || !dm->getNumFaces ( dm ) ) return 0;

				mvert = dm->getVertArray ( dm );
				verts = clmd->clothObject->verts;
				numverts = clmd->clothObject->numverts = dm->getNumVerts ( dm );

				for ( i = 0; i < numverts; i++, verts++ )
				{
					VECCOPY ( verts->x, mvert[i].co );
					Mat4MulVecfl ( ob->obmat, verts->x );
					verts->flags = 0;
					VECCOPY ( verts->xold, verts->x );
					VECCOPY ( verts->txold, verts->x );
					VECCOPY ( verts->tx, verts->x );
					VecMulf ( verts->v, 0.0f );
					verts->impulse_count = 0;
					VECCOPY ( verts->impulse, tnull );
				}
				clmd->clothObject->tree =  bvh_build_from_cloth ( clmd,clmd->coll_parms->epsilon );

			}

			return 1;
		default: return 0; // TODO - we do not support changing meshes
	}
}

/*
helper function to get proper spring length
when object is rescaled
*/
float cloth_globallen ( float *v1,float *v2,Object *ob )
{
	float p1[3],p2[3];
	VECCOPY ( p1,v1 );
	Mat4MulVecfl ( ob->obmat, p1 );
	VECCOPY ( p2,v2 );
	Mat4MulVecfl ( ob->obmat, p2 );
	return VecLenf ( p1,p2 );
}

// only meshes supported at the moment
static int cloth_from_object ( Object *ob, ClothModifierData *clmd, DerivedMesh *dm, float ( *vertexCos ) [3], unsigned int numverts, float framenr )
{
	unsigned int i = 0;
	// dm->getNumVerts(dm);
	MVert *mvert = NULL; // CDDM_get_verts(dm);
	ClothVertex *verts = NULL;
	float tnull[3] = {0,0,0};

	/* If we have a clothObject, free it. */
	if ( clmd->clothObject != NULL )
		cloth_free_modifier ( clmd );

	/* Allocate a new cloth object. */
	clmd->clothObject = MEM_callocN ( sizeof ( Cloth ), "cloth" );
	if ( clmd->clothObject )
	{
		clmd->clothObject->old_solver_type = 255;
		// clmd->clothObject->old_collision_type = 255;
	}
	else if ( !clmd->clothObject )
	{
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject." );
		return 0;
	}
	
	clmd->sim_parms->ob = ob;

	switch ( ob->type )
	{
		case OB_MESH:

			// mesh input objects need DerivedMesh
			if ( !dm )
				return 0;

			cloth_from_mesh ( ob, clmd, dm );

			if ( clmd->clothObject != NULL )
			{
				/* create springs */
				clmd->clothObject->springs = NULL;
				clmd->clothObject->numsprings = -1;
				
				mvert = CDDM_get_verts ( dm );
				verts = clmd->clothObject->verts;

				/* set initial values */
				for ( i = 0; i < numverts; i++, verts++ )
				{
					VECCOPY ( verts->x, mvert[i].co );
					Mat4MulVecfl ( ob->obmat, verts->x );

					verts->mass = clmd->sim_parms->mass;

					if ( clmd->sim_parms->flags & CLOTH_SIMSETTINGS_FLAG_GOAL )
						verts->goal= clmd->sim_parms->defgoal;
					else
						verts->goal= 0.0f;

					verts->flags = 0;
					VECCOPY ( verts->xold, verts->x );
					VECCOPY ( verts->xconst, verts->x );
					VECCOPY ( verts->txold, verts->x );
					VecMulf ( verts->v, 0.0f );

					verts->impulse_count = 0;
					VECCOPY ( verts->impulse, tnull );
				}
				
				if ( !cloth_build_springs ( clmd->clothObject, dm ) )
				{
					modifier_setError ( & ( clmd->modifier ), "Can't build springs." );
					return 0;
				}

				// apply / set vertex groups
				if ( clmd->sim_parms->vgroup_mass > 0 )
					cloth_apply_vgroup ( clmd, dm, clmd->sim_parms->vgroup_mass );

				// init our solver
				if ( solvers [clmd->sim_parms->solver_type].init )
					solvers [clmd->sim_parms->solver_type].init ( ob, clmd );

				clmd->clothObject->tree = bvh_build_from_cloth ( clmd, clmd->coll_parms->epsilon );

				cloth_write_cache(ob, clmd, framenr-1);
			}

			return 1;
		case OB_LATTICE:
			printf ( "Not supported: OB_LATTICE\n" );
			// lattice_to_softbody(ob);
			return 1;
		case OB_CURVE:
		case OB_SURF:
			printf ( "Not supported: OB_SURF| OB_CURVE\n" );
			return 1;
		default: return 0; // TODO - we do not support changing meshes
	}

	return 0;
}

static void cloth_from_mesh ( Object *ob, ClothModifierData *clmd, DerivedMesh *dm )
{
	unsigned int numverts = dm->getNumVerts ( dm );
	unsigned int numfaces = dm->getNumFaces ( dm );
	MFace *mface = dm->getFaceArray ( dm );
	unsigned int i = 0;

	/* Allocate our vertices.
	*/
	clmd->clothObject->numverts = numverts;
	clmd->clothObject->verts = MEM_callocN ( sizeof ( ClothVertex ) * clmd->clothObject->numverts, "clothVertex" );
	if ( clmd->clothObject->verts == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->verts." );
		return;
	}

	// save face information
	clmd->clothObject->numfaces = numfaces;
	clmd->clothObject->mfaces = MEM_callocN ( sizeof ( MFace ) * clmd->clothObject->numfaces, "clothMFaces" );
	if ( clmd->clothObject->mfaces == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->mfaces." );
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

// be carefull: implicit solver has to be resettet when using this one!
// --> only for implicit handling of this spring!
int cloth_add_spring ( ClothModifierData *clmd, unsigned int indexA, unsigned int indexB, float restlength, int spring_type)
{
	Cloth *cloth = clmd->clothObject;
	ClothSpring *spring = NULL;
	
	if(cloth)
	{
		// TODO: look if this spring is already there
		
		spring = ( ClothSpring * ) MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );
		
		spring->ij = indexA;
		spring->kl = indexB;
		spring->restlen =  restlength;
		spring->type = spring_type;
		spring->flags = 0;
		
		cloth->numsprings++;
	
		BLI_linklist_append ( &cloth->springs, spring );
		
		return 1;
	}
	return 0;
}

int cloth_build_springs ( Cloth *cloth, DerivedMesh *dm )
{
	ClothSpring *spring = NULL, *tspring = NULL, *tspring2 = NULL;
	unsigned int struct_springs = 0, shear_springs=0, bend_springs = 0;
	unsigned int i = 0, j = 0, akku_count;
	unsigned int numverts = dm->getNumVerts ( dm );
	unsigned int numedges = dm->getNumEdges ( dm );
	unsigned int numfaces = dm->getNumFaces ( dm );
	MEdge *medge = CDDM_get_edges ( dm );
	MFace *mface = CDDM_get_faces ( dm );
	unsigned int index2 = 0; // our second vertex index
	LinkNode **edgelist = NULL;
	EdgeHash *edgehash = NULL;
	LinkNode *search = NULL, *search2 = NULL;
	float temp[3], akku, min, max;
	LinkNode *node = NULL, *node2 = NULL;
	
	// error handling
	if ( numedges==0 )
		return 0;

	cloth->springs = NULL;

	edgelist = MEM_callocN ( sizeof ( LinkNode * ) * numverts, "cloth_edgelist_alloc" );
	for ( i = 0; i < numverts; i++ )
	{
		edgelist[i] = NULL;
	}

	if ( cloth->springs )
		MEM_freeN ( cloth->springs );

	// create spring network hash
	edgehash = BLI_edgehash_new();

	// structural springs
	for ( i = 0; i < numedges; i++ )
	{
		spring = ( ClothSpring * ) MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );

		if ( spring )
		{
			spring->ij = medge[i].v1;
			spring->kl = medge[i].v2;
			VECSUB ( temp, cloth->verts[spring->kl].x, cloth->verts[spring->ij].x );
			spring->restlen =  sqrt ( INPR ( temp, temp ) );
			spring->type = CLOTH_SPRING_TYPE_STRUCTURAL;
			spring->flags = 0;
			struct_springs++;
			
			if(!i)
				node2 = BLI_linklist_append_fast ( &cloth->springs, spring );
			else
				node2 = BLI_linklist_append_fast ( &node->next, spring );
			node = node2;
		}
	}
	
	// shear springs
	for ( i = 0; i < numfaces; i++ )
	{
		spring = ( ClothSpring *) MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );

		spring->ij = mface[i].v1;
		spring->kl = mface[i].v3;
		VECSUB ( temp, cloth->verts[spring->kl].x, cloth->verts[spring->ij].x );
		spring->restlen =  sqrt ( INPR ( temp, temp ) );
		spring->type = CLOTH_SPRING_TYPE_SHEAR;

		BLI_linklist_append ( &edgelist[spring->ij], spring );
		BLI_linklist_append ( &edgelist[spring->kl], spring );
		shear_springs++;

		node2 = BLI_linklist_append_fast ( &node->next, spring );
		node = node2;

		if ( mface[i].v4 )
		{
			spring = ( ClothSpring * ) MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );

			spring->ij = mface[i].v2;
			spring->kl = mface[i].v4;
			VECSUB ( temp, cloth->verts[spring->kl].x, cloth->verts[spring->ij].x );
			spring->restlen =  sqrt ( INPR ( temp, temp ) );
			spring->type = CLOTH_SPRING_TYPE_SHEAR;

			BLI_linklist_append ( &edgelist[spring->ij], spring );
			BLI_linklist_append ( &edgelist[spring->kl], spring );
			shear_springs++;

			node2 = BLI_linklist_append_fast ( &node->next, spring );
			node = node2;
		}
	}
	
	// bending springs
	search2 = cloth->springs;
	for ( i = struct_springs; i < struct_springs+shear_springs; i++ )
	{
		if ( !search2 )
			break;

		tspring2 = search2->link;
		search = edgelist[tspring2->kl];
		while ( search )
		{
			tspring = search->link;
			index2 = ( ( tspring->ij==tspring2->kl ) ? ( tspring->kl ) : ( tspring->ij ) );
			
			// check for existing spring
			// check also if startpoint is equal to endpoint
			if ( !BLI_edgehash_haskey ( edgehash, index2, tspring2->ij )
						   && !BLI_edgehash_haskey ( edgehash, tspring2->ij, index2 )
						   && ( index2!=tspring2->ij ) )
			{
				spring = ( ClothSpring * ) MEM_callocN ( sizeof ( ClothSpring ), "cloth spring" );

				spring->ij = tspring2->ij;
				spring->kl = index2;
				VECSUB ( temp, cloth->verts[index2].x, cloth->verts[tspring2->ij].x );
				spring->restlen =  sqrt ( INPR ( temp, temp ) );
				spring->type = CLOTH_SPRING_TYPE_BENDING;
				BLI_edgehash_insert ( edgehash, spring->ij, index2, NULL );
				bend_springs++;

				node2 = BLI_linklist_append_fast ( &node->next, spring );
				node = node2;
			}
			search = search->next;
		}
		search2 = search2->next;
	}
	
	cloth->numsprings = struct_springs + shear_springs + bend_springs;
	
	for ( i = 0; i < numverts; i++ )
	{
		BLI_linklist_free ( edgelist[i],NULL );
	}
	if ( edgelist )
		MEM_freeN ( edgelist );
	
	BLI_edgehash_free ( edgehash, NULL );

	return 1;

} /* cloth_build_springs */
/***************************************************************************************
* SPRING NETWORK BUILDING IMPLEMENTATION END
***************************************************************************************/

