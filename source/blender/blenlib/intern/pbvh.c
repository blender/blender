#include "MEM_guardedalloc.h"

#include "DNA_meshdata_types.h"

#include "BLI_arithb.h"
#include "BLI_ghash.h"
#include "BLI_pbvh.h"

#include "BKE_utildefines.h"

#include "gpu_buffers.h"

#include <float.h>
#include <stdlib.h>
#include <string.h>

#define LEAF_LIMIT 10000

//#define PERFCNTRS

/* Bitmap */
typedef char* BLI_bitmap;

BLI_bitmap BLI_bitmap_new(int tot)
{
	return MEM_callocN((tot >> 3) + 1, "BLI bitmap");
}

int BLI_bitmap_get(BLI_bitmap b, int index)
{
	return b[index >> 3] & (1 << (index & 7));
}

void BLI_bitmap_set(BLI_bitmap b, int index)
{
	b[index >> 3] |= (1 << (index & 7));
}

void BLI_bitmap_clear(BLI_bitmap b, int index)
{
	b[index >> 3] &= ~(1 << (index & 7));
}

typedef enum {
	PBVH_Leaf = 1,
	PBVH_Modified = 2
} NodeFlags;

/* Axis-aligned bounding box */
typedef struct {
	float bmin[3], bmax[3];
} BB;

/* Axis-aligned bounding box with centroid */
typedef struct {
	float bmin[3], bmax[3], bcentroid[3];
} BBC;

typedef struct {
	/* Opaque handle for drawing code */
	void *draw_buffers;

	int *vert_indices;

	/* Voxel bounds */
	BB vb;

	/* For internal nodes */
	int children_offset;

	/* Range of faces used in the node */
	int face_offset;

	unsigned short totface;
	unsigned short uniq_verts, face_verts;

	char flag;
} Node;

typedef struct PBVH {
	Node *nodes;
	int node_mem_count, totnode;

	int *face_indices;
	int totface;

	BB modified_bb;

	BLI_pbvh_HitCallback update_cb;
	void *update_cb_data;

	/* Mesh data */
	MVert *verts;
	MFace *faces;

	int modified_lock;

	/* Only used during BVH build and update,
	   don't need to remain valid after */
	BLI_bitmap vert_bitmap;

#ifdef PERFCNTRS
	int perf_modified;
#endif
} PBVH;

static void BB_reset(BB *bb)
{
	bb->bmin[0] = bb->bmin[1] = bb->bmin[2] = FLT_MAX;
	bb->bmax[0] = bb->bmax[1] = bb->bmax[2] = -FLT_MAX;
}

/* Expand the bounding box to include a new coordinate */
static void BB_expand(BB *bb, float co[3])
{
	if(co[0] < bb->bmin[0]) bb->bmin[0] = co[0];
	if(co[1] < bb->bmin[1]) bb->bmin[1] = co[1];
	if(co[2] < bb->bmin[2]) bb->bmin[2] = co[2];

	if(co[0] > bb->bmax[0]) bb->bmax[0] = co[0];
	if(co[1] > bb->bmax[1]) bb->bmax[1] = co[1];
	if(co[2] > bb->bmax[2]) bb->bmax[2] = co[2];
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
static void update_node_vb(PBVH *bvh, Node *node)
{
	BB_reset(&node->vb);
	
	if(node->flag & PBVH_Leaf) {
		int i, j;
		for(i = node->face_offset + node->totface - 1;
		    i >= node->face_offset; --i) {
			MFace *f = bvh->faces + bvh->face_indices[i];
			const int sides = f->v4 ? 4 : 3;

			for(j = 0; j < sides; ++j)
				BB_expand(&node->vb,
					  bvh->verts[*((&f->v1) + j)].co);
		}
	}
	else {
		BB_expand_with_bb(&node->vb,
				  &bvh->nodes[node->children_offset].vb);
		BB_expand_with_bb(&node->vb,
				  &bvh->nodes[node->children_offset + 1].vb);
	}
}

/* Adapted from BLI_kdopbvh.c */
/* Returns the index of the first element on the right of the partition */
static int partition_indices(int *face_indices, int lo, int hi, int axis,
			     float mid, BBC *prim_bbc)
{
	int i=lo, j=hi;
	for(;;) {
		for(; prim_bbc[face_indices[i]].bcentroid[axis] < mid; i++);
		for(; mid < prim_bbc[face_indices[j]].bcentroid[axis]; j--);
		
		if(!(i < j))
			return i;
		
		SWAP(int, face_indices[i], face_indices[j]);
		i++;
	}
}

void check_partitioning(int *face_indices, int lo, int hi, int axis,
			       float mid, BBC *prim_bbc, int index_of_2nd_partition)
{
	int i;
	for(i = lo; i <= hi; ++i) {
		const float c = prim_bbc[face_indices[i]].bcentroid[axis];

		if((i < index_of_2nd_partition && c > mid) ||
		   (i > index_of_2nd_partition && c < mid)) {
			printf("fail\n");
		}
	}
}

static void grow_nodes(PBVH *bvh, int totnode)
{
	if(totnode > bvh->node_mem_count) {
		Node *prev = bvh->nodes;
		bvh->node_mem_count *= 1.33;
		if(bvh->node_mem_count < totnode)
			bvh->node_mem_count = totnode;
		bvh->nodes = MEM_callocN(sizeof(Node) * bvh->node_mem_count,
					 "bvh nodes");
		memcpy(bvh->nodes, prev, bvh->totnode * sizeof(Node));
		MEM_freeN(prev);
	}

	bvh->totnode = totnode;
}

/* Add a vertex to the map, with a positive value for unique vertices and
   a negative value for additional vertices */
static void map_insert_vert(PBVH *bvh, GHash *map,
			    unsigned short *face_verts,
			    unsigned short *uniq_verts, int vertex)
{
	void *value, *key = SET_INT_IN_POINTER(vertex);

	if(!BLI_ghash_haskey(map, key)) {
		if(BLI_bitmap_get(bvh->vert_bitmap, vertex)) {
			value = SET_INT_IN_POINTER(-(*face_verts) - 1);
			++(*face_verts);
		}
		else {
			BLI_bitmap_set(bvh->vert_bitmap, vertex);
			value = SET_INT_IN_POINTER(*uniq_verts);
			++(*uniq_verts);
		}
		
		BLI_ghash_insert(map, key, value);
	}
}

/* Find vertices used by the faces in this node and update the draw buffers */
static void build_leaf_node(PBVH *bvh, Node *node)
{
	GHashIterator *iter;
	GHash *map;
	int i, j;

	map = BLI_ghash_new(BLI_ghashutil_inthash, BLI_ghashutil_intcmp);
	
	node->uniq_verts = node->face_verts = 0;

	for(i = node->face_offset + node->totface - 1;
	    i >= node->face_offset; --i) {
		MFace *f = bvh->faces + bvh->face_indices[i];
		int sides = f->v4 ? 4 : 3;

		for(j = 0; j < sides; ++j) {
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

	node->draw_buffers =
		GPU_build_buffers(map, bvh->verts, bvh->faces,
				  bvh->face_indices + node->face_offset,
				  node->totface, node->vert_indices,
				  node->uniq_verts,
				  node->uniq_verts + node->face_verts);

	BLI_ghash_free(map, NULL, NULL);
}

/* Recursively build a node in the tree

   vb is the voxel box around all of the primitives contained in
   this node.

   cb is the bounding box around all the centroids of the primitives
   contained in this node

   offset and start indicate a range in the array of primitive indices
*/

void build_sub(PBVH *bvh, int node_index, BB *cb, BBC *prim_bbc,
	       int offset, int count)
{
	int i, axis, end;
	BB cb_backing;

	/* Decide whether this is a leaf or not */
	if(count <= LEAF_LIMIT) {
		bvh->nodes[node_index].flag |= PBVH_Leaf;

		bvh->nodes[node_index].face_offset = offset;
		bvh->nodes[node_index].totface = count;

		/* Still need vb for searches */
		BB_reset(&bvh->nodes[node_index].vb);
		for(i = offset + count - 1; i >= offset; --i) {
			BB_expand_with_bb(&bvh->nodes[node_index].vb,
					  (BB*)(prim_bbc +
						bvh->face_indices[i]));
		}
		
		build_leaf_node(bvh, bvh->nodes + node_index);

		/* Done with this subtree */
		return;
	}
	else {
		BB_reset(&bvh->nodes[node_index].vb);
		bvh->nodes[node_index].children_offset = bvh->totnode;
		grow_nodes(bvh, bvh->totnode + 2);

		if(!cb) {
			cb = &cb_backing;
			BB_reset(cb);
			for(i = offset + count - 1; i >= offset; --i)
				BB_expand(cb, prim_bbc[bvh->face_indices[i]].bcentroid);
		}
	}

	axis = BB_widest_axis(cb);

	for(i = offset + count - 1; i >= offset; --i) {
		BB_expand_with_bb(&bvh->nodes[node_index].vb,
				  (BB*)(prim_bbc + bvh->face_indices[i]));
	}

	end = partition_indices(bvh->face_indices, offset, offset + count - 1,
				axis,
				(cb->bmax[axis] + cb->bmin[axis]) * 0.5f,
				prim_bbc);
	check_partitioning(bvh->face_indices, offset, offset + count - 1,
			   axis,
			   (cb->bmax[axis] + cb->bmin[axis]) * 0.5f,
			   prim_bbc, end);

	build_sub(bvh, bvh->nodes[node_index].children_offset, NULL,
		  prim_bbc, offset, end - offset);
	build_sub(bvh, bvh->nodes[node_index].children_offset + 1, NULL,
		  prim_bbc, end, offset + count - end);
}

/* Do a full rebuild */
void BLI_pbvh_build(PBVH *bvh, MFace *faces, MVert *verts, int totface, int totvert)
{
	BBC *prim_bbc = NULL;
	BB cb;
	int i, j;

	if(totface != bvh->totface) {
		bvh->totface = totface;
		if(bvh->nodes) MEM_freeN(bvh->nodes);
		if(bvh->face_indices) MEM_freeN(bvh->face_indices);
		bvh->face_indices = MEM_callocN(sizeof(int) * totface,
						"bvh face indices");
		for(i = 0; i < totface; ++i)
			bvh->face_indices[i] = i;
		bvh->totnode = 0;
		if(bvh->node_mem_count < 100) {
			bvh->node_mem_count = 100;
			bvh->nodes = MEM_callocN(sizeof(Node) *
						 bvh->node_mem_count,
						 "bvh initial nodes");
		}
	}

	bvh->faces = faces;
	bvh->verts = verts;
	bvh->vert_bitmap = BLI_bitmap_new(totvert);

	BB_reset(&bvh->modified_bb);

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

	bvh->totnode = 1;
	build_sub(bvh, 0, &cb, prim_bbc, 0, totface);

	MEM_freeN(prim_bbc);
	MEM_freeN(bvh->vert_bitmap);
}

PBVH *BLI_pbvh_new(BLI_pbvh_HitCallback update_cb, void *update_cb_data)
{
	PBVH *bvh = MEM_callocN(sizeof(PBVH), "pbvh");
	
	bvh->update_cb = update_cb;
	bvh->update_cb_data = update_cb_data;

	return bvh;
}

void BLI_pbvh_free(PBVH *bvh)
{
	int i;

	for(i = 0; i < bvh->totnode; ++i) {
		if(bvh->nodes[i].flag & PBVH_Leaf) {
			GPU_free_buffers(bvh->nodes[i].draw_buffers);
			MEM_freeN(bvh->nodes[i].vert_indices);
		}
	}

	MEM_freeN(bvh->nodes);
	MEM_freeN(bvh->face_indices);
	MEM_freeN(bvh);
}

void BLI_pbvh_set_source(PBVH *bvh, MVert *mvert, MFace *mface)
{
	bvh->verts = mvert;
	bvh->faces = mface;
}

static void do_hit_callback(PBVH *bvh, Node *node,
			    BLI_pbvh_HitCallback cb, void *data)
{
	if(cb) {
		cb(bvh->face_indices + node->face_offset, node->vert_indices,
		   node->totface, node->uniq_verts, data);
	}
}

static int search_sub(PBVH *bvh, Node *node,
		      BLI_pbvh_SearchCallback scb, void *search_data_f,
		      BLI_pbvh_HitCallback hcb, void *hit_data_f,
		      PBVH_SearchMode mode)
{
	void *search_data = search_data_f;
	void *hit_data = hit_data_f;

	if(search_data_f == PBVH_NodeData)
		search_data = &node->flag;
	if(hit_data_f == PBVH_DrawData)
		hit_data = node->draw_buffers;

	if(scb(node->vb.bmin, node->vb.bmax, search_data)) {
		if(node->flag & PBVH_Leaf) {
			switch(mode) {
			case PBVH_SEARCH_MARK_MODIFIED:
				node->flag |= PBVH_Modified;
#ifdef PERFCNTRS
				++bvh->perf_modified;
#endif
				break;
			case PBVH_SEARCH_MODIFIED:
				if(node->flag & PBVH_Modified) {
					if(bvh->update_cb) {
						do_hit_callback
							(bvh, node,
							 bvh->update_cb,
							 bvh->update_cb_data);
					}

					GPU_update_buffers(node->draw_buffers,
							   bvh->verts,
							   node->vert_indices,
							   node->uniq_verts +
							   node->face_verts);
				}
			default:
				break;
			}

			do_hit_callback(bvh, node, hcb, hit_data);
		}
		else {
			int mod = 0;
			if(search_sub(bvh, bvh->nodes + node->children_offset,
				      scb, search_data_f, hcb,hit_data_f, mode))
				mod = 1;
			if(search_sub(bvh,
				      bvh->nodes + node->children_offset + 1,
				      scb, search_data_f, hcb,hit_data_f, mode))
				mod = 1;

			if(mod)
				node->flag |= PBVH_Modified;
		}
	}

	if(mode == PBVH_SEARCH_MODIFIED) {
#ifdef PERFCNTRS
		if(node->flag & PBVH_Modified && node->flag & PBVH_Leaf)
			--bvh->perf_modified;
#endif
		if(!bvh->modified_lock)
			node->flag &= ~PBVH_Modified;
	}
	else if(mode == PBVH_SEARCH_UPDATE) {
		if(node->flag & PBVH_Modified) {
			update_node_vb(bvh, node);
			if(node->flag & PBVH_Leaf)
				BB_expand_with_bb(&bvh->modified_bb, &node->vb);
		}
	}

	return node->flag & PBVH_Modified;
}

void BLI_pbvh_search(PBVH *bvh, BLI_pbvh_SearchCallback scb, void *search_data,
		     BLI_pbvh_HitCallback hcb, void *hit_data,
		     PBVH_SearchMode mode)
{
#ifdef PERFCNTRS
	printf("search mode=%s\n",
	       mode==PBVH_SEARCH_MARK_MODIFIED?"mark-modified":
	       mode==PBVH_SEARCH_MODIFIED?"modified":
	       mode==PBVH_SEARCH_UPDATE?"update":
	       mode==PBVH_SEARCH_NORMAL?"normal":"unknown-mode");
	if(mode == PBVH_SEARCH_MARK_MODIFIED)
		bvh->perf_modified = 0;
#endif

	search_sub(bvh, bvh->nodes, scb, search_data, hcb, hit_data, mode);
#ifdef PERFCNTRS
	printf("%d nodes marked modified\n", bvh->perf_modified);
	printf("search complete\n\n");
#endif
}

typedef struct {
	/* Ray */
	float start[3];
	int sign[3];
	float inv_dir[3];
} RaycastData;

/* Adapted from here: http://www.gamedev.net/community/forums/topic.asp?topic_id=459973 */
static int ray_aabb_intersect(float bb_min[3], float bb_max[3], void *data_v)
{
	RaycastData *ray = data_v;
	float bbox[2][3];
	float tmin, tmax, tymin, tymax, tzmin, tzmax;

	VecCopyf(bbox[0], bb_min);
	VecCopyf(bbox[1], bb_max);

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

	return 1;

	/* XXX: Not sure about this? 
	   if(tzmin > tmin)
	   tmin = tzmin;
	   if(tzmax < tmax)
	   tmax = tzmax;
	   return ((tmin < t1) && (tmax > t0));
	*/

}

void BLI_pbvh_raycast(PBVH *bvh, BLI_pbvh_HitCallback cb, void *data,
		      float ray_start[3], float ray_normal[3])
{
	RaycastData rcd;

	VecCopyf(rcd.start, ray_start);
	rcd.inv_dir[0] = 1.0f / ray_normal[0];
	rcd.inv_dir[1] = 1.0f / ray_normal[1];
	rcd.inv_dir[2] = 1.0f / ray_normal[2];
	rcd.sign[0] = rcd.inv_dir[0] < 0;
	rcd.sign[1] = rcd.inv_dir[1] < 0;
	rcd.sign[2] = rcd.inv_dir[2] < 0;

	BLI_pbvh_search(bvh, ray_aabb_intersect, &rcd, cb, data,
			PBVH_SEARCH_NORMAL);
}

int BLI_pbvh_update_search_cb(float bb_min[3], float bb_max[3], void *data_v)
{
	int *data = data_v;

	return ((*data) & PBVH_Modified);
}

void BLI_pbvh_modified_bounding_box(PBVH *bvh, float bb_min[3], float bb_max[3])
{
	VecCopyf(bb_min, bvh->modified_bb.bmin);
	VecCopyf(bb_max, bvh->modified_bb.bmax);
}

void BLI_pbvh_reset_modified_bounding_box(PBVH *bvh)
{
	BB_reset(&bvh->modified_bb);	
}

void BLI_pbvh_toggle_modified_lock(PBVH *bvh)
{
	bvh->modified_lock = !bvh->modified_lock;
}
