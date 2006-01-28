
#include "MEM_guardedalloc.h"

#include "BLI_memarena.h"
#include "BLI_arithb.h"
#include "BLI_rand.h"

#include "BKE_utildefines.h"

#include "BIF_editsima.h"
#include "BIF_toolbox.h"

#include "ONL_opennl.h"

#include "parametrizer.h"
#include "parametrizer_intern.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#define M_PI 3.14159265358979323846
#endif

/* Hash */

static int PHashSizes[] = {
	1, 3, 5, 11, 17, 37, 67, 131, 257, 521, 1031, 2053, 4099, 8209, 
	16411, 32771, 65537, 131101, 262147, 524309, 1048583, 2097169, 
	4194319, 8388617, 16777259, 33554467, 67108879, 134217757, 268435459
};

#define PHASH_hash(ph, item) (((unsigned long) (item))%((unsigned int) (ph)->cursize))

PHash *phash_new(int sizehint)
{
	PHash *ph = (PHash*)MEM_callocN(sizeof(PHash), "PHash");
	ph->size = 0;
	ph->cursize_id = 0;
	ph->first = NULL;

	while (PHashSizes[ph->cursize_id] < sizehint)
		ph->cursize_id++;

	ph->cursize = PHashSizes[ph->cursize_id];
	ph->buckets = (PHashLink**)MEM_callocN(ph->cursize*sizeof(*ph->buckets), "PHashBuckets");

	return ph;
}

void phash_delete(PHash *ph)
{
	MEM_freeN(ph->buckets);
	MEM_freeN(ph);
}

void phash_delete_with_links(PHash *ph)
{
	PHashLink *link, *next=NULL;

	for (link = ph->first; link; link = next) {
		next = link->next;
		MEM_freeN(link);
	}

	phash_delete(ph);
}

int phash_size(PHash *ph)
{
	return ph->size;
}

void phash_insert(PHash *ph, PHashLink *link)
{
	int size = ph->cursize;
	int hash = PHASH_hash(ph, link->key);
	PHashLink *lookup = ph->buckets[hash];

	if (lookup == NULL) {
		/* insert in front of the list */
		ph->buckets[hash] = link;
		link->next = ph->first;
		ph->first = link;
	}
	else {
		/* insert after existing element */
		link->next = lookup->next;
		lookup->next = link;
	}
		
	ph->size++;

	if (ph->size > (size*3)) {
		PHashLink *next = NULL, *first = ph->first;

		ph->cursize = PHashSizes[++ph->cursize_id];
		MEM_freeN(ph->buckets);
		ph->buckets = (PHashLink**)MEM_callocN(ph->cursize*sizeof(*ph->buckets), "PHashBuckets");
		ph->size = 0;
		ph->first = NULL;

		for (link = first; link; link = next) {
			next = link->next;
			phash_insert(ph, link);
		}
	}
}

PHashLink *phash_lookup(PHash *ph, PHashKey key)
{
	PHashLink *link;
	int hash = PHASH_hash(ph, key);

	for (link = ph->buckets[hash]; link; link = link->next)
		if (link->key == key)
			return link;
		else if (PHASH_hash(ph, link->key) != hash)
			return NULL;
	
	return link;
}

PHashLink *phash_next(PHash *ph, PHashKey key, PHashLink *link)
{
	int hash = PHASH_hash(ph, key);

	for (link = link->next; link; link = link->next)
		if (link->key == key)
			return link;
		else if (PHASH_hash(ph, link->key) != hash)
			return NULL;
	
	return link;
}

/* Heap */

#define PHEAP_PARENT(i) ((i-1)>>1)
#define PHEAP_LEFT(i)   ((i<<1)+1)
#define PHEAP_RIGHT(i)  ((i<<1)+2)
#define PHEAP_COMPARE(a, b) (a->value < b->value)
#define PHEAP_EQUALS(a, b) (a->value == b->value)
#define PHEAP_SWAP(heap, i, j) \
	{ SWAP(int, heap->tree[i]->index, heap->tree[j]->index); \
	  SWAP(PHeapLink*, heap->tree[i], heap->tree[j]);  }

static void pheap_down(PHeap *heap, int i)
{
	while (P_TRUE) {
		int size = heap->size, smallest;
		int l = PHEAP_LEFT(i);
		int r = PHEAP_RIGHT(i);

		smallest = ((l < size) && PHEAP_COMPARE(heap->tree[l], heap->tree[i]))? l: i;

		if ((r < size) && PHEAP_COMPARE(heap->tree[r], heap->tree[smallest]))
			smallest = r;
		
		if (smallest == i)
			break;

		PHEAP_SWAP(heap, i, smallest);
		i = smallest;
	}
}

static void pheap_up(PHeap *heap, int i)
{
	while (i > 0) {
		int p = PHEAP_PARENT(i);

		if (PHEAP_COMPARE(heap->tree[p], heap->tree[i]))
			break;

		PHEAP_SWAP(heap, p, i);
		i = p;
	}
}

PHeap *pheap_new()
{
	/* TODO: replace mallocN with something faster */

	PHeap *heap = (PHeap*)MEM_callocN(sizeof(PHeap), "PHeap");
	heap->bufsize = 1;
	heap->tree = (PHeapLink**)MEM_mallocN(sizeof(PHeapLink*), "PHeapTree");

	return heap;
}

void pheap_delete(PHeap *heap)
{
	MEM_freeN(heap->tree);
	MEM_freeN(heap);
}

PHeapLink *pheap_insert(PHeap *heap, float value, void *ptr)
{
	PHeapLink *link;

	if ((heap->size + 1) > heap->bufsize) {
		int newsize = heap->bufsize*2;

		PHeapLink **ntree = (PHeapLink**)MEM_mallocN(newsize*sizeof(PHeapLink*), "PHeapTree");
		memcpy(ntree, heap->tree, sizeof(PHeapLink*)*heap->size);
		MEM_freeN(heap->tree);

		heap->tree = ntree;
		heap->bufsize = newsize;
	}

	param_assert(heap->size < heap->bufsize);

	link = MEM_mallocN(sizeof *link, "PHeapLink");
	link->value = value;
	link->ptr = ptr;
	link->index = heap->size;

	heap->tree[link->index] = link;

	heap->size++;

	pheap_up(heap, heap->size-1);

	return link;
}

int pheap_empty(PHeap *heap)
{
	return (heap->size == 0);
}

int pheap_size(PHeap *heap)
{
	return heap->size;
}

void *pheap_min(PHeap *heap)
{
	return heap->tree[0]->ptr;
}

void *pheap_popmin(PHeap *heap)
{
	void *ptr = heap->tree[0]->ptr;

	MEM_freeN(heap->tree[0]);

	if (heap->size == 1)
		heap->size--;
	else {
		PHEAP_SWAP(heap, 0, heap->size-1);
		heap->size--;

		pheap_down(heap, 0);
	}

	return ptr;
}

static void pheap_remove(PHeap *heap, PHeapLink *link)
{
	int i = link->index;

	while (i > 0) {
		int p = PHEAP_PARENT(i);

		PHEAP_SWAP(heap, p, i);
		i = p;
	}

	pheap_popmin(heap);
}

/* Construction */

PEdge *p_wheel_edge_next(PEdge *e)
{
	return e->next->next->pair;
}

PEdge *p_wheel_edge_prev(PEdge *e)
{   
	return (e->pair)? e->pair->next: NULL;
}

static PVert *p_vert_add(PChart *chart, PHashKey key, float *co, PEdge *e)
{
	PVert *v = (PVert*)BLI_memarena_alloc(chart->handle->arena, sizeof *v);
	v->co = co;
	v->link.key = key;
	v->edge = e;
	v->flag = 0;

	phash_insert(chart->verts, (PHashLink*)v);

	return v;
}

static PVert *p_vert_lookup(PChart *chart, PHashKey key, float *co, PEdge *e)
{
	PVert *v = (PVert*)phash_lookup(chart->verts, key);

	if (v)
		return v;
	else
		return p_vert_add(chart, key, co, e);
}

static PVert *p_vert_copy(PChart *chart, PVert *v)
{
	PVert *nv = (PVert*)BLI_memarena_alloc(chart->handle->arena, sizeof *nv);
	nv->co = v->co;
	nv->uv[0] = v->uv[0];
	nv->uv[1] = v->uv[1];
	nv->link.key = v->link.key;
	nv->edge = v->edge;
	nv->flag = v->flag;

	phash_insert(chart->verts, (PHashLink*)nv);

	return nv;
}

static PEdge *p_edge_lookup(PChart *chart, PHashKey *vkeys)
{
	PHashKey key = vkeys[0]^vkeys[1];
	PEdge *e = (PEdge*)phash_lookup(chart->edges, key);

	while (e) {
		if ((e->vert->link.key == vkeys[0]) && (e->next->vert->link.key == vkeys[1]))
			return e;
		else if ((e->vert->link.key == vkeys[1]) && (e->next->vert->link.key == vkeys[0]))
			return e;

		e = (PEdge*)phash_next(chart->edges, key, (PHashLink*)e);
	}

	return NULL;
}

static void p_face_flip(PFace *f)
{
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
	PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;
	int f1 = e1->flag, f2 = e2->flag, f3 = e3->flag;

	e1->vert = v2;
	e1->next = e3;
	e1->flag = (f1 & ~PEDGE_VERTEX_FLAGS) | (f2 & PEDGE_VERTEX_FLAGS);

	e2->vert = v3;
	e2->next = e1;
	e2->flag = (f2 & ~PEDGE_VERTEX_FLAGS) | (f3 & PEDGE_VERTEX_FLAGS);

	e3->vert = v1;
	e3->next = e2;
	e3->flag = (f3 & ~PEDGE_VERTEX_FLAGS) | (f1 & PEDGE_VERTEX_FLAGS);
}

static void p_vert_load_pin_select_uvs(PVert *v)
{
	PEdge *e;
	int nedges = 0;

	v->uv[0] = v->uv[1] = 0.0f;
	nedges = 0;
	e = v->edge;
	do {
		if (e->orig_uv && (e->flag & PEDGE_PIN)) {
			if (e->flag & PEDGE_SELECT)
				v->flag |= PVERT_SELECT;

			v->flag |= PVERT_PIN;
			v->uv[0] += e->orig_uv[0];
			v->uv[1] += e->orig_uv[1];
			nedges++;
		}

		e = p_wheel_edge_next(e);
	} while (e && e != (v->edge));

	if (nedges > 0) {
		v->uv[0] /= nedges;
		v->uv[1] /= nedges;
	}
}

static void p_vert_load_select_uvs(PVert *v)
{
	PEdge *e;
	int nedges = 0;

	v->uv[0] = v->uv[1] = 0.0f;
	nedges = 0;
	e = v->edge;
	do {
		if (e->orig_uv && (e->flag & PEDGE_SELECT))
			v->flag |= PVERT_SELECT;

		v->uv[0] += e->orig_uv[0];
		v->uv[1] += e->orig_uv[1];
		nedges++;

		e = p_wheel_edge_next(e);
	} while (e && e != (v->edge));

	if (nedges > 0) {
		v->uv[0] /= nedges;
		v->uv[1] /= nedges;
	}
}

static void p_extrema_verts(PChart *chart, PVert **v1, PVert **v2)
{
	float minv[3], maxv[3], dirlen;
	PVert *v, *minvert[3], *maxvert[3];
	int i, dir;

	/* find minimum and maximum verts over x/y/z axes */
	minv[0] = minv[1] = minv[2] = 1e20;
	maxv[0] = maxv[1] = maxv[2] = -1e20;

	minvert[0] = minvert[1] = minvert[2] = NULL;
	maxvert[0] = maxvert[1] = maxvert[2] = NULL;

	for (v = (PVert*)chart->verts->first; v; v=v->link.next) {
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

	if (minvert[dir] == maxvert[dir]) {
		/* degenerate case */
		PFace *f = (PFace*)chart->faces->first;
		*v1 = f->edge->vert;
		*v2 = f->edge->next->vert;

		(*v1)->uv[0] = 0.0f;
		(*v1)->uv[1] = 0.5f;
		(*v2)->uv[0] = 1.0f;
		(*v2)->uv[1] = 0.5f;
	}
	else {
		*v1 = minvert[dir];
		*v2 = maxvert[dir];

		(*v1)->uv[0] = (*v1)->co[dir];
		(*v1)->uv[1] = (*v1)->co[(dir+1)%3];
		(*v2)->uv[0] = (*v2)->co[dir];
		(*v2)->uv[1] = (*v2)->co[(dir+1)%3];
	}
}

static float p_vec_normalise(float *v)
{
   float d;
   
    d = sqrt(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);

    if(d != 0.0f) {
		d = 1.0f/d;

		v[0] *= d;
		v[1] *= d;
		v[2] *= d;
	}

	return d;
}

static float p_vec_angle_cos(float *v1, float *v2, float *v3)
{
	float d1[3], d2[3];

	d1[0] = v1[0] - v2[0];
	d1[1] = v1[1] - v2[1];
	d1[2] = v1[2] - v2[2];

	d2[0] = v3[0] - v2[0];
	d2[1] = v3[1] - v2[1];
	d2[2] = v3[2] - v2[2];

	p_vec_normalise(d1);
	p_vec_normalise(d2);

	return d1[0]*d2[0] + d1[1]*d2[1] + d1[2]*d2[2];
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

static void p_face_angles(PFace *f, float *a1, float *a2, float *a3)
{
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
	PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

	*a1 = p_vec_angle(v3->co, v1->co, v2->co);
	*a2 = p_vec_angle(v1->co, v2->co, v3->co);
	*a3 = M_PI - *a2 - *a1;
}

static float p_face_area(PFace *f)
{
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
	PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

	return AreaT3Dfl(v1->co, v2->co, v3->co);
}

static float p_face_uv_area_signed(PFace *f)
{
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
	PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;

	return 0.5f*(((v2->uv[0]-v1->uv[0]) * (v3->uv[1]-v1->uv[1])) - 
	            ((v3->uv[0]-v1->uv[0]) * (v2->uv[1]-v1->uv[1])));
}

static float p_face_uv_area(PFace *f)
{
	return fabs(p_face_uv_area_signed(f));
}

static void p_chart_area(PChart *chart, float *uv_area, float *area)
{
	PFace *f;

	*uv_area = *area = 0.0f;

	for (f=(PFace*)chart->faces->first; f; f=f->link.next) {
		*uv_area += p_face_uv_area(f);
		*area += p_face_area(f);
	}
}

static PChart *p_chart_new(PHandle *handle)
{
	PChart *chart = (PChart*)MEM_callocN(sizeof*chart, "PChart");
	chart->verts = phash_new(1);
	chart->edges = phash_new(1);
	chart->faces = phash_new(1);
	chart->handle = handle;

	return chart;
}

static void p_chart_delete(PChart *chart)
{
	/* the actual links are free by memarena */
	phash_delete(chart->verts);
	phash_delete(chart->edges);
	phash_delete(chart->faces);

	MEM_freeN(chart);
}

static PBool p_edge_implicit_seam(PEdge *e, PEdge *ep)
{
	float *uv1, *uv2, *uvp1, *uvp2;
	float limit[2];

	uv1 = e->orig_uv;
	uv2 = e->next->orig_uv;

	if (e->vert->link.key == ep->vert->link.key) {
		uvp1 = ep->orig_uv;
		uvp2 = ep->next->orig_uv;
	}
	else {
		uvp1 = ep->next->orig_uv;
		uvp2 = ep->orig_uv;
	}

	get_connected_limit_tface_uv(limit);

	if((fabs(uv1[0]-uvp1[0]) > limit[0]) && (fabs(uv1[1]-uvp1[1]) > limit[1])) {
		e->flag |= PEDGE_SEAM;
		ep->flag |= PEDGE_SEAM;
		return P_TRUE;
	}
	if((fabs(uv2[0]-uvp2[0]) > limit[0]) && (fabs(uv2[1]-uvp2[1]) > limit[1])) {
		e->flag |= PEDGE_SEAM;
		ep->flag |= PEDGE_SEAM;
		return P_TRUE;
	}
	
	return P_FALSE;
}

static PBool p_edge_has_pair(PChart *chart, PEdge *e, PEdge **pair, PBool impl)
{
	PHashKey key;
	PEdge *pe;
	PVert *v1, *v2;
	PHashKey key1 = e->vert->link.key;
	PHashKey key2 = e->next->vert->link.key;

	if (e->flag & PEDGE_SEAM)
		return P_FALSE;
	
	key = key1 ^ key2;
	pe = (PEdge*)phash_lookup(chart->edges, key);
	*pair = NULL;

	while (pe) {
		if (pe != e) {
			v1 = pe->vert;
			v2 = pe->next->vert;

			if (((v1->link.key == key1) && (v2->link.key == key2)) ||
				((v1->link.key == key2) && (v2->link.key == key1))) {

				/* don't connect seams and t-junctions */
				if ((pe->flag & PEDGE_SEAM) || *pair ||
				    (impl && p_edge_implicit_seam(e, pe))) {
					*pair = NULL;
					return P_FALSE;
				}

				*pair = pe;
			}
		}

		pe = (PEdge*)phash_next(chart->edges, key, (PHashLink*)pe);
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

static PBool p_edge_connect_pair(PChart *chart, PEdge *e, PEdge ***stack, PBool impl)
{
	PEdge *pair = NULL;

	if(!e->pair && p_edge_has_pair(chart, e, &pair, impl)) {
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

static int p_connect_pairs(PChart *chart, PBool impl)
{
	PEdge **stackbase = MEM_mallocN(sizeof*stackbase * phash_size(chart->faces), "Pstackbase");
	PEdge **stack = stackbase;
	PFace *f, *first;
	PEdge *e, *e1, *e2;
	int ncharts = 0;

	/* connect pairs, count edges, set vertex-edge pointer to a pairless edge */
	for (first=(PFace*)chart->faces->first; first; first=first->link.next) {
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

			if (!p_edge_connect_pair(chart, e, &stack, impl))
				e->vert->edge = e;
			if (!p_edge_connect_pair(chart, e1, &stack, impl))
				e1->vert->edge = e1;
			if (!p_edge_connect_pair(chart, e2, &stack, impl))
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
			phash_insert(chart->verts, (PHashLink*)v);
		}
	}

	if (copy) {
		/* not found, copying */
		v = p_vert_copy(chart, v);
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
	PChart **charts = MEM_mallocN(sizeof*charts * ncharts, "PCharts"), *nchart;
	PFace *f, *nextf;
	int i;

	for (i = 0; i < ncharts; i++)
		charts[i] = p_chart_new(handle);

	f = (PFace*)chart->faces->first;
	while (f) {
		PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
		nextf = f->link.next;

		nchart = charts[f->u.chart];

		phash_insert(nchart->faces, (PHashLink*)f);
		phash_insert(nchart->edges, (PHashLink*)e1);
		phash_insert(nchart->edges, (PHashLink*)e2);
		phash_insert(nchart->edges, (PHashLink*)e3);

		p_split_vert(nchart, e1);
		p_split_vert(nchart, e2);
		p_split_vert(nchart, e3);

		f = nextf;
	}

	return charts;
}

static void p_face_backup_uvs(PFace *f)
{
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;

	e1->old_uv[0] = e1->orig_uv[0];
	e1->old_uv[1] = e1->orig_uv[1];
	e2->old_uv[0] = e2->orig_uv[0];
	e2->old_uv[1] = e2->orig_uv[1];
	e3->old_uv[0] = e3->orig_uv[0];
	e3->old_uv[1] = e3->orig_uv[1];
}

static void p_face_restore_uvs(PFace *f)
{
	PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;

	e1->orig_uv[0] = e1->old_uv[0];
	e1->orig_uv[1] = e1->old_uv[1];
	e2->orig_uv[0] = e2->old_uv[0];
	e2->orig_uv[1] = e2->old_uv[1];
	e3->orig_uv[0] = e3->old_uv[0];
	e3->orig_uv[1] = e3->old_uv[1];
}

static PFace *p_face_add(PChart *chart, ParamKey key, ParamKey *vkeys,
                         float *co[3], float *uv[3], int i1, int i2, int i3,
                         ParamBool *pin, ParamBool *select)
{
	PFace *f;
	PEdge *e1, *e2, *e3;

	/* allocate */
	f = (PFace*)BLI_memarena_alloc(chart->handle->arena, sizeof *f);
	f->flag=0; // init !

	e1 = (PEdge*)BLI_memarena_alloc(chart->handle->arena, sizeof *e1);
	e2 = (PEdge*)BLI_memarena_alloc(chart->handle->arena, sizeof *e2);
	e3 = (PEdge*)BLI_memarena_alloc(chart->handle->arena, sizeof *e3);

	


	/* set up edges */
	f->edge = e1;
	e1->face = e2->face = e3->face = f;

	e1->next = e2;
	e2->next = e3;
	e3->next = e1;

	e1->pair = NULL;
	e2->pair = NULL;
	e3->pair = NULL;
   
	e1->flag =0;
	e2->flag =0;
	e3->flag =0;


	if (co && uv) {
		e1->vert = p_vert_lookup(chart, vkeys[i1], co[i1], e1);
		e2->vert = p_vert_lookup(chart, vkeys[i2], co[i2], e2);
		e3->vert = p_vert_lookup(chart, vkeys[i3], co[i3], e3);

		e1->orig_uv = uv[i1];
		e2->orig_uv = uv[i2];
		e3->orig_uv = uv[i3];

	}
	else {
		/* internal call to add face */
		e1->vert = e2->vert = e3->vert = NULL;
		e1->orig_uv = e2->orig_uv = e3->orig_uv = NULL;
	}

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
	f->link.key = key;
	phash_insert(chart->faces, (PHashLink*)f);

	e1->link.key = vkeys[i1]^vkeys[i2];
	e2->link.key = vkeys[i2]^vkeys[i3];
	e3->link.key = vkeys[i3]^vkeys[i1];

	phash_insert(chart->edges, (PHashLink*)e1);
	phash_insert(chart->edges, (PHashLink*)e2);
	phash_insert(chart->edges, (PHashLink*)e3);

	return f;
}

static PBool p_quad_split_direction(float **co)
{
    float a1, a2;
	
	a1 = p_vec_angle_cos(co[0], co[1], co[2]);
	a1 += p_vec_angle_cos(co[1], co[0], co[2]);
	a1 += p_vec_angle_cos(co[2], co[0], co[1]);

	a2 = p_vec_angle_cos(co[0], co[1], co[3]);
	a2 += p_vec_angle_cos(co[1], co[0], co[3]);
	a2 += p_vec_angle_cos(co[3], co[0], co[1]);

	return (a1 > a2);
}

static float p_edge_length(PEdge *e)
{
    PVert *v1 = e->vert, *v2 = e->next->vert;
    float d[3];

    d[0] = v2->co[0] - v1->co[0];
    d[1] = v2->co[1] - v1->co[1];
    d[2] = v2->co[2] - v1->co[2];

    return sqrt(d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
}

static float p_edge_uv_length(PEdge *e)
{
    PVert *v1 = e->vert, *v2 = e->next->vert;
    float d[3];

    d[0] = v2->uv[0] - v1->uv[0];
    d[1] = v2->uv[1] - v1->uv[1];

    return sqrt(d[0]*d[0] + d[1]*d[1]);
}

void p_chart_uv_bbox(PChart *chart, float *minv, float *maxv)
{
	PVert *v;

	INIT_MINMAX2(minv, maxv);

	for (v=(PVert*)chart->verts->first; v; v=v->link.next) {
		DO_MINMAX2(v->uv, minv, maxv);
	}
}

static void p_chart_uv_scale(PChart *chart, float scale)
{
	PVert *v;

	for (v=(PVert*)chart->verts->first; v; v=v->link.next) {
		v->uv[0] *= scale;
		v->uv[1] *= scale;
	}
}

static void p_chart_uv_translate(PChart *chart, float trans[2])
{
	PVert *v;

	for (v=(PVert*)chart->verts->first; v; v=v->link.next) {
		v->uv[0] += trans[0];
		v->uv[1] += trans[1];
	}
}

static void p_chart_boundaries(PChart *chart, int *nboundaries, PEdge **outer)
{   
    PEdge *e, *be;
    float len, maxlen = -1.0;

	*nboundaries = 0;
	*outer = NULL;

	for (e=(PEdge*)chart->edges->first; e; e=e->link.next) {
        if (e->pair || (e->flag & PEDGE_DONE))
            continue;

		(*nboundaries)++;
        len = 0.0f;
    
		be = e;
		do {
            be->flag |= PEDGE_DONE;
            len += p_edge_length(be);
			be = be->next->vert->edge;
        } while(be != e);

        if (len > maxlen) {
			*outer = e;
            maxlen = len;
        }
    }

	for (e=(PEdge*)chart->edges->first; e; e=e->link.next)
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

static void p_chart_fill_boundary(PChart *chart, PEdge *be, int nedges)
{
	PEdge *e, *e1, *e2;
	PHashKey vkeys[3];
	PFace *f;
	struct PHeap *heap = pheap_new(nedges);
	float angle;

	e = be;
	do {
		angle = p_edge_boundary_angle(e);
		e->u.heaplink = pheap_insert(heap, angle, e);

		e = e->next->vert->edge;
	} while(e != be);

	if (nedges == 2) {
		/* no real boundary, but an isolated seam */
		e = be->next->vert->edge;
		e->pair = be;
		be->pair = e;

		pheap_remove(heap, e->u.heaplink);
		pheap_remove(heap, be->u.heaplink);
	}
	else {
		while (nedges > 2) {
			PEdge *ne, *ne1, *ne2;

			e = pheap_popmin(heap);

			e1 = p_boundary_edge_prev(e);
			e2 = p_boundary_edge_next(e);

			pheap_remove(heap, e1->u.heaplink);
			pheap_remove(heap, e2->u.heaplink);
			e->u.heaplink = e1->u.heaplink = e2->u.heaplink = NULL;

			e->flag |= PEDGE_FILLED;
			e1->flag |= PEDGE_FILLED;

			vkeys[0] = e->vert->link.key;
			vkeys[1] = e1->vert->link.key;
			vkeys[2] = e2->vert->link.key;

			f = p_face_add(chart, -1, vkeys, NULL, NULL, 0, 1, 2, NULL, NULL);
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
				
				ne2->u.heaplink = pheap_insert(heap, p_edge_boundary_angle(ne2), ne2);
				e2->u.heaplink = pheap_insert(heap, p_edge_boundary_angle(e2), e2);
			}

			nedges--;
		}
	}

	pheap_delete(heap);
}
#if 0
static void p_chart_fill_boundaries(PChart *chart, PEdge *outer)
{
	PEdge *e, *enext, *be;
	int nedges;

	for (e=(PEdge*)chart->edges->first; e; e=e->link.next) {
		enext = e->link.next;

        if (e->pair || (e->flag & PEDGE_FILLED))
            continue;

		nedges = 0;
		be = e;
		do {
			be->flag |= PEDGE_FILLED;
			be = be->next->vert->edge;
			nedges++;
		} while(be != e);

		if (e != outer)
			p_chart_fill_boundary(chart, e, nedges);
    }
}
#endif
static void p_flush_uvs(PChart *chart)
{
	PEdge *e;

	for (e=(PEdge*)chart->edges->first; e; e=e->link.next) {
		if (e->orig_uv) {
			e->orig_uv[0] = e->vert->uv[0];
			e->orig_uv[1] = e->vert->uv[1];
		}
	}
}

static void p_flush_uvs_blend(PChart *chart, float blend)
{
	PEdge *e;
	float invblend = 1.0f - blend;

	for (e=(PEdge*)chart->edges->first; e; e=e->link.next) {
		if (e->orig_uv) {
			e->orig_uv[0] = blend*e->old_uv[0] + invblend*e->vert->uv[0];
			e->orig_uv[1] = blend*e->old_uv[1] + invblend*e->vert->uv[1];
		}
	}
}

/* Exported */

ParamHandle *param_construct_begin()
{
	PHandle *handle = MEM_callocN(sizeof*handle, "PHandle");
	handle->construction_chart = p_chart_new(handle);
	handle->state = PHANDLE_STATE_ALLOCATED;
	handle->arena = BLI_memarena_new((1<<16));
	
	return (ParamHandle*)handle;
}

void param_delete(ParamHandle *handle)
{
	PHandle *phandle = (PHandle*)handle;
	int i;

	param_assert((phandle->state == PHANDLE_STATE_ALLOCATED) ||
	             (phandle->state == PHANDLE_STATE_CONSTRUCTED));

	for (i = 0; i < phandle->ncharts; i++)
		p_chart_delete(phandle->charts[i]);
	
	if (phandle->charts)
		MEM_freeN(phandle->charts);

	if (phandle->construction_chart)
		p_chart_delete(phandle->construction_chart);

	BLI_memarena_free(phandle->arena);
	MEM_freeN(phandle);
}

void param_face_add(ParamHandle *handle, ParamKey key, int nverts,
                    ParamKey *vkeys, float **co, float **uv,
                    ParamBool *pin, ParamBool *select)
{
	PHandle *phandle = (PHandle*)handle;
	PChart *chart = phandle->construction_chart;

	param_assert(phash_lookup(chart->faces, key) == NULL);
	param_assert(phandle->state == PHANDLE_STATE_ALLOCATED);
	param_assert((nverts == 3) || (nverts == 4));

	if (nverts == 4) {
		if (!p_quad_split_direction(co)) {
			p_face_add(chart, key, vkeys, co, uv, 0, 1, 2, pin, select);
			p_face_add(chart, key, vkeys, co, uv, 0, 2, 3, pin, select);
		}
		else {
			p_face_add(chart, key, vkeys, co, uv, 0, 1, 3, pin, select);
			p_face_add(chart, key, vkeys, co, uv, 1, 2, 3, pin, select);
		}
	}
	else
		p_face_add(chart, key, vkeys, co, uv, 0, 1, 2, pin, select);
}

void param_edge_set_seam(ParamHandle *handle, ParamKey *vkeys)
{
	PHandle *phandle = (PHandle*)handle;
	PChart *chart = phandle->construction_chart;
	PEdge *e;

	param_assert(phandle->state == PHANDLE_STATE_ALLOCATED);

	e = p_edge_lookup(chart, vkeys);
	if (e)
		e->flag |= PEDGE_SEAM;
}

void param_construct_end(ParamHandle *handle, ParamBool fill, ParamBool impl)
{
	PHandle *phandle = (PHandle*)handle;
	PChart *chart = phandle->construction_chart;
	int i, j, nboundaries = 0;
	PEdge *outer;

	param_assert(phandle->state == PHANDLE_STATE_ALLOCATED);

	phandle->ncharts = p_connect_pairs(chart, impl);
	phandle->charts = p_split_charts(phandle, chart, phandle->ncharts);

	p_chart_delete(chart);
	phandle->construction_chart = NULL;

	for (i = j = 0; i < phandle->ncharts; i++) {
		p_chart_boundaries(phandle->charts[i], &nboundaries, &outer);

		if (nboundaries == 0) {
			p_chart_delete(phandle->charts[i]);
			continue;
		}

		phandle->charts[j] = phandle->charts[i];
		j++;

#if 0
		if (fill && (nboundaries > 1))
			p_chart_fill_boundaries(phandle->charts[i], outer);
#endif
	}

	phandle->ncharts = j;

	phandle->state = PHANDLE_STATE_CONSTRUCTED;
}

/* Least Squares Conformal Maps */

static void p_chart_lscm_load_solution(PChart *chart)
{
	PVert *v;

	for (v=(PVert*)chart->verts->first; v; v=v->link.next) {
		v->uv[0] = nlGetVariable(2*v->u.index);
		v->uv[1] = nlGetVariable(2*v->u.index + 1);
	}
}

static void p_chart_lscm_begin(PChart *chart, PBool live)
{
	PVert *v, *pin1, *pin2;
	PBool select = P_FALSE;
	int npins = 0, id = 0;

	/* give vertices matrix indices and count pins */
	for (v=(PVert*)chart->verts->first; v; v=v->link.next) {
		p_vert_load_pin_select_uvs(v);

		if (v->flag & PVERT_PIN)
			npins++;

		if (v->flag & PVERT_SELECT)
			select = P_TRUE;

		v->u.index = id++;
	}

	if ((live && !select) || (npins == 1)) {
		chart->u.lscm.context = NULL;
	}
	else {
		if (npins <= 1) {
			/* not enough pins, lets find some ourself */
			p_extrema_verts(chart, &pin1, &pin2);

			chart->u.lscm.pin1 = pin1;
			chart->u.lscm.pin2 = pin2;
		}
		else {
			chart->flag |= PCHART_NOPACK;
		}

		nlNewContext();
		nlSolverParameteri(NL_NB_VARIABLES, 2*phash_size(chart->verts));
		nlSolverParameteri(NL_LEAST_SQUARES, NL_TRUE);

		chart->u.lscm.context = nlGetCurrent();
	}
}

static PBool p_chart_lscm_solve(PChart *chart)
{
	PVert *v, *pin1 = chart->u.lscm.pin1, *pin2 = chart->u.lscm.pin2;
	PFace *f;

	nlMakeCurrent(chart->u.lscm.context);

	nlBegin(NL_SYSTEM);

	for (v=(PVert*)chart->verts->first; v; v=v->link.next)
		if (v->flag & PVERT_PIN)
			p_vert_load_pin_select_uvs(v);

	if (chart->u.lscm.pin1) {
		nlLockVariable(2*pin1->u.index);
		nlLockVariable(2*pin1->u.index + 1);
		nlLockVariable(2*pin2->u.index);
		nlLockVariable(2*pin2->u.index + 1);
	
		nlSetVariable(2*pin1->u.index, pin1->uv[0]);
		nlSetVariable(2*pin1->u.index + 1, pin1->uv[1]);
		nlSetVariable(2*pin2->u.index, pin2->uv[0]);
		nlSetVariable(2*pin2->u.index + 1, pin2->uv[1]);
	}
	else {
		/* set and lock the pins */
		for (v=(PVert*)chart->verts->first; v; v=v->link.next) {
			if (v->flag & PVERT_PIN) {
				nlLockVariable(2*v->u.index);
				nlLockVariable(2*v->u.index + 1);

				nlSetVariable(2*v->u.index, v->uv[0]);
				nlSetVariable(2*v->u.index + 1, v->uv[1]);
			}
		}
	}

	/* construct matrix */

	nlBegin(NL_MATRIX);

	for (f=(PFace*)chart->faces->first; f; f=f->link.next) {
		PEdge *e1 = f->edge, *e2 = e1->next, *e3 = e2->next;
		PVert *v1 = e1->vert, *v2 = e2->vert, *v3 = e3->vert;
		float a1, a2, a3, ratio, cosine, sine;
		float sina1, sina2, sina3, sinmax;

		if (chart->u.lscm.abf_alpha) {
			/* use abf angles if passed on */
			a1 = *(chart->u.lscm.abf_alpha++);
			a2 = *(chart->u.lscm.abf_alpha++);
			a3 = *(chart->u.lscm.abf_alpha++);
		}
		else
			p_face_angles(f, &a1, &a2, &a3);

		sina1 = sin(a1);
		sina2 = sin(a2);
		sina3 = sin(a3);

		sinmax = MAX3(sina1, sina2, sina3);

		/* shift vertices to find most stable order */
		#define SHIFT3(type, a, b, c) \
			{ type tmp; tmp = a; a = c; c = b; b = tmp; }

		if (sina3 != sinmax) {
			SHIFT3(PVert*, v1, v2, v3);
			SHIFT3(float, a1, a2, a3);
			SHIFT3(float, sina1, sina2, sina3);

			if (sina2 == sinmax) {
				SHIFT3(PVert*, v1, v2, v3);
				SHIFT3(float, a1, a2, a3);
				SHIFT3(float, sina1, sina2, sina3);
			}
		}

		/* angle based lscm formulation */
		ratio = (sina3 == 0.0f)? 0.0f: sina2/sina3;
		cosine = cos(a1)*ratio;
		sine = sina1*ratio;

		nlBegin(NL_ROW);
		nlCoefficient(2*v1->u.index,   cosine - 1.0);
		nlCoefficient(2*v1->u.index+1, -sine);
		nlCoefficient(2*v2->u.index,   -cosine);
		nlCoefficient(2*v2->u.index+1, sine);
		nlCoefficient(2*v3->u.index,   1.0);
		nlEnd(NL_ROW);

		nlBegin(NL_ROW);
		nlCoefficient(2*v1->u.index,   sine);
		nlCoefficient(2*v1->u.index+1, cosine - 1.0);
		nlCoefficient(2*v2->u.index,   -sine);
		nlCoefficient(2*v2->u.index+1, -cosine);
		nlCoefficient(2*v3->u.index+1, 1.0);
		nlEnd(NL_ROW);
	}

	nlEnd(NL_MATRIX);

	nlEnd(NL_SYSTEM);

	if (nlSolveAdvanced(NULL, NL_TRUE)) {
		p_chart_lscm_load_solution(chart);
		return P_TRUE;
	}

	return P_FALSE;
}

static void p_chart_lscm_end(PChart *chart)
{
	if (chart->u.lscm.context)
		nlDeleteContext(chart->u.lscm.context);

	chart->u.lscm.context = NULL;
	chart->u.lscm.pin1 = NULL;
	chart->u.lscm.pin2 = NULL;
}

void param_lscm_begin(ParamHandle *handle, ParamBool live)
{
	PHandle *phandle = (PHandle*)handle;
	int i;

	param_assert(phandle->state == PHANDLE_STATE_CONSTRUCTED);
	phandle->state = PHANDLE_STATE_LSCM;

	for (i = 0; i < phandle->ncharts; i++)
		p_chart_lscm_begin(phandle->charts[i], live);
}

void param_lscm_solve(ParamHandle *handle)
{
	PHandle *phandle = (PHandle*)handle;
	PChart *chart;
	int i;
	PBool result;

	param_assert(phandle->state == PHANDLE_STATE_LSCM);

	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];

		if (chart->u.lscm.context) {
			result = p_chart_lscm_solve(chart);

			if (!result || (chart->u.lscm.pin1))
				p_chart_lscm_end(chart);
		}
	}
}

void param_lscm_end(ParamHandle *handle)
{
	PHandle *phandle = (PHandle*)handle;
	int i;

	param_assert(phandle->state == PHANDLE_STATE_LSCM);

	for (i = 0; i < phandle->ncharts; i++)
		p_chart_lscm_end(phandle->charts[i]);

	phandle->state = PHANDLE_STATE_CONSTRUCTED;
}

/* Stretch */

#define P_STRETCH_ITER 20

static void p_stretch_pin_boundary(PChart *chart)
{
	PVert *v;

	for(v=(PVert*)chart->verts->first; v; v=v->link.next)
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
	
	if (f->flag & PFACE_FILLED)
		return 0.0f;

	w= 1.0f/(2.0f*area);

	/* compute derivatives */
	VecCopyf(Ps, v1->co);
	VecMulf(Ps, (v2->uv[1] - v3->uv[1]));

	VecCopyf(tmp, v2->co);
	VecMulf(tmp, (v3->uv[1] - v1->uv[1]));
	VecAddf(Ps, Ps, tmp);

	VecCopyf(tmp, v3->co);
	VecMulf(tmp, (v1->uv[1] - v2->uv[1]));
	VecAddf(Ps, Ps, tmp);

	VecMulf(Ps, w);

	VecCopyf(Pt, v1->co);
	VecMulf(Pt, (v3->uv[0] - v2->uv[0]));

	VecCopyf(tmp, v2->co);
	VecMulf(tmp, (v1->uv[0] - v3->uv[0]));
	VecAddf(Pt, Pt, tmp);

	VecCopyf(tmp, v3->co);
	VecMulf(tmp, (v2->uv[0] - v1->uv[0]));
	VecAddf(Pt, Pt, tmp);

	VecMulf(Pt, w);

	/* Sander Tensor */
	a= Inpf(Ps, Ps);
	c= Inpf(Pt, Pt);

	T = sqrt(0.5f*(a + c)*f->u.area3d);

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

	for(v=(PVert*)chart->verts->first; v; v=v->link.next) {
		if((v->flag & PVERT_PIN) || !(v->flag & PVERT_SELECT))
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

		random_angle = rng_getFloat(rng) * 2.0 * M_PI;
		dir[0] = trusted_radius * cos(random_angle);
		dir[1] = trusted_radius * sin(random_angle);

		/* calculate old and new stretch */
		low = 0;
		stretch_low = orig_stretch;

		Vec2Addf(v->uv, orig_uv, dir);
		high = 1;
		stretch = stretch_high = p_stretch_compute_vertex(v);

		/* binary search for lowest stretch position */
		for (j = 0; j < P_STRETCH_ITER; j++) {
			mid = 0.5 * (low + high);
			v->uv[0]= orig_uv[0] + mid*dir[0];
			v->uv[1]= orig_uv[1] + mid*dir[1];
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
		if(stretch >= orig_stretch)
			Vec2Copyf(v->uv, orig_uv);
	}
}

void param_stretch_begin(ParamHandle *handle)
{
	PHandle *phandle = (PHandle*)handle;
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

		for (v=(PVert*)chart->verts->first; v; v=v->link.next)
			p_vert_load_select_uvs(v);

		p_stretch_pin_boundary(chart);

		for (f=(PFace*)chart->faces->first; f; f=f->link.next) {
			p_face_backup_uvs(f);
			f->u.area3d = p_face_area(f);
		}
	}
}

void param_stretch_blend(ParamHandle *handle, float blend)
{
	PHandle *phandle = (PHandle*)handle;

	param_assert(phandle->state == PHANDLE_STATE_STRETCH);
	phandle->blend = blend;
}

void param_stretch_iter(ParamHandle *handle)
{
	PHandle *phandle = (PHandle*)handle;
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
	PHandle *phandle = (PHandle*)handle;

	param_assert(phandle->state == PHANDLE_STATE_STRETCH);
	phandle->state = PHANDLE_STATE_CONSTRUCTED;

	rng_free(phandle->rng);
	phandle->rng = NULL;
}

/* Flushing */

void param_flush(ParamHandle *handle)
{
	PHandle *phandle = (PHandle*)handle;
	PChart *chart;
	int i;

	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];

		if ((phandle->state == PHANDLE_STATE_LSCM) && !chart->u.lscm.context)
			continue;

		if (phandle->blend == 0.0f)
			p_flush_uvs(chart);
		else
			p_flush_uvs_blend(chart, phandle->blend);
	}
}

void param_flush_restore(ParamHandle *handle)
{
	PHandle *phandle = (PHandle*)handle;
	PChart *chart;
	PFace *f;
	int i;

	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];

		for (f=(PFace*)chart->faces->first; f; f=f->link.next)
			p_face_restore_uvs(f);
	}
}

/* Packing */

static int compare_chart_area(const void *a, const void *b)
{
	PChart *ca = *((PChart**)a);
	PChart *cb = *((PChart**)b);

    if (ca->u.pack.area > cb->u.pack.area)
		return -1;
	else if (ca->u.pack.area == cb->u.pack.area)
		return 0;
	else
		return 1;
}

static PBool p_pack_try(PHandle *handle, float side)
{
	PChart *chart;
	float packx, packy, rowh, groupw, w, h;
	int i;

	packx= packy= 0.0;
	rowh= 0.0;
	groupw= 1.0/sqrt(handle->ncharts);

	for (i = 0; i < handle->ncharts; i++) {
		chart = handle->charts[i];

		if (chart->flag & PCHART_NOPACK)
			continue;

		w = chart->u.pack.size[0];
		h = chart->u.pack.size[1];

		if(w <= (side-packx)) {
			chart->u.pack.trans[0] = packx;
			chart->u.pack.trans[1] = packy;

			packx += w;
			rowh= MAX2(rowh, h);
		}
		else {
			packy += rowh;
			packx = w;
			rowh = h;

			chart->u.pack.trans[0] = 0.0;
			chart->u.pack.trans[1] = packy;
		}

		if (packy+rowh > side)
			return P_FALSE;
	}

	return P_TRUE;
}

#define PACK_SEARCH_DEPTH 15

void param_pack(ParamHandle *handle)
{
	PHandle *phandle = (PHandle*)handle;
	PChart *chart;
	float uv_area, area, trans[2], minside, maxside, totarea, side;
	int i;

	/* very simple rectangle packing */

	if (phandle->ncharts == 0)
		return;

	totarea = 0.0f;
	maxside = 0.0f;

	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];

		if (chart->flag & PCHART_NOPACK) {
			chart->u.pack.area = 0.0f;
			continue;
		}

		p_chart_area(chart, &uv_area, &area);
		p_chart_uv_bbox(chart, trans, chart->u.pack.size);

		/* translate to origin and make area equal to 3d area */
		chart->u.pack.rescale = (uv_area > 0.0f)? sqrt(area)/sqrt(uv_area): 0.0f;
		chart->u.pack.area = area;
		totarea += area;

		trans[0] = -trans[0];
		trans[1] = -trans[1];
		p_chart_uv_translate(chart, trans);
		p_chart_uv_scale(chart, chart->u.pack.rescale);

		/* compute new dimensions for packing */
		chart->u.pack.size[0] += trans[0];
		chart->u.pack.size[1] += trans[1];
		chart->u.pack.size[0] *= chart->u.pack.rescale;
		chart->u.pack.size[1] *= chart->u.pack.rescale;

		maxside = MAX3(maxside, chart->u.pack.size[0], chart->u.pack.size[1]);
	}

	/* sort by chart area, largest first */
	qsort(phandle->charts, phandle->ncharts, sizeof(PChart*), compare_chart_area);

	/* binary search over pack region size */
	minside = MAX2(sqrt(totarea), maxside);
	maxside = (((int)sqrt(phandle->ncharts-1))+1)*maxside;

	if (minside < maxside) { /* should always be true */

		for (i = 0; i < PACK_SEARCH_DEPTH; i++) {
			if (p_pack_try(phandle, (minside+maxside)*0.5f + 1e-5))
				maxside = (minside+maxside)*0.5f;
			else
				minside = (minside+maxside)*0.5f;
		}
	}

	/* do the actual packing */
	side = maxside + 1e-5;
	if (!p_pack_try(phandle, side))
		param_warning("packing failed.\n");

	for (i = 0; i < phandle->ncharts; i++) {
		chart = phandle->charts[i];

		if (chart->flag & PCHART_NOPACK)
			continue;

		p_chart_uv_scale(chart, 1.0f/side);
		trans[0] = chart->u.pack.trans[0]/side;
		trans[1] = chart->u.pack.trans[1]/side;
		p_chart_uv_translate(chart, trans);
	}
}

