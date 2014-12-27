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

/* Math stuff for ray casting on mesh faces and for nearest surface */

float bvhtree_ray_tri_intersection(const BVHTreeRay *ray, const float UNUSED(m_dist), const float v0[3], const float v1[3], const float v2[3])
{
	float dist;

	if (isect_ray_tri_epsilon_v3(ray->origin, ray->direction, v0, v1, v2, &dist, NULL, FLT_EPSILON))
		return dist;

	return FLT_MAX;
}

static float sphereray_tri_intersection(const BVHTreeRay *ray, float radius, const float m_dist, const float v0[3], const float v1[3], const float v2[3])
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

/* Callback to bvh tree nearest point. The tree must bust have been built using bvhtree_from_mesh_faces.
 * userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree. */
static void mesh_faces_nearest_point(void *userdata, int index, const float co[3], BVHTreeNearest *nearest)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *) userdata;
	MVert *vert = data->vert;
	MFace *face = data->face + index;

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

			if (t1 == vert[face->v3].co)
				nearest->flags |= BVH_ONQUAD;
		}

		t1 = t2;
		t2 = t3;
		t3 = NULL;

	} while (t2);
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

/* Callback to bvh tree raycast. The tree must bust have been built using bvhtree_from_mesh_faces.
 * userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree. */
static void mesh_faces_spherecast(void *userdata, int index, const BVHTreeRay *ray, BVHTreeRayHit *hit)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *) userdata;
	MVert *vert = data->vert;
	MFace *face = data->face + index;

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
			dist = sphereray_tri_intersection(ray, data->sphere_radius, hit->dist, t0, t1, t2);

		if (dist >= 0 && dist < hit->dist) {
			hit->index = index;
			hit->dist = dist;
			madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

			normal_tri_v3(hit->no, t0, t1, t2);

			if (t1 == vert[face->v3].co)
				hit->flags |= BVH_ONQUAD;
		}

		t1 = t2;
		t2 = t3;
		t3 = NULL;

	} while (t2);
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
			dist = sphereray_tri_intersection(ray, data->sphere_radius, hit->dist, t0, t1, t2);

		if (dist >= 0 && dist < hit->dist) {
			hit->index = index;
			hit->dist = dist;
			madd_v3_v3v3fl(hit->co, ray->origin, ray->direction, dist);

			normal_tri_v3(hit->no, t0, t1, t2);
		}
	}
}

/* Callback to bvh tree nearest point. The tree must bust have been built using bvhtree_from_mesh_edges.
 * userdata must be a BVHMeshCallbackUserdata built from the same mesh as the tree. */
static void mesh_edges_nearest_point(void *userdata, int index, const float co[3], BVHTreeNearest *nearest)
{
	const BVHTreeFromMesh *data = (BVHTreeFromMesh *) userdata;
	MVert *vert = data->vert;
	MEdge *edge = data->edge + index;
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

/*
 * BVH builders
 */
/* Builds a bvh tree.. where nodes are the vertexs of the given mesh */
BVHTree *bvhtree_from_mesh_verts(BVHTreeFromMesh *data, DerivedMesh *dm, float epsilon, int tree_type, int axis)
{
	BVHTree *tree;
	MVert *vert;
	bool vert_allocated;

	BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_READ);
	tree = bvhcache_find(&dm->bvhCache, BVHTREE_FROM_VERTICES);
	BLI_rw_mutex_unlock(&cache_rwlock);

	vert = DM_get_vert_array(dm, &vert_allocated);

	/* Not in cache */
	if (tree == NULL) {
		BLI_rw_mutex_lock(&cache_rwlock, THREAD_LOCK_WRITE);
		tree = bvhcache_find(&dm->bvhCache, BVHTREE_FROM_VERTICES);
		if (tree == NULL) {
			int i;
			int numVerts = dm->getNumVerts(dm);

			if (vert != NULL) {
				tree = BLI_bvhtree_new(numVerts, epsilon, tree_type, axis);

				if (tree != NULL) {
					for (i = 0; i < numVerts; i++) {
						BLI_bvhtree_insert(tree, i, vert[i].co, 1);
					}

					BLI_bvhtree_balance(tree);

					/* Save on cache for later use */
//					printf("BVHTree built and saved on cache\n");
					bvhcache_insert(&dm->bvhCache, tree, BVHTREE_FROM_VERTICES);
				}
			}
		}
		BLI_rw_mutex_unlock(&cache_rwlock);
	}
	else {
//		printf("BVHTree is already build, using cached tree\n");
	}


	/* Setup BVHTreeFromMesh */
	memset(data, 0, sizeof(*data));
	data->tree = tree;

	if (data->tree) {
		data->cached = true;

		/* a NULL nearest callback works fine
		 * remember the min distance to point is the same as the min distance to BV of point */
		data->nearest_callback = NULL;
		data->raycast_callback = NULL;

		data->vert = vert;
		data->vert_allocated = vert_allocated;
		data->face = DM_get_tessface_array(dm, &data->face_allocated);

		data->sphere_radius = epsilon;
	}
	else {
		if (vert_allocated) {
			MEM_freeN(vert);
		}
	}

	return data->tree;
}

/* Builds a bvh tree.. where nodes are the faces of the given dm. */
BVHTree *bvhtree_from_mesh_faces(BVHTreeFromMesh *data, DerivedMesh *dm, float epsilon, int tree_type, int axis)
{
	BMEditMesh *em = data->em_evil;
	const int bvhcache_type = em ? BVHTREE_FROM_FACES_EDITMESH : BVHTREE_FROM_FACES;
	BVHTree *tree;
	MVert *vert;
	MFace *face;
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
			int i;
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

			if (numFaces != 0) {
				/* Create a bvh-tree of the given target */
				// printf("%s: building BVH, total=%d\n", __func__, numFaces);
				tree = BLI_bvhtree_new(numFaces, epsilon, tree_type, axis);
				if (tree != NULL) {
					if (em) {
						const struct BMLoop *(*looptris)[3] = (void *)em->looptris;

						/* avoid double-up on face searches for quads-ngons */
						bool insert_prev = false;
						BMFace *f_prev = NULL;

						/* data->em_evil is only set for snapping, and only for the mesh of the object
						 * which is currently open in edit mode. When set, the bvhtree should not contain
						 * faces that will interfere with snapping (e.g. faces that are hidden/selected
						 * or faces that have selected verts).*/

						/* Insert BMesh-tessellation triangles into the bvh tree, unless they are hidden
						 * and/or selected. Even if the faces themselves are not selected for the snapped
						 * transform, having a vertex selected means the face (and thus it's tessellated
						 * triangles) will be moving and will not be a good snap targets.*/
						for (i = 0; i < em->tottri; i++) {
							const BMLoop **ltri = looptris[i];
							BMFace *f = ltri[0]->f;
							bool insert;

							/* Start with the assumption the triangle should be included for snapping. */
							if (f == f_prev) {
								insert = insert_prev;
							}
							else {
								if (BM_elem_flag_test(f, BM_ELEM_SELECT) || BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
									/* Don't insert triangles tessellated from faces that are hidden
									 * or selected*/
									insert = false;
								}
								else {
									BMLoop *l_iter, *l_first;
									insert = true;
									l_iter = l_first = BM_FACE_FIRST_LOOP(f);
									do {
										if (BM_elem_flag_test(l_iter->v, BM_ELEM_SELECT)) {
											/* Don't insert triangles tessellated from faces that have
											 * any selected verts.*/
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
								/* No reason found to block hit-testing the triangle for snap,
								 * so insert it now.*/
								float co[3][3];
								copy_v3_v3(co[0], ltri[0]->v->co);
								copy_v3_v3(co[1], ltri[1]->v->co);
								copy_v3_v3(co[2], ltri[2]->v->co);

								BLI_bvhtree_insert(tree, i, co[0], 3);
							}
						}
					}
					else {
						if (vert != NULL && face != NULL) {
							for (i = 0; i < numFaces; i++) {
								float co[4][3];
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

					/* Save on cache for later use */
//					printf("BVHTree built and saved on cache\n");
					bvhcache_insert(&dm->bvhCache, tree, bvhcache_type);
				}
			}
		}
		BLI_rw_mutex_unlock(&cache_rwlock);
	}
	else {
//		printf("BVHTree is already build, using cached tree\n");
	}


	/* Setup BVHTreeFromMesh */
	memset(data, 0, sizeof(*data));
	data->tree = tree;
	data->em_evil = em;

	if (data->tree) {
		data->cached = true;

		if (em) {
			data->nearest_callback = editmesh_faces_nearest_point;
			data->raycast_callback = editmesh_faces_spherecast;
		}
		else {
			data->nearest_callback = mesh_faces_nearest_point;
			data->raycast_callback = mesh_faces_spherecast;

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

	return data->tree;

}

/* Builds a bvh tree.. where nodes are the faces of the given dm. */
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
						float co[4][3];
						copy_v3_v3(co[0], vert[edge[i].v1].co);
						copy_v3_v3(co[1], vert[edge[i].v2].co);

						BLI_bvhtree_insert(tree, i, co[0], 2);
					}
					BLI_bvhtree_balance(tree);

					/* Save on cache for later use */
//					printf("BVHTree built and saved on cache\n");
					bvhcache_insert(&dm->bvhCache, tree, BVHTREE_FROM_EDGES);
				}
			}
		}
		BLI_rw_mutex_unlock(&cache_rwlock);
	}
	else {
//		printf("BVHTree is already build, using cached tree\n");
	}


	/* Setup BVHTreeFromMesh */
	memset(data, 0, sizeof(*data));
	data->tree = tree;

	if (data->tree) {
		data->cached = true;

		data->nearest_callback = mesh_edges_nearest_point;
		data->raycast_callback = NULL;

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

/* Frees data allocated by a call to bvhtree_from_mesh_*. */
void free_bvhtree_from_mesh(struct BVHTreeFromMesh *data)
{
	if (data->tree) {
		if (!data->cached)
			BLI_bvhtree_free(data->tree);

		if (data->vert_allocated) {
			MEM_freeN(data->vert);
		}
		if (data->edge_allocated) {
			MEM_freeN(data->edge);
		}
		if (data->face_allocated) {
			MEM_freeN(data->face);
		}

		memset(data, 0, sizeof(*data));
	}
}


/* BVHCache */
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


