/**
 * shrinkwrap.c
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <string.h>
#include <float.h>
#include <math.h>
#include <memory.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>

#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_scene_types.h"

#include "BKE_shrinkwrap.h"
#include "BKE_DerivedMesh.h"
#include "BKE_lattice.h"
#include "BKE_utildefines.h"
#include "BKE_deform.h"
#include "BKE_mesh.h"
#include "BKE_subsurf.h"

#include "BLI_math.h"
#include "BLI_editVert.h"

#include "MEM_guardedalloc.h"


/* Util macros */
#define TO_STR(a)	#a
#define JOIN(a,b)	a##b

#define OUT_OF_MEMORY()	((void)printf("Shrinkwrap: Out of memory\n"))

/* Benchmark macros */
#if !defined(_WIN32) && 0

#include <sys/time.h>

#define BENCH(a)	\
	do {			\
		double _t1, _t2;				\
		struct timeval _tstart, _tend;	\
		clock_t _clock_init = clock();	\
		gettimeofday ( &_tstart, NULL);	\
		(a);							\
		gettimeofday ( &_tend, NULL);	\
		_t1 = ( double ) _tstart.tv_sec + ( double ) _tstart.tv_usec/ ( 1000*1000 );	\
		_t2 = ( double )   _tend.tv_sec + ( double )   _tend.tv_usec/ ( 1000*1000 );	\
		printf("%s: %fs (real) %fs (cpu)\n", #a, _t2-_t1, (float)(clock()-_clock_init)/CLOCKS_PER_SEC);\
	} while(0)

#else

#define BENCH(a)	(a)

#endif

typedef void ( *Shrinkwrap_ForeachVertexCallback) (DerivedMesh *target, float *co, float *normal);

/* get derived mesh */
//TODO is anyfunction that does this? returning the derivedFinal witouth we caring if its in edit mode or not?
DerivedMesh *object_get_derived_final(struct Scene *scene, Object *ob, CustomDataMask dataMask)
{
	Mesh *me= ob->data;
	EditMesh *em = BKE_mesh_get_editmesh(me);

	if (em)
	{
		DerivedMesh *final = NULL;
		editmesh_get_derived_cage_and_final(scene, ob, em, &final, dataMask);
		
		BKE_mesh_end_editmesh(me, em);
		return final;
	}
	else
		return mesh_get_derived_final(scene, ob, dataMask);
}

/* Space transform */
void space_transform_from_matrixs(SpaceTransform *data, float local[4][4], float target[4][4])
{
	float itarget[4][4];
	invert_m4_m4(itarget, target);
	mul_serie_m4(data->local2target, itarget, local, 0, 0, 0, 0, 0, 0);
	invert_m4_m4(data->target2local, data->local2target);
}

void space_transform_apply(const SpaceTransform *data, float *co)
{
	mul_v3_m4v3(co, ((SpaceTransform*)data)->local2target, co);
}

void space_transform_invert(const SpaceTransform *data, float *co)
{
	mul_v3_m4v3(co, ((SpaceTransform*)data)->target2local, co);
}

static void space_transform_apply_normal(const SpaceTransform *data, float *no)
{
	mul_mat3_m4_v3( ((SpaceTransform*)data)->local2target, no);
	normalize_v3(no); // TODO: could we just determine de scale value from the matrix?
}

static void space_transform_invert_normal(const SpaceTransform *data, float *no)
{
	mul_mat3_m4_v3(((SpaceTransform*)data)->target2local, no);
	normalize_v3(no); // TODO: could we just determine de scale value from the matrix?
}

/*
 * Returns the squared distance between two given points
 */
static float squared_dist(const float *a, const float *b)
{
	float tmp[3];
	VECSUB(tmp, a, b);
	return INPR(tmp, tmp);
}

/*
 * Shrinkwrap to the nearest vertex
 *
 * it builds a kdtree of vertexs we can attach to and then
 * for each vertex performs a nearest vertex search on the tree
 */
static void shrinkwrap_calc_nearest_vertex(ShrinkwrapCalcData *calc)
{
	int i;

	BVHTreeFromMesh treeData = NULL_BVHTreeFromMesh;
	BVHTreeNearest  nearest  = NULL_BVHTreeNearest;


	BENCH(bvhtree_from_mesh_verts(&treeData, calc->target, 0.0, 2, 6));
	if(treeData.tree == NULL)
	{
		OUT_OF_MEMORY();
		return;
	}

	//Setup nearest
	nearest.index = -1;
	nearest.dist = FLT_MAX;
#ifndef __APPLE__
#pragma omp parallel for default(none) private(i) firstprivate(nearest) shared(treeData,calc) schedule(static)
#endif
	for(i = 0; i<calc->numVerts; ++i)
	{
		float *co = calc->vertexCos[i];
		float tmp_co[3];
		float weight = defvert_array_find_weight_safe(calc->dvert, i, calc->vgroup);
		if(weight == 0.0f) continue;


		//Convert the vertex to tree coordinates
		if(calc->vert)
		{
			VECCOPY(tmp_co, calc->vert[i].co);
		}
		else
		{
			VECCOPY(tmp_co, co);
		}
		space_transform_apply(&calc->local2target, tmp_co);

		//Use local proximity heuristics (to reduce the nearest search)
		//
		//If we already had an hit before.. we assume this vertex is going to have a close hit to that other vertex
		//so we can initiate the "nearest.dist" with the expected value to that last hit.
		//This will lead in prunning of the search tree.
		if(nearest.index != -1)
			nearest.dist = squared_dist(tmp_co, nearest.co);
		else
			nearest.dist = FLT_MAX;

		BLI_bvhtree_find_nearest(treeData.tree, tmp_co, &nearest, treeData.nearest_callback, &treeData);


		//Found the nearest vertex
		if(nearest.index != -1)
		{
			//Adjusting the vertex weight, so that after interpolating it keeps a certain distance from the nearest position
			float dist = sasqrt(nearest.dist);
			if(dist > FLT_EPSILON) weight *= (dist - calc->keepDist)/dist;

			//Convert the coordinates back to mesh coordinates
			VECCOPY(tmp_co, nearest.co);
			space_transform_invert(&calc->local2target, tmp_co);

			interp_v3_v3v3(co, co, tmp_co, weight);	//linear interpolation
		}
	}

	free_bvhtree_from_mesh(&treeData);
}

/*
 * This function raycast a single vertex and updates the hit if the "hit" is considered valid.
 * Returns TRUE if "hit" was updated.
 * Opts control whether an hit is valid or not
 * Supported options are:
 *	MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE (front faces hits are ignored)
 *	MOD_SHRINKWRAP_CULL_TARGET_BACKFACE (back faces hits are ignored)
 */
int normal_projection_project_vertex(char options, const float *vert, const float *dir, const SpaceTransform *transf, BVHTree *tree, BVHTreeRayHit *hit, BVHTree_RayCastCallback callback, void *userdata)
{
	float tmp_co[3], tmp_no[3];
	const float *co, *no;
	BVHTreeRayHit hit_tmp;

	//Copy from hit (we need to convert hit rays from one space coordinates to the other
	memcpy( &hit_tmp, hit, sizeof(hit_tmp) );

	//Apply space transform (TODO readjust dist)
	if(transf)
	{
		VECCOPY( tmp_co, vert );
		space_transform_apply( transf, tmp_co );
		co = tmp_co;

		VECCOPY( tmp_no, dir );
		space_transform_apply_normal( transf, tmp_no );
		no = tmp_no;

		hit_tmp.dist *= mat4_to_scale( ((SpaceTransform*)transf)->local2target );
	}
	else
	{
		co = vert;
		no = dir;
	}

	hit_tmp.index = -1;

	BLI_bvhtree_ray_cast(tree, co, no, 0.0f, &hit_tmp, callback, userdata);

	if(hit_tmp.index != -1)
	{
		float dot = INPR( dir, hit_tmp.no);

		if(((options & MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE) && dot <= 0.0f)
		|| ((options & MOD_SHRINKWRAP_CULL_TARGET_BACKFACE) && dot >= 0.0f))
			return FALSE; //Ignore hit


		//Inverting space transform (TODO make coeherent with the initial dist readjust)
		if(transf)
		{
			space_transform_invert( transf, hit_tmp.co );
			space_transform_invert_normal( transf, hit_tmp.no );

			hit_tmp.dist = len_v3v3( (float*)vert, hit_tmp.co );
		}

		memcpy(hit, &hit_tmp, sizeof(hit_tmp) );
		return TRUE;
	}
	return FALSE;
}


static void shrinkwrap_calc_normal_projection(ShrinkwrapCalcData *calc, struct Scene *scene)
{
	int i;

	//Options about projection direction
	const char use_normal	= calc->smd->shrinkOpts;
	float proj_axis[3]		= {0.0f, 0.0f, 0.0f};

	//Raycast and tree stuff
	BVHTreeRayHit hit;
	BVHTreeFromMesh treeData= NULL_BVHTreeFromMesh;

	//auxiliar target
	DerivedMesh *auxMesh	= NULL;
	BVHTreeFromMesh auxData	= NULL_BVHTreeFromMesh;
	SpaceTransform local2aux;

	//If the user doesn't allows to project in any direction of projection axis
	//then theres nothing todo.
	if((use_normal & (MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR | MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR)) == 0)
		return;


	//Prepare data to retrieve the direction in which we should project each vertex
	if(calc->smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL)
	{
		if(calc->vert == NULL) return;
	}
	else
	{
		//The code supports any axis that is a combination of X,Y,Z
		//altought currently UI only allows to set the 3 diferent axis
		if(calc->smd->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS) proj_axis[0] = 1.0f;
		if(calc->smd->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS) proj_axis[1] = 1.0f;
		if(calc->smd->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS) proj_axis[2] = 1.0f;

		normalize_v3(proj_axis);

		//Invalid projection direction
		if(INPR(proj_axis, proj_axis) < FLT_EPSILON)
			return; 
	}

	if(calc->smd->auxTarget)
	{
		auxMesh = object_get_derived_final(scene, calc->smd->auxTarget, CD_MASK_BAREMESH);
		space_transform_setup( &local2aux, calc->ob, calc->smd->auxTarget);
	}

	//After sucessufuly build the trees, start projection vertexs
	if( bvhtree_from_mesh_faces(&treeData, calc->target, calc->keepDist, 4, 6)
	&&  (auxMesh == NULL || bvhtree_from_mesh_faces(&auxData, auxMesh, 0.0, 4, 6)))
	{

#ifndef __APPLE__
#pragma omp parallel for private(i,hit) schedule(static)
#endif
		for(i = 0; i<calc->numVerts; ++i)
		{
			float *co = calc->vertexCos[i];
			float tmp_co[3], tmp_no[3];
			float weight = defvert_array_find_weight_safe(calc->dvert, i, calc->vgroup);

			if(weight == 0.0f) continue;

			if(calc->vert)
			{
				VECCOPY(tmp_co, calc->vert[i].co);
				if(calc->smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL)
					normal_short_to_float_v3(tmp_no, calc->vert[i].no);
				else
					VECCOPY(tmp_no, proj_axis);
			}
			else
			{
				VECCOPY(tmp_co, co);
				VECCOPY(tmp_no, proj_axis);
			}


			hit.index = -1;
			hit.dist = 10000.0f; //TODO: we should use FLT_MAX here, but sweepsphere code isnt prepared for that

			//Project over positive direction of axis
			if(use_normal & MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR)
			{

				if(auxData.tree)
					normal_projection_project_vertex(0, tmp_co, tmp_no, &local2aux, auxData.tree, &hit, auxData.raycast_callback, &auxData);

				normal_projection_project_vertex(calc->smd->shrinkOpts, tmp_co, tmp_no, &calc->local2target, treeData.tree, &hit, treeData.raycast_callback, &treeData);
			}

			//Project over negative direction of axis
			if(use_normal & MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR)
			{
				float inv_no[3] = { -tmp_no[0], -tmp_no[1], -tmp_no[2] };


				if(auxData.tree)
					normal_projection_project_vertex(0, tmp_co, inv_no, &local2aux, auxData.tree, &hit, auxData.raycast_callback, &auxData);

				normal_projection_project_vertex(calc->smd->shrinkOpts, tmp_co, inv_no, &calc->local2target, treeData.tree, &hit, treeData.raycast_callback, &treeData);
			}


			if(hit.index != -1)
			{
				interp_v3_v3v3(co, co, hit.co, weight);
			}
		}
	}

	//free data structures
	free_bvhtree_from_mesh(&treeData);
	free_bvhtree_from_mesh(&auxData);
}

/*
 * Shrinkwrap moving vertexs to the nearest surface point on the target
 *
 * it builds a BVHTree from the target mesh and then performs a
 * NN matchs for each vertex
 */
static void shrinkwrap_calc_nearest_surface_point(ShrinkwrapCalcData *calc)
{
	int i;

	BVHTreeFromMesh treeData = NULL_BVHTreeFromMesh;
	BVHTreeNearest  nearest  = NULL_BVHTreeNearest;

	//Create a bvh-tree of the given target
	BENCH(bvhtree_from_mesh_faces( &treeData, calc->target, 0.0, 2, 6));
	if(treeData.tree == NULL)
	{
		OUT_OF_MEMORY();
		return;
	}

	//Setup nearest
	nearest.index = -1;
	nearest.dist = FLT_MAX;


	//Find the nearest vertex
#ifndef __APPLE__
#pragma omp parallel for default(none) private(i) firstprivate(nearest) shared(calc,treeData) schedule(static)
#endif
	for(i = 0; i<calc->numVerts; ++i)
	{
		float *co = calc->vertexCos[i];
		float tmp_co[3];
		float weight = defvert_array_find_weight_safe(calc->dvert, i, calc->vgroup);
		if(weight == 0.0f) continue;

		//Convert the vertex to tree coordinates
		if(calc->vert)
		{
			VECCOPY(tmp_co, calc->vert[i].co);
		}
		else
		{
			VECCOPY(tmp_co, co);
		}
		space_transform_apply(&calc->local2target, tmp_co);

		//Use local proximity heuristics (to reduce the nearest search)
		//
		//If we already had an hit before.. we assume this vertex is going to have a close hit to that other vertex
		//so we can initiate the "nearest.dist" with the expected value to that last hit.
		//This will lead in prunning of the search tree.
		if(nearest.index != -1)
			nearest.dist = squared_dist(tmp_co, nearest.co);
		else
			nearest.dist = FLT_MAX;

		BLI_bvhtree_find_nearest(treeData.tree, tmp_co, &nearest, treeData.nearest_callback, &treeData);

		//Found the nearest vertex
		if(nearest.index != -1)
		{
			if(calc->smd->shrinkOpts & MOD_SHRINKWRAP_KEEP_ABOVE_SURFACE)
			{
				//Make the vertex stay on the front side of the face
				VECADDFAC(tmp_co, nearest.co, nearest.no, calc->keepDist);
			}
			else
			{
				//Adjusting the vertex weight, so that after interpolating it keeps a certain distance from the nearest position
				float dist = sasqrt( nearest.dist );
				if(dist > FLT_EPSILON)
					interp_v3_v3v3(tmp_co, tmp_co, nearest.co, (dist - calc->keepDist)/dist);	//linear interpolation
				else
					VECCOPY( tmp_co, nearest.co );
			}

			//Convert the coordinates back to mesh coordinates
			space_transform_invert(&calc->local2target, tmp_co);
			interp_v3_v3v3(co, co, tmp_co, weight);	//linear interpolation
		}
	}

	free_bvhtree_from_mesh(&treeData);
}

/* Main shrinkwrap function */
void shrinkwrapModifier_deform(ShrinkwrapModifierData *smd, Scene *scene, Object *ob, DerivedMesh *dm, float (*vertexCos)[3], int numVerts)
{

	DerivedMesh *ss_mesh	= NULL;
	ShrinkwrapCalcData calc = NULL_ShrinkwrapCalcData;

	//remove loop dependencies on derived meshs (TODO should this be done elsewhere?)
	if(smd->target == ob) smd->target = NULL;
	if(smd->auxTarget == ob) smd->auxTarget = NULL;


	//Configure Shrinkwrap calc data
	calc.smd = smd;
	calc.ob = ob;
	calc.numVerts = numVerts;
	calc.vertexCos = vertexCos;

	//DeformVertex
	calc.vgroup = defgroup_name_index(calc.ob, calc.smd->vgroup_name);
	if(dm)
	{
		calc.dvert = dm->getVertDataArray(dm, CD_MDEFORMVERT);
	}
	else if(calc.ob->type == OB_LATTICE)
	{
		calc.dvert = lattice_get_deform_verts(calc.ob);
	}


	if(smd->target)
	{
		calc.target = object_get_derived_final(scene, smd->target, CD_MASK_BAREMESH);

		//TODO there might be several "bugs" on non-uniform scales matrixs
		//because it will no longer be nearest surface, not sphere projection
		//because space has been deformed
		space_transform_setup(&calc.local2target, ob, smd->target);

		//TODO: smd->keepDist is in global units.. must change to local
		calc.keepDist = smd->keepDist;
	}



	calc.vgroup = defgroup_name_index(calc.ob, smd->vgroup_name);

	if(dm != NULL && smd->shrinkType == MOD_SHRINKWRAP_PROJECT)
	{
		//Setup arrays to get vertexs positions, normals and deform weights
		calc.vert   = dm->getVertDataArray(dm, CD_MVERT);
		calc.dvert  = dm->getVertDataArray(dm, CD_MDEFORMVERT);

		//Using vertexs positions/normals as if a subsurface was applied 
		if(smd->subsurfLevels)
		{
			SubsurfModifierData ssmd;
			memset(&ssmd, 0, sizeof(ssmd));
			ssmd.subdivType	= ME_CC_SUBSURF;		//catmull clark
			ssmd.levels		= smd->subsurfLevels;	//levels

			ss_mesh = subsurf_make_derived_from_derived(dm, &ssmd, FALSE, NULL, 0, 0);

			if(ss_mesh)
			{
				calc.vert = ss_mesh->getVertDataArray(ss_mesh, CD_MVERT);
				if(calc.vert)
				{
					//TRICKY: this code assumes subsurface will have the transformed original vertices
					//in their original order at the end of the vert array.
					calc.vert = calc.vert + ss_mesh->getNumVerts(ss_mesh) - dm->getNumVerts(dm);
				}
			}

			//Just to make sure we are not leaving any memory behind
			assert(ssmd.emCache == NULL);
			assert(ssmd.mCache == NULL);
		}
	}

	//Projecting target defined - lets work!
	if(calc.target)
	{
		switch(smd->shrinkType)
		{
			case MOD_SHRINKWRAP_NEAREST_SURFACE:
				BENCH(shrinkwrap_calc_nearest_surface_point(&calc));
			break;

			case MOD_SHRINKWRAP_PROJECT:
				BENCH(shrinkwrap_calc_normal_projection(&calc, scene));
			break;

			case MOD_SHRINKWRAP_NEAREST_VERTEX:
				BENCH(shrinkwrap_calc_nearest_vertex(&calc));
			break;
		}
	}

	//free memory
	if(ss_mesh)
		ss_mesh->release(ss_mesh);
}

