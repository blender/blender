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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <string.h>
#include <float.h>
#include <math.h>
#include <stdio.h>
#include <time.h>
#include <assert.h>
//TODO: its late and I don't fill like adding ifs() printfs (I'll remove them on end)

#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_shrinkwrap.h"
#include "BKE_DerivedMesh.h"
#include "BKE_utildefines.h"
#include "BKE_deform.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_global.h"

#include "BLI_arithb.h"
#include "BLI_kdtree.h"

#include "RE_raytrace.h"

#define TO_STR(a)	#a
#define JOIN(a,b)	a##b

#define OUT_OF_MEMORY()	((void)printf("Shrinkwrap: Out of memory\n"))

/* Benchmark macros */
#if 1

#define BENCH(a)	\
	do {			\
		clock_t _clock_init = clock();	\
		(a);							\
		printf("%s: %fms\n", #a, (float)(clock()-_clock_init)*1000/CLOCKS_PER_SEC);	\
	} while(0)

#define BENCH_VAR(name)		clock_t JOIN(_bench_step,name) = 0, JOIN(_bench_total,name) = 0
#define BENCH_BEGIN(name)	JOIN(_bench_step, name) = clock()
#define BENCH_END(name)		JOIN(_bench_total,name) += clock() - JOIN(_bench_step,name)
#define BENCH_RESET(name)	JOIN(_bench_total, name) = 0
#define BENCH_REPORT(name)	printf("%s: %fms\n", TO_STR(name), JOIN(_bench_total,name)*1000.0f/CLOCKS_PER_SEC)

#else

#define BENCH(a)	(a)
#define BENCH_VAR(name)
#define BENCH_BEGIN(name)
#define BENCH_END(name)
#define BENCH_RESET(name)
#define BENCH_REPORT(name)

#endif

#define CONST
typedef void ( *Shrinkwrap_ForeachVertexCallback) (DerivedMesh *target, float *co, float *normal);


static void normal_short2float(const short *ns, float *nf)
{
	nf[0] = ns[0] / 32767.0f;
	nf[1] = ns[1] / 32767.0f;
	nf[2] = ns[2] / 32767.0f;
}

static float vertexgroup_get_weight(MDeformVert *dvert, int index, int vgroup)
{
	if(dvert && vgroup >= 0)
	{
		int j;
		for(j = 0; j < dvert[index].totweight; j++)
			if(dvert[index].dw[j].def_nr == vgroup)
				return dvert[index].dw[j].weight;
	}
	return -1;
}

/*
 * Raytree from mesh
 */
static MVert *raytree_from_mesh_verts = NULL;
static float raytree_from_mesh_start[3] = { 1e10f, 1e10f, 1e10f }; 
static int raytree_check_always(Isect *is, int ob, RayFace *face)
{
	return TRUE;
}

static void raytree_from_mesh_get_coords(RayFace *face, float **v1, float **v2, float **v3, float **v4)
{
	MFace *mface= (MFace*)face;

	if(mface == NULL)
	{
		*v1 = raytree_from_mesh_start;
		*v2 = raytree_from_mesh_start;
		*v3 = raytree_from_mesh_start;
		*v4 = NULL;
		return;
	}

	*v1= raytree_from_mesh_verts[mface->v1].co;
	*v2= raytree_from_mesh_verts[mface->v2].co;
	*v3= raytree_from_mesh_verts[mface->v3].co;
	*v4= (mface->v4)? raytree_from_mesh_verts[mface->v4].co: NULL;
}

/*
 * Creates a raytree from the given mesh
 * No copy of the mesh is done, so it must exist and remain
 * imutable as long the tree is intended to be used
 *
 * No more than 1 raytree can exist.. since this code uses a static variable
 * to pass data to raytree_from_mesh_get_coords
 */
static RayTree* raytree_create_from_mesh(DerivedMesh *mesh)
{
	int i;
	float min[3], max[3];

	RayTree*tree= NULL;

	int numFaces= mesh->getNumFaces(mesh);
	MFace *face = mesh->getFaceDataArray(mesh, CD_MFACE);
	int numVerts= mesh->getNumVerts(mesh);
	raytree_from_mesh_verts = mesh->getVertDataArray(mesh, CD_MVERT);


	//calculate bounding box
	INIT_MINMAX(min, max);

	for(i=0; i<numVerts; i++)
		DO_MINMAX(raytree_from_mesh_verts[i].co, min, max);
	
	tree = RE_ray_tree_create(64, numFaces, min, max, raytree_from_mesh_get_coords, raytree_check_always, NULL, NULL);
	if(tree == NULL)
		return NULL;

	//Add faces to the RayTree
	for(i=0; i<numFaces; i++)
		RE_ray_tree_add_face(tree, 0, (RayFace*)(face+i));

	RE_ray_tree_done(tree);

	return tree;
}

static void free_raytree_from_mesh(RayTree *tree)
{
	raytree_from_mesh_verts = NULL;
	RE_ray_tree_free(tree);
}

/*
 * Cast a ray on the specified direction
 * Returns the distance the ray must travel until intersect something
 * Returns FLT_MAX in case of nothing intersection
 */
static float raytree_cast_ray(RayTree *tree, const float *coord, const float *direction)
{
	Isect isec = {};

	//Setup intersection
	isec.mode		= RE_RAY_MIRROR; //We want closest intersection
	isec.lay		= -1;
	isec.face_last	= NULL;
	isec.faceorig	= NULL;
	isec.labda		= 1e10f;

	VECCOPY(isec.start, coord);
	VECCOPY(isec.vec, direction);
	VECADDFAC(isec.end, isec.start, isec.vec, isec.labda);

	if(!RE_ray_tree_intersect(tree, &isec))
		return FLT_MAX;

	isec.labda = ABS(isec.labda);
	VECADDFAC(isec.end, isec.start, isec.vec, isec.labda);
	return VecLenf(coord, isec.end);
}

/*
 * This calculates the distance (in dir units) that the ray must travel to intersect plane
 * It can return negative values
 *
 * TODO theres probably something like this on blender code
 *
 * Returns FLT_MIN in parallel case
 */
static float ray_intersect_plane(CONST float *point, CONST float *dir, CONST float *plane_point, CONST float *plane_normal)
{
		float pp[3];
		float a, pp_dist;

		a = INPR(dir, plane_normal);

		if(fabs(a) < 1e-5f) return FLT_MIN;

		VecSubf(pp, point, plane_point);
		pp_dist = INPR(pp, plane_normal);

		return -pp_dist/a;
}

/*
 * Returns the minimum distance between the point and a triangle surface
 * Writes the nearest surface point in the given nearest
 */
static float nearest_point_in_tri_surface(CONST float *co, CONST float *v0, CONST float *v1, CONST float *v2, float *nearest)
{
	//TODO: make this efficient (probably this can be made with something like 3 point_in_slice())
	if(point_in_tri_prism(co, v0, v1, v2))
	{
		float normal[3];
		float dist;

		CalcNormFloat(v0, v1, v2, normal);
		dist = ray_intersect_plane(co, normal, v0, normal);

		VECADDFAC(nearest, co, normal, dist);
		return fabs(dist);
	}
	else
	{
		float dist = FLT_MAX, tdist;
		float closest[3];

		PclosestVL3Dfl(closest, co, v0, v1);
		tdist = VecLenf(co, closest);
		if(tdist < dist)
		{
			dist = tdist;
			VECCOPY(nearest, closest);
		}

		PclosestVL3Dfl(closest, co, v1, v2);
		tdist = VecLenf(co, closest);
		if(tdist < dist)
		{
			dist = tdist;
			VECCOPY(nearest, closest);
		}

		PclosestVL3Dfl(closest, co, v2, v0);
		tdist = VecLenf(co, closest);
		if(tdist < dist)
		{
			dist = tdist;
			VECCOPY(nearest, closest);
		}

		return dist;
	}
}



/*
 * Shrink to nearest surface point on target mesh
 */
static void bruteforce_shrinkwrap_calc_nearest_surface_point(DerivedMesh *target, float *co, float *unused)
{
	//TODO: this should use raycast code probably existent in blender
	float minDist = FLT_MAX;
	float orig_co[3];

	int i;
	int	numFaces = target->getNumFaces(target);
	MVert *vert = target->getVertDataArray(target, CD_MVERT);
	MFace *face = target->getFaceDataArray(target, CD_MFACE);

	VECCOPY(orig_co, co);

	for (i = 0; i < numFaces; i++)
	{
		float *v0, *v1, *v2, *v3;

		v0 = vert[ face[i].v1 ].co;
		v1 = vert[ face[i].v2 ].co;
		v2 = vert[ face[i].v3 ].co;
		v3 = face[i].v4 ? vert[ face[i].v4 ].co : 0;

		while(v2)
		{
			float dist;
			float tmp[3];

			dist = nearest_point_in_tri_surface(orig_co, v0, v1, v2, tmp);

			if(dist < minDist)
			{
				minDist = dist;
				VECCOPY(co, tmp);
			}

			v1 = v2;
			v2 = v3;
			v3 = 0;
		}
	}
}

/*
 * Projects the vertex on the normal direction over the target mesh
 */
static void bruteforce_shrinkwrap_calc_normal_projection(DerivedMesh *target, float *co, float *vnormal)
{
	//TODO: this should use raycast code probably existent in blender
	float minDist = FLT_MAX;
	float orig_co[3];

	int i;
	int	numFaces = target->getNumFaces(target);
	MVert *vert = target->getVertDataArray(target, CD_MVERT);
	MFace *face = target->getFaceDataArray(target, CD_MFACE);

	VECCOPY(orig_co, co);

	for (i = 0; i < numFaces; i++)
	{
		float *v0, *v1, *v2, *v3;

		v0 = vert[ face[i].v1 ].co;
		v1 = vert[ face[i].v2 ].co;
		v2 = vert[ face[i].v3 ].co;
		v3 = face[i].v4 ? vert[ face[i].v4 ].co : 0;

		while(v2)
		{
			float dist;
			float pnormal[3];

			CalcNormFloat(v0, v1, v2, pnormal);
			dist =  ray_intersect_plane(orig_co, vnormal, v0, pnormal);

			if(fabs(dist) < minDist)
			{
				float tmp[3], nearest[3];
				VECADDFAC(tmp, orig_co, vnormal, dist);

				if( fabs(nearest_point_in_tri_surface(tmp, v0, v1, v2, nearest)) < 0.0001)
				{
					minDist = fabs(dist);
					VECCOPY(co, nearest);
				}
			}
			v1 = v2;
			v2 = v3;
			v3 = 0;
		}
	}
}

/*
 * Shrink to nearest vertex on target mesh
 */
static void bruteforce_shrinkwrap_calc_nearest_vertex(DerivedMesh *target, float *co, float *unused)
{
	float minDist = FLT_MAX;
	float orig_co[3];

	int i;
	int	numVerts = target->getNumVerts(target);
	MVert *vert = target->getVertDataArray(target, CD_MVERT);

	VECCOPY(orig_co, co);

	for (i = 0; i < numVerts; i++)
	{
		float diff[3], sdist;
		VECSUB(diff, orig_co, vert[i].co);
		sdist = INPR(diff, diff);
		
		if(sdist < minDist)
		{
			minDist = sdist;
			VECCOPY(co, vert[i].co);
		}
	}
}


static void shrinkwrap_calc_foreach_vertex(ShrinkwrapCalcData *calc, Shrinkwrap_ForeachVertexCallback callback)
{
	int i;
	int vgroup		= get_named_vertexgroup_num(calc->ob, calc->smd->vgroup_name);
	int	numVerts	= 0;

	MDeformVert *dvert = NULL;
	MVert		*vert  = NULL;

	numVerts = calc->final->getNumVerts(calc->final);
	dvert = calc->final->getVertDataArray(calc->final, CD_MDEFORMVERT);
	vert  = calc->final->getVertDataArray(calc->final, CD_MVERT);

	//Shrink (calculate each vertex final position)
	for(i = 0; i<numVerts; i++)
	{
		float weight = vertexgroup_get_weight(dvert, i, vgroup);

		float orig[3], final[3]; //Coords relative to target
		float normal[3];

		if(weight == 0.0f) continue;	//Skip vertexs where we have no influence

		VecMat4MulVecfl(orig, calc->local2target, vert[i].co);
		VECCOPY(final, orig);

		//We also need to apply the rotation to normal
		if(calc->smd->shrinkType == MOD_SHRINKWRAP_NORMAL)
		{
			normal_short2float(vert[i].no, normal);
			Mat4Mul3Vecfl(calc->local2target, normal);
			Normalize(normal);	//Watch out for scaling (TODO: do we really needed a unit-len normal?)
		}
		(callback)(calc->target, final, normal);

		VecLerpf(final, orig, final, weight);	//linear interpolation

		VecMat4MulVecfl(vert[i].co, calc->target2local, final);
	}
}

/* Main shrinkwrap function */
DerivedMesh *shrinkwrapModifier_do(ShrinkwrapModifierData *smd, Object *ob, DerivedMesh *dm, int useRenderParams, int isFinalCalc)
{

	ShrinkwrapCalcData calc = {};


	//Init Shrinkwrap calc data
	calc.smd = smd;

	calc.ob = ob;
	calc.original = dm;
	calc.final = CDDM_copy(calc.original);

	if(!calc.final)
	{
		OUT_OF_MEMORY();
		return dm;
	}

	if(smd->target)
	{
		calc.target = (DerivedMesh *)smd->target->derivedFinal;

		if(!calc.target)
		{
			printf("Target derived mesh is null! :S\n");
		}

		//TODO should we reduce the number of matrix mults? by choosing applying matrixs to target or to derived mesh?
		//Calculate matrixs for local <-> target
		Mat4Invert (smd->target->imat, smd->target->obmat);	//inverse is outdated
		Mat4MulSerie(calc.local2target, smd->target->imat, ob->obmat, 0, 0, 0, 0, 0, 0);
		Mat4Invert(calc.target2local, calc.local2target);

	}

	calc.moved = NULL;


	//Projecting target defined - lets work!
	if(calc.target)
	{
		printf("Shrinkwrap (%s)%d over (%s)%d\n",
			calc.ob->id.name,			calc.final->getNumVerts(calc.final),
			calc.smd->target->id.name,	calc.target->getNumVerts(calc.target)
		);

		switch(smd->shrinkType)
		{
			case MOD_SHRINKWRAP_NEAREST_SURFACE:
				BENCH(shrinkwrap_calc_foreach_vertex(&calc, bruteforce_shrinkwrap_calc_nearest_surface_point));
			break;

			case MOD_SHRINKWRAP_NORMAL:
				BENCH(shrinkwrap_calc_normal_projection(&calc));
//				BENCH(shrinkwrap_calc_foreach_vertex(&calc, bruteforce_shrinkwrap_calc_normal_projection));
			break;

			case MOD_SHRINKWRAP_NEAREST_VERTEX:
				BENCH(shrinkwrap_calc_nearest_vertex(&calc));
//				BENCH(shrinkwrap_calc_foreach_vertex(&calc, bruteforce_shrinkwrap_calc_nearest_vertex));
			break;
		}

	}

	//Destroy faces, edges and stuff
	if(calc.moved)
	{
		//TODO
	}

	CDDM_calc_normals(calc.final);	

	return calc.final;
}


/*
 * Shrinkwrap to the nearest vertex
 *
 * it builds a kdtree of vertexs we can attach to and then
 * for each vertex on performs a nearest vertex search on the tree
 */
void shrinkwrap_calc_nearest_vertex(ShrinkwrapCalcData *calc)
{
	int i;
	int vgroup		= get_named_vertexgroup_num(calc->ob, calc->smd->vgroup_name);

	KDTree* target = NULL;
	KDTreeNearest nearest;
	float tmp_co[3];

	BENCH_VAR(build);
	BENCH_VAR(query);

	int	numVerts;
	MVert *vert = NULL;
	MDeformVert *dvert = NULL;

	//Generate kd-tree with target vertexs
	BENCH_BEGIN(build);

	target = BLI_kdtree_new(calc->target->getNumVerts(calc->target));
	if(target == NULL) return OUT_OF_MEMORY();

	numVerts= calc->target->getNumVerts(calc->target);
	vert	= calc->target->getVertDataArray(calc->target, CD_MVERT);	


	for( ;numVerts--; vert++)
		BLI_kdtree_insert(target, 0, vert->co, NULL);

	BLI_kdtree_balance(target);

	BENCH_END(build);


	//Find the nearest vertex 
	numVerts= calc->final->getNumVerts(calc->final);
	vert	= calc->final->getVertDataArray(calc->final, CD_MVERT);	
	dvert	= calc->final->getVertDataArray(calc->final, CD_MDEFORMVERT);

	for(i=0; i<numVerts; i++)
	{
		int t;
		float weight = vertexgroup_get_weight(dvert, i, vgroup);
		if(weight == 0.0f) continue;

		VecMat4MulVecfl(tmp_co, calc->local2target, vert[i].co);

		BENCH_BEGIN(query);
		t = BLI_kdtree_find_nearest(target, tmp_co, 0, &nearest);
		BENCH_END(query);

		if(t != -1)
		{
			float weight = 1.0f;

			VecMat4MulVecfl(nearest.co, calc->target2local, nearest.co);
			VecLerpf(vert[i].co, vert[i].co, nearest.co, weight);	//linear interpolation

			if(calc->moved)
				calc->moved[i] = TRUE;
		}
	}

	BENCH_BEGIN(build);
	BLI_kdtree_free(target);
	BENCH_END(build);


	BENCH_REPORT(build);
	BENCH_REPORT(query);
}

/*
 * Shrinkwrap projecting vertexs allong their normals over the target
 *
 * it builds a RayTree from the target mesh and then performs a
 * raycast for each vertex (ray direction = normal)
 */
void shrinkwrap_calc_normal_projection(ShrinkwrapCalcData *calc)
{
	int i;
	int vgroup		= get_named_vertexgroup_num(calc->ob, calc->smd->vgroup_name);
	char use_normal = calc->smd->shrinkOpts;
	RayTree *target = NULL;

	int	numVerts;
	MVert *vert = NULL;
	MDeformVert *dvert = NULL;
	float tmp_co[3], tmp_no[3];

	if( (use_normal & (MOD_SHRINKWRAP_ALLOW_INVERTED_NORMAL | MOD_SHRINKWRAP_ALLOW_DEFAULT_NORMAL)) == 0)
		return;	//Nothing todo

	//setup raytracing
	target = raytree_create_from_mesh(calc->target);
	if(target == NULL) return OUT_OF_MEMORY();



	//Project each vertex along normal
	numVerts= calc->final->getNumVerts(calc->final);
	vert	= calc->final->getVertDataArray(calc->final, CD_MVERT);	
	dvert	= calc->final->getVertDataArray(calc->final, CD_MDEFORMVERT);

	for(i=0; i<numVerts; i++)
	{
		float dist = FLT_MAX;
		float weight = vertexgroup_get_weight(dvert, i, vgroup);
//		if(weight == 0.0f) continue;
		weight = 1.0;

		//Transform coordinates local->target
		VecMat4MulVecfl(tmp_co, calc->local2target, vert[i].co);

		normal_short2float(vert[i].no, tmp_no);
		Mat4Mul3Vecfl(calc->local2target, tmp_no);	//Watch out for scaling on normal
		Normalize(tmp_no);							//(TODO: do we really needed a unit-len normal? and we could know the scale factor before hand?)


		if(use_normal & MOD_SHRINKWRAP_ALLOW_DEFAULT_NORMAL)
		{
			dist = raytree_cast_ray(target, tmp_co, tmp_no);
		}

		normal_short2float(vert[i].no, tmp_no);
		Mat4Mul3Vecfl(calc->local2target, tmp_no);	//Watch out for scaling on normal
		Normalize(tmp_no);							//(TODO: do we really needed a unit-len normal? and we could know the scale factor before hand?)

		if(use_normal & MOD_SHRINKWRAP_ALLOW_INVERTED_NORMAL)
		{
			float inv[3]; // = {-tmp_no[0], -tmp_no[1], -tmp_no[2]};
			float tdist;

			inv[0] = -tmp_no[0];
			inv[1] = -tmp_no[1];
			inv[2] = -tmp_no[2];

			tdist = raytree_cast_ray(target, tmp_co, inv);

			if(ABS(tdist) < ABS(dist))
				dist = -tdist;
		}

		if(ABS(dist) != FLT_MAX)
		{
			VECADDFAC(tmp_co, tmp_co, tmp_no, dist);
			VecMat4MulVecfl(tmp_co, calc->target2local, tmp_co);
			VecLerpf(vert[i].co, vert[i].co, tmp_co, weight);	//linear interpolation

			if(calc->moved)
				calc->moved[i] = TRUE;
		}

	}

	free_raytree_from_mesh(target);
}


