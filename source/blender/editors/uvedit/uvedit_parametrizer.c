/** \file blender/editors/uvedit/uvedit_parametrizer.c
 *  \ingroup eduv
 */

#include "MEM_guardedalloc.h"

#include "BLI_memarena.h"
#include "BLI_math.h"
#include "BLI_rand.h"
#include "BLI_heap.h"
#include "BLI_boxpack2d.h"
#include "BLI_utildefines.h"



#include "ONL_opennl.h"

#include "uvedit_intern.h"
#include "uvedit_parametrizer.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "BLO_sys_types.h" // for intptr_t support

#if defined(_WIN32)
#define M_PI 3.14159265358979323846
#endif

/* Utils */

#if 0
	#define param_assert(condition)
	#define param_warning(message)
	#define param_test_equals_ptr(condition)
	#define param_test_equals_int(condition)
#else
	#define param_assert(condition) \
		if (!(condition)) \
			{ /*printf("Assertion %s:%d\n", __FILE__, __LINE__); abort();*/ } (void)0
	#define param_warning(message) \
		{ /*printf("Warning %s:%d: %s\n", __FILE__, __LINE__, message);*/ } (void)0
	#define param_test_equals_ptr(str, a, b) \
		if (a != b) \
			{ /*printf("Equals %s => %p != %p\n", str, a, b);*/ } (void)0
	#define param_test_equals_int(str, a, b) \
		if (a != b) \
			{ /*printf("Equals %s => %d != %d\n", str, a, b);*/ } (void)0
#endif
typedef enum PBool {
	P_TRUE = 1,
	P_FALSE = 0
} PBool;

/* Special Purpose Hash */

typedef intptr_t PHashKey;

typedef struct PHashLink {
	struct PHashLink *next;
	PHashKey key;
} PHashLink;

typedef struct PHash {
	PHashLink **list;
	PHashLink **buckets;
	int size, cursize, cursize_id;
} PHash;



struct PVert;
struct PEdge;
struct PFace;
struct PChart;
struct PHandle;

/* Simplices */

typedef struct PVert {
	struct PVert *nextlink;

	union PVertUnion {
		PHashKey key;           /* construct */
		int id;                 /* abf/lscm matrix index */
		float distortion;       /* area smoothing */
		HeapNode *heaplink;     /* edge collapsing */
	} u;

	struct PEdge *edge;
	float co[3];
	float uv[2];
	unsigned char flag;

} PVert; 

typedef struct PEdge {
	struct PEdge *nextlink;

	union PEdgeUnion {
		PHashKey key;                   /* construct */
		int id;                         /* abf matrix index */
		HeapNode *heaplink;             /* fill holes */
		struct PEdge *nextcollapse;     /* simplification */
	} u;

	struct PVert *vert;
	struct PEdge *pair;
	struct PEdge *next;
	struct PFace *face;
	float *orig_uv, old_uv[2];
	unsigned short flag;

} PEdge;

typedef struct PFace {
	struct PFace *nextlink;

	union PFaceUnion {
		PHashKey key;           /* construct */
		int chart;              /* construct splitting*/
		float area3d;           /* stretch */
		int id;                 /* abf matrix index */
	} u;

	struct PEdge *edge;
	unsigned char flag;

} PFace;

enum PVertFlag {
	PVERT_PIN = 1,
	PVERT_SELECT = 2,
	PVERT_INTERIOR = 4,
	PVERT_COLLAPSE = 8,
	PVERT_SPLIT = 16
};

enum PEdgeFlag {
	PEDGE_SEAM = 1,
	PEDGE_VERTEX_SPLIT = 2,
	PEDGE_PIN = 4,
	PEDGE_SELECT = 8,
	PEDGE_DONE = 16,
	PEDGE_FILLED = 32,
	PEDGE_COLLAPSE = 64,
	PEDGE_COLLAPSE_EDGE = 128,
	PEDGE_COLLAPSE_PAIR = 256
};

/* for flipping faces */
#define PEDGE_VERTEX_FLAGS (PEDGE_PIN)

enum PFaceFlag {
	PFACE_CONNECTED = 1,
	PFACE_FILLED = 2,
	PFACE_COLLAPSE = 4
};

/* Chart */

typedef struct PChart {
	PVert *verts;
	PEdge *edges;
	PFace *faces;
	int nverts, nedges, nfaces;

	PVert *collapsed_verts;
	PEdge *collapsed_edges;
	PFace *collapsed_faces;

	union PChartUnion {
		struct PChartLscm {
			NLContext context;
			float *abf_alpha;
			PVert *pin1, *pin2;
		} lscm;
		struct PChartPack {
			float rescale, area;
			float size[2], trans[2];
		} pack;
	} u;

	unsigned char flag;
	struct PHandle *handle;
} PChart;

enum PChartFlag {
	PCHART_NOPACK = 1
};

enum PHandleState {
	PHANDLE_STATE_ALLOCATED,
	PHANDLE_STATE_CONSTRUCTED,
	PHANDLE_STATE_LSCM,
	PHANDLE_STATE_STRETCH
};

typedef struct PHandle {
	enum PHandleState state;
	MemArena *arena;

	PChart *construction_chart;
	PHash *hash_verts;
	PHash *hash_edges;
	PHash *hash_faces;

	PChart **charts;
	int ncharts;

	float aspx, aspy;

	RNG *rng;
	float blend;
} PHandle;


/* PHash
 * - special purpose hash that keeps all its elements in a single linked list.
 * - after construction, this hash is thrown away, and the list remains.
 * - removing elements is not possible efficiently.
 */

static int PHashSizes[] = {
	1, 3, 5, 11, 17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209, 
	16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169, 
	4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 268435459
};

#define PHASH_hash(ph, item) (((uintptr_t) (item)) % ((unsigned int) (ph)->cursize))
#define PHASH_edge(v1, v2)   ((v1) ^ (v2))

static PHash *phash_new(PHashLink **list, int sizehint)
{
	PHash *ph = (PHash *)MEM_callocN(sizeof(PHash), "PHash");
	ph->size = 0;
	ph->cursize_id = 0;
	ph->list = list;

	while (PHashSizes[ph->cursize_id] < sizehint)
		ph->cursize_id++;

	ph->cursize = PHashSizes[ph->cursize_id];
	ph->buckets = (PHashLink **)MEM_callocN(ph->cursize * sizeof(*ph->buckets), "PHashBuckets");

	return ph;
}

static void phash_delete(PHash *ph)
{
	MEM_freeN(ph->buckets);
	MEM_freeN(ph);
}

static int phash_size(PHash *ph)
{
	return ph->size;
}

static void phash_insert(PHash *ph, PHashLink *link)
{
	int size = ph->cursize;
	uintptr_t hash = PHASH_hash(ph, link->key);
	PHashLink *lookup = ph->buckets[hash];

	if (lookup == NULL) {
		/* insert in front of the list */
		ph->buckets[hash] = link;
		link->next = *(ph->list);
		*(ph->list) = link;
	}
	else {
		/* insert after existing element */
		link->next = lookup->next;
		lookup->next = link;
	}
		
	ph->size++;

	if (ph->size > (size * 3)) {
		PHashLink *next = NULL, *first = *(ph->list);

		ph->cursize = PHashSizes[++ph->cursize_id];
		MEM_freeN(ph->buckets);
		ph->buckets = (PHashLink **)MEM_callocN(ph->cursize * sizeof(*ph->buckets), "PHashBuckets");
		ph->size = 0;
		*(ph->list) = NULL;

		for (link = first; link; link = next) {
			next = link->next;
			phash_insert(ph, link);
		}
	}
}

static PHashLink *phash_lookup(PHash *ph, PHashKey key)
{
	PHashLink *link;
	uintptr_t hash = PHASH_hash(ph, key);

	for (link = ph->buckets[hash]; link; link = link->next)
		if (link->key == key)
			return link;
		else if (PHASH_hash(ph, link->key) != hash)
			return NULL;
	
	return link;
}

static PHashLink *phash_next(PHash *ph, PHashKey key, PHashLink *link)
{
	uintptr_t hash = PHASH_hash(ph, key);

	for (link = link->next; link; link = link->next)
		if (link->key == key)
			return link;
		else if (PHASH_hash(ph, link->key) != hash)
			return NULL;
	
	return link;
}

/* Geometry */

static float p_vec_angle_cos(float *v1, float *v2, float *v3)
{
	float d1[3], d2[3];

	d1[0] = v1[0] - v2[0];
	d1[1] = v1[1] - v2[1];
	d1[2] = v1[2] - v2[2];

	d2[0] = v3[0] - v2[0];
	d2[1] = v3[1] - v2[1];
	d2[2] = v3[2] - v2[2];

	normalize_v3(d1);
	normalize_v3(d2);

	return d1[0] * d2[0] + d1[1] * d2[1] + d1[2] * d2[2];
}

static float p_vec_angle(float *v1, float *v2, float *v3)
{
	float dot = p_vec_angle_cos(v1, v2, v3);

	if (dot <= -1.0f)
		return (float)M_PI;
	else if (dot >= 1.0f)
		return 0.0f;
	else
		return (float)acos(dot);
}

static float p_vec2_angle(float *v1, float *v2, float *v3)
{
	float u1[3], u2[3], u3[3];

	u1[0] = v1[0]; u1[1] = v1[1]; u1[2] = 0.0f;
	u2[0] = v2[0]; u2[1] = v2[1]; u2[2] = 0.0f;
	u3[0] = v3[0]; u3[1] = v3[1]; u3[2] = 0.0f;

	return p_vec_angle(u1, u2, u3);
}

static void p_triangle_angles(float *v1, float *v2, float *v3, float *a1, float *a2, float *a3)
{
	*a1 = p_vec_angle(v3, v1, v2);
	*a2 = p_vec_angle(v1, v2, v3);
	*a3 = (float)M_PI - *a2 - *a1;
}

static void p_face_angles(PFace *f, float *a1, float *a2, float *a3)
{
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
	PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

	p_triangle_angles(v1->co, v2->co, v3->co, a1, a2, a3);
}

static float p_face_area(PFace *f)
{
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
	PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

	return area_tri_v3(v1->co, v2->co, v3->co);
}

static float p_area_signed(float *v1, float *v2, float *v3)
{
	return 0.5f * (((v2[0] - v1[0]) * (v3[1] - v1[1])) -
	               ((v3[0] - v1[0]) * (v2[1] - v1[1])));
}

static float p_face_uv_area_signed(PFace *f)
{
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
	PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

	return 0.5f * (((v2->uv[0] - v1->uv[0]) * (v3->uv[1] - v1->uv[1])) -
	               ((v3->uv[0] - v1->uv[0]) * (v2->uv[1] - v1->uv[1])));
}

static float p_edge_length(PEdge *e)
{
	PVert *v1 = e->vert, *v2 = e->next->vert;
	float d[3];

	d[0] = v2->co[0] - v1->co[0];
	d[1] = v2->co[1] - v1->co[1];
	d[2] = v2->co[2] - v1->co[2];

	return sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
}

static float p_edge_uv_length(PEdge *e)
{
	PVert *v1 = e->vert, *v2 = e->next->vert;
	float d[3];

	d[0] = v2->uv[0] - v1->uv[0];
	d[1] = v2->uv[1] - v1->uv[1];

	return sqrt(d[0] * d[0] + d[1] * d[1]);
}

static void p_chart_uv_bbox(PChart *chart, float *minv, float *maxv)
{
	PVert *v;

	INIT_MINMAX2(minv, maxv);

	for (v = chart->verts; v; v = v->nextlink) {
		DO_MINMAX2(v->uv, minv, maxv);
	}
}

static void p_chart_uv_scale(PChart *chart, float scale)
{
	PVert *v;

	for (v = chart->verts; v; v = v->nextlink) {
		v->uv[0] *= scale;
		v->uv[1] *= scale;
	}
}

static void p_chart_uv_scale_xy(PChart *chart, float x, float y)
{
	PVert *v;

	for (v = chart->verts; v; v = v->nextlink) {
		v->uv[0] *= x;
		v->uv[1] *= y;
	}
}

static void p_chart_uv_translate(PChart *chart, float trans[2])
{
	PVert *v;

	for (v = chart->verts; v; v = v->nextlink) {
		v->uv[0] += trans[0];
		v->uv[1] += trans[1];
	}
}

static PBool p_intersect_line_2d_dir(float *v1, float *dir1, float *v2, float *dir2, float *isect)
{
	float lmbda, div;

	div = dir2[0] * dir1[1] - dir2[1] * dir1[0];

	if (div == 0.0f)
		return P_FALSE;

	lmbda = ((v1[1] - v2[1]) * dir1[0] - (v1[0] - v2[0]) * dir1[1]) / div;
	isect[0] = v1[0] + lmbda * dir2[0];
	isect[1] = v1[1] + lmbda * dir2[1];

	return P_TRUE;
}

#if 0
static PBool p_intersect_line_2d(float *v1, float *v2, float *v3, float *v4, float *isect)
{
	float dir1[2], dir2[2];

	dir1[0] = v4[0] - v3[0];
	dir1[1] = v4[1] - v3[1];

	dir2[0] = v2[0] - v1[0];
	dir2[1] = v2[1] - v1[1];

	if (!p_intersect_line_2d_dir(v1, dir1, v2, dir2, isect)) {
		/* parallel - should never happen in theory for polygon kernel, but
		 * let's give a point nearby in case things go wrong */
		isect[0] = (v1[0] + v2[0]) * 0.5f;
		isect[1] = (v1[1] + v2[1]) * 0.5f;
		return P_FALSE;
	}

	return P_TRUE;
}
#endif

/* Topological Utilities */

static PEdge *p_wheel_edge_next(PEdge *e)
{
	return e->next->next->pair;
}

static PEdge *p_wheel_edge_prev(PEdge *e)
{   
	return (e->pair) ? e->pair->next : NULL;
}

static PEdge *p_boundary_edge_next(PEdge *e)
{
	return e->next->vert->edge;
}

static PEdge *p_boundary_edge_prev(PEdge *e)
{
	PEdge *we = e, *last;

	do {
		last = we;
		we = p_wheel_edge_next(we);
	} while (we && (we != e));

	return last->next->next;
}

static PBool p_vert_interior(PVert *v)
{
	return (v->edge->pair != NULL);
}

static void p_face_flip(PFace *f)
{
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
	PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;
	int f1 = e1->flag, f2 = e2->flag, f3 = e3->flag;
	float *orig_uv1 = e1->orig_uv, *orig_uv2 = e2->orig_uv, *orig_uv3 = e3->orig_uv;

	e1->vert = v2;
	e1->next = e3;
	e1->orig_uv = orig_uv2;
	e1->flag = (f1 & ~PEDGE_VERTEX_FLAGS) | (f2 & PEDGE_VERTEX_FLAGS);

	e2->vert = v3;
	e2->next = e1;
	e2->orig_uv = orig_uv3;
	e2->flag = (f2 & ~PEDGE_VERTEX_FLAGS) | (f3 & PEDGE_VERTEX_FLAGS);

	e3->vert = v1;
	e3->next = e2;
	e3->orig_uv = orig_uv1;
	e3->flag = (f3 & ~PEDGE_VERTEX_FLAGS) | (f1 & PEDGE_VERTEX_FLAGS);
}

#if 0
static void p_chart_topological_sanity_check(PChart *chart)
{
	PVert *v;
	PEdge *e;

	for (v = chart->verts; v; v = v->nextlink)
		param_test_equals_ptr("v->edge->vert", v, v->edge->vert);
	
	for (e = chart->edges; e; e = e->nextlink) {
		if (e->pair) {
			param_test_equals_ptr("e->pair->pair", e, e->pair->pair);
			param_test_equals_ptr("pair->vert", e->vert, e->pair->next->vert);
			param_test_equals_ptr("pair->next->vert", e->next->vert, e->pair->vert);
		}
	}
}
#endif

/* Loading / Flushing */

static void p_vert_load_pin_select_uvs(PHandle *handle, PVert *v)
{
	PEdge *e;
	int nedges = 0, npins = 0;
	float pinuv[2];

	v->uv[0] = v->uv[1] = 0.0f;
	pinuv[0] = pinuv[1] = 0.0f;
	e = v->edge;
	do {
		if (e->orig_uv) {
			if (e->flag & PEDGE_SELECT)
				v->flag |= PVERT_SELECT;

			if (e->flag & PEDGE_PIN) {
				pinuv[0] += e->orig_uv[0] * handle->aspx;
				pinuv[1] += e->orig_uv[1] * handle->aspy;
				npins++;
			}
			else {
				v->uv[0] += e->orig_uv[0] * handle->aspx;
				v->uv[1] += e->orig_uv[1] * handle->aspy;
			}

			nedges++;
		}

		e = p_wheel_edge_next(e);
	} while (e && e != (v->edge));

	if (npins > 0) {
		v->uv[0] = pinuv[0] / npins;
		v->uv[1] = pinuv[1] / npins;
		v->flag |= PVERT_PIN;
	}
	else if (nedges > 0) {
		v->uv[0] /= nedges;
		v->uv[1] /= nedges;
	}
}

static void p_flush_uvs(PHandle *handle, PChart *chart)
{
	PEdge *e;

	for (e = chart->edges; e; e = e->nextlink) {
		if (e->orig_uv) {
			e->orig_uv[0] = e->vert->uv[0] / handle->aspx;
			e->orig_uv[1] = e->vert->uv[1] / handle->aspy;
		}
	}
}

static void p_flush_uvs_blend(PHandle *handle, PChart *chart, float blend)
{
	PEdge *e;
	float invblend = 1.0f - blend;

	for (e = chart->edges; e; e = e->nextlink) {
		if (e->orig_uv) {
			e->orig_uv[0] = blend * e->old_uv[0] + invblend * e->vert->uv[0] / handle->aspx;
			e->orig_uv[1] = blend * e->old_uv[1] + invblend * e->vert->uv[1] / handle->aspy;
		}
	}
}

static void p_face_backup_uvs(PFace *f)
{
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;

	if (e1->orig_uv) {
		e1->old_uv[0] = e1->orig_uv[0];
		e1->old_uv[1] = e1->orig_uv[1];
	}
	if (e2->orig_uv) {
		e2->old_uv[0] = e2->orig_uv[0];
		e2->old_uv[1] = e2->orig_uv[1];
	}
	if (e3->orig_uv) {
		e3->old_uv[0] = e3->orig_uv[0];
		e3->old_uv[1] = e3->orig_uv[1];
	}
}

static void p_face_restore_uvs(PFace *f)
{
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;

	if (e1->orig_uv) {
		e1->orig_uv[0] = e1->old_uv[0];
		e1->orig_uv[1] = e1->old_uv[1];
	}
	if (e2->orig_uv) {
		e2->orig_uv[0] = e2->old_uv[0];
		e2->orig_uv[1] = e2->old_uv[1];
	}
	if (e3->orig_uv) {
		e3->orig_uv[0] = e3->old_uv[0];
		e3->orig_uv[1] = e3->old_uv[1];
	}
}

/* Construction (use only during construction, relies on u.key being set */

static PVert *p_vert_add(PHandle *handle, PHashKey key, float *co, PEdge *e)
{
	PVert *v = (PVert *)BLI_memarena_alloc(handle->arena, sizeof *v);
	copy_v3_v3(v->co, co);
	v->u.key = key;
	v->edge = e;
	v->flag = 0;

	phash_insert(handle->hash_verts, (PHashLink *)v);

	return v;
}

static PVert *p_vert_lookup(PHandle *handle, PHashKey key, float *co, PEdge *e)
{
	PVert *v = (PVert *)phash_lookup(handle->hash_verts, key);

	if (v)
		return v;
	else
		return p_vert_add(handle, key, co, e);
}

static PVert *p_vert_copy(PChart *chart, PVert *v)
{
	PVert *nv = (PVert *)BLI_memarena_alloc(chart->handle->arena, sizeof *nv);

	copy_v3_v3(nv->co, v->co);
	nv->uv[0] = v->uv[0];
	nv->uv[1] = v->uv[1];
	nv->u.key = v->u.key;
	nv->edge = v->edge;
	nv->flag = v->flag;

	return nv;
}

static PEdge *p_edge_lookup(PHandle *handle, PHashKey *vkeys)
{
	PHashKey key = PHASH_edge(vkeys[0], vkeys[1]);
	PEdge *e = (PEdge *)phash_lookup(handle->hash_edges, key);

	while (e) {
		if ((e->vert->u.key == vkeys[0]) && (e->next->vert->u.key == vkeys[1]))
			return e;
		else if ((e->vert->u.key == vkeys[1]) && (e->next->vert->u.key == vkeys[0]))
			return e;

		e = (PEdge *)phash_next(handle->hash_edges, key, (PHashLink *)e);
	}

	return NULL;
}

int p_face_exists(ParamHandle *phandle, ParamKey *pvkeys, int i1, int i2, int i3)
{
	PHandle *handle = (PHandle *)phandle;
	PHashKey *vkeys = (PHashKey *)pvkeys;
	PHashKey key = PHASH_edge(vkeys[i1], vkeys[i2]);
	PEdge *e = (PEdge *)phash_lookup(handle->hash_edges, key);

	while (e) {
		if ((e->vert->u.key == vkeys[i1]) && (e->next->vert->u.key == vkeys[i2])) {
			if (e->next->next->vert->u.key == vkeys[i3])
				return P_TRUE;
		}
		else if ((e->vert->u.key == vkeys[i2]) && (e->next->vert->u.key == vkeys[i1])) {
			if (e->next->next->vert->u.key == vkeys[i3])
				return P_TRUE;
		}

		e = (PEdge *)phash_next(handle->hash_edges, key, (PHashLink *)e);
	}

	return P_FALSE;
}

static PChart *p_chart_new(PHandle *handle)
{
	PChart *chart = (PChart *)MEM_callocN(sizeof *chart, "PChart");
	chart->handle = handle;

	return chart;
}

static void p_chart_delete(PChart *chart)
{
	/* the actual links are free by memarena */
	MEM_freeN(chart);
}

static PBool p_edge_implicit_seam(PEdge *e, PEdge *ep)
{
	float *uv1, *uv2, *uvp1, *uvp2;
	float limit[2];

	limit[0] = 0.00001;
	limit[1] = 0.00001;

	uv1 = e->orig_uv;
	uv2 = e->next->orig_uv;

	if (e->vert->u.key == ep->vert->u.key) {
		uvp1 = ep->orig_uv;
		uvp2 = ep->next->orig_uv;
	}
	else {
		uvp1 = ep->next->orig_uv;
		uvp2 = ep->orig_uv;
	}

	if ((fabsf(uv1[0] - uvp1[0]) > limit[0]) || (fabsf(uv1[1] - uvp1[1]) > limit[1])) {
		e->flag |= PEDGE_SEAM;
		ep->flag |= PEDGE_SEAM;
		return P_TRUE;
	}
	if ((fabsf(uv2[0] - uvp2[0]) > limit[0]) || (fabsf(uv2[1] - uvp2[1]) > limit[1])) {
		e->flag |= PEDGE_SEAM;
		ep->flag |= PEDGE_SEAM;
		return P_TRUE;
	}
	
	return P_FALSE;
}

static PBool p_edge_has_pair(PHandle *handle, PEdge *e, PEdge **pair, PBool impl)
{
	PHashKey key;
	PEdge *pe;
	PVert *v1, *v2;
	PHashKey key1 = e->vert->u.key;
	PHashKey key2 = e->next->vert->u.key;

	if (e->flag & PEDGE_SEAM)
		return P_FALSE;
	
	key = PHASH_edge(key1, key2);
	pe = (PEdge *)phash_lookup(handle->hash_edges, key);
	*pair = NULL;

	while (pe) {
		if (pe != e) {
			v1 = pe->vert;
			v2 = pe->next->vert;

			if (((v1->u.key == key1) && (v2->u.key == key2)) ||
			    ((v1->u.key == key2) && (v2->u.key == key1)))
			{

				/* don't connect seams and t-junctions */
				if ((pe->flag & PEDGE_SEAM) || *pair ||
				    (impl && p_edge_implicit_seam(e, pe)))
				{
					*pair = NULL;
					return P_FALSE;
				}

				*pair = pe;
			}
		}

		pe = (PEdge *)phash_next(handle->hash_edges, key, (PHashLink *)pe);
	}

	if (*pair && (e->vert == (*pair)->vert)) {
		if ((*pair)->next->pair || (*pair)->next->next->pair) {
			/* non unfoldable, maybe mobius ring or klein bottle */
			*pair = NULL;
			return P_FALSE;
		}
	}

	return (*pair != NULL);
}

static PBool p_edge_connect_pair(PHandle *handle, PEdge *e, PEdge ***stack, PBool impl)
{
	PEdge *pair = NULL;

	if (!e->pair && p_edge_has_pair(handle, e, &pair, impl)) {
		if (e->vert == pair->vert)
			p_face_flip(pair->face);

		e->pair = pair;
		pair->pair = e;

		if (!(pair->face->flag & PFACE_CONNECTED)) {
			**stack = pair;
			(*stack)++;
		}
	}

	return (e->pair != NULL);
}

static int p_connect_pairs(PHandle *handle, PBool impl)
{
	PEdge **stackbase = MEM_mallocN(sizeof *stackbase * phash_size(handle->hash_faces), "Pstackbase");
	PEdge **stack = stackbase;
	PFace *f, *first;
	PEdge *e, *e1, *e2;
	PChart *chart = handle->construction_chart;
	int ncharts = 0;

	/* connect pairs, count edges, set vertex-edge pointer to a pairless edge */
	for (first = chart->faces; first; first = first->nextlink) {
		if (first->flag & PFACE_CONNECTED)
			continue;

		*stack = first->edge;
		stack++;

		while (stack != stackbase) {
			stack--;
			e = *stack;
			e1 = e->next;
			e2 = e1->next;

			f = e->face;
			f->flag |= PFACE_CONNECTED;

			/* assign verts to charts so we can sort them later */
			f->u.chart = ncharts;

			if (!p_edge_connect_pair(handle, e, &stack, impl))
				e->vert->edge = e;
			if (!p_edge_connect_pair(handle, e1, &stack, impl))
				e1->vert->edge = e1;
			if (!p_edge_connect_pair(handle, e2, &stack, impl))
				e2->vert->edge = e2;
		}

		ncharts++;
	}

	MEM_freeN(stackbase);

	return ncharts;
}

static void p_split_vert(PChart *chart, PEdge *e)
{
	PEdge *we, *lastwe = NULL;
	PVert *v = e->vert;
	PBool copy = P_TRUE;

	if (e->flag & PEDGE_VERTEX_SPLIT)
		return;

	/* rewind to start */
	lastwe = e;
	for (we = p_wheel_edge_prev(e); we && (we != e); we = p_wheel_edge_prev(we))
		lastwe = we;
	
	/* go over all edges in wheel */
	for (we = lastwe; we; we = p_wheel_edge_next(we)) {
		if (we->flag & PEDGE_VERTEX_SPLIT)
			break;

		we->flag |= PEDGE_VERTEX_SPLIT;

		if (we == v->edge) {
			/* found it, no need to copy */
			copy = P_FALSE;
			v->nextlink = chart->verts;
			chart->verts = v;
			chart->nverts++;
		}
	}

	if (copy) {
		/* not found, copying */
		v->flag |= PVERT_SPLIT;
		v = p_vert_copy(chart, v);
		v->flag |= PVERT_SPLIT;

		v->nextlink = chart->verts;
		chart->verts = v;
		chart->nverts++;

		v->edge = lastwe;

		we = lastwe;
		do {
			we->vert = v;
			we = p_wheel_edge_next(we);
		} while (we && (we != lastwe));
	}
}

static PChart **p_split_charts(PHandle *handle, PChart *chart, int ncharts)
{
	PChart **charts = MEM_mallocN(sizeof *charts * ncharts, "PCharts"), *nchart;
	PFace *f, *nextf;
	int i;

	for (i = 0; i < ncharts; i++)
		charts[i] = p_chart_new(handle);

	f = chart->faces;
	while (f) {
		PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
		nextf = f->nextlink;

		nchart = charts[f->u.chart];

		f->nextlink = nchart->faces;
		nchart->faces = f;
		e1->nextlink = nchart->edges;
		nchart->edges = e1;
		e2->nextlink = nchart->edges;
		nchart->edges = e2;
		e3->nextlink = nchart->edges;
		nchart->edges = e3;

		nchart->nfaces++;
		nchart->nedges += 3;

		p_split_vert(nchart, e1);
		p_split_vert(nchart, e2);
		p_split_vert(nchart, e3);

		f = nextf;
	}

	return charts;
}

static PFace *p_face_add(PHandle *handle)
{
	PFace *f;
	PEdge *e1, *e2, *e3;

	/* allocate */
	f = (PFace *)BLI_memarena_alloc(handle->arena, sizeof *f);
	f->flag = 0; // init !

	e1 = (PEdge *)BLI_memarena_alloc(handle->arena, sizeof *e1);
	e2 = (PEdge *)BLI_memarena_alloc(handle->arena, sizeof *e2);
	e3 = (PEdge *)BLI_memarena_alloc(handle->arena, sizeof *e3);

	/* set up edges */
	f->edge = e1;
	e1->face = e2->face = e3->face = f;

	e1->next = e2;
	e2->next = e3;
	e3->next = e1;

	e1->pair = NULL;
	e2->pair = NULL;
	e3->pair = NULL;
   
	e1->flag = 0;
	e2->flag = 0;
	e3->flag = 0;

	return f;
}

static PFace *p_face_add_construct(PHandle *handle, ParamKey key, ParamKey *vkeys,
                                   float *co[3], float *uv[3], int i1, int i2, int i3,
                                   ParamBool *pin, ParamBool *select)
{
	PFace *f = p_face_add(handle);
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;

	e1->vert = p_vert_lookup(handle, vkeys[i1], co[i1], e1);
	e2->vert = p_vert_lookup(handle, vkeys[i2], co[i2], e2);
	e3->vert = p_vert_lookup(handle, vkeys[i3], co[i3], e3);

	e1->orig_uv = uv[i1];
	e2->orig_uv = uv[i2];
	e3->orig_uv = uv[i3];

	if (pin) {
		if (pin[i1]) e1->flag |= PEDGE_PIN;
		if (pin[i2]) e2->flag |= PEDGE_PIN;
		if (pin[i3]) e3->flag |= PEDGE_PIN;
	}

	if (select) {
		if (select[i1]) e1->flag |= PEDGE_SELECT;
		if (select[i2]) e2->flag |= PEDGE_SELECT;
		if (select[i3]) e3->flag |= PEDGE_SELECT;
	}

	/* insert into hash */
	f->u.key = key;
	phash_insert(handle->hash_faces, (PHashLink *)f);

	e1->u.key = PHASH_edge(vkeys[i1], vkeys[i2]);
	e2->u.key = PHASH_edge(vkeys[i2], vkeys[i3]);
	e3->u.key = PHASH_edge(vkeys[i3], vkeys[i1]);

	phash_insert(handle->hash_edges, (PHashLink *)e1);
	phash_insert(handle->hash_edges, (PHashLink *)e2);
	phash_insert(handle->hash_edges, (PHashLink *)e3);

	return f;
}

static PFace *p_face_add_fill(PChart *chart, PVert *v1, PVert *v2, PVert *v3)
{
	PFace *f = p_face_add(chart->handle);
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;

	e1->vert = v1;
	e2->vert = v2;
	e3->vert = v3;

	e1->orig_uv = e2->orig_uv = e3->orig_uv = NULL;

	f->nextlink = chart->faces;
	chart->faces = f;
	e1->nextlink = chart->edges;
	chart->edges = e1;
	e2->nextlink = chart->edges;
	chart->edges = e2;
	e3->nextlink = chart->edges;
	chart->edges = e3;

	chart->nfaces++;
	chart->nedges += 3;

	return f;
}

static PBool p_quad_split_direction(PHandle *handle, float **co, PHashKey *vkeys)
{
	float fac = len_v3v3(co[0], co[2]) - len_v3v3(co[1], co[3]);
	PBool dir = (fac <= 0.0f);

	/* the face exists check is there because of a special case: when
	 * two quads share three vertices, they can each be split into two
	 * triangles, resulting in two identical triangles. for example in
	 * suzanne's nose. */
	if (dir) {
		if (p_face_exists(handle, vkeys, 0, 1, 2) || p_face_exists(handle, vkeys, 0, 2, 3))
			return !dir;
	}
	else {
		if (p_face_exists(handle, vkeys, 0, 1, 3) || p_face_exists(handle, vkeys, 1, 2, 3))
			return !dir;
	}

	return dir;
}

/* Construction: boundary filling */

static void p_chart_boundaries(PChart *chart, int *nboundaries, PEdge **outer)
{   
	PEdge *e, *be;
	float len, maxlen = -1.0;

	if (nboundaries)
		*nboundaries = 0;
	if (outer)
		*outer = NULL;

	for (e = chart->edges; e; e = e->nextlink) {
		if (e->pair || (e->flag & PEDGE_DONE))
			continue;

		if (nboundaries)
			(*nboundaries)++;

		len = 0.0f;

		be = e;
		do {
			be->flag |= PEDGE_DONE;
			len += p_edge_length(be);
			be = be->next->vert->edge;
		} while (be != e);

		if (outer && (len > maxlen)) {
			*outer = e;
			maxlen = len;
		}
	}

	for (e = chart->edges; e; e = e->nextlink)
		e->flag &= ~PEDGE_DONE;
}

static float p_edge_boundary_angle(PEdge *e)
{
	PEdge *we;
	PVert *v, *v1, *v2;
	float angle;
	int n = 0;

	v = e->vert;

	/* concave angle check -- could be better */
	angle = M_PI;

	we = v->edge;
	do {
		v1 = we->next->vert;
		v2 = we->next->next->vert;
		angle -= p_vec_angle(v1->co, v->co, v2->co);

		we = we->next->next->pair;
		n++;
	} while (we && (we != v->edge));

	return angle;
}

static void p_chart_fill_boundary(PChart *chart, PEdge *be, int nedges)
{
	PEdge *e, *e1, *e2;

	PFace *f;
	struct Heap *heap = BLI_heap_new();
	float angle;

	e = be;
	do {
		angle = p_edge_boundary_angle(e);
		e->u.heaplink = BLI_heap_insert(heap, angle, e);

		e = p_boundary_edge_next(e);
	} while (e != be);

	if (nedges == 2) {
		/* no real boundary, but an isolated seam */
		e = be->next->vert->edge;
		e->pair = be;
		be->pair = e;

		BLI_heap_remove(heap, e->u.heaplink);
		BLI_heap_remove(heap, be->u.heaplink);
	}
	else {
		while (nedges > 2) {
			PEdge *ne, *ne1, *ne2;

			e = (PEdge *)BLI_heap_popmin(heap);

			e1 = p_boundary_edge_prev(e);
			e2 = p_boundary_edge_next(e);

			BLI_heap_remove(heap, e1->u.heaplink);
			BLI_heap_remove(heap, e2->u.heaplink);
			e->u.heaplink = e1->u.heaplink = e2->u.heaplink = NULL;

			e->flag |= PEDGE_FILLED;
			e1->flag |= PEDGE_FILLED;





			f = p_face_add_fill(chart, e->vert, e1->vert, e2->vert);
			f->flag |= PFACE_FILLED;

			ne = f->edge->next->next;
			ne1 = f->edge;
			ne2 = f->edge->next;

			ne->flag = ne1->flag = ne2->flag = PEDGE_FILLED;

			e->pair = ne;
			ne->pair = e;
			e1->pair = ne1;
			ne1->pair = e1;

			ne->vert = e2->vert;
			ne1->vert = e->vert;
			ne2->vert = e1->vert;

			if (nedges == 3) {
				e2->pair = ne2;
				ne2->pair = e2;
			}
			else {
				ne2->vert->edge = ne2;
				
				ne2->u.heaplink = BLI_heap_insert(heap, p_edge_boundary_angle(ne2), ne2);
				e2->u.heaplink = BLI_heap_insert(heap, p_edge_boundary_angle(e2), e2);
			}

			nedges--;
		}
	}

	BLI_heap_free(heap, NULL);
}

static void p_chart_fill_boundaries(PChart *chart, PEdge *outer)
{
	PEdge *e, *be; /* *enext - as yet unused */
	int nedges;

	for (e = chart->edges; e; e = e->nextlink) {
		/* enext = e->nextlink; - as yet unused */

		if (e->pair || (e->flag & PEDGE_FILLED))
			continue;

		nedges = 0;
		be = e;
		do {
			be->flag |= PEDGE_FILLED;
			be = be->next->vert->edge;
			nedges++;
		} while (be != e);

		if (e != outer)
			p_chart_fill_boundary(chart, e, nedges);
	}
}

#if 0
/* Polygon kernel for inserting uv's non overlapping */

static int p_polygon_point_in(float *cp1, float *cp2, float *p)
{
	if ((cp1[0] == p[0]) && (cp1[1] == p[1]))
		return 2;
	else if ((cp2[0] == p[0]) && (cp2[1] == p[1]))
		return 3;
	else
		return (p_area_signed(cp1, cp2, p) >= 0.0f);
}

static void p_polygon_kernel_clip(float (*oldpoints)[2], int noldpoints, float (*newpoints)[2], int *nnewpoints, float *cp1, float *cp2)
{
	float *p2, *p1, isect[2];
	int i, p2in, p1in;

	p1 = oldpoints[noldpoints - 1];
	p1in = p_polygon_point_in(cp1, cp2, p1);
	*nnewpoints = 0;

	for (i = 0; i < noldpoints; i++) {
		p2 = oldpoints[i];
		p2in = p_polygon_point_in(cp1, cp2, p2);

		if ((p2in >= 2) || (p1in && p2in)) {
			newpoints[*nnewpoints][0] = p2[0];
			newpoints[*nnewpoints][1] = p2[1];
			(*nnewpoints)++;
		}
		else if (p1in && !p2in) {
			if (p1in != 3) {
				p_intersect_line_2d(p1, p2, cp1, cp2, isect);
				newpoints[*nnewpoints][0] = isect[0];
				newpoints[*nnewpoints][1] = isect[1];
				(*nnewpoints)++;
			}
		}
		else if (!p1in && p2in) {
			p_intersect_line_2d(p1, p2, cp1, cp2, isect);
			newpoints[*nnewpoints][0] = isect[0];
			newpoints[*nnewpoints][1] = isect[1];
			(*nnewpoints)++;

			newpoints[*nnewpoints][0] = p2[0];
			newpoints[*nnewpoints][1] = p2[1];
			(*nnewpoints)++;
		}
		
		p1in = p2in;
		p1 = p2;
	}
}

static void p_polygon_kernel_center(float (*points)[2], int npoints, float *center)
{
	int i, size, nnewpoints = npoints;
	float (*oldpoints)[2], (*newpoints)[2], *p1, *p2;
	
	size = npoints * 3;
	oldpoints = MEM_mallocN(sizeof(float) * 2 * size, "PPolygonOldPoints");
	newpoints = MEM_mallocN(sizeof(float) * 2 * size, "PPolygonNewPoints");

	memcpy(oldpoints, points, sizeof(float) * 2 * npoints);

	for (i = 0; i < npoints; i++) {
		p1 = points[i];
		p2 = points[(i + 1) % npoints];
		p_polygon_kernel_clip(oldpoints, nnewpoints, newpoints, &nnewpoints, p1, p2);

		if (nnewpoints == 0) {
			/* degenerate case, use center of original polygon */
			memcpy(oldpoints, points, sizeof(float) * 2 * npoints);
			nnewpoints = npoints;
			break;
		}
		else if (nnewpoints == 1) {
			/* degenerate case, use remaining point */
			center[0] = newpoints[0][0];
			center[1] = newpoints[0][1];

			MEM_freeN(oldpoints);
			MEM_freeN(newpoints);

			return;
		}

		if (nnewpoints * 2 > size) {
			size *= 2;
			MEM_freeN(oldpoints);
			oldpoints = MEM_mallocN(sizeof(float) * 2 * size, "oldpoints");
			memcpy(oldpoints, newpoints, sizeof(float) * 2 * nnewpoints);
			MEM_freeN(newpoints);
			newpoints = MEM_mallocN(sizeof(float) * 2 * size, "newpoints");
		}
		else {
			float (*sw_points)[2] = oldpoints;
			oldpoints = newpoints;
			newpoints = sw_points;
		}
	}

	center[0] = center[1] = 0.0f;

	for (i = 0; i < nnewpoints; i++) {
		center[0] += oldpoints[i][0];
		center[1] += oldpoints[i][1];
	}

	center[0] /= nnewpoints;
	center[1] /= nnewpoints;

	MEM_freeN(oldpoints);
	MEM_freeN(newpoints);
}
#endif

#if 0
/* Edge Collapser */

int NCOLLAPSE = 1;
int NCOLLAPSEX = 0;
	
static float p_vert_cotan(float *v1, float *v2, float *v3)
{
	float a[3], b[3], c[3], clen;

	sub_v3_v3v3(a, v2, v1);
	sub_v3_v3v3(b, v3, v1);
	cross_v3_v3v3(c, a, b);

	clen = len_v3(c);

	if (clen == 0.0f)
		return 0.0f;
	
	return dot_v3v3(a, b) / clen;
}
	
static PBool p_vert_flipped_wheel_triangle(PVert *v)
{
	PEdge *e = v->edge;

	do {
		if (p_face_uv_area_signed(e->face) < 0.0f)
			return P_TRUE;

		e = p_wheel_edge_next(e);
	} while (e && (e != v->edge));

	return P_FALSE;
}

static PBool p_vert_map_harmonic_weights(PVert *v)
{
	float weightsum, positionsum[2], olduv[2];

	weightsum = 0.0f;
	positionsum[0] = positionsum[1] = 0.0f;

	if (p_vert_interior(v)) {
		PEdge *e = v->edge;

		do {
			float t1, t2, weight;
			PVert *v1, *v2;
			
			v1 = e->next->vert;
			v2 = e->next->next->vert;
			t1 = p_vert_cotan(v2->co, e->vert->co, v1->co);

			v1 = e->pair->next->vert;
			v2 = e->pair->next->next->vert;
			t2 = p_vert_cotan(v2->co, e->pair->vert->co, v1->co);

			weight = 0.5f * (t1 + t2);
			weightsum += weight;
			positionsum[0] += weight * e->pair->vert->uv[0];
			positionsum[1] += weight * e->pair->vert->uv[1];

			e = p_wheel_edge_next(e);
		} while (e && (e != v->edge));
	}
	else {
		PEdge *e = v->edge;

		do {
			float t1, t2;
			PVert *v1, *v2;

			v2 = e->next->vert;
			v1 = e->next->next->vert;

			t1 = p_vert_cotan(v1->co, v->co, v2->co);
			t2 = p_vert_cotan(v2->co, v->co, v1->co);

			weightsum += t1 + t2;
			positionsum[0] += (v2->uv[1] - v1->uv[1]) + (t1 * v2->uv[0] + t2 * v1->uv[0]);
			positionsum[1] += (v1->uv[0] - v2->uv[0]) + (t1 * v2->uv[1] + t2 * v1->uv[1]);
		
			e = p_wheel_edge_next(e);
		} while (e && (e != v->edge));
	}

	if (weightsum != 0.0f) {
		weightsum = 1.0f / weightsum;
		positionsum[0] *= weightsum;
		positionsum[1] *= weightsum;
	}

	olduv[0] = v->uv[0];
	olduv[1] = v->uv[1];
	v->uv[0] = positionsum[0];
	v->uv[1] = positionsum[1];

	if (p_vert_flipped_wheel_triangle(v)) {
		v->uv[0] = olduv[0];
		v->uv[1] = olduv[1];

		return P_FALSE;
	}

	return P_TRUE;
}

static void p_vert_harmonic_insert(PVert *v)
{
	PEdge *e;

	if (!p_vert_map_harmonic_weights(v)) {
		/* do polygon kernel center insertion: this is quite slow, but should
		 * only be needed for 0.01 % of verts or so, when insert with harmonic
		 * weights fails */

		int npoints = 0, i;
		float (*points)[2];

		e = v->edge;
		do {
			npoints++;	
			e = p_wheel_edge_next(e);
		} while (e && (e != v->edge));

		if (e == NULL)
			npoints++;

		points = MEM_mallocN(sizeof(float) * 2 * npoints, "PHarmonicPoints");

		e = v->edge;
		i = 0;
		do {
			PEdge *nexte = p_wheel_edge_next(e);

			points[i][0] = e->next->vert->uv[0]; 
			points[i][1] = e->next->vert->uv[1]; 

			if (nexte == NULL) {
				i++;
				points[i][0] = e->next->next->vert->uv[0]; 
				points[i][1] = e->next->next->vert->uv[1]; 
				break;
			}

			e = nexte;
			i++;
		} while (e != v->edge);
		
		p_polygon_kernel_center(points, npoints, v->uv);

		MEM_freeN(points);
	}

	e = v->edge;
	do {
		if (!(e->next->vert->flag & PVERT_PIN))
			p_vert_map_harmonic_weights(e->next->vert);
		e = p_wheel_edge_next(e);
	} while (e && (e != v->edge));

	p_vert_map_harmonic_weights(v);
}

static void p_vert_fix_edge_pointer(PVert *v)
{
	PEdge *start = v->edge;

	/* set v->edge pointer to the edge with no pair, if there is one */
	while (v->edge->pair) {
		v->edge = p_wheel_edge_prev(v->edge);
		
		if (v->edge == start)
			break;
	}
}

static void p_collapsing_verts(PEdge *edge, PEdge *pair, PVert **newv, PVert **keepv)
{
	/* the two vertices that are involved in the collapse */
	if (edge) {
		*newv = edge->vert;
		*keepv = edge->next->vert;
	}
	else {
		*newv = pair->next->vert;
		*keepv = pair->vert;
	}
}

static void p_collapse_edge(PEdge *edge, PEdge *pair)
{
	PVert *oldv, *keepv;
	PEdge *e;

	p_collapsing_verts(edge, pair, &oldv, &keepv);

	/* change e->vert pointers from old vertex to the target vertex */
	e = oldv->edge;
	do {
		if ((e != edge) && !(pair && pair->next == e))
			e->vert = keepv;

		e = p_wheel_edge_next(e);
	} while (e && (e != oldv->edge));

	/* set keepv->edge pointer */
	if ((edge && (keepv->edge == edge->next)) || (keepv->edge == pair)) {
		if (edge && edge->next->pair)
			keepv->edge = edge->next->pair->next;
		else if (pair && pair->next->next->pair)
			keepv->edge = pair->next->next->pair;
		else if (edge && edge->next->next->pair)
			keepv->edge = edge->next->next->pair;
		else
			keepv->edge = pair->next->pair->next;
	}
	
	/* update pairs and v->edge pointers */
	if (edge) {
		PEdge *e1 = edge->next, *e2 = e1->next;

		if (e1->pair)
			e1->pair->pair = e2->pair;

		if (e2->pair) {
			e2->pair->pair = e1->pair;
			e2->vert->edge = p_wheel_edge_prev(e2);
		}
		else
			e2->vert->edge = p_wheel_edge_next(e2);

		p_vert_fix_edge_pointer(e2->vert);
	}

	if (pair) {
		PEdge *e1 = pair->next, *e2 = e1->next;

		if (e1->pair)
			e1->pair->pair = e2->pair;

		if (e2->pair) {
			e2->pair->pair = e1->pair;
			e2->vert->edge = p_wheel_edge_prev(e2);
		}
		else
			e2->vert->edge = p_wheel_edge_next(e2);

		p_vert_fix_edge_pointer(e2->vert);
	}

	p_vert_fix_edge_pointer(keepv);

	/* mark for move to collapsed list later */
	oldv->flag |= PVERT_COLLAPSE;

	if (edge) {
		PFace *f = edge->face;
		PEdge *e1 = edge->next, *e2 = e1->next;

		f->flag |= PFACE_COLLAPSE;
		edge->flag |= PEDGE_COLLAPSE;
		e1->flag |= PEDGE_COLLAPSE;
		e2->flag |= PEDGE_COLLAPSE;
	}

	if (pair) {
		PFace *f = pair->face;
		PEdge *e1 = pair->next, *e2 = e1->next;

		f->flag |= PFACE_COLLAPSE;
		pair->flag |= PEDGE_COLLAPSE;
		e1->flag |= PEDGE_COLLAPSE;
		e2->flag |= PEDGE_COLLAPSE;
	}
}

static void p_split_vertex(PEdge *edge, PEdge *pair)
{
	PVert *newv, *keepv;
	PEdge *e;

	p_collapsing_verts(edge, pair, &newv, &keepv);

	/* update edge pairs */
	if (edge) {
		PEdge *e1 = edge->next, *e2 = e1->next;

		if (e1->pair)
			e1->pair->pair = e1;
		if (e2->pair)
			e2->pair->pair = e2;

		e2->vert->edge = e2;
		p_vert_fix_edge_pointer(e2->vert);
		keepv->edge = e1;
	}

	if (pair) {
		PEdge *e1 = pair->next, *e2 = e1->next;

		if (e1->pair)
			e1->pair->pair = e1;
		if (e2->pair)
			e2->pair->pair = e2;

		e2->vert->edge = e2;
		p_vert_fix_edge_pointer(e2->vert);
		keepv->edge = pair;
	}

	p_vert_fix_edge_pointer(keepv);

	/* set e->vert pointers to restored vertex */
	e = newv->edge;
	do {
		e->vert = newv;
		e = p_wheel_edge_next(e);
	} while (e && (e != newv->edge));
}

static PBool p_collapse_allowed_topologic(PEdge *edge, PEdge *pair)
{
	PVert *oldv, *keepv;

	p_collapsing_verts(edge, pair, &oldv, &keepv);

	/* boundary edges */
	if (!edge || !pair) {
		/* avoid collapsing chart into an edge */
		if (edge && !edge->next->pair && !edge->next->next->pair)
			return P_FALSE;
		else if (pair && !pair->next->pair && !pair->next->next->pair)
			return P_FALSE;
	}
	/* avoid merging two boundaries (oldv and keepv are on the 'other side' of
	 * the chart) */
	else if (!p_vert_interior(oldv) && !p_vert_interior(keepv))
		return P_FALSE;
	
	return P_TRUE;
}

static PBool p_collapse_normal_flipped(float *v1, float *v2, float *vold, float *vnew)
{
	float nold[3], nnew[3], sub1[3], sub2[3];

	sub_v3_v3v3(sub1, vold, v1);
	sub_v3_v3v3(sub2, vold, v2);
	cross_v3_v3v3(nold, sub1, sub2);

	sub_v3_v3v3(sub1, vnew, v1);
	sub_v3_v3v3(sub2, vnew, v2);
	cross_v3_v3v3(nnew, sub1, sub2);

	return (dot_v3v3(nold, nnew) <= 0.0f);
}

static PBool p_collapse_allowed_geometric(PEdge *edge, PEdge *pair)
{
	PVert *oldv, *keepv;
	PEdge *e;
	float angulardefect, angle;

	p_collapsing_verts(edge, pair, &oldv, &keepv);

	angulardefect = 2 * M_PI;

	e = oldv->edge;
	do {
		float a[3], b[3], minangle, maxangle;
		PEdge *e1 = e->next, *e2 = e1->next;
		PVert *v1 = e1->vert, *v2 = e2->vert;
		int i;

		angle = p_vec_angle(v1->co, oldv->co, v2->co);
		angulardefect -= angle;

		/* skip collapsing faces */
		if (v1 == keepv || v2 == keepv) {
			e = p_wheel_edge_next(e);
			continue;
		}

		if (p_collapse_normal_flipped(v1->co, v2->co, oldv->co, keepv->co))
			return P_FALSE;
	
		a[0] = angle;
		a[1] = p_vec_angle(v2->co, v1->co, oldv->co);
		a[2] = M_PI - a[0] - a[1];

		b[0] = p_vec_angle(v1->co, keepv->co, v2->co);
		b[1] = p_vec_angle(v2->co, v1->co, keepv->co);
		b[2] = M_PI - b[0] - b[1];

		/* abf criterion 1: avoid sharp and obtuse angles */
		minangle = 15.0f * M_PI / 180.0f;
		maxangle = M_PI - minangle;

		for (i = 0; i < 3; i++) {
			if ((b[i] < a[i]) && (b[i] < minangle))
				return P_FALSE;
			else if ((b[i] > a[i]) && (b[i] > maxangle))
				return P_FALSE;
		}

		e = p_wheel_edge_next(e);
	} while (e && (e != oldv->edge));

	if (p_vert_interior(oldv)) {
		/* hlscm criterion: angular defect smaller than threshold */
		if (fabs(angulardefect) > (M_PI * 30.0 / 180.0))
			return P_FALSE;
	}
	else {
		PVert *v1 = p_boundary_edge_next(oldv->edge)->vert;
		PVert *v2 = p_boundary_edge_prev(oldv->edge)->vert;

		/* abf++ criterion 2: avoid collapsing verts inwards */
		if (p_vert_interior(keepv))
			return P_FALSE;
		
		/* don't collapse significant boundary changes */
		angle = p_vec_angle(v1->co, oldv->co, v2->co);
		if (angle < (M_PI * 160.0 / 180.0))
			return P_FALSE;
	}

	return P_TRUE;
}

static PBool p_collapse_allowed(PEdge *edge, PEdge *pair)
{
	PVert *oldv, *keepv;

	p_collapsing_verts(edge, pair, &oldv, &keepv);

	if (oldv->flag & PVERT_PIN)
		return P_FALSE;
	
	return (p_collapse_allowed_topologic(edge, pair) &&
	        p_collapse_allowed_geometric(edge, pair));
}

static float p_collapse_cost(PEdge *edge, PEdge *pair)
{
	/* based on volume and boundary optimization from:
	 * "Fast and Memory Efficient Polygonal Simplification" P. Lindstrom, G. Turk */

	PVert *oldv, *keepv;
	PEdge *e;
	PFace *oldf1, *oldf2;
	float volumecost = 0.0f, areacost = 0.0f, edgevec[3], cost, weight, elen;
	float shapecost = 0.0f;
	float shapeold = 0.0f, shapenew = 0.0f;
	int nshapeold = 0, nshapenew = 0;

	p_collapsing_verts(edge, pair, &oldv, &keepv);
	oldf1 = (edge) ? edge->face : NULL;
	oldf2 = (pair) ? pair->face : NULL;

	sub_v3_v3v3(edgevec, keepv->co, oldv->co);

	e = oldv->edge;
	do {
		float a1, a2, a3;
		float *co1 = e->next->vert->co;
		float *co2 = e->next->next->vert->co;

		if ((e->face != oldf1) && (e->face != oldf2)) {
			float tetrav2[3], tetrav3[3], c[3];

			/* tetrahedron volume = (1/3!)*|a.(b x c)| */
			sub_v3_v3v3(tetrav2, co1, oldv->co);
			sub_v3_v3v3(tetrav3, co2, oldv->co);
			cross_v3_v3v3(c, tetrav2, tetrav3);

			volumecost += fabs(dot_v3v3(edgevec, c) / 6.0f);
#if 0
			shapecost += dot_v3v3(co1, keepv->co);

			if (p_wheel_edge_next(e) == NULL)
				shapecost += dot_v3v3(co2, keepv->co);
#endif

			p_triangle_angles(oldv->co, co1, co2, &a1, &a2, &a3);
			a1 = a1 - M_PI / 3.0;
			a2 = a2 - M_PI / 3.0;
			a3 = a3 - M_PI / 3.0;
			shapeold = (a1 * a1 + a2 * a2 + a3 * a3) / ((M_PI / 2) * (M_PI / 2));

			nshapeold++;
		}
		else {
			p_triangle_angles(keepv->co, co1, co2, &a1, &a2, &a3);
			a1 = a1 - M_PI / 3.0;
			a2 = a2 - M_PI / 3.0;
			a3 = a3 - M_PI / 3.0;
			shapenew = (a1 * a1 + a2 * a2 + a3 * a3) / ((M_PI / 2) * (M_PI / 2));

			nshapenew++;
		}

		e = p_wheel_edge_next(e);
	} while (e && (e != oldv->edge));

	if (!p_vert_interior(oldv)) {
		PVert *v1 = p_boundary_edge_prev(oldv->edge)->vert;
		PVert *v2 = p_boundary_edge_next(oldv->edge)->vert;

		areacost = area_tri_v3(oldv->co, v1->co, v2->co);
	}

	elen = len_v3(edgevec);
	weight = 1.0f; /* 0.2f */
	cost = weight * volumecost * volumecost + elen * elen * areacost * areacost;
#if 0
	cost += shapecost;
#else
	shapeold /= nshapeold;
	shapenew /= nshapenew;
	shapecost = (shapeold + 0.00001) / (shapenew + 0.00001);

	cost *= shapecost;
#endif

	return cost;
}
	
static void p_collapse_cost_vertex(PVert *vert, float *mincost, PEdge **mine)
{
	PEdge *e, *enext, *pair;

	*mine = NULL;
	*mincost = 0.0f;
	e = vert->edge;
	do {
		if (p_collapse_allowed(e, e->pair)) {
			float cost = p_collapse_cost(e, e->pair);

			if ((*mine == NULL) || (cost < *mincost)) {
				*mincost = cost;
				*mine = e;
			}
		}

		enext = p_wheel_edge_next(e);

		if (enext == NULL) {
			/* the other boundary edge, where we only have the pair halfedge */
			pair = e->next->next;

			if (p_collapse_allowed(NULL, pair)) {
				float cost = p_collapse_cost(NULL, pair);

				if ((*mine == NULL) || (cost < *mincost)) {
					*mincost = cost;
					*mine = pair;
				}
			}

			break;
		}

		e = enext;
	} while (e != vert->edge);
}

static void p_chart_post_collapse_flush(PChart *chart, PEdge *collapsed)
{
	/* move to collapsed_ */

	PVert *v, *nextv = NULL, *verts = chart->verts;
	PEdge *e, *nexte = NULL, *edges = chart->edges, *laste = NULL;
	PFace *f, *nextf = NULL, *faces = chart->faces;

	chart->verts = chart->collapsed_verts = NULL;
	chart->edges = chart->collapsed_edges = NULL;
	chart->faces = chart->collapsed_faces = NULL;

	chart->nverts = chart->nedges = chart->nfaces = 0;

	for (v = verts; v; v = nextv) {
		nextv = v->nextlink;

		if (v->flag & PVERT_COLLAPSE) {
			v->nextlink = chart->collapsed_verts;
			chart->collapsed_verts = v;
		}
		else {
			v->nextlink = chart->verts;
			chart->verts = v;
			chart->nverts++;
		}
	}

	for (e = edges; e; e = nexte) {
		nexte = e->nextlink;

		if (!collapsed || !(e->flag & PEDGE_COLLAPSE_EDGE)) {
			if (e->flag & PEDGE_COLLAPSE) {
				e->nextlink = chart->collapsed_edges;
				chart->collapsed_edges = e;
			}
			else {
				e->nextlink = chart->edges;
				chart->edges = e;
				chart->nedges++;
			}
		}
	}

	/* these are added last so they can be popped of in the right order
	 * for splitting */
	for (e = collapsed; e; e = e->nextlink) {
		e->nextlink = e->u.nextcollapse;
		laste = e;
	}
	if (laste) {
		laste->nextlink = chart->collapsed_edges;
		chart->collapsed_edges = collapsed;
	}

	for (f = faces; f; f = nextf) {
		nextf = f->nextlink;

		if (f->flag & PFACE_COLLAPSE) {
			f->nextlink = chart->collapsed_faces;
			chart->collapsed_faces = f;
		}
		else {
			f->nextlink = chart->faces;
			chart->faces = f;
			chart->nfaces++;
		}
	}
}

static void p_chart_post_split_flush(PChart *chart)
{
	/* move from collapsed_ */

	PVert *v, *nextv = NULL;
	PEdge *e, *nexte = NULL;
	PFace *f, *nextf = NULL;

	for (v = chart->collapsed_verts; v; v = nextv) {
		nextv = v->nextlink;
		v->nextlink = chart->verts;
		chart->verts = v;
		chart->nverts++;
	}

	for (e = chart->collapsed_edges; e; e = nexte) {
		nexte = e->nextlink;
		e->nextlink = chart->edges;
		chart->edges = e;
		chart->nedges++;
	}

	for (f = chart->collapsed_faces; f; f = nextf) {
		nextf = f->nextlink;
		f->nextlink = chart->faces;
		chart->faces = f;
		chart->nfaces++;
	}

	chart->collapsed_verts = NULL;
	chart->collapsed_edges = NULL;
	chart->collapsed_faces = NULL;
}

static void p_chart_simplify_compute(PChart *chart)
{
	/* Computes a list of edge collapses / vertex splits. The collapsed
	 * simplices go in the chart->collapsed_* lists, The original and
	 * collapsed may then be view as stacks, where the next collapse/split
	 * is at the top of the respective lists. */

	Heap *heap = BLI_heap_new();
	PVert *v, **wheelverts;
	PEdge *collapsededges = NULL, *e;
	int nwheelverts, i, ncollapsed = 0;

	wheelverts = MEM_mallocN(sizeof(PVert *) * chart->nverts, "PChartWheelVerts");

	/* insert all potential collapses into heap */
	for (v = chart->verts; v; v = v->nextlink) {
		float cost;
		PEdge *e = NULL;
		
		p_collapse_cost_vertex(v, &cost, &e);

		if (e)
			v->u.heaplink = BLI_heap_insert(heap, cost, e);
		else
			v->u.heaplink = NULL;
	}

	for (e = chart->edges; e; e = e->nextlink)
		e->u.nextcollapse = NULL;

	/* pop edge collapse out of heap one by one */
	while (!BLI_heap_empty(heap)) {
		if (ncollapsed == NCOLLAPSE)
			break;

		HeapNode *link = BLI_heap_top(heap);
		PEdge *edge = (PEdge *)BLI_heap_popmin(heap), *pair = edge->pair;
		PVert *oldv, *keepv;
		PEdge *wheele, *nexte;

		/* remember the edges we collapsed */
		edge->u.nextcollapse = collapsededges;
		collapsededges = edge;

		if (edge->vert->u.heaplink != link) {
			edge->flag |= (PEDGE_COLLAPSE_EDGE | PEDGE_COLLAPSE_PAIR);
			edge->next->vert->u.heaplink = NULL;
			SWAP(PEdge *, edge, pair);
		}
		else {
			edge->flag |= PEDGE_COLLAPSE_EDGE;
			edge->vert->u.heaplink = NULL;
		}

		p_collapsing_verts(edge, pair, &oldv, &keepv);

		/* gather all wheel verts and remember them before collapse */
		nwheelverts = 0;
		wheele = oldv->edge;

		do {
			wheelverts[nwheelverts++] = wheele->next->vert;
			nexte = p_wheel_edge_next(wheele);

			if (nexte == NULL)
				wheelverts[nwheelverts++] = wheele->next->next->vert;

			wheele = nexte;
		} while (wheele && (wheele != oldv->edge));

		/* collapse */
		p_collapse_edge(edge, pair);

		for (i = 0; i < nwheelverts; i++) {
			float cost;
			PEdge *collapse = NULL;

			v = wheelverts[i];

			if (v->u.heaplink) {
				BLI_heap_remove(heap, v->u.heaplink);
				v->u.heaplink = NULL;
			}
		
			p_collapse_cost_vertex(v, &cost, &collapse);

			if (collapse)
				v->u.heaplink = BLI_heap_insert(heap, cost, collapse);
		}

		ncollapsed++;
	}

	MEM_freeN(wheelverts);
	BLI_heap_free(heap, NULL);

	p_chart_post_collapse_flush(chart, collapsededges);
}

static void p_chart_complexify(PChart *chart)
{
	PEdge *e, *pair, *edge;
	PVert *newv, *keepv;
	int x = 0;

	for (e = chart->collapsed_edges; e; e = e->nextlink) {
		if (!(e->flag & PEDGE_COLLAPSE_EDGE))
			break;

		edge = e;
		pair = e->pair;

		if (edge->flag & PEDGE_COLLAPSE_PAIR) {
			SWAP(PEdge *, edge, pair);
		}

		p_split_vertex(edge, pair);
		p_collapsing_verts(edge, pair, &newv, &keepv);

		if (x >= NCOLLAPSEX) {
			newv->uv[0] = keepv->uv[0];
			newv->uv[1] = keepv->uv[1];
		}
		else {
			p_vert_harmonic_insert(newv);
			x++;
		}
	}

	p_chart_post_split_flush(chart);
}

#if 0
static void p_chart_simplify(PChart *chart)
{
	/* Not implemented, needs proper reordering in split_flush. */
}
#endif
#endif

/* ABF */

#define ABF_MAX_ITER 20

typedef struct PAbfSystem {
	int ninterior, nfaces, nangles;
	float *alpha, *beta, *sine, *cosine, *weight;
	float *bAlpha, *bTriangle, *bInterior;
	float *lambdaTriangle, *lambdaPlanar, *lambdaLength;
	float (*J2dt)[3], *bstar, *dstar;
	float minangle, maxangle;
} PAbfSystem;

static void p_abf_setup_system(PAbfSystem *sys)
{
	int i;

	sys->alpha = (float *)MEM_mallocN(sizeof(float) * sys->nangles, "ABFalpha");
	sys->beta = (float *)MEM_mallocN(sizeof(float) * sys->nangles, "ABFbeta");
	sys->sine = (float *)MEM_mallocN(sizeof(float) * sys->nangles, "ABFsine");
	sys->cosine = (float *)MEM_mallocN(sizeof(float) * sys->nangles, "ABFcosine");
	sys->weight = (float *)MEM_mallocN(sizeof(float) * sys->nangles, "ABFweight");

	sys->bAlpha = (float *)MEM_mallocN(sizeof(float) * sys->nangles, "ABFbalpha");
	sys->bTriangle = (float *)MEM_mallocN(sizeof(float) * sys->nfaces, "ABFbtriangle");
	sys->bInterior = (float *)MEM_mallocN(sizeof(float) * 2 * sys->ninterior, "ABFbinterior");

	sys->lambdaTriangle = (float *)MEM_callocN(sizeof(float) * sys->nfaces, "ABFlambdatri");
	sys->lambdaPlanar = (float *)MEM_callocN(sizeof(float) * sys->ninterior, "ABFlamdaplane");
	sys->lambdaLength = (float *)MEM_mallocN(sizeof(float) * sys->ninterior, "ABFlambdalen");

	sys->J2dt = MEM_mallocN(sizeof(float) * sys->nangles * 3, "ABFj2dt");
	sys->bstar = (float *)MEM_mallocN(sizeof(float) * sys->nfaces, "ABFbstar");
	sys->dstar = (float *)MEM_mallocN(sizeof(float) * sys->nfaces, "ABFdstar");

	for (i = 0; i < sys->ninterior; i++)
		sys->lambdaLength[i] = 1.0;
	
	sys->minangle = 7.5 * M_PI / 180.0;
	sys->maxangle = (float)M_PI - sys->minangle;
}

static void p_abf_free_system(PAbfSystem *sys)
{
	MEM_freeN(sys->alpha);
	MEM_freeN(sys->beta);
	MEM_freeN(sys->sine);
	MEM_freeN(sys->cosine);
	MEM_freeN(sys->weight);
	MEM_freeN(sys->bAlpha);
	MEM_freeN(sys->bTriangle);
	MEM_freeN(sys->bInterior);
	MEM_freeN(sys->lambdaTriangle);
	MEM_freeN(sys->lambdaPlanar);
	MEM_freeN(sys->lambdaLength);
	MEM_freeN(sys->J2dt);
	MEM_freeN(sys->bstar);
	MEM_freeN(sys->dstar);
}

static void p_abf_compute_sines(PAbfSystem *sys)
{
	int i;
	float *sine = sys->sine, *cosine = sys->cosine, *alpha = sys->alpha;

	for (i = 0; i < sys->nangles; i++, sine++, cosine++, alpha++) {
		*sine = sin(*alpha);
		*cosine = cos(*alpha);
	}
}

static float p_abf_compute_sin_product(PAbfSystem *sys, PVert *v, int aid)
{
	PEdge *e, *e1, *e2;
	float sin1, sin2;

	sin1 = sin2 = 1.0;

	e = v->edge;
	do {
		e1 = e->next;
		e2 = e->next->next;

		if (aid == e1->u.id) {
			/* we are computing a derivative for this angle,
			 * so we use cos and drop the other part */
			sin1 *= sys->cosine[e1->u.id];
			sin2 = 0.0;
		}
		else
			sin1 *= sys->sine[e1->u.id];

		if (aid == e2->u.id) {
			/* see above */
			sin1 = 0.0;
			sin2 *= sys->cosine[e2->u.id];
		}
		else
			sin2 *= sys->sine[e2->u.id];

		e = e->next->next->pair;
	} while (e && (e != v->edge));

	return (sin1 - sin2);
}

static float p_abf_compute_grad_alpha(PAbfSystem *sys, PFace *f, PEdge *e)
{
	PVert *v = e->vert, *v1 = e->next->vert, *v2 = e->next->next->vert;
	float deriv;

	deriv = (sys->alpha[e->u.id] - sys->beta[e->u.id]) * sys->weight[e->u.id];
	deriv += sys->lambdaTriangle[f->u.id];

	if (v->flag & PVERT_INTERIOR) {
		deriv += sys->lambdaPlanar[v->u.id];
	}

	if (v1->flag & PVERT_INTERIOR) {
		float product = p_abf_compute_sin_product(sys, v1, e->u.id);
		deriv += sys->lambdaLength[v1->u.id] * product;
	}

	if (v2->flag & PVERT_INTERIOR) {
		float product = p_abf_compute_sin_product(sys, v2, e->u.id);
		deriv += sys->lambdaLength[v2->u.id] * product;
	}

	return deriv;
}

static float p_abf_compute_gradient(PAbfSystem *sys, PChart *chart)
{
	PFace *f;
	PEdge *e;
	PVert *v;
	float norm = 0.0;

	for (f = chart->faces; f; f = f->nextlink) {
		PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
		float gtriangle, galpha1, galpha2, galpha3;

		galpha1 = p_abf_compute_grad_alpha(sys, f, e1);
		galpha2 = p_abf_compute_grad_alpha(sys, f, e2);
		galpha3 = p_abf_compute_grad_alpha(sys, f, e3);

		sys->bAlpha[e1->u.id] = -galpha1;
		sys->bAlpha[e2->u.id] = -galpha2;
		sys->bAlpha[e3->u.id] = -galpha3;

		norm += galpha1 * galpha1 + galpha2 * galpha2 + galpha3 * galpha3;

		gtriangle = sys->alpha[e1->u.id] + sys->alpha[e2->u.id] + sys->alpha[e3->u.id] - (float)M_PI;
		sys->bTriangle[f->u.id] = -gtriangle;
		norm += gtriangle * gtriangle;
	}

	for (v = chart->verts; v; v = v->nextlink) {
		if (v->flag & PVERT_INTERIOR) {
			float gplanar = -2 * M_PI, glength;

			e = v->edge;
			do {
				gplanar += sys->alpha[e->u.id];
				e = e->next->next->pair;
			} while (e && (e != v->edge));

			sys->bInterior[v->u.id] = -gplanar;
			norm += gplanar * gplanar;

			glength = p_abf_compute_sin_product(sys, v, -1);
			sys->bInterior[sys->ninterior + v->u.id] = -glength;
			norm += glength * glength;
		}
	}

	return norm;
}

static PBool p_abf_matrix_invert(PAbfSystem *sys, PChart *chart)
{
	PFace *f;
	PEdge *e;
	int i, j, ninterior = sys->ninterior, nvar = 2 * sys->ninterior;
	PBool success;

	nlNewContext();
	nlSolverParameteri(NL_NB_VARIABLES, nvar);

	nlBegin(NL_SYSTEM);

	nlBegin(NL_MATRIX);

	for (i = 0; i < nvar; i++)
		nlRightHandSideAdd(0, i, sys->bInterior[i]);

	for (f = chart->faces; f; f = f->nextlink) {
		float wi1, wi2, wi3, b, si, beta[3], j2[3][3], W[3][3];
		float row1[6], row2[6], row3[6];
		int vid[6];
		PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
		PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

		wi1 = 1.0f / sys->weight[e1->u.id];
		wi2 = 1.0f / sys->weight[e2->u.id];
		wi3 = 1.0f / sys->weight[e3->u.id];

		/* bstar1 = (J1*dInv*bAlpha - bTriangle) */
		b = sys->bAlpha[e1->u.id] * wi1;
		b += sys->bAlpha[e2->u.id] * wi2;
		b += sys->bAlpha[e3->u.id] * wi3;
		b -= sys->bTriangle[f->u.id];

		/* si = J1*d*J1t */
		si = 1.0f / (wi1 + wi2 + wi3);

		/* J1t*si*bstar1 - bAlpha */
		beta[0] = b * si - sys->bAlpha[e1->u.id];
		beta[1] = b * si - sys->bAlpha[e2->u.id];
		beta[2] = b * si - sys->bAlpha[e3->u.id];

		/* use this later for computing other lambda's */
		sys->bstar[f->u.id] = b;
		sys->dstar[f->u.id] = si;

		/* set matrix */
		W[0][0] = si - sys->weight[e1->u.id]; W[0][1] = si; W[0][2] = si;
		W[1][0] = si; W[1][1] = si - sys->weight[e2->u.id]; W[1][2] = si;
		W[2][0] = si; W[2][1] = si; W[2][2] = si - sys->weight[e3->u.id];

		vid[0] = vid[1] = vid[2] = vid[3] = vid[4] = vid[5] = -1;

		if (v1->flag & PVERT_INTERIOR) {
			vid[0] = v1->u.id;
			vid[3] = ninterior + v1->u.id;

			sys->J2dt[e1->u.id][0] = j2[0][0] = 1.0f * wi1;
			sys->J2dt[e2->u.id][0] = j2[1][0] = p_abf_compute_sin_product(sys, v1, e2->u.id) * wi2;
			sys->J2dt[e3->u.id][0] = j2[2][0] = p_abf_compute_sin_product(sys, v1, e3->u.id) * wi3;

			nlRightHandSideAdd(0, v1->u.id, j2[0][0] * beta[0]);
			nlRightHandSideAdd(0, ninterior + v1->u.id, j2[1][0] * beta[1] + j2[2][0] * beta[2]);

			row1[0] = j2[0][0] * W[0][0];
			row2[0] = j2[0][0] * W[1][0];
			row3[0] = j2[0][0] * W[2][0];

			row1[3] = j2[1][0] * W[0][1] + j2[2][0] * W[0][2];
			row2[3] = j2[1][0] * W[1][1] + j2[2][0] * W[1][2];
			row3[3] = j2[1][0] * W[2][1] + j2[2][0] * W[2][2];
		}

		if (v2->flag & PVERT_INTERIOR) {
			vid[1] = v2->u.id;
			vid[4] = ninterior + v2->u.id;

			sys->J2dt[e1->u.id][1] = j2[0][1] = p_abf_compute_sin_product(sys, v2, e1->u.id) * wi1;
			sys->J2dt[e2->u.id][1] = j2[1][1] = 1.0f * wi2;
			sys->J2dt[e3->u.id][1] = j2[2][1] = p_abf_compute_sin_product(sys, v2, e3->u.id) * wi3;

			nlRightHandSideAdd(0, v2->u.id, j2[1][1] * beta[1]);
			nlRightHandSideAdd(0, ninterior + v2->u.id, j2[0][1] * beta[0] + j2[2][1] * beta[2]);

			row1[1] = j2[1][1] * W[0][1];
			row2[1] = j2[1][1] * W[1][1];
			row3[1] = j2[1][1] * W[2][1];

			row1[4] = j2[0][1] * W[0][0] + j2[2][1] * W[0][2];
			row2[4] = j2[0][1] * W[1][0] + j2[2][1] * W[1][2];
			row3[4] = j2[0][1] * W[2][0] + j2[2][1] * W[2][2];
		}

		if (v3->flag & PVERT_INTERIOR) {
			vid[2] = v3->u.id;
			vid[5] = ninterior + v3->u.id;

			sys->J2dt[e1->u.id][2] = j2[0][2] = p_abf_compute_sin_product(sys, v3, e1->u.id) * wi1;
			sys->J2dt[e2->u.id][2] = j2[1][2] = p_abf_compute_sin_product(sys, v3, e2->u.id) * wi2;
			sys->J2dt[e3->u.id][2] = j2[2][2] = 1.0f * wi3;

			nlRightHandSideAdd(0, v3->u.id, j2[2][2] * beta[2]);
			nlRightHandSideAdd(0, ninterior + v3->u.id, j2[0][2] * beta[0] + j2[1][2] * beta[1]);

			row1[2] = j2[2][2] * W[0][2];
			row2[2] = j2[2][2] * W[1][2];
			row3[2] = j2[2][2] * W[2][2];

			row1[5] = j2[0][2] * W[0][0] + j2[1][2] * W[0][1];
			row2[5] = j2[0][2] * W[1][0] + j2[1][2] * W[1][1];
			row3[5] = j2[0][2] * W[2][0] + j2[1][2] * W[2][1];
		}

		for (i = 0; i < 3; i++) {
			int r = vid[i];

			if (r == -1)
				continue;

			for (j = 0; j < 6; j++) {
				int c = vid[j];

				if (c == -1)
					continue;

				if (i == 0)
					nlMatrixAdd(r, c, j2[0][i] * row1[j]);
				else
					nlMatrixAdd(r + ninterior, c, j2[0][i] * row1[j]);

				if (i == 1)
					nlMatrixAdd(r, c, j2[1][i] * row2[j]);
				else
					nlMatrixAdd(r + ninterior, c, j2[1][i] * row2[j]);


				if (i == 2)
					nlMatrixAdd(r, c, j2[2][i] * row3[j]);
				else
					nlMatrixAdd(r + ninterior, c, j2[2][i] * row3[j]);
			}
		}
	}

	nlEnd(NL_MATRIX);

	nlEnd(NL_SYSTEM);

	success = nlSolve();

	if (success) {
		for (f = chart->faces; f; f = f->nextlink) {
			float dlambda1, pre[3], dalpha;
			PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
			PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

			pre[0] = pre[1] = pre[2] = 0.0;

			if (v1->flag & PVERT_INTERIOR) {
				float x = nlGetVariable(0, v1->u.id);
				float x2 = nlGetVariable(0, ninterior + v1->u.id);
				pre[0] += sys->J2dt[e1->u.id][0] * x;
				pre[1] += sys->J2dt[e2->u.id][0] * x2;
				pre[2] += sys->J2dt[e3->u.id][0] * x2;
			}

			if (v2->flag & PVERT_INTERIOR) {
				float x = nlGetVariable(0, v2->u.id);
				float x2 = nlGetVariable(0, ninterior + v2->u.id);
				pre[0] += sys->J2dt[e1->u.id][1] * x2;
				pre[1] += sys->J2dt[e2->u.id][1] * x;
				pre[2] += sys->J2dt[e3->u.id][1] * x2;
			}

			if (v3->flag & PVERT_INTERIOR) {
				float x = nlGetVariable(0, v3->u.id);
				float x2 = nlGetVariable(0, ninterior + v3->u.id);
				pre[0] += sys->J2dt[e1->u.id][2] * x2;
				pre[1] += sys->J2dt[e2->u.id][2] * x2;
				pre[2] += sys->J2dt[e3->u.id][2] * x;
			}

			dlambda1 = pre[0] + pre[1] + pre[2];
			dlambda1 = sys->dstar[f->u.id] * (sys->bstar[f->u.id] - dlambda1);
			
			sys->lambdaTriangle[f->u.id] += dlambda1;

			dalpha = (sys->bAlpha[e1->u.id] - dlambda1);
			sys->alpha[e1->u.id] += dalpha / sys->weight[e1->u.id] - pre[0];

			dalpha = (sys->bAlpha[e2->u.id] - dlambda1);
			sys->alpha[e2->u.id] += dalpha / sys->weight[e2->u.id] - pre[1];

			dalpha = (sys->bAlpha[e3->u.id] - dlambda1);
			sys->alpha[e3->u.id] += dalpha / sys->weight[e3->u.id] - pre[2];

			/* clamp */
			e = f->edge;
			do {
				if (sys->alpha[e->u.id] > (float)M_PI)
					sys->alpha[e->u.id] = (float)M_PI;
				else if (sys->alpha[e->u.id] < 0.0f)
					sys->alpha[e->u.id] = 0.0f;
			} while (e != f->edge);
		}

		for (i = 0; i < ninterior; i++) {
			sys->lambdaPlanar[i] += nlGetVariable(0, i);
			sys->lambdaLength[i] += nlGetVariable(0, ninterior + i);
		}
	}

	nlDeleteContext(nlGetCurrent());

	return success;
}

static PBool p_chart_abf_solve(PChart *chart)
{
	PVert *v;
	PFace *f;
	PEdge *e, *e1, *e2, *e3;
	PAbfSystem sys;
	int i;
	float /* lastnorm, */ /* UNUSED */ limit = (chart->nfaces > 100) ? 1.0f : 0.001f;

	/* setup id's */
	sys.ninterior = sys.nfaces = sys.nangles = 0;

	for (v = chart->verts; v; v = v->nextlink) {
		if (p_vert_interior(v)) {
			v->flag |= PVERT_INTERIOR;
			v->u.id = sys.ninterior++;
		}
		else
			v->flag &= ~PVERT_INTERIOR;
	}

	for (f = chart->faces; f; f = f->nextlink) {
		e1 = f->edge; e2 = e1->next; e3 = e2->next;
		f->u.id = sys.nfaces++;

		/* angle id's are conveniently stored in half edges */
		e1->u.id = sys.nangles++;
		e2->u.id = sys.nangles++;
		e3->u.id = sys.nangles++;
	}

	p_abf_setup_system(&sys);

	/* compute initial angles */
	for (f = chart->faces; f; f = f->nextlink) {
		float a1, a2, a3;

		e1 = f->edge; e2 = e1->next; e3 = e2->next;
		p_face_angles(f, &a1, &a2, &a3);

		if (a1 < sys.minangle)
			a1 = sys.minangle;
		else if (a1 > sys.maxangle)
			a1 = sys.maxangle;
		if (a2 < sys.minangle)
			a2 = sys.minangle;
		else if (a2 > sys.maxangle)
			a2 = sys.maxangle;
		if (a3 < sys.minangle)
			a3 = sys.minangle;
		else if (a3 > sys.maxangle)
			a3 = sys.maxangle;

		sys.alpha[e1->u.id] = sys.beta[e1->u.id] = a1;
		sys.alpha[e2->u.id] = sys.beta[e2->u.id] = a2;
		sys.alpha[e3->u.id] = sys.beta[e3->u.id] = a3;

		sys.weight[e1->u.id] = 2.0f / (a1 * a1);
		sys.weight[e2->u.id] = 2.0f / (a2 * a2);
		sys.weight[e3->u.id] = 2.0f / (a3 * a3);
	}

	for (v = chart->verts; v; v = v->nextlink) {
		if (v->flag & PVERT_INTERIOR) {
			float anglesum = 0.0, scale;

			e = v->edge;
			do {
				anglesum += sys.beta[e->u.id];
				e = e->next->next->pair;
			} while (e && (e != v->edge));

			scale = (anglesum == 0.0f) ? 0.0f : 2.0f * (float)M_PI / anglesum;

			e = v->edge;
			do {
				sys.beta[e->u.id] = sys.alpha[e->u.id] = sys.beta[e->u.id] * scale;
				e = e->next->next->pair;
			} while (e && (e != v->edge));
		}
	}

	if (sys.ninterior > 0) {
		p_abf_compute_sines(&sys);

		/* iteration */
		/* lastnorm = 1e10; */ /* UNUSED */

		for (i = 0; i < ABF_MAX_ITER; i++) {
			float norm = p_abf_compute_gradient(&sys, chart);

			/* lastnorm = norm; */ /* UNUSED */

			if (norm < limit)
				break;

			if (!p_abf_matrix_invert(&sys, chart)) {
				param_warning("ABF failed to invert matrix");
				p_abf_free_system(&sys);
				return P_FALSE;
			}

			p_abf_compute_sines(&sys);
		}

		if (i == ABF_MAX_ITER) {
			param_warning("ABF maximum iterations reached");
			p_abf_free_system(&sys);
			return P_FALSE;
		}
	}

	chart->u.lscm.abf_alpha = MEM_dupallocN(sys.alpha);
	p_abf_free_system(&sys);

	return P_TRUE;
}

/* Least Squares Conformal Maps */

static void p_chart_pin_positions(PChart *chart, PVert **pin1, PVert **pin2)
{
	if (pin1 == pin2) {
		/* degenerate case */
		PFace *f = chart->faces;
		*pin1 = f->edge->vert;
		*pin2 = f->edge->next->vert;

		(*pin1)->uv[0] = 0.0f;
		(*pin1)->uv[1] = 0.5f;
		(*pin2)->uv[0] = 1.0f;
		(*pin2)->uv[1] = 0.5f;
	}
	else {
		int diru, dirv, dirx, diry;
		float sub[3];

		sub_v3_v3v3(sub, (*pin1)->co, (*pin2)->co);
		sub[0] = fabs(sub[0]);
		sub[1] = fabs(sub[1]);
		sub[2] = fabs(sub[2]);

		if ((sub[0] > sub[1]) && (sub[0] > sub[2])) {
			dirx = 0;
			diry = (sub[1] > sub[2]) ? 1 : 2;
		}
		else if ((sub[1] > sub[0]) && (sub[1] > sub[2])) {
			dirx = 1;
			diry = (sub[0] > sub[2]) ? 0 : 2;
		}
		else {
			dirx = 2;
			diry = (sub[0] > sub[1]) ? 0 : 1;
		}

		if (dirx == 2) {
			diru = 1;
			dirv = 0;
		}
		else {
			diru = 0;
			dirv = 1;
		}

		(*pin1)->uv[diru] = (*pin1)->co[dirx];
		(*pin1)->uv[dirv] = (*pin1)->co[diry];
		(*pin2)->uv[diru] = (*pin2)->co[dirx];
		(*pin2)->uv[dirv] = (*pin2)->co[diry];
	}
}

static PBool p_chart_symmetry_pins(PChart *chart, PEdge *outer, PVert **pin1, PVert **pin2)
{
	PEdge *be, *lastbe = NULL, *maxe1 = NULL, *maxe2 = NULL, *be1, *be2;
	PEdge *cure = NULL, *firste1 = NULL, *firste2 = NULL, *nextbe;
	float maxlen = 0.0f, curlen = 0.0f, totlen = 0.0f, firstlen = 0.0f;
	float len1, len2;
 
	/* find longest series of verts split in the chart itself, these are
	 * marked during construction */
	be = outer;
	lastbe = p_boundary_edge_prev(be);
	do {
		float len = p_edge_length(be);
		totlen += len;

		nextbe = p_boundary_edge_next(be);

		if ((be->vert->flag & PVERT_SPLIT) ||
		    (lastbe->vert->flag & nextbe->vert->flag & PVERT_SPLIT))
		{
			if (!cure) {
				if (be == outer)
					firste1 = be;
				cure = be;
			}
			else
				curlen += p_edge_length(lastbe);
		}
		else if (cure) {
			if (curlen > maxlen) {
				maxlen = curlen;
				maxe1 = cure;
				maxe2 = lastbe;
			}

			if (firste1 == cure) {
				firstlen = curlen;
				firste2 = lastbe;
			}

			curlen = 0.0f;
			cure = NULL;
		}

		lastbe = be;
		be = nextbe;
	} while (be != outer);

	/* make sure we also count a series of splits over the starting point */
	if (cure && (cure != outer)) {
		firstlen += curlen + p_edge_length(be);

		if (firstlen > maxlen) {
			maxlen = firstlen;
			maxe1 = cure;
			maxe2 = firste2;
		}
	}

	if (!maxe1 || !maxe2 || (maxlen < 0.5f * totlen))
		return P_FALSE;
	
	/* find pin1 in the split vertices */
	be1 = maxe1;
	be2 = maxe2;
	len1 = 0.0f;
	len2 = 0.0f;

	do {
		if (len1 < len2) {
			len1 += p_edge_length(be1);
			be1 = p_boundary_edge_next(be1);
		}
		else {
			be2 = p_boundary_edge_prev(be2);
			len2 += p_edge_length(be2);
		}
	} while (be1 != be2);

	*pin1 = be1->vert;

	/* find pin2 outside the split vertices */
	be1 = maxe1;
	be2 = maxe2;
	len1 = 0.0f;
	len2 = 0.0f;

	do {
		if (len1 < len2) {
			be1 = p_boundary_edge_prev(be1);
			len1 += p_edge_length(be1);
		}
		else {
			len2 += p_edge_length(be2);
			be2 = p_boundary_edge_next(be2);
		}
	} while (be1 != be2);

	*pin2 = be1->vert;

	p_chart_pin_positions(chart, pin1, pin2);

	return P_TRUE;
}

static void p_chart_extrema_verts(PChart *chart, PVert **pin1, PVert **pin2)
{
	float minv[3], maxv[3], dirlen;
	PVert *v, *minvert[3], *maxvert[3];
	int i, dir;

	/* find minimum and maximum verts over x/y/z axes */
	minv[0] = minv[1] = minv[2] = 1e20;
	maxv[0] = maxv[1] = maxv[2] = -1e20;

	minvert[0] = minvert[1] = minvert[2] = NULL;
	maxvert[0] = maxvert[1] = maxvert[2] = NULL;

	for (v = chart->verts; v; v = v->nextlink) {
		for (i = 0; i < 3; i++) {
			if (v->co[i] < minv[i]) {
				minv[i] = v->co[i];
				minvert[i] = v;
			}
			if (v->co[i] > maxv[i]) {
				maxv[i] = v->co[i];
				maxvert[i] = v;
			}
		}
	}

	/* find axes with longest distance */
	dir = 0;
	dirlen = -1.0;

	for (i = 0; i < 3; i++) {
		if (maxv[i] - minv[i] > dirlen) {
			dir = i;
			dirlen = maxv[i] - minv[i];
		}
	}

	*pin1 = minvert[dir];
	*pin2 = maxvert[dir];

	p_chart_pin_positions(chart, pin1, pin2);
}

static void p_chart_lscm_load_solution(PChart *chart)
{
	PVert *v;

	for (v = chart->verts; v; v = v->nextlink) {
		v->uv[0] = nlGetVariable(0, 2 * v->u.id);
		v->uv[1] = nlGetVariable(0, 2 * v->u.id + 1);
	}
}

static void p_chart_lscm_begin(PChart *chart, PBool live, PBool abf)
{
	PVert *v, *pin1, *pin2;
	PBool select = P_FALSE, deselect = P_FALSE;
	int npins = 0, id = 0;

	/* give vertices matrix indices and count pins */
	for (v = chart->verts; v; v = v->nextlink) {
		if (v->flag & PVERT_PIN) {
			npins++;
			if (v->flag & PVERT_SELECT)
				select = P_TRUE;
		}

		if (!(v->flag & PVERT_SELECT))
			deselect = P_TRUE;
	}

	if ((live && (!select || !deselect)) || (npins == 1)) {
		chart->u.lscm.context = NULL;
	}
	else {
#if 0
		p_chart_simplify_compute(chart);
		p_chart_topological_sanity_check(chart);
#endif

		if (abf) {
			if (!p_chart_abf_solve(chart))
				param_warning("ABF solving failed: falling back to LSCM.\n");
		}

		if (npins <= 1) {
			/* not enough pins, lets find some ourself */
			PEdge *outer;

			p_chart_boundaries(chart, NULL, &outer);

			if (!p_chart_symmetry_pins(chart, outer, &pin1, &pin2))
				p_chart_extrema_verts(chart, &pin1, &pin2);

			chart->u.lscm.pin1 = pin1;
			chart->u.lscm.pin2 = pin2;
		}
		else {
			chart->flag |= PCHART_NOPACK;
		}

		for (v = chart->verts; v; v = v->nextlink)
			v->u.id = id++;

		nlNewContext();
		nlSolverParameteri(NL_NB_VARIABLES, 2 * chart->nverts);
		nlSolverParameteri(NL_NB_ROWS, 2 * chart->nfaces);
		nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);

		chart->u.lscm.context = nlGetCurrent();
	}
}

static PBool p_chart_lscm_solve(PHandle *handle, PChart *chart)
{
	PVert *v, *pin1 = chart->u.lscm.pin1, *pin2 = chart->u.lscm.pin2;
	PFace *f;
	float *alpha = chart->u.lscm.abf_alpha;
	int row;

	nlMakeCurrent(chart->u.lscm.context);

	nlBegin(NL_SYSTEM);

#if 0
	/* TODO: make loading pins work for simplify/complexify. */
#endif

	for (v = chart->verts; v; v = v->nextlink)
		if (v->flag & PVERT_PIN)
			p_vert_load_pin_select_uvs(handle, v);  /* reload for live */

	if (chart->u.lscm.pin1) {
		nlLockVariable(2 * pin1->u.id);
		nlLockVariable(2 * pin1->u.id + 1);
		nlLockVariable(2 * pin2->u.id);
		nlLockVariable(2 * pin2->u.id + 1);

		nlSetVariable(0, 2 * pin1->u.id, pin1->uv[0]);
		nlSetVariable(0, 2 * pin1->u.id + 1, pin1->uv[1]);
		nlSetVariable(0, 2 * pin2->u.id, pin2->uv[0]);
		nlSetVariable(0, 2 * pin2->u.id + 1, pin2->uv[1]);
	}
	else {
		/* set and lock the pins */
		for (v = chart->verts; v; v = v->nextlink) {
			if (v->flag & PVERT_PIN) {
				nlLockVariable(2 * v->u.id);
				nlLockVariable(2 * v->u.id + 1);

				nlSetVariable(0, 2 * v->u.id, v->uv[0]);
				nlSetVariable(0, 2 * v->u.id + 1, v->uv[1]);
			}
		}
	}

	/* construct matrix */

	nlBegin(NL_MATRIX);

	row = 0;
	for (f = chart->faces; f; f = f->nextlink) {
		PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
		PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;
		float a1, a2, a3, ratio, cosine, sine;
		float sina1, sina2, sina3, sinmax;

		if (alpha) {
			/* use abf angles if passed on */
			a1 = *(alpha++);
			a2 = *(alpha++);
			a3 = *(alpha++);
		}
		else
			p_face_angles(f, &a1, &a2, &a3);

		sina1 = sin(a1);
		sina2 = sin(a2);
		sina3 = sin(a3);

		sinmax = MAX3(sina1, sina2, sina3);

		/* shift vertices to find most stable order */
		if (sina3 != sinmax) {
			SHIFT3(PVert *, v1, v2, v3);
			SHIFT3(float, a1, a2, a3);
			SHIFT3(float, sina1, sina2, sina3);

			if (sina2 == sinmax) {
				SHIFT3(PVert *, v1, v2, v3);
				SHIFT3(float, a1, a2, a3);
				SHIFT3(float, sina1, sina2, sina3);
			}
		}

		/* angle based lscm formulation */
		ratio = (sina3 == 0.0f) ? 1.0f : sina2 / sina3;
		cosine = cosf(a1) * ratio;
		sine = sina1 * ratio;

#if 0
		nlBegin(NL_ROW);
		nlCoefficient(2 * v1->u.id,   cosine - 1.0);
		nlCoefficient(2 * v1->u.id + 1, -sine);
		nlCoefficient(2 * v2->u.id,   -cosine);
		nlCoefficient(2 * v2->u.id + 1, sine);
		nlCoefficient(2 * v3->u.id,   1.0);
		nlEnd(NL_ROW);

		nlBegin(NL_ROW);
		nlCoefficient(2 * v1->u.id,   sine);
		nlCoefficient(2 * v1->u.id + 1, cosine - 1.0);
		nlCoefficient(2 * v2->u.id,   -sine);
		nlCoefficient(2 * v2->u.id + 1, -cosine);
		nlCoefficient(2 * v3->u.id + 1, 1.0);
		nlEnd(NL_ROW);
#else
		nlMatrixAdd(row, 2 * v1->u.id,   cosine - 1.0f);
		nlMatrixAdd(row, 2 * v1->u.id + 1, -sine);
		nlMatrixAdd(row, 2 * v2->u.id,   -cosine);
		nlMatrixAdd(row, 2 * v2->u.id + 1, sine);
		nlMatrixAdd(row, 2 * v3->u.id,   1.0);
		row++;

		nlMatrixAdd(row, 2 * v1->u.id,   sine);
		nlMatrixAdd(row, 2 * v1->u.id + 1, cosine - 1.0f);
		nlMatrixAdd(row, 2 * v2->u.id,   -sine);
		nlMatrixAdd(row, 2 * v2->u.id + 1, -cosine);
		nlMatrixAdd(row, 2 * v3->u.id + 1, 1.0);
		row++;
#endif
	}

	nlEnd(NL_MATRIX);

	nlEnd(NL_SYSTEM);

	if (nlSolveAdvanced(NULL, NL_TRUE)) {
		p_chart_lscm_load_solution(chart);
		return P_TRUE;
	}
	else {
		for (v = chart->verts; v; v = v->nextlink) {
			v->uv[0] = 0.0f;
			v->uv[1] = 0.0f;
		}
	}

	return P_FALSE;
}

static void p_chart_lscm_end(PChart *chart)
{
	if (chart->u.lscm.context)
		nlDeleteContext(chart->u.lscm.context);
	
	if (chart->u.lscm.abf_alpha) {
		MEM_freeN(chart->u.lscm.abf_alpha);
		chart->u.lscm.abf_alpha = NULL;
	}

	chart->u.lscm.context = NULL;
	chart->u.lscm.pin1 = NULL;
	chart->u.lscm.pin2 = NULL;
}

/* Stretch */

#define P_STRETCH_ITER 20

static void p_stretch_pin_boundary(PChart *chart)
{
	PVert *v;

	for (v = chart->verts; v; v = v->nextlink)
		if (v->edge->pair == NULL)
			v->flag |= PVERT_PIN;
		else
			v->flag &= ~PVERT_PIN;
}

static float p_face_stretch(PFace *f)
{
	float T, w, tmp[3];
	float Ps[3], Pt[3];
	float a, c, area;
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
	PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

	area = p_face_uv_area_signed(f);

	if (area <= 0.0f) /* flipped face -> infinite stretch */
		return 1e10f;
	
	w = 1.0f / (2.0f * area);

	/* compute derivatives */
	copy_v3_v3(Ps, v1->co);
	mul_v3_fl(Ps, (v2->uv[1] - v3->uv[1]));

	copy_v3_v3(tmp, v2->co);
	mul_v3_fl(tmp, (v3->uv[1] - v1->uv[1]));
	add_v3_v3(Ps, tmp);

	copy_v3_v3(tmp, v3->co);
	mul_v3_fl(tmp, (v1->uv[1] - v2->uv[1]));
	add_v3_v3(Ps, tmp);

	mul_v3_fl(Ps, w);

	copy_v3_v3(Pt, v1->co);
	mul_v3_fl(Pt, (v3->uv[0] - v2->uv[0]));

	copy_v3_v3(tmp, v2->co);
	mul_v3_fl(tmp, (v1->uv[0] - v3->uv[0]));
	add_v3_v3(Pt, tmp);

	copy_v3_v3(tmp, v3->co);
	mul_v3_fl(tmp, (v2->uv[0] - v1->uv[0]));
	add_v3_v3(Pt, tmp);

	mul_v3_fl(Pt, w);

	/* Sander Tensor */
	a = dot_v3v3(Ps, Ps);
	c = dot_v3v3(Pt, Pt);

	T =  sqrt(0.5f * (a + c));
	if (f->flag & PFACE_FILLED)
		T *= 0.2f;

	return T;
}

static float p_stretch_compute_vertex(PVert *v)
{
	PEdge *e = v->edge;
	float sum = 0.0f;

	do {
		sum += p_face_stretch(e->face);
		e = p_wheel_edge_next(e);
	} while (e && e != (v->edge));

	return sum;
}

static void p_chart_stretch_minimize(PChart *chart, RNG *rng)
{
	PVert *v;
	PEdge *e;
	int j, nedges;
	float orig_stretch, low, stretch_low, high, stretch_high, mid, stretch;
	float orig_uv[2], dir[2], random_angle, trusted_radius;

	for (v = chart->verts; v; v = v->nextlink) {
		if ((v->flag & PVERT_PIN) || !(v->flag & PVERT_SELECT))
			continue;

		orig_stretch = p_stretch_compute_vertex(v);
		orig_uv[0] = v->uv[0];
		orig_uv[1] = v->uv[1];

		/* move vertex in a random direction */
		trusted_radius = 0.0f;
		nedges = 0;
		e = v->edge;

		do {
			trusted_radius += p_edge_uv_length(e);
			nedges++;

			e = p_wheel_edge_next(e);
		} while (e && e != (v->edge));

		trusted_radius /= 2 * nedges;

		random_angle = rng_getFloat(rng) * 2.0f * (float)M_PI;
		dir[0] = trusted_radius * cosf(random_angle);
		dir[1] = trusted_radius * sinf(random_angle);

		/* calculate old and new stretch */
		low = 0;
		stretch_low = orig_stretch;

		add_v2_v2v2(v->uv, orig_uv, dir);
		high = 1;
		stretch = stretch_high = p_stretch_compute_vertex(v);

		/* binary search for lowest stretch position */
		for (j = 0; j < P_STRETCH_ITER; j++) {
			mid = 0.5f * (low + high);
			v->uv[0] = orig_uv[0] + mid * dir[0];
			v->uv[1] = orig_uv[1] + mid * dir[1];
			stretch = p_stretch_compute_vertex(v);

			if (stretch_low < stretch_high) {
				high = mid;
				stretch_high = stretch;
			}
			else {
				low = mid;
				stretch_low = stretch;
			}
		}

		/* no luck, stretch has increased, reset to old values */
		if (stretch >= orig_stretch)
			copy_v2_v2(v->uv, orig_uv);
	}
}

/* Minimum area enclosing rectangle for packing */

static int p_compare_geometric_uv(const void *a, const void *b)
{
	PVert *v1 = *(PVert **)a;
	PVert *v2 = *(PVert **)b;

	if (v1->uv[0] < v2->uv[0])
		return -1;
	else if (v1->uv[0] == v2->uv[0]) {
		if (v1->uv[1] < v2->uv[1])
			return -1;
		else if (v1->uv[1] == v2->uv[1])
			return 0;
		else
			return 1;
	}
	else
		return 1;
}

static PBool p_chart_convex_hull(PChart *chart, PVert ***verts, int *nverts, int *right)
{
	/* Graham algorithm, taken from:
	 * http://aspn.activestate.com/ASPN/Cookbook/Python/Recipe/117225 */

	PEdge *be, *e;
	int npoints = 0, i, ulen, llen;
	PVert **U, **L, **points, **p;

	p_chart_boundaries(chart, NULL, &be);

	if (!be)
		return P_FALSE;

	e = be;
	do {
		npoints++;
		e = p_boundary_edge_next(e);
	} while (e != be);

	p = points = (PVert **)MEM_mallocN(sizeof(PVert *) * npoints * 2, "PCHullpoints");
	U = (PVert **)MEM_mallocN(sizeof(PVert *) * npoints, "PCHullU");
	L = (PVert **)MEM_mallocN(sizeof(PVert *) * npoints, "PCHullL");

	e = be;
	do {
		*p = e->vert;
		p++;
		e = p_boundary_edge_next(e);
	} while (e != be);

	qsort(points, npoints, sizeof(PVert *), p_compare_geometric_uv);

	ulen = llen = 0;
	for (p = points, i = 0; i < npoints; i++, p++) {
		while ((ulen > 1) && (p_area_signed(U[ulen - 2]->uv, (*p)->uv, U[ulen - 1]->uv) <= 0))
			ulen--;
		while ((llen > 1) && (p_area_signed(L[llen - 2]->uv, (*p)->uv, L[llen - 1]->uv) >= 0))
			llen--;

		U[ulen] = *p;
		ulen++;
		L[llen] = *p;
		llen++;
	}

	npoints = 0;
	for (p = points, i = 0; i < ulen; i++, p++, npoints++)
		*p = U[i];

	/* the first and last point in L are left out, since they are also in U */
	for (i = llen - 2; i > 0; i--, p++, npoints++)
		*p = L[i];

	*verts = points;
	*nverts = npoints;
	*right = ulen - 1;

	MEM_freeN(U);
	MEM_freeN(L);

	return P_TRUE;
}

static float p_rectangle_area(float *p1, float *dir, float *p2, float *p3, float *p4)
{
	/* given 4 points on the rectangle edges and the direction of on edge,
	 * compute the area of the rectangle */

	float orthodir[2], corner1[2], corner2[2], corner3[2];

	orthodir[0] = dir[1];
	orthodir[1] = -dir[0];

	if (!p_intersect_line_2d_dir(p1, dir, p2, orthodir, corner1))
		return 1e10;

	if (!p_intersect_line_2d_dir(p1, dir, p4, orthodir, corner2))
		return 1e10;

	if (!p_intersect_line_2d_dir(p3, dir, p4, orthodir, corner3))
		return 1e10;

	return len_v2v2(corner1, corner2) * len_v2v2(corner2, corner3);
}

static float p_chart_minimum_area_angle(PChart *chart)
{
	/* minimum area enclosing rectangle with rotating calipers, info:
	 * http://cgm.cs.mcgill.ca/~orm/maer.html */

	float rotated, minarea, minangle, area, len;
	float *angles, miny, maxy, v[2], a[4], mina;
	int npoints, right, mini, maxi, i, idx[4], nextidx;
	PVert **points, *p1, *p2, *p3, *p4, *p1n;

	/* compute convex hull */
	if (!p_chart_convex_hull(chart, &points, &npoints, &right))
		return 0.0;

	/* find left/top/right/bottom points, and compute angle for each point */
	angles = MEM_mallocN(sizeof(float) * npoints, "PMinAreaAngles");

	mini = maxi = 0;
	miny = 1e10;
	maxy = -1e10;

	for (i = 0; i < npoints; i++) {
		p1 = (i == 0) ? points[npoints - 1] : points[i - 1];
		p2 = points[i];
		p3 = (i == npoints - 1) ? points[0] : points[i + 1];

		angles[i] = (float)M_PI - p_vec2_angle(p1->uv, p2->uv, p3->uv);

		if (points[i]->uv[1] < miny) {
			miny = points[i]->uv[1];
			mini = i;
		}
		if (points[i]->uv[1] > maxy) {
			maxy = points[i]->uv[1];
			maxi = i;
		}
	}

	/* left, top, right, bottom */
	idx[0] = 0;
	idx[1] = maxi;
	idx[2] = right;
	idx[3] = mini;

	v[0] = points[idx[0]]->uv[0];
	v[1] = points[idx[0]]->uv[1] + 1.0f;
	a[0] = p_vec2_angle(points[(idx[0] + 1) % npoints]->uv, points[idx[0]]->uv, v);

	v[0] = points[idx[1]]->uv[0] + 1.0f;
	v[1] = points[idx[1]]->uv[1];
	a[1] = p_vec2_angle(points[(idx[1] + 1) % npoints]->uv, points[idx[1]]->uv, v);

	v[0] = points[idx[2]]->uv[0];
	v[1] = points[idx[2]]->uv[1] - 1.0f;
	a[2] = p_vec2_angle(points[(idx[2] + 1) % npoints]->uv, points[idx[2]]->uv, v);

	v[0] = points[idx[3]]->uv[0] - 1.0f;
	v[1] = points[idx[3]]->uv[1];
	a[3] = p_vec2_angle(points[(idx[3] + 1) % npoints]->uv, points[idx[3]]->uv, v);

	/* 4 rotating calipers */

	rotated = 0.0;
	minarea = 1e10;
	minangle = 0.0;

	while (rotated <= (float)(M_PI / 2.0)) { /* INVESTIGATE: how far to rotate? */
		/* rotate with the smallest angle */
		mini = 0;
		mina = 1e10;

		for (i = 0; i < 4; i++)
			if (a[i] < mina) {
				mina = a[i];
				mini = i;
			}

		rotated += mina;
		nextidx = (idx[mini] + 1) % npoints;

		a[mini] = angles[nextidx];
		a[(mini + 1) % 4] = a[(mini + 1) % 4] - mina;
		a[(mini + 2) % 4] = a[(mini + 2) % 4] - mina;
		a[(mini + 3) % 4] = a[(mini + 3) % 4] - mina;

		/* compute area */
		p1 = points[idx[mini]];
		p1n = points[nextidx];
		p2 = points[idx[(mini + 1) % 4]];
		p3 = points[idx[(mini + 2) % 4]];
		p4 = points[idx[(mini + 3) % 4]];

		len = len_v2v2(p1->uv, p1n->uv);

		if (len > 0.0f) {
			len = 1.0f / len;
			v[0] = (p1n->uv[0] - p1->uv[0]) * len;
			v[1] = (p1n->uv[1] - p1->uv[1]) * len;

			area = p_rectangle_area(p1->uv, v, p2->uv, p3->uv, p4->uv);

			/* remember smallest area */
			if (area < minarea) {
				minarea = area;
				minangle = rotated;
			}
		}

		idx[mini] = nextidx;
	}

	/* try keeping rotation as small as possible */
	if (minangle > (float)(M_PI / 4))
		minangle -= (float)(M_PI / 2.0);

	MEM_freeN(angles);
	MEM_freeN(points);

	return minangle;
}

static void p_chart_rotate_minimum_area(PChart *chart)
{
	float angle = p_chart_minimum_area_angle(chart);
	float sine = sin(angle);
	float cosine = cos(angle);
	PVert *v;

	for (v = chart->verts; v; v = v->nextlink) {
		float oldu = v->uv[0], oldv = v->uv[1];
		v->uv[0] = cosine * oldu - sine * oldv;
		v->uv[1] = sine * oldu + cosine * oldv;
	}
}

/* Area Smoothing */

/* 2d bsp tree for inverse mapping - that's a bit silly */

typedef struct SmoothTriangle {
	float co1[2], co2[2], co3[2];
	float oco1[2], oco2[2], oco3[2];
} SmoothTriangle;

typedef struct SmoothNode {
	struct SmoothNode *c1, *c2;
	SmoothTriangle **tri;
	float split;
	int axis, ntri;
} SmoothNode;

static void p_barycentric_2d(float *v1, float *v2, float *v3, float *p, float *b)
{
	float a[2], c[2], h[2], div;

	a[0] = v2[0] - v1[0];
	a[1] = v2[1] - v1[1];
	c[0] = v3[0] - v1[0];
	c[1] = v3[1] - v1[1];

	div = a[0] * c[1] - a[1] * c[0];

	if (div == 0.0f) {
		b[0] = 1.0f / 3.0f;
		b[1] = 1.0f / 3.0f;
		b[2] = 1.0f / 3.0f;
	}
	else {
		h[0] = p[0] - v1[0];
		h[1] = p[1] - v1[1];

		div = 1.0f / div;

		b[1] = (h[0] * c[1] - h[1] * c[0]) * div;
		b[2] = (a[0] * h[1] - a[1] * h[0]) * div;
		b[0] = 1.0f - b[1] - b[2];
	}
}

static PBool p_triangle_inside(SmoothTriangle *t, float *co)
{
	float b[3];

	p_barycentric_2d(t->co1, t->co2, t->co3, co, b);

	if ((b[0] >= 0.0f) && (b[1] >= 0.0f) && (b[2] >= 0.0f)) {
		co[0] = t->oco1[0] * b[0] + t->oco2[0] * b[1] + t->oco3[0] * b[2];
		co[1] = t->oco1[1] * b[0] + t->oco2[1] * b[1] + t->oco3[1] * b[2];
		return P_TRUE;
	}

	return P_FALSE;
}

static SmoothNode *p_node_new(MemArena *arena, SmoothTriangle **tri, int ntri, float *bmin, float *bmax, int depth)
{
	SmoothNode *node = BLI_memarena_alloc(arena, sizeof *node);
	int axis, i, t1size = 0, t2size = 0;
	float split, /* mi, */ /* UNUSED */ mx;
	SmoothTriangle **t1, **t2, *t;

	node->tri = tri;
	node->ntri = ntri;

	if (ntri <= 10 || depth >= 15)
		return node;
	
	t1 = MEM_mallocN(sizeof(SmoothTriangle) * ntri, "PNodeTri1");
	t2 = MEM_mallocN(sizeof(SmoothTriangle) * ntri, "PNodeTri1");

	axis = (bmax[0] - bmin[0] > bmax[1] - bmin[1]) ? 0 : 1;
	split = 0.5f * (bmin[axis] + bmax[axis]);

	for (i = 0; i < ntri; i++) {
		t = tri[i];

		if ((t->co1[axis] <= split) || (t->co2[axis] <= split) || (t->co3[axis] <= split)) {
			t1[t1size] = t;
			t1size++;
		}
		if ((t->co1[axis] >= split) || (t->co2[axis] >= split) || (t->co3[axis] >= split)) {
			t2[t2size] = t;
			t2size++;
		}
	}

	if ((t1size == t2size) && (t1size == ntri)) {
		MEM_freeN(t1);
		MEM_freeN(t2);
		return node;
	}
	
	node->tri = NULL;
	node->ntri = 0;
	MEM_freeN(tri);

	node->axis = axis;
	node->split = split;

	/* mi = bmin[axis]; */ /* UNUSED */
	mx = bmax[axis];
	bmax[axis] = split;
	node->c1 = p_node_new(arena, t1, t1size, bmin, bmax, depth + 1);

	bmin[axis] = bmax[axis];
	bmax[axis] = mx;
	node->c2 = p_node_new(arena, t2, t2size, bmin, bmax, depth + 1);

	return node;
}

static void p_node_delete(SmoothNode *node)
{
	if (node->c1)
		p_node_delete(node->c1);
	if (node->c2)
		p_node_delete(node->c2);
	if (node->tri)
		MEM_freeN(node->tri);
}

static PBool p_node_intersect(SmoothNode *node, float *co)
{
	int i;

	if (node->tri) {
		for (i = 0; i < node->ntri; i++)
			if (p_triangle_inside(node->tri[i], co))
				return P_TRUE;

		return P_FALSE;
	}
	else {
		if (co[node->axis] < node->split)
			return p_node_intersect(node->c1, co);
		else
			return p_node_intersect(node->c2, co);
	}

}

/* smooothing */

static int p_compare_float(const void *a, const void *b)
{
	if (*((float *)a) < *((float *)b))
		return -1;
	else if (*((float *)a) == *((float *)b))
		return 0;
	else
		return 1;
}

static float p_smooth_median_edge_length(PChart *chart)
{
	PEdge *e;
	float *lengths = MEM_mallocN(sizeof(chart->edges) * chart->nedges, "PMedianLength");
	float median;
	int i;

	/* ok, so i'm lazy */
	for (i = 0, e = chart->edges; e; e = e->nextlink, i++)
		lengths[i] = p_edge_length(e);
	
	qsort(lengths, i, sizeof(float), p_compare_float);

	median = lengths[i / 2];
	MEM_freeN(lengths);

	return median;
}

static float p_smooth_distortion(PEdge *e, float avg2d, float avg3d)
{
	float len2d = p_edge_uv_length(e) * avg3d;
	float len3d = p_edge_length(e) * avg2d;

	return (len3d == 0.0f) ? 0.0f : len2d / len3d;
}

static void p_smooth(PChart *chart)
{
	PEdge *e;
	PVert *v;
	PFace *f;
	int j, it2, maxiter2, it;
	int nedges = chart->nedges, nwheel, gridx, gridy;
	int edgesx, edgesy, nsize, esize, i, x, y, maxiter, totiter;
	float minv[2], maxv[2], median, invmedian, avglen2d, avglen3d;
	float center[2], dx, dy, *nodes, dlimit, d, *oldnodesx, *oldnodesy;
	float *nodesx, *nodesy, *hedges, *vedges, climit, moved, padding;
	SmoothTriangle *triangles, *t, *t2, **tri, **trip;
	SmoothNode *root;
	MemArena *arena;

	if (nedges == 0)
		return;

	p_chart_uv_bbox(chart, minv, maxv);
	median = p_smooth_median_edge_length(chart) * 0.10f;

	if (median == 0.0f)
		return;

	invmedian = 1.0f / median;

	/* compute edge distortion */
	avglen2d = avglen3d = 0.0;

	for (e = chart->edges; e; e = e->nextlink) {
		avglen2d += p_edge_uv_length(e);
		avglen3d += p_edge_length(e);
	}

	avglen2d /= nedges;
	avglen3d /= nedges;

	for (v = chart->verts; v; v = v->nextlink) {
		v->u.distortion = 0.0;
		nwheel = 0;

		e = v->edge;
		do {
			v->u.distortion += p_smooth_distortion(e, avglen2d, avglen3d);
			nwheel++;

			e = e->next->next->pair;
		} while (e && (e != v->edge));

		v->u.distortion /= nwheel;
	}

	/* need to do excessive grid size checking still */
	center[0] = 0.5f * (minv[0] + maxv[0]);
	center[1] = 0.5f * (minv[1] + maxv[1]);

	dx = 0.5f * (maxv[0] - minv[0]);
	dy = 0.5f * (maxv[1] - minv[1]);

	padding = 0.15f;
	dx += padding * dx + 2.0f * median;
	dy += padding * dy + 2.0f * median;

	gridx = (int)(dx * invmedian);
	gridy = (int)(dy * invmedian);

	minv[0] = center[0] - median * gridx;
	minv[1] = center[1] - median * gridy;
	maxv[0] = center[0] + median * gridx;
	maxv[1] = center[1] + median * gridy;

	/* create grid */
	gridx = gridx * 2 + 1;
	gridy = gridy * 2 + 1;

	if ((gridx <= 2) || (gridy <= 2))
		return;
	
	edgesx = gridx - 1;
	edgesy = gridy - 1;
	nsize = gridx * gridy;
	esize = edgesx * edgesy;

	nodes = MEM_mallocN(sizeof(float) * nsize, "PSmoothNodes");
	nodesx = MEM_mallocN(sizeof(float) * nsize, "PSmoothNodesX");
	nodesy = MEM_mallocN(sizeof(float) * nsize, "PSmoothNodesY");
	oldnodesx = MEM_mallocN(sizeof(float) * nsize, "PSmoothOldNodesX");
	oldnodesy = MEM_mallocN(sizeof(float) * nsize, "PSmoothOldNodesY");
	hedges = MEM_mallocN(sizeof(float) * esize, "PSmoothHEdges");
	vedges = MEM_mallocN(sizeof(float) * esize, "PSmoothVEdges");

	if (!nodes || !nodesx || !nodesy || !oldnodesx || !oldnodesy || !hedges || !vedges) {
		if (nodes) MEM_freeN(nodes);
		if (nodesx) MEM_freeN(nodesx);
		if (nodesy) MEM_freeN(nodesy);
		if (oldnodesx) MEM_freeN(oldnodesx);
		if (oldnodesy) MEM_freeN(oldnodesy);
		if (hedges) MEM_freeN(hedges);
		if (vedges) MEM_freeN(vedges);

		// printf("Not enough memory for area smoothing grid");
		return;
	}

	for (x = 0; x < gridx; x++) {
		for (y = 0; y < gridy; y++) {
			i = x + y * gridx;

			nodesx[i] = minv[0] + median * x;
			nodesy[i] = minv[1] + median * y;

			nodes[i] = 1.0f;
		}
	}

	/* embed in grid */
	for (f = chart->faces; f; f = f->nextlink) {
		PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
		float fmin[2], fmax[2];
		int bx1, by1, bx2, by2;

		INIT_MINMAX2(fmin, fmax);

		DO_MINMAX2(e1->vert->uv, fmin, fmax);
		DO_MINMAX2(e2->vert->uv, fmin, fmax);
		DO_MINMAX2(e3->vert->uv, fmin, fmax);

		bx1 = (int)((fmin[0] - minv[0]) * invmedian);
		by1 = (int)((fmin[1] - minv[1]) * invmedian);
		bx2 = (int)((fmax[0] - minv[0]) * invmedian + 2);
		by2 = (int)((fmax[1] - minv[1]) * invmedian + 2);

		for (x = bx1; x < bx2; x++) {
			for (y = by1; y < by2; y++) {
				float p[2], b[3];

				i = x + y * gridx;
		
				p[0] = nodesx[i];
				p[1] = nodesy[i];

				p_barycentric_2d(e1->vert->uv, e2->vert->uv, e3->vert->uv, p, b);

				if ((b[0] > 0.0f) && (b[1] > 0.0f) && (b[2] > 0.0f)) {
					nodes[i] = e1->vert->u.distortion * b[0];
					nodes[i] += e2->vert->u.distortion * b[1];
					nodes[i] += e3->vert->u.distortion * b[2];
				}
			}
		}
	}

	/* smooth the grid */
	maxiter = 10;
	totiter = 0;
	climit = 0.00001f * nsize;

	for (it = 0; it < maxiter; it++) {
		moved = 0.0f;

		for (x = 0; x < edgesx; x++) {
			for (y = 0; y < edgesy; y++) {
				i = x + y * gridx;
				j = x + y * edgesx;

				hedges[j] = (nodes[i] + nodes[i + 1]) * 0.5f;
				vedges[j] = (nodes[i] + nodes[i + gridx]) * 0.5f;

				/* we do *inverse* mapping */
				hedges[j] = 1.0f / hedges[j];
				vedges[j] = 1.0f / vedges[j];
			}
		}

		maxiter2 = 50;
		dlimit = 0.0001f;

		for (it2 = 0; it2 < maxiter2; it2++) {
			d = 0.0f;
			totiter += 1;
			
			memcpy(oldnodesx, nodesx, sizeof(float) * nsize);
			memcpy(oldnodesy, nodesy, sizeof(float) * nsize);

			for (x = 1; x < gridx - 1; x++) {
				for (y = 1; y < gridy - 1; y++) {
					float p[2], oldp[2], sum1, sum2, diff[2], length;

					i = x + gridx * y;
					j = x + edgesx * y;

					oldp[0] = oldnodesx[i];
					oldp[1] = oldnodesy[i];

					sum1 = hedges[j - 1] * oldnodesx[i - 1];
					sum1 += hedges[j] * oldnodesx[i + 1];
					sum1 += vedges[j - edgesx] * oldnodesx[i - gridx];
					sum1 += vedges[j] * oldnodesx[i + gridx];

					sum2 = hedges[j - 1];
					sum2 += hedges[j];
					sum2 += vedges[j - edgesx];
					sum2 += vedges[j];

					nodesx[i] = sum1 / sum2;

					sum1 = hedges[j - 1] * oldnodesy[i - 1];
					sum1 += hedges[j] * oldnodesy[i + 1];
					sum1 += vedges[j - edgesx] * oldnodesy[i - gridx];
					sum1 += vedges[j] * oldnodesy[i + gridx];

					nodesy[i] = sum1 / sum2;
					
					p[0] = nodesx[i];
					p[1] = nodesy[i];

					diff[0] = p[0] - oldp[0];
					diff[1] = p[1] - oldp[1];

					length = sqrt(diff[0] * diff[0] + diff[1] * diff[1]);
					d = MAX2(d, length);
					moved += length;
				}
			}

			if (d < dlimit)
				break;
		}

		if (moved < climit)
			break;
	}

	MEM_freeN(oldnodesx);
	MEM_freeN(oldnodesy);
	MEM_freeN(hedges);
	MEM_freeN(vedges);

	/* create bsp */
	t = triangles = MEM_mallocN(sizeof(SmoothTriangle) * esize * 2, "PSmoothTris");
	trip = tri = MEM_mallocN(sizeof(SmoothTriangle *) * esize * 2, "PSmoothTriP");

	if (!triangles || !tri) {
		MEM_freeN(nodes);
		MEM_freeN(nodesx);
		MEM_freeN(nodesy);

		if (triangles) MEM_freeN(triangles);
		if (tri) MEM_freeN(tri);

		// printf("Not enough memory for area smoothing grid");
		return;
	}

	for (x = 0; x < edgesx; x++) {
		for (y = 0; y < edgesy; y++) {
			i = x + y * gridx;

			t->co1[0] = nodesx[i];
			t->co1[1] = nodesy[i];

			t->co2[0] = nodesx[i + 1];
			t->co2[1] = nodesy[i + 1];

			t->co3[0] = nodesx[i + gridx];
			t->co3[1] = nodesy[i + gridx];

			t->oco1[0] = minv[0] + x * median;
			t->oco1[1] = minv[1] + y * median;

			t->oco2[0] = minv[0] + (x + 1) * median;
			t->oco2[1] = minv[1] + y * median;

			t->oco3[0] = minv[0] + x * median;
			t->oco3[1] = minv[1] + (y + 1) * median;

			t2 = t + 1;

			t2->co1[0] = nodesx[i + gridx + 1];
			t2->co1[1] = nodesy[i + gridx + 1];

			t2->oco1[0] = minv[0] + (x + 1) * median;
			t2->oco1[1] = minv[1] + (y + 1) * median;

			t2->co2[0] = t->co2[0]; t2->co2[1] = t->co2[1];
			t2->oco2[0] = t->oco2[0]; t2->oco2[1] = t->oco2[1];

			t2->co3[0] = t->co3[0]; t2->co3[1] = t->co3[1];
			t2->oco3[0] = t->oco3[0]; t2->oco3[1] = t->oco3[1];

			*trip = t; trip++; t++; 
			*trip = t; trip++; t++; 
		}
	}

	MEM_freeN(nodes);
	MEM_freeN(nodesx);
	MEM_freeN(nodesy);

	arena = BLI_memarena_new(1 << 16, "param smooth arena");
	root = p_node_new(arena, tri, esize * 2, minv, maxv, 0);

	for (v = chart->verts; v; v = v->nextlink)
		if (!p_node_intersect(root, v->uv))
			param_warning("area smoothing error: couldn't find mapping triangle\n");

	p_node_delete(root);
	BLI_memarena_free(arena);
	
	MEM_freeN(triangles);
}

/* Exported */

ParamHandle *param_construct_begin(void)
{
	PHandle *handle = MEM_callocN(sizeof *handle, "PHandle");
	handle->construction_chart = p_chart_new(handle);
	handle->state = PHANDLE_STATE_ALLOCATED;
	handle->arena = BLI_memarena_new((1 << 16), "param construct arena");
	handle->aspx = 1.0f;
	handle->aspy = 1.0f;

	handle->hash_verts = phash_new((PHashLink **)&handle->construction_chart->verts, 1);
	handle->hash_edges = phash_new((PHashLink **)&handle->construction_chart->edges, 1);
	handle->hash_faces = phash_new((PHashLink **)&handle->construction_chart->faces, 1);

	return (ParamHandle *)handle;
}

void param_aspect_ratio(ParamHandle *handle, float aspx, float aspy)
{
	PHandle *phandle = (PHandle *)handle;

	phandle->aspx = aspx;
	phandle->aspy = aspy;
}

void param_delete(ParamHandle *handle)
{
	PHandle *phandle = (PHandle *)handle;
	int i;

	param_assert((phandle->state == PHANDLE_STATE_ALLOCATED) ||
	             (phandle->state == PHANDLE_STATE_CONSTRUCTED));

	for (i = 0; i < phandle->ncharts; i++)
		p_chart_delete(phandle->charts[i]);
	
	if (phandle->charts)
		MEM_freeN(phandle->charts);

	if (phandle->construction_chart) {
		p_chart_delete(phandle->construction_chart);

		phash_delete(phandle->hash_verts);
		phash_delete(phandle->hash_edges);
		phash_delete(phandle->hash_faces);
	}

	BLI_memarena_free(phandle->arena);
	MEM_freeN(phandle);
}

void param_face_add(ParamHandle *handle, ParamKey key, int nverts,
                    ParamKey *vkeys, float **co, float **uv,
                    ParamBool *pin, ParamBool *select)
{
	PHandle *phandle = (PHandle *)handle;

	param_assert(phash_lookup(phandle->hash_faces, key) == NULL);
	param_assert(phandle->state == PHANDLE_STATE_ALLOCATED);
	param_assert((nverts == 3) || (nverts == 4));

	if (nverts == 4) {
		if (p_quad_split_direction(phandle, co, vkeys)) {
			p_face_add_construct(phandle, key, vkeys, co, uv, 0, 1, 2, pin, select);
			p_face_add_construct(phandle, key, vkeys, co, uv, 0, 2, 3, pin, select);
		}
		else {
			p_face_add_construct(phandle, key, vkeys, co, uv, 0, 1, 3, pin, select);
			p_face_add_construct(phandle, key, vkeys, co, uv, 1, 2, 3, pin, select);
		}
	}
	else
		p_face_add_construct(phandle, key, vkeys, co, uv, 0, 1, 2, pin, select);
}

void param_edge_set_seam(ParamHandle *handle, ParamKey *vkeys)
{
	PHandle *phandle = (PHandle *)handle;
	PEdge *e;

	param_assert(phandle->state == PHANDLE_STATE_ALLOCATED);

	e = p_edge_lookup(phandle, vkeys);
	if (e)
		e->flag |= PEDGE_SEAM;
}

void param_construct_end(ParamHandle *handle, ParamBool fill, ParamBool impl)
{
	PHandle *phandle = (PHandle *)handle;
	PChart *chart = phandle->construction_chart;
	int i, j, nboundaries = 0;
	PEdge *outer;

	param_assert(phandle->state == PHANDLE_STATE_ALLOCATED);

	phandle->ncharts = p_connect_pairs(phandle, (PBool)impl);
	phandle->charts = p_split_charts(phandle, chart, phandle->ncharts);

	p_chart_delete(phandle->construction_chart);
	phandle->construction_chart = NULL;

	phash_delete(phandle->hash_verts);
	phash_delete(phandle->hash_edges);
	phash_delete(phandle->hash_faces);
	phandle->hash_verts = phandle->hash_edges = phandle->hash_faces = NULL;

	for (i = j = 0; i < phandle->ncharts; i++) {
		PVert *v;
		chart = phandle->charts[i];

		p_chart_boundaries(chart, &nboundaries, &outer);

		if (!impl && nboundaries == 0) {
			p_chart_delete(chart);
			continue;
		}

		phandle->charts[j] = chart;
		j++;

		if (fill && (nboundaries > 1))
			p_chart_fill_boundaries(chart, outer);

		for (v = chart->verts; v; v = v->nextlink)
			p_vert_load_pin_select_uvs(handle, v);
	}

	phandle->ncharts = j;

	phandle->state = PHANDLE_STATE_CONSTRUCTED;
}

void param_lscm_begin(ParamHandle *handle, ParamBool live, ParamBool abf)
{
	PHandle *phandle = (PHandle *)handle;
	PFace *f;
	int i;

	param_assert(phandle->state == PHANDLE_STATE_CONSTRUCTED);
	phandle->state = PHANDLE_STATE_LSCM;

	for (i = 0; i < phandle->ncharts; i++) {
		for (f = phandle->charts[i]->faces; f; f = f->nextlink)
			p_face_backup_uvs(f);
		p_chart_lscm_begin(phandle->charts[i], (PBool)live, (PBool)abf);
	}
}

void param_lscm_solve(ParamHandle *handle)
{
	PHandle *phandle = (PHandle *)handle;
	PChart *chart;
	int i;
	PBool result;

	param_assert(phandle->state == PHANDLE_STATE_LSCM);

	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];

		if (chart->u.lscm.context) {
			result = p_chart_lscm_solve(phandle, chart);

			if (result && !(chart->flag & PCHART_NOPACK))
				p_chart_rotate_minimum_area(chart);

			if (!result || (chart->u.lscm.pin1))
				p_chart_lscm_end(chart);
		}
	}
}

void param_lscm_end(ParamHandle *handle)
{
	PHandle *phandle = (PHandle *)handle;
	int i;

	param_assert(phandle->state == PHANDLE_STATE_LSCM);

	for (i = 0; i < phandle->ncharts; i++) {
		p_chart_lscm_end(phandle->charts[i]);
#if 0
		p_chart_complexify(phandle->charts[i]);
#endif
	}

	phandle->state = PHANDLE_STATE_CONSTRUCTED;
}

void param_stretch_begin(ParamHandle *handle)
{
	PHandle *phandle = (PHandle *)handle;
	PChart *chart;
	PVert *v;
	PFace *f;
	int i;

	param_assert(phandle->state == PHANDLE_STATE_CONSTRUCTED);
	phandle->state = PHANDLE_STATE_STRETCH;

	phandle->rng = rng_new(31415926);
	phandle->blend = 0.0f;

	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];

		for (v = chart->verts; v; v = v->nextlink)
			v->flag &= ~PVERT_PIN;  /* don't use user-defined pins */

		p_stretch_pin_boundary(chart);

		for (f = chart->faces; f; f = f->nextlink) {
			p_face_backup_uvs(f);
			f->u.area3d = p_face_area(f);
		}
	}
}

void param_stretch_blend(ParamHandle *handle, float blend)
{
	PHandle *phandle = (PHandle *)handle;

	param_assert(phandle->state == PHANDLE_STATE_STRETCH);
	phandle->blend = blend;
}

void param_stretch_iter(ParamHandle *handle)
{
	PHandle *phandle = (PHandle *)handle;
	PChart *chart;
	int i;

	param_assert(phandle->state == PHANDLE_STATE_STRETCH);

	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];
		p_chart_stretch_minimize(chart, phandle->rng);
	}
}

void param_stretch_end(ParamHandle *handle)
{
	PHandle *phandle = (PHandle *)handle;

	param_assert(phandle->state == PHANDLE_STATE_STRETCH);
	phandle->state = PHANDLE_STATE_CONSTRUCTED;

	rng_free(phandle->rng);
	phandle->rng = NULL;
}

void param_smooth_area(ParamHandle *handle)
{
	PHandle *phandle = (PHandle *)handle;
	int i;

	param_assert(phandle->state == PHANDLE_STATE_CONSTRUCTED);

	for (i = 0; i < phandle->ncharts; i++) {
		PChart *chart = phandle->charts[i];
		PVert *v;

		for (v = chart->verts; v; v = v->nextlink)
			v->flag &= ~PVERT_PIN;

		p_smooth(chart);
	}
}
 
void param_pack(ParamHandle *handle, float margin)
{	
	/* box packing variables */
	boxPack *boxarray, *box;
	float tot_width, tot_height, scale;
	 
	PChart *chart;
	int i, unpacked = 0;
	float trans[2];
	double area = 0.0;
	
	PHandle *phandle = (PHandle *)handle;
	
	if (phandle->ncharts == 0)
		return;
	
	if (phandle->aspx != phandle->aspy)
		param_scale(handle, 1.0f / phandle->aspx, 1.0f / phandle->aspy);
	
	/* we may not use all these boxes */
	boxarray = MEM_mallocN(phandle->ncharts * sizeof(boxPack), "boxPack box");
	
	
	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];
		
		if (chart->flag & PCHART_NOPACK) {
			unpacked++;
			continue;
		}
		
		box = boxarray + (i - unpacked);
		
		p_chart_uv_bbox(chart, trans, chart->u.pack.size);
		
		trans[0] = -trans[0];
		trans[1] = -trans[1];
		
		p_chart_uv_translate(chart, trans);
		
		box->w =  chart->u.pack.size[0] + trans[0];
		box->h =  chart->u.pack.size[1] + trans[1];
		box->index = i; /* warning this index skips PCHART_NOPACK boxes */
		
		if (margin > 0.0f)
			area += sqrt(box->w * box->h);
	}	
	
	if (margin > 0.0f) {
		/* multiply the margin by the area to give predictable results not dependent on UV scale,
		 * ...Without using the area running pack multiple times also gives a bad feedback loop.
		 * multiply by 0.1 so the margin value from the UI can be from 0.0 to 1.0 but not give a massive margin */
		margin = (margin * (float)area) * 0.1f;
		unpacked = 0;
		for (i = 0; i < phandle->ncharts; i++) {
			chart = phandle->charts[i];
			
			if (chart->flag & PCHART_NOPACK) {
				unpacked++;
				continue;
			}
			
			box = boxarray + (i - unpacked);
			trans[0] = margin;
			trans[1] = margin;
			p_chart_uv_translate(chart, trans);
			box->w += margin * 2;
			box->h += margin * 2;
		}
	}
	
	boxPack2D(boxarray, phandle->ncharts - unpacked, &tot_width, &tot_height);
	
	if (tot_height > tot_width)
		scale = 1.0f / tot_height;
	else
		scale = 1.0f / tot_width;
	
	for (i = 0; i < phandle->ncharts - unpacked; i++) {
		box = boxarray + i;
		trans[0] = box->x;
		trans[1] = box->y;
		
		chart = phandle->charts[box->index];
		p_chart_uv_translate(chart, trans);
		p_chart_uv_scale(chart, scale);
	}
	MEM_freeN(boxarray);

	if (phandle->aspx != phandle->aspy)
		param_scale(handle, phandle->aspx, phandle->aspy);
}

void param_average(ParamHandle *handle)
{
	PChart *chart;
	int i;
	float tot_uvarea = 0.0f, tot_facearea = 0.0f;
	float tot_fac, fac;
	float minv[2], maxv[2], trans[2];
	PHandle *phandle = (PHandle *)handle;
	
	if (phandle->ncharts == 0)
		return;
	
	for (i = 0; i < phandle->ncharts; i++) {
		PFace *f;
		chart = phandle->charts[i];
		
		chart->u.pack.area = 0.0f; /* 3d area */
		chart->u.pack.rescale = 0.0f; /* UV area, abusing rescale for tmp storage, oh well :/ */
		
		for (f = chart->faces; f; f = f->nextlink) {
			chart->u.pack.area += p_face_area(f);
			chart->u.pack.rescale += fabsf(p_face_uv_area_signed(f));
		}
		
		tot_facearea += chart->u.pack.area;
		tot_uvarea += chart->u.pack.rescale;
	}
	
	if (tot_facearea == tot_uvarea || tot_facearea == 0.0f || tot_uvarea == 0.0f) {
		/* nothing to do */
		return;
	}
	
	tot_fac = tot_facearea / tot_uvarea;
	
	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];
		if (chart->u.pack.area != 0.0f && chart->u.pack.rescale != 0.0f) {
			fac = chart->u.pack.area / chart->u.pack.rescale;
			
			/* Get the island center */
			p_chart_uv_bbox(chart, minv, maxv);
			trans[0] = (minv[0] + maxv[0]) / -2.0f;
			trans[1] = (minv[1] + maxv[1]) / -2.0f;
			
			/* Move center to 0,0 */
			p_chart_uv_translate(chart, trans);
			p_chart_uv_scale(chart, sqrt(fac / tot_fac));
			
			/* Move to original center */
			trans[0] = -trans[0];
			trans[1] = -trans[1];
			p_chart_uv_translate(chart, trans);
		}
	}
}

void param_scale(ParamHandle *handle, float x, float y)
{
	PHandle *phandle = (PHandle *)handle;
	PChart *chart;
	int i;

	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];
		p_chart_uv_scale_xy(chart, x, y);
	}
}

void param_flush(ParamHandle *handle)
{
	PHandle *phandle = (PHandle *)handle;
	PChart *chart;
	int i;

	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];

		if ((phandle->state == PHANDLE_STATE_LSCM) && !chart->u.lscm.context)
			continue;

		if (phandle->blend == 0.0f)
			p_flush_uvs(phandle, chart);
		else
			p_flush_uvs_blend(phandle, chart, phandle->blend);
	}
}

void param_flush_restore(ParamHandle *handle)
{
	PHandle *phandle = (PHandle *)handle;
	PChart *chart;
	PFace *f;
	int i;

	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];

		for (f = chart->faces; f; f = f->nextlink)
			p_face_restore_uvs(f);
	}
}

