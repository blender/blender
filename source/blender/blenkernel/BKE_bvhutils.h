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

/*
 * This header encapsulates necessary code to buld a BVH
 */

struct DerivedMesh;
struct MVert;
struct MFace;

/*
 * struct that kepts basic information about a BVHTree build from a mesh
 */
typedef struct BVHTreeFromMesh {
	struct BVHTree *tree;

	/* default callbacks to bvh nearest and raycast */
	BVHTree_NearestPointCallback nearest_callback;
	BVHTree_RayCastCallback raycast_callback;

	/* Vertex array, so that callbacks have instante access to data */
	struct MVert *vert;
	struct MEdge *edge;     /* only used for BVHTreeFromMeshEdges */
	struct MFace *face;
	bool vert_allocated;
	bool edge_allocated;
	bool face_allocated;

	/* radius for raycast */
	float sphere_radius;

	/* Private data */
	void *em_evil;  /* var only for snapping */
	bool cached;

} BVHTreeFromMesh;

/*
 * Builds a bvh tree where nodes are the relevant elements of the given mesh.
 * Configures BVHTreeFromMesh.
 *
 * The tree is build in mesh space coordinates, this means special care must be made on queries
 * so that the coordinates and rays are first translated on the mesh local coordinates.
 * Reason for this is that bvh_from_mesh_* can use a cache in some cases and so it becomes possible to reuse a BVHTree.
 * 
 * free_bvhtree_from_mesh should be called when the tree is no longer needed.
 */
BVHTree *bvhtree_from_mesh_verts(struct BVHTreeFromMesh *data, struct DerivedMesh *mesh, float epsilon, int tree_type, int axis);
BVHTree *bvhtree_from_mesh_verts_ex(struct BVHTreeFromMesh *data, struct MVert *vert, const int numVerts,
                                    const bool vert_allocated, BLI_bitmap *mask, int numVerts_active,
                                    float epsilon, int tree_type, int axis);

BVHTree *bvhtree_from_mesh_edges(struct BVHTreeFromMesh *data, struct DerivedMesh *mesh, float epsilon, int tree_type, int axis);

BVHTree *bvhtree_from_mesh_faces(struct BVHTreeFromMesh *data, struct DerivedMesh *mesh, float epsilon, int tree_type, int axis);
BVHTree *bvhtree_from_mesh_faces_ex(struct BVHTreeFromMesh *data, struct MVert *vert, const bool vert_allocated,
                                    struct MFace *face, const int numFaces, const bool face_allocated,
                                    BLI_bitmap *mask, int numFaces_active,
                                    float epsilon, int tree_type, int axis);

/*
 * Frees data allocated by a call to bvhtree_from_mesh_*.
 */
void free_bvhtree_from_mesh(struct BVHTreeFromMesh *data);

/*
 * Math functions used by callbacks
 */
float bvhtree_ray_tri_intersection(const BVHTreeRay *ray, const float m_dist, const float v0[3], const float v1[3], const float v2[3]);
float nearest_point_in_tri_surface_squared(const float v0[3], const float v1[3], const float v2[3], const float p[3], int *v, int *e, float nearest[3]);

/*
 * BVHCache
 */

/* Using local coordinates */
enum {
	BVHTREE_FROM_VERTS           = 0,
	BVHTREE_FROM_EDGES           = 1,
	BVHTREE_FROM_FACES           = 2,
	BVHTREE_FROM_FACES_EDITMESH  = 3,
};

typedef struct LinkNode *BVHCache;


/*
 * Queries a bvhcache for the cache bvhtree of the request type
 */
BVHTree *bvhcache_find(BVHCache *cache, int type);

/*
 * Inserts a BVHTree of the given type under the cache
 * After that the caller no longer needs to worry when to free the BVHTree
 * as that will be done when the cache is freed.
 *
 * A call to this assumes that there was no previous cached tree of the given type
 */
void bvhcache_insert(BVHCache *cache, BVHTree *tree, int type);

/*
 * inits and frees a bvhcache
 */
void bvhcache_init(BVHCache *cache);
void bvhcache_free(BVHCache *cache);

#endif

