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
#include "BLI_kdopbvh.h"

#include "RE_raytrace.h"
#include "MEM_guardedalloc.h"


/* Util macros */
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
	return 1.0;
}

/*
 * Raytree from mesh
 */
static MVert *raytree_from_mesh_verts = NULL;
static MFace *raytree_from_mesh_faces = NULL;
//static float raytree_from_mesh_start[3] = { 0.0f, 0.0f, 0.0f }; 
static int raytree_check_always(Isect *is, int ob, RayFace *face)
{
	return TRUE;
}

static void raytree_from_mesh_get_coords(RayFace *face, float **v1, float **v2, float **v3, float **v4)
{
	MFace *mface= raytree_from_mesh_faces + (int)face/2 - 1 ;

	if(face == (RayFace*)(-1))
	{
		*v1 = NULL; //raytree_from_mesh_start;
		*v2 = NULL; //raytree_from_mesh_start;
		*v3 = NULL; //raytree_from_mesh_start;
		*v4 = NULL;
		return;
	}

	//Nasty quad splitting
	if(((int)face) & 1)	//we want the 2 triangle of the quad
	{
		assert(mface->v4);
		*v1= raytree_from_mesh_verts[mface->v1].co;
		*v2= raytree_from_mesh_verts[mface->v4].co;
		*v3= raytree_from_mesh_verts[mface->v3].co;
		*v4= NULL;
	}
	else
	{
		*v1= raytree_from_mesh_verts[mface->v1].co;
		*v2= raytree_from_mesh_verts[mface->v2].co;
		*v3= raytree_from_mesh_verts[mface->v3].co;
		*v4= NULL;
	}
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

	//Initialize static vars
	raytree_from_mesh_verts = mesh->getVertDataArray(mesh, CD_MVERT);
	raytree_from_mesh_faces = face;


	//calculate bounding box
	INIT_MINMAX(min, max);

	for(i=0; i<numVerts; i++)
		DO_MINMAX(raytree_from_mesh_verts[i].co, min, max);
	
	tree = RE_ray_tree_create(64, numFaces, min, max, raytree_from_mesh_get_coords, raytree_check_always, NULL, NULL);
	if(tree == NULL)
		return NULL;

	//Add faces to the RayTree (RayTree uses face=0, with some special value to setup things)
	for(i=1; i<=numFaces; i++)
	{
		RE_ray_tree_add_face(tree, 0, (RayFace*)(i*2) );

		//Theres some nasty thing with non-coplanar quads (that I can't find the issue)
		//so we split quads (an odd numbered face represents the second triangle of the quad)
		if(face[i-1].v4)
			RE_ray_tree_add_face(tree, 0, (RayFace*)(i*2+1));
	}

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
	isec.faceorig	= (RayFace*)(-1);
	isec.labda		= 1e10f;

	VECCOPY(isec.start, coord);
	VECCOPY(isec.vec, direction);
	VECADDFAC(isec.end, isec.start, isec.vec, isec.labda);

	if(!RE_ray_tree_intersect(tree, &isec))
		return FLT_MAX;

	isec.labda = ABS(isec.labda);
	VECADDFAC(isec.end, isec.start, isec.vec, isec.labda);
	return VecLenf((float*)coord, (float*)isec.end);
}

/*
 * This calculates the distance (in dir units) that the ray must travel to intersect plane
 * It can return negative values
 *
 * TODO theres probably something like this on blender code
 *
 * Returns FLT_MIN in parallel case
 */
static float ray_intersect_plane(const float *point, const float *dir, const float *plane_point, const float *plane_normal)
{
		float pp[3];
		float a, pp_dist;

		a = INPR(dir, plane_normal);

		if(fabs(a) < 1e-5f) return FLT_MIN;

		VECSUB(pp, point, plane_point);
		pp_dist = INPR(pp, plane_normal);

		return -pp_dist/a;
}

/*
 * This calculates the distance from point to the plane
 * Distance is negative if point is on the back side of plane
 */
static float point_plane_distance(const float *point, const float *plane_point, const float *plane_normal)
{
	float pp[3];
	VECSUB(pp, point, plane_point);
	return INPR(pp, plane_normal);
}
static float choose_nearest(const float v0[2], const float v1[2], const float point[2], float closest[2])
{
	float d[2][2], sdist[2];
	VECSUB2D(d[0], v0, point);
	VECSUB2D(d[1], v1, point);

	sdist[0] = d[0][0]*d[0][0] + d[0][1]*d[0][1];
	sdist[1] = d[1][0]*d[1][0] + d[1][1]*d[1][1];

	if(sdist[0] < sdist[1])
	{
		if(closest)
			VECCOPY2D(closest, v0);
		return sdist[0];
	}
	else
	{
		if(closest)
			VECCOPY2D(closest, v1);
		return sdist[1];
	}
}
/*
 * calculates the closest point between point-tri (2D)
 * returns that tri must be right-handed
 * Returns square distance
 */
static float closest_point_in_tri2D(const float point[2], const float tri[3][2], float closest[2])
{
	float edge_di[2];
	float v_point[2];
	float proj[2];					//point projected over edge-dir, edge-normal (witouth normalized edge)
	const float *v0 = tri[2], *v1;
	float edge_slen, d;				//edge squared length
	int i;
	const float *nearest_vertex = NULL;


	//for each edge
	for(i=0, v0=tri[2], v1=tri[0]; i < 3; v0=tri[i++], v1=tri[i])
	{
		VECSUB2D(edge_di,    v1, v0);
		VECSUB2D(v_point, point, v0);

		proj[1] =  v_point[0]*edge_di[1] - v_point[1]*edge_di[0];	//dot product with edge normal

		//point inside this edge
		if(proj[1] < 0)
			continue;

		proj[0] = v_point[0]*edge_di[0] + v_point[1]*edge_di[1];

		//closest to this edge is v0
		if(proj[0] < 0)
		{
 			if(nearest_vertex == NULL || nearest_vertex == v0)
				nearest_vertex = v0;
			else
			{
				//choose nearest
				return choose_nearest(nearest_vertex, v0, point, closest);
			}
			i++;	//We can skip next edge
			continue;
		}

		edge_slen = edge_di[0]*edge_di[0] + edge_di[1]*edge_di[1];	//squared edge len
		//closest to this edge is v1
		if(proj[0] > edge_slen)
		{
 			if(nearest_vertex == NULL || nearest_vertex == v1)
				nearest_vertex = v1;
			else
			{
				return choose_nearest(nearest_vertex, v1, point, closest);
			}
			continue;
		}

		//nearest is on this edge
		d= proj[1] / edge_slen;
		closest[0] = point[0] - edge_di[1] * d;
		closest[1] = point[1] + edge_di[0] * d;

		return proj[1]*proj[1]/edge_slen;
	}

	if(nearest_vertex)
	{
		VECSUB2D(v_point, nearest_vertex, point);
		VECCOPY2D(closest, nearest_vertex);
		return v_point[0]*v_point[0] + v_point[1]*v_point[1];
	}
	else
	{
		VECCOPY(closest, point);	//point is already inside
		return 0.0f;
	}
}

/*
 * Returns the square of the minimum distance between the point and a triangle surface
 * If nearest is not NULL the nearest surface point is written on it
 */
static float nearest_point_in_tri_surface(const float *point, const float *v0, const float *v1, const float *v2, float *nearest)
{
	//Lets solve the 2D problem (closest point-tri)
	float normal_dist, plane_sdist, plane_offset;
	float du[3], dv[3], dw[3];	//orthogonal axis (du=(v0->v1), dw=plane normal)

	float p_2d[2], tri_2d[3][2], nearest_2d[2];

	CalcNormFloat((float*)v0, (float*)v1, (float*)v2, dw);

	//point-plane distance and calculate axis
	normal_dist = point_plane_distance(point, v0, dw);

	VECSUB(du, v1, v0);
	Normalize(du);
	Crossf(dv, dw, du);
	plane_offset = INPR(v0, dw);

	//project stuff to 2d
	tri_2d[0][0] = INPR(du, v0);
	tri_2d[0][1] = INPR(dv, v0);

	tri_2d[1][0] = INPR(du, v1);
	tri_2d[1][1] = INPR(dv, v1);

	tri_2d[2][0] = INPR(du, v2);
	tri_2d[2][1] = INPR(dv, v2);

	p_2d[0] = INPR(du, point);
	p_2d[1] = INPR(dv, point);

	//we always have a right-handed tri
	//this should always happen because of the way normal is calculated
	plane_sdist = closest_point_in_tri2D(p_2d, tri_2d, nearest_2d);

	//project back to 3d
	if(nearest)
	{
		nearest[0] = du[0]*nearest_2d[0] + dv[0] * nearest_2d[1] + dw[0] * plane_offset;
		nearest[1] = du[1]*nearest_2d[0] + dv[1] * nearest_2d[1] + dw[1] * plane_offset;
		nearest[2] = du[2]*nearest_2d[0] + dv[2] * nearest_2d[1] + dw[2] * plane_offset;
	}

	return sasqrt(plane_sdist + normal_dist*normal_dist);
}



/*
 * Shrink to nearest surface point on target mesh
 */
static void bruteforce_shrinkwrap_calc_nearest_surface_point(DerivedMesh *target, float *co, float *unused)
{
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
		float dist;

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

		VecMat4MulVecfl(final, calc->target2local, final);

		dist = VecLenf(vert[i].co, final);
		if(dist > 1e-5) weight *= (dist - calc->keptDist)/dist;
		VecLerpf(vert[i].co, vert[i].co, final, weight);	//linear interpolation
	}
}


/*
 * This function removes Unused faces, vertexs and edges from calc->target
 *
 * This function may modify calc->final. As so no data retrieved from
 * it before the call to this function  can be considered valid
 * In case it creates a new DerivedMesh, the old calc->final is freed
 */
//TODO memory checks on allocs
static void shrinkwrap_removeUnused(ShrinkwrapCalcData *calc)
{
	int i, t;

	DerivedMesh *old = calc->final, *new = NULL;
	MFace *new_face = NULL;
	MVert *new_vert  = NULL;

	int numVerts= old->getNumVerts(old);
	MVert *vert = old->getVertDataArray(old, CD_MVERT);

	int	numFaces= old->getNumFaces(old);
	MFace *face = old->getFaceDataArray(old, CD_MFACE);

	BitSet moved_verts = calc->moved;

	//Arrays to translate to new vertexs indexs
	int *vert_index = (int*)MEM_callocN(sizeof(int)*(numVerts), "shrinkwrap used verts");
	BitSet used_faces = bitset_new(numFaces, "shrinkwrap used faces");
	int numUsedFaces = 0;

	//calc real number of faces, and vertices
	//Count used faces
	for(i=0; i<numFaces; i++)
	{
		char res = bitset_get(moved_verts, face[i].v1)
				 | bitset_get(moved_verts, face[i].v2)
				 | bitset_get(moved_verts, face[i].v3)
				 | (face[i].v4 ? bitset_get(moved_verts, face[i].v4) : 0);

		if(res)
		{
			bitset_set(used_faces, i);	//Mark face to maintain
			numUsedFaces++;

			vert_index[face[i].v1] = 1;
			vert_index[face[i].v2] = 1;
			vert_index[face[i].v3] = 1;
			if(face[i].v4) vert_index[face[i].v4] = 1;
		}
	}

	//DP: Accumulate vertexs indexs.. (will calculate the new vertex index with a 1 offset)
	for(i=1; i<numVerts; i++)
		vert_index[i] += vert_index[i-1];
		
	
	//Start creating the clean mesh
	new = CDDM_new(vert_index[numVerts-1], 0, numUsedFaces);

	//Copy vertexs (unused are are removed)
	new_vert  = new->getVertDataArray(new, CD_MVERT);
	for(i=0, t=0; i<numVerts; i++)
	{
		if(vert_index[i] != t)
		{
			t = vert_index[i];
			memcpy(new_vert++, vert+i, sizeof(MVert));
		}
	}

	//Copy faces
	new_face = new->getFaceDataArray(new, CD_MFACE);
	for(i=0, t=0; i<numFaces; i++)
	{
		if(bitset_get(used_faces, i))
		{
			memcpy(new_face, face+i, sizeof(MFace));
			//update vertices indexs
			new_face->v1 = vert_index[new_face->v1]-1;
			new_face->v2 = vert_index[new_face->v2]-1;
			new_face->v3 = vert_index[new_face->v3]-1;
			if(new_face->v4)
			{
				new_face->v4 = vert_index[new_face->v4]-1;

				//Ups translated vertex ended on 0 .. TODO fix this
				if(new_face->v4 == 0)
				{
				}
			}			
			new_face++;
		}
	}

	//Free memory
	bitset_free(used_faces);
	MEM_freeN(vert_index);
	old->release(old);

	//Update edges
	CDDM_calc_edges(new);
	CDDM_calc_normals(new);

	calc->final = new;
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
	
		calc.keptDist = smd->keptDist;	//TODO: smd->keptDist is in global units.. must change to local
	}

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
		shrinkwrap_removeUnused(&calc);
		bitset_free(calc.moved);
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

	BVHTree *tree	= NULL;

	BENCH_VAR(build);
	BENCH_VAR(query);

	int	numVerts;
	MVert *vert = NULL, *tvert = NULL;
	MDeformVert *dvert = NULL;

	numVerts= calc->target->getNumVerts(calc->target);
	vert = tvert	= calc->target->getVertDataArray(calc->target, CD_MVERT);	

	BENCH_RESET(build);
	BENCH_BEGIN(build);

	tree = BLI_bvhtree_new(numVerts, 0, 8, 6);
	if(tree == NULL) return OUT_OF_MEMORY();

	for(i = 0; i < numVerts; i++)
		BLI_bvhtree_insert(tree, i, vert[i].co, 1);
	BLI_bvhtree_balance(tree);
	BENCH_END(build);
	BENCH_REPORT(build);


	//Generate kd-tree with target vertexs
	BENCH_RESET(build);
	BENCH_BEGIN(build);

	target = BLI_kdtree_new(numVerts);
	if(target == NULL) return OUT_OF_MEMORY();

	for(i = 0; i < numVerts; i++)
		BLI_kdtree_insert(target, 0, vert[i].co, NULL);

	BLI_kdtree_balance(target);

	BENCH_END(build);
	BENCH_REPORT(build);


	//Find the nearest vertex 
	numVerts= calc->final->getNumVerts(calc->final);
	vert	= calc->final->getVertDataArray(calc->final, CD_MVERT);	
	dvert	= calc->final->getVertDataArray(calc->final, CD_MDEFORMVERT);

	BENCH_BEGIN(query);
	for(i=0; i<numVerts; i++)
	{
		int t, index;
		float weight = vertexgroup_get_weight(dvert, i, vgroup);
		if(weight == 0.0f) continue;

/*		VecMat4MulVecfl(tmp_co, calc->local2target, vert[i].co);

		index = BLI_bvhtree_find_nearest(tree, tmp_co);
		if(index != -1)
		{
			float dist;
			VecMat4MulVecfl(tmp_co, calc->target2local, tvert[index].co);
			dist = VecLenf(vert[i].co, tmp_co);
			if(dist > 1e-5) weight *= (dist - calc->keptDist)/dist;
			VecLerpf(vert[i].co, vert[i].co, nearest.co, weight);	//linear interpolation
		}

	*/	
		t = BLI_kdtree_find_nearest(target, tmp_co, 0, &nearest);

		if(t != -1)
		{
			float dist;

			VecMat4MulVecfl(nearest.co, calc->target2local, nearest.co);
			dist = VecLenf(vert[i].co, tmp_co);
			if(dist > 1e-5) weight *= (dist - calc->keptDist)/dist;
			VecLerpf(vert[i].co, vert[i].co, nearest.co, weight);	//linear interpolation
		}
		
	}
	BENCH_END(query);
	BENCH_REPORT(query);

	BLI_kdtree_free(target);
	BLI_bvhtree_free(tree);
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

	if(calc->smd->shrinkOpts & MOD_SHRINKWRAP_REMOVE_UNPROJECTED_FACES)
		calc->moved = bitset_new(numVerts, "shrinkwrap bitset data");

	for(i=0; i<numVerts; i++)
	{
		float dist = FLT_MAX;
		float weight = vertexgroup_get_weight(dvert, i, vgroup);
		if(weight == 0.0f) continue;

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
			float dist_t;

			VECADDFAC(tmp_co, tmp_co, tmp_no, dist);
			VecMat4MulVecfl(tmp_co, calc->target2local, tmp_co);

			dist_t = VecLenf(vert[i].co, tmp_co);
			if(dist_t > 1e-5) weight *= (dist_t - calc->keptDist)/dist_t;
			VecLerpf(vert[i].co, vert[i].co, tmp_co, weight);	//linear interpolation

			if(calc->moved)
				bitset_set(calc->moved, i);
		}

	}

	free_raytree_from_mesh(target);
}


