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
 * The Original Code is Copyright (C) Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Andr Pinto.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/blenkernel/intern/bvhutils.c
 *  \ingroup bke
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <assert.h>

#include "DNA_meshdata_types.h"

#include "BLI_utildefines.h"
#include "BLI_linklist.h"
#include "BLI_math.h"
#include "BLI_threads.h"

#include "BKE_DerivedMesh.h"
#include "BKE_editmesh.h"

#include "MEM_guardedalloc.h"

static ThreadRWMutex cache_rwlock = BLI_RWLOCK_INITIALIZER;

/* -------------------------------------------------------------------- */
/** \name Local Callbacks
 * \{ */

/* Math stuff for ray casting on mesh faces and for nearest surface */

float bvhtree_ray_tri_intersection(
        const BVHTreeRay *ray, const float UNUSED(m_dist),
        const float v0[3], const float v1[3], const float v2[3])
{
	float dist;

#ifdef USE_KDOPBVH_WATERTIGHT
	if (isect_ray_tri_watertight_v3(ray->origin, ray->isect_precalc, v0, v1, v2, &dist, NULL))
#else
	if (isect_ray_tri_epsilon_v3(ray->origin, ray->direction, v0, v1, v2, &dist, NULL, FLT_EPSILON))
#endif
	{
		return dist;
	}

	return FLT_MAX;
}

float bvhtree_sphereray_tri_intersection(
        const BVHTreeRay *ray, float radius, const float m_dist,
        const float v0[3], const float v1[3], const float v2[3])
{
	
	float idist;
	float p1[3];
	float hit_point[3];

	madd_v3_v3v3fl(p1, ray->origin, ray->direction, m_dist);
	if (isect_sweeping_sphere_tri_v3(ray->origin, p1, radius, v0, v1, v2, &idist, hit_point)) {
		return idist * m_dist;
	}

	return FLT_MAX;
}

/*
 * BVH from meshes callbacks
 */

/* Callback to bvh tree nearest point. The tree must have been built using bvhtree_from_mesh_faces.
 * userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree. */
static void mesh_faces_nearest_point(void *userdata, int index, const float co[3], BVHTreeNearest *nearest)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *) userdata;
	const MVert *vert = data->vert;
	const MFace *face = data->face + index;

	const float *t0, *t1, *t2, *t3;
	t0 = vert[face->v1].co;
	t1 = vert[face->v2].co;
	t2 = vert[face->v3].co;
	t3 = face->v4 ? vert[face->v4].co : NULL;

	
	do {
		float nearest_tmp[3], dist_sq;

		closest_on_tri_to_point_v3(nearest_tmp, co, t0, t1, t2);
		dist_sq = len_squared_v3v3(co, nearest_tmp);

		if (dist_sq < nearest->dist_sq) {
			nearest->index = index;
			nearest->dist_sq = dist_sq;
			copy_v3_v3(nearest->co, nearest_tmp);
			normal_tri_v3(nearest->no, t0, t1, t2);
		}

		t1 = t2;
		t2 = t3;
		t3 = NULL;

	} while (t2);
}
/* copy of function above */
static void mesh_looptri_nearest_point(void *userdata, int index, const float co[3], BVHTreeNearest *nearest)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *) userdata;
	const MVert *vert = data->vert;
	const MLoopTri *lt = &data->looptri[index];
	const float *vtri_co[3] = {
	    vert[data->loop[lt->tri[0]].v].co,
	    vert[data->loop[lt->tri[1]].v].co,
	    vert[data->loop[lt->tri[2]].v].co,
	};
	float nearest_tmp[3], dist_sq;

	closest_on_tri_to_point_v3(nearest_tmp, co, UNPACK3(vtri_co));
	dist_sq = len_squared_v3v3(co, nearest_tmp);

	if (dist_sq < nearest->dist_sq) {
		nearest->index = index;
		nearest->dist_sq = dist_sq;
		copy_v3_v3(nearest->co, nearest_tmp);
		normal_tri_v3(nearest->no, UNPACK3(vtri_co));
	}
}
/* copy of function above (warning, should de-duplicate with editmesh_bvh.c) */
static void editmesh_looptri_nearest_point(void *userdata, int index, const float co[3], BVHTreeNearest *nearest)
{
	const BVHTreeFromEditMesh *data = userdata;
	BMEditMesh *em = data->em;
	const BMLoop **ltri = (const BMLoop **)em->looptris[index];

	const float *t0, *t1, *t2;
	t0 = ltri[0]->v->co;
	t1 = ltri[1]->v->co;
	t2 = ltri[2]->v->co;

	{
		float nearest_tmp[3], dist_sq;

		closest_on_tri_to_point_v3(nearest_tmp, co, t0, t1, t2);
		dist_sq = len_squared_v3v3(co, nearest_tmp);

		if (dist_sq < nearest->dist_sq) {
			nearest->index = index;
			nearest->dist_sq = dist_sq;
			copy_v3_v3(nearest->co, nearest_tmp);
			normal_tri_v3(nearest->no, t0, t1, t2);
		}
	}
}

/* Callback to bvh tree raycast. The tree must have been built using bvhtree_from_mesh_faces.
 * userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree. */
static void mesh_faces_spherecast(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *) userdata;
	const MVert *vert = data->vert;
	const MFace *face = &data->face[index];

	const float *t0, *t1, *t2, *t3;
	t0 = vert[face->v1].co;
	t1 = vert[face->v2].co;
	t2 = vert[face->v3].co;
	t3 = face->v4 ? vert[face->v4].co : NULL;

	
	do {
		float dist;
		if (data->sphere_radius == 0.0f)
			dist = bvhtree_ray_tri_intersection(ray, hit->dist, t0, t1, t2);
		else
			dist = bvhtree_sphereray_tri_intersection(ray, data->sphere_radius, hit->dist, t0, t1, t2);

		if (dist >= 0 && dist < hit->dist) {
			hit->index = index;
			hit->dist = dist;
			madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

			normal_tri_v3(hit->no, t0, t1, t2);
		}

		t1 = t2;
		t2 = t3;
		t3 = NULL;

	} while (t2);
}
/* copy of function above */
static void mesh_looptri_spherecast(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *) userdata;
	const MVert *vert = data->vert;
	const MLoopTri *lt = &data->looptri[index];
	const float *vtri_co[3] = {
	    vert[data->loop[lt->tri[0]].v].co,
	    vert[data->loop[lt->tri[1]].v].co,
	    vert[data->loop[lt->tri[2]].v].co,
	};
	float dist;

	if (data->sphere_radius == 0.0f)
		dist = bvhtree_ray_tri_intersection(ray, hit->dist, UNPACK3(vtri_co));
	else
		dist = bvhtree_sphereray_tri_intersection(ray, data->sphere_radius, hit->dist, UNPACK3(vtri_co));

	if (dist >= 0 && dist < hit->dist) {
		hit->index = index;
		hit->dist = dist;
		madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

		normal_tri_v3(hit->no, UNPACK3(vtri_co));
	}
}
/* copy of function above (warning, should de-duplicate with editmesh_bvh.c) */
static void editmesh_looptri_spherecast(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	const BVHTreeFromEditMesh *data = (BVHTreeFromEditMesh *)userdata;
	BMEditMesh *em = data->em;
	const BMLoop **ltri = (const BMLoop **)em->looptris[index];

	const float *t0, *t1, *t2;
	t0 = ltri[0]->v->co;
	t1 = ltri[1]->v->co;
	t2 = ltri[2]->v->co;


	{
		float dist;
		if (data->sphere_radius == 0.0f)
			dist = bvhtree_ray_tri_intersection(ray, hit->dist, t0, t1, t2);
		else
			dist = bvhtree_sphereray_tri_intersection(ray, data->sphere_radius, hit->dist, t0, t1, t2);

		if (dist >= 0 && dist < hit->dist) {
			hit->index = index;
			hit->dist = dist;
			madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

			normal_tri_v3(hit->no, t0, t1, t2);
		}
	}
}

/* Callback to bvh tree nearest point. The tree must have been built using bvhtree_from_mesh_edges.
 * userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree. */
static void mesh_edges_nearest_point(void *userdata, int index, const float co[3], BVHTreeNearest *nearest)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *) userdata;
	const MVert *vert = data->vert;
	const MEdge *edge = data->edge + index;
	float nearest_tmp[3], dist_sq;

	const float *t0, *t1;
	t0 = vert[edge->v1].co;
	t1 = vert[edge->v2].co;

	closest_to_line_segment_v3(nearest_tmp, co, t0, t1);
	dist_sq = len_squared_v3v3(nearest_tmp, co);
	
	if (dist_sq < nearest->dist_sq) {
		nearest->index = index;
		nearest->dist_sq = dist_sq;
		copy_v3_v3(nearest->co, nearest_tmp);
		sub_v3_v3v3(nearest->no, t0, t1);
		normalize_v3(nearest->no);
	}
}

/* Helper, does all the point-spherecast work actually. */
static void mesh_verts_spherecast_do(
        int index, const float v[3], const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	float dist;
	const float *r1;
	float r2[3], i1[3];
	r1 = ray->origin;
	add_v3_v3v3(r2, r1, ray->direction);

	closest_to_line_segment_v3(i1, v, r1, r2);

	/* No hit if closest point is 'behind' the origin of the ray, or too far away from it. */
	if ((dot_v3v3v3(r1, i1, r2) >= 0.0f) && ((dist = len_v3v3(r1, i1)) < hit->dist)) {
		hit->index = index;
		hit->dist = dist;
		copy_v3_v3(hit->co, i1);
	}
}

static void editmesh_verts_spherecast(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	const BVHTreeFromEditMesh *data = userdata;
	BMVert *eve = BM_vert_at_index(data->em->bm, index);

	mesh_verts_spherecast_do(index, eve->co, ray, hit);
}

/* Callback to bvh tree raycast. The tree must have been built using bvhtree_from_mesh_verts.
 * userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree. */
static void mesh_verts_spherecast(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
	const float *v = data->vert[index].co;

	mesh_verts_spherecast_do(index, v, ray, hit);
}

/* Callback to bvh tree raycast. The tree must have been built using bvhtree_from_mesh_edges.
 * userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree. */
static void mesh_edges_spherecast(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
	const MVert *vert = data->vert;
	const MEdge *edge = &data->edge[index];

	const float radius_sq = SQUARE(data->sphere_radius);
	float dist;
	const float *v1, *v2, *r1;
	float r2[3], i1[3], i2[3];
	v1 = vert[edge->v1].co;
	v2 = vert[edge->v2].co;

	/* In case we get a zero-length edge, handle it as a point! */
	if (equals_v3v3(v1, v2)) {
		mesh_verts_spherecast_do(index, v1, ray, hit);
		return;
	}

	r1 = ray->origin;
	add_v3_v3v3(r2, r1, ray->direction);

	if (isect_line_line_v3(v1, v2, r1, r2, i1, i2)) {
		/* No hit if intersection point is 'behind' the origin of the ray, or too far away from it. */
		if ((dot_v3v3v3(r1, i2, r2) >= 0.0f) && ((dist = len_v3v3(r1, i2)) < hit->dist)) {
			const float e_fac = line_point_factor_v3(i1, v1, v2);
			if (e_fac < 0.0f) {
				copy_v3_v3(i1, v1);
			}
			else if (e_fac > 1.0f) {
				copy_v3_v3(i1, v2);
			}
			/* Ensure ray is really close enough from edge! */
			if (len_squared_v3v3(i1, i2) <= radius_sq) {
				hit->index = index;
				hit->dist = dist;
				copy_v3_v3(hit->co, i2);
			}
		}
	}
}

/** \} */

/*
 * BVH builders
 */


/* -------------------------------------------------------------------- */

/** \name Vertex Builder
 * \{ */

static BVHTree *bvhtree_from_editmesh_verts_create_tree(
        float epsilon, int tree_type, int axis,
        BMEditMesh *em, const int verts_num,
        const BLI_bitmap *verts_mask, int verts_num_active)
{
	BM_mesh_elem_table_ensure(em->bm, BM_VERT);
	if (verts_mask) {
		BLI_assert(IN_RANGE_INCL(verts_num_active, 0, verts_num));
	}
	else {
		verts_num_active = verts_num;
	}

	BVHTree *tree = BLI_bvhtree_new(verts_num_active, epsilon, tree_type, axis);

	if (tree) {
		for (int i = 0; i < verts_num; i++) {
			if (verts_mask && !BLI_BITMAP_TEST_BOOL(verts_mask, i)) {
				continue;
			}
			BMVert *eve = BM_vert_at_index(em->bm, i);
			BLI_bvhtree_insert(tree, i, eve->co, 1);
		}
		BLI_assert(BLI_bvhtree_get_size(tree) == verts_num_active);
		BLI_bvhtree_balance(tree);
	}

	return tree;
}

static BVHTree *bvhtree_from_mesh_verts_create_tree(
        float epsilon, int tree_type, int axis,
        const MVert *vert, const int verts_num,
        const BLI_bitmap *verts_mask, int verts_num_active)
{
	BLI_assert(vert != NULL);
	if (verts_mask) {
		BLI_assert(IN_RANGE_INCL(verts_num_active, 0, verts_num));
	}
	else {
		verts_num_active = verts_num;
	}

	BVHTree *tree = BLI_bvhtree_new(verts_num_active, epsilon, tree_type, axis);

	if (tree) {
		for (int i = 0; i < verts_num; i++) {
			if (verts_mask && !BLI_BITMAP_TEST_BOOL(verts_mask, i)) {
				continue;
			}
			BLI_bvhtree_insert(tree, i, vert[i].co, 1);
		}
		BLI_assert(BLI_bvhtree_get_size(tree) == verts_num_active);
		BLI_bvhtree_balance(tree);
	}

	return tree;
}

static void bvhtree_from_mesh_verts_setup_data(
        BVHTreeFromMesh *data, BVHTree *tree, const bool is_cached, float epsilon,
        const MVert *vert, const bool vert_allocated)
{
	memset(data, 0, sizeof(*data));

	data->tree = tree;
	data->cached = is_cached;

	/* a NULL nearest callback works fine
	 * remember the min distance to point is the same as the min distance to BV of point */
	data->nearest_callback = NULL;
	data->raycast_callback = mesh_verts_spherecast;

	data->vert = vert;
	data->vert_allocated = vert_allocated;
	//data->face = DM_get_tessface_array(dm, &data->face_allocated);  /* XXX WHY???? */

	data->sphere_radius = epsilon;
}

/* Builds a bvh tree where nodes are the vertices of the given em */
BVHTree *bvhtree_from_editmesh_verts_ex(
        BVHTreeFromEditMesh *data, BMEditMesh *em,
        const BLI_bitmap *verts_mask, int verts_num_active,
        float epsilon, int tree_type, int axis)
{
	BVHTree *tree = bvhtree_from_editmesh_verts_create_tree(
	        epsilon, tree_type, axis,
	        em, em->bm->totvert, verts_mask, verts_num_active);

	if (tree) {
		memset(data, 0, sizeof(*data));
		data->tree = tree;
		data->em = em;
		data->nearest_callback = NULL;
		data->raycast_callback = editmesh_verts_spherecast;
	}

	return tree;
}

BVHTree *bvhtree_from_editmesh_verts(
        BVHTreeFromEditMesh *data, BMEditMesh *em,
        float epsilon, int tree_type, int axis)
{
	return bvhtree_from_editmesh_verts_ex(
	        data, em,
	        NULL, -1,
	        epsilon, tree_type, axis);
}

/* Builds a bvh tree where nodes are the vertices of the given dm
 * and stores the BVHTree in dm->bvhCache */
BVHTree *bvhtree_from_mesh_verts(
        BVHTreeFromMesh *data, DerivedMesh *dm,
        float epsilon, int tree_type, int axis)
{
	BVHTree *tree;
	MVert *vert;
	bool vert_allocated;

	BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_READ);
	tree = bvhcache_find(dm->bvhCache, BVHTREE_FROM_VERTS);
	BLI_rw_mutex_unlock(&cache_rwlock);

	vert = DM_get_vert_array(dm, &vert_allocated);

	/* Not in cache */
	if (tree == NULL) {
		BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_WRITE);
		tree = bvhcache_find(dm->bvhCache, BVHTREE_FROM_VERTS);
		if (tree == NULL) {

			int vert_num = dm->getNumVerts(dm);
			BLI_assert(vert_num != 0);

			tree = bvhtree_from_mesh_verts_create_tree(
				epsilon, tree_type, axis,
				vert, vert_num, NULL, -1);

			if (tree) {
				/* Save on cache for later use */
				/* printf("BVHTree built and saved on cache\n"); */
				bvhcache_insert(&dm->bvhCache, tree, BVHTREE_FROM_VERTS);
			}
		}
		BLI_rw_mutex_unlock(&cache_rwlock);
	}
	else {
		/* printf("BVHTree is already build, using cached tree\n"); */
	}

	if (tree) {
		/* Setup BVHTreeFromMesh */
		bvhtree_from_mesh_verts_setup_data(
		        data, tree, true, epsilon, vert, vert_allocated);
	}
	else {
		if (vert_allocated) {
			MEM_freeN(vert);
		}
		memset(data, 0, sizeof(*data));
	}
	return tree;
}

/**
 * Builds a bvh tree where nodes are the given vertices (note: does not copy given mverts!).
 * \param vert_allocated if true, vert freeing will be done when freeing data.
 * \param verts_mask if not null, true elements give which vert to add to BVH tree.
 * \param verts_num_active if >= 0, number of active verts to add to BVH tree (else will be computed from mask).
 */
BVHTree *bvhtree_from_mesh_verts_ex(
        BVHTreeFromMesh *data, const MVert *vert, const int verts_num, const bool vert_allocated,
        const BLI_bitmap *verts_mask, int verts_num_active,
        float epsilon, int tree_type, int axis)
{
	BVHTree *tree = bvhtree_from_mesh_verts_create_tree(
	        epsilon, tree_type, axis, vert, verts_num, verts_mask, verts_num_active);

	/* Setup BVHTreeFromMesh */
	bvhtree_from_mesh_verts_setup_data(
	        data, tree, false, epsilon, vert, vert_allocated);

	return tree;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Edge Builder
 * \{ */

static BVHTree *bvhtree_from_editmesh_edges_create_tree(
        float epsilon, int tree_type, int axis,
        BMEditMesh *em, const int edges_num,
        const BLI_bitmap *edges_mask, int edges_num_active)
{
	BM_mesh_elem_table_ensure(em->bm, BM_EDGE);
	if (edges_mask) {
		BLI_assert(IN_RANGE_INCL(edges_num_active, 0, edges_num));
	}
	else {
		edges_num_active = edges_num;
	}

	BVHTree *tree = BLI_bvhtree_new(edges_num_active, epsilon, tree_type, axis);

	if (tree) {
		int i;
		BMIter iter;
		BMEdge *eed;
		BM_ITER_MESH_INDEX (eed, &iter, em->bm, BM_EDGES_OF_MESH, i) {
			if (edges_mask && !BLI_BITMAP_TEST_BOOL(edges_mask, i)) {
				continue;
			}
			float co[2][3];
			copy_v3_v3(co[0], eed->v1->co);
			copy_v3_v3(co[1], eed->v2->co);

			BLI_bvhtree_insert(tree, i, co[0], 2);
		}
		BLI_assert(BLI_bvhtree_get_size(tree) == edges_num_active);
		BLI_bvhtree_balance(tree);
	}

	return tree;
}

static BVHTree *bvhtree_from_mesh_edges_create_tree(
        const MVert *vert, const MEdge *edge, const int edge_num,
        const BLI_bitmap *edges_mask, int edges_num_active,
        float epsilon, int tree_type, int axis)
{
	if (edges_mask) {
		BLI_assert(IN_RANGE_INCL(edges_num_active, 0, edge_num));
	}
	else {
		edges_num_active = edge_num;
	}
	BLI_assert(vert != NULL);
	BLI_assert(edge != NULL);

	/* Create a bvh-tree of the given target */
	BVHTree *tree = BLI_bvhtree_new(edges_num_active, epsilon, tree_type, axis);
	if (tree) {
		for (int i = 0; i < edge_num; i++) {
			if (edges_mask && !BLI_BITMAP_TEST_BOOL(edges_mask, i)) {
				continue;
			}
			float co[2][3];
			copy_v3_v3(co[0], vert[edge[i].v1].co);
			copy_v3_v3(co[1], vert[edge[i].v2].co);

			BLI_bvhtree_insert(tree, i, co[0], 2);
		}
		BLI_bvhtree_balance(tree);
	}

	return tree;
}

static void bvhtree_from_mesh_edges_setup_data(
        BVHTreeFromMesh *data, BVHTree *tree,
        const bool is_cached, float epsilon,
        const MVert *vert, const bool vert_allocated,
        const MEdge *edge, const bool edge_allocated)
{
	memset(data, 0, sizeof(*data));

	data->tree = tree;

	data->cached = is_cached;

	data->nearest_callback = mesh_edges_nearest_point;
	data->raycast_callback = mesh_edges_spherecast;

	data->vert = vert;
	data->vert_allocated = vert_allocated;
	data->edge = edge;
	data->edge_allocated = edge_allocated;

	data->sphere_radius = epsilon;
}

/* Builds a bvh tree where nodes are the edges of the given em */
BVHTree *bvhtree_from_editmesh_edges_ex(
        BVHTreeFromEditMesh *data, BMEditMesh *em,
        const BLI_bitmap *edges_mask, int edges_num_active,
        float epsilon, int tree_type, int axis)
{
	int edge_num = em->bm->totedge;

	BVHTree *tree = bvhtree_from_editmesh_edges_create_tree(
	        epsilon, tree_type, axis,
	        em, edge_num, edges_mask, edges_num_active);

	if (tree) {
		memset(data, 0, sizeof(*data));
		data->tree = tree;
		data->em = em;
		data->nearest_callback = NULL;  /* TODO */
		data->raycast_callback = NULL;  /* TODO */
	}

	return tree;
}

BVHTree *bvhtree_from_editmesh_edges(
        BVHTreeFromEditMesh *data, BMEditMesh *em,
        float epsilon, int tree_type, int axis)
{
	return bvhtree_from_editmesh_edges_ex(
	        data, em,
	        NULL, -1,
	        epsilon, tree_type, axis);
}

/* Builds a bvh tree where nodes are the edges of the given dm */
BVHTree *bvhtree_from_mesh_edges(
        BVHTreeFromMesh *data, DerivedMesh *dm,
        float epsilon, int tree_type, int axis)
{
	BVHTree *tree;
	MVert *vert;
	MEdge *edge;
	bool vert_allocated, edge_allocated;

	BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_READ);
	tree = bvhcache_find(dm->bvhCache, BVHTREE_FROM_EDGES);
	BLI_rw_mutex_unlock(&cache_rwlock);

	vert = DM_get_vert_array(dm, &vert_allocated);
	edge = DM_get_edge_array(dm, &edge_allocated);

	/* Not in cache */
	if (tree == NULL) {
		BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_WRITE);
		tree = bvhcache_find(dm->bvhCache, BVHTREE_FROM_EDGES);
		if (tree == NULL) {
			tree = bvhtree_from_mesh_edges_create_tree(
			        vert, edge, dm->getNumEdges(dm),
			        NULL, -1, epsilon, tree_type, axis);

			/* Save on cache for later use */
			/* printf("BVHTree built and saved on cache\n"); */
			bvhcache_insert(&dm->bvhCache, tree, BVHTREE_FROM_EDGES);
		}
		BLI_rw_mutex_unlock(&cache_rwlock);
	}
	else {
		/* printf("BVHTree is already build, using cached tree\n"); */
	}

	if (tree) {
		/* Setup BVHTreeFromMesh */
		bvhtree_from_mesh_edges_setup_data(
		        data, tree, true, epsilon, vert, vert_allocated, edge, edge_allocated);
	}
	else {
		if (vert_allocated) {
			MEM_freeN(vert);
		}
		if (edge_allocated) {
			MEM_freeN(edge);
		}
		memset(data, 0, sizeof(*data));
	}
	return tree;
}

/**
 * Builds a bvh tree where nodes are the given edges .
 * \param vert/edge_allocated if true, elem freeing will be done when freeing data.
 * \param edges_mask if not null, true elements give which vert to add to BVH tree.
 * \param edges_num_active if >= 0, number of active edges to add to BVH tree (else will be computed from mask).
 */
BVHTree *bvhtree_from_mesh_edges_ex(
        BVHTreeFromMesh *data,
        const MVert *vert, const bool vert_allocated,
        const MEdge *edge, const int edges_num, const bool edge_allocated,
        const BLI_bitmap *edges_mask, int edges_num_active,
        float epsilon, int tree_type, int axis)
{
	BVHTree *tree = bvhtree_from_mesh_edges_create_tree(
	        vert, edge, edges_num, edges_mask, edges_num_active,
	        epsilon, tree_type, axis);

	/* Setup BVHTreeFromMesh */
	bvhtree_from_mesh_edges_setup_data(
	        data, tree, false, epsilon, vert, vert_allocated, edge, edge_allocated);

	return tree;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Tessellated Face Builder
 * \{ */

static BVHTree *bvhtree_from_mesh_faces_create_tree(
        float epsilon, int tree_type, int axis,
        const MVert *vert, const MFace *face, const int faces_num,
        const BLI_bitmap *faces_mask, int faces_num_active)
{
	BVHTree *tree = NULL;
	int i;

	if (faces_num) {
		if (faces_mask) {
			BLI_assert(IN_RANGE_INCL(faces_num_active, 0, faces_num));
		}
		else {
			faces_num_active = faces_num;
		}

		/* Create a bvh-tree of the given target */
		/* printf("%s: building BVH, total=%d\n", __func__, numFaces); */
		tree = BLI_bvhtree_new(faces_num_active, epsilon, tree_type, axis);
		if (tree) {
			if (vert && face) {
				for (i = 0; i < faces_num; i++) {
					float co[4][3];
					if (faces_mask && !BLI_BITMAP_TEST_BOOL(faces_mask, i)) {
						continue;
					}

					copy_v3_v3(co[0], vert[face[i].v1].co);
					copy_v3_v3(co[1], vert[face[i].v2].co);
					copy_v3_v3(co[2], vert[face[i].v3].co);
					if (face[i].v4)
						copy_v3_v3(co[3], vert[face[i].v4].co);

					BLI_bvhtree_insert(tree, i, co[0], face[i].v4 ? 4 : 3);
				}
			}
			BLI_assert(BLI_bvhtree_get_size(tree) == faces_num_active);
			BLI_bvhtree_balance(tree);
		}
	}

	return tree;
}

static void bvhtree_from_mesh_faces_setup_data(
        BVHTreeFromMesh *data, BVHTree *tree, const bool is_cached, float epsilon,
        const MVert *vert, const bool vert_allocated,
        const MFace *face, const bool face_allocated)
{
	memset(data, 0, sizeof(*data));

	data->tree = tree;
	data->cached = is_cached;

	data->nearest_callback = mesh_faces_nearest_point;
	data->raycast_callback = mesh_faces_spherecast;

	data->vert = vert;
	data->vert_allocated = vert_allocated;
	data->face = face;
	data->face_allocated = face_allocated;

	data->sphere_radius = epsilon;
}

/* Builds a bvh tree where nodes are the tesselated faces of the given dm */
BVHTree *bvhtree_from_mesh_faces(
        BVHTreeFromMesh *data, DerivedMesh *dm,
        float epsilon, int tree_type, int axis)
{
	BVHTree *tree;
	MVert *vert = NULL;
	MFace *face = NULL;
	bool vert_allocated = false, face_allocated = false;

	BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_READ);
	tree = bvhcache_find(dm->bvhCache, BVHTREE_FROM_FACES);
	BLI_rw_mutex_unlock(&cache_rwlock);

	vert = DM_get_vert_array(dm, &vert_allocated);
	face = DM_get_tessface_array(dm, &face_allocated);

	/* Not in cache */
	if (tree == NULL) {
		BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_WRITE);
		tree = bvhcache_find(dm->bvhCache, BVHTREE_FROM_FACES);
		if (tree == NULL) {
			int numFaces = dm->getNumTessFaces(dm);
			BLI_assert(!(numFaces == 0 && dm->getNumPolys(dm) != 0));

			tree = bvhtree_from_mesh_faces_create_tree(
			        epsilon, tree_type, axis,
			        vert, face, numFaces, NULL, -1);
			if (tree) {
				/* Save on cache for later use */
				/* printf("BVHTree built and saved on cache\n"); */
				bvhcache_insert(&dm->bvhCache, tree, BVHTREE_FROM_FACES);
			}
		}
		BLI_rw_mutex_unlock(&cache_rwlock);
	}
	else {
		/* printf("BVHTree is already build, using cached tree\n"); */
	}

	if (tree) {
		/* Setup BVHTreeFromMesh */
		bvhtree_from_mesh_faces_setup_data(
		        data, tree, true, epsilon, vert, vert_allocated, face, face_allocated);
	}
	else {
		if (vert_allocated) {
			MEM_freeN(vert);
		}
		if (face_allocated) {
			MEM_freeN(face);
		}
		memset(data, 0, sizeof(*data));
	}
	return tree;
}

/**
 * Builds a bvh tree where nodes are the given tessellated faces (note: does not copy given mfaces!).
 * \param vert_allocated: if true, vert freeing will be done when freeing data.
 * \param face_allocated: if true, face freeing will be done when freeing data.
 * \param faces_mask: if not null, true elements give which faces to add to BVH tree.
 * \param faces_num_active: if >= 0, number of active faces to add to BVH tree (else will be computed from mask).
 */
BVHTree *bvhtree_from_mesh_faces_ex(
        BVHTreeFromMesh *data, const MVert *vert, const bool vert_allocated,
        const MFace *face, const int numFaces, const bool face_allocated,
        const BLI_bitmap *faces_mask, int faces_num_active,
        float epsilon, int tree_type, int axis)
{
	BVHTree *tree = bvhtree_from_mesh_faces_create_tree(
	        epsilon, tree_type, axis,
	        vert, face, numFaces,
	        faces_mask, faces_num_active);

	/* Setup BVHTreeFromMesh */
	bvhtree_from_mesh_faces_setup_data(
	        data, tree, false, epsilon, vert, vert_allocated, face, face_allocated);

	return tree;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name LoopTri Face Builder
 * \{ */

static BVHTree *bvhtree_from_editmesh_looptri_create_tree(
        float epsilon, int tree_type, int axis,
        BMEditMesh *em, const int looptri_num,
        const BLI_bitmap *looptri_mask, int looptri_num_active)
{
	BVHTree *tree = NULL;
	int i;

	if (looptri_num) {
		if (looptri_mask) {
			BLI_assert(IN_RANGE_INCL(looptri_num_active, 0, looptri_num));
		}
		else {
			looptri_num_active = looptri_num;
		}

		/* Create a bvh-tree of the given target */
		/* printf("%s: building BVH, total=%d\n", __func__, numFaces); */
		tree = BLI_bvhtree_new(looptri_num_active, epsilon, tree_type, axis);
		if (tree) {
			if (em) {
				const struct BMLoop *(*looptris)[3] = (void *)em->looptris;

				/* Insert BMesh-tessellation triangles into the bvh tree, unless they are hidden
				 * and/or selected. Even if the faces themselves are not selected for the snapped
				 * transform, having a vertex selected means the face (and thus it's tessellated
				 * triangles) will be moving and will not be a good snap targets. */
				for (i = 0; i < looptri_num; i++) {
					const BMLoop **ltri = looptris[i];
					bool insert = looptri_mask ? BLI_BITMAP_TEST_BOOL(looptri_mask, i) : true;

					if (insert) {
						/* No reason found to block hit-testing the triangle for snap, so insert it now.*/
						float co[3][3];
						copy_v3_v3(co[0], ltri[0]->v->co);
						copy_v3_v3(co[1], ltri[1]->v->co);
						copy_v3_v3(co[2], ltri[2]->v->co);

						BLI_bvhtree_insert(tree, i, co[0], 3);
					}
				}
			}
			BLI_assert(BLI_bvhtree_get_size(tree) == looptri_num_active);
			BLI_bvhtree_balance(tree);
		}
	}

	return tree;
}

static BVHTree *bvhtree_from_mesh_looptri_create_tree(
        float epsilon, int tree_type, int axis,
        const MVert *vert, const MLoop *mloop, const MLoopTri *looptri, const int looptri_num,
        const BLI_bitmap *looptri_mask, int looptri_num_active)
{
	BVHTree *tree = NULL;
	int i;

	if (looptri_num) {
		if (looptri_mask) {
			BLI_assert(IN_RANGE_INCL(looptri_num_active, 0, looptri_num));
		}
		else {
			looptri_num_active = looptri_num;
		}

		/* Create a bvh-tree of the given target */
		/* printf("%s: building BVH, total=%d\n", __func__, numFaces); */
		tree = BLI_bvhtree_new(looptri_num_active, epsilon, tree_type, axis);
		if (tree) {
			if (vert && looptri) {
				for (i = 0; i < looptri_num; i++) {
					float co[3][3];
					if (looptri_mask && !BLI_BITMAP_TEST_BOOL(looptri_mask, i)) {
						continue;
					}

					copy_v3_v3(co[0], vert[mloop[looptri[i].tri[0]].v].co);
					copy_v3_v3(co[1], vert[mloop[looptri[i].tri[1]].v].co);
					copy_v3_v3(co[2], vert[mloop[looptri[i].tri[2]].v].co);

					BLI_bvhtree_insert(tree, i, co[0], 3);
				}
			}
			BLI_assert(BLI_bvhtree_get_size(tree) == looptri_num_active);
			BLI_bvhtree_balance(tree);
		}
	}

	return tree;
}

static void bvhtree_from_mesh_looptri_setup_data(
        BVHTreeFromMesh *data, BVHTree *tree, const bool is_cached, float epsilon,
        const MVert *vert, const bool vert_allocated,
        const MLoop *mloop, const bool loop_allocated,
        const MLoopTri *looptri, const bool looptri_allocated)
{
	memset(data, 0, sizeof(*data));

	data->tree = tree;
	data->cached = is_cached;

	data->nearest_callback = mesh_looptri_nearest_point;
	data->raycast_callback = mesh_looptri_spherecast;

	data->vert = vert;
	data->vert_allocated = vert_allocated;
	data->loop = mloop;
	data->loop_allocated = loop_allocated;
	data->looptri = looptri;
	data->looptri_allocated = looptri_allocated;

	data->sphere_radius = epsilon;
}

/**
 * Builds a bvh tree where nodes are the looptri faces of the given bm
 */
BVHTree *bvhtree_from_editmesh_looptri_ex(
        BVHTreeFromEditMesh *data, BMEditMesh *em,
        const BLI_bitmap *looptri_mask, int looptri_num_active,
        float epsilon, int tree_type, int axis, BVHCache **bvhCache)
{
	/* BMESH specific check that we have tessfaces,
	 * we _could_ tessellate here but rather not - campbell */

	BVHTree *tree;
	if (bvhCache) {
		BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_READ);
		tree = bvhcache_find(*bvhCache, BVHTREE_FROM_EM_LOOPTRI);
		BLI_rw_mutex_unlock(&cache_rwlock);
		if (tree == NULL) {
			BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_WRITE);
			tree = bvhcache_find(*bvhCache, BVHTREE_FROM_EM_LOOPTRI);
			if (tree == NULL) {
				tree = bvhtree_from_editmesh_looptri_create_tree(
				        epsilon, tree_type, axis,
				        em, em->tottri, looptri_mask, looptri_num_active);
				if (tree) {
					/* Save on cache for later use */
					/* printf("BVHTree built and saved on cache\n"); */
					bvhcache_insert(bvhCache, tree, BVHTREE_FROM_EM_LOOPTRI);
				}
			}
			BLI_rw_mutex_unlock(&cache_rwlock);
		}
	}
	else {
		tree = bvhtree_from_editmesh_looptri_create_tree(
		        epsilon, tree_type, axis,
		        em, em->tottri, looptri_mask, looptri_num_active);
	}

	if (tree) {
		data->tree = tree;
		data->nearest_callback = editmesh_looptri_nearest_point;
		data->raycast_callback = editmesh_looptri_spherecast;
		data->sphere_radius = 0.0f;
		data->em = em;
		data->cached = bvhCache != NULL;
	}
	return tree;
}

BVHTree *bvhtree_from_editmesh_looptri(
        BVHTreeFromEditMesh *data, BMEditMesh *em,
        float epsilon, int tree_type, int axis, BVHCache **bvhCache)
{
	return bvhtree_from_editmesh_looptri_ex(
	        data, em, NULL, -1,
	        epsilon, tree_type, axis, bvhCache);
}

/**
 * Builds a bvh tree where nodes are the looptri faces of the given dm
 *
 * \note for editmesh this is currently a duplicate of bvhtree_from_mesh_faces
 */
BVHTree *bvhtree_from_mesh_looptri(
        BVHTreeFromMesh *data, DerivedMesh *dm,
        float epsilon, int tree_type, int axis)
{
	BVHTree *tree;
	MVert *mvert = NULL;
	MLoop *mloop = NULL;
	const MLoopTri *looptri = NULL;
	bool vert_allocated = false;
	bool loop_allocated = false;

	BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_READ);
	tree = bvhcache_find(dm->bvhCache, BVHTREE_FROM_LOOPTRI);
	BLI_rw_mutex_unlock(&cache_rwlock);

	MPoly *mpoly;
	bool poly_allocated = false;

	mvert = DM_get_vert_array(dm, &vert_allocated);
	mpoly = DM_get_poly_array(dm, &poly_allocated);

	mloop = DM_get_loop_array(dm, &loop_allocated);
	looptri = dm->getLoopTriArray(dm);

	if (poly_allocated) {
		MEM_freeN(mpoly);
	}

	/* Not in cache */
	if (tree == NULL) {
		BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_WRITE);
		tree = bvhcache_find(dm->bvhCache, BVHTREE_FROM_LOOPTRI);
		if (tree == NULL) {
			int looptri_num = dm->getNumLoopTri(dm);

			/* this assert checks we have looptris,
			 * if not caller should use DM_ensure_looptri() */
			BLI_assert(!(looptri_num == 0 && dm->getNumPolys(dm) != 0));

			tree = bvhtree_from_mesh_looptri_create_tree(
			        epsilon, tree_type, axis,
			        mvert, mloop, looptri, looptri_num, NULL, -1);
			if (tree) {
				/* Save on cache for later use */
				/* printf("BVHTree built and saved on cache\n"); */
				bvhcache_insert(&dm->bvhCache, tree, BVHTREE_FROM_LOOPTRI);
			}
		}
		BLI_rw_mutex_unlock(&cache_rwlock);
	}
	else {
		/* printf("BVHTree is already build, using cached tree\n"); */
	}

	if (tree) {
		/* Setup BVHTreeFromMesh */
		bvhtree_from_mesh_looptri_setup_data(
		        data, tree, true, epsilon,
		        mvert, vert_allocated,
		        mloop, loop_allocated,
		        looptri, false);
	}
	else {
		if (vert_allocated) {
			MEM_freeN(mvert);
		}
		if (loop_allocated) {
			MEM_freeN(mloop);
		}
		memset(data, 0, sizeof(*data));
	}

	return tree;
}

BVHTree *bvhtree_from_mesh_looptri_ex(
        BVHTreeFromMesh *data,
        const struct MVert *vert, const bool vert_allocated,
        const struct MLoop *mloop, const bool loop_allocated,
        const struct MLoopTri *looptri, const int looptri_num, const bool looptri_allocated,
        const BLI_bitmap *looptri_mask, int looptri_num_active,
        float epsilon, int tree_type, int axis)
{
	BVHTree *tree = bvhtree_from_mesh_looptri_create_tree(
	        epsilon, tree_type, axis,
	        vert, mloop, looptri, looptri_num,
	        looptri_mask, looptri_num_active);

	/* Setup BVHTreeFromMesh */
	bvhtree_from_mesh_looptri_setup_data(
	        data, tree, false, epsilon,
	        vert, vert_allocated,
	        mloop, loop_allocated,
	        looptri, looptri_allocated);

	return tree;
}

/** \} */


/* Frees data allocated by a call to bvhtree_from_editmesh_*. */
void free_bvhtree_from_editmesh(struct BVHTreeFromEditMesh *data)
{
	if (data->tree) {
		if (!data->cached) {
			BLI_bvhtree_free(data->tree);
		}
		memset(data, 0, sizeof(*data));
	}
}

/* Frees data allocated by a call to bvhtree_from_mesh_*. */
void free_bvhtree_from_mesh(struct BVHTreeFromMesh *data)
{
	if (data->tree && !data->cached) {
		BLI_bvhtree_free(data->tree);
	}

	if (data->vert_allocated) {
		MEM_freeN((void *)data->vert);
	}
	if (data->edge_allocated) {
		MEM_freeN((void *)data->edge);
	}
	if (data->face_allocated) {
		MEM_freeN((void *)data->face);
	}
	if (data->loop_allocated) {
		MEM_freeN((void *)data->loop);
	}
	if (data->looptri_allocated) {
		MEM_freeN((void *)data->looptri);
	}

	memset(data, 0, sizeof(*data));
}


/* -------------------------------------------------------------------- */

/** \name BVHCache
 * \{ */

typedef struct BVHCacheItem {
	int type;
	BVHTree *tree;

} BVHCacheItem;

/**
 * Queries a bvhcache for the cache bvhtree of the request type
 */
BVHTree *bvhcache_find(BVHCache *cache, int type)
{
	while (cache) {
		const BVHCacheItem *item = cache->link;
		if (item->type == type) {
			return item->tree;
		}
		cache = cache->next;
	}
	return NULL;
}

bool bvhcache_has_tree(const BVHCache *cache, const BVHTree *tree)
{
	while (cache) {
		const BVHCacheItem *item = cache->link;
		if (item->tree == tree) {
			return true;
		}
		cache = cache->next;
	}
	return false;
}

/**
 * Inserts a BVHTree of the given type under the cache
 * After that the caller no longer needs to worry when to free the BVHTree
 * as that will be done when the cache is freed.
 *
 * A call to this assumes that there was no previous cached tree of the given type
 */
void bvhcache_insert(BVHCache **cache_p, BVHTree *tree, int type)
{
	BVHCacheItem *item = NULL;

	assert(tree != NULL);
	assert(bvhcache_find(*cache_p, type) == NULL);

	item = MEM_mallocN(sizeof(BVHCacheItem), "BVHCacheItem");
	assert(item != NULL);

	item->type = type;
	item->tree = tree;

	BLI_linklist_prepend(cache_p, item);
}

/**
 * inits and frees a bvhcache
 */
void bvhcache_init(BVHCache **cache_p)
{
	*cache_p = NULL;
}

static void bvhcacheitem_free(void *_item)
{
	BVHCacheItem *item = (BVHCacheItem *)_item;

	BLI_bvhtree_free(item->tree);
	MEM_freeN(item);
}


void bvhcache_free(BVHCache **cache_p)
{
	BLI_linklist_free(*cache_p, (LinkNodeFreeFP)bvhcacheitem_free);
	*cache_p = NULL;
}

/** \} */
