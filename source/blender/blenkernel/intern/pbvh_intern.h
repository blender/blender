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
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __PBVH_INTERN_H__
#define __PBVH_INTERN_H__

/* Axis-aligned bounding box */
typedef struct {
	float bmin[3], bmax[3];
} BB;

/* Axis-aligned bounding box with centroid */
typedef struct {
	float bmin[3], bmax[3], bcentroid[3];
} BBC;

struct PBVHNode {
	/* Opaque handle for drawing code */
	GPU_Buffers *draw_buffers;

	/* Voxel bounds */
	BB vb;
	BB orig_vb;

	/* For internal nodes, the offset of the children in the PBVH
	 * 'nodes' array. */
	int children_offset;

	/* Pointer into the PBVH prim_indices array and the number of
	 * primitives used by this leaf node.
	 *
	 * Used for leaf nodes in both mesh- and multires-based PBVHs.
	 */
	int *prim_indices;
	unsigned int totprim;

	/* Array of indices into the mesh's MVert array. Contains the
	 * indices of all vertices used by faces that are within this
	 * node's bounding box.
	 *
	 * Note that a vertex might be used by a multiple faces, and
	 * these faces might be in different leaf nodes. Such a vertex
	 * will appear in the vert_indices array of each of those leaf
	 * nodes.
	 *
	 * In order to support cases where you want access to multiple
	 * nodes' vertices without duplication, the vert_indices array
	 * is ordered such that the first part of the array, up to
	 * index 'uniq_verts', contains "unique" vertex indices. These
	 * vertices might not be truly unique to this node, but if
	 * they appear in another node's vert_indices array, they will
	 * be above that node's 'uniq_verts' value.
	 *
	 * Used for leaf nodes in a mesh-based PBVH (not multires.)
	 */
	int *vert_indices;
	unsigned int uniq_verts, face_verts;

	/* An array mapping face corners into the vert_indices
	 * array. The array is sized to match 'totprim', and each of
	 * the face's corners gets an index into the vert_indices
	 * array, in the same order as the corners in the original
	 * MFace. The fourth value should not be used if the original
	 * face is a triangle.
	 *
	 * Used for leaf nodes in a mesh-based PBVH (not multires.)
	 */
	int (*face_vert_indices)[4];

	/* Indicates whether this node is a leaf or not; also used for
	 * marking various updates that need to be applied. */
	PBVHNodeFlags flag : 8;

	/* Used for raycasting: how close bb is to the ray point. */
	float tmin;

	int proxy_count;
	PBVHProxyNode *proxies;
};

struct PBVH {
	PBVHType type;

	PBVHNode *nodes;
	int node_mem_count, totnode;

	int *prim_indices;
	int totprim;
	int totvert;

	int leaf_limit;

	/* Mesh data */
	MVert *verts;
	MFace *faces;
	CustomData *vdata;

	/* Grid Data */
	CCGKey gridkey;
	CCGElem **grids;
	DMGridAdjacency *gridadj;
	void **gridfaces;
	const DMFlagMat *grid_flag_mats;
	int totgrid;
	BLI_bitmap *grid_hidden;

	/* Only used during BVH build and update,
	 * don't need to remain valid after */
	BLI_bitmap vert_bitmap;

#ifdef PERFCNTRS
	int perf_modified;
#endif

	/* flag are verts/faces deformed */
	int deformed;

	int show_diffuse_color;
};

#endif
