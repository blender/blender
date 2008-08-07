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

#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_shrinkwrap.h"
#include "BKE_DerivedMesh.h"
#include "BKE_utildefines.h"
#include "BKE_deform.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
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

#define BENCH_VAR(name)		clock_t JOIN(_bench_step,name) = 0, JOIN(_bench_total,name) = 0
#define BENCH_BEGIN(name)	JOIN(_bench_step, name) = clock()
#define BENCH_END(name)		JOIN(_bench_total,name) += clock() - JOIN(_bench_step,name)
#define BENCH_RESET(name)	JOIN(_bench_total, name) = 0
#define BENCH_REPORT(name)	printf("%s: %fms (cpu) \n", TO_STR(name), JOIN(_bench_total,name)*1000.0f/CLOCKS_PER_SEC)

#else

#define BENCH(a)	(a)
#define BENCH_VAR(name)
#define BENCH_BEGIN(name)
#define BENCH_END(name)
#define BENCH_RESET(name)
#define BENCH_REPORT(name)

#endif

typedef void ( *Shrinkwrap_ForeachVertexCallback) (DerivedMesh *target, float *co, float *normal);

static float nearest_point_in_tri_surface(const float *point, const float *v0, const float *v1, const float *v2, float *nearest);
static float ray_intersect_plane(const float *point, const float *dir, const float *plane_point, const float *plane_normal);

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

/* ray - triangle */
#define ISECT_EPSILON 1e-6
static float ray_tri_intersection(const BVHTreeRay *ray, const float m_dist, const float *v0, const float *v1, const float *v2)
{
	float dist;

	if(RayIntersectsTriangle(ray->origin, ray->direction, v0, v1, v2, &dist, NULL))
		return dist;

	return FLT_MAX;
}

static float sphereray_tri_intersection(const BVHTreeRay *ray, float radius, const float m_dist, const float *v0, const float *v1, const float *v2)
{
	
	float idist;
	float p1[3];
	float plane_normal[3], hit_point[3];

	CalcNormFloat((float*)v0, (float*)v1, (float*)v2, plane_normal);

	VECADDFAC( p1, ray->origin, ray->direction, m_dist);
	if(SweepingSphereIntersectsTriangleUV(ray->origin, p1, radius, v0, v1, v2, &idist, &hit_point))
	{
		return idist * m_dist;
	}

	return FLT_MAX;
}

/* Space transform */
void space_transform_from_matrixs(SpaceTransform *data, float local[4][4], float target[4][4])
{
	float itarget[4][4];
	Mat4Invert(itarget, target); //Invserse might be outdated
	Mat4MulSerie(data->local2target, itarget, local, 0, 0, 0, 0, 0, 0);
	Mat4Invert(data->target2local, data->local2target);
}

void space_transform_apply(const SpaceTransform *data, float *co)
{
	VecMat4MulVecfl(co, data->local2target, co);
}

void space_transform_invert(const SpaceTransform *data, float *co)
{
	VecMat4MulVecfl(co, data->target2local, co);
}

void space_transform_apply_normal(const SpaceTransform *data, float *no)
{
	Mat4Mul3Vecfl(data->local2target, no);
	Normalize(no); // TODO: could we just determine de scale value from the matrix?
}

void space_transform_invert_normal(const SpaceTransform *data, float *no)
{
	Mat4Mul3Vecfl(data->target2local, no);
	Normalize(no); // TODO: could we just determine de scale value from the matrix?
}


/*
 * BVH Tree from Mesh
 */
//callbacks
static void mesh_faces_nearest_point(void *userdata, int index, const float *co, BVHTreeNearest *nearest);
static void mesh_faces_spherecast(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit);

/*
 * Builds a bvh tree.. where nodes are the vertexs of the given mesh
 */
BVHTree* bvhtree_from_mesh_verts(BVHTreeFromMesh *data, DerivedMesh *mesh, float epsilon, int tree_type, int axis)
{
	int i;
	int numVerts= mesh->getNumVerts(mesh);
	MVert *vert	= mesh->getVertDataArray(mesh, CD_MVERT);
	BVHTree *tree = NULL;

	if(data)
		memset(data, 0, sizeof(*data));

	if(vert == NULL)
	{
		printf("bvhtree cant be build: cant get a vertex array");
		return NULL;
	}

	tree = BLI_bvhtree_new(numVerts, epsilon, tree_type, axis);
	if(tree != NULL)
	{
		for(i = 0; i < numVerts; i++)
			BLI_bvhtree_insert(tree, i, vert[i].co, 1);

		BLI_bvhtree_balance(tree);

		if(data)
		{
			data->tree = tree;
			data->nearest_callback = NULL;
			data->raycast_callback = NULL;

			data->mesh = mesh;
			data->vert = mesh->getVertDataArray(mesh, CD_MVERT);
			data->face = mesh->getFaceDataArray(mesh, CD_MFACE);

			data->sphere_radius = epsilon;
		}
	}

	return tree;
}

/*
 * Builds a bvh tree.. where nodes are the faces of the given mesh.
 */
BVHTree* bvhtree_from_mesh_faces(BVHTreeFromMesh *data, DerivedMesh *mesh, float epsilon, int tree_type, int axis)
{
	int i;
	int numFaces= mesh->getNumFaces(mesh);
	MVert *vert	= mesh->getVertDataArray(mesh, CD_MVERT);
	MFace *face = mesh->getFaceDataArray(mesh, CD_MFACE);
	BVHTree *tree = NULL;

	if(data)
		memset(data, 0, sizeof(*data));

	if(vert == NULL && face == NULL)
	{
		printf("bvhtree cant be build: cant get a vertex/face array");
		return NULL;
	}

	/* Create a bvh-tree of the given target */
	tree = BLI_bvhtree_new(numFaces, epsilon, tree_type, axis);
	if(tree != NULL)
	{
		for(i = 0; i < numFaces; i++)
		{
			float co[4][3];
			VECCOPY(co[0], vert[ face[i].v1 ].co);
			VECCOPY(co[1], vert[ face[i].v2 ].co);
			VECCOPY(co[2], vert[ face[i].v3 ].co);
			if(face[i].v4)
				VECCOPY(co[3], vert[ face[i].v4 ].co);

			BLI_bvhtree_insert(tree, i, co[0], face[i].v4 ? 4 : 3);
		}
		BLI_bvhtree_balance(tree);

		if(data)
		{
			data->tree = tree;
			data->nearest_callback = mesh_faces_nearest_point;
			data->raycast_callback = mesh_faces_spherecast;

			data->mesh = mesh;
			data->vert = mesh->getVertDataArray(mesh, CD_MVERT);
			data->face = mesh->getFaceDataArray(mesh, CD_MFACE);

			data->sphere_radius = epsilon;
		}
	}

	return tree;
}

/*
 * Callback to bvh tree nearest point. The tree must bust have been built using bvhtree_from_mesh_faces.
 * userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree.
 */
static void mesh_faces_nearest_point(void *userdata, int index, const float *co, BVHTreeNearest *nearest)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh*) userdata;
	MVert *vert	= data->vert;
	MFace *face = data->face + index;

	float *t0, *t1, *t2, *t3;
	t0 = vert[ face->v1 ].co;
	t1 = vert[ face->v2 ].co;
	t2 = vert[ face->v3 ].co;
	t3 = face->v4 ? vert[ face->v4].co : NULL;

	
	do
	{	
		float nearest_tmp[3], dist;

		dist = nearest_point_in_tri_surface(co,t0, t1, t2, nearest_tmp);
		if(dist < nearest->dist)
		{
			nearest->index = index;
			nearest->dist = dist;
			VECCOPY(nearest->co, nearest_tmp);
			CalcNormFloat((float*)t0, (float*)t1, (float*)t2, nearest->no); //TODO.. (interpolate normals from the vertexs coordinates?
		}


		t1 = t2;
		t2 = t3;
		t3 = NULL;

	} while(t2);
}

/*
 * Callback to bvh tree raycast. The tree must bust have been built using bvhtree_from_mesh_faces.
 * userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree.
 */
static void mesh_faces_spherecast(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh*) userdata;
	MVert *vert	= data->vert;
	MFace *face = data->face + index;

	float *t0, *t1, *t2, *t3;
	t0 = vert[ face->v1 ].co;
	t1 = vert[ face->v2 ].co;
	t2 = vert[ face->v3 ].co;
	t3 = face->v4 ? vert[ face->v4].co : NULL;

	
	do
	{	
		float dist;
		if(data->sphere_radius == 0.0f)
			dist = ray_tri_intersection(ray, hit->dist, t0, t1, t2);
		else
			dist = sphereray_tri_intersection(ray, data->sphere_radius, hit->dist, t0, t1, t2);

		if(dist >= 0 && dist < hit->dist)
		{
			hit->index = index;
			hit->dist = dist;
			VECADDFAC(hit->co, ray->origin, ray->direction, dist);

			CalcNormFloat(t0, t1, t2, hit->no);
		}

		t1 = t2;
		t2 = t3;
		t3 = NULL;

	} while(t2);
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
 *
 */
static void derivedmesh_mergeNearestPoints(DerivedMesh *dm, float mdist, BitSet skipVert)
{
	if(mdist > 0.0f)
	{
		int i, j, merged;
		int	numVerts = dm->getNumVerts(dm);
		int *translate_vert = MEM_mallocN( sizeof(int)*numVerts, "merge points array");

		MVert *vert = dm->getVertDataArray(dm, CD_MVERT);

		if(!translate_vert) return;

		merged = 0;
		for(i=0; i<numVerts; i++)
		{
			translate_vert[i] = i;

			if(skipVert && bitset_get(skipVert, i)) continue;

			for(j = 0; j<i; j++)
			{
				if(skipVert && bitset_get(skipVert, j)) continue;
				if(squared_dist(vert[i].co, vert[j].co) < mdist)
				{
					translate_vert[i] = j;
					merged++;
					break;
				}
			}
		}

		//some vertexs were merged.. recalculate structure (edges and faces)
		if(merged > 0)
		{
			int	numFaces = dm->getNumFaces(dm);
			int freeVert;
			MFace *face = dm->getFaceDataArray(dm, CD_MFACE);


			//Adjust vertexs using the translation_table.. only translations to back indexs are allowed
			//which means t[i] <= i must always verify
			for(i=0, freeVert = 0; i<numVerts; i++)
			{
				if(translate_vert[i] == i)
				{
					memcpy(&vert[freeVert], &vert[i], sizeof(*vert));
					translate_vert[i] = freeVert++;
				}
				else translate_vert[i] = translate_vert[ translate_vert[i] ];
			}

			CDDM_lower_num_verts(dm, numVerts - merged);

			for(i=0; i<numFaces; i++)
			{
				MFace *f = face+i;
				f->v1 = translate_vert[f->v1];
				f->v2 = translate_vert[f->v2];
				f->v3 = translate_vert[f->v3];
				//TODO be carefull with vertexs v4 being translated to 0
				f->v4 = translate_vert[f->v4];
			}

			//TODO: maybe update edges could be done outside this function
			CDDM_calc_edges(dm);
			//CDDM_calc_normals(dm);
		}

		if(translate_vert) MEM_freeN( translate_vert );
	}
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
static float closest_point_in_tri2D(const float point[2], /*const*/ float tri[3][2], float closest[2])
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

	// OPTIMIZATION
	//	if we are only interested in nearest distance if its closer than some distance already found
	//  we can:
	//		if(normal_dist*normal_dist >= best_dist_so_far) return FLOAT_MAX;
	//

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

	return plane_sdist + normal_dist*normal_dist;
}

/*
 * This function removes Unused faces, vertexs and edges from calc->target
 *
 * This function may modify calc->final. As so no data retrieved from
 * it before the call to this function  can be considered valid
 * In case it creates a new DerivedMesh, the old calc->final is freed
 */
//TODO memory checks on allocs
/*
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


	//calculate which vertexs need to be used
	//even unmoved vertices might need to be used if theres a face that needs it
	//calc real number of faces, and vertices
	//Count used faces
	for(i=0; i<numFaces; i++)
	{
		char res = 0;
		if(bitset_get(moved_verts, face[i].v1)) res++;
		if(bitset_get(moved_verts, face[i].v2)) res++;
		if(bitset_get(moved_verts, face[i].v3)) res++;
		if(face[i].v4 && bitset_get(moved_verts, face[i].v4)) res++;

		//Ignore a face were not a single vertice moved
		if(res == 0) continue;


		//Only 1 vertice moved.. (if its a quad.. remove the vertice oposite to it)
		if(res == 1 && face[i].v4)
		{
			if(bitset_get(moved_verts, face[i].v1))
			{
				//remove vertex 3
				face[i].v3 = face[i].v4;
			}
			else if(bitset_get(moved_verts, face[i].v2))
			{
				//remove vertex 4
			}
			else if(bitset_get(moved_verts, face[i].v3))
			{
				//remove vertex 1
				face[i].v1 = face[i].v4;
			}
			else if(bitset_get(moved_verts, face[i].v4))
			{
				//remove vertex 2
				face[i].v2 = face[i].v3;
				face[i].v3 = face[i].v4;
			}

			face[i].v4 = 0;	//this quad turned on a tri
		}
		
#if 0
		if(face[i].v4 && res == 3)
		{
			if(!bitset_get(moved_verts, face[i].v1))
			{
				face[i].v1 = face[i].v4;
			}
			else if(!bitset_get(moved_verts, face[i].v2))
			{
				face[i].v2 = face[i].v3;
				face[i].v3 = face[i].v4;
			}
			else if(!bitset_get(moved_verts, face[i].v3))
			{
				face[i].v3 = face[i].v4;
			}

			face[i].v4 = 0;	//this quad turned on a tri
		}
#endif

		bitset_set(used_faces, i);	//Mark face to maintain
		numUsedFaces++;

		//Mark vertices are needed
		vert_index[face[i].v1] = 1;
		vert_index[face[i].v2] = 1;
		vert_index[face[i].v3] = 1;
		if(face[i].v4) vert_index[face[i].v4] = 1;
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

			if(bitset_get(moved_verts, i))
				bitset_set(moved_verts, t-1);
			else
				bitset_unset(moved_verts, t-1);
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


void shrinkwrap_projectToCutPlane(ShrinkwrapCalcData *calc_data)
{
	if(calc_data->smd->cutPlane && calc_data->moved)
	{
		int i;
		int unmoved = 0;
		int numVerts= 0;
		MVert *vert = NULL;
		MVert *vert_unmoved = NULL;

		ShrinkwrapCalcData calc;
		memcpy(&calc, calc_data, sizeof(calc));

		calc.moved = 0;

		if(calc.smd->cutPlane)
		{
			//TODO currently we need a copy in case object_get_derived_final returns an emDM that does not defines getVertArray or getFace array
			calc.target = CDDM_copy( object_get_derived_final(calc.smd->cutPlane, CD_MASK_BAREMESH) );

			if(!calc.target)
			{
				return;
			}

			space_transform_setup(&calc.local2target, calc.ob, calc.smd->cutPlane);
			calc.keptDist = 0;
		}


		//Make a mesh with the points we want to project
		numVerts = calc_data->final->getNumVerts(calc_data->final);

		unmoved = 0;
		for(i=0; i<numVerts; i++)
			if(!bitset_get(calc_data->moved, i))
				unmoved++;

		calc.final = CDDM_new(unmoved, 0, 0);
		if(!calc.final) return;


		vert = calc_data->final->getVertDataArray(calc_data->final, CD_MVERT);
		vert_unmoved = calc.final->getVertDataArray(calc.final, CD_MVERT);

		for(i=0; i<numVerts; i++)
			if(!bitset_get(calc_data->moved, i))
				memcpy(vert_unmoved++, vert+i, sizeof(*vert_unmoved));

		//use shrinkwrap projection
		shrinkwrap_calc_normal_projection(&calc);

		//Copy the points back to the mesh
		vert = calc_data->final->getVertDataArray(calc_data->final, CD_MVERT);
		vert_unmoved = calc.final->getVertDataArray(calc.final, CD_MVERT);
		for(i=0; i<numVerts; i++)
			if(!bitset_get(calc_data->moved, i))
				memcpy(vert+i, vert_unmoved++, sizeof(*vert_unmoved) );

		//free memory
		calc.final->release(calc.final);
		calc.target->release(calc.target);
	}	

}
*/

/* Main shrinkwrap function */
/*
DerivedMesh *shrinkwrapModifier_do(ShrinkwrapModifierData *smd, Object *ob, DerivedMesh *dm, int useRenderParams, int isFinalCalc)
{

	ShrinkwrapCalcData calc;
	memset(&calc, 0, sizeof(calc));

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

	CDDM_calc_normals(calc.final);	//Normals maybe not be calculated yet

	//remove loop dependencies on derived meshs (TODO should this be done elsewhere?)
	if(smd->target == ob) smd->target = NULL;
	if(smd->cutPlane == ob) smd->cutPlane = NULL;


	if(smd->target)
	{
		//TODO currently we need a copy in case object_get_derived_final returns an emDM that does not defines getVertArray or getFace array
		calc.target = CDDM_copy( object_get_derived_final(smd->target, CD_MASK_BAREMESH) );

		if(!calc.target)
		{
			printf("Target derived mesh is null! :S\n");
		}

		//TODO there might be several "bugs" on non-uniform scales matrixs.. because it will no longer be nearest surface, not sphere projection
		//because space has been deformed
		space_transform_setup(&calc.local2target, ob, smd->target);

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
				BENCH(shrinkwrap_calc_nearest_surface_point(&calc));
//				BENCH(shrinkwrap_calc_foreach_vertex(&calc, bruteforce_shrinkwrap_calc_nearest_surface_point));
			break;

			case MOD_SHRINKWRAP_NORMAL:

				if(calc.smd->shrinkOpts & MOD_SHRINKWRAP_REMOVE_UNPROJECTED_FACES)
				calc.moved = bitset_new( calc.final->getNumVerts(calc.final), "shrinkwrap bitset data");


//				BENCH(shrinkwrap_calc_normal_projection_raytree(&calc));
//				calc.final->release( calc.final );
//				calc.final = CDDM_copy(calc.original);

				BENCH(shrinkwrap_calc_normal_projection(&calc));
//				BENCH(shrinkwrap_calc_foreach_vertex(&calc, bruteforce_shrinkwrap_calc_normal_projection));

				if(calc.moved)
				{
					//Adjust vertxs that didn't moved (project to cut plane)
					shrinkwrap_projectToCutPlane(&calc);

					//Destroy faces, edges and stuff
					shrinkwrap_removeUnused(&calc);

					//Merge points that didn't moved
					derivedmesh_mergeNearestPoints(calc.final, calc.smd->mergeDist, calc.moved);
					bitset_free(calc.moved);
				}
			break;

			case MOD_SHRINKWRAP_NEAREST_VERTEX:

				BENCH(shrinkwrap_calc_nearest_vertex(&calc));
//				BENCH(shrinkwrap_calc_foreach_vertex(&calc, bruteforce_shrinkwrap_calc_nearest_vertex));
			break;
		}

		//free derived mesh
		calc.target->release( calc.target );
		calc.target = NULL;
	}

	CDDM_calc_normals(calc.final);

	return calc.final;
}
*/

/* Main shrinkwrap function */
void shrinkwrapModifier_deform(ShrinkwrapModifierData *smd, Object *ob, DerivedMesh *dm, float (*vertexCos)[3], int numVerts)
{

	ShrinkwrapCalcData calc = NULL_ShrinkwrapCalcData;

	//Init Shrinkwrap calc data
	calc.smd = smd;

	calc.ob = ob;
	calc.original = dm;

	calc.numVerts = numVerts;
	calc.vertexCos = vertexCos;

	//remove loop dependencies on derived meshs (TODO should this be done elsewhere?)
	if(smd->target == ob) smd->target = NULL;
	if(smd->cutPlane == ob) smd->cutPlane = NULL;


	if(smd->target)
	{
		//TODO currently we need a copy in case object_get_derived_final returns an emDM that does not defines getVertArray or getFace array
		calc.target = CDDM_copy( object_get_derived_final(smd->target, CD_MASK_BAREMESH) );

		if(!calc.target)
		{
			printf("Target derived mesh is null! :S\n");
		}

		//TODO there might be several "bugs" on non-uniform scales matrixs.. because it will no longer be nearest surface, not sphere projection
		//because space has been deformed
		space_transform_setup(&calc.local2target, ob, smd->target);

		calc.keptDist = smd->keptDist;	//TODO: smd->keptDist is in global units.. must change to local
	}

	//Projecting target defined - lets work!
	if(calc.target)
	{

		printf("Shrinkwrap (%s)%d over (%s)%d\n",
			calc.ob->id.name,			calc.numVerts,
			calc.smd->target->id.name,	calc.target->getNumVerts(calc.target)
		);


		switch(smd->shrinkType)
		{
			case MOD_SHRINKWRAP_NEAREST_SURFACE:
				BENCH(shrinkwrap_calc_nearest_surface_point(&calc));
			break;

			case MOD_SHRINKWRAP_NORMAL:
				BENCH(shrinkwrap_calc_normal_projection(&calc));
			break;

			case MOD_SHRINKWRAP_NEAREST_VERTEX:
				BENCH(shrinkwrap_calc_nearest_vertex(&calc));
			break;
		}

		//free derived mesh
		calc.target->release( calc.target );
		calc.target = NULL;
	}
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

	BVHTreeFromMesh treeData = NULL_BVHTreeFromMesh;
	BVHTreeNearest  nearest  = NULL_BVHTreeNearest;

	MDeformVert *dvert = calc->original ? calc->original->getVertDataArray(calc->original, CD_MDEFORMVERT) : NULL;


	BENCH(bvhtree_from_mesh_verts(&treeData, calc->target, 0.0, 2, 6));
	if(treeData.tree == NULL) return OUT_OF_MEMORY();

	//Setup nearest
	nearest.index = -1;
	nearest.dist = FLT_MAX;

//#pragma omp parallel for private(i) private(nearest) schedule(static)
	for(i = 0; i<calc->numVerts; ++i)
	{
		float *co = calc->vertexCos[i];
		int index;
		float tmp_co[3];
		float weight = vertexgroup_get_vertex_weight(dvert, i, vgroup);
		if(weight == 0.0f) continue;

		VECCOPY(tmp_co, co);
		space_transform_apply(&calc->local2target, tmp_co);

		//Use local proximity heuristics (to reduce the nearest search)
		if(nearest.index != -1)
			nearest.dist = squared_dist(tmp_co, nearest.co);
		else
			nearest.dist = FLT_MAX;

		index = BLI_bvhtree_find_nearest(treeData.tree, tmp_co, &nearest, treeData.nearest_callback, &treeData);

		if(index != -1)
		{
			float dist = nearest.dist;
			if(dist > 1e-5) weight *= (dist - calc->keptDist)/dist;

			VECCOPY(tmp_co, nearest.co);
			space_transform_invert(&calc->local2target, tmp_co);
			VecLerpf(co, co, tmp_co, weight);	//linear interpolation
		}
	}

	BLI_bvhtree_free(treeData.tree);
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

		hit_tmp.dist *= Mat4ToScalef( transf->local2target );
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

		if(((options & MOD_SHRINKWRAP_CULL_TARGET_FRONTFACE) && dot < 0)
		|| ((options & MOD_SHRINKWRAP_CULL_TARGET_BACKFACE) && dot > 0))
			return FALSE; //Ignore hit


		//Inverting space transform (TODO make coeherent with the initial dist readjust)
		if(transf)
		{
			space_transform_invert( transf, hit_tmp.co );
			space_transform_invert_normal( transf, hit_tmp.no );

			hit_tmp.dist = VecLenf( vert, hit_tmp.co );
		}

		memcpy(hit, &hit_tmp, sizeof(hit_tmp) );
		return TRUE;
	}
	return FALSE;
}


void shrinkwrap_calc_normal_projection(ShrinkwrapCalcData *calc)
{
	int i;
	int vgroup		= get_named_vertexgroup_num(calc->ob, calc->smd->vgroup_name);
	char use_normal = calc->smd->shrinkOpts;

	//setup raytracing
	BVHTreeFromMesh treeData = NULL_BVHTreeFromMesh;
	BVHTreeRayHit   hit      = NULL_BVHTreeRayHit;

	//cutTree
	DerivedMesh * limit_mesh = NULL;
	BVHTreeFromMesh limitData= NULL_BVHTreeFromMesh;
	SpaceTransform local2cut;

	MVert        *vert = calc->original ? calc->original->getVertDataArray(calc->original, CD_MVERT) : NULL;		//Needed because of vertex normal
	MDeformVert *dvert = calc->original ? calc->original->getVertDataArray(calc->original, CD_MDEFORMVERT) : NULL;

	if(vert == NULL)
	{
		printf("Shrinkwrap cant normal project witouth normal information");
		return;
	}
	if((use_normal & (MOD_SHRINKWRAP_ALLOW_INVERTED_NORMAL | MOD_SHRINKWRAP_ALLOW_DEFAULT_NORMAL)) == 0)
		return;	//Nothing todo

	CDDM_calc_normals(calc->original);	//Normals maybe arent yet calculated

	BENCH(bvhtree_from_mesh_faces(&treeData, calc->target, calc->keptDist, 4, 6));
	if(treeData.tree == NULL) return OUT_OF_MEMORY();


	if(calc->smd->cutPlane)
	{
		space_transform_setup( &local2cut, calc->ob, calc->smd->cutPlane);

		//TODO currently we need a copy in case object_get_derived_final returns an emDM that does not defines getVertArray or getFace array
		limit_mesh = CDDM_copy( object_get_derived_final(calc->smd->cutPlane, CD_MASK_BAREMESH) );
		if(limit_mesh)
			BENCH(bvhtree_from_mesh_faces(&limitData, limit_mesh, 0.0, 4, 6));
		else
			printf("CutPlane finalDerived mesh is null\n");
	}

//#pragma omp parallel for private(i) private(hit) schedule(static)
	for(i = 0; i<calc->numVerts; ++i)
	{
		float *co = calc->vertexCos[i];
		float tmp_co[3], tmp_no[3];
		float lim = 1000;		//TODO: we should use FLT_MAX here, but sweepsphere code isnt prepared for that
		float weight = vertexgroup_get_vertex_weight(dvert, i, vgroup);
		char moved = FALSE;

		if(weight == 0.0f) continue;

		VECCOPY(tmp_co, co);
		NormalShortToFloat(tmp_no, vert[i].no);


		hit.index = -1;
		hit.dist = lim;


		if(use_normal & MOD_SHRINKWRAP_ALLOW_DEFAULT_NORMAL)
		{

			if(limitData.tree)
				normal_projection_project_vertex(0, tmp_co, tmp_no, &local2cut, limitData.tree, &hit, limitData.raycast_callback, &limitData);

			if(normal_projection_project_vertex(calc->smd->shrinkOpts, tmp_co, tmp_no, &calc->local2target, treeData.tree, &hit, treeData.raycast_callback, &treeData))
				moved = TRUE;
		}


		if(use_normal & MOD_SHRINKWRAP_ALLOW_INVERTED_NORMAL)
		{
			float inv_no[3] = { -tmp_no[0], -tmp_no[1], -tmp_no[2] };


			if(limitData.tree)
				normal_projection_project_vertex(0, tmp_co, inv_no, &local2cut, limitData.tree, &hit, limitData.raycast_callback, &limitData);

			if(normal_projection_project_vertex(calc->smd->shrinkOpts, tmp_co, inv_no, &calc->local2target, treeData.tree, &hit, treeData.raycast_callback, &treeData))
				moved = TRUE;
		}


		if(hit.index != -1)
		{
			VecLerpf(co, co, hit.co, weight);

			if(moved && calc->moved)
				bitset_set(calc->moved, i);
		}
	}

	BLI_bvhtree_free(treeData.tree);


	if(limitData.tree)
		BLI_bvhtree_free(limitData.tree);

	if(limit_mesh)
		limit_mesh->release(limit_mesh);
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

	const int vgroup		 = get_named_vertexgroup_num(calc->ob, calc->smd->vgroup_name);
	const MDeformVert *dvert = calc->original ? calc->original->getVertDataArray(calc->original, CD_MDEFORMVERT) : NULL;

	BVHTreeFromMesh treeData = NULL_BVHTreeFromMesh;
	BVHTreeNearest  nearest  = NULL_BVHTreeNearest;



	//Create a bvh-tree of the given target
	BENCH(bvhtree_from_mesh_faces( &treeData, calc->target, 0.0, 2, 6));
	if(treeData.tree == NULL) return OUT_OF_MEMORY();

	//Setup nearest
	nearest.index = -1;
	nearest.dist = FLT_MAX;


	//Find the nearest vertex 
//#pragma omp parallel for private(i) private(nearest) schedule(static)
	for(i = 0; i<calc->numVerts; ++i)
	{
		float *co = calc->vertexCos[i];
		int index;
		float tmp_co[3];
		float weight = vertexgroup_get_vertex_weight(dvert, i, vgroup);
		if(weight == 0.0f) continue;

		VECCOPY(tmp_co, co);
		space_transform_apply(&calc->local2target, tmp_co);

		if(nearest.index != -1)
		{
			nearest.dist = squared_dist(tmp_co, nearest.co);
		}
		else nearest.dist = FLT_MAX;

		index = BLI_bvhtree_find_nearest(treeData.tree, tmp_co, &nearest, treeData.nearest_callback, &treeData);

		if(index != -1)
		{
			if(calc->smd->shrinkOpts & MOD_SHRINKWRAP_KEPT_ABOVE_SURFACE)
			{
				VECADDFAC(tmp_co, nearest.co, nearest.no, calc->keptDist);
			}
			else
			{
				float dist = sasqrt( nearest.dist );
				VecLerpf(tmp_co, tmp_co, nearest.co, (dist - calc->keptDist)/dist);	//linear interpolation
			}
			space_transform_invert(&calc->local2target, tmp_co);
			VecLerpf(co, co, tmp_co, weight);	//linear interpolation
		}
	}

	BLI_bvhtree_free(treeData.tree);
}

