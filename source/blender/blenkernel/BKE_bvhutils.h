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
 * The Original Code is Copyright (C) 2006 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): André Pinto
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef __BKE_BVHUTILS_H__
#define __BKE_BVHUTILS_H__

/** \file BKE_bvhutils.h
 *  \ingroup bke
 */

#include "BLI_bitmap.h"
#include "BLI_kdopbvh.h"

/**
 * This header encapsulates necessary code to build a BVH
 */

struct DerivedMesh;
struct BMEditMesh;
struct MVert;
struct MFace;

typedef struct LinkNode BVHCache;

/**
 * Struct that stores basic information about a BVHTree built from a edit-mesh.
 */
typedef struct BVHTreeFromEditMesh {
	struct BVHTree *tree;

	/* default callbacks to bvh nearest and raycast */
	BVHTree_NearestPointCallback nearest_callback;
	BVHTree_RayCastCallback raycast_callback;

	struct BMEditMesh *em;

	/* Private data */
	bool cached;

} BVHTreeFromEditMesh;

/**
 * Struct that stores basic information about a BVHTree built from a mesh.
 */
typedef struct BVHTreeFromMesh {
	struct BVHTree *tree;

	/* default callbacks to bvh nearest and raycast */
	BVHTree_NearestPointCallback nearest_callback;
	BVHTree_RayCastCallback raycast_callback;

	/* Vertex array, so that callbacks have instante access to data */
	const struct MVert *vert;
	const struct MEdge *edge;     /* only used for BVHTreeFromMeshEdges */
	const struct MFace *face;
	const struct MLoop *loop;
	const struct MLoopTri *looptri;
	bool vert_allocated;
	bool edge_allocated;
	bool face_allocated;
	bool loop_allocated;
	bool looptri_allocated;

	/* Private data */
	bool cached;

} BVHTreeFromMesh;

/**
 * Builds a bvh tree where nodes are the relevant elements of the given mesh.
 * Configures BVHTreeFromMesh.
 *
 * The tree is build in mesh space coordinates, this means special care must be made on queries
 * so that the coordinates and rays are first translated on the mesh local coordinates.
 * Reason for this is that bvh_from_mesh_* can use a cache in some cases and so it becomes possible to reuse a BVHTree.
 *
 * free_bvhtree_from_mesh should be called when the tree is no longer needed.
 */
BVHTree *bvhtree_from_editmesh_verts(
        BVHTreeFromEditMesh *data, struct BMEditMesh *em,
        float epsilon, int tree_type, int axis);
BVHTree *bvhtree_from_editmesh_verts_ex(
        BVHTreeFromEditMesh *data, struct BMEditMesh *em,
        const BLI_bitmap *mask, int verts_num_active,
        float epsilon, int tree_type, int axis);

BVHTree *bvhtree_from_mesh_verts_ex(
        struct BVHTreeFromMesh *data, const struct MVert *vert, const int numVerts,
        const bool vert_allocated, const BLI_bitmap *mask, int verts_num_active,
        float epsilon, int tree_type, int axis);

BVHTree *bvhtree_from_editmesh_edges(
        BVHTreeFromEditMesh *data, struct BMEditMesh *em,
        float epsilon, int tree_type, int axis);
BVHTree *bvhtree_from_editmesh_edges_ex(
        BVHTreeFromEditMesh *data, struct BMEditMesh *em,
        const BLI_bitmap *edges_mask, int edges_num_active,
        float epsilon, int tree_type, int axis);

BVHTree *bvhtree_from_mesh_edges_ex(
        struct BVHTreeFromMesh *data,
        const struct MVert *vert, const bool vert_allocated,
        const struct MEdge *edge, const int edges_num, const bool edge_allocated,
        const BLI_bitmap *edges_mask, int edges_num_active,
        float epsilon, int tree_type, int axis);

BVHTree *bvhtree_from_mesh_faces_ex(
        struct BVHTreeFromMesh *data,
        const struct MVert *vert, const bool vert_allocated,
        const struct MFace *face, const int numFaces, const bool face_allocated,
        const BLI_bitmap *mask, int numFaces_active,
        float epsilon, int tree_type, int axis);

BVHTree *bvhtree_from_editmesh_looptri(
        BVHTreeFromEditMesh *data, struct BMEditMesh *em,
        float epsilon, int tree_type, int axis, BVHCache **bvhCache);
BVHTree *bvhtree_from_editmesh_looptri_ex(
        BVHTreeFromEditMesh *data, struct BMEditMesh *em,
        const BLI_bitmap *mask, int looptri_num_active,
        float epsilon, int tree_type, int axis, BVHCache **bvhCache);

BVHTree *bvhtree_from_mesh_looptri_ex(
        struct BVHTreeFromMesh *data,
        const struct MVert *vert, const bool vert_allocated,
        const struct MLoop *mloop, const bool loop_allocated,
        const struct MLoopTri *looptri, const int looptri_num, const bool looptri_allocated,
        const BLI_bitmap *mask, int looptri_num_active,
        float epsilon, int tree_type, int axis);

BVHTree *bvhtree_from_mesh_get(
        struct BVHTreeFromMesh *data, struct DerivedMesh *mesh,
        const int type, const int tree_type);

/**
 * Frees data allocated by a call to bvhtree_from_mesh_*.
 */
void free_bvhtree_from_editmesh(struct BVHTreeFromEditMesh *data);
void free_bvhtree_from_mesh(struct BVHTreeFromMesh *data);

/**
 * Math functions used by callbacks
 */
float bvhtree_ray_tri_intersection(
        const BVHTreeRay *ray, const float m_dist,
        const float v0[3], const float v1[3], const float v2[3]);
float bvhtree_sphereray_tri_intersection(
        const BVHTreeRay *ray, float radius, const float m_dist,
        const float v0[3], const float v1[3], const float v2[3]);


/**
 * BVHCache
 */

/* Using local coordinates */
enum {
	BVHTREE_FROM_VERTS           = 0,
	BVHTREE_FROM_EDGES           = 1,
	BVHTREE_FROM_FACES           = 2,
	BVHTREE_FROM_LOOPTRI         = 3,

	BVHTREE_FROM_EM_LOOPTRI      = 4,
};


BVHTree *bvhcache_find(BVHCache *cache, int type);
bool     bvhcache_has_tree(const BVHCache *cache, const BVHTree *tree);
void     bvhcache_insert(BVHCache **cache_p, BVHTree *tree, int type);
void     bvhcache_init(BVHCache **cache_p);
void     bvhcache_free(BVHCache **cache_p);


#endif
