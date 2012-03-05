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

/** \file blender/blenlib/intern/pbvh.c
 *  \ingroup bli
 */



#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"
#include "BLI_pbvh.h"

#include "BKE_DerivedMesh.h"
#include "BKE_mesh.h" /* for mesh_calc_normals */
#include "BKE_global.h" /* for mesh_calc_normals */

#include "GPU_buffers.h"

#define LEAF_LIMIT 10000

//#define PERFCNTRS

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
	PBVHProxyNode* proxies;
};

struct PBVH {
	PBVHNode *nodes;
	int node_mem_count, totnode;

	int *prim_indices;
	int totprim;
	int totvert;

	int leaf_limit;

	/* Mesh data */
	MVert *verts;
	MFace *faces;

	/* Grid Data */
	DMGridData **grids;
	DMGridAdjacency *gridadj;
	void **gridfaces;
	int totgrid;
	int gridsize;

	/* Only used during BVH build and update,
	 * don't need to remain valid after */
	BLI_bitmap vert_bitmap;

#ifdef PERFCNTRS
	int perf_modified;
#endif

	/* flag are verts/faces deformed */
	int deformed;
};

#define STACK_FIXED_DEPTH	100

typedef struct PBVHStack {
	PBVHNode *node;
	int revisiting;
} PBVHStack;

typedef struct PBVHIter {
	PBVH *bvh;
	BLI_pbvh_SearchCallback scb;
	void *search_data;

	PBVHStack *stack;
	int stacksize;

	PBVHStack stackfixed[STACK_FIXED_DEPTH];
	int stackspace;
} PBVHIter;

static void BB_reset(BB *bb)
{
	bb->bmin[0] = bb->bmin[1] = bb->bmin[2] = FLT_MAX;
	bb->bmax[0] = bb->bmax[1] = bb->bmax[2] = -FLT_MAX;
}

/* Expand the bounding box to include a new coordinate */
static void BB_expand(BB *bb, float co[3])
{
	int i;
	for(i = 0; i < 3; ++i) {
		bb->bmin[i] = MIN2(bb->bmin[i], co[i]);
		bb->bmax[i] = MAX2(bb->bmax[i], co[i]);
	}
}

/* Expand the bounding box to include another bounding box */
static void BB_expand_with_bb(BB *bb, BB *bb2)
{
	int i;
	for(i = 0; i < 3; ++i) {
		bb->bmin[i] = MIN2(bb->bmin[i], bb2->bmin[i]);
		bb->bmax[i] = MAX2(bb->bmax[i], bb2->bmax[i]);
	}
}

/* Return 0, 1, or 2 to indicate the widest axis of the bounding box */
static int BB_widest_axis(BB *bb)
{
	float dim[3];
	int i;

	for(i = 0; i < 3; ++i)
		dim[i] = bb->bmax[i] - bb->bmin[i];

	if(dim[0] > dim[1]) {
		if(dim[0] > dim[2])
			return 0;
		else
			return 2;
	}
	else {
		if(dim[1] > dim[2])
			return 1;
		else
			return 2;
	}
}

static void BBC_update_centroid(BBC *bbc)
{
	int i;
	for(i = 0; i < 3; ++i)
		bbc->bcentroid[i] = (bbc->bmin[i] + bbc->bmax[i]) * 0.5f;
}

/* Not recursive */
static void update_node_vb(PBVH *bvh, PBVHNode *node)
{
	BB vb;

	BB_reset(&vb);
	
	if(node->flag & PBVH_Leaf) {
		PBVHVertexIter vd;

		BLI_pbvh_vertex_iter_begin(bvh, node, vd, PBVH_ITER_ALL) {
			BB_expand(&vb, vd.co);
		}
		BLI_pbvh_vertex_iter_end;
	}
	else {
		BB_expand_with_bb(&vb,
				  &bvh->nodes[node->children_offset].vb);
		BB_expand_with_bb(&vb,
				  &bvh->nodes[node->children_offset + 1].vb);
	}

	node->vb= vb;
}

//void BLI_pbvh_node_BB_reset(PBVHNode* node)
//{
//	BB_reset(&node->vb);
//}
//
//void BLI_pbvh_node_BB_expand(PBVHNode* node, float co[3])
//{
//	BB_expand(&node->vb, co);
//}


/* Adapted from BLI_kdopbvh.c */
/* Returns the index of the first element on the right of the partition */
static int partition_indices(int *prim_indices, int lo, int hi, int axis,
				 float mid, BBC *prim_bbc)
{
	int i=lo, j=hi;
	for(;;) {
		for(; prim_bbc[prim_indices[i]].bcentroid[axis] < mid; i++);
		for(; mid < prim_bbc[prim_indices[j]].bcentroid[axis]; j--);
		
		if(!(i < j))
			return i;
		
		SWAP(int, prim_indices[i], prim_indices[j]);
		i++;
	}
}

static void grow_nodes(PBVH *bvh, int totnode)
{
	if(totnode > bvh->node_mem_count) {
		PBVHNode *prev = bvh->nodes;
		bvh->node_mem_count *= 1.33;
		if(bvh->node_mem_count < totnode)
			bvh->node_mem_count = totnode;
		bvh->nodes = MEM_callocN(sizeof(PBVHNode) * bvh->node_mem_count,
					 "bvh nodes");
		memcpy(bvh->nodes, prev, bvh->totnode * sizeof(PBVHNode));
		MEM_freeN(prev);
	}

	bvh->totnode = totnode;
}

/* Add a vertex to the map, with a positive value for unique vertices and
 * a negative value for additional vertices */
static int map_insert_vert(PBVH *bvh, GHash *map,
				unsigned int *face_verts,
				unsigned int *uniq_verts, int vertex)
{
	void *value, *key = SET_INT_IN_POINTER(vertex);

	if(!BLI_ghash_haskey(map, key)) {
		if(BLI_BITMAP_GET(bvh->vert_bitmap, vertex)) {
			value = SET_INT_IN_POINTER(~(*face_verts));
			++(*face_verts);
		}
		else {
			BLI_BITMAP_SET(bvh->vert_bitmap, vertex);
			value = SET_INT_IN_POINTER(*uniq_verts);
			++(*uniq_verts);
		}
		
		BLI_ghash_insert(map, key, value);
		return GET_INT_FROM_POINTER(value);
	}
	else
		return GET_INT_FROM_POINTER(BLI_ghash_lookup(map, key));
}

/* Find vertices used by the faces in this node and update the draw buffers */
static void build_mesh_leaf_node(PBVH *bvh, PBVHNode *node)
{
	GHashIterator *iter;
	GHash *map;
	int i, j, totface;

	map = BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp, "build_mesh_leaf_node gh");
	
	node->uniq_verts = node->face_verts = 0;
	totface= node->totprim;

	node->face_vert_indices = MEM_callocN(sizeof(int) * 4*totface,
					      "bvh node face vert indices");

	for(i = 0; i < totface; ++i) {
		MFace *f = bvh->faces + node->prim_indices[i];
		int sides = f->v4 ? 4 : 3;

		for(j = 0; j < sides; ++j) {
			node->face_vert_indices[i][j]= 
				map_insert_vert(bvh, map, &node->face_verts,
						&node->uniq_verts, (&f->v1)[j]);
		}
	}

	node->vert_indices = MEM_callocN(sizeof(int) *
					 (node->uniq_verts + node->face_verts),
					 "bvh node vert indices");

	/* Build the vertex list, unique verts first */
	for(iter = BLI_ghashIterator_new(map), i = 0;
		!BLI_ghashIterator_isDone(iter);
		BLI_ghashIterator_step(iter), ++i) {
		void *value = BLI_ghashIterator_getValue(iter);
		int ndx = GET_INT_FROM_POINTER(value);

		if(ndx < 0)
			ndx = -ndx + node->uniq_verts - 1;

		node->vert_indices[ndx] =
			GET_INT_FROM_POINTER(BLI_ghashIterator_getKey(iter));
	}

	BLI_ghashIterator_free(iter);

	for(i = 0; i < totface; ++i) {
		MFace *f = bvh->faces + node->prim_indices[i];
		int sides = f->v4 ? 4 : 3;
		
		for(j = 0; j < sides; ++j) {
			if(node->face_vert_indices[i][j] < 0)
				node->face_vert_indices[i][j]=
					-node->face_vert_indices[i][j] +
					node->uniq_verts - 1;
		}
	}

	if(!G.background) {
		node->draw_buffers =
			GPU_build_mesh_buffers(node->face_vert_indices,
					bvh->faces,
					node->prim_indices,
					node->totprim);
	}

	node->flag |= PBVH_UpdateDrawBuffers;

	BLI_ghash_free(map, NULL, NULL);
}

static void build_grids_leaf_node(PBVH *bvh, PBVHNode *node)
{
	if(!G.background) {
		node->draw_buffers =
			GPU_build_grid_buffers(node->totprim, bvh->gridsize);
	}
	node->flag |= PBVH_UpdateDrawBuffers;
}

static void build_leaf(PBVH *bvh, int node_index, const BBC *prim_bbc,
					   int offset, int count)
{
	int i;

	bvh->nodes[node_index].flag |= PBVH_Leaf;

	bvh->nodes[node_index].prim_indices = bvh->prim_indices + offset;
	bvh->nodes[node_index].totprim = count;

	/* Still need vb for searches */
	BB_reset(&bvh->nodes[node_index].vb);
	for(i = offset + count - 1; i >= offset; --i) {
		BB_expand_with_bb(&bvh->nodes[node_index].vb,
						  (BB*)(prim_bbc +
								bvh->prim_indices[i]));
	}
		
	if(bvh->faces)
		build_mesh_leaf_node(bvh, bvh->nodes + node_index);
	else
		build_grids_leaf_node(bvh, bvh->nodes + node_index);
	bvh->nodes[node_index].orig_vb= bvh->nodes[node_index].vb;
}

/* Recursively build a node in the tree
 *
 * vb is the voxel box around all of the primitives contained in
 * this node.
 *
 * cb is the bounding box around all the centroids of the primitives
 * contained in this node
 *
 * offset and start indicate a range in the array of primitive indices
 */

static void build_sub(PBVH *bvh, int node_index, BB *cb, BBC *prim_bbc,
		   int offset, int count)
{
	int i, axis, end;
	BB cb_backing;

	/* Decide whether this is a leaf or not */
	if(count <= bvh->leaf_limit) {
		build_leaf(bvh, node_index, prim_bbc, offset, count);
		return;
	}

	/* Add two child nodes */
	BB_reset(&bvh->nodes[node_index].vb);
	bvh->nodes[node_index].children_offset = bvh->totnode;
	grow_nodes(bvh, bvh->totnode + 2);

	/* Update parent node bounding box */
	for(i = offset + count - 1; i >= offset; --i) {
		BB_expand_with_bb(&bvh->nodes[node_index].vb,
				  (BB*)(prim_bbc + bvh->prim_indices[i]));
	}
	bvh->nodes[node_index].orig_vb= bvh->nodes[node_index].vb;

	/* Find axis with widest range of primitive centroids */
	if(!cb) {
		cb = &cb_backing;
		BB_reset(cb);
		for(i = offset + count - 1; i >= offset; --i)
			BB_expand(cb, prim_bbc[bvh->prim_indices[i]].bcentroid);
	}
	axis = BB_widest_axis(cb);

	/* Partition primitives along that axis */
	end = partition_indices(bvh->prim_indices, offset, offset + count - 1,
	                        axis,
	                        (cb->bmax[axis] + cb->bmin[axis]) * 0.5f,
	                        prim_bbc);

	/* Build children */
	build_sub(bvh, bvh->nodes[node_index].children_offset, NULL,
		  prim_bbc, offset, end - offset);
	build_sub(bvh, bvh->nodes[node_index].children_offset + 1, NULL,
		  prim_bbc, end, offset + count - end);
}

static void pbvh_build(PBVH *bvh, BB *cb, BBC *prim_bbc, int totprim)
{
	int i;

	if(totprim != bvh->totprim) {
		bvh->totprim = totprim;
		if(bvh->nodes) MEM_freeN(bvh->nodes);
		if(bvh->prim_indices) MEM_freeN(bvh->prim_indices);
		bvh->prim_indices = MEM_callocN(sizeof(int) * totprim,
						"bvh prim indices");
		for(i = 0; i < totprim; ++i)
			bvh->prim_indices[i] = i;
		bvh->totnode = 0;
		if(bvh->node_mem_count < 100) {
			bvh->node_mem_count = 100;
			bvh->nodes = MEM_callocN(sizeof(PBVHNode) *
						 bvh->node_mem_count,
						 "bvh initial nodes");
		}
	}

	bvh->totnode = 1;
	build_sub(bvh, 0, cb, prim_bbc, 0, totprim);
}

/* Do a full rebuild with on Mesh data structure */
void BLI_pbvh_build_mesh(PBVH *bvh, MFace *faces, MVert *verts, int totface, int totvert)
{
	BBC *prim_bbc = NULL;
	BB cb;
	int i, j;

	bvh->faces = faces;
	bvh->verts = verts;
	bvh->vert_bitmap = BLI_BITMAP_NEW(totvert, "bvh->vert_bitmap");
	bvh->totvert = totvert;
	bvh->leaf_limit = LEAF_LIMIT;

	BB_reset(&cb);

	/* For each face, store the AABB and the AABB centroid */
	prim_bbc = MEM_mallocN(sizeof(BBC) * totface, "prim_bbc");

	for(i = 0; i < totface; ++i) {
		MFace *f = faces + i;
		const int sides = f->v4 ? 4 : 3;
		BBC *bbc = prim_bbc + i;

		BB_reset((BB*)bbc);

		for(j = 0; j < sides; ++j)
			BB_expand((BB*)bbc, verts[(&f->v1)[j]].co);

		BBC_update_centroid(bbc);

		BB_expand(&cb, bbc->bcentroid);
	}

	if(totface)
		pbvh_build(bvh, &cb, prim_bbc, totface);

	MEM_freeN(prim_bbc);
	MEM_freeN(bvh->vert_bitmap);
}

/* Do a full rebuild with on Grids data structure */
void BLI_pbvh_build_grids(PBVH *bvh, DMGridData **grids, DMGridAdjacency *gridadj,
	int totgrid, int gridsize, void **gridfaces)
{
	BBC *prim_bbc = NULL;
	BB cb;
	int i, j;

	bvh->grids= grids;
	bvh->gridadj= gridadj;
	bvh->gridfaces= gridfaces;
	bvh->totgrid= totgrid;
	bvh->gridsize= gridsize;
	bvh->leaf_limit = MAX2(LEAF_LIMIT/((gridsize-1)*(gridsize-1)), 1);

	BB_reset(&cb);

	/* For each grid, store the AABB and the AABB centroid */
	prim_bbc = MEM_mallocN(sizeof(BBC) * totgrid, "prim_bbc");

	for(i = 0; i < totgrid; ++i) {
		DMGridData *grid= grids[i];
		BBC *bbc = prim_bbc + i;

		BB_reset((BB*)bbc);

		for(j = 0; j < gridsize*gridsize; ++j)
			BB_expand((BB*)bbc, grid[j].co);

		BBC_update_centroid(bbc);

		BB_expand(&cb, bbc->bcentroid);
	}

	if(totgrid)
		pbvh_build(bvh, &cb, prim_bbc, totgrid);

	MEM_freeN(prim_bbc);
}

PBVH *BLI_pbvh_new(void)
{
	PBVH *bvh = MEM_callocN(sizeof(PBVH), "pbvh");

	return bvh;
}

void BLI_pbvh_free(PBVH *bvh)
{
	PBVHNode *node;
	int i;

	for(i = 0; i < bvh->totnode; ++i) {
		node= &bvh->nodes[i];

		if(node->flag & PBVH_Leaf) {
			if(node->draw_buffers)
				GPU_free_buffers(node->draw_buffers);
			if(node->vert_indices)
				MEM_freeN(node->vert_indices);
			if(node->face_vert_indices)
				MEM_freeN(node->face_vert_indices);
		}
	}

	if (bvh->deformed) {
		if (bvh->verts) {
			/* if pbvh was deformed, new memory was allocated for verts/faces -- free it */

			MEM_freeN(bvh->verts);
			if(bvh->faces)
				MEM_freeN(bvh->faces);
		}
	}

	if(bvh->nodes)
		MEM_freeN(bvh->nodes);

	if(bvh->prim_indices)
		MEM_freeN(bvh->prim_indices);

	MEM_freeN(bvh);
}

static void pbvh_iter_begin(PBVHIter *iter, PBVH *bvh, BLI_pbvh_SearchCallback scb, void *search_data)
{
	iter->bvh= bvh;
	iter->scb= scb;
	iter->search_data= search_data;

	iter->stack= iter->stackfixed;
	iter->stackspace= STACK_FIXED_DEPTH;

	iter->stack[0].node= bvh->nodes;
	iter->stack[0].revisiting= 0;
	iter->stacksize= 1;
}

static void pbvh_iter_end(PBVHIter *iter)
{
	if(iter->stackspace > STACK_FIXED_DEPTH)
		MEM_freeN(iter->stack);
}

static void pbvh_stack_push(PBVHIter *iter, PBVHNode *node, int revisiting)
{
	if(iter->stacksize == iter->stackspace) {
		PBVHStack *newstack;

		iter->stackspace *= 2;
		newstack= MEM_callocN(sizeof(PBVHStack)*iter->stackspace, "PBVHStack");
		memcpy(newstack, iter->stack, sizeof(PBVHStack)*iter->stacksize);

		if(iter->stackspace > STACK_FIXED_DEPTH)
			MEM_freeN(iter->stack);
		iter->stack= newstack;
	}

	iter->stack[iter->stacksize].node= node;
	iter->stack[iter->stacksize].revisiting= revisiting;
	iter->stacksize++;
}

static PBVHNode *pbvh_iter_next(PBVHIter *iter)
{
	PBVHNode *node;
	int revisiting;

	/* purpose here is to traverse tree, visiting child nodes before their
	 * parents, this order is necessary for e.g. computing bounding boxes */

	while(iter->stacksize) {
		/* pop node */
		iter->stacksize--;
		node= iter->stack[iter->stacksize].node;

		/* on a mesh with no faces this can happen
		 * can remove this check if we know meshes have at least 1 face */
		if(node==NULL)
			return NULL;

		revisiting= iter->stack[iter->stacksize].revisiting;

		/* revisiting node already checked */
		if(revisiting)
			return node;

		if(iter->scb && !iter->scb(node, iter->search_data))
			continue; /* don't traverse, outside of search zone */

		if(node->flag & PBVH_Leaf) {
			/* immediately hit leaf node */
			return node;
		}
		else {
			/* come back later when children are done */
			pbvh_stack_push(iter, node, 1);

			/* push two child nodes on the stack */
			pbvh_stack_push(iter, iter->bvh->nodes+node->children_offset+1, 0);
			pbvh_stack_push(iter, iter->bvh->nodes+node->children_offset, 0);
		}
	}

	return NULL;
}

static PBVHNode *pbvh_iter_next_occluded(PBVHIter *iter)
{
	PBVHNode *node;

	while(iter->stacksize) {
		/* pop node */
		iter->stacksize--;
		node= iter->stack[iter->stacksize].node;

		/* on a mesh with no faces this can happen
		 * can remove this check if we know meshes have at least 1 face */
		if(node==NULL) return NULL;

		if(iter->scb && !iter->scb(node, iter->search_data)) continue; /* don't traverse, outside of search zone */

		if(node->flag & PBVH_Leaf) {
			/* immediately hit leaf node */
			return node;
		}
		else {
			pbvh_stack_push(iter, iter->bvh->nodes+node->children_offset+1, 0);
			pbvh_stack_push(iter, iter->bvh->nodes+node->children_offset, 0);
		}
	}

	return NULL;
}

void BLI_pbvh_search_gather(PBVH *bvh,
	BLI_pbvh_SearchCallback scb, void *search_data,
	PBVHNode ***r_array, int *r_tot)
{
	PBVHIter iter;
	PBVHNode **array= NULL, **newarray, *node;
	int tot= 0, space= 0;

	pbvh_iter_begin(&iter, bvh, scb, search_data);

	while((node=pbvh_iter_next(&iter))) {
		if(node->flag & PBVH_Leaf) {
			if(tot == space) {
				/* resize array if needed */
				space= (tot == 0)? 32: space*2;
				newarray= MEM_callocN(sizeof(PBVHNode)*space, "PBVHNodeSearch");

				if(array) {
					memcpy(newarray, array, sizeof(PBVHNode)*tot);
					MEM_freeN(array);
				}

				array= newarray;
			}

			array[tot]= node;
			tot++;
		}
	}

	pbvh_iter_end(&iter);

	if(tot == 0 && array) {
		MEM_freeN(array);
		array= NULL;
	}

	*r_array= array;
	*r_tot= tot;
}

void BLI_pbvh_search_callback(PBVH *bvh,
	BLI_pbvh_SearchCallback scb, void *search_data,
	BLI_pbvh_HitCallback hcb, void *hit_data)
{
	PBVHIter iter;
	PBVHNode *node;

	pbvh_iter_begin(&iter, bvh, scb, search_data);

	while((node=pbvh_iter_next(&iter)))
		if (node->flag & PBVH_Leaf)
			hcb(node, hit_data);

	pbvh_iter_end(&iter);
}

typedef struct node_tree {
	PBVHNode* data;

	struct node_tree* left;
	struct node_tree* right;
} node_tree;

static void node_tree_insert(node_tree* tree, node_tree* new_node)
{
	if (new_node->data->tmin < tree->data->tmin) {
		if (tree->left) {
			node_tree_insert(tree->left, new_node);
		}
		else {
			tree->left = new_node;
		}
	}
	else {
		if (tree->right) {
			node_tree_insert(tree->right, new_node);
		}
		else {
			tree->right = new_node;
		}
	}
}

static void traverse_tree(node_tree* tree, BLI_pbvh_HitOccludedCallback hcb, void* hit_data, float* tmin)
{
	if (tree->left) traverse_tree(tree->left, hcb, hit_data, tmin);

	hcb(tree->data, hit_data, tmin);

	if (tree->right) traverse_tree(tree->right, hcb, hit_data, tmin);
}

static void free_tree(node_tree* tree)
{
	if (tree->left) {
		free_tree(tree->left);
		tree->left = 0;
	}

	if (tree->right) {
		free_tree(tree->right);
		tree->right = 0;
	}

	free(tree);
}

float BLI_pbvh_node_get_tmin(PBVHNode* node)
{
	return node->tmin;
}

static void BLI_pbvh_search_callback_occluded(PBVH *bvh,
	BLI_pbvh_SearchCallback scb, void *search_data,
	BLI_pbvh_HitOccludedCallback hcb, void *hit_data)
{
	PBVHIter iter;
	PBVHNode *node;
	node_tree *tree = 0;

	pbvh_iter_begin(&iter, bvh, scb, search_data);

	while((node=pbvh_iter_next_occluded(&iter))) {
		if(node->flag & PBVH_Leaf) {
			node_tree* new_node = malloc(sizeof(node_tree));

			new_node->data = node;

			new_node->left  = NULL;
			new_node->right = NULL;

			if (tree) {
				node_tree_insert(tree, new_node);
			}
			else {
				tree = new_node;
			}
		}
	}

	pbvh_iter_end(&iter);

	if (tree) {
		float tmin = FLT_MAX;
		traverse_tree(tree, hcb, hit_data, &tmin);
		free_tree(tree);
	}
}

static int update_search_cb(PBVHNode *node, void *data_v)
{
	int flag= GET_INT_FROM_POINTER(data_v);

	if(node->flag & PBVH_Leaf)
		return (node->flag & flag);
	
	return 1;
}

static void pbvh_update_normals(PBVH *bvh, PBVHNode **nodes,
	int totnode, float (*face_nors)[3])
{
	float (*vnor)[3];
	int n;

	if(bvh->grids)
		return;

	/* could be per node to save some memory, but also means
	 * we have to store for each vertex which node it is in */
	vnor= MEM_callocN(sizeof(float)*3*bvh->totvert, "bvh temp vnors");

    /* subtle assumptions:
     * - We know that for all edited vertices, the nodes with faces
     *   adjacent to these vertices have been marked with PBVH_UpdateNormals.
     *   This is true because if the vertex is inside the brush radius, the
     *   bounding box of it's adjacent faces will be as well.
     * - However this is only true for the vertices that have actually been
     *   edited, not for all vertices in the nodes marked for update, so we
     *   can only update vertices marked with ME_VERT_PBVH_UPDATE.
     */

	#pragma omp parallel for private(n) schedule(static)
	for(n = 0; n < totnode; n++) {
		PBVHNode *node= nodes[n];

		if((node->flag & PBVH_UpdateNormals)) {
			int i, j, totface, *faces;

			faces= node->prim_indices;
			totface= node->totprim;

			for(i = 0; i < totface; ++i) {
				MFace *f= bvh->faces + faces[i];
				float fn[3];
				unsigned int *fv = &f->v1;
				int sides= (f->v4)? 4: 3;

				if(f->v4)
					normal_quad_v3(fn, bvh->verts[f->v1].co, bvh->verts[f->v2].co,
								   bvh->verts[f->v3].co, bvh->verts[f->v4].co);
				else
					normal_tri_v3(fn, bvh->verts[f->v1].co, bvh->verts[f->v2].co,
								  bvh->verts[f->v3].co);

				for(j = 0; j < sides; ++j) {
					int v= fv[j];

					if(bvh->verts[v].flag & ME_VERT_PBVH_UPDATE) {
						/* this seems like it could be very slow but profile
						 * does not show this, so just leave it for now? */
						#pragma omp atomic
						vnor[v][0] += fn[0];
						#pragma omp atomic
						vnor[v][1] += fn[1];
						#pragma omp atomic
						vnor[v][2] += fn[2];
					}
				}

				if(face_nors)
					copy_v3_v3(face_nors[faces[i]], fn);
			}
		}
	}

	#pragma omp parallel for private(n) schedule(static)
	for(n = 0; n < totnode; n++) {
		PBVHNode *node= nodes[n];

		if(node->flag & PBVH_UpdateNormals) {
			int i, *verts, totvert;

			verts= node->vert_indices;
			totvert= node->uniq_verts;

			for(i = 0; i < totvert; ++i) {
				const int v = verts[i];
				MVert *mvert= &bvh->verts[v];

				if(mvert->flag & ME_VERT_PBVH_UPDATE) {
					float no[3];

					copy_v3_v3(no, vnor[v]);
					normalize_v3(no);
					
					mvert->no[0] = (short)(no[0]*32767.0f);
					mvert->no[1] = (short)(no[1]*32767.0f);
					mvert->no[2] = (short)(no[2]*32767.0f);
					
					mvert->flag &= ~ME_VERT_PBVH_UPDATE;
				}
			}

			node->flag &= ~PBVH_UpdateNormals;
		}
	}

	MEM_freeN(vnor);
}

static void pbvh_update_BB_redraw(PBVH *bvh, PBVHNode **nodes,
	int totnode, int flag)
{
	int n;

	/* update BB, redraw flag */
	#pragma omp parallel for private(n) schedule(static)
	for(n = 0; n < totnode; n++) {
		PBVHNode *node= nodes[n];

		if((flag & PBVH_UpdateBB) && (node->flag & PBVH_UpdateBB))
			/* don't clear flag yet, leave it for flushing later */
			update_node_vb(bvh, node);

		if((flag & PBVH_UpdateOriginalBB) && (node->flag & PBVH_UpdateOriginalBB))
			node->orig_vb= node->vb;

		if((flag & PBVH_UpdateRedraw) && (node->flag & PBVH_UpdateRedraw))
			node->flag &= ~PBVH_UpdateRedraw;
	}
}

static void pbvh_update_draw_buffers(PBVH *bvh, PBVHNode **nodes, int totnode, int smooth)
{
	PBVHNode *node;
	int n;

	/* can't be done in parallel with OpenGL */
	for(n = 0; n < totnode; n++) {
		node= nodes[n];

		if(node->flag & PBVH_UpdateDrawBuffers) {
			if(bvh->grids) {
				GPU_update_grid_buffers(node->draw_buffers,
						   bvh->grids,
						   node->prim_indices,
						   node->totprim,
						   bvh->gridsize,
						   smooth);
			}
			else {
				GPU_update_mesh_buffers(node->draw_buffers,
						   bvh->verts,
						   node->vert_indices,
						   node->uniq_verts +
						   node->face_verts,
		                   smooth);
			}

			node->flag &= ~PBVH_UpdateDrawBuffers;
		}
	}
}

static int pbvh_flush_bb(PBVH *bvh, PBVHNode *node, int flag)
{
	int update= 0;

	/* difficult to multithread well, we just do single threaded recursive */
	if(node->flag & PBVH_Leaf) {
		if(flag & PBVH_UpdateBB) {
			update |= (node->flag & PBVH_UpdateBB);
			node->flag &= ~PBVH_UpdateBB;
		}

		if(flag & PBVH_UpdateOriginalBB) {
			update |= (node->flag & PBVH_UpdateOriginalBB);
			node->flag &= ~PBVH_UpdateOriginalBB;
		}

		return update;
	}
	else {
		update |= pbvh_flush_bb(bvh, bvh->nodes + node->children_offset, flag);
		update |= pbvh_flush_bb(bvh, bvh->nodes + node->children_offset + 1, flag);

		if(update & PBVH_UpdateBB)
			update_node_vb(bvh, node);
		if(update & PBVH_UpdateOriginalBB)
			node->orig_vb= node->vb;
	}

	return update;
}

void BLI_pbvh_update(PBVH *bvh, int flag, float (*face_nors)[3])
{
	PBVHNode **nodes;
	int totnode;

	if(!bvh->nodes)
		return;

	BLI_pbvh_search_gather(bvh, update_search_cb, SET_INT_IN_POINTER(flag),
		&nodes, &totnode);

	if(flag & PBVH_UpdateNormals)
		pbvh_update_normals(bvh, nodes, totnode, face_nors);

	if(flag & (PBVH_UpdateBB|PBVH_UpdateOriginalBB|PBVH_UpdateRedraw))
		pbvh_update_BB_redraw(bvh, nodes, totnode, flag);

	if(flag & (PBVH_UpdateBB|PBVH_UpdateOriginalBB))
		pbvh_flush_bb(bvh, bvh->nodes, flag);

	if(nodes) MEM_freeN(nodes);
}

void BLI_pbvh_redraw_BB(PBVH *bvh, float bb_min[3], float bb_max[3])
{
	PBVHIter iter;
	PBVHNode *node;
	BB bb;

	BB_reset(&bb);

	pbvh_iter_begin(&iter, bvh, NULL, NULL);

	while((node=pbvh_iter_next(&iter)))
		if(node->flag & PBVH_UpdateRedraw)
			BB_expand_with_bb(&bb, &node->vb);

	pbvh_iter_end(&iter);

	copy_v3_v3(bb_min, bb.bmin);
	copy_v3_v3(bb_max, bb.bmax);
}

void BLI_pbvh_get_grid_updates(PBVH *bvh, int clear, void ***gridfaces, int *totface)
{
	PBVHIter iter;
	PBVHNode *node;
	GHashIterator *hiter;
	GHash *map;
	void *face, **faces;
	unsigned i;
	int tot;

	map = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "pbvh_get_grid_updates gh");

	pbvh_iter_begin(&iter, bvh, NULL, NULL);

	while((node=pbvh_iter_next(&iter))) {
		if(node->flag & PBVH_UpdateNormals) {
			for(i = 0; i < node->totprim; ++i) {
				face= bvh->gridfaces[node->prim_indices[i]];
				if(!BLI_ghash_lookup(map, face))
					BLI_ghash_insert(map, face, face);
			}

			if(clear)
				node->flag &= ~PBVH_UpdateNormals;
		}
	}

	pbvh_iter_end(&iter);
	
	tot= BLI_ghash_size(map);
	if(tot == 0) {
		*totface= 0;
		*gridfaces= NULL;
		BLI_ghash_free(map, NULL, NULL);
		return;
	}

	faces= MEM_callocN(sizeof(void*)*tot, "PBVH Grid Faces");

	for(hiter = BLI_ghashIterator_new(map), i = 0;
		!BLI_ghashIterator_isDone(hiter);
		BLI_ghashIterator_step(hiter), ++i)
		faces[i]= BLI_ghashIterator_getKey(hiter);

	BLI_ghashIterator_free(hiter);

	BLI_ghash_free(map, NULL, NULL);

	*totface= tot;
	*gridfaces= faces;
}

/***************************** Node Access ***********************************/

void BLI_pbvh_node_mark_update(PBVHNode *node)
{
	node->flag |= PBVH_UpdateNormals|PBVH_UpdateBB|PBVH_UpdateOriginalBB|PBVH_UpdateDrawBuffers|PBVH_UpdateRedraw;
}

void BLI_pbvh_node_get_verts(PBVH *bvh, PBVHNode *node, int **vert_indices, MVert **verts)
{
	if(vert_indices) *vert_indices= node->vert_indices;
	if(verts) *verts= bvh->verts;
}

void BLI_pbvh_node_num_verts(PBVH *bvh, PBVHNode *node, int *uniquevert, int *totvert)
{
	if(bvh->grids) {
		const int tot= node->totprim*bvh->gridsize*bvh->gridsize;
		if(totvert) *totvert= tot;
		if(uniquevert) *uniquevert= tot;
	}
	else {
		if(totvert) *totvert= node->uniq_verts + node->face_verts;
		if(uniquevert) *uniquevert= node->uniq_verts;
	}
}

void BLI_pbvh_node_get_grids(PBVH *bvh, PBVHNode *node, int **grid_indices, int *totgrid, int *maxgrid, int *gridsize, DMGridData ***griddata, DMGridAdjacency **gridadj)
{
	if(bvh->grids) {
		if(grid_indices) *grid_indices= node->prim_indices;
		if(totgrid) *totgrid= node->totprim;
		if(maxgrid) *maxgrid= bvh->totgrid;
		if(gridsize) *gridsize= bvh->gridsize;
		if(griddata) *griddata= bvh->grids;
		if(gridadj) *gridadj= bvh->gridadj;
	}
	else {
		if(grid_indices) *grid_indices= NULL;
		if(totgrid) *totgrid= 0;
		if(maxgrid) *maxgrid= 0;
		if(gridsize) *gridsize= 0;
		if(griddata) *griddata= NULL;
		if(gridadj) *gridadj= NULL;
	}
}

void BLI_pbvh_node_get_BB(PBVHNode *node, float bb_min[3], float bb_max[3])
{
	copy_v3_v3(bb_min, node->vb.bmin);
	copy_v3_v3(bb_max, node->vb.bmax);
}

void BLI_pbvh_node_get_original_BB(PBVHNode *node, float bb_min[3], float bb_max[3])
{
	copy_v3_v3(bb_min, node->orig_vb.bmin);
	copy_v3_v3(bb_max, node->orig_vb.bmax);
}

void BLI_pbvh_node_get_proxies(PBVHNode* node, PBVHProxyNode** proxies, int* proxy_count)
{
	if (node->proxy_count > 0) {
		if (proxies) *proxies = node->proxies;
		if (proxy_count) *proxy_count = node->proxy_count;
	}
	else {
		if (proxies) *proxies = 0;
		if (proxy_count) *proxy_count = 0;
	}
}

/********************************* Raycast ***********************************/

typedef struct {
	/* Ray */
	float start[3];
	int sign[3];
	float inv_dir[3];
	int original;
} RaycastData;

/* Adapted from here: http://www.gamedev.net/community/forums/topic.asp?topic_id=459973 */
static int ray_aabb_intersect(PBVHNode *node, void *data_v)
{
	RaycastData *ray = data_v;
	float bbox[2][3];
	float tmin, tmax, tymin, tymax, tzmin, tzmax;

	if(ray->original)
		BLI_pbvh_node_get_original_BB(node, bbox[0], bbox[1]);
	else
		BLI_pbvh_node_get_BB(node, bbox[0], bbox[1]);

	tmin = (bbox[ray->sign[0]][0] - ray->start[0]) * ray->inv_dir[0];
	tmax = (bbox[1-ray->sign[0]][0] - ray->start[0]) * ray->inv_dir[0];

	tymin = (bbox[ray->sign[1]][1] - ray->start[1]) * ray->inv_dir[1];
	tymax = (bbox[1-ray->sign[1]][1] - ray->start[1]) * ray->inv_dir[1];

	if((tmin > tymax) || (tymin > tmax))
		return 0;

	if(tymin > tmin)
		tmin = tymin;

	if(tymax < tmax)
		tmax = tymax;

	tzmin = (bbox[ray->sign[2]][2] - ray->start[2]) * ray->inv_dir[2];
	tzmax = (bbox[1-ray->sign[2]][2] - ray->start[2]) * ray->inv_dir[2];

	if((tmin > tzmax) || (tzmin > tmax))
		return 0;

	if(tzmin > tmin)
		tmin = tzmin;

	// XXX jwilkins: tmax does not need to be updated since we don't use it
	// keeping this here for future reference
	//if(tzmax < tmax) tmax = tzmax; 

	node->tmin = tmin;

	return 1;
}

void BLI_pbvh_raycast(PBVH *bvh, BLI_pbvh_HitOccludedCallback cb, void *data,
			  float ray_start[3], float ray_normal[3], int original)
{
	RaycastData rcd;

	copy_v3_v3(rcd.start, ray_start);
	rcd.inv_dir[0] = 1.0f / ray_normal[0];
	rcd.inv_dir[1] = 1.0f / ray_normal[1];
	rcd.inv_dir[2] = 1.0f / ray_normal[2];
	rcd.sign[0] = rcd.inv_dir[0] < 0;
	rcd.sign[1] = rcd.inv_dir[1] < 0;
	rcd.sign[2] = rcd.inv_dir[2] < 0;
	rcd.original = original;

	BLI_pbvh_search_callback_occluded(bvh, ray_aabb_intersect, &rcd, cb, data);
}

static int ray_face_intersection(float ray_start[3], float ray_normal[3],
				 float *t0, float *t1, float *t2, float *t3,
				 float *fdist)
{
	float dist;

	if ((isect_ray_tri_epsilon_v3(ray_start, ray_normal, t0, t1, t2, &dist, NULL, 0.1f) && dist < *fdist) ||
	   (t3 && isect_ray_tri_epsilon_v3(ray_start, ray_normal, t0, t2, t3, &dist, NULL, 0.1f) && dist < *fdist))
	{
		*fdist = dist;
		return 1;
	}
	else {
		return 0;
	}
}

int BLI_pbvh_node_raycast(PBVH *bvh, PBVHNode *node, float (*origco)[3],
	float ray_start[3], float ray_normal[3], float *dist)
{
	int hit= 0;

	if(bvh->faces) {
		MVert *vert = bvh->verts;
		int *faces= node->prim_indices;
		int totface= node->totprim;
		int i;

		for(i = 0; i < totface; ++i) {
			MFace *f = bvh->faces + faces[i];
			int *face_verts = node->face_vert_indices[i];

			if(origco) {
				/* intersect with backuped original coordinates */
				hit |= ray_face_intersection(ray_start, ray_normal,
							 origco[face_verts[0]],
							 origco[face_verts[1]],
							 origco[face_verts[2]],
							 f->v4? origco[face_verts[3]]: NULL,
							 dist);
			}
			else {
				/* intersect with current coordinates */
				hit |= ray_face_intersection(ray_start, ray_normal,
							 vert[f->v1].co,
							 vert[f->v2].co,
							 vert[f->v3].co,
							 f->v4 ? vert[f->v4].co : NULL,
							 dist);
			}
		}
	}
	else {
		int totgrid= node->totprim;
		int gridsize= bvh->gridsize;
		int i, x, y;

		for(i = 0; i < totgrid; ++i) {
			DMGridData *grid= bvh->grids[node->prim_indices[i]];
			if (!grid)
				continue;

			for(y = 0; y < gridsize-1; ++y) {
				for(x = 0; x < gridsize-1; ++x) {
					if(origco) {
						hit |= ray_face_intersection(ray_start, ray_normal,
									 origco[y*gridsize + x],
									 origco[y*gridsize + x+1],
									 origco[(y+1)*gridsize + x+1],
									 origco[(y+1)*gridsize + x],
									 dist);
					}
					else {
						hit |= ray_face_intersection(ray_start, ray_normal,
									 grid[y*gridsize + x].co,
									 grid[y*gridsize + x+1].co,
									 grid[(y+1)*gridsize + x+1].co,
									 grid[(y+1)*gridsize + x].co,
									 dist);
					}
				}
			}

			if(origco)
				origco += gridsize*gridsize;
		}
	}

	return hit;
}

//#include <GL/glew.h>

void BLI_pbvh_node_draw(PBVHNode *node, void *UNUSED(data))
{
#if 0
	/* XXX: Just some quick code to show leaf nodes in different colors */
	float col[3]; int i;

	if(0) { //is_partial) {
		col[0] = (rand() / (float)RAND_MAX); col[1] = col[2] = 0.6;
	}
	else {
		srand((long long)node);
		for(i = 0; i < 3; ++i)
			col[i] = (rand() / (float)RAND_MAX) * 0.3 + 0.7;
	}
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, col);

	glColor3f(1, 0, 0);
#endif
	GPU_draw_buffers(node->draw_buffers);
}

/* Adapted from:
 * http://www.gamedev.net/community/forums/topic.asp?topic_id=512123
 * Returns true if the AABB is at least partially within the frustum
 * (ok, not a real frustum), false otherwise.
 */
int BLI_pbvh_node_planes_contain_AABB(PBVHNode *node, void *data)
{
	float (*planes)[4] = data;
	int i, axis;
	float vmin[3] /*, vmax[3]*/, bb_min[3], bb_max[3];

	BLI_pbvh_node_get_BB(node, bb_min, bb_max);

	for(i = 0; i < 4; ++i) { 
		for(axis = 0; axis < 3; ++axis) {
			if(planes[i][axis] > 0) { 
				vmin[axis] = bb_min[axis];
				/*vmax[axis] = bb_max[axis];*/ /*UNUSED*/
			}
			else {
				vmin[axis] = bb_max[axis];
				/*vmax[axis] = bb_min[axis];*/ /*UNUSED*/
			}
		}
		
		if(dot_v3v3(planes[i], vmin) + planes[i][3] > 0)
			return 0;
	} 

	return 1;
}

void BLI_pbvh_draw(PBVH *bvh, float (*planes)[4], float (*face_nors)[3], int smooth)
{
	PBVHNode **nodes;
	int totnode;

	BLI_pbvh_search_gather(bvh, update_search_cb, SET_INT_IN_POINTER(PBVH_UpdateNormals|PBVH_UpdateDrawBuffers),
		&nodes, &totnode);

	pbvh_update_normals(bvh, nodes, totnode, face_nors);
	pbvh_update_draw_buffers(bvh, nodes, totnode, smooth);

	if(nodes) MEM_freeN(nodes);

	if(planes) {
		BLI_pbvh_search_callback(bvh, BLI_pbvh_node_planes_contain_AABB,
				planes, BLI_pbvh_node_draw, NULL);
	}
	else {
		BLI_pbvh_search_callback(bvh, NULL, NULL, BLI_pbvh_node_draw, NULL);
	}
}

void BLI_pbvh_grids_update(PBVH *bvh, DMGridData **grids, DMGridAdjacency *gridadj, void **gridfaces)
{
	bvh->grids= grids;
	bvh->gridadj= gridadj;
	bvh->gridfaces= gridfaces;
}

float (*BLI_pbvh_get_vertCos(PBVH *pbvh))[3]
{
	int a;
	float (*vertCos)[3]= NULL;

	if (pbvh->verts) {
		float *co;
		MVert *mvert= pbvh->verts;

		vertCos= MEM_callocN(3*pbvh->totvert*sizeof(float), "BLI_pbvh_get_vertCoords");
		co= (float*)vertCos;

		for (a= 0; a<pbvh->totvert; a++, mvert++, co+= 3) {
			copy_v3_v3(co, mvert->co);
		}
	}

	return vertCos;
}

void BLI_pbvh_apply_vertCos(PBVH *pbvh, float (*vertCos)[3])
{
	int a;

	if (!pbvh->deformed) {
		if (pbvh->verts) {
			/* if pbvh is not already deformed, verts/faces points to the */
			/* original data and applying new coords to this arrays would lead to */
			/* unneeded deformation -- duplicate verts/faces to avoid this */

			pbvh->verts= MEM_dupallocN(pbvh->verts);
			pbvh->faces= MEM_dupallocN(pbvh->faces);

			pbvh->deformed= 1;
		}
	}

	if (pbvh->verts) {
		MVert *mvert= pbvh->verts;
		/* copy new verts coords */
		for (a= 0; a < pbvh->totvert; ++a, ++mvert) {
			copy_v3_v3(mvert->co, vertCos[a]);
			mvert->flag |= ME_VERT_PBVH_UPDATE;
		}

		/* coordinates are new -- normals should also be updated */
		mesh_calc_normals_tessface(pbvh->verts, pbvh->totvert, pbvh->faces, pbvh->totprim, NULL);

		for (a= 0; a < pbvh->totnode; ++a)
			BLI_pbvh_node_mark_update(&pbvh->nodes[a]);

		BLI_pbvh_update(pbvh, PBVH_UpdateBB, NULL);
		BLI_pbvh_update(pbvh, PBVH_UpdateOriginalBB, NULL);

	}
}

int BLI_pbvh_isDeformed(PBVH *pbvh)
{
	return pbvh->deformed;
}
/* Proxies */

PBVHProxyNode* BLI_pbvh_node_add_proxy(PBVH* bvh, PBVHNode* node)
{
	int index, totverts;

	#pragma omp critical
	{

		index = node->proxy_count;

		node->proxy_count++;

		if (node->proxies)
			node->proxies= MEM_reallocN(node->proxies, node->proxy_count*sizeof(PBVHProxyNode));
		else
			node->proxies= MEM_mallocN(sizeof(PBVHProxyNode), "PBVHNodeProxy");

		if (bvh->grids)
			totverts = node->totprim*bvh->gridsize*bvh->gridsize;
		else
			totverts = node->uniq_verts;

		node->proxies[index].co= MEM_callocN(sizeof(float[3])*totverts, "PBVHNodeProxy.co");
	}

	return node->proxies + index;
}

void BLI_pbvh_node_free_proxies(PBVHNode* node)
{
	#pragma omp critical
	{
		int p;

		for (p= 0; p < node->proxy_count; p++) {
			MEM_freeN(node->proxies[p].co);
			node->proxies[p].co= 0;
		}

		MEM_freeN(node->proxies);
		node->proxies = 0;

		node->proxy_count= 0;
	}
}

void BLI_pbvh_gather_proxies(PBVH* pbvh, PBVHNode*** r_array,  int* r_tot)
{
	PBVHNode **array= NULL, **newarray, *node;
	int tot= 0, space= 0;
	int n;

	for (n= 0; n < pbvh->totnode; n++) {
		node = pbvh->nodes + n;

		if(node->proxy_count > 0) {
			if(tot == space) {
				/* resize array if needed */
				space= (tot == 0)? 32: space*2;
				newarray= MEM_callocN(sizeof(PBVHNode)*space, "BLI_pbvh_gather_proxies");

				if (array) {
					memcpy(newarray, array, sizeof(PBVHNode)*tot);
					MEM_freeN(array);
				}

				array= newarray;
			}

			array[tot]= node;
			tot++;
		}
	}

	if(tot == 0 && array) {
		MEM_freeN(array);
		array= NULL;
	}

	*r_array= array;
	*r_tot= tot;
}

void pbvh_vertex_iter_init(PBVH *bvh, PBVHNode *node,
						   PBVHVertexIter *vi, int mode)
{
	struct DMGridData **grids;
	struct MVert *verts;
	int *grid_indices, *vert_indices;
	int totgrid, gridsize, uniq_verts, totvert;
	
	vi->grid= 0;
	vi->no= 0;
	vi->fno= 0;
	vi->mvert= 0;
	
	BLI_pbvh_node_get_grids(bvh, node, &grid_indices, &totgrid, NULL, &gridsize, &grids, NULL);
	BLI_pbvh_node_num_verts(bvh, node, &uniq_verts, &totvert);
	BLI_pbvh_node_get_verts(bvh, node, &vert_indices, &verts);
	
	vi->grids= grids;
	vi->grid_indices= grid_indices;
	vi->totgrid= (grids)? totgrid: 1;
	vi->gridsize= gridsize;
	
	if(mode == PBVH_ITER_ALL)
		vi->totvert = totvert;
	else
		vi->totvert= uniq_verts;
	vi->vert_indices= vert_indices;
	vi->mverts= verts;
}
