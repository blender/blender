#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_memarena.h"
#include "BLI_mempool.h"
#include "BLI_heap.h"
#include "BLI_ghash.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_smallhash.h"
#include "BLI_rand.h"

#include "bmesh.h"
#include "bmesh_operators_private.h"

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
	
	struct {
		struct BMEdge *next, *prev;
	} dlink1;
	struct {
		struct BMEdge *next, *prev;
	} dlink2;
} EdgeData;

typedef struct VertData {
	BMEdge *e;
	float no[3], offco[3], sco[3]; /*offco is vertex coordinate slightly offset randomly*/
	int tag;
} VertData;

static int count_edge_faces(BMesh *bm, BMEdge *e);

/****  rotation system code ***/

#define rs_get_edge_link(e, v, ed) (Link*)((v) == ((BMEdge*)(e))->v1 ? &(((EdgeData*)(ed))->dlink1) : &(((EdgeData*)(ed))->dlink2))

int rotsys_append_edge(struct BMEdge *e, struct BMVert *v, 
						EdgeData *edata, VertData *vdata)
{
	EdgeData *ed = &edata[BMINDEX_GET(e)];
	VertData *vd = &vdata[BMINDEX_GET(v)];
	
	if (!vd->e) {
		Link *e1 = (Link*)rs_get_edge_link(e, v, ed);

		vd->e = e;
		e1->next = e1->prev = (Link*)e;
	} else {
		Link *e1, *e2, *e3;
		EdgeData *ved = &edata[BMINDEX_GET(vd->e)];

		e1 = rs_get_edge_link(e, v, ed);
		e2 = rs_get_edge_link(vd->e, v, ved);
		e3 = e2->prev ? rs_get_edge_link(e2->prev, v, &edata[BMINDEX_GET(e2->prev)]) : NULL;

		e1->next = (Link*)vd->e;
		e1->prev = e2->prev;

		e2->prev = (Link*)e;
		if (e3)
			e3->next = (Link*)e;
	}

	return 1;
}

void rotsys_remove_edge(struct BMEdge *e, struct BMVert *v, 
						EdgeData *edata, VertData *vdata)
{
	EdgeData *ed = edata + BMINDEX_GET(e);
	VertData *vd = vdata + BMINDEX_GET(v);
	Link *e1, *e2;

	e1 = rs_get_edge_link(e, v, ed);
	if (e1->prev) {
		e2 = rs_get_edge_link(e1->prev, v, ed);
		e2->next = e1->next;
	}

	if (e1->next) {
		e2 = rs_get_edge_link(e1->next, v, ed);
		e2->prev = e1->prev;
	}

	if (vd->e == e)
		vd->e = e!=e1->next ? (BMEdge*)e1->next : NULL;

	e1->next = e1->prev = NULL;
}

struct BMEdge *rotsys_nextedge(struct BMEdge *e, struct BMVert *v, 
									EdgeData *edata, VertData *vdata)
{
	if (v == e->v1)
		return edata[BMINDEX_GET(e)].dlink1.next;
	if (v == e->v2)
		return edata[BMINDEX_GET(e)].dlink2.next;
	return NULL;
}

BMEdge *rotsys_prevedge(BMEdge *e, BMVert *v, 
						EdgeData *edata, VertData *vdata)
{
	if (v == e->v1)
		return edata[BMINDEX_GET(e)].dlink1.prev;
	if (v == e->v2)
		return edata[BMINDEX_GET(e)].dlink2.prev;
	return NULL;
}

struct BMEdge *rotsys_reverse(struct BMEdge *e, struct BMVert *v, EdgeData *edata, VertData *vdata)
{
	BMEdge **edges = NULL;
	BMEdge *e2;
	BLI_array_staticdeclare(edges, 256);
	int i, totedge;
	
	e2 = vdata[BMINDEX_GET(v)].e;
	do {
		BLI_array_append(edges, e2);
		e2 = rotsys_nextedge(e2, v, edata, vdata);
	} while (e2 != vdata[BMINDEX_GET(v)].e);
	
	totedge = BLI_array_count(edges);
	for (i=0; i<totedge/2; i++) {
		SWAP(BMEdge*, edges[i], edges[totedge-1-i]);
	}
	
	vdata[BMINDEX_GET(v)].e = NULL;
	for (i=0; i<totedge; i++) {
		rotsys_append_edge(edges[i], v, edata, vdata);
	}
	
	BLI_array_free(edges);
}

int rotsys_count(struct BMVert *v, EdgeData *edata, VertData *vdata)
{
	BMEdge *e = vdata[BMINDEX_GET(v)].e;
	int i=0;

	if (!e)
		return 0;

	do {
		if (!e)
			return 0;
		e =  rotsys_nextedge(e, v, edata, vdata);

		if (i >= (1<<20)) {
			printf("bmesh error: infinite loop in disk cycle!\n");
			return 0;
		}

		i += 1;
	} while (e != vdata[BMINDEX_GET(v)].e);

	return i;
}

int rotsys_fill_faces(BMesh *bm, EdgeData *edata, VertData *vdata)
{
	BMIter iter;
	BMEdge *e, **edges = NULL;
	BLI_array_declare(edges);
	BMVert *v, **verts = NULL;
	BMFace *f;
	BLI_array_declare(verts);
	SmallHash visithash, *hash=&visithash;
	int i;
	
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		BMEdge *e2, *starte;
		BMVert *startv;
		int rad, ok;
		
		rad = count_edge_faces(bm, e);
		
		if (rad < 2)
			starte = e;
		else
			continue;
		
		/*do two passes, going forward then backward*/
		for (i=0; i<2; i++) {
			BLI_smallhash_init(hash);
			
			BLI_array_empty(verts);
			BLI_array_empty(edges);

			startv = v = starte->v1;
			e2 = starte;
			ok = 1;
			if (!v || !e2)
				continue;
				
			do {
				if (BLI_smallhash_haskey(hash, (intptr_t)e2) 
				 || BLI_smallhash_haskey(hash, (intptr_t)v)) {
					ok = 0;
					break;
				}
				
				BLI_array_append(verts, v);
				BLI_array_append(edges, e2);
				
				BLI_smallhash_insert(hash, (intptr_t)e2, NULL);
	
				v = BM_OtherEdgeVert(e2, v);
				e2 = i ? rotsys_prevedge(e2, v, edata, vdata) : rotsys_nextedge(e2, v, edata, vdata);
			} while (e2 != starte && v != startv);
			
			BLI_smallhash_release(hash);
			
			if (!ok || BLI_array_count(edges) < 3)
				continue;
			
			f = BM_Make_Ngon(bm, verts[0], verts[1], edges, BLI_array_count(edges), 1);
			if (!f)
				continue;
		}
	}
}

void rotsys_make_consistent(BMesh *bm, EdgeData *edata, VertData *vdata)
{
	BMIter iter;
	BMEdge *e;
	BMVert *v, **stack=NULL;
	BLI_array_declare(stack);
	int i;
	
	for (i=0; i<bm->totvert; i++) {
		vdata[i].tag = 0;
	}
	
	while (1) {
		VertData *vd;
		BMVert *startv = NULL;
		float dis;
		
		v = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL);
		for (i=0; i<bm->totvert; i++, BMIter_Step(&iter)) {
			vd = vdata + BMINDEX_GET(v);
			
			if (vd->tag)
				continue;
			
			if (!startv || dot_v3v3(vd->offco, vd->offco) > dis) {
				dis = dot_v3v3(vd->offco, vd->offco);
				startv = v;
			}
		}
		
		if (!startv)
			break;
		
		vd = vdata + BMINDEX_GET(startv);
		
		BLI_array_empty(stack);
		BLI_array_append(stack, startv);
		
		vd->tag = 1;
		
		while (BLI_array_count(stack)) {
			v = BLI_array_pop(stack);
			vd = vdata + BMINDEX_GET(v);
			
			if (!vd->e)
				continue;
			
			e = vd->e;
			do {
				BMVert *v2 = BM_OtherEdgeVert(e, v);
				VertData *vd2 = vdata + BMINDEX_GET(v2);
				
				if (dot_v3v3(vd->no, vd2->no) < 0.0f+FLT_EPSILON*2) {
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
	
}

void init_rotsys(BMesh *bm, EdgeData *edata, VertData *vdata)
{
	BMIter iter;
	BMEdge *e;
	BMEdge **edges = NULL;
	BLI_array_staticdeclare(edges, 256);
	BMVert *v, *lastv, **verts = NULL;
	BLI_array_staticdeclare(verts, 256);
	int i;
	
	#define SIGN(n) ((n)<0.0f)
	
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BMIter eiter;
		float no[3], cent[3];
		int j, k=0, totedge=0;
		
		if (BMINDEX_GET(v) == -1)
			continue;
		
		BLI_array_empty(edges);
		
		BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
			if (BMO_TestFlag(bm, e, EDGE_MARK)) {
				BLI_array_append(edges, e);
				totedge++;
			}
		}
		
		copy_v3_v3(cent, v->co);
		
		zero_v3(no);
		for (i=0; i<totedge; i++) {
			BMEdge *e1, *e2;
			float cno[3], vec1[3], vec2[3];
			
			e1 = edges[i];
			e2 = edges[(i+1)%totedge];

			sub_v3_v3v3(vec1, (BM_OtherEdgeVert(e1, v))->co, v->co);
			sub_v3_v3v3(vec2, (BM_OtherEdgeVert(e2, v))->co, v->co);
			
			cross_v3_v3v3(cno, vec1, vec2);
			normalize_v3(cno);
			
			if (i && dot_v3v3(cno, no) < 0.0f+FLT_EPSILON*10)
				mul_v3_fl(cno, -1.0f);
			
			add_v3_v3(no, cno);
			normalize_v3(no);
		}
		
		/*generate plane-flattened coordinates*/
		for (i=0; i<totedge; i++) {
			BMEdge *e1;
			BMVert *v2;
			float cvec[3], vec1[3];
			
			e1 = edges[i];
			v2 = BM_OtherEdgeVert(e1, v);
			
			sub_v3_v3v3(vec1, v2->co, v->co);
			
			cross_v3_v3v3(cvec, vec1, no);
			cross_v3_v3v3(vec1, cvec, no);
			normalize_v3(vec1);
			
			mul_v3_fl(vec1, len_v3v3(v2->co, v->co));
			add_v3_v3(vec1, v->co);
			
			copy_v3_v3(vdata[BMINDEX_GET(v2)].sco, vec1);
		}
		
		/*first, ensure no 0 or 180 angles between adjacent
		  (and that adjacent's adjacent) edges*/
		for (i=0, k=0; i<totedge; i++) {
			BMEdge *e1, *e2, *e3=NULL;
			BMVert *v1, *v2, *v3;
			VertData *vd1, *vd2, *vd3;
			float vec1[3], vec2[3], vec3[3], size;
			int s1, s2, s3;
			
			if (totedge < 3)
				continue;
			
			e1 = edges[(i+totedge-1) % totedge];
			e2 = edges[i];
			e3 = edges[(i+1) % totedge];
			
			v1 = BM_OtherEdgeVert(e1, v); v2 = BM_OtherEdgeVert(e2, v); v3 = BM_OtherEdgeVert(e3, v);
			vd1 = vdata+BMINDEX_GET(v1); vd2 = vdata+BMINDEX_GET(v2); vd3 = vdata+BMINDEX_GET(v3);
			
			sub_v3_v3v3(vec1, vd1->sco, cent);
			sub_v3_v3v3(vec2, vd2->sco, cent);
			sub_v3_v3v3(vec3, vd3->sco, cent);
			
			size = (len_v3(vec1) + len_v3(vec3))*0.01;
			normalize_v3(vec1); normalize_v3(vec2); normalize_v3(vec3);
			
			#ifdef STRAIGHT
			#undef STRAIGHT
			#endif
			#define STRAIGHT(vec11, vec22) (fabs(dot_v3v3((vec11), (vec22))) > 1.0-FLT_EPSILON*1000)
			
			s1 = STRAIGHT(vec1, vec2); s2 = STRAIGHT(vec2, vec3); s3 = STRAIGHT(vec1, vec3);
			
			if (s1 || s2 || s3) {
				copy_v3_v3(cent, v->co);

				for (j=0; j<3; j++) {
					float fac = (BLI_frand()-0.5f)*size;
					cent[j] += fac;
				}
				
				if (k < 2000) {
					i = 0;
					k++;
					continue;
				} else {
					k++;
					continue;
				}

			}
		}
		
		copy_v3_v3(vdata[BMINDEX_GET(v)].offco, cent);
		copy_v3_v3(v->co, cent);
		
		/*now, sort edges so the triangle fan of all edges
		  has a consistent normal.  this is the same as
		  sorting by polar coordinates along a group normal*/
		for (j=0; j<totedge; j++) {
			for (i=0; i<totedge; i++) {
				BMEdge *e1, *e2, *e3=NULL;
				BMVert *v1, *v2, *v3;
				VertData *vd1, *vd2, *vd3;
				float vec1[3], vec2[3], vec3[3], n1[3], n2[3], n3[3];
				int s1, s2, s3;
				
				e1 = edges[(i+totedge-1) % totedge];
				e2 = edges[i];
				e3 = edges[(i+1) % totedge];
				
				v1 = BM_OtherEdgeVert(e1, v); v2 = BM_OtherEdgeVert(e2, v); v3 = BM_OtherEdgeVert(e3, v);
				vd1 = vdata+BMINDEX_GET(v1); vd2 = vdata+BMINDEX_GET(v2); vd3 = vdata+BMINDEX_GET(v3);
	
				sub_v3_v3v3(vec1, vd1->sco, cent);
				sub_v3_v3v3(vec2, vd2->sco, cent);
				sub_v3_v3v3(vec3, vd3->sco, cent);
				
				cross_v3_v3v3(n1, vec1, vec2);
				cross_v3_v3v3(n2, vec2, vec3);
				cross_v3_v3v3(n3, vec1, vec3);
				
				normalize_v3(n1); normalize_v3(n2); normalize_v3(n3);
				
				s1 = STRAIGHT(vec1, vec2); s2 = STRAIGHT(vec2, vec3); s3 = STRAIGHT(vec1, vec3);
								
				if (s1 || s2 || s3) {
					printf("yeek! s1: %d, s2: %d, s3: %dx\n", s1, s2, s3);
				}
				if (dot_v3v3(n1, n2) < 0.0f) {
					if (dot_v3v3(n1, n3) >= 0.0f + FLT_EPSILON*10) {
						SWAP(BMEdge*, edges[i], edges[(i+1)%totedge]);
					} else {
						SWAP(BMEdge*, edges[(i+totedge-1)%totedge], edges[(i+1)%totedge])
						SWAP(BMEdge*, edges[i], edges[(i+1)%totedge])
					}
				} 
			}
		}
		
		#undef STRAIGHT
				
		zero_v3(no);
		
		/*yay, edges is sorted*/
		for (i=0; i<totedge; i++) {
			BMEdge *e1 = edges[i], *e2 = edges[(i+1)%totedge];
			float eno[3];
			
			normal_tri_v3(eno, BM_OtherEdgeVert(e1, v)->co, v->co, BM_OtherEdgeVert(e2, v)->co);
			add_v3_v3(no, eno);
			
			rotsys_append_edge(edges[i], v, edata, vdata);
		}
		
		normalize_v3(no);
		copy_v3_v3(vdata[BMINDEX_GET(v)].no, no);
	}
	
	/*now, make sure rotation system is topologically consistent
	  (e.g. vert normals consistently point either inside or outside)*/
	rotsys_make_consistent(bm, edata, vdata);

	//rotsys_fill_faces(bm, edata, vdata);

#if 0
	/*create visualizing geometry*/
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BMVert *v2;
		BMFace *f;
		int totedge = BM_Vert_EdgeCount(v);

		if (BMINDEX_GET(v) == -1)
			continue;
		
		//cv = BM_Make_Vert(bm, cent, v);
		//BMINDEX_SET(cv, -1);
		i = 0;
		e = vdata[BMINDEX_GET(v)].e;
		lastv = NULL;
		do {
			BMEdge *e2;
			BMVert *v2;
			float f = ((float)i / (float)totedge)*0.35 + 0.05;
			float co[3];
			
			if (!e)
				break;
				
			if (!BM_OtherEdgeVert(e, v))
				continue;
			
			sub_v3_v3v3(co, (BM_OtherEdgeVert(e, v))->co, vdata[BMINDEX_GET(v)].offco);
			mul_v3_fl(co, f);
			add_v3_v3(co, vdata[BMINDEX_GET(v)].offco);
			
			v2 = BM_Make_Vert(bm, co, NULL);
			BMINDEX_SET(v2, -1);
			//BM_Make_Edge(bm, cv, v2, NULL, 0);
			
			BM_Select(bm, v2, 1);
			if (lastv) {
				e2 =
				 BM_Make_Edge(bm, lastv, v2, NULL, 0);
				BM_Select(bm, e2, 1);
			}
			
			lastv = v2;
			
			e = rotsys_nextedge(e, v, edata, vdata);
			i++;
		} while (e != vdata[BMINDEX_GET(v)].e);
	}
#endif

	BLI_array_free(edges);
}

PathBase *edge_pathbase_new(void)
{
	PathBase *pb = MEM_callocN(sizeof(PathBase), "PathBase");

	pb->nodepool = BLI_mempool_create(sizeof(EPathNode), 1, 512, 1, 0);
	pb->pathpool = BLI_mempool_create(sizeof(EPath), 1, 512, 1, 0);

	return pb;
}

void edge_pathbase_free(PathBase *pathbase)
{
	BLI_mempool_destroy(pathbase->nodepool);
	BLI_mempool_destroy(pathbase->pathpool);
	MEM_freeN(pathbase);
}

EPath *edge_copy_add_path(PathBase *pb, EPath *path, BMVert *appendv, BMEdge *e)
{
	EPath *path2;
	EPathNode *node, *node2;

	path2 = BLI_mempool_alloc(pb->pathpool);
	path2->nodes.first = path2->nodes.last = NULL;
	path2->weight = 0.0f;
	path2->group = path->group;
	
	for (node=path->nodes.first; node; node=node->next) {
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

EPath *edge_path_new(PathBase *pb, BMVert *start, BMEdge *starte)
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

float edge_weight_path(EPath *path, EdgeData *edata, VertData *vdata)
{
	EPathNode *node, *first=path->nodes.first;
	float w = 0.0;

	for (node=path->nodes.first; node; node=node->next) {
		if (node->e && node != path->nodes.first) {
			w += edata[BMINDEX_GET(node->e)].ftag;
			if (node->prev) {
				//w += len_v3v3(node->v->co, first->e->v1->co)*0.0001f;
				//w += len_v3v3(node->v->co, first->e->v2->co)*0.0001f;				
			}
		}

		w += 1.0f;
	}

	return w;
}


void edge_free_path(PathBase *pathbase, EPath *path)
{
	EPathNode *node, *next;

	for (node=path->nodes.first; node; node=next) {
		next = node->next;
		BLI_mempool_free(pathbase->nodepool, node);
	}

	BLI_mempool_free(pathbase->pathpool, path);
}

EPath *edge_find_shortest_path(BMesh *bm, BMOperator *op, BMEdge *edge, EdgeData *edata, 
								VertData *vdata, PathBase *pathbase, int group)
{
	BMEdge *e, *starte;
	GHash *gh = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "createops find shortest path");
	BMVert *v1, *v2;
	BMVert **verts = NULL;
	BLI_array_staticdeclare(verts, 1024);
	Heap *heap = BLI_heap_new();
	EPath *path = NULL, *path2;
	BMVert *startv;
	BMVert *endv;
	EPathNode *node;
	int i, use_restrict = BMO_Get_Int(op, "use_restrict");

	startv = edata[BMINDEX_GET(edge)].ftag ? edge->v2 : edge->v1;
	endv = edata[BMINDEX_GET(edge)].ftag ? edge->v1 : edge->v2;
	
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
			/*make sure this path loop doesn't already exist*/
			i = 0;
			BLI_array_empty(verts);
			for (i=0, node = path->nodes.first; node; node=node->next, i++) {
				BLI_array_growone(verts);
				verts[i] = node->v;
			}

			if (BM_Face_Exists(bm, verts, i, &f)) {
				if (!BMO_TestFlag(bm, f, FACE_IGNORE)) {
					BLI_ghash_remove(gh, endv, NULL, NULL);
					continue;
				}
			}
			break;
		}
		
		vd = vdata + BMINDEX_GET(v1);
		if (!vd->e)
			continue;
		
		v2 = NULL;
		while (1) {		
			if (!last->cure) {
				last->cure = e = vdata[BMINDEX_GET(last->v)].e;
			} else {
				last->cure = e = rotsys_nextedge(last->cure, last->v, edata, vdata);
				if (last->cure == vdata[BMINDEX_GET(last->v)].e) {
					v2 = NULL;
					break;
				}
			}
			
			if (e == edge || !BMO_TestFlag(bm, e, EDGE_MARK)) {
				continue;
			}
				
			v2 = BM_OtherEdgeVert(e, last->v);
			
			if (BLI_ghash_haskey(gh, v2)) {
				v2 = NULL;
				continue;
			}
			
			if (use_restrict && BMO_InMap(bm, op, "restrict", e)) {
				int group = BMO_Get_MapInt(bm, op, "restrict", e);
				
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
		
		/*add path back into heap*/
		BLI_heap_insert(heap, path->weight, path);
		
		/*put v2 in gh map*/
		BLI_ghash_insert(gh, v2, NULL);

		path2 = edge_copy_add_path(pathbase, path, v2, e);
		path2->weight = edge_weight_path(path2, edata, vdata);

		BLI_heap_insert(heap, path2->weight, path2);
	}
	
	if (path && ((EPathNode*)path->nodes.last)->v != endv) {
		edge_free_path(pathbase, path);
		path = NULL;
	}
		
	BLI_array_free(verts);
	BLI_heap_free(heap, NULL);
	BLI_ghash_free(gh, NULL, NULL);

	return path;
}

static int count_edge_faces(BMesh *bm, BMEdge *e) {
	int i=0;
	BMLoop *l = e->l;
	
	if (!l)
		return 0;
	
	do {
		if (!BMO_TestFlag(bm, l->f, FACE_IGNORE))
			i++;

		l = l->radial_next;
	} while (l != e->l);
	
	return i;
}

void bmesh_edgenet_fill_exec(BMesh *bm, BMOperator *op)
{
	BMIter iter;
	BMOIter siter;
	BMFace *f;
	BMEdge *e, *edge;
	BMVert *v, **verts = NULL;
	BLI_array_declare(verts);
	EPath *path;
	EPathNode *node;
	EdgeData *edata;
	VertData *vdata;
	BMEdge **edges = NULL;
	PathBase *pathbase = edge_pathbase_new();
	BLI_array_declare(edges);
	int use_restrict = BMO_Get_Int(op, "use_restrict");
	int i, j, group = 0;

	if (!bm->totvert || !bm->totedge)
		return;

	edata = MEM_callocN(sizeof(EdgeData)*bm->totedge, "EdgeData");
	vdata = MEM_callocN(sizeof(VertData)*bm->totvert, "VertData");
	
	BMO_Flag_Buffer(bm, op, "edges", EDGE_MARK, BM_EDGE);
	BMO_Flag_Buffer(bm, op, "excludefaces", FACE_IGNORE, BM_FACE);
	
	i = 0;
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BMINDEX_SET(v, i);
		i++;	
	}

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		BMO_SetFlag(bm, f, ELE_ORIG);
	}

	i = 0;
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		BMINDEX_SET(e, i);
		
		if (!BMO_TestFlag(bm, e, EDGE_MARK)) {
			edata[i].tag = 2;
		}

		i += 1;
	}

	init_rotsys(bm, edata, vdata);
	
	while (1) {
		edge = NULL;
		group = 0;
		
		BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
			/*if restrict is on, only start on faces in the restrict map*/
			if (use_restrict && !BMO_InMap(bm, op, "restrict", e))
				continue;
				
			if (edata[BMINDEX_GET(e)].tag < 2) {
				edge = e;

	 			if (use_restrict) {
					int i=0, j=0, gi=0;
					
					group = BMO_Get_MapInt(bm, op, "restrict", e);				
					
					for (i=0; i<30; i++) {
						if (group & (1<<i)) {
							j++;
							gi = i;

							if (j-1 == edata[BMINDEX_GET(e)].tag)
								break;
						}
					}
					
					group = 1<<gi;
				}
				
				break;
			}
		}

		if (!edge)
			break;

		edata[BMINDEX_GET(edge)].tag += 1;

		path = edge_find_shortest_path(bm, op, edge, edata, vdata, pathbase, group);
		if (!path)
			continue;
		
		BLI_array_empty(edges);
		BLI_array_empty(verts);
		i = 0;
		for (node=path->nodes.first; node; node=node->next) {
			if (!node->next)
				continue;

			e = BM_Edge_Exist(node->v, node->next->v);
			
			/*this should never happen*/
			if (!e)
				break;
			
			edata[BMINDEX_GET(e)].ftag++;
			BLI_array_growone(edges);
			edges[i++] = e;

			BLI_array_append(verts, node->v);
		}
		
		BLI_array_growone(edges);
		edges[i++] = edge;
		edata[BMINDEX_GET(edge)].ftag++;
		
		for (j=0; j<i; j++) {
			if (count_edge_faces(bm, edges[j]) >= 2) {			
				edge_free_path(pathbase, path);
				break;
			}
		}
		
		if (j != i)
			continue;
		
		if (i) {
			f = BM_Make_Ngon(bm, edge->v1, edge->v2, edges, i, 1);
			if (f && !BMO_TestFlag(bm, f, ELE_ORIG)) {
				BMO_SetFlag(bm, f, FACE_NEW);
			}

			if (use_restrict)
				BMO_Insert_MapInt(bm, op, "faceout_groupmap", f, path->group);
		}
		
		edge_free_path(pathbase, path);
	}

	BMO_Flag_To_Slot(bm, op, "faceout", FACE_NEW, BM_FACE);

	BLI_array_free(edges);
	BLI_array_free(verts);
	edge_pathbase_free(pathbase);
	MEM_freeN(edata);
	MEM_freeN(vdata);
}

/* evaluate if entire quad is a proper convex quad */
static int convex(float *v1, float *v2, float *v3, float *v4)
{
	float nor[3], nor1[3], nor2[3], vec[4][2];
	
	/* define projection, do both trias apart, quad is undefined! */
	normal_tri_v3( nor1,v1, v2, v3);
	normal_tri_v3( nor2,v1, v3, v4);
	nor[0]= ABS(nor1[0]) + ABS(nor2[0]);
	nor[1]= ABS(nor1[1]) + ABS(nor2[1]);
	nor[2]= ABS(nor1[2]) + ABS(nor2[2]);

	if(nor[2] >= nor[0] && nor[2] >= nor[1]) {
		vec[0][0]= v1[0]; vec[0][1]= v1[1];
		vec[1][0]= v2[0]; vec[1][1]= v2[1];
		vec[2][0]= v3[0]; vec[2][1]= v3[1];
		vec[3][0]= v4[0]; vec[3][1]= v4[1];
	}
	else if(nor[1] >= nor[0] && nor[1]>= nor[2]) {
		vec[0][0]= v1[0]; vec[0][1]= v1[2];
		vec[1][0]= v2[0]; vec[1][1]= v2[2];
		vec[2][0]= v3[0]; vec[2][1]= v3[2];
		vec[3][0]= v4[0]; vec[3][1]= v4[2];
	}
	else {
		vec[0][0]= v1[1]; vec[0][1]= v1[2];
		vec[1][0]= v2[1]; vec[1][1]= v2[2];
		vec[2][0]= v3[1]; vec[2][1]= v3[2];
		vec[3][0]= v4[1]; vec[3][1]= v4[2];
	}
	
	/* linetests, the 2 diagonals have to instersect to be convex */
	if( isect_line_line_v2(vec[0], vec[2], vec[1], vec[3]) > 0 ) return 1;
	return 0;
}

BMEdge *edge_next(BMesh *bm, BMEdge *e)
{
	BMIter iter;
	BMEdge *e2;
	int i;

	for (i=0; i<2; i++) {
		BM_ITER(e2, &iter, bm, BM_EDGES_OF_VERT, i?e->v2:e->v1) {
			if (BMO_TestFlag(bm, e2, EDGE_MARK) 
			    && !BMO_TestFlag(bm, e2, EDGE_VIS) && e2 != e)
			{
				return e2;
			}
		}
	}

	return NULL;
}

void bmesh_edgenet_prepare(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMEdge *e, *e2;
	BMEdge **edges1 = NULL, **edges2 = NULL, **edges;
	BLI_array_declare(edges1);
	BLI_array_declare(edges2);
	BLI_array_declare(edges);
	int ok = 1;
	int i, count;

	BMO_Flag_Buffer(bm, op, "edges", EDGE_MARK, BM_EDGE);
	
	/*validate that each edge has at most one other tagged edge in the
	  disk cycle around each of it's vertices*/
	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		for (i=0; i<2; i++) {
			count = BMO_Vert_CountEdgeFlags(bm, i?e->v2:e->v1, EDGE_MARK);
			if (count > 2) {
				ok = 0;
				break;
			}
		}

		if (!ok) break;
	}

	/*we don't have valid edge layouts, return*/
	if (!ok)
		return;


	/*find connected loops within the input edges*/
	count = 0;
	while (1) {
		BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
			if (!BMO_TestFlag(bm, e, EDGE_VIS)) {
				if (BMO_Vert_CountEdgeFlags(bm, e->v1, EDGE_MARK)==1)
					break;
				if (BMO_Vert_CountEdgeFlags(bm, e->v2, EDGE_MARK)==1)
					break;
			}
		}
		
		if (!e) break;
		
		if (!count)
			edges = edges1;
		else if (count==1)
			edges = edges2;
		else break;
		
		i = 0;
		while (e) {
			BMO_SetFlag(bm, e, EDGE_VIS);
			BLI_array_growone(edges);
			edges[i] = e;

			e = edge_next(bm, e);
			i++;
		}

		if (!count) {
			edges1 = edges;
			BLI_array_set_length(edges1, BLI_array_count(edges));
		} else {
			edges2 = edges;
			BLI_array_set_length(edges2, BLI_array_count(edges));
		}

		BLI_array_empty(edges);
		count++;
	}

#define EDGECON(e1, e2) (e1->v1 == e2->v1 || e1->v2 == e2->v2 || e1->v1 == e2->v2)

	if (edges1 && BLI_array_count(edges1) > 2 && EDGECON(edges1[0], edges1[BLI_array_count(edges1)-1])) {
		if (edges2 && BLI_array_count(edges2) > 2 && EDGECON(edges2[0], edges2[BLI_array_count(edges2)-1])) {
			BLI_array_free(edges1);
			BLI_array_free(edges2);
			return;
		} else {
			edges1 = edges2;
			edges2 = NULL;
		}
	}

	if (edges2 && BLI_array_count(edges2) > 2 && EDGECON(edges2[0], edges2[BLI_array_count(edges2)-1])) {
		edges2 = NULL;
	}

	/*two unconnected loops, connect them*/
	if (edges1 && edges2) {
		BMVert *v1, *v2, *v3, *v4;

		if (BLI_array_count(edges1)==1) {
			v1 = edges1[0]->v1;
			v2 = edges1[0]->v2;
		} else {
			if (BM_Vert_In_Edge(edges1[1], edges1[0]->v1))
				v1 = edges1[0]->v2;
			else v1 = edges1[0]->v1;

			i = BLI_array_count(edges1)-1;
			if (BM_Vert_In_Edge(edges1[i-1], edges1[i]->v1))
				v2 = edges1[i]->v2;
			else v2 = edges1[i]->v1;
		}

		if (BLI_array_count(edges2)==1) {
			v3 = edges2[0]->v1;
			v4 = edges2[0]->v2;
		} else {
			if (BM_Vert_In_Edge(edges2[1], edges2[0]->v1))
				v3 = edges2[0]->v2;
			else v3 = edges2[0]->v1;

			i = BLI_array_count(edges2)-1;
			if (BM_Vert_In_Edge(edges2[i-1], edges2[i]->v1))
				v4 = edges2[i]->v2;
			else v4 = edges2[i]->v1;
		}

		if (len_v3v3(v1->co, v3->co) > len_v3v3(v1->co, v4->co)) {
			BMVert *v;
			v = v3;
			v3 = v4;
			v4 = v;
		}

		e = BM_Make_Edge(bm, v1, v3, NULL, 1);
		BMO_SetFlag(bm, e, ELE_NEW);
		e = BM_Make_Edge(bm, v2, v4, NULL, 1);
		BMO_SetFlag(bm, e, ELE_NEW);
	} else if (edges1) {
		BMVert *v1, *v2;
		
		if (BLI_array_count(edges1) > 1) {
			if (BM_Vert_In_Edge(edges1[1], edges1[0]->v1))
				v1 = edges1[0]->v2;
			else v1 = edges1[0]->v1;

			i = BLI_array_count(edges1)-1;
			if (BM_Vert_In_Edge(edges1[i-1], edges1[i]->v1))
				v2 = edges1[i]->v2;
			else v2 = edges1[i]->v1;

			e = BM_Make_Edge(bm, v1, v2, NULL, 1);
			BMO_SetFlag(bm, e, ELE_NEW);
		}
	}
	
	BMO_Flag_To_Slot(bm, op, "edgeout", ELE_NEW, BM_EDGE);

	BLI_array_free(edges1);
	BLI_array_free(edges2);

#undef EDGECON
}

/*this is essentially new fkey*/
void bmesh_contextual_create_exec(BMesh *bm, BMOperator *op)
{
	BMOperator op2;
	BMOIter oiter;
	BMIter iter;
	BMHeader *h;
	BMVert *v, *verts[4];
	BMEdge *e;
	BMFace *f;
	int totv=0, tote=0, totf=0, amount;

	/*count number of each element type we were passed*/
	BMO_ITER(h, &oiter, bm, op, "geom", BM_VERT|BM_EDGE|BM_FACE) {
		switch (h->type) {
			case BM_VERT: totv++; break;
			case BM_EDGE: tote++; break;
			case BM_FACE: totf++; break;
		}

		BMO_SetFlag(bm, h, ELE_NEW);
	}
	
	/*call edgenet create*/
	/*  call edgenet prepare op so additional face creation cases work*/
	BMO_InitOpf(bm, &op2, "edgenet_prepare edges=%fe", ELE_NEW);
	BMO_Exec_Op(bm, &op2);
	BMO_Flag_Buffer(bm, &op2, "edgeout", ELE_NEW, BM_EDGE);
	BMO_Finish_Op(bm, &op2);

	BMO_InitOpf(bm, &op2, "edgenet_fill edges=%fe", ELE_NEW);
	BMO_Exec_Op(bm, &op2);

	/*return if edge net create did something*/
	if (BMO_CountSlotBuf(bm, &op2, "faceout")) {
		BMO_CopySlot(&op2, op, "faceout", "faceout");
		BMO_Finish_Op(bm, &op2);
		return;
	}

	BMO_Finish_Op(bm, &op2);
	
	/*now call dissolve faces*/
	BMO_InitOpf(bm, &op2, "dissolvefaces faces=%ff", ELE_NEW);
	BMO_Exec_Op(bm, &op2);
	
	/*if we dissolved anything, then return.*/
	if (BMO_CountSlotBuf(bm, &op2, "regionout")) {
		BMO_CopySlot(&op2, op, "regionout", "faceout");
		BMO_Finish_Op(bm, &op2);
		return;
	}

	BMO_Finish_Op(bm, &op2);

	/*now, count how many verts we have*/
	amount = 0;
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		if (BMO_TestFlag(bm, v, ELE_NEW)) {
			verts[amount] = v;
			amount++;

			if (amount > 4) break;
		}
	}

	if (amount == 2) {
		/*create edge*/
		e = BM_Make_Edge(bm, verts[0], verts[1], NULL, 1);
		BMO_SetFlag(bm, e, ELE_OUT);		
	} else if (amount == 3) {
		/*create triangle*/
		BM_Make_QuadTri(bm, verts[0], verts[1], verts[2], NULL, NULL, 1);
	} else if (amount == 4) {
		f = NULL;

		/* the order of vertices can be anything, 6 cases to check */
		if( convex(verts[0]->co, verts[1]->co, verts[2]->co, verts[3]->co) ) {
			f= BM_Make_QuadTri(bm, verts[0], verts[1], verts[2], verts[3], NULL, 1);
		}
		else if( convex(verts[0]->co, verts[2]->co, verts[3]->co, verts[1]->co) ) {
			f= BM_Make_QuadTri(bm, verts[0], verts[2], verts[3], verts[1], NULL, 1);
		}
		else if( convex(verts[0]->co, verts[2]->co, verts[1]->co, verts[3]->co) ) {
			f= BM_Make_QuadTri(bm, verts[0], verts[2], verts[1], verts[3], NULL, 1);
		}
		else if( convex(verts[0]->co, verts[1]->co, verts[3]->co, verts[2]->co) ) {
			f= BM_Make_QuadTri(bm, verts[0], verts[1], verts[3], verts[2], NULL, 1);
		}
		else if( convex(verts[0]->co, verts[3]->co, verts[2]->co, verts[1]->co) ) {
			f= BM_Make_QuadTri(bm, verts[0], verts[3], verts[2], verts[1], NULL, 1);
		}
		else if( convex(verts[0]->co, verts[3]->co, verts[1]->co, verts[2]->co) ) {
			f= BM_Make_QuadTri(bm, verts[0], verts[3], verts[1], verts[2], NULL, 1);
		}
		else {
			printf("cannot find nice quad from concave set of vertices\n");
		}

		if (f) BMO_SetFlag(bm, f, ELE_OUT);
	}
}
