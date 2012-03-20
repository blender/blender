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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "BLI_heap.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_smallhash.h"
#include "BLI_rand.h"

#include "bmesh.h"


#define EDGE_MARK	1
#define EDGE_VIS	2

#define FACE_NEW	1

#define ELE_NEW		1
#define ELE_OUT		2
#define ELE_ORIG	4

#define FACE_IGNORE	16

typedef struct EPathNode {
	struct EPathNode *next, *prev;
	BMVert *v;
	BMEdge *e;
	BMEdge *cure;
} EPathNode;

typedef struct EPath {
	ListBase nodes;
	float weight;
	int group;
} EPath;

typedef struct PathBase {
	BLI_mempool *nodepool, *pathpool;
} PathBase;

typedef struct EdgeData {
	int tag;
	int ftag;
	BMDiskLink v1_disk_link, v2_disk_link;
} EdgeData;

typedef struct VertData {
	BMEdge *e;
	float no[3], offco[3], sco[3]; /* offco is vertex coordinate slightly offset randomly */
	int tag;
} VertData;

static int count_edge_faces(BMesh *bm, BMEdge *e);

/****  rotation system code * */

BLI_INLINE BMDiskLink *rs_edge_link_get(BMEdge *e, BMVert *v, EdgeData *e_data)
{
	return 	v == ((BMEdge *)e)->v1 ? &(((EdgeData *)e_data)->v1_disk_link) :
	                                 &(((EdgeData *)e_data)->v2_disk_link) ;
}

static int rotsys_append_edge(BMEdge *e, BMVert *v,
                              EdgeData *edata, VertData *vdata)
{
	EdgeData *ed = &edata[BM_elem_index_get(e)];
	VertData *vd = &vdata[BM_elem_index_get(v)];
	
	if (!vd->e) {
		Link *e1 = (Link *)rs_edge_link_get(e, v, ed);

		vd->e = e;
		e1->next = e1->prev = (Link *)e;
	}
	else {
		BMDiskLink *dl1, *dl2, *dl3;
		EdgeData *ved = &edata[BM_elem_index_get(vd->e)];

		dl1 = rs_edge_link_get(e, v, ed);
		dl2 = rs_edge_link_get(vd->e, v, ved);
		dl3 = dl2->prev ? rs_edge_link_get(dl2->prev, v, &edata[BM_elem_index_get(dl2->prev)]) : NULL;

		dl1->next = vd->e;
		dl1->prev = dl2->prev;

		dl2->prev = e;
		if (dl3) {
			dl3->next = e;
		}
	}

	return TRUE;
}

static void UNUSED_FUNCTION(rotsys_remove_edge)(BMEdge *e, BMVert *v,
                                                EdgeData *edata, VertData *vdata)
{
	EdgeData *ed = edata + BM_elem_index_get(e);
	VertData *vd = vdata + BM_elem_index_get(v);
	BMDiskLink *e1, *e2;

	e1 = rs_edge_link_get(e, v, ed);
	if (e1->prev) {
		e2 = rs_edge_link_get(e1->prev, v, ed);
		e2->next = e1->next;
	}

	if (e1->next) {
		e2 = rs_edge_link_get(e1->next, v, ed);
		e2->prev = e1->prev;
	}

	if (vd->e == e)
		vd->e = (e != e1->next) ? e1->next : NULL;

	e1->next = e1->prev = NULL;
}

static BMEdge *rotsys_nextedge(BMEdge *e, BMVert *v,
                               EdgeData *edata, VertData *UNUSED(vdata))
{
	if (v == e->v1)
		return edata[BM_elem_index_get(e)].v1_disk_link.next;
	if (v == e->v2)
		return edata[BM_elem_index_get(e)].v2_disk_link.next;
	return NULL;
}

static BMEdge *rotsys_prevedge(BMEdge *e, BMVert *v,
                               EdgeData *edata, VertData *UNUSED(vdata))
{
	if (v == e->v1)
		return edata[BM_elem_index_get(e)].v1_disk_link.prev;
	if (v == e->v2)
		return edata[BM_elem_index_get(e)].v2_disk_link.prev;
	return NULL;
}

static void rotsys_reverse(BMEdge *UNUSED(e), BMVert *v, EdgeData *edata, VertData *vdata)
{
	BMEdge **edges = NULL;
	BMEdge *e_first;
	BMEdge *e;
	BLI_array_staticdeclare(edges, BM_NGON_STACK_SIZE);
	int i, totedge;
	
	e = e_first = vdata[BM_elem_index_get(v)].e;
	do {
		BLI_array_append(edges, e);
		e = rotsys_nextedge(e, v, edata, vdata);
	} while (e != e_first);
	
	totedge = BLI_array_count(edges);
	for (i = 0; i < totedge / 2; i++) {
		SWAP(BMEdge *, edges[i], edges[totedge - 1 - i]);
	}
	
	vdata[BM_elem_index_get(v)].e = NULL;
	for (i = 0; i < totedge; i++) {
		rotsys_append_edge(edges[i], v, edata, vdata);
	}
	
	BLI_array_free(edges);
}

static int UNUSED_FUNCTION(rotsys_count)(BMVert *v, EdgeData *edata, VertData *vdata)
{
	BMEdge *e = vdata[BM_elem_index_get(v)].e;
	int i = 0;

	if (!e)
		return 0;

	do {
		if (!e)
			return 0;
		e =  rotsys_nextedge(e, v, edata, vdata);

		if (i >= (1 << 20)) {
			printf("bmesh error: infinite loop in disk cycle!\n");
			return 0;
		}

		i += 1;
	} while (e != vdata[BM_elem_index_get(v)].e);

	return i;
}

static int UNUSED_FUNCTION(rotsys_fill_faces)(BMesh *bm, EdgeData *edata, VertData *vdata)
{
	BMIter iter;
	BMEdge *e, **edges = NULL;
	BLI_array_declare(edges);
	BMVert *v, **verts = NULL;
	BMFace *f;
	BLI_array_declare(verts);
	SmallHash visithash, *hash = &visithash;
	int i;
	
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		BMEdge *e2, *starte;
		BMVert *startv;
		int rad, ok;
		
		rad = count_edge_faces(bm, e);
		
		if (rad < 2) {
			starte = e;
		}
		else {
			continue;
		}

		/* do two passes, going forward then backward */
		for (i = 0; i < 2; i++) {
			BLI_smallhash_init(hash);
			
			BLI_array_empty(verts);
			BLI_array_empty(edges);

			startv = v = starte->v1;
			e2 = starte;
			ok = 1;
			if (!v || !e2)
				continue;

			do {
				if (BLI_smallhash_haskey(hash, (intptr_t)e2) ||
				    BLI_smallhash_haskey(hash, (intptr_t)v))
				{
					ok = 0;
					break;
				}
				
				BLI_array_append(verts, v);
				BLI_array_append(edges, e2);
				
				BLI_smallhash_insert(hash, (intptr_t)e2, NULL);

				v = BM_edge_other_vert(e2, v);
				e2 = i ? rotsys_prevedge(e2, v, edata, vdata) : rotsys_nextedge(e2, v, edata, vdata);
			} while (e2 != starte && v != startv);
			
			BLI_smallhash_release(hash);
			
			if (!ok || BLI_array_count(edges) < 3)
				continue;
			
			f = BM_face_create_ngon(bm, verts[0], verts[1], edges, BLI_array_count(edges), TRUE);
			if (!f)
				continue;
		}
	}
	
	return 0;
}

static void rotsys_make_consistent(BMesh *bm, EdgeData *edata, VertData *vdata)
{
	BMIter iter;
	BMEdge *e;
	BMVert *v, **stack = NULL;
	BLI_array_declare(stack);
	int i;
	
	for (i = 0; i < bm->totvert; i++) {
		vdata[i].tag = 0;
	}
	
	while (1) {
		VertData *vd;
		BMVert *startv = NULL;
		float dis;
		
		v = BM_iter_new(&iter, bm, BM_VERTS_OF_MESH, NULL);
		for (i = 0; i < bm->totvert; i++, BM_iter_step(&iter)) {
			vd = vdata + BM_elem_index_get(v);
			
			if (vd->tag)
				continue;
			
			if (!startv || dot_v3v3(vd->offco, vd->offco) > dis) {
				dis = dot_v3v3(vd->offco, vd->offco);
				startv = v;
			}
		}
		
		if (!startv)
			break;
		
		vd = vdata + BM_elem_index_get(startv);
		
		BLI_array_empty(stack);
		BLI_array_append(stack, startv);
		
		vd->tag = 1;
		
		while (BLI_array_count(stack)) {
			v = BLI_array_pop(stack);
			vd = vdata + BM_elem_index_get(v);
			
			if (!vd->e)
				continue;
			
			e = vd->e;
			do {
				BMVert *v2 = BM_edge_other_vert(e, v);
				VertData *vd2 = vdata + BM_elem_index_get(v2);
				
				if (dot_v3v3(vd->no, vd2->no) < 0.0f + FLT_EPSILON * 2) {
					rotsys_reverse(e, v2, edata, vdata);
					mul_v3_fl(vd2->no, -1.0f);
				}

				if (!vd2->tag) {
					BLI_array_append(stack, v2);
					vd2->tag = 1;
				}
				
				e = rotsys_nextedge(e, v, edata, vdata);
			} while (e != vd->e);
		}
	}

	BLI_array_free(stack);
}

static void init_rotsys(BMesh *bm, EdgeData *edata, VertData *vdata)
{
	BMIter iter;
	BMEdge *e;
	BMEdge **edges = NULL;
	BLI_array_staticdeclare(edges, BM_NGON_STACK_SIZE);
	BMVert *v;
	/* BMVert **verts = NULL; */
	/* BLI_array_staticdeclare(verts, BM_NGON_STACK_SIZE); */ /* UNUSE */
	int i;
	
#define SIGN(n) ((n)<0.0f)
	
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BMIter eiter;
		float no[3], cent[3];
		int j, k = 0, totedge = 0;
		
		if (BM_elem_index_get(v) == -1)
			continue;
		
		BLI_array_empty(edges);
		
		BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
			if (BMO_elem_flag_test(bm, e, EDGE_MARK)) {
				BLI_array_append(edges, e);
				totedge++;
			}
		}
		
		copy_v3_v3(cent, v->co);
		
		zero_v3(no);
		for (i = 0; i < totedge; i++) {
			BMEdge *e1, *e2;
			float cno[3], vec1[3], vec2[3];
			
			e1 = edges[i];
			e2 = edges[(i + 1) % totedge];

			sub_v3_v3v3(vec1, (BM_edge_other_vert(e1, v))->co, v->co);
			sub_v3_v3v3(vec2, (BM_edge_other_vert(e2, v))->co, v->co);
			
			cross_v3_v3v3(cno, vec1, vec2);
			normalize_v3(cno);
			
			if (i && dot_v3v3(cno, no) < 0.0f + FLT_EPSILON * 10)
				mul_v3_fl(cno, -1.0f);
			
			add_v3_v3(no, cno);
			normalize_v3(no);
		}
		
		/* generate plane-flattened coordinates */
		for (i = 0; i < totedge; i++) {
			BMEdge *e1;
			BMVert *v2;
			float cvec[3], vec1[3];
			
			e1 = edges[i];
			v2 = BM_edge_other_vert(e1, v);
			
			sub_v3_v3v3(vec1, v2->co, v->co);
			
			cross_v3_v3v3(cvec, vec1, no);
			cross_v3_v3v3(vec1, cvec, no);
			normalize_v3(vec1);
			
			mul_v3_fl(vec1, len_v3v3(v2->co, v->co));
			add_v3_v3(vec1, v->co);
			
			copy_v3_v3(vdata[BM_elem_index_get(v2)].sco, vec1);
		}
		
		BLI_srandom(0);
		
		/* first, ensure no 0 or 180 angles between adjacent
		 * (and that adjacent's adjacent) edges */
		for (i = 0, k = 0; i < totedge; i++) {
			BMEdge *e1, *e2, *e3 = NULL;
			BMVert *v1, *v2, *v3;
			VertData *vd1, *vd2, *vd3;
			float vec1[3], vec2[3], vec3[3], size;
			int s1, s2, s3;
			
			if (totedge < 3)
				continue;
			
			e1 = edges[(i + totedge - 1) % totedge];
			e2 = edges[i];
			e3 = edges[(i + 1) % totedge];
			
			v1 = BM_edge_other_vert(e1, v);
			v2 = BM_edge_other_vert(e2, v);
			v3 = BM_edge_other_vert(e3, v);

			vd1 = vdata + BM_elem_index_get(v1);
			vd2 = vdata + BM_elem_index_get(v2);
			vd3 = vdata + BM_elem_index_get(v3);
			
			sub_v3_v3v3(vec1, vd1->sco, cent);
			sub_v3_v3v3(vec2, vd2->sco, cent);
			sub_v3_v3v3(vec3, vd3->sco, cent);
			
			size = (len_v3(vec1) + len_v3(vec3)) * 0.01f;
			normalize_v3(vec1); normalize_v3(vec2); normalize_v3(vec3);
			
#ifdef STRAIGHT
#undef STRAIGHT
#endif
#define STRAIGHT(vec11, vec22) (fabsf(dot_v3v3((vec11), (vec22))) > 1.0f - ((float)FLT_EPSILON * 1000.0f))
			
			s1 = STRAIGHT(vec1, vec2); s2 = STRAIGHT(vec2, vec3); s3 = STRAIGHT(vec1, vec3);
			
			if (s1 || s2 || s3) {
				copy_v3_v3(cent, v->co);

				for (j = 0; j < 3; j++) {
					float fac = (BLI_frand() - 0.5f)*size;
					cent[j] += fac;
				}
				
				if (k < 2000) {
					i = 0;
					k++;
					continue;
				}
				else {
					k++;
					continue;
				}

			}
		}
		
		copy_v3_v3(vdata[BM_elem_index_get(v)].offco, cent);
		//copy_v3_v3(v->co, cent);
		
		/* now, sort edges so the triangle fan of all edges
		 * has a consistent normal.  this is the same as
		 * sorting by polar coordinates along a group normal */
		for (j = 0; j < totedge; j++) {
			for (i = 0; i < totedge; i++) {
				BMEdge *e1, *e2, *e3 = NULL;
				BMVert *v1, *v2, *v3;
				VertData *vd1, *vd2, *vd3;
				float vec1[3], vec2[3], vec3[3], n1[3], n2[3], n3[3];
				
				e1 = edges[(i + totedge - 1) % totedge];
				e2 = edges[i];
				e3 = edges[(i + 1) % totedge];
				
				v1 = BM_edge_other_vert(e1, v);
				v2 = BM_edge_other_vert(e2, v);
				v3 = BM_edge_other_vert(e3, v);

				vd1 = vdata + BM_elem_index_get(v1);
				vd2 = vdata + BM_elem_index_get(v2);
				vd3 = vdata + BM_elem_index_get(v3);

				sub_v3_v3v3(vec1, vd1->sco, cent);
				sub_v3_v3v3(vec2, vd2->sco, cent);
				sub_v3_v3v3(vec3, vd3->sco, cent);
				
				cross_v3_v3v3(n1, vec1, vec2);
				cross_v3_v3v3(n2, vec2, vec3);
				cross_v3_v3v3(n3, vec1, vec3);

				/* this case happens often enough and probably not worth bothering users with,
				 * maybe enable for debugging code but not for everyday use - campbell */
#if 0
				/* Other way to determine if two vectors approach are (nearly) parallel: the
				 * cross product of the two vectors will approach zero */
				{
					int s1, s2, s3;
					s1 = (dot_v3v3(n1, n1) < (0.0f + FLT_EPSILON * 10));
					s2 = (dot_v3v3(n2, n2) < (0.0f + FLT_EPSILON * 10));
					s3 = (totedge < 3) ? 0 : (dot_v3v3(n3, n3) < (0.0f + FLT_EPSILON * 10));

					if (s1 || s2 || s3) {
						fprintf(stderr, "%s: s1: %d, s2: %d, s3: %dx (bmesh internal error)\n", __func__, s1, s2, s3);
					}
				}
#endif

				normalize_v3(n1); normalize_v3(n2); normalize_v3(n3);


				if (dot_v3v3(n1, n2) < 0.0f) {
					if (dot_v3v3(n1, n3) >= 0.0f + FLT_EPSILON * 10) {
						SWAP(BMEdge *, edges[i], edges[(i + 1) % totedge]);
					}
					else {
						SWAP(BMEdge *, edges[(i + totedge - 1) % totedge], edges[(i + 1) % totedge]);
						SWAP(BMEdge *, edges[i], edges[(i + 1) % totedge]);
					}
				}
			}
		}
		
#undef STRAIGHT

		zero_v3(no);

		/* yay, edges are sorted */
		for (i = 0; i < totedge; i++) {
			BMEdge *e1 = edges[i], *e2 = edges[(i + 1) % totedge];
			float eno[3];
			
			normal_tri_v3(eno, BM_edge_other_vert(e1, v)->co, v->co, BM_edge_other_vert(e2, v)->co);
			add_v3_v3(no, eno);
			
			rotsys_append_edge(edges[i], v, edata, vdata);
		}
		
		normalize_v3(no);
		copy_v3_v3(vdata[BM_elem_index_get(v)].no, no);
	}
	
	/* now, make sure rotation system is topologically consistent
	 * (e.g. vert normals consistently point either inside or outside) */
	rotsys_make_consistent(bm, edata, vdata);

	//rotsys_fill_faces(bm, edata, vdata);

#if 0
	/* create visualizing geometr */
	BMVert *lastv;
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BMVert *v2;
		BMFace *f;
		int totedge = BM_vert_edge_count(v);

		if (BM_elem_index_get(v) == -1)
			continue;
		
		//cv = BM_vert_create(bm, cent, v);
		//BM_elem_index_set(cv, -1);  /* set_dirty! */
		i = 0;
		e = vdata[BM_elem_index_get(v)].e;
		lastv = NULL;
		do {
			BMEdge *e2;
			BMVert *v2;
			float f = ((float)i / (float)totedge) * 0.35 + 0.05;
			float co[3];
			
			if (!e)
				break;

			if (!BM_edge_other_vert(e, v))
				continue;
			
			sub_v3_v3v3(co, (BM_edge_other_vert(e, v))->co, vdata[BM_elem_index_get(v)].offco);
			mul_v3_fl(co, f);
			add_v3_v3(co, vdata[BM_elem_index_get(v)].offco);
			
			v2 = BM_vert_create(bm, co, NULL);
			BM_elem_index_set(v2, -1); /* set_dirty! */
			//BM_edge_create(bm, cv, v2, NULL, FALSE);
			
			BM_elem_select_set(bm, v2, TRUE);
			if (lastv) {
				e2 = BM_edge_create(bm, lastv, v2, NULL, FALSE);
				BM_elem_select_set(bm, e2, TRUE);
			}
			
			lastv = v2;
			
			e = rotsys_nextedge(e, v, edata, vdata);
			i++;
		} while (e != vdata[BM_elem_index_get(v)].e);
	}
#endif

	BLI_array_free(edges);
}

static PathBase *edge_pathbase_new(void)
{
	PathBase *pb = MEM_callocN(sizeof(PathBase), "PathBase");

	pb->nodepool = BLI_mempool_create(sizeof(EPathNode), 1, 512, BLI_MEMPOOL_SYSMALLOC);
	pb->pathpool = BLI_mempool_create(sizeof(EPath), 1, 512, BLI_MEMPOOL_SYSMALLOC);

	return pb;
}

static void edge_pathbase_free(PathBase *pathbase)
{
	BLI_mempool_destroy(pathbase->nodepool);
	BLI_mempool_destroy(pathbase->pathpool);
	MEM_freeN(pathbase);
}

static EPath *edge_copy_add_path(PathBase *pb, EPath *path, BMVert *appendv, BMEdge *e)
{
	EPath *path2;
	EPathNode *node, *node2;

	path2 = BLI_mempool_alloc(pb->pathpool);
	path2->nodes.first = path2->nodes.last = NULL;
	path2->weight = 0.0f;
	path2->group = path->group;
	
	for (node = path->nodes.first; node; node = node->next) {
		node2 = BLI_mempool_alloc(pb->nodepool);
		*node2 = *node;
		BLI_addtail(&path2->nodes, node2);
	}

	node2 = BLI_mempool_alloc(pb->nodepool);
	node2->v = appendv;
	node2->e = e;
	node2->cure = NULL;

	BLI_addtail(&path2->nodes, node2);

	return path2;
}

static EPath *edge_path_new(PathBase *pb, BMVert *start, BMEdge *starte)
{
	EPath *path;
	EPathNode *node;

	path = BLI_mempool_alloc(pb->pathpool);
	node = BLI_mempool_alloc(pb->nodepool);
	
	path->nodes.first = path->nodes.last = NULL;
	
	node->v = start;
	node->e = starte;
	node->cure = NULL;

	BLI_addtail(&path->nodes, node);
	path->weight = 0.0f;

	return path;
}

static float edge_weight_path(EPath *path, EdgeData *edata, VertData *UNUSED(vdata))
{
	EPathNode *node, *first = path->nodes.first;
	float w = 0.0;

	for (node = path->nodes.first; node; node = node->next) {
		if (node->e && node != path->nodes.first) {
			w += edata[BM_elem_index_get(node->e)].ftag;
			if (node->prev) {
				/* BMESH_TOD */
				(void)first;
				//w += len_v3v3(node->v->co, first->e->v1->co) * 0.0001f;
				//w += len_v3v3(node->v->co, first->e->v2->co) * 0.0001f;
			}
		}

		w += 1.0f;
	}

	return w;
}


static void edge_free_path(PathBase *pathbase, EPath *path)
{
	EPathNode *node, *next;

	for (node = path->nodes.first; node; node = next) {
		next = node->next;
		BLI_mempool_free(pathbase->nodepool, node);
	}

	BLI_mempool_free(pathbase->pathpool, path);
}

static EPath *edge_find_shortest_path(BMesh *bm, BMOperator *op, BMEdge *edge, EdgeData *edata,
                                      VertData *vdata, PathBase *pathbase, int group)
{
	BMEdge *e;
	GHash *gh = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "createops find shortest path");
	BMVert *v1, *v2;
	BMVert **verts = NULL;
	BLI_array_staticdeclare(verts, 1024);
	Heap *heap = BLI_heap_new();
	EPath *path = NULL, *path2;
	BMVert *startv;
	BMVert *endv;
	EPathNode *node;
	int i, use_restrict = BMO_slot_bool_get(op, "use_restrict");

	startv = edata[BM_elem_index_get(edge)].ftag ? edge->v2 : edge->v1;
	endv = edata[BM_elem_index_get(edge)].ftag ? edge->v1 : edge->v2;
	
	path = edge_path_new(pathbase, startv, edge);
	BLI_ghash_insert(gh, startv, NULL);
	BLI_heap_insert(heap, path->weight, path);
	path->group = group;

	while (BLI_heap_size(heap)) {
		VertData *vd;
		EPathNode *last;
		BMFace *f = NULL;
		
		path = BLI_heap_popmin(heap);
		last = path->nodes.last;
		v1 = last->v;
		
		if (v1 == endv) {
			/* make sure this path loop doesn't already exists */
			i = 0;
			BLI_array_empty(verts);
			for (i = 0, node = path->nodes.first; node; node = node->next, i++) {
				BLI_array_growone(verts);
				verts[i] = node->v;
			}

			if (BM_face_exists(bm, verts, i, &f)) {
				if (!BMO_elem_flag_test(bm, f, FACE_IGNORE)) {
					BLI_ghash_remove(gh, endv, NULL, NULL);
					continue;
				}
			}
			break;
		}
		
		vd = vdata + BM_elem_index_get(v1);
		if (!vd->e)
			continue;
		
		v2 = NULL;
		while (1) {
			if (!last->cure) {
				last->cure = e = vdata[BM_elem_index_get(last->v)].e;
			}
			else {
				last->cure = e = rotsys_nextedge(last->cure, last->v, edata, vdata);
				if (last->cure == vdata[BM_elem_index_get(last->v)].e) {
					v2 = NULL;
					break;
				}
			}
			
			if (e == edge || !BMO_elem_flag_test(bm, e, EDGE_MARK)) {
				continue;
			}

			v2 = BM_edge_other_vert(e, last->v);
			
			if (BLI_ghash_haskey(gh, v2)) {
				v2 = NULL;
				continue;
			}
			
			if (use_restrict && BMO_slot_map_contains(bm, op, "restrict", e)) {
				int group = BMO_slot_map_int_get(bm, op, "restrict", e);
				
				if (!(group & path->group)) {
					v2 = NULL;
					continue;
				}
			}

			break;
		}
		
		if (!v2) {
			if (path) {
				edge_free_path(pathbase, path);
				path = NULL;
			}
			continue;
		}
		
		/* add path back into heap */
		BLI_heap_insert(heap, path->weight, path);
		
		/* put v2 in gh ma */
		BLI_ghash_insert(gh, v2, NULL);

		path2 = edge_copy_add_path(pathbase, path, v2, e);
		path2->weight = edge_weight_path(path2, edata, vdata);

		BLI_heap_insert(heap, path2->weight, path2);
	}
	
	if (path && ((EPathNode *)path->nodes.last)->v != endv) {
		edge_free_path(pathbase, path);
		path = NULL;
	}

	BLI_array_free(verts);
	BLI_heap_free(heap, NULL);
	BLI_ghash_free(gh, NULL, NULL);

	return path;
}

static int count_edge_faces(BMesh *bm, BMEdge *e)
{
	int i = 0;
	BMLoop *l = e->l;
	
	if (!l) {
		return 0;
	}

	do {
		if (!BMO_elem_flag_test(bm, l->f, FACE_IGNORE)) {
			i++;
		}

		l = l->radial_next;
	} while (l != e->l);

	return i;
}

BLI_INLINE void vote_on_winding(BMEdge *edge, EPathNode *node, unsigned int winding[2])
{
	BMVert *test_v1, *test_v2;
	/* we want to use the reverse winding to the existing order */
	BM_edge_ordered_verts(edge, &test_v2, &test_v1);

	/* edges vote on which winding wins out */
	winding[(test_v1 == node->v)]++;
}

void bmo_edgenet_fill_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter;
	BMOIter siter;
	BMFace *f;
	BMEdge *e, *edge;
	BMVert **verts = NULL;
	BLI_array_declare(verts);
	EPath *path;
	EPathNode *node;
	EdgeData *edata;
	VertData *vdata;
	BMEdge **edges = NULL;
	PathBase *pathbase;
	BLI_array_declare(edges);
	int use_restrict   = BMO_slot_bool_get(op, "use_restrict");
	int use_fill_check = BMO_slot_bool_get(op, "use_fill_check");
	int i, j, group = 0;
	unsigned int winding[2]; /* accumulte winding directions for each edge which has a face */

	if (!bm->totvert || !bm->totedge)
		return;

	pathbase = edge_pathbase_new();

	edata = MEM_callocN(sizeof(EdgeData) * bm->totedge, "EdgeData");
	vdata = MEM_callocN(sizeof(VertData) * bm->totvert, "VertData");
	
	BMO_slot_buffer_flag_enable(bm, op, "edges", BM_EDGE, EDGE_MARK);
	BMO_slot_buffer_flag_enable(bm, op, "excludefaces", BM_FACE, FACE_IGNORE);
	
	BM_mesh_elem_index_ensure(bm, BM_VERT);

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		BMO_elem_flag_enable(bm, f, ELE_ORIG);
	}

	i = 0;
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		BM_elem_index_set(e, i); /* set_inline */
		
		if (!BMO_elem_flag_test(bm, e, EDGE_MARK)) {
			edata[i].tag = 2;
		}

		i++;
	}
	bm->elem_index_dirty &= ~BM_EDGE;

	init_rotsys(bm, edata, vdata);
	
	while (1) {
		edge = NULL;
		group = 0;
		
		BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
			/* if restrict is on, only start on faces in the restrict map */
			if (use_restrict && !BMO_slot_map_contains(bm, op, "restrict", e))
				continue;

			if (edata[BM_elem_index_get(e)].tag < 2) {
				edge = e;

				if (use_restrict) {
					int i = 0, j = 0, gi = 0;
					
					group = BMO_slot_map_int_get(bm, op, "restrict", e);
					
					for (i = 0; i < 30; i++) {
						if (group & (1 << i)) {
							j++;
							gi = i;

							if (j - 1 == edata[BM_elem_index_get(e)].tag) {
								break;
							}
						}
					}

					group = (1 << gi);
				}
				
				break;
			}
		}

		if (!edge)
			break;

		edata[BM_elem_index_get(edge)].tag += 1;

		path = edge_find_shortest_path(bm, op, edge, edata, vdata, pathbase, group);
		if (!path)
			continue;
		
		winding[0] = winding[1] = 0;

		BLI_array_empty(edges);
		BLI_array_empty(verts);
		i = 0;
		for (node = path->nodes.first; node; node = node->next) {
			if (!node->next)
				continue;

			e = BM_edge_exists(node->v, node->next->v);
			
			/* this should never happe */
			if (!e)
				break;
			
			/* check on the winding */
			if (e->l) {
				vote_on_winding(e, node, winding);
			}

			edata[BM_elem_index_get(e)].ftag++;
			BLI_array_growone(edges);
			edges[i++] = e;

			BLI_array_append(verts, node->v);
		}
		
		if (edge->l) {
			vote_on_winding(edge, path->nodes.last, winding);
		}

		BLI_array_growone(edges);
		edges[i++] = edge;
		edata[BM_elem_index_get(edge)].ftag++;
		
		for (j = 0; j < i; j++) {
			if (count_edge_faces(bm, edges[j]) >= 2) {
				edge_free_path(pathbase, path);
				break;
			}
		}

		if (j != i) {
			continue;
		}

		if (i) {
			BMVert *v1, *v2;

			/* to define the winding order must select first edge,
			 * otherwise we could leave this as-is */
			edge = edges[0];

			/* if these are even it doesnt really matter what to do,
			 * with consistent geometry one will be zero, the choice is clear */
			if (winding[0] < winding[1]) {
				v1 = verts[0];
				v2 = verts[1];
			}
			else {
				v1 = verts[1];
				v2 = verts[0];
			}

			if ((use_fill_check == FALSE) ||
			    /* fairly expensive check - see if there are already faces filling this area */
			    (BM_face_exists_multi_edge(bm, edges, i) == FALSE))
			{
				f = BM_face_create_ngon(bm, v1, v2, edges, i, TRUE);
				if (f && !BMO_elem_flag_test(bm, f, ELE_ORIG)) {
					BMO_elem_flag_enable(bm, f, FACE_NEW);
				}

				if (use_restrict) {
					BMO_slot_map_int_insert(bm, op, "faceout_groupmap", f, path->group);
				}
			}
		}
		
		edge_free_path(pathbase, path);
	}

	BMO_slot_buffer_from_flag(bm, op, "faceout", BM_FACE, FACE_NEW);

	BLI_array_free(edges);
	BLI_array_free(verts);
	edge_pathbase_free(pathbase);
	MEM_freeN(edata);
	MEM_freeN(vdata);
}

static BMEdge *edge_next(BMesh *bm, BMEdge *e)
{
	BMIter iter;
	BMEdge *e2;
	int i;

	for (i = 0; i < 2; i++) {
		BM_ITER(e2, &iter, bm, BM_EDGES_OF_VERT, i ? e->v2 : e->v1) {
			if ((BMO_elem_flag_test(bm, e2, EDGE_MARK)) &&
			    (!BMO_elem_flag_test(bm, e2, EDGE_VIS)) &&
			    (e2 != e))
			{
				return e2;
			}
		}
	}

	return NULL;
}

void bmo_edgenet_prepare(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMEdge *e;
	BMEdge **edges1 = NULL, **edges2 = NULL, **edges;
	BLI_array_declare(edges1);
	BLI_array_declare(edges2);
	BLI_array_declare(edges);
	int ok = 1;
	int i, count;

	BMO_slot_buffer_flag_enable(bm, op, "edges", BM_EDGE, EDGE_MARK);
	
	/* validate that each edge has at most one other tagged edge in the
	 * disk cycle around each of it's vertices */
	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		for (i = 0; i < 2; i++) {
			count = BMO_vert_edge_flags_count(bm, i ? e->v2 : e->v1, EDGE_MARK);
			if (count > 2) {
				ok = 0;
				break;
			}
		}

		if (!ok) {
			break;
		}
	}

	/* we don't have valid edge layouts, retur */
	if (!ok) {
		return;
	}

	/* find connected loops within the input edge */
	count = 0;
	while (1) {
		BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
			if (!BMO_elem_flag_test(bm, e, EDGE_VIS)) {
				if (BMO_vert_edge_flags_count(bm, e->v1, EDGE_MARK) == 1 ||
				    BMO_vert_edge_flags_count(bm, e->v2, EDGE_MARK) == 1)
				{
					break;
				}
			}
		}
		
		if (!e) {
			break;
		}

		if (!count) {
			edges = edges1;
		}
		else if (count == 1) {
			edges = edges2;
		}
		else {
			break;
		}
		
		i = 0;
		while (e) {
			BMO_elem_flag_enable(bm, e, EDGE_VIS);
			BLI_array_growone(edges);
			edges[i] = e;

			e = edge_next(bm, e);
			i++;
		}

		if (!count) {
			edges1 = edges;
			BLI_array_set_length(edges1, BLI_array_count(edges));
		}
		else {
			edges2 = edges;
			BLI_array_set_length(edges2, BLI_array_count(edges));
		}

		BLI_array_empty(edges);
		count++;
	}

	if (edges1 && BLI_array_count(edges1) > 2 &&
	    BM_edge_share_vert_count(edges1[0], edges1[BLI_array_count(edges1) - 1]))
	{
		if (edges2 && BLI_array_count(edges2) > 2 &&
		    BM_edge_share_vert_count(edges2[0], edges2[BLI_array_count(edges2) - 1]))
		{
			BLI_array_free(edges1);
			BLI_array_free(edges2);
			return;
		}
		else {
			edges1 = edges2;
			edges2 = NULL;
		}
	}

	if (edges2 && BLI_array_count(edges2) > 2 &&
	    BM_edge_share_vert_count(edges2[0], edges2[BLI_array_count(edges2) - 1]))
	{
		edges2 = NULL;
	}

	/* two unconnected loops, connect the */
	if (edges1 && edges2) {
		BMVert *v1, *v2, *v3, *v4;
		float dvec1[3];
		float dvec2[3];

		if (BLI_array_count(edges1) == 1) {
			v1 = edges1[0]->v1;
			v2 = edges1[0]->v2;
		}
		else {
			v1 = BM_vert_in_edge(edges1[1], edges1[0]->v1) ? edges1[0]->v2 : edges1[0]->v1;
			i  = BLI_array_count(edges1) - 1;
			v2 = BM_vert_in_edge(edges1[i - 1], edges1[i]->v1) ? edges1[i]->v2 : edges1[i]->v1;
		}

		if (BLI_array_count(edges2) == 1) {
			v3 = edges2[0]->v1;
			v4 = edges2[0]->v2;
		}
		else {
			v3 = BM_vert_in_edge(edges2[1], edges2[0]->v1) ? edges2[0]->v2 : edges2[0]->v1;
			i  = BLI_array_count(edges2) - 1;
			v4 = BM_vert_in_edge(edges2[i - 1], edges2[i]->v1) ? edges2[i]->v2 : edges2[i]->v1;
		}

		/* if there is ever bowtie quads between two edges the problem is here! [#30367] */
#if 0
		normal_tri_v3(dvec1, v1->co, v2->co, v4->co);
		normal_tri_v3(dvec2, v1->co, v4->co, v3->co);
#else
		{
			/* save some CPU cycles and skip the sqrt and 1 subtraction */
			float a1[3], a2[3], a3[3];
			sub_v3_v3v3(a1, v1->co, v2->co);
			sub_v3_v3v3(a2, v1->co, v4->co);
			sub_v3_v3v3(a3, v1->co, v3->co);
			cross_v3_v3v3(dvec1, a1, a2);
			cross_v3_v3v3(dvec2, a2, a3);
		}
#endif
		if (dot_v3v3(dvec1, dvec2) < 0.0f) {
			SWAP(BMVert *, v3, v4);
		}

		e = BM_edge_create(bm, v1, v3, NULL, TRUE);
		BMO_elem_flag_enable(bm, e, ELE_NEW);
		e = BM_edge_create(bm, v2, v4, NULL, TRUE);
		BMO_elem_flag_enable(bm, e, ELE_NEW);
	}
	else if (edges1) {
		BMVert *v1, *v2;

		if (BLI_array_count(edges1) > 1) {
			v1 = BM_vert_in_edge(edges1[1], edges1[0]->v1) ? edges1[0]->v2 : edges1[0]->v1;
			i  = BLI_array_count(edges1) - 1;
			v2 = BM_vert_in_edge(edges1[i - 1], edges1[i]->v1) ? edges1[i]->v2 : edges1[i]->v1;
			e  = BM_edge_create(bm, v1, v2, NULL, TRUE);
			BMO_elem_flag_enable(bm, e, ELE_NEW);
		}
	}
	
	BMO_slot_buffer_from_flag(bm, op, "edgeout", BM_EDGE, ELE_NEW);

	BLI_array_free(edges1);
	BLI_array_free(edges2);
}

/* This is what runs when pressing the F key
 * doing the best thing here isn't always easy create vs dissolve, its nice to support
 * but it it _really_ gives issues we might have to not call dissolve. - campbell
 */
void bmo_contextual_create_exec(BMesh *bm, BMOperator *op)
{
	BMOperator op2;
	BMOIter oiter;
	BMIter iter;
	BMHeader *h;
	BMVert *v, *verts[4];
	BMEdge *e;
	BMFace *f;
	int totv = 0, tote = 0, totf = 0, amount;

	/* count number of each element type we were passe */
	BMO_ITER(h, &oiter, bm, op, "geom", BM_VERT|BM_EDGE|BM_FACE) {
		switch (h->htype) {
			case BM_VERT: totv++; break;
			case BM_EDGE: tote++; break;
			case BM_FACE: totf++; break;
		}

		BMO_elem_flag_enable(bm, (BMElemF *)h, ELE_NEW);
	}
	
	/* --- Support for Special Case ---
	 * where there is a contiguous edge ring with one isolated vertex.
	 *
	 * This example shows 2 edges created from 3 verts
	 * with 1 free standing vertex. Dotted lines denote the 2 edges that are created.
	 *
	 * note that this works for any sided shape.
	 *
	 * +--------+
	 * |        .
	 * |        .
	 * |        .
	 * |        .
	 * +........+ <-- starts out free standing.
	 *
	 */

	/* Here we check for consistancy and create 2 edges */
	if (totf == 0 && totv >= 4 && totv == tote + 2) {
		/* find a free standing vertex and 2 endpoint verts */
		BMVert *v_free = NULL, *v_a = NULL, *v_b = NULL;
		int ok = TRUE;


		BMO_ITER(v, &oiter, bm, op, "geom", BM_VERT) {
			/* count how many flagged edges this vertex uses */
			int tot_edges = 0;
			BM_ITER(e, &iter, bm, BM_EDGES_OF_VERT, v) {
				if (BMO_elem_flag_test(bm, e, ELE_NEW)) {
					tot_edges++;
					if (tot_edges > 2) {
						break;
					}
				}
			}

			if (tot_edges == 0) {
				/* only accept 1 free vert */
				if (v_free == NULL)  v_free = v;
				else                 ok = FALSE;  /* only ever want one of these */
			}
			else if (tot_edges == 1) {
				if (v_a == NULL)       v_a = v;
				else if (v_b == NULL)  v_b = v;
				else                   ok = FALSE;  /* only ever want 2 of these */
			}
			else if (tot_edges == 2) {
				/* do nothing, regular case */
			}
			else {
				ok = FALSE; /* if a vertex has 3+ edge users then cancel - this is only simple cases */
			}

			if (ok == FALSE) {
				break;
			}
		}

		if (ok == TRUE && v_free && v_a && v_b) {
			e = BM_edge_create(bm, v_free, v_a, NULL, TRUE);
			BMO_elem_flag_enable(bm, e, ELE_NEW);

			e = BM_edge_create(bm, v_free, v_b, NULL, TRUE);
			BMO_elem_flag_enable(bm, e, ELE_NEW);
		}
	}
	/* --- end special case support, continue as normal --- */

	/* call edgenet create */
	/* call edgenet prepare op so additional face creation cases wore */
	BMO_op_initf(bm, &op2, "edgenet_prepare edges=%fe", ELE_NEW);
	BMO_op_exec(bm, &op2);
	BMO_slot_buffer_flag_enable(bm, &op2, "edgeout", BM_EDGE, ELE_NEW);
	BMO_op_finish(bm, &op2);

	BMO_op_initf(bm, &op2, "edgenet_fill edges=%fe use_fill_check=%b", ELE_NEW, TRUE);
	BMO_op_exec(bm, &op2);

	/* return if edge net create did something */
	if (BMO_slot_buffer_count(bm, &op2, "faceout")) {
		BMO_slot_copy(&op2, op, "faceout", "faceout");
		BMO_op_finish(bm, &op2);
		return;
	}

	BMO_op_finish(bm, &op2);
	
	/* now call dissolve face */
	BMO_op_initf(bm, &op2, "dissolve_faces faces=%ff", ELE_NEW);
	BMO_op_exec(bm, &op2);
	
	/* if we dissolved anything, then return */
	if (BMO_slot_buffer_count(bm, &op2, "regionout")) {
		BMO_slot_copy(&op2, op, "regionout", "faceout");
		BMO_op_finish(bm, &op2);
		return;
	}

	BMO_op_finish(bm, &op2);

	/* now, count how many verts we have */
	amount = 0;
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		if (BMO_elem_flag_test(bm, v, ELE_NEW)) {
			verts[amount] = v;
			amount++;

			if (amount > 4) break;
		}
	}

	if (amount == 2) {
		/* create edge */
		e = BM_edge_create(bm, verts[0], verts[1], NULL, TRUE);
		BMO_elem_flag_enable(bm, e, ELE_OUT);
	}
	else if (0) { /* nice feature but perhaps it should be a different tool? */

		/* tricky feature for making a line/edge from selection history...
		 *
		 * Rather then do nothing, when 5+ verts are selected, check if they are in our history,
		 * when this is so, we can make edges from them, but _not_ a face,
		 * if it is the intention to make a face the user can just hit F again since there will be edges next
		 * time around.
		 *
		 * if all history verts have ELE_NEW flagged and the total number of history verts == totv,
		 * then we know the history contains all verts here and we can continue...
		 */

		BMEditSelection *ese;
		int tot_ese_v = 0;

		for (ese = bm->selected.first; ese; ese = ese->next) {
			if (ese->htype == BM_VERT) {
				if (BMO_elem_flag_test(bm, (BMElemF *)ese->ele, ELE_NEW)) {
					tot_ese_v++;
				}
				else {
					/* unflagged vert means we are not in sync */
					tot_ese_v = -1;
					break;
				}
			}
		}

		if (tot_ese_v == totv) {
			BMVert *v_prev = NULL;
			/* yes, all select-history verts are accounted for, now make edges */

			for (ese = bm->selected.first; ese; ese = ese->next) {
				if (ese->htype == BM_VERT) {
					v = (BMVert *)ese->ele;
					if (v_prev) {
						e = BM_edge_create(bm, v, v_prev, NULL, TRUE);
						BMO_elem_flag_enable(bm, e, ELE_OUT);
					}
					v_prev = v;
				}
			}
		}
		/* done creating edges */
	}
	else if (amount > 2) {
		/* TODO, all these verts may be connected by edges.
		 * we should check on this before assuming they are a random set of verts */

		BMVert **vert_arr = MEM_mallocN(sizeof(BMVert **) * totv, __func__);
		int i = 0;

		BMO_ITER(v, &oiter, bm, op, "geom", BM_VERT) {
			vert_arr[i] = v;
			i++;
		}

		f = BM_face_create_ngon_vcloud(bm, vert_arr, totv, TRUE);

		if (f) {
			BMO_elem_flag_enable(bm, f, ELE_OUT);
		}

		MEM_freeN(vert_arr);
	}
}
