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
#include "BKE_collisions.h"
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
static void cloth_to_object (Object *ob,  DerivedMesh *dm, ClothModifierData *clmd);
static void cloth_from_mesh (Object *ob, ClothModifierData *clmd, DerivedMesh *dm, float framenr);
static int cloth_from_object(Object *ob, ClothModifierData *clmd, DerivedMesh *dm, DerivedMesh *olddm, float framenr);

int cloth_build_springs ( Cloth *cloth, DerivedMesh *dm );
static void cloth_apply_vgroup(ClothModifierData *clmd, DerivedMesh *dm, short vgroup);
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
void cloth_init (ClothModifierData *clmd)
{
	/* Initialize our new data structure to reasonable values. */
	clmd->sim_parms.gravity [0] = 0.0;
	clmd->sim_parms.gravity [1] = 0.0;
	clmd->sim_parms.gravity [2] = -9.81;
	clmd->sim_parms.structural = 100.0;
	clmd->sim_parms.shear = 100.0;
	clmd->sim_parms.bending = 1.0;
	clmd->sim_parms.Cdis = 5.0;
	clmd->sim_parms.Cvi = 1.0;
	clmd->sim_parms.mass = 1.0f;
	clmd->sim_parms.stepsPerFrame = 5;
	clmd->sim_parms.sim_time = 1.0;
	clmd->sim_parms.flags = CLOTH_SIMSETTINGS_FLAG_RESET;
	clmd->sim_parms.solver_type = 0; 
	clmd->sim_parms.preroll = 0;
	clmd->sim_parms.maxspringlen = 10;
	clmd->coll_parms.self_friction = 5.0;
	clmd->coll_parms.friction = 10.0;
	clmd->coll_parms.loop_count = 1;
	clmd->coll_parms.epsilon = 0.01f;
	
	/* These defaults are copied from softbody.c's
	* softbody_calc_forces() function.
	*/
	clmd->sim_parms.eff_force_scale = 1000.0;
	clmd->sim_parms.eff_wind_scale = 250.0;

	// also from softbodies
	clmd->sim_parms.maxgoal = 1.0f;
	clmd->sim_parms.mingoal = 0.0f;
	clmd->sim_parms.defgoal = 0.7f;
	clmd->sim_parms.goalspring = 100.0f;
	clmd->sim_parms.goalfrict = 0.0f;

	clmd->sim_parms.cache = NULL;
}

// unused in the moment, cloth needs quads from mesh
DerivedMesh *CDDM_convert_to_triangle(DerivedMesh *dm)
{
	/*
	DerivedMesh *result = NULL;
	int i;
	int numverts = dm->getNumVerts(dm);
	int numedges = dm->getNumEdges(dm);
	int numfaces = dm->getNumFaces(dm);

	MVert *mvert = CDDM_get_verts(dm);
	MEdge *medge = CDDM_get_edges(dm);
	MFace *mface = CDDM_get_faces(dm);

	MVert *mvert2;
	MFace *mface2;
	unsigned int numtris=0;
	unsigned int numquads=0;
	int a = 0;
	int random = 0;
	int firsttime = 0;
	float vec1[3], vec2[3], vec3[3], vec4[3], vec5[3];
	float mag1=0, mag2=0;

	for(i = 0; i < numfaces; i++)
	{
		if(mface[i].v4)
			numquads++;
		else
			numtris++;	
	}

	result = CDDM_from_template(dm, numverts, 0, numtris + 2*numquads);

	if(!result)
		return NULL;

	// do verts
	mvert2 = CDDM_get_verts(result);
	for(a=0; a<numverts; a++) 
	{
		MVert *inMV;
		MVert *mv = &mvert2[a];

		inMV = &mvert[a];

		DM_copy_vert_data(dm, result, a, a, 1);
		*mv = *inMV;
	}


	// do faces
	mface2 = CDDM_get_faces(result);
	for(a=0, i=0; a<numfaces; a++) 
	{
		MFace *mf = &mface2[i];
		MFace *inMF;
		inMF = &mface[a];

		
		// DM_copy_face_data(dm, result, a, i, 1);

		// *mf = *inMF;
		

		if(mface[a].v4 && random==1)
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

		test_index_face(mf, NULL, 0, 3);

		if(mface[a].v4)
		{
			MFace *mf2;

			i++;

			mf2 = &mface2[i];
			
			// DM_copy_face_data(dm, result, a, i, 1);

			// *mf2 = *inMF;
			

			if(random==1)
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

			test_index_face(mf2, NULL, 0, 3);
		}

		i++;
	}

	CDDM_calc_edges(result);
	CDDM_calc_normals(result);

	return result;
	*/
	
	return NULL;
}


DerivedMesh *CDDM_create_tearing(ClothModifierData *clmd, DerivedMesh *dm)
{
	/*
	DerivedMesh *result = NULL;
	unsigned int i = 0, a = 0, j=0;
	int numverts = dm->getNumVerts(dm);
	int numedges = dm->getNumEdges(dm);
	int numfaces = dm->getNumFaces(dm);

	MVert *mvert = CDDM_get_verts(dm);
	MEdge *medge = CDDM_get_edges(dm);
	MFace *mface = CDDM_get_faces(dm);

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
	
	for(i = 0; i < numsprings; i++)
	{
		if((springs[i].flags & CSPRING_FLAG_DEACTIVATE)
		&&(!BLI_edgehash_haskey(edgehash, springs[i].ij, springs[i].kl)))
		{
			BLI_edgehash_insert(edgehash, springs[i].ij, springs[i].kl, NULL);
			BLI_edgehash_insert(edgehash, springs[i].kl, springs[i].ij, NULL);
			j++;
		}
	}
	
	// printf("found %d tears\n", j);
	
	result = CDDM_from_template(dm, numverts, 0, numfaces);

	if(!result)
		return NULL;

	// do verts
	mvert2 = CDDM_get_verts(result);
	for(a=0; a<numverts; a++) 
	{
		MVert *inMV;
		MVert *mv = &mvert2[a];

		inMV = &mvert[a];

		DM_copy_vert_data(dm, result, a, a, 1);
		*mv = *inMV;
	}


	// do faces
	mface2 = CDDM_get_faces(result);
	for(a=0, i=0; a<numfaces; a++) 
	{
		MFace *mf = &mface2[i];
		MFace *inMF;
		inMF = &mface[a];

		
		// DM_copy_face_data(dm, result, a, i, 1);

		// *mf = *inMF;
		
		
		if((!BLI_edgehash_haskey(edgehash, mface[a].v1, mface[a].v2))
		&&(!BLI_edgehash_haskey(edgehash, mface[a].v2, mface[a].v3))
		&&(!BLI_edgehash_haskey(edgehash, mface[a].v3, mface[a].v4))
		&&(!BLI_edgehash_haskey(edgehash, mface[a].v4, mface[a].v1)))
		{
			mf->v1 = mface[a].v1;
			mf->v2 = mface[a].v2;
			mf->v3 = mface[a].v3;
			mf->v4 = mface[a].v4;
	
			test_index_face(mf, NULL, 0, 4);
	
			i++;
		}
	}

	CDDM_lower_num_faces(result, i);
	CDDM_calc_edges(result);
	CDDM_calc_normals(result);
	
	BLI_edgehash_free(edgehash, NULL);

	return result;
	*/
	
	return NULL;
}


int cloth_cache_search_frame ( ClothModifierData *clmd, float time )
{
	Frame *frame = NULL;
	LinkNode *search = NULL;

	if ( clmd->clothObject )
	{
		search = clmd->sim_parms.cache;

		while ( search )
		{
			frame = ( Frame * ) search->link;

			if ( frame )
			{
				if ( frame->time == time )
					return 1;
			}

			search = search->next;
		}
	}

	return 0;

}

float cloth_cache_last_frame ( ClothModifierData *clmd )
{
	Frame *frame = NULL;
	LinkNode *search = NULL;
	float time = 0;

	if ( clmd->clothObject )
	{
		search = clmd->sim_parms.cache;

		while ( search )
		{
			frame = ( Frame * ) search->link;

			if ( frame )
			{
				if ( frame->time > time )
					time = frame->time;
			}
		}
	}
	return time;
}

float cloth_cache_first_frame ( ClothModifierData *clmd )
{
	Frame *frame = NULL;
	LinkNode *search = NULL;
	float time = -1.0;

	if ( clmd->clothObject )
	{
		search = clmd->sim_parms.cache;

		while ( search )
		{
			frame = ( Frame * ) search->link;

			if ( frame )
			{
				if ( time < 0.0 )
					time = frame->time;
				else
				{
					if ( frame->time < time )
						time = frame->time;
				}
			}
		}
	}
	return time;
}

void cloth_cache_get_frame ( ClothModifierData *clmd, float time )
{
	Frame *frame = NULL;
	LinkNode *search = NULL;
	float newtime = time + clmd->sim_parms.preroll;

	if ( clmd->clothObject )
	{
		search = clmd->sim_parms.cache;

		while ( search )
		{
			frame = ( Frame * ) search->link;

			if ( frame )
			{
				if ( frame->time == newtime )
				{
					// something changed, free cache!
					if ( clmd->clothObject->numverts != frame->numverts )
					{
						cloth_cache_free ( clmd, 0 );
						printf ( "clmd->clothObject->numverts != frame->numverts\n" );
						return;
					}

					memcpy ( clmd->clothObject->verts, frame->verts, sizeof ( ClothVertex ) *frame->numverts );
					
					memcpy ( clmd->clothObject->x, frame->x, sizeof ( float ) *frame->numverts * 3);
					
					memcpy ( clmd->clothObject->xold, frame->xold, sizeof ( float ) *frame->numverts * 3);
					
					implicit_set_positions ( clmd );

					return;
				}
			}

			search = search->next;
		}
	}
}

void cloth_cache_set_frame ( ClothModifierData *clmd, float time )
{
	Frame *frame = NULL;

	if ( clmd->clothObject )
	{
		frame = ( Frame * ) MEM_callocN ( sizeof ( Frame ), "cloth_cache_frame" );

		if ( frame )
		{
			frame->time = time;
			frame->numverts = clmd->clothObject->numverts;
			frame->verts = MEM_dupallocN ( clmd->clothObject->verts );

			if ( !frame->verts )
			{
				MEM_freeN ( frame );
				return;
			}
			
			frame->x = MEM_dupallocN ( clmd->clothObject->x );
			
			if ( !frame->x )
			{
				MEM_freeN ( frame->verts );
				MEM_freeN ( frame );
				return;
			}
			
			frame->xold = MEM_dupallocN ( clmd->clothObject->xold );
			
			if ( !frame->xold )
			{
				MEM_freeN ( frame->verts );
				MEM_freeN ( frame->x );
				MEM_freeN ( frame );
				return;
			}
			
			BLI_linklist_append ( &clmd->sim_parms.cache, frame );

		}
	}

}

// free cloth cache
void cloth_cache_free ( ClothModifierData *clmd, float time )
{
	Frame *frame = NULL;
	LinkNode *search = NULL, *lastsearch = NULL;
	float newtime = time + clmd->sim_parms.preroll;

	if ( time <= 2.0 )
		newtime = time;

	if ( clmd->clothObject )
	{
		if ( clmd->sim_parms.cache )
		{
			lastsearch = search = clmd->sim_parms.cache;

			while ( search )
			{
				frame = ( Frame * ) search->link;

				if ( frame->time >= newtime )
				{
					if ( frame->verts )
					{
						MEM_freeN ( frame->verts );
					}
					if ( frame->x )
					{
						MEM_freeN ( frame->x );
					}
					if ( frame->xold )
					{
						MEM_freeN ( frame->xold );
					}
					MEM_freeN ( frame );

					lastsearch->next = search->next;
					MEM_freeN ( search );
					search = lastsearch->next;
					lastsearch->next = NULL;
				}
				else
				{
					lastsearch = search;
					search = search->next;
				}
			}

			if ( time <= 1.0 )
			{
				clmd->sim_parms.cache = NULL;
			}

			if ( time <= 2.0 )
				clmd->sim_parms.preroll = 0;
		}
	}
}


/**
* cloth_deform_verts - simulates one step, framenr is in frames.
* 
**/
DerivedMesh *clothModifier_do(ClothModifierData *clmd,Object *ob, DerivedMesh *dm, int useRenderParams, int isFinalCalc)
{
	unsigned int i;
	unsigned int numverts = -1;
	unsigned int numedges = -1;
	unsigned int numfaces = -1;
	MVert *mvert = NULL;
	MEdge *medge = NULL;
	MFace *mface = NULL;
	DerivedMesh *result = NULL;
	Cloth *cloth = clmd->clothObject;
	unsigned int framenr = (float)G.scene->r.cfra;
	float current_time = bsystem_time(ob, (float)G.scene->r.cfra, 0.0);
	ListBase *effectors = NULL;
	ClothVertex *verts = NULL;
	float deltaTime = current_time - clmd->sim_parms.sim_time;	

	clmd->sim_parms.dt = 1.0f / (clmd->sim_parms.stepsPerFrame * G.scene->r.frs_sec);

	result = CDDM_copy(dm);
	
	if(!result)
	{
		return dm;
	}
	
	numverts = result->getNumVerts(result);
	numedges = result->getNumEdges(result);
	numfaces = result->getNumFaces(result);
	mvert = CDDM_get_verts(result);
	medge = CDDM_get_edges(result);
	mface = CDDM_get_faces(result);

	clmd->sim_parms.sim_time = current_time;

	// only be active during a specific period:
	// that's "first frame" and "last frame" on GUI
	/*
	if ( clmd->clothObject )
	{
		if ( clmd->sim_parms.cache )
		{
			if ( current_time < clmd->sim_parms.firstframe )
			{
				int frametime = cloth_cache_first_frame ( clmd );
				if ( cloth_cache_search_frame ( clmd, frametime ) )
				{
					cloth_cache_get_frame ( clmd, frametime );
					cloth_to_object ( ob, result, clmd );
				}
				return result;
			}
			else if ( current_time > clmd->sim_parms.lastframe )
			{
				int frametime = cloth_cache_last_frame ( clmd );
				if ( cloth_cache_search_frame ( clmd, frametime ) )
				{
					cloth_cache_get_frame ( clmd, frametime );
					cloth_to_object ( ob, result, clmd );
				}
				return result;
			}
			else if ( ABS ( deltaTime ) >= 2.0f ) // no timewarps allowed
			{
				if ( cloth_cache_search_frame ( clmd, framenr ) )
				{
					cloth_cache_get_frame ( clmd, framenr );
					cloth_to_object ( ob, result, clmd );
				}
				clmd->sim_parms.sim_time = current_time;
				return result;
			}
		}
	}
	*/
	
	if(deltaTime == 1.0f)
	{
		if ((clmd->clothObject == NULL) || (numverts != clmd->clothObject->numverts) ) 
		{
			if(!cloth_from_object (ob, clmd, result, dm, framenr))
				return result;

			if(clmd->clothObject == NULL)
				return result;

			cloth = clmd->clothObject;
		}

		clmd->clothObject->old_solver_type = clmd->sim_parms.solver_type;

		// Insure we have a clmd->clothObject, in case allocation failed.
		if (clmd->clothObject != NULL) 
		{
			if(!cloth_cache_search_frame(clmd, framenr))
			{
				verts = cloth->verts;
				
				// Force any pinned verts to their constrained location.
				for ( i = 0; i < clmd->clothObject->numverts; i++, verts++ )
				{
					// Save the previous position.
					VECCOPY ( cloth->xold[i], verts->xconst );
					VECCOPY ( cloth->current_xold[i], cloth->x[i] );
					// Get the current position.
					VECCOPY ( verts->xconst, mvert[i].co );
					Mat4MulVecfl ( ob->obmat, verts->xconst );
				}
				
				tstart();
				
				/* Call the solver. */
				if (solvers [clmd->sim_parms.solver_type].solver)
					solvers [clmd->sim_parms.solver_type].solver (ob, framenr, clmd, effectors);
				
				tend();
				
				printf("Cloth simulation time: %f\n", tval());
				
				cloth_cache_set_frame(clmd, framenr);

			}
			else // just retrieve the cached frame
			{
				cloth_cache_get_frame(clmd, framenr);
			}

			// Copy the result back to the object.
			cloth_to_object (ob, result, clmd);
			
			// bvh_free(clmd->clothObject->tree);
			// clmd->clothObject->tree = bvh_build(clmd, clmd->coll_parms.epsilon);
		} 

	}
	else if ( ( deltaTime <= 0.0f ) || ( deltaTime > 1.0f ) )
	{
		if ( ( clmd->clothObject != NULL ) && ( clmd->sim_parms.cache ) )
		{
			if ( cloth_cache_search_frame ( clmd, framenr ) )
			{
				cloth_cache_get_frame(clmd, framenr);
				cloth_to_object (ob, result, clmd);
			}
		}
	}
	
	return result;
}

/* frees all */
void cloth_free_modifier (ClothModifierData *clmd)
{
	Cloth	*cloth = NULL;

	if(!clmd)
		return;

	cloth = clmd->clothObject;

	// free our frame cache
	cloth_cache_free(clmd, 0);

	/* Calls the solver and collision frees first as they
	* might depend on data in clmd->clothObject. */

	if (cloth) 
	{	
		// If our solver provides a free function, call it
		if (cloth->old_solver_type < 255 && solvers [cloth->old_solver_type].free) 
		{	
			solvers [cloth->old_solver_type].free (clmd);
		}
		
		// Free the verts.
		if (cloth->verts != NULL)
			MEM_freeN (cloth->verts);
		
		// Free the faces.
		if ( cloth->mfaces != NULL )
			MEM_freeN ( cloth->mfaces );

		// Free the verts.
		if ( cloth->x != NULL )
			MEM_freeN ( cloth->x );
		
		// Free the verts.
		if ( cloth->xold != NULL )
			MEM_freeN ( cloth->xold );
		
		// Free the verts.
		if ( cloth->current_x != NULL )
			MEM_freeN ( cloth->current_x );
		
		// Free the verts.
		if ( cloth->current_xold != NULL )
			MEM_freeN ( cloth->current_xold );

		cloth->verts = NULL;
		cloth->numverts = -1;
		
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

		cloth->numsprings = -1;
		
		// free BVH collision tree
		if(cloth->tree)
			bvh_free((BVH *)cloth->tree);
		
		MEM_freeN (cloth);
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
* This function is a modified version of the softbody.c:softbody_to_object() function.
**/
static void cloth_to_object (Object *ob,  DerivedMesh *dm, ClothModifierData *clmd)
{
	unsigned int	i = 0;
	MVert *mvert = NULL;
	unsigned int numverts;
	Cloth *cloth = clmd->clothObject;

	if (clmd->clothObject) {
		/* inverse matrix is not uptodate... */
		Mat4Invert (ob->imat, ob->obmat);

		mvert = CDDM_get_verts(dm);
		numverts = dm->getNumVerts(dm);

		for (i = 0; i < numverts; i++)
		{
			VECCOPY (mvert[i].co, cloth->x[i]);
			Mat4MulVecfl (ob->imat, mvert[i].co);	/* softbody is in global coords */
		}
	}
}


/**
* cloth_apply_vgroup - applies a vertex group as specified by type
*
**/
static void cloth_apply_vgroup(ClothModifierData *clmd, DerivedMesh *dm, short vgroup)
{
	unsigned int i = 0;
	unsigned int j = 0;
	MDeformVert	*dvert = NULL;
	Cloth *clothObj = NULL;
	unsigned int numverts = 0;
	float goalfac = 0;
	ClothVertex *verts = NULL;

	clothObj = clmd->clothObject;

	if ( !dm )
		return;

	numverts = dm->getNumVerts(dm);

	/* vgroup is 1 based, decrement so we can match the right group. */
	--vgroup;

	verts = clothObj->verts;

	for ( i = 0; i < numverts; i++, verts++ )
	{
		// LATER ON, support also mass painting here
		if ( clmd->sim_parms.flags & CLOTH_SIMSETTINGS_FLAG_GOAL )
		{
			dvert = dm->getVertData ( dm, i, CD_MDEFORMVERT );
			if ( dvert )
			{
				for ( j = 0; j < dvert->totweight; j++ )
				{
					if ( dvert->dw[j].def_nr == vgroup )
					{
						verts->goal = dvert->dw [j].weight;

						goalfac= ABS ( clmd->sim_parms.maxgoal - clmd->sim_parms.mingoal );
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
static int cloth_from_object(Object *ob, ClothModifierData *clmd, DerivedMesh *dm, DerivedMesh *olddm, float framenr)
{
	unsigned int i;
	unsigned int numverts = dm->getNumVerts(dm);
	MVert *mvert = CDDM_get_verts(dm);
	float tnull[3] = {0,0,0};
	
	/* If we have a clothObject, free it. */
	if (clmd->clothObject != NULL)
		cloth_free_modifier (clmd);

	/* Allocate a new cloth object. */
	clmd->clothObject = MEM_callocN (sizeof(Cloth), "cloth");
	if (clmd->clothObject) 
	{
		clmd->clothObject->old_solver_type = 255;
	}
	else if (clmd->clothObject == NULL) 
	{
		modifier_setError (&(clmd->modifier), "Out of memory on allocating clmd->clothObject.");
		return 0;
	}

	switch (ob->type)
	{
		case OB_MESH:
		
			// mesh input objects need DerivedMesh
			if ( !dm )
				return 0;

		cloth_from_mesh (ob, clmd, dm, framenr);

			if ( clmd->clothObject != NULL )
			{
				/* create springs */
				clmd->clothObject->springs = NULL;
				clmd->clothObject->numsprings = -1;

			if (!cloth_build_springs (clmd->clothObject, dm) )
			{
				modifier_setError (&(clmd->modifier), "Can't build springs.");
				return 0;
			}  

			/* set initial values */
			for (i = 0; i < numverts; ++i)
			{
				VECCOPY (clmd->clothObject->x[i], mvert[i].co);
				Mat4MulVecfl(ob->obmat, clmd->clothObject->x[i]);

				clmd->clothObject->verts [i].mass = clmd->sim_parms.mass;
				if ( clmd->sim_parms.flags & CLOTH_SIMSETTINGS_FLAG_GOAL )
					clmd->clothObject->verts [i].goal= clmd->sim_parms.defgoal;
				else
					clmd->clothObject->verts [i].goal= 0.0f;
				clmd->clothObject->verts [i].flags = 0;
				VECCOPY(clmd->clothObject->xold[i], clmd->clothObject->x[i]);
				VECCOPY(clmd->clothObject->verts [i].xconst, clmd->clothObject->x[i]);
				VECCOPY(clmd->clothObject->current_xold[i], clmd->clothObject->x[i]);
				VecMulf(clmd->clothObject->verts [i].v, 0.0f);

				clmd->clothObject->verts [i].impulse_count = 0;
				VECCOPY ( clmd->clothObject->verts [i].impulse, tnull );
			}

			/* apply / set vertex groups */
			if (clmd->sim_parms.vgroup_mass > 0)
				cloth_apply_vgroup (clmd, olddm, clmd->sim_parms.vgroup_mass);

			/* init our solver */
			if (solvers [clmd->sim_parms.solver_type].init)
				solvers [clmd->sim_parms.solver_type].init (ob, clmd);

			clmd->clothObject->tree = NULL; // bvh_build(clmd, clmd->coll_parms.epsilon);

			// cloth_cache_set_frame(clmd, 1);
		}

		return 1;
		default: return 0; // TODO - we do not support changing meshes
	}
	
	return 0;
}

static void cloth_from_mesh (Object *ob, ClothModifierData *clmd, DerivedMesh *dm, float framenr)
{
	unsigned int numverts = dm->getNumVerts(dm);
	unsigned int numfaces = dm->getNumFaces(dm);
	MFace *mface = CDDM_get_faces(dm);

	/* Allocate our vertices.
	*/
	clmd->clothObject->numverts = numverts;
	clmd->clothObject->verts = MEM_callocN (sizeof (ClothVertex) * clmd->clothObject->numverts, "clothVertex");
	if (clmd->clothObject->verts == NULL) 
	{
		cloth_free_modifier (clmd);
		modifier_setError (&(clmd->modifier), "Out of memory on allocating clmd->clothObject->verts.");
		return;
	}
	
	clmd->clothObject->x = MEM_callocN ( sizeof ( float ) * clmd->clothObject->numverts * 3, "Cloth MVert_x" );
	if ( clmd->clothObject->x == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->x." );
		return;
	}
	
	clmd->clothObject->xold = MEM_callocN ( sizeof ( float ) * clmd->clothObject->numverts * 3, "Cloth MVert_xold" );
	if ( clmd->clothObject->xold == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->xold." );
		return;
	}
	
	clmd->clothObject->current_x = MEM_callocN ( sizeof ( float ) * clmd->clothObject->numverts * 3, "Cloth MVert_current_x" );
	if ( clmd->clothObject->current_x == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->current_x." );
		return;
	}
	
	clmd->clothObject->current_xold = MEM_callocN ( sizeof ( float ) * clmd->clothObject->numverts * 3, "Cloth MVert_current_xold" );
	if ( clmd->clothObject->current_xold == NULL )
	{
		cloth_free_modifier ( clmd );
		modifier_setError ( & ( clmd->modifier ), "Out of memory on allocating clmd->clothObject->current_xold." );
		return;
	}

	// save face information
	clmd->clothObject->numfaces = numfaces;
	clmd->clothObject->mfaces = MEM_callocN (sizeof (MFace) * clmd->clothObject->numfaces, "clothMFaces");
	if (clmd->clothObject->mfaces == NULL) 
	{
		cloth_free_modifier (clmd);
		modifier_setError (&(clmd->modifier), "Out of memory on allocating clmd->clothObject->mfaces.");
		return;
	}
	memcpy(clmd->clothObject->mfaces, mface, sizeof(MFace)*numfaces);

	/* Free the springs since they can't be correct if the vertices
	* changed.
	*/
	if (clmd->clothObject->springs != NULL)
		MEM_freeN (clmd->clothObject->springs);

}

/***************************************************************************************
* SPRING NETWORK BUILDING IMPLEMENTATION BEGIN
***************************************************************************************/

// be carefull: implicit solver has to be resettet when using this one!
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
	unsigned int i = 0;
	unsigned int numverts = dm->getNumVerts ( dm );
	unsigned int numedges = dm->getNumEdges ( dm );
	unsigned int numfaces = dm->getNumFaces ( dm );
	MVert *mvert = CDDM_get_verts ( dm );
	MEdge *medge = CDDM_get_edges ( dm );
	MFace *mface = CDDM_get_faces ( dm );
	unsigned int index2 = 0; // our second vertex index
	LinkNode **edgelist = NULL;
	EdgeHash *edgehash = NULL;
	LinkNode *search = NULL, *search2 = NULL;
	float temp[3];
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
			VECSUB ( temp, mvert[spring->kl].co, mvert[spring->ij].co );
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
		VECSUB ( temp, mvert[spring->kl].co, mvert[spring->ij].co );
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

			spring->ij = mface[i].v1;
			spring->kl = mface[i].v3;
			VECSUB ( temp, mvert[spring->kl].co, mvert[spring->ij].co );
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
				VECSUB ( temp, mvert[index2].co, mvert[tspring2->ij].co );
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

