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

/** \file blender/blenkernel/intern/pbvh.c
 *  \ingroup bli
 */

#include "DNA_meshdata_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_bitmap.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"
#include "BLI_ghash.h"

#include "BKE_pbvh.h"
#include "BKE_ccg.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_mesh.h" /* for BKE_mesh_calc_normals */
#include "BKE_paint.h"

#include "GPU_buffers.h"

#include "bmesh.h"

#include "pbvh_intern.h"

#define LEAF_LIMIT 10000

//#define PERFCNTRS

#define STACK_FIXED_DEPTH   100

/* Setting zero so we can catch bugs in OpenMP/PBVH. */
#ifdef _OPENMP
#  ifdef DEBUG
#    define PBVH_OMP_LIMIT 0
#  else
#    define PBVH_OMP_LIMIT 8
#  endif
#endif

typedef struct PBVHStack {
	PBVHNode *node;
	int revisiting;
} PBVHStack;

typedef struct PBVHIter {
	PBVH *bvh;
	BKE_pbvh_SearchCallback scb;
	void *search_data;

	PBVHStack *stack;
	int stacksize;

	PBVHStack stackfixed[STACK_FIXED_DEPTH];
	int stackspace;
} PBVHIter;

void BB_reset(BB *bb)
{
	bb->bmin[0] = bb->bmin[1] = bb->bmin[2] = FLT_MAX;
	bb->bmax[0] = bb->bmax[1] = bb->bmax[2] = -FLT_MAX;
}

/* Expand the bounding box to include a new coordinate */
void BB_expand(BB *bb, const float co[3])
{
	int i;
	for (i = 0; i < 3; ++i) {
		bb->bmin[i] = min_ff(bb->bmin[i], co[i]);
		bb->bmax[i] = max_ff(bb->bmax[i], co[i]);
	}
}

/* Expand the bounding box to include another bounding box */
void BB_expand_with_bb(BB *bb, BB *bb2)
{
	int i;
	for (i = 0; i < 3; ++i) {
		bb->bmin[i] = min_ff(bb->bmin[i], bb2->bmin[i]);
		bb->bmax[i] = max_ff(bb->bmax[i], bb2->bmax[i]);
	}
}

/* Return 0, 1, or 2 to indicate the widest axis of the bounding box */
int BB_widest_axis(const BB *bb)
{
	float dim[3];
	int i;

	for (i = 0; i < 3; ++i)
		dim[i] = bb->bmax[i] - bb->bmin[i];

	if (dim[0] > dim[1]) {
		if (dim[0] > dim[2])
			return 0;
		else
			return 2;
	}
	else {
		if (dim[1] > dim[2])
			return 1;
		else
			return 2;
	}
}

void BBC_update_centroid(BBC *bbc)
{
	int i;
	for (i = 0; i < 3; ++i)
		bbc->bcentroid[i] = (bbc->bmin[i] + bbc->bmax[i]) * 0.5f;
}

/* Not recursive */
static void update_node_vb(PBVH *bvh, PBVHNode *node)
{
	BB vb;

	BB_reset(&vb);
	
	if (node->flag & PBVH_Leaf) {
		PBVHVertexIter vd;

		BKE_pbvh_vertex_iter_begin(bvh, node, vd, PBVH_ITER_ALL)
		{
			BB_expand(&vb, vd.co);
		}
		BKE_pbvh_vertex_iter_end;
	}
	else {
		BB_expand_with_bb(&vb,
		                  &bvh->nodes[node->children_offset].vb);
		BB_expand_with_bb(&vb,
		                  &bvh->nodes[node->children_offset + 1].vb);
	}

	node->vb = vb;
}

//void BKE_pbvh_node_BB_reset(PBVHNode *node)
//{
//	BB_reset(&node->vb);
//}
//
//void BKE_pbvh_node_BB_expand(PBVHNode *node, float co[3])
//{
//	BB_expand(&node->vb, co);
//}

static int face_materials_match(const MFace *f1, const MFace *f2)
{
	return ((f1->flag & ME_SMOOTH) == (f2->flag & ME_SMOOTH) &&
	        (f1->mat_nr == f2->mat_nr));
}

static int grid_materials_match(const DMFlagMat *f1, const DMFlagMat *f2)
{
	return ((f1->flag & ME_SMOOTH) == (f2->flag & ME_SMOOTH) &&
	        (f1->mat_nr == f2->mat_nr));
}

/* Adapted from BLI_kdopbvh.c */
/* Returns the index of the first element on the right of the partition */
static int partition_indices(int *prim_indices, int lo, int hi, int axis,
                             float mid, BBC *prim_bbc)
{
	int i = lo, j = hi;
	for (;; ) {
		for (; prim_bbc[prim_indices[i]].bcentroid[axis] < mid; i++) ;
		for (; mid < prim_bbc[prim_indices[j]].bcentroid[axis]; j--) ;
		
		if (!(i < j))
			return i;
		
		SWAP(int, prim_indices[i], prim_indices[j]);
		i++;
	}
}

/* Returns the index of the first element on the right of the partition */
static int partition_indices_material(PBVH *bvh, int lo, int hi)
{
	const MFace *faces = bvh->faces;
	const DMFlagMat *flagmats = bvh->grid_flag_mats;
	const int *indices = bvh->prim_indices;
	const void *first;
	int i = lo, j = hi;

	if (bvh->faces)
		first = &faces[bvh->prim_indices[lo]];
	else
		first = &flagmats[bvh->prim_indices[lo]];

	for (;; ) {
		if (bvh->faces) {
			for (; face_materials_match(first, &faces[indices[i]]); i++) ;
			for (; !face_materials_match(first, &faces[indices[j]]); j--) ;
		}
		else {
			for (; grid_materials_match(first, &flagmats[indices[i]]); i++) ;
			for (; !grid_materials_match(first, &flagmats[indices[j]]); j--) ;
		}
		
		if (!(i < j))
			return i;

		SWAP(int, bvh->prim_indices[i], bvh->prim_indices[j]);
		i++;
	}
}

void pbvh_grow_nodes(PBVH *bvh, int totnode)
{
	if (totnode > bvh->node_mem_count) {
		PBVHNode *prev = bvh->nodes;
		bvh->node_mem_count *= 1.33;
		if (bvh->node_mem_count < totnode)
			bvh->node_mem_count = totnode;
		bvh->nodes = MEM_mallocN(sizeof(PBVHNode) * bvh->node_mem_count,
		                         "bvh nodes");
		memcpy(bvh->nodes, prev, bvh->totnode * sizeof(PBVHNode));
		memset(bvh->nodes + bvh->totnode, 0, (bvh->node_mem_count - bvh->totnode) * sizeof(PBVHNode));
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
	void *key, **value_p;

	key = SET_INT_IN_POINTER(vertex);
	value_p = BLI_ghash_lookup_p(map, key);

	if (value_p == NULL) {
		void *value;
		if (BLI_BITMAP_TEST(bvh->vert_bitmap, vertex)) {
			value = SET_INT_IN_POINTER(~(*face_verts));
			++(*face_verts);
		}
		else {
			BLI_BITMAP_ENABLE(bvh->vert_bitmap, vertex);
			value = SET_INT_IN_POINTER(*uniq_verts);
			++(*uniq_verts);
		}
		
		BLI_ghash_insert(map, key, value);
		return GET_INT_FROM_POINTER(value);
	}
	else {
		return GET_INT_FROM_POINTER(*value_p);
	}
}

/* Find vertices used by the faces in this node and update the draw buffers */
static void build_mesh_leaf_node(PBVH *bvh, PBVHNode *node)
{
	GHashIterator *iter;
	GHash *map;
	int i, j, totface;
	bool has_visible = false;

	node->uniq_verts = node->face_verts = 0;
	totface = node->totprim;

	/* reserve size is rough guess */
	map = BLI_ghash_int_new_ex("build_mesh_leaf_node gh", 2 * totface);

	node->face_vert_indices = MEM_callocN(sizeof(int) * 4 * totface,
	                                      "bvh node face vert indices");

	for (i = 0; i < totface; ++i) {
		MFace *f = bvh->faces + node->prim_indices[i];
		int sides = f->v4 ? 4 : 3;

		for (j = 0; j < sides; ++j) {
			node->face_vert_indices[i][j] =
			        map_insert_vert(bvh, map, &node->face_verts,
			                        &node->uniq_verts, (&f->v1)[j]);
		}

		if (!paint_is_face_hidden(f, bvh->verts))
			has_visible = true;
	}

	node->vert_indices = MEM_callocN(sizeof(int) *
	                                 (node->uniq_verts + node->face_verts),
	                                 "bvh node vert indices");

	/* Build the vertex list, unique verts first */
	for (iter = BLI_ghashIterator_new(map), i = 0;
	     BLI_ghashIterator_done(iter) == false;
	     BLI_ghashIterator_step(iter), ++i)
	{
		void *value = BLI_ghashIterator_getValue(iter);
		int ndx = GET_INT_FROM_POINTER(value);

		if (ndx < 0)
			ndx = -ndx + node->uniq_verts - 1;

		node->vert_indices[ndx] =
		    GET_INT_FROM_POINTER(BLI_ghashIterator_getKey(iter));
	}

	BLI_ghashIterator_free(iter);

	for (i = 0; i < totface; ++i) {
		MFace *f = bvh->faces + node->prim_indices[i];
		int sides = f->v4 ? 4 : 3;
		
		for (j = 0; j < sides; ++j) {
			if (node->face_vert_indices[i][j] < 0)
				node->face_vert_indices[i][j] =
				        -node->face_vert_indices[i][j] +
				        node->uniq_verts - 1;
		}
	}

	BKE_pbvh_node_mark_rebuild_draw(node);

	BKE_pbvh_node_fully_hidden_set(node, !has_visible);

	BLI_ghash_free(map, NULL, NULL);
}

static void update_vb(PBVH *bvh, PBVHNode *node, BBC *prim_bbc,
                      int offset, int count)
{
	int i;
	
	BB_reset(&node->vb);
	for (i = offset + count - 1; i >= offset; --i) {
		BB_expand_with_bb(&node->vb, (BB *)(&prim_bbc[bvh->prim_indices[i]]));
	}
	node->orig_vb = node->vb;
}

/* Returns the number of visible quads in the nodes' grids. */
int BKE_pbvh_count_grid_quads(BLI_bitmap **grid_hidden,
                              int *grid_indices, int totgrid,
                              int gridsize)
{
	int gridarea = (gridsize - 1) * (gridsize - 1);
	int i, x, y, totquad;

	/* grid hidden layer is present, so have to check each grid for
	 * visibility */

	for (i = 0, totquad = 0; i < totgrid; i++) {
		const BLI_bitmap *gh = grid_hidden[grid_indices[i]];

		if (gh) {
			/* grid hidden are present, have to check each element */
			for (y = 0; y < gridsize - 1; y++) {
				for (x = 0; x < gridsize - 1; x++) {
					if (!paint_is_grid_face_hidden(gh, gridsize, x, y))
						totquad++;
				}
			}
		}
		else
			totquad += gridarea;
	}

	return totquad;
}

static void build_grid_leaf_node(PBVH *bvh, PBVHNode *node)
{
	int totquads = BKE_pbvh_count_grid_quads(bvh->grid_hidden, node->prim_indices,
	                                         node->totprim, bvh->gridkey.grid_size);
	BKE_pbvh_node_fully_hidden_set(node, (totquads == 0));
	BKE_pbvh_node_mark_rebuild_draw(node);
}


static void build_leaf(PBVH *bvh, int node_index, BBC *prim_bbc,
                       int offset, int count)
{
	bvh->nodes[node_index].flag |= PBVH_Leaf;

	bvh->nodes[node_index].prim_indices = bvh->prim_indices + offset;
	bvh->nodes[node_index].totprim = count;

	/* Still need vb for searches */
	update_vb(bvh, &bvh->nodes[node_index], prim_bbc, offset, count);
		
	if (bvh->faces)
		build_mesh_leaf_node(bvh, bvh->nodes + node_index);
	else {
		build_grid_leaf_node(bvh, bvh->nodes + node_index);
	}
}

/* Return zero if all primitives in the node can be drawn with the
 * same material (including flat/smooth shading), non-zero otherwise */
static int leaf_needs_material_split(PBVH *bvh, int offset, int count)
{
	int i, prim;

	if (count <= 1)
		return 0;

	if (bvh->faces) {
		const MFace *first = &bvh->faces[bvh->prim_indices[offset]];

		for (i = offset + count - 1; i > offset; --i) {
			prim = bvh->prim_indices[i];
			if (!face_materials_match(first, &bvh->faces[prim]))
				return 1;
		}
	}
	else {
		const DMFlagMat *first = &bvh->grid_flag_mats[bvh->prim_indices[offset]];

		for (i = offset + count - 1; i > offset; --i) {
			prim = bvh->prim_indices[i];
			if (!grid_materials_match(first, &bvh->grid_flag_mats[prim]))
				return 1;
		}
	}

	return 0;
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
	int i, axis, end, below_leaf_limit;
	BB cb_backing;

	/* Decide whether this is a leaf or not */
	below_leaf_limit = count <= bvh->leaf_limit;
	if (below_leaf_limit) {
		if (!leaf_needs_material_split(bvh, offset, count)) {
			build_leaf(bvh, node_index, prim_bbc, offset, count);
			return;
		}
	}

	/* Add two child nodes */
	bvh->nodes[node_index].children_offset = bvh->totnode;
	pbvh_grow_nodes(bvh, bvh->totnode + 2);

	/* Update parent node bounding box */
	update_vb(bvh, &bvh->nodes[node_index], prim_bbc, offset, count);

	if (!below_leaf_limit) {
		/* Find axis with widest range of primitive centroids */
		if (!cb) {
			cb = &cb_backing;
			BB_reset(cb);
			for (i = offset + count - 1; i >= offset; --i)
				BB_expand(cb, prim_bbc[bvh->prim_indices[i]].bcentroid);
		}
		axis = BB_widest_axis(cb);

		/* Partition primitives along that axis */
		end = partition_indices(bvh->prim_indices,
		                        offset, offset + count - 1,
		                        axis,
		                        (cb->bmax[axis] + cb->bmin[axis]) * 0.5f,
		                        prim_bbc);
	}
	else {
		/* Partition primitives by material */
		end = partition_indices_material(bvh, offset, offset + count - 1);
	}

	/* Build children */
	build_sub(bvh, bvh->nodes[node_index].children_offset, NULL,
	          prim_bbc, offset, end - offset);
	build_sub(bvh, bvh->nodes[node_index].children_offset + 1, NULL,
	          prim_bbc, end, offset + count - end);
}

static void pbvh_build(PBVH *bvh, BB *cb, BBC *prim_bbc, int totprim)
{
	int i;

	if (totprim != bvh->totprim) {
		bvh->totprim = totprim;
		if (bvh->nodes) MEM_freeN(bvh->nodes);
		if (bvh->prim_indices) MEM_freeN(bvh->prim_indices);
		bvh->prim_indices = MEM_callocN(sizeof(int) * totprim,
		                                "bvh prim indices");
		for (i = 0; i < totprim; ++i)
			bvh->prim_indices[i] = i;
		bvh->totnode = 0;
		if (bvh->node_mem_count < 100) {
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
void BKE_pbvh_build_mesh(PBVH *bvh, MFace *faces, MVert *verts, int totface, int totvert, struct CustomData *vdata)
{
	BBC *prim_bbc = NULL;
	BB cb;
	int i, j;

	bvh->type = PBVH_FACES;
	bvh->faces = faces;
	bvh->verts = verts;
	bvh->vert_bitmap = BLI_BITMAP_NEW(totvert, "bvh->vert_bitmap");
	bvh->totvert = totvert;
	bvh->leaf_limit = LEAF_LIMIT;
	bvh->vdata = vdata;

	BB_reset(&cb);

	/* For each face, store the AABB and the AABB centroid */
	prim_bbc = MEM_mallocN(sizeof(BBC) * totface, "prim_bbc");

	for (i = 0; i < totface; ++i) {
		MFace *f = faces + i;
		const int sides = f->v4 ? 4 : 3;
		BBC *bbc = prim_bbc + i;

		BB_reset((BB *)bbc);

		for (j = 0; j < sides; ++j)
			BB_expand((BB *)bbc, verts[(&f->v1)[j]].co);

		BBC_update_centroid(bbc);

		BB_expand(&cb, bbc->bcentroid);
	}

	if (totface)
		pbvh_build(bvh, &cb, prim_bbc, totface);

	MEM_freeN(prim_bbc);
	MEM_freeN(bvh->vert_bitmap);
}

/* Do a full rebuild with on Grids data structure */
void BKE_pbvh_build_grids(PBVH *bvh, CCGElem **grids, DMGridAdjacency *gridadj,
                          int totgrid, CCGKey *key, void **gridfaces, DMFlagMat *flagmats, BLI_bitmap **grid_hidden)
{
	BBC *prim_bbc = NULL;
	BB cb;
	int gridsize = key->grid_size;
	int i, j;

	bvh->type = PBVH_GRIDS;
	bvh->grids = grids;
	bvh->gridadj = gridadj;
	bvh->gridfaces = gridfaces;
	bvh->grid_flag_mats = flagmats;
	bvh->totgrid = totgrid;
	bvh->gridkey = *key;
	bvh->grid_hidden = grid_hidden;
	bvh->leaf_limit = max_ii(LEAF_LIMIT / ((gridsize - 1) * (gridsize - 1)), 1);

	BB_reset(&cb);

	/* For each grid, store the AABB and the AABB centroid */
	prim_bbc = MEM_mallocN(sizeof(BBC) * totgrid, "prim_bbc");

	for (i = 0; i < totgrid; ++i) {
		CCGElem *grid = grids[i];
		BBC *bbc = prim_bbc + i;

		BB_reset((BB *)bbc);

		for (j = 0; j < gridsize * gridsize; ++j)
			BB_expand((BB *)bbc, CCG_elem_offset_co(key, grid, j));

		BBC_update_centroid(bbc);

		BB_expand(&cb, bbc->bcentroid);
	}

	if (totgrid)
		pbvh_build(bvh, &cb, prim_bbc, totgrid);

	MEM_freeN(prim_bbc);
}

PBVH *BKE_pbvh_new(void)
{
	PBVH *bvh = MEM_callocN(sizeof(PBVH), "pbvh");

	return bvh;
}

void BKE_pbvh_free(PBVH *bvh)
{
	PBVHNode *node;
	int i;

	for (i = 0; i < bvh->totnode; ++i) {
		node = &bvh->nodes[i];

		if (node->flag & PBVH_Leaf) {
			if (node->draw_buffers)
				GPU_free_pbvh_buffers(node->draw_buffers);
			if (node->vert_indices)
				MEM_freeN(node->vert_indices);
			if (node->face_vert_indices)
				MEM_freeN(node->face_vert_indices);
			BKE_pbvh_node_layer_disp_free(node);

			if (node->bm_faces)
				BLI_gset_free(node->bm_faces, NULL);
			if (node->bm_unique_verts)
				BLI_gset_free(node->bm_unique_verts, NULL);
			if (node->bm_other_verts)
				BLI_gset_free(node->bm_other_verts, NULL);
		}
	}

	if (bvh->deformed) {
		if (bvh->verts) {
			/* if pbvh was deformed, new memory was allocated for verts/faces -- free it */

			MEM_freeN(bvh->verts);
			if (bvh->faces)
				MEM_freeN(bvh->faces);
		}
	}

	if (bvh->nodes)
		MEM_freeN(bvh->nodes);

	if (bvh->prim_indices)
		MEM_freeN(bvh->prim_indices);

	MEM_freeN(bvh);
}

void BKE_pbvh_free_layer_disp(PBVH *bvh)
{
	int i;
	for (i = 0; i < bvh->totnode; ++i)
		BKE_pbvh_node_layer_disp_free(&bvh->nodes[i]);
}

static void pbvh_iter_begin(PBVHIter *iter, PBVH *bvh, BKE_pbvh_SearchCallback scb, void *search_data)
{
	iter->bvh = bvh;
	iter->scb = scb;
	iter->search_data = search_data;

	iter->stack = iter->stackfixed;
	iter->stackspace = STACK_FIXED_DEPTH;

	iter->stack[0].node = bvh->nodes;
	iter->stack[0].revisiting = 0;
	iter->stacksize = 1;
}

static void pbvh_iter_end(PBVHIter *iter)
{
	if (iter->stackspace > STACK_FIXED_DEPTH)
		MEM_freeN(iter->stack);
}

static void pbvh_stack_push(PBVHIter *iter, PBVHNode *node, int revisiting)
{
	if (iter->stacksize == iter->stackspace) {
		PBVHStack *newstack;

		iter->stackspace *= 2;
		newstack = MEM_callocN(sizeof(PBVHStack) * iter->stackspace, "PBVHStack");
		memcpy(newstack, iter->stack, sizeof(PBVHStack) * iter->stacksize);

		if (iter->stackspace > STACK_FIXED_DEPTH)
			MEM_freeN(iter->stack);
		iter->stack = newstack;
	}

	iter->stack[iter->stacksize].node = node;
	iter->stack[iter->stacksize].revisiting = revisiting;
	iter->stacksize++;
}

static PBVHNode *pbvh_iter_next(PBVHIter *iter)
{
	PBVHNode *node;
	int revisiting;

	/* purpose here is to traverse tree, visiting child nodes before their
	 * parents, this order is necessary for e.g. computing bounding boxes */

	while (iter->stacksize) {
		/* pop node */
		iter->stacksize--;
		node = iter->stack[iter->stacksize].node;

		/* on a mesh with no faces this can happen
		 * can remove this check if we know meshes have at least 1 face */
		if (node == NULL)
			return NULL;

		revisiting = iter->stack[iter->stacksize].revisiting;

		/* revisiting node already checked */
		if (revisiting)
			return node;

		if (iter->scb && !iter->scb(node, iter->search_data))
			continue;  /* don't traverse, outside of search zone */

		if (node->flag & PBVH_Leaf) {
			/* immediately hit leaf node */
			return node;
		}
		else {
			/* come back later when children are done */
			pbvh_stack_push(iter, node, 1);

			/* push two child nodes on the stack */
			pbvh_stack_push(iter, iter->bvh->nodes + node->children_offset + 1, 0);
			pbvh_stack_push(iter, iter->bvh->nodes + node->children_offset, 0);
		}
	}

	return NULL;
}

static PBVHNode *pbvh_iter_next_occluded(PBVHIter *iter)
{
	PBVHNode *node;

	while (iter->stacksize) {
		/* pop node */
		iter->stacksize--;
		node = iter->stack[iter->stacksize].node;

		/* on a mesh with no faces this can happen
		 * can remove this check if we know meshes have at least 1 face */
		if (node == NULL) return NULL;

		if (iter->scb && !iter->scb(node, iter->search_data)) continue;  /* don't traverse, outside of search zone */

		if (node->flag & PBVH_Leaf) {
			/* immediately hit leaf node */
			return node;
		}
		else {
			pbvh_stack_push(iter, iter->bvh->nodes + node->children_offset + 1, 0);
			pbvh_stack_push(iter, iter->bvh->nodes + node->children_offset, 0);
		}
	}

	return NULL;
}

void BKE_pbvh_search_gather(PBVH *bvh,
                            BKE_pbvh_SearchCallback scb, void *search_data,
                            PBVHNode ***r_array, int *r_tot)
{
	PBVHIter iter;
	PBVHNode **array = NULL, *node;
	int tot = 0, space = 0;

	pbvh_iter_begin(&iter, bvh, scb, search_data);

	while ((node = pbvh_iter_next(&iter))) {
		if (node->flag & PBVH_Leaf) {
			if (tot == space) {
				/* resize array if needed */
				space = (tot == 0) ? 32 : space * 2;
				array = MEM_recallocN_id(array, sizeof(PBVHNode *) * space, __func__);
			}

			array[tot] = node;
			tot++;
		}
	}

	pbvh_iter_end(&iter);

	if (tot == 0 && array) {
		MEM_freeN(array);
		array = NULL;
	}

	*r_array = array;
	*r_tot = tot;
}

void BKE_pbvh_search_callback(PBVH *bvh,
                              BKE_pbvh_SearchCallback scb, void *search_data,
                              BKE_pbvh_HitCallback hcb, void *hit_data)
{
	PBVHIter iter;
	PBVHNode *node;

	pbvh_iter_begin(&iter, bvh, scb, search_data);

	while ((node = pbvh_iter_next(&iter)))
		if (node->flag & PBVH_Leaf)
			hcb(node, hit_data);

	pbvh_iter_end(&iter);
}

typedef struct node_tree {
	PBVHNode *data;

	struct node_tree *left;
	struct node_tree *right;
} node_tree;

static void node_tree_insert(node_tree *tree, node_tree *new_node)
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

static void traverse_tree(node_tree *tree, BKE_pbvh_HitOccludedCallback hcb, void *hit_data, float *tmin)
{
	if (tree->left) traverse_tree(tree->left, hcb, hit_data, tmin);

	hcb(tree->data, hit_data, tmin);

	if (tree->right) traverse_tree(tree->right, hcb, hit_data, tmin);
}

static void free_tree(node_tree *tree)
{
	if (tree->left) {
		free_tree(tree->left);
		tree->left = NULL;
	}

	if (tree->right) {
		free_tree(tree->right);
		tree->right = NULL;
	}

	free(tree);
}

float BKE_pbvh_node_get_tmin(PBVHNode *node)
{
	return node->tmin;
}

static void BKE_pbvh_search_callback_occluded(PBVH *bvh,
                                              BKE_pbvh_SearchCallback scb, void *search_data,
                                              BKE_pbvh_HitOccludedCallback hcb, void *hit_data)
{
	PBVHIter iter;
	PBVHNode *node;
	node_tree *tree = NULL;

	pbvh_iter_begin(&iter, bvh, scb, search_data);

	while ((node = pbvh_iter_next_occluded(&iter))) {
		if (node->flag & PBVH_Leaf) {
			node_tree *new_node = malloc(sizeof(node_tree));

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

static bool update_search_cb(PBVHNode *node, void *data_v)
{
	int flag = GET_INT_FROM_POINTER(data_v);

	if (node->flag & PBVH_Leaf)
		return (node->flag & flag) != 0;

	return 1;
}

static void pbvh_update_normals(PBVH *bvh, PBVHNode **nodes,
                                int totnode, float (*face_nors)[3])
{
	float (*vnor)[3];
	int n;

	if (bvh->type == PBVH_BMESH) {
		pbvh_bmesh_normals_update(nodes, totnode);
		return;
	}

	if (bvh->type != PBVH_FACES)
		return;

	/* could be per node to save some memory, but also means
	 * we have to store for each vertex which node it is in */
	vnor = MEM_callocN(sizeof(float) * 3 * bvh->totvert, "bvh temp vnors");

	/* subtle assumptions:
	 * - We know that for all edited vertices, the nodes with faces
	 *   adjacent to these vertices have been marked with PBVH_UpdateNormals.
	 *   This is true because if the vertex is inside the brush radius, the
	 *   bounding box of it's adjacent faces will be as well.
	 * - However this is only true for the vertices that have actually been
	 *   edited, not for all vertices in the nodes marked for update, so we
	 *   can only update vertices marked with ME_VERT_PBVH_UPDATE.
	 */

#pragma omp parallel for private(n) schedule(static) if (totnode > PBVH_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHNode *node = nodes[n];

		if ((node->flag & PBVH_UpdateNormals)) {
			int i, j, totface, *faces;

			faces = node->prim_indices;
			totface = node->totprim;

			for (i = 0; i < totface; ++i) {
				MFace *f = bvh->faces + faces[i];
				float fn[3];
				unsigned int *fv = &f->v1;
				int sides = (f->v4) ? 4 : 3;

				if (f->v4)
					normal_quad_v3(fn, bvh->verts[f->v1].co, bvh->verts[f->v2].co,
					               bvh->verts[f->v3].co, bvh->verts[f->v4].co);
				else
					normal_tri_v3(fn, bvh->verts[f->v1].co, bvh->verts[f->v2].co,
					              bvh->verts[f->v3].co);

				for (j = 0; j < sides; ++j) {
					int v = fv[j];

					if (bvh->verts[v].flag & ME_VERT_PBVH_UPDATE) {
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

				if (face_nors)
					copy_v3_v3(face_nors[faces[i]], fn);
			}
		}
	}

#pragma omp parallel for private(n) schedule(static) if (totnode > PBVH_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHNode *node = nodes[n];

		if (node->flag & PBVH_UpdateNormals) {
			int i, *verts, totvert;

			verts = node->vert_indices;
			totvert = node->uniq_verts;

			for (i = 0; i < totvert; ++i) {
				const int v = verts[i];
				MVert *mvert = &bvh->verts[v];

				if (mvert->flag & ME_VERT_PBVH_UPDATE) {
					float no[3];

					copy_v3_v3(no, vnor[v]);
					normalize_v3(no);
					normal_float_to_short_v3(mvert->no, no);

					mvert->flag &= ~ME_VERT_PBVH_UPDATE;
				}
			}

			node->flag &= ~PBVH_UpdateNormals;
		}
	}

	MEM_freeN(vnor);
}

void pbvh_update_BB_redraw(PBVH *bvh, PBVHNode **nodes, int totnode, int flag)
{
	int n;

	/* update BB, redraw flag */
#pragma omp parallel for private(n) schedule(static) if (totnode > PBVH_OMP_LIMIT)
	for (n = 0; n < totnode; n++) {
		PBVHNode *node = nodes[n];

		if ((flag & PBVH_UpdateBB) && (node->flag & PBVH_UpdateBB))
			/* don't clear flag yet, leave it for flushing later */
			update_node_vb(bvh, node);

		if ((flag & PBVH_UpdateOriginalBB) && (node->flag & PBVH_UpdateOriginalBB))
			node->orig_vb = node->vb;

		if ((flag & PBVH_UpdateRedraw) && (node->flag & PBVH_UpdateRedraw))
			node->flag &= ~PBVH_UpdateRedraw;
	}
}

static void pbvh_update_draw_buffers(PBVH *bvh, PBVHNode **nodes, int totnode)
{
	PBVHNode *node;
	int n;

	/* can't be done in parallel with OpenGL */
	for (n = 0; n < totnode; n++) {
		node = nodes[n];

		if (node->flag & PBVH_RebuildDrawBuffers) {
			GPU_free_pbvh_buffers(node->draw_buffers);
			switch (bvh->type) {
				case PBVH_GRIDS:
					node->draw_buffers =
						GPU_build_grid_pbvh_buffers(node->prim_indices,
					                           node->totprim,
					                           bvh->grid_hidden,
					                           bvh->gridkey.grid_size);
					break;
				case PBVH_FACES:
					node->draw_buffers =
						GPU_build_mesh_pbvh_buffers(node->face_vert_indices,
					                           bvh->faces, bvh->verts,
					                           node->prim_indices,
					                           node->totprim);
					break;
				case PBVH_BMESH:
					node->draw_buffers =
						GPU_build_bmesh_pbvh_buffers(bvh->flags &
					                            PBVH_DYNTOPO_SMOOTH_SHADING);
					break;
			}
 
			node->flag &= ~PBVH_RebuildDrawBuffers;
		}

		if (node->flag & PBVH_UpdateDrawBuffers) {
			switch (bvh->type) {
				case PBVH_GRIDS:
					GPU_update_grid_pbvh_buffers(node->draw_buffers,
					                        bvh->grids,
					                        bvh->grid_flag_mats,
					                        node->prim_indices,
					                        node->totprim,
					                        &bvh->gridkey,
					                        bvh->show_diffuse_color);
					break;
				case PBVH_FACES:
					GPU_update_mesh_pbvh_buffers(node->draw_buffers,
					                        bvh->verts,
					                        node->vert_indices,
					                        node->uniq_verts +
					                        node->face_verts,
					                        CustomData_get_layer(bvh->vdata,
					                                             CD_PAINT_MASK),
					                        node->face_vert_indices,
					                        bvh->show_diffuse_color);
					break;
				case PBVH_BMESH:
					GPU_update_bmesh_pbvh_buffers(node->draw_buffers,
					                         bvh->bm,
					                         node->bm_faces,
					                         node->bm_unique_verts,
					                         node->bm_other_verts,
					                         bvh->show_diffuse_color);
					break;
			}

			node->flag &= ~PBVH_UpdateDrawBuffers;
		}
	}
}

static void pbvh_draw_BB(PBVH *bvh)
{
	PBVHNode *node;
	int a;

	GPU_init_draw_pbvh_BB();

	for (a = 0; a < bvh->totnode; a++) {
		node = &bvh->nodes[a];

		GPU_draw_pbvh_BB(node->vb.bmin, node->vb.bmax, ((node->flag & PBVH_Leaf) != 0));
	}

	GPU_end_draw_pbvh_BB();
}

static int pbvh_flush_bb(PBVH *bvh, PBVHNode *node, int flag)
{
	int update = 0;

	/* difficult to multithread well, we just do single threaded recursive */
	if (node->flag & PBVH_Leaf) {
		if (flag & PBVH_UpdateBB) {
			update |= (node->flag & PBVH_UpdateBB);
			node->flag &= ~PBVH_UpdateBB;
		}

		if (flag & PBVH_UpdateOriginalBB) {
			update |= (node->flag & PBVH_UpdateOriginalBB);
			node->flag &= ~PBVH_UpdateOriginalBB;
		}

		return update;
	}
	else {
		update |= pbvh_flush_bb(bvh, bvh->nodes + node->children_offset, flag);
		update |= pbvh_flush_bb(bvh, bvh->nodes + node->children_offset + 1, flag);

		if (update & PBVH_UpdateBB)
			update_node_vb(bvh, node);
		if (update & PBVH_UpdateOriginalBB)
			node->orig_vb = node->vb;
	}

	return update;
}

void BKE_pbvh_update(PBVH *bvh, int flag, float (*face_nors)[3])
{
	PBVHNode **nodes;
	int totnode;

	if (!bvh->nodes)
		return;

	BKE_pbvh_search_gather(bvh, update_search_cb, SET_INT_IN_POINTER(flag),
	                       &nodes, &totnode);

	if (flag & PBVH_UpdateNormals)
		pbvh_update_normals(bvh, nodes, totnode, face_nors);

	if (flag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateRedraw))
		pbvh_update_BB_redraw(bvh, nodes, totnode, flag);

	if (flag & (PBVH_UpdateBB | PBVH_UpdateOriginalBB))
		pbvh_flush_bb(bvh, bvh->nodes, flag);

	if (nodes) MEM_freeN(nodes);
}

void BKE_pbvh_redraw_BB(PBVH *bvh, float bb_min[3], float bb_max[3])
{
	PBVHIter iter;
	PBVHNode *node;
	BB bb;

	BB_reset(&bb);

	pbvh_iter_begin(&iter, bvh, NULL, NULL);

	while ((node = pbvh_iter_next(&iter)))
		if (node->flag & PBVH_UpdateRedraw)
			BB_expand_with_bb(&bb, &node->vb);

	pbvh_iter_end(&iter);

	copy_v3_v3(bb_min, bb.bmin);
	copy_v3_v3(bb_max, bb.bmax);
}

void BKE_pbvh_get_grid_updates(PBVH *bvh, int clear, void ***r_gridfaces, int *r_totface)
{
	PBVHIter iter;
	PBVHNode *node;
	GSetIterator gs_iter;
	GSet *face_set;
	void *face, **faces;
	unsigned i;
	int tot;

	face_set = BLI_gset_ptr_new(__func__);

	pbvh_iter_begin(&iter, bvh, NULL, NULL);

	while ((node = pbvh_iter_next(&iter))) {
		if (node->flag & PBVH_UpdateNormals) {
			for (i = 0; i < node->totprim; ++i) {
				face = bvh->gridfaces[node->prim_indices[i]];
				if (!BLI_gset_haskey(face_set, face))
					BLI_gset_insert(face_set, face);
			}

			if (clear)
				node->flag &= ~PBVH_UpdateNormals;
		}
	}

	pbvh_iter_end(&iter);
	
	tot = BLI_gset_size(face_set);
	if (tot == 0) {
		*r_totface = 0;
		*r_gridfaces = NULL;
		BLI_gset_free(face_set, NULL);
		return;
	}

	faces = MEM_mallocN(sizeof(*faces) * tot, "PBVH Grid Faces");

	GSET_ITER_INDEX (gs_iter, face_set, i) {
		faces[i] = BLI_gsetIterator_getKey(&gs_iter);
	}

	BLI_gset_free(face_set, NULL);

	*r_totface = tot;
	*r_gridfaces = faces;
}

/***************************** PBVH Access ***********************************/

PBVHType BKE_pbvh_type(const PBVH *bvh)
{
	return bvh->type;
}

void BKE_pbvh_bounding_box(const PBVH *bvh, float min[3], float max[3])
{
	if (bvh->totnode) {
		const BB *bb = &bvh->nodes[0].vb;
		copy_v3_v3(min, bb->bmin);
		copy_v3_v3(max, bb->bmax);
	}
	else {
		zero_v3(min);
		zero_v3(max);
	}
}

BLI_bitmap **BKE_pbvh_grid_hidden(const PBVH *bvh)
{
	BLI_assert(bvh->type == PBVH_GRIDS);
	return bvh->grid_hidden;
}

void BKE_pbvh_get_grid_key(const PBVH *bvh, CCGKey *key)
{
	BLI_assert(bvh->type == PBVH_GRIDS);
	*key = bvh->gridkey;
}

BMesh *BKE_pbvh_get_bmesh(PBVH *bvh)
{
	BLI_assert(bvh->type == PBVH_BMESH);
	return bvh->bm;
}

/***************************** Node Access ***********************************/

void BKE_pbvh_node_mark_update(PBVHNode *node)
{
	node->flag |= PBVH_UpdateNormals | PBVH_UpdateBB | PBVH_UpdateOriginalBB | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_rebuild_draw(PBVHNode *node)
{
	node->flag |= PBVH_RebuildDrawBuffers | PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_redraw(PBVHNode *node)
{
	node->flag |= PBVH_UpdateDrawBuffers | PBVH_UpdateRedraw;
}

void BKE_pbvh_node_mark_normals_update(PBVHNode *node)
{
	node->flag |= PBVH_UpdateNormals;
}


void BKE_pbvh_node_fully_hidden_set(PBVHNode *node, int fully_hidden)
{
	BLI_assert(node->flag & PBVH_Leaf);
	
	if (fully_hidden)
		node->flag |= PBVH_FullyHidden;
	else
		node->flag &= ~PBVH_FullyHidden;
}

void BKE_pbvh_node_get_verts(PBVH *bvh, PBVHNode *node, int **vert_indices, MVert **verts)
{
	if (vert_indices) *vert_indices = node->vert_indices;
	if (verts) *verts = bvh->verts;
}

void BKE_pbvh_node_num_verts(PBVH *bvh, PBVHNode *node, int *uniquevert, int *totvert)
{
	int tot;
	
	switch (bvh->type) {
		case PBVH_GRIDS:
			tot = node->totprim * bvh->gridkey.grid_area;
			if (totvert) *totvert = tot;
			if (uniquevert) *uniquevert = tot;
			break;
		case PBVH_FACES:
			if (totvert) *totvert = node->uniq_verts + node->face_verts;
			if (uniquevert) *uniquevert = node->uniq_verts;
			break;
		case PBVH_BMESH:
			tot = BLI_gset_size(node->bm_unique_verts);
			if (totvert) *totvert = tot + BLI_gset_size(node->bm_other_verts);
			if (uniquevert) *uniquevert = tot;
			break;
	}
}

void BKE_pbvh_node_get_grids(PBVH *bvh, PBVHNode *node, int **grid_indices, int *totgrid, int *maxgrid, int *gridsize, CCGElem ***griddata, DMGridAdjacency **gridadj)
{
	switch (bvh->type) {
		case PBVH_GRIDS:
			if (grid_indices) *grid_indices = node->prim_indices;
			if (totgrid) *totgrid = node->totprim;
			if (maxgrid) *maxgrid = bvh->totgrid;
			if (gridsize) *gridsize = bvh->gridkey.grid_size;
			if (griddata) *griddata = bvh->grids;
			if (gridadj) *gridadj = bvh->gridadj;
			break;
		case PBVH_FACES:
		case PBVH_BMESH:
			if (grid_indices) *grid_indices = NULL;
			if (totgrid) *totgrid = 0;
			if (maxgrid) *maxgrid = 0;
			if (gridsize) *gridsize = 0;
			if (griddata) *griddata = NULL;
			if (gridadj) *gridadj = NULL;
			break;
	}
}

void BKE_pbvh_node_get_BB(PBVHNode *node, float bb_min[3], float bb_max[3])
{
	copy_v3_v3(bb_min, node->vb.bmin);
	copy_v3_v3(bb_max, node->vb.bmax);
}

void BKE_pbvh_node_get_original_BB(PBVHNode *node, float bb_min[3], float bb_max[3])
{
	copy_v3_v3(bb_min, node->orig_vb.bmin);
	copy_v3_v3(bb_max, node->orig_vb.bmax);
}

void BKE_pbvh_node_get_proxies(PBVHNode *node, PBVHProxyNode **proxies, int *proxy_count)
{
	if (node->proxy_count > 0) {
		if (proxies) *proxies = node->proxies;
		if (proxy_count) *proxy_count = node->proxy_count;
	}
	else {
		if (proxies) *proxies = NULL;
		if (proxy_count) *proxy_count = 0;
	}
}

/********************************* Raycast ***********************************/

typedef struct {
	IsectRayAABBData ray;
	int original;
} RaycastData;

static bool ray_aabb_intersect(PBVHNode *node, void *data_v)
{
	RaycastData *rcd = data_v;
	float bb_min[3], bb_max[3];

	if (rcd->original)
		BKE_pbvh_node_get_original_BB(node, bb_min, bb_max);
	else
		BKE_pbvh_node_get_BB(node, bb_min, bb_max);

	return isect_ray_aabb(&rcd->ray, bb_min, bb_max, &node->tmin);
}

void BKE_pbvh_raycast(PBVH *bvh, BKE_pbvh_HitOccludedCallback cb, void *data,
                      const float ray_start[3], const float ray_normal[3],
                      int original)
{
	RaycastData rcd;

	isect_ray_aabb_initialize(&rcd.ray, ray_start, ray_normal);
	rcd.original = original;

	BKE_pbvh_search_callback_occluded(bvh, ray_aabb_intersect, &rcd, cb, data);
}

bool ray_face_intersection(const float ray_start[3],
                           const float ray_normal[3],
                           const float t0[3], const float t1[3],
                           const float t2[3], const float t3[3],
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

static bool pbvh_faces_node_raycast(PBVH *bvh, const PBVHNode *node,
                                    float (*origco)[3],
                                    const float ray_start[3],
                                    const float ray_normal[3], float *dist)
{
	const MVert *vert = bvh->verts;
	const int *faces = node->prim_indices;
	int i, totface = node->totprim;
	bool hit = false;

	for (i = 0; i < totface; ++i) {
		const MFace *f = bvh->faces + faces[i];
		const int *face_verts = node->face_vert_indices[i];

		if (paint_is_face_hidden(f, vert))
			continue;

		if (origco) {
			/* intersect with backuped original coordinates */
			hit |= ray_face_intersection(ray_start, ray_normal,
			                             origco[face_verts[0]],
			                             origco[face_verts[1]],
			                             origco[face_verts[2]],
			                             f->v4 ? origco[face_verts[3]] : NULL,
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

	return hit;
}

static bool pbvh_grids_node_raycast(
        PBVH *bvh, PBVHNode *node,
        float (*origco)[3],
        const float ray_start[3], const float ray_normal[3],
        float *dist)
{
	int totgrid = node->totprim;
	int gridsize = bvh->gridkey.grid_size;
	int i, x, y;
	bool hit = false;

	for (i = 0; i < totgrid; ++i) {
		CCGElem *grid = bvh->grids[node->prim_indices[i]];
		BLI_bitmap *gh;

		if (!grid)
			continue;

		gh = bvh->grid_hidden[node->prim_indices[i]];

		for (y = 0; y < gridsize - 1; ++y) {
			for (x = 0; x < gridsize - 1; ++x) {
				/* check if grid face is hidden */
				if (gh) {
					if (paint_is_grid_face_hidden(gh, gridsize, x, y))
						continue;
				}

				if (origco) {
					hit |= ray_face_intersection(ray_start, ray_normal,
					                             origco[y * gridsize + x],
					                             origco[y * gridsize + x + 1],
					                             origco[(y + 1) * gridsize + x + 1],
					                             origco[(y + 1) * gridsize + x],
					                             dist);
				}
				else {
					hit |= ray_face_intersection(ray_start, ray_normal,
					                             CCG_grid_elem_co(&bvh->gridkey, grid, x, y),
					                             CCG_grid_elem_co(&bvh->gridkey, grid, x + 1, y),
					                             CCG_grid_elem_co(&bvh->gridkey, grid, x + 1, y + 1),
					                             CCG_grid_elem_co(&bvh->gridkey, grid, x, y + 1),
					                             dist);
				}
			}
		}

		if (origco)
			origco += gridsize * gridsize;
	}

	return hit;
}

bool BKE_pbvh_node_raycast(
        PBVH *bvh, PBVHNode *node, float (*origco)[3], int use_origco,
        const float ray_start[3], const float ray_normal[3],
        float *dist)
{
	bool hit = false;

	if (node->flag & PBVH_FullyHidden)
		return 0;

	switch (bvh->type) {
		case PBVH_FACES:
			hit |= pbvh_faces_node_raycast(bvh, node, origco,
			                               ray_start, ray_normal, dist);
			break;
		case PBVH_GRIDS:
			hit |= pbvh_grids_node_raycast(bvh, node, origco,
			                               ray_start, ray_normal, dist);
			break;
		case PBVH_BMESH:
			hit = pbvh_bmesh_node_raycast(node, ray_start, ray_normal, dist, use_origco);
			break;
	}

	return hit;
}

void BKE_pbvh_raycast_project_ray_root (PBVH *bvh, bool original, float ray_start[3], float ray_end[3], float ray_normal[3])
{
	if (bvh->nodes) {
		float rootmin_start, rootmin_end;
		float bb_min_root[3], bb_max_root[3], bb_center[3], bb_diff[3];
		IsectRayAABBData ray;
		float ray_normal_inv[3];
		float offset = 1.0f + 1e-3f;
		float offset_vec[3] = {1e-3f, 1e-3f, 1e-3f};

		if (original)
			BKE_pbvh_node_get_original_BB(bvh->nodes, bb_min_root, bb_max_root);
		else
			BKE_pbvh_node_get_BB(bvh->nodes, bb_min_root, bb_max_root);

		/* slightly offset min and max in case we have a zero width node (due to a plane mesh for instance),
		 * or faces very close to the bounding box boundary. */
		mid_v3_v3v3(bb_center, bb_max_root, bb_min_root);
		/* diff should be same for both min/max since it's calculated from center */
		sub_v3_v3v3(bb_diff, bb_max_root, bb_center);
		/* handles case of zero width bb */
		add_v3_v3(bb_diff, offset_vec);
		madd_v3_v3v3fl(bb_max_root, bb_center, bb_diff, offset);
		madd_v3_v3v3fl(bb_min_root, bb_center, bb_diff, -offset);

		/* first project start ray */
		isect_ray_aabb_initialize(&ray, ray_start, ray_normal);
		if (!isect_ray_aabb(&ray, bb_min_root, bb_max_root, &rootmin_start))
			return;

		/* then the end ray */
		mul_v3_v3fl(ray_normal_inv, ray_normal, -1.0);
		isect_ray_aabb_initialize(&ray, ray_end, ray_normal_inv);
		/* unlikely to fail exiting if entering succeeded, still keep this here */
		if (!isect_ray_aabb(&ray, bb_min_root, bb_max_root, &rootmin_end))
			return;

		madd_v3_v3v3fl(ray_start, ray_start, ray_normal, rootmin_start);
		madd_v3_v3v3fl(ray_end, ray_end, ray_normal_inv, rootmin_end);
	}
}


//#include "GPU_glew.h"

typedef struct {
	DMSetMaterial setMaterial;
	bool wireframe;
} PBVHNodeDrawData;

void BKE_pbvh_node_draw(PBVHNode *node, void *data_v)
{
	PBVHNodeDrawData *data = data_v;

#if 0
	/* XXX: Just some quick code to show leaf nodes in different colors */
	float col[3]; int i;

	if (0) { //is_partial) {
		col[0] = (rand() / (float)RAND_MAX); col[1] = col[2] = 0.6;
	}
	else {
		srand((long long)node);
		for (i = 0; i < 3; ++i)
			col[i] = (rand() / (float)RAND_MAX) * 0.3 + 0.7;
	}
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, col);

	glColor3f(1, 0, 0);
#endif

	if (!(node->flag & PBVH_FullyHidden)) {
		GPU_draw_pbvh_buffers(node->draw_buffers,
		                 data->setMaterial,
		                 data->wireframe);
	}
}

typedef enum {
	ISECT_INSIDE,
	ISECT_OUTSIDE,
	ISECT_INTERSECT
} PlaneAABBIsect;

/* Adapted from:
 * http://www.gamedev.net/community/forums/topic.asp?topic_id=512123
 * Returns true if the AABB is at least partially within the frustum
 * (ok, not a real frustum), false otherwise.
 */
static PlaneAABBIsect test_planes_aabb(const float bb_min[3],
                                       const float bb_max[3],
                                       const float (*planes)[4])
{
	float vmin[3], vmax[3];
	PlaneAABBIsect ret = ISECT_INSIDE;
	int i, axis;
	
	for (i = 0; i < 4; ++i) {
		for (axis = 0; axis < 3; ++axis) {
			if (planes[i][axis] > 0) {
				vmin[axis] = bb_min[axis];
				vmax[axis] = bb_max[axis];
			}
			else {
				vmin[axis] = bb_max[axis];
				vmax[axis] = bb_min[axis];
			}
		}
		
		if (dot_v3v3(planes[i], vmin) + planes[i][3] > 0)
			return ISECT_OUTSIDE;
		else if (dot_v3v3(planes[i], vmax) + planes[i][3] >= 0)
			ret = ISECT_INTERSECT;
	}

	return ret;
}

bool BKE_pbvh_node_planes_contain_AABB(PBVHNode *node, void *data)
{
	float bb_min[3], bb_max[3];
	
	BKE_pbvh_node_get_BB(node, bb_min, bb_max);
	return test_planes_aabb(bb_min, bb_max, data) != ISECT_OUTSIDE;
}

bool BKE_pbvh_node_planes_exclude_AABB(PBVHNode *node, void *data)
{
	float bb_min[3], bb_max[3];
	
	BKE_pbvh_node_get_BB(node, bb_min, bb_max);
	return test_planes_aabb(bb_min, bb_max, data) != ISECT_INSIDE;
}

static void pbvh_node_check_diffuse_changed(PBVH *bvh, PBVHNode *node)
{
	if (!node->draw_buffers)
		return;

	if (GPU_pbvh_buffers_diffuse_changed(node->draw_buffers, node->bm_faces, bvh->show_diffuse_color))
		node->flag |= PBVH_UpdateDrawBuffers;
}

void BKE_pbvh_draw(PBVH *bvh, float (*planes)[4], float (*face_nors)[3],
                   DMSetMaterial setMaterial, bool wireframe)
{
	PBVHNodeDrawData draw_data = {setMaterial, wireframe};
	PBVHNode **nodes;
	int a, totnode;

	for (a = 0; a < bvh->totnode; a++)
		pbvh_node_check_diffuse_changed(bvh, &bvh->nodes[a]);

	BKE_pbvh_search_gather(bvh, update_search_cb, SET_INT_IN_POINTER(PBVH_UpdateNormals | PBVH_UpdateDrawBuffers),
	                       &nodes, &totnode);

	pbvh_update_normals(bvh, nodes, totnode, face_nors);
	pbvh_update_draw_buffers(bvh, nodes, totnode);

	if (nodes) MEM_freeN(nodes);

	if (planes) {
		BKE_pbvh_search_callback(bvh, BKE_pbvh_node_planes_contain_AABB,
		                         planes, BKE_pbvh_node_draw, &draw_data);
	}
	else {
		BKE_pbvh_search_callback(bvh, NULL, NULL, BKE_pbvh_node_draw, &draw_data);
	}

	if (G.debug_value == 14)
		pbvh_draw_BB(bvh);
}

void BKE_pbvh_grids_update(PBVH *bvh, CCGElem **grids, DMGridAdjacency *gridadj, void **gridfaces,
                           DMFlagMat *flagmats, BLI_bitmap **grid_hidden)
{
	int a;

	bvh->grids = grids;
	bvh->gridadj = gridadj;
	bvh->gridfaces = gridfaces;

	if (flagmats != bvh->grid_flag_mats || bvh->grid_hidden != grid_hidden) {
		bvh->grid_flag_mats = flagmats;
		bvh->grid_hidden = grid_hidden;

		for (a = 0; a < bvh->totnode; ++a)
			BKE_pbvh_node_mark_rebuild_draw(&bvh->nodes[a]);
	}
}

/* Get the node's displacement layer, creating it if necessary */
float *BKE_pbvh_node_layer_disp_get(PBVH *bvh, PBVHNode *node)
{
	if (!node->layer_disp) {
		int totvert = 0;
		BKE_pbvh_node_num_verts(bvh, node, &totvert, NULL);
		node->layer_disp = MEM_callocN(sizeof(float) * totvert, "layer disp");
	}
	return node->layer_disp;
}

/* If the node has a displacement layer, free it and set to null */
void BKE_pbvh_node_layer_disp_free(PBVHNode *node)
{
	if (node->layer_disp) {
		MEM_freeN(node->layer_disp);
		node->layer_disp = NULL;
	}
}

float (*BKE_pbvh_get_vertCos(PBVH *pbvh))[3]
{
	int a;
	float (*vertCos)[3] = NULL;

	if (pbvh->verts) {
		float *co;
		MVert *mvert = pbvh->verts;

		vertCos = MEM_callocN(3 * pbvh->totvert * sizeof(float), "BKE_pbvh_get_vertCoords");
		co = (float *)vertCos;

		for (a = 0; a < pbvh->totvert; a++, mvert++, co += 3) {
			copy_v3_v3(co, mvert->co);
		}
	}

	return vertCos;
}

void BKE_pbvh_apply_vertCos(PBVH *pbvh, float (*vertCos)[3])
{
	int a;

	if (!pbvh->deformed) {
		if (pbvh->verts) {
			/* if pbvh is not already deformed, verts/faces points to the */
			/* original data and applying new coords to this arrays would lead to */
			/* unneeded deformation -- duplicate verts/faces to avoid this */

			pbvh->verts = MEM_dupallocN(pbvh->verts);
			pbvh->faces = MEM_dupallocN(pbvh->faces);

			pbvh->deformed = 1;
		}
	}

	if (pbvh->verts) {
		MVert *mvert = pbvh->verts;
		/* copy new verts coords */
		for (a = 0; a < pbvh->totvert; ++a, ++mvert) {
			copy_v3_v3(mvert->co, vertCos[a]);
			mvert->flag |= ME_VERT_PBVH_UPDATE;
		}

		/* coordinates are new -- normals should also be updated */
		BKE_mesh_calc_normals_tessface(pbvh->verts, pbvh->totvert, pbvh->faces, pbvh->totprim, NULL);

		for (a = 0; a < pbvh->totnode; ++a)
			BKE_pbvh_node_mark_update(&pbvh->nodes[a]);

		BKE_pbvh_update(pbvh, PBVH_UpdateBB, NULL);
		BKE_pbvh_update(pbvh, PBVH_UpdateOriginalBB, NULL);

	}
}

bool BKE_pbvh_isDeformed(PBVH *pbvh)
{
	return pbvh->deformed;
}
/* Proxies */

PBVHProxyNode *BKE_pbvh_node_add_proxy(PBVH *bvh, PBVHNode *node)
{
	int index, totverts;

#pragma omp critical
	{

		index = node->proxy_count;

		node->proxy_count++;

		if (node->proxies)
			node->proxies = MEM_reallocN(node->proxies, node->proxy_count * sizeof(PBVHProxyNode));
		else
			node->proxies = MEM_mallocN(sizeof(PBVHProxyNode), "PBVHNodeProxy");

		BKE_pbvh_node_num_verts(bvh, node, &totverts, NULL);
		node->proxies[index].co = MEM_callocN(sizeof(float[3]) * totverts, "PBVHNodeProxy.co");
	}

	return node->proxies + index;
}

void BKE_pbvh_node_free_proxies(PBVHNode *node)
{
#pragma omp critical
	{
		int p;

		for (p = 0; p < node->proxy_count; p++) {
			MEM_freeN(node->proxies[p].co);
			node->proxies[p].co = NULL;
		}

		MEM_freeN(node->proxies);
		node->proxies = NULL;

		node->proxy_count = 0;
	}
}

void BKE_pbvh_gather_proxies(PBVH *pbvh, PBVHNode ***r_array,  int *r_tot)
{
	PBVHNode **array = NULL, *node;
	int tot = 0, space = 0;
	int n;

	for (n = 0; n < pbvh->totnode; n++) {
		node = pbvh->nodes + n;

		if (node->proxy_count > 0) {
			if (tot == space) {
				/* resize array if needed */
				space = (tot == 0) ? 32 : space * 2;
				array = MEM_recallocN_id(array, sizeof(PBVHNode *) * space, __func__);
			}

			array[tot] = node;
			tot++;
		}
	}

	if (tot == 0 && array) {
		MEM_freeN(array);
		array = NULL;
	}

	*r_array = array;
	*r_tot = tot;
}

void pbvh_vertex_iter_init(PBVH *bvh, PBVHNode *node,
                           PBVHVertexIter *vi, int mode)
{
	struct CCGElem **grids;
	struct MVert *verts;
	int *grid_indices, *vert_indices;
	int totgrid, gridsize, uniq_verts, totvert;
	
	vi->grid = NULL;
	vi->no = NULL;
	vi->fno = NULL;
	vi->mvert = NULL;
	
	BKE_pbvh_node_get_grids(bvh, node, &grid_indices, &totgrid, NULL, &gridsize, &grids, NULL);
	BKE_pbvh_node_num_verts(bvh, node, &uniq_verts, &totvert);
	BKE_pbvh_node_get_verts(bvh, node, &vert_indices, &verts);
	vi->key = &bvh->gridkey;
	
	vi->grids = grids;
	vi->grid_indices = grid_indices;
	vi->totgrid = (grids) ? totgrid : 1;
	vi->gridsize = gridsize;
	
	if (mode == PBVH_ITER_ALL)
		vi->totvert = totvert;
	else
		vi->totvert = uniq_verts;
	vi->vert_indices = vert_indices;
	vi->mverts = verts;

	if (bvh->type == PBVH_BMESH) {
		BLI_gsetIterator_init(&vi->bm_unique_verts, node->bm_unique_verts);
		BLI_gsetIterator_init(&vi->bm_other_verts, node->bm_other_verts);
		vi->bm_vdata = &bvh->bm->vdata;
		vi->cd_vert_mask_offset = CustomData_get_offset(vi->bm_vdata, CD_PAINT_MASK);
	}

	vi->gh = NULL;
	if (vi->grids && mode == PBVH_ITER_UNIQUE)
		vi->grid_hidden = bvh->grid_hidden;

	vi->mask = NULL;
	if (bvh->type == PBVH_FACES)
		vi->vmask = CustomData_get_layer(bvh->vdata, CD_PAINT_MASK);
}

void pbvh_show_diffuse_color_set(PBVH *bvh, bool show_diffuse_color)
{
	bool has_mask = false;

	switch (bvh->type) {
		case PBVH_GRIDS:
			has_mask = (bvh->gridkey.has_mask != 0);
			break;
		case PBVH_FACES:
			has_mask = (bvh->vdata && CustomData_get_layer(bvh->vdata,
			                                CD_PAINT_MASK));
			break;
		case PBVH_BMESH:
			has_mask = (bvh->bm && (CustomData_get_offset(&bvh->bm->vdata, CD_PAINT_MASK) != -1));
			break;
	}

	bvh->show_diffuse_color = !has_mask || show_diffuse_color;
}
