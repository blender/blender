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
static void editmesh_faces_nearest_point(void *userdata, int index, const float co[3], BVHTreeNearest *nearest)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *) userdata;
	BMEditMesh *em = data->em_evil;
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
static void editmesh_faces_spherecast(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *) userdata;
	BMEditMesh *em = data->em_evil;
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
	const BVHTreeFromMesh *UNUSED(data), int index, const float v[3], const BVHTreeRay *ray, BVHTreeRayHit *hit)
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

/* Callback to bvh tree raycast. The tree must have been built using bvhtree_from_mesh_verts.
 * userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree. */
static void mesh_verts_spherecast(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *)userdata;
	const float *v = data->vert[index].co;

	mesh_verts_spherecast_do(data, index, v, ray, hit);
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
		mesh_verts_spherecast_do(data, index, v1, ray, hit);
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

static BVHTree *bvhtree_from_mesh_verts_create_tree(
        float epsilon, int tree_type, int axis,
        BMEditMesh *em, const int *index_array,
        MVert *vert, const int numVerts,
        BLI_bitmap *mask, int numVerts_active)
{
	BVHTree *tree = NULL;
	BMVert *eve = NULL;
	int i;
	int index = 0;
	if (em != NULL) {
		BM_mesh_elem_table_ensure(em->bm, BM_VERT);
	}
	if (vert) {
		if (mask && numVerts_active < 0) {
			numVerts_active = 0;
			for (i = 0; i < numVerts; i++) {
				if (BLI_BITMAP_TEST_BOOL(mask, i)) {
					if (em != NULL) {
						if (index_array) {
							index = index_array[i];
							if (index == ORIGINDEX_NONE) {
								continue;
							}
						}
						else {
							index = i;
						}

						eve = BM_vert_at_index(em->bm, index);
						if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN) ||
						    BM_elem_flag_test(eve, BM_ELEM_SELECT))
						{
							continue;
						}
					}
					numVerts_active++;
				}
			}
		}
		else if (!mask) {
			numVerts_active = numVerts;
		}

		tree = BLI_bvhtree_new(numVerts_active, epsilon, tree_type, axis);

		if (tree) {
			for (i = 0; i < numVerts; i++) {
				if (mask && !BLI_BITMAP_TEST_BOOL(mask, i)) {
					continue;
				}
				if (em != NULL) {
					if (index_array) {
						index = index_array[i];
						if (index == ORIGINDEX_NONE) {
							continue;
						}
					}
					else {
						index = i;
					}

					eve = BM_vert_at_index(em->bm, index);
					if (BM_elem_flag_test(eve, BM_ELEM_HIDDEN) ||
					    BM_elem_flag_test(eve, BM_ELEM_SELECT))
					{
						continue;
					}
				}
				BLI_bvhtree_insert(tree, i, vert[i].co, 1);
			}

			BLI_bvhtree_balance(tree);
		}
	}

	return tree;
}

static void bvhtree_from_mesh_verts_setup_data(
        BVHTreeFromMesh *data, BVHTree *tree, const bool is_cached, float epsilon,
        MVert *vert, const bool vert_allocated)
{
	memset(data, 0, sizeof(*data));

	if (tree) {
		data->tree = tree;
		data->cached = is_cached;

		/* a NULL nearest callback works fine
		 * remember the min distance to point is the same as the min distance to BV of point */
		data->nearest_callback = NULL;
		data->raycast_callback = mesh_verts_spherecast;
		data->nearest_to_ray_callback = NULL;

		data->vert = vert;
		data->vert_allocated = vert_allocated;
		//data->face = DM_get_tessface_array(dm, &data->face_allocated);  /* XXX WHY???? */

		data->sphere_radius = epsilon;
	}
	else {
		if (vert_allocated) {
			MEM_freeN(vert);
		}
	}
}

/* Builds a bvh tree where nodes are the vertices of the given dm */
BVHTree *bvhtree_from_mesh_verts(BVHTreeFromMesh *data, DerivedMesh *dm, float epsilon, int tree_type, int axis)
{
	BMEditMesh *em = data->em_evil;
	const int bvhcache_type = em ? BVHTREE_FROM_VERTS_EDITMESH_SNAP : BVHTREE_FROM_VERTS;
	BVHTree *tree;
	MVert *vert;
	bool vert_allocated;

	BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_READ);
	tree = bvhcache_find(&dm->bvhCache, bvhcache_type);
	BLI_rw_mutex_unlock(&cache_rwlock);

	vert = DM_get_vert_array(dm, &vert_allocated);

	/* Not in cache */
	if (tree == NULL) {
		BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_WRITE);
		tree = bvhcache_find(&dm->bvhCache, bvhcache_type);
		if (tree == NULL) {
			int vert_num, *index_array = NULL;
			if (em != NULL) {
				vert_num = em->bm->totvert;
				index_array = dm->getVertDataArray(dm, CD_ORIGINDEX);
			}
			else {
				vert_num = dm->getNumVerts(dm);
				BLI_assert(vert_num != 0);
			}
			tree = bvhtree_from_mesh_verts_create_tree(
				epsilon, tree_type, axis,
				em, index_array,
				vert, vert_num, NULL, -1);

			if (tree) {
				/* Save on cache for later use */
				/* printf("BVHTree built and saved on cache\n"); */
				bvhcache_insert(&dm->bvhCache, tree, bvhcache_type);
			}
		}
		BLI_rw_mutex_unlock(&cache_rwlock);
	}
	else {
		/* printf("BVHTree is already build, using cached tree\n"); */
	}

	/* Setup BVHTreeFromMesh */
	bvhtree_from_mesh_verts_setup_data(data, tree, true, epsilon, vert, vert_allocated);

	return data->tree;
}

/**
 * Builds a bvh tree where nodes are the given vertices (note: does not copy given mverts!).
 * \param vert_allocated if true, vert freeing will be done when freeing data.
 * \param mask if not null, true elements give which vert to add to BVH tree.
 * \param numVerts_active if >= 0, number of active verts to add to BVH tree (else will be computed from mask).
 */
BVHTree *bvhtree_from_mesh_verts_ex(
        BVHTreeFromMesh *data, MVert *vert, const int numVerts, const bool vert_allocated,
        BLI_bitmap *mask, int numVerts_active,
        float epsilon, int tree_type, int axis)
{
	BVHTree *tree = bvhtree_from_mesh_verts_create_tree(epsilon, tree_type, axis, NULL, NULL, vert, numVerts, mask, numVerts_active);

	/* Setup BVHTreeFromMesh */
	bvhtree_from_mesh_verts_setup_data(data, tree, false, epsilon, vert, vert_allocated);

	return data->tree;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Edge Builder
 * \{ */

/* Builds a bvh tree where nodes are the edges of the given dm */
BVHTree *bvhtree_from_mesh_edges(BVHTreeFromMesh *data, DerivedMesh *dm, float epsilon, int tree_type, int axis)
{
	BVHTree *tree;
	MVert *vert;
	MEdge *edge;
	bool vert_allocated, edge_allocated;

	BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_READ);
	tree = bvhcache_find(&dm->bvhCache, BVHTREE_FROM_EDGES);
	BLI_rw_mutex_unlock(&cache_rwlock);

	vert = DM_get_vert_array(dm, &vert_allocated);
	edge = DM_get_edge_array(dm, &edge_allocated);

	/* Not in cache */
	if (tree == NULL) {
		BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_WRITE);
		tree = bvhcache_find(&dm->bvhCache, BVHTREE_FROM_EDGES);
		if (tree == NULL) {
			int i;
			int numEdges = dm->getNumEdges(dm);

			if (vert != NULL && edge != NULL) {
				/* Create a bvh-tree of the given target */
				tree = BLI_bvhtree_new(numEdges, epsilon, tree_type, axis);
				if (tree != NULL) {
					for (i = 0; i < numEdges; i++) {
						float co[2][3];
						copy_v3_v3(co[0], vert[edge[i].v1].co);
						copy_v3_v3(co[1], vert[edge[i].v2].co);

						BLI_bvhtree_insert(tree, i, co[0], 2);
					}
					BLI_bvhtree_balance(tree);

					/* Save on cache for later use */
					/* printf("BVHTree built and saved on cache\n"); */
					bvhcache_insert(&dm->bvhCache, tree, BVHTREE_FROM_EDGES);
				}
			}
		}
		BLI_rw_mutex_unlock(&cache_rwlock);
	}
	else {
		/* printf("BVHTree is already build, using cached tree\n"); */
	}


	/* Setup BVHTreeFromMesh */
	memset(data, 0, sizeof(*data));
	data->tree = tree;

	if (data->tree) {
		data->cached = true;

		data->nearest_callback = mesh_edges_nearest_point;
		data->raycast_callback = mesh_edges_spherecast;
		data->nearest_to_ray_callback = NULL;

		data->vert = vert;
		data->vert_allocated = vert_allocated;
		data->edge = edge;
		data->edge_allocated = edge_allocated;

		data->sphere_radius = epsilon;
	}
	else {
		if (vert_allocated) {
			MEM_freeN(vert);
		}
		if (edge_allocated) {
			MEM_freeN(edge);
		}
	}
	return data->tree;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name Tessellated Face Builder
 * \{ */

static BVHTree *bvhtree_from_mesh_faces_create_tree(
        float epsilon, int tree_type, int axis,
        BMEditMesh *em, const bool em_all,
        MVert *vert, MFace *face, const int numFaces,
        BLI_bitmap *mask, int numFaces_active)
{
	BVHTree *tree = NULL;
	int i;

	if (numFaces) {
		if (mask && numFaces_active < 0) {
			numFaces_active = 0;
			for (i = 0; i < numFaces; i++) {
				if (BLI_BITMAP_TEST_BOOL(mask, i)) {
					numFaces_active++;
				}
			}
		}
		else if (!mask) {
			numFaces_active = numFaces;
		}

		/* Create a bvh-tree of the given target */
		/* printf("%s: building BVH, total=%d\n", __func__, numFaces); */
		tree = BLI_bvhtree_new(numFaces_active, epsilon, tree_type, axis);
		if (tree) {
			if (em) {
				const struct BMLoop *(*looptris)[3] = (void *)em->looptris;

				/* avoid double-up on face searches for quads-ngons */
				bool insert_prev = false;
				BMFace *f_prev = NULL;

				/* data->em_evil is only set for snapping, and only for the mesh of the object
				 * which is currently open in edit mode. When set, the bvhtree should not contain
				 * faces that will interfere with snapping (e.g. faces that are hidden/selected
				 * or faces that have selected verts). */

				/* Insert BMesh-tessellation triangles into the bvh tree, unless they are hidden
				 * and/or selected. Even if the faces themselves are not selected for the snapped
				 * transform, having a vertex selected means the face (and thus it's tessellated
				 * triangles) will be moving and will not be a good snap targets. */
				for (i = 0; i < numFaces; i++) {
					const BMLoop **ltri = looptris[i];
					BMFace *f = ltri[0]->f;
					bool insert = mask ? BLI_BITMAP_TEST_BOOL(mask, i) : true;

					/* Start with the assumption the triangle should be included for snapping. */
					if (f == f_prev) {
						insert = insert_prev;
					}
					else if (insert) {
						if (em_all) {
							/* pass */
						}
						else if (BM_elem_flag_test(f, BM_ELEM_SELECT) || BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
							/* Don't insert triangles tessellated from faces that are hidden or selected */
							insert = false;
						}
						else {
							BMLoop *l_iter, *l_first;
							l_iter = l_first = BM_FACE_FIRST_LOOP(f);
							do {
								if (BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)) {
									/* Don't insert triangles tessellated from faces that have any selected verts */
									insert = false;
									break;
								}
							} while ((l_iter = l_iter->next) != l_first);
						}

						/* skip if face doesn't change */
						f_prev = f;
						insert_prev = insert;
					}

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
			else {
				if (vert && face) {
					for (i = 0; i < numFaces; i++) {
						float co[4][3];
						if (mask && !BLI_BITMAP_TEST_BOOL(mask, i)) {
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
			}
			BLI_bvhtree_balance(tree);
		}
	}

	return tree;
}

static void bvhtree_from_mesh_faces_setup_data(
        BVHTreeFromMesh *data, BVHTree *tree, const bool is_cached,
        float epsilon, BMEditMesh *em,
        MVert *vert, const bool vert_allocated,
        MFace *face, const bool face_allocated)
{
	memset(data, 0, sizeof(*data));
	data->em_evil = em;

	if (tree) {
		data->tree = tree;
		data->cached = is_cached;

		if (em) {
			data->nearest_callback = editmesh_faces_nearest_point;
			data->raycast_callback = editmesh_faces_spherecast;
			data->nearest_to_ray_callback = NULL;
		}
		else {
			data->nearest_callback = mesh_faces_nearest_point;
			data->raycast_callback = mesh_faces_spherecast;
			data->nearest_to_ray_callback = NULL;

			data->vert = vert;
			data->vert_allocated = vert_allocated;
			data->face = face;
			data->face_allocated = face_allocated;
		}

		data->sphere_radius = epsilon;
	}
	else {
		if (vert_allocated) {
			MEM_freeN(vert);
		}
		if (face_allocated) {
			MEM_freeN(face);
		}
	}
}

/* Builds a bvh tree where nodes are the tesselated faces of the given dm */
BVHTree *bvhtree_from_mesh_faces(BVHTreeFromMesh *data, DerivedMesh *dm, float epsilon, int tree_type, int axis)
{
	BMEditMesh *em = data->em_evil;
	const int bvhcache_type = em ?
	        (data->em_evil_all ? BVHTREE_FROM_FACES_EDITMESH_ALL : BVHTREE_FROM_FACES_EDITMESH_SNAP) :
	        BVHTREE_FROM_FACES;
	BVHTree *tree;
	MVert *vert = NULL;
	MFace *face = NULL;
	bool vert_allocated = false, face_allocated = false;

	BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_READ);
	tree = bvhcache_find(&dm->bvhCache, bvhcache_type);
	BLI_rw_mutex_unlock(&cache_rwlock);

	if (em == NULL) {
		vert = DM_get_vert_array(dm, &vert_allocated);
		face = DM_get_tessface_array(dm, &face_allocated);
	}

	/* Not in cache */
	if (tree == NULL) {
		BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_WRITE);
		tree = bvhcache_find(&dm->bvhCache, bvhcache_type);
		if (tree == NULL) {
			int numFaces;

			/* BMESH specific check that we have tessfaces,
			 * we _could_ tessellate here but rather not - campbell
			 *
			 * this assert checks we have tessfaces,
			 * if not caller should use DM_ensure_tessface() */
			if (em) {
				numFaces = em->tottri;
			}
			else {
				numFaces = dm->getNumTessFaces(dm);
				BLI_assert(!(numFaces == 0 && dm->getNumPolys(dm) != 0));
			}

			tree = bvhtree_from_mesh_faces_create_tree(
			        epsilon, tree_type, axis,
			        em, (bvhcache_type == BVHTREE_FROM_FACES_EDITMESH_ALL),
			        vert, face, numFaces, NULL, -1);
			if (tree) {
				/* Save on cache for later use */
				/* printf("BVHTree built and saved on cache\n"); */
				bvhcache_insert(&dm->bvhCache, tree, bvhcache_type);
			}
		}
		BLI_rw_mutex_unlock(&cache_rwlock);
	}
	else {
		/* printf("BVHTree is already build, using cached tree\n"); */
	}

	/* Setup BVHTreeFromMesh */
	bvhtree_from_mesh_faces_setup_data(data, tree, true, epsilon, em, vert, vert_allocated, face, face_allocated);

	return data->tree;
}

/**
 * Builds a bvh tree where nodes are the given tessellated faces (note: does not copy given mfaces!).
 * \param vert_allocated if true, vert freeing will be done when freeing data.
 * \param face_allocated if true, face freeing will be done when freeing data.
 * \param mask if not null, true elements give which faces to add to BVH tree.
 * \param numFaces_active if >= 0, number of active faces to add to BVH tree (else will be computed from mask).
 */
BVHTree *bvhtree_from_mesh_faces_ex(
        BVHTreeFromMesh *data, MVert *vert, const bool vert_allocated,
        MFace *face, const int numFaces, const bool face_allocated,
        BLI_bitmap *mask, int numFaces_active, float epsilon, int tree_type, int axis)
{
	BVHTree *tree = bvhtree_from_mesh_faces_create_tree(
	        epsilon, tree_type, axis,
	        NULL, false,
	        vert, face, numFaces,
	        mask, numFaces_active);

	/* Setup BVHTreeFromMesh */
	bvhtree_from_mesh_faces_setup_data(data, tree, false, epsilon, NULL, vert, vert_allocated, face, face_allocated);

	return data->tree;
}

/** \} */


/* -------------------------------------------------------------------- */

/** \name LoopTri Face Builder
 * \{ */

static BVHTree *bvhtree_from_mesh_looptri_create_tree(
        float epsilon, int tree_type, int axis,
        BMEditMesh *em, const bool em_all,
        const MVert *vert, const MLoop *mloop, const MLoopTri *looptri, const int looptri_num,
        BLI_bitmap *mask, int looptri_num_active)
{
	BVHTree *tree = NULL;
	int i;

	if (looptri_num) {
		if (mask && looptri_num_active < 0) {
			looptri_num_active = 0;
			for (i = 0; i < looptri_num; i++) {
				if (BLI_BITMAP_TEST_BOOL(mask, i)) {
					looptri_num_active++;
				}
			}
		}
		else if (!mask) {
			looptri_num_active = looptri_num;
		}

		/* Create a bvh-tree of the given target */
		/* printf("%s: building BVH, total=%d\n", __func__, numFaces); */
		tree = BLI_bvhtree_new(looptri_num_active, epsilon, tree_type, axis);
		if (tree) {
			if (em) {
				const struct BMLoop *(*looptris)[3] = (void *)em->looptris;

				/* avoid double-up on face searches for quads-ngons */
				bool insert_prev = false;
				BMFace *f_prev = NULL;

				/* data->em_evil is only set for snapping, and only for the mesh of the object
				 * which is currently open in edit mode. When set, the bvhtree should not contain
				 * faces that will interfere with snapping (e.g. faces that are hidden/selected
				 * or faces that have selected verts). */

				/* Insert BMesh-tessellation triangles into the bvh tree, unless they are hidden
				 * and/or selected. Even if the faces themselves are not selected for the snapped
				 * transform, having a vertex selected means the face (and thus it's tessellated
				 * triangles) will be moving and will not be a good snap targets. */
				for (i = 0; i < looptri_num; i++) {
					const BMLoop **ltri = looptris[i];
					BMFace *f = ltri[0]->f;
					bool insert = mask ? BLI_BITMAP_TEST_BOOL(mask, i) : true;

					/* Start with the assumption the triangle should be included for snapping. */
					if (f == f_prev) {
						insert = insert_prev;
					}
					else if (insert) {
						if (em_all) {
							/* pass */
						}
						else if (BM_elem_flag_test(f, BM_ELEM_SELECT) || BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
							/* Don't insert triangles tessellated from faces that are hidden or selected */
							insert = false;
						}
						else {
							BMLoop *l_iter, *l_first;
							l_iter = l_first = BM_FACE_FIRST_LOOP(f);
							do {
								if (BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)) {
									/* Don't insert triangles tessellated from faces that have any selected verts */
									insert = false;
									break;
								}
							} while ((l_iter = l_iter->next) != l_first);
						}

						/* skip if face doesn't change */
						f_prev = f;
						insert_prev = insert;
					}

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
			else {
				if (vert && looptri) {
					for (i = 0; i < looptri_num; i++) {
						float co[3][3];
						if (mask && !BLI_BITMAP_TEST_BOOL(mask, i)) {
							continue;
						}

						copy_v3_v3(co[0], vert[mloop[looptri[i].tri[0]].v].co);
						copy_v3_v3(co[1], vert[mloop[looptri[i].tri[1]].v].co);
						copy_v3_v3(co[2], vert[mloop[looptri[i].tri[2]].v].co);

						BLI_bvhtree_insert(tree, i, co[0], 3);
					}
				}
			}
			BLI_bvhtree_balance(tree);
		}
	}

	return tree;
}

static void bvhtree_from_mesh_looptri_setup_data(
        BVHTreeFromMesh *data, BVHTree *tree, const bool is_cached,
        float epsilon, BMEditMesh *em,
        const MVert *vert, const bool vert_allocated,
        const MLoop *mloop, const bool loop_allocated,
        const MLoopTri *looptri, const bool looptri_allocated)
{
	memset(data, 0, sizeof(*data));
	data->em_evil = em;

	if (tree) {
		data->tree = tree;
		data->cached = is_cached;

		if (em) {
			data->nearest_callback = editmesh_faces_nearest_point;
			data->raycast_callback = editmesh_faces_spherecast;
			data->nearest_to_ray_callback = NULL;
		}
		else {
			data->nearest_callback = mesh_looptri_nearest_point;
			data->raycast_callback = mesh_looptri_spherecast;
			data->nearest_to_ray_callback = NULL;

			data->vert = vert;
			data->vert_allocated = vert_allocated;
			data->loop = mloop;
			data->loop_allocated = loop_allocated;
			data->looptri = looptri;
			data->looptri_allocated = looptri_allocated;
		}

		data->sphere_radius = epsilon;
	}
	else {
		if (vert_allocated) {
			MEM_freeN((void *)vert);
		}
		if (loop_allocated) {
			MEM_freeN((void *)mloop);
		}
		if (looptri_allocated) {
			MEM_freeN((void *)looptri);
		}
	}
}

/**
 * Builds a bvh tree where nodes are the looptri faces of the given dm
 *
 * \note for editmesh this is currently a duplicate of bvhtree_from_mesh_faces
 */
BVHTree *bvhtree_from_mesh_looptri(BVHTreeFromMesh *data, DerivedMesh *dm, float epsilon, int tree_type, int axis)
{
	BMEditMesh *em = data->em_evil;
	const int bvhcache_type = em ?
	        (data->em_evil_all ? BVHTREE_FROM_FACES_EDITMESH_ALL : BVHTREE_FROM_FACES_EDITMESH_SNAP) :
	        BVHTREE_FROM_LOOPTRI;
	BVHTree *tree;
	MVert *mvert = NULL;
	MLoop *mloop = NULL;
	const MLoopTri *looptri = NULL;
	bool vert_allocated = false;
	bool loop_allocated = false;
	bool looptri_allocated = false;

	BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_READ);
	tree = bvhcache_find(&dm->bvhCache, bvhcache_type);
	BLI_rw_mutex_unlock(&cache_rwlock);

	if (em == NULL) {
		MPoly *mpoly;
		bool poly_allocated = false;

		mvert = DM_get_vert_array(dm, &vert_allocated);
		mpoly = DM_get_poly_array(dm, &poly_allocated);

		mloop = DM_get_loop_array(dm, &loop_allocated);
		looptri = DM_get_looptri_array(
		        dm,
		        mvert,
		        mpoly, dm->getNumPolys(dm),
		        mloop, dm->getNumLoops(dm),
		        &looptri_allocated);

		if (poly_allocated) {
			MEM_freeN(mpoly);
		}

	}

	/* Not in cache */
	if (tree == NULL) {
		BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_WRITE);
		tree = bvhcache_find(&dm->bvhCache, bvhcache_type);
		if (tree == NULL) {
			int looptri_num;

			/* BMESH specific check that we have tessfaces,
			 * we _could_ tessellate here but rather not - campbell
			 *
			 * this assert checks we have tessfaces,
			 * if not caller should use DM_ensure_tessface() */
			if (em) {
				looptri_num = em->tottri;
			}
			else {
				looptri_num = dm->getNumLoopTri(dm);
				BLI_assert(!(looptri_num == 0 && dm->getNumPolys(dm) != 0));
			}

			tree = bvhtree_from_mesh_looptri_create_tree(
			        epsilon, tree_type, axis,
			        em, (bvhcache_type == BVHTREE_FROM_FACES_EDITMESH_ALL),
			        mvert, mloop, looptri, looptri_num, NULL, -1);
			if (tree) {
				/* Save on cache for later use */
				/* printf("BVHTree built and saved on cache\n"); */
				bvhcache_insert(&dm->bvhCache, tree, bvhcache_type);
			}
		}
		BLI_rw_mutex_unlock(&cache_rwlock);
	}
	else {
		/* printf("BVHTree is already build, using cached tree\n"); */
	}

	/* Setup BVHTreeFromMesh */
	bvhtree_from_mesh_looptri_setup_data(
	        data, tree, true, epsilon, em,
	        mvert, vert_allocated,
	        mloop, loop_allocated,
	        looptri, looptri_allocated);

	return data->tree;
}

BVHTree *bvhtree_from_mesh_looptri_ex(
        BVHTreeFromMesh *data,
        const struct MVert *vert, const bool vert_allocated,
        const struct MLoop *mloop, const bool loop_allocated,
        const struct MLoopTri *looptri, const int looptri_num, const bool looptri_allocated,
        BLI_bitmap *mask, int looptri_num_active,
        float epsilon, int tree_type, int axis)
{
	BVHTree *tree = bvhtree_from_mesh_looptri_create_tree(
	        epsilon, tree_type, axis,
	        NULL, false,
	        vert, mloop, looptri, looptri_num,
	        mask, looptri_num_active);

	/* Setup BVHTreeFromMesh */
	bvhtree_from_mesh_looptri_setup_data(
	        data, tree, false, epsilon, NULL,
	        vert, vert_allocated,
	        mloop, loop_allocated,
	        looptri, looptri_allocated);

	return data->tree;
}

/** \} */


/* Frees data allocated by a call to bvhtree_from_mesh_*. */
void free_bvhtree_from_mesh(struct BVHTreeFromMesh *data)
{
	if (data->tree) {
		if (!data->cached) {
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
}


/* -------------------------------------------------------------------- */

/** \name BVHCache
 * \{ */

typedef struct BVHCacheItem {
	int type;
	BVHTree *tree;

} BVHCacheItem;

static void bvhcacheitem_set_if_match(void *_cached, void *_search)
{
	BVHCacheItem *cached = (BVHCacheItem *)_cached;
	BVHCacheItem *search = (BVHCacheItem *)_search;

	if (search->type == cached->type) {
		search->tree = cached->tree;
	}
} 

BVHTree *bvhcache_find(BVHCache *cache, int type)
{
	BVHCacheItem item;
	item.type = type;
	item.tree = NULL;

	BLI_linklist_apply(*cache, bvhcacheitem_set_if_match, &item);
	return item.tree;
}

void bvhcache_insert(BVHCache *cache, BVHTree *tree, int type)
{
	BVHCacheItem *item = NULL;

	assert(tree != NULL);
	assert(bvhcache_find(cache, type) == NULL);

	item = MEM_mallocN(sizeof(BVHCacheItem), "BVHCacheItem");
	assert(item != NULL);

	item->type = type;
	item->tree = tree;

	BLI_linklist_prepend(cache, item);
}


void bvhcache_init(BVHCache *cache)
{
	*cache = NULL;
}

static void bvhcacheitem_free(void *_item)
{
	BVHCacheItem *item = (BVHCacheItem *)_item;

	BLI_bvhtree_free(item->tree);
	MEM_freeN(item);
}


void bvhcache_free(BVHCache *cache)
{
	BLI_linklist_free(*cache, (LinkNodeFreeFP)bvhcacheitem_free);
	*cache = NULL;
}

/** \} */
