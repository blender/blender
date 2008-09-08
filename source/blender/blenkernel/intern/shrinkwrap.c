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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
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

#include "BKE_shrinkwrap.h"
#include "BKE_DerivedMesh.h"
#include "BKE_utildefines.h"
#include "BKE_deform.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_subsurf.h"

#include "BLI_arithb.h"
#include "BLI_kdtree.h"
#include "BLI_kdopbvh.h"

#include "RE_raytrace.h"
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
DerivedMesh *object_get_derived_final(Object *ob, CustomDataMask dataMask)
{
	if (ob==G.obedit)
	{
		DerivedMesh *final = NULL;
		editmesh_get_derived_cage_and_final(&final, dataMask);
		return final;
	}
	else
		return mesh_get_derived_final(ob, dataMask);
}

/* Space transform */
void space_transform_from_matrixs(SpaceTransform *data, float local[4][4], float target[4][4])
{
	float itarget[4][4];
	Mat4Invert(itarget, target);
	Mat4MulSerie(data->local2target, itarget, local, 0, 0, 0, 0, 0, 0);
	Mat4Invert(data->target2local, data->local2target);
}

void space_transform_apply(const SpaceTransform *data, float *co)
{
	VecMat4MulVecfl(co, ((SpaceTransform*)data)->local2target, co);
}

void space_transform_invert(const SpaceTransform *data, float *co)
{
	VecMat4MulVecfl(co, ((SpaceTransform*)data)->target2local, co);
}

void space_transform_apply_normal(const SpaceTransform *data, float *no)
{
	Mat4Mul3Vecfl( ((SpaceTransform*)data)->local2target, no);
	Normalize(no); // TODO: could we just determine de scale value from the matrix?
}

void space_transform_invert_normal(const SpaceTransform *data, float *no)
{
	Mat4Mul3Vecfl(((SpaceTransform*)data)->target2local, no);
	Normalize(no); // TODO: could we just determine de scale value from the matrix?
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

/* Main shrinkwrap function */
void shrinkwrapModifier_deform(ShrinkwrapModifierData *smd, Object *ob, DerivedMesh *dm, float (*vertexCos)[3], int numVerts)
{

	ShrinkwrapCalcData calc = NULL_ShrinkwrapCalcData;

	//remove loop dependencies on derived meshs (TODO should this be done elsewhere?)
	if(smd->target == ob) smd->target = NULL;
	if(smd->auxTarget == ob) smd->auxTarget = NULL;


	//Configure Shrinkwrap calc data
	calc.smd = smd;
	calc.ob = ob;
	calc.original = dm;
	calc.numVerts = numVerts;
	calc.vertexCos = vertexCos;

	if(smd->target)
	{
		//TODO currently we need a copy in case object_get_derived_final returns an emDM that does not defines getVertArray or getFace array
		calc.target = CDDM_copy( object_get_derived_final(smd->target, CD_MASK_BAREMESH) );

		//TODO there might be several "bugs" on non-uniform scales matrixs.. because it will no longer be nearest surface, not sphere projection
		//because space has been deformed
		space_transform_setup(&calc.local2target, ob, smd->target);

		calc.keepDist = smd->keepDist;	//TODO: smd->keepDist is in global units.. must change to local
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
				BENCH(shrinkwrap_calc_normal_projection(&calc));
			break;

			case MOD_SHRINKWRAP_NEAREST_VERTEX:
				BENCH(shrinkwrap_calc_nearest_vertex(&calc));
			break;
		}
	}

	//free memory
	if(calc.target)
		calc.target->release( calc.target );
}

/*
 * Shrinkwrap to the nearest vertex
 *
 * it builds a kdtree of vertexs we can attach to and then
 * for each vertex performs a nearest vertex search on the tree
 */
void shrinkwrap_calc_nearest_vertex(ShrinkwrapCalcData *calc)
{
	int i;
	const int vgroup		 = get_named_vertexgroup_num(calc->ob, calc->smd->vgroup_name);
	MDeformVert *const dvert = calc->original ? calc->original->getVertDataArray(calc->original, CD_MDEFORMVERT) : NULL;

	BVHTreeFromMesh treeData = NULL_BVHTreeFromMesh;
	BVHTreeNearest  nearest  = NULL_BVHTreeNearest;


	BENCH(bvhtree_from_mesh_verts(&treeData, calc->target, 0.0, 2, 6));
	if(treeData.tree == NULL) return OUT_OF_MEMORY();

	//Setup nearest
	nearest.index = -1;
	nearest.dist = FLT_MAX;

#pragma omp parallel for default(none) private(i) firstprivate(nearest) shared(treeData,calc) schedule(static)
	for(i = 0; i<calc->numVerts; ++i)
	{
		float *co = calc->vertexCos[i];
		float tmp_co[3];
		float weight = vertexgroup_get_vertex_weight(dvert, i, vgroup);
		if(weight == 0.0f) continue;

		VECCOPY(tmp_co, co);
		space_transform_apply(&calc->local2target, tmp_co); //Convert the coordinates to the tree coordinates

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

			VecLerpf(co, co, tmp_co, weight);	//linear interpolation
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

		hit_tmp.dist *= Mat4ToScalef( ((SpaceTransform*)transf)->local2target );
	}
	else
	{
		co = vert;
		no = dir;
	}

	hit_tmp.index = -1;

	BLI_bvhtree_ray_cast(tree, co, no, &hit_tmp, callback, userdata);

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

			hit_tmp.dist = VecLenf( (float*)vert, hit_tmp.co );
		}

		memcpy(hit, &hit_tmp, sizeof(hit_tmp) );
		return TRUE;
	}
	return FALSE;
}


void shrinkwrap_calc_normal_projection(ShrinkwrapCalcData *calc)
{
	int i;

	//Options about projection direction
	const char use_normal    = calc->smd->shrinkOpts;
	float proj_axis[3] = {0.0f, 0.0f, 0.0f};
	MVert *vert  = NULL; //Needed in case of vertex normal
	DerivedMesh* ss_mesh = NULL;

	//Vertex group data
	const int vgroup		   = get_named_vertexgroup_num(calc->ob, calc->smd->vgroup_name);
	const MDeformVert *dvert = calc->original ? calc->original->getVertDataArray(calc->original, CD_MDEFORMVERT) : NULL;


	//Raycast and tree stuff
	BVHTreeRayHit hit;
	BVHTreeFromMesh treeData = NULL_BVHTreeFromMesh; 	//target

	//auxiliar target
	DerivedMesh * aux_mesh = NULL;
	BVHTreeFromMesh auxData= NULL_BVHTreeFromMesh;
	SpaceTransform local2aux;

do
{

	//Prepare data to retrieve the direction in which we should project each vertex
	if(calc->smd->projAxis == MOD_SHRINKWRAP_PROJECT_OVER_NORMAL)
	{
		//No Mvert information: jump to "free memory and return" part
		if(calc->original == NULL) break;

		if(calc->smd->subsurfLevels)
		{
			SubsurfModifierData smd;
			memset(&smd, 0, sizeof(smd));
			smd.subdivType = ME_CC_SUBSURF;			//catmull clark
			smd.levels = calc->smd->subsurfLevels;	//levels

			ss_mesh = subsurf_make_derived_from_derived(calc->original, &smd, FALSE, NULL, 0, 0);

			if(ss_mesh)
			{
				vert = ss_mesh->getVertDataArray(ss_mesh, CD_MVERT);
				if(vert)
				{
					//TRICKY: this code assumes subsurface will have the transformed original vertices
					//in their original order at the end of the vert array.
					vert = vert
						 + ss_mesh->getNumVerts(ss_mesh)
						 - calc->original->getNumVerts(calc->original);
				}
			}

			//To make sure we are not letting any memory behind
			assert(smd.emCache == NULL);
			assert(smd.mCache == NULL);
		}
		else
			vert = calc->original->getVertDataArray(calc->original, CD_MVERT);

		//Not able to get vert information: jump to "free memory and return" part
		if(vert == NULL) break;
	}
	else
	{
		//The code supports any axis that is a combination of X,Y,Z.. altought currently UI only allows to set the 3 diferent axis
		if(calc->smd->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_X_AXIS) proj_axis[0] = 1.0f;
		if(calc->smd->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_Y_AXIS) proj_axis[1] = 1.0f;
		if(calc->smd->projAxis & MOD_SHRINKWRAP_PROJECT_OVER_Z_AXIS) proj_axis[2] = 1.0f;

		Normalize(proj_axis);

		//Invalid projection direction: jump to "free memory and return" part
		if(INPR(proj_axis, proj_axis) < FLT_EPSILON) break; 
	}

	//If the user doesn't allows to project in any direction of projection axis... then theres nothing todo.
	if((use_normal & (MOD_SHRINKWRAP_PROJECT_ALLOW_POS_DIR | MOD_SHRINKWRAP_PROJECT_ALLOW_NEG_DIR)) == 0)
		break; //jump to "free memory and return" part


	//Build target tree
	BENCH(bvhtree_from_mesh_faces(&treeData, calc->target, calc->keepDist, 4, 6));
	if(treeData.tree == NULL)
		break; //jump to "free memory and return" part


	//Build auxiliar target
	if(calc->smd->auxTarget)
	{
		space_transform_setup( &local2aux, calc->ob, calc->smd->auxTarget);

		aux_mesh = CDDM_copy( object_get_derived_final(calc->smd->auxTarget, CD_MASK_BAREMESH) ); 		//TODO currently we need a copy in case object_get_derived_final returns an emDM that does not defines getVertArray or getFace array
		if(aux_mesh)
			BENCH(bvhtree_from_mesh_faces(&auxData, aux_mesh, 0.0, 4, 6));
		else
			printf("Auxiliar target finalDerived mesh is null\n");
	}


	//Now, everything is ready to project the vertexs!
#pragma omp parallel for private(i,hit) schedule(static)
	for(i = 0; i<calc->numVerts; ++i)
	{
		float *co = calc->vertexCos[i];
		float tmp_co[3], tmp_no[3];
		float lim = 10000.0f; //TODO: we should use FLT_MAX here, but sweepsphere code isnt prepared for that
		float weight = vertexgroup_get_vertex_weight(dvert, i, vgroup);

		if(weight == 0.0f) continue;

		if(ss_mesh)
		{
			VECCOPY(tmp_co, vert[i].co);
		}
		else
		{
			VECCOPY(tmp_co, co);
		}


		if(vert)
			NormalShortToFloat(tmp_no, vert[i].no);
		else
			VECCOPY( tmp_no, proj_axis );


		hit.index = -1;
		hit.dist = lim;


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
			VecLerpf(co, co, hit.co, weight);
		}
	}


//Simple do{} while(0) structure to allow to easily jump to the "free memory and return" part
} while(0);

	//free data structures

	free_bvhtree_from_mesh(&treeData);
	free_bvhtree_from_mesh(&auxData);

	if(aux_mesh)
		aux_mesh->release(aux_mesh);

	if(ss_mesh)
		ss_mesh->release(ss_mesh);
}

/*
 * Shrinkwrap moving vertexs to the nearest surface point on the target
 *
 * it builds a BVHTree from the target mesh and then performs a
 * NN matchs for each vertex
 */
void shrinkwrap_calc_nearest_surface_point(ShrinkwrapCalcData *calc)
{
	int i;

	const int vgroup = get_named_vertexgroup_num(calc->ob, calc->smd->vgroup_name);
	const MDeformVert *const dvert = calc->original ? calc->original->getVertDataArray(calc->original, CD_MDEFORMVERT) : NULL;

	BVHTreeFromMesh treeData = NULL_BVHTreeFromMesh;
	BVHTreeNearest  nearest  = NULL_BVHTreeNearest;



	//Create a bvh-tree of the given target
	BENCH(bvhtree_from_mesh_faces( &treeData, calc->target, 0.0, 2, 6));
	if(treeData.tree == NULL) return OUT_OF_MEMORY();

	//Setup nearest
	nearest.index = -1;
	nearest.dist = FLT_MAX;


	//Find the nearest vertex
#pragma omp parallel for default(none) private(i) firstprivate(nearest) shared(calc,treeData) schedule(static)
	for(i = 0; i<calc->numVerts; ++i)
	{
		float *co = calc->vertexCos[i];
		float tmp_co[3];
		float weight = vertexgroup_get_vertex_weight(dvert, i, vgroup);
		if(weight == 0.0f) continue;

		//Convert the vertex to tree coordinates
		VECCOPY(tmp_co, co);
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
					VecLerpf(tmp_co, tmp_co, nearest.co, (dist - calc->keepDist)/dist);	//linear interpolation
				else
					VECCOPY( tmp_co, nearest.co );
			}

			//Convert the coordinates back to mesh coordinates
			space_transform_invert(&calc->local2target, tmp_co);
			VecLerpf(co, co, tmp_co, weight);	//linear interpolation
		}
	}


	free_bvhtree_from_mesh(&treeData);
}

