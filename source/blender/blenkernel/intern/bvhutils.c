/**
 *
 * $Id$
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
 * Contributor(s): André Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "BKE_bvhutils.h"

#include "DNA_object_types.h"
#include "DNA_modifier_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_utildefines.h"
#include "BKE_deform.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_displist.h"
#include "BKE_global.h"

#include "BLI_arithb.h"

/* Math stuff for ray casting on mesh faces and for nearest surface */

static float nearest_point_in_tri_surface(const float *point, const float *v0, const float *v1, const float *v2, float *nearest);

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
 * BVH from meshs callbacks
 */

// Callback to bvh tree nearest point. The tree must bust have been built using bvhtree_from_mesh_faces.
// userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree.
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

// Callback to bvh tree raycast. The tree must bust have been built using bvhtree_from_mesh_faces.
// userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree.
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
 * BVH builders
 */
// Builds a bvh tree.. where nodes are the vertexs of the given mesh
void bvhtree_from_mesh_verts(BVHTreeFromMesh *data, DerivedMesh *mesh, float epsilon, int tree_type, int axis)
{
	int i;
	int numVerts= mesh->getNumVerts(mesh);
	MVert *vert	= mesh->getVertDataArray(mesh, CD_MVERT);
	BVHTree *tree = NULL;

	memset(data, 0, sizeof(*data));

	if(vert == NULL)
	{
		printf("bvhtree cant be build: cant get a vertex array");
		return;
	}

	tree = BLI_bvhtree_new(numVerts, epsilon, tree_type, axis);
	if(tree != NULL)
	{
		for(i = 0; i < numVerts; i++)
			BLI_bvhtree_insert(tree, i, vert[i].co, 1);

		BLI_bvhtree_balance(tree);

		data->tree = tree;

		//a NULL nearest callback works fine
		//remeber the min distance to point is the same as the min distance to BV of point
		data->nearest_callback = NULL;
		data->raycast_callback = NULL;

		data->mesh = mesh;
		data->vert = mesh->getVertDataArray(mesh, CD_MVERT);
		data->face = mesh->getFaceDataArray(mesh, CD_MFACE);

		data->sphere_radius = epsilon;
	}
}

// Builds a bvh tree.. where nodes are the faces of the given mesh.
void bvhtree_from_mesh_faces(BVHTreeFromMesh *data, DerivedMesh *mesh, float epsilon, int tree_type, int axis)
{
	int i;
	int numFaces= mesh->getNumFaces(mesh);
	MVert *vert	= mesh->getVertDataArray(mesh, CD_MVERT);
	MFace *face = mesh->getFaceDataArray(mesh, CD_MFACE);
	BVHTree *tree = NULL;

	memset(data, 0, sizeof(*data));

	if(vert == NULL && face == NULL)
	{
		printf("bvhtree cant be build: cant get a vertex/face array");
		return;
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

		data->tree = tree;
		data->nearest_callback = mesh_faces_nearest_point;
		data->raycast_callback = mesh_faces_spherecast;

		data->mesh = mesh;
		data->vert = mesh->getVertDataArray(mesh, CD_MVERT);
		data->face = mesh->getFaceDataArray(mesh, CD_MFACE);

		data->sphere_radius = epsilon;
	}
}

// Frees data allocated by a call to bvhtree_from_mesh_*.
void free_bvhtree_from_mesh(struct BVHTreeFromMesh *data)
{
	if(data->tree)
	{
		BLI_bvhtree_free(data->tree);
		memset( data, 0, sizeof(data) );
	}
}


