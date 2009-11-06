/**
 * A BVH for high poly meshes.
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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef BLI_PBVH_H
#define BLI_PBVH_H

struct MFace;
struct MVert;
struct PBVH;
struct PBVHNode;
struct ListBase;

typedef struct PBVH PBVH;
typedef struct PBVHNode PBVHNode;

/* Callbacks */

/* returns 1 if the search should continue from this node, 0 otherwise */
typedef int (*BLI_pbvh_SearchCallback)(PBVHNode *node, void *data);

typedef void (*BLI_pbvh_HitCallback)(PBVHNode *node, void *data);

/* Building */

PBVH *BLI_pbvh_new(void);
void BLI_pbvh_build(PBVH *bvh, struct MFace *faces, struct MVert *verts,
		    int totface, int totvert);
void BLI_pbvh_free(PBVH *bvh);

void BLI_pbvh_set_source(PBVH *bvh, struct MVert *, struct MFace *mface);

/* Hierarchical Search in the BVH, two methods:
   * for each hit calling a callback
   * gather nodes in an array (easy to multithread) */

void BLI_pbvh_search_callback(PBVH *bvh,
	BLI_pbvh_SearchCallback scb, void *search_data,
	BLI_pbvh_HitCallback hcb, void *hit_data);

void BLI_pbvh_search_gather(PBVH *bvh,
	BLI_pbvh_SearchCallback scb, void *search_data,
	PBVHNode ***array, int *tot);

/* Raycast
   the hit callback is called for all leaf nodes intersecting the ray;
   it's up to the callback to find the primitive within the leaves that is
   hit first */

void BLI_pbvh_raycast(PBVH *bvh, BLI_pbvh_HitCallback cb, void *data,
		      float ray_start[3], float ray_normal[3], int original);

/* Node Access */

typedef enum {
	PBVH_Leaf = 1,

	PBVH_UpdateNormals = 2,
	PBVH_UpdateBB = 4,
	PBVH_UpdateOriginalBB = 4,
	PBVH_UpdateDrawBuffers = 8,
	PBVH_UpdateRedraw = 16
} PBVHNodeFlags;

void BLI_pbvh_node_mark_update(PBVHNode *node);

void BLI_pbvh_node_get_verts(PBVHNode *node, int **vert_indices,
	int *totvert, int *allverts);
void BLI_pbvh_node_get_faces(PBVHNode *node, int **face_indices,
	int **face_vert_indices, int *totface);
void *BLI_pbvh_node_get_draw_buffers(PBVHNode *node);
void BLI_pbvh_node_get_BB(PBVHNode *node, float bb_min[3], float bb_max[3]);
void BLI_pbvh_node_get_original_BB(PBVHNode *node, float bb_min[3], float bb_max[3]);

/* Update Normals/Bounding Box/Draw Buffers/Redraw and clear flags */

void BLI_pbvh_update(PBVH *bvh, int flags, float (*face_nors)[3]);
void BLI_pbvh_redraw_BB(PBVH *bvh, float bb_min[3], float bb_max[3]);

#endif /* BLI_PBVH_H */

