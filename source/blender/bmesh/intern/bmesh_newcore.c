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
 * Contributor(s): Joseph Eagar, Geoffrey Bantle, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/intern/bmesh_newcore.c
 *  \ingroup bmesh
 *
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"

#include "BKE_DerivedMesh.h"

#include "BLI_listbase.h"
#include "BLI_array.h"

#include "bmesh.h"
#include "bmesh_private.h"

/* use so valgrinds memcheck alerts us when undefined index is used.
 * TESTING ONLY! */
// #define USE_DEBUG_INDEX_MEMCHECK

#ifdef USE_DEBUG_INDEX_MEMCHECK
#define DEBUG_MEMCHECK_INDEX_INVALIDATE(ele)               \
	{                                                      \
		int undef_idx;                                     \
		BM_elem_index_set(ele, undef_idx); /* set_ok_invalid */  \
	}                                                      \

#endif

BMVert *BM_vert_create(BMesh *bm, const float co[3], const struct BMVert *example)
{
	BMVert *v = BLI_mempool_calloc(bm->vpool);

#ifdef USE_DEBUG_INDEX_MEMCHECK
	DEBUG_MEMCHECK_INDEX_INVALIDATE(v)
#else
	BM_elem_index_set(v, -1); /* set_ok_invalid */
#endif

	bm->elem_index_dirty |= BM_VERT; /* may add to middle of the pool */

	bm->totvert++;

	v->head.htype = BM_VERT;

	/* 'v->no' is handled by BM_elem_attrs_copy */
	if (co) copy_v3_v3(v->co, co);
	
	/* allocate flag */
	v->oflags = BLI_mempool_calloc(bm->toolflagpool);

	CustomData_bmesh_set_default(&bm->vdata, &v->head.data);
	
	if (example) {
		BM_elem_attrs_copy(bm, bm, (BMVert *)example, v);
	}

	BM_CHECK_ELEMENT(bm, v);

	return (BMVert *) v;
}

/**
 *			BMESH EDGE EXIST
 *
 *  Finds out if two vertices already have an edge
 *  connecting them. Note that multiple edges may
 *  exist between any two vertices, and therefore
 *  This function only returns the first one found.
 *
 *  Returns -
 *	BMEdge pointer
 */
BMEdge *BM_edge_exists(BMVert *v1, BMVert *v2)
{
	BMIter iter;
	BMEdge *e;

	BM_ITER(e, &iter, NULL, BM_EDGES_OF_VERT, v1) {
		if (e->v1 == v2 || e->v2 == v2)
			return e;
	}

	return NULL;
}

BMEdge *BM_edge_create(BMesh *bm, BMVert *v1, BMVert *v2, const BMEdge *example, int nodouble)
{
	BMEdge *e;
	
	if (nodouble && (e = BM_edge_exists(v1, v2)))
		return (BMEdge *)e;
	
	e = BLI_mempool_calloc(bm->epool);

#ifdef USE_DEBUG_INDEX_MEMCHECK
	DEBUG_MEMCHECK_INDEX_INVALIDATE(e)
#else
	BM_elem_index_set(e, -1); /* set_ok_invalid */
#endif

	bm->elem_index_dirty |= BM_EDGE; /* may add to middle of the pool */

	bm->totedge++;

	e->head.htype = BM_EDGE;
	
	/* allocate flag */
	e->oflags = BLI_mempool_calloc(bm->toolflagpool);

	e->v1 = (BMVert *) v1;
	e->v2 = (BMVert *) v2;
	
	BM_elem_flag_enable(e, BM_ELEM_SMOOTH);
	
	CustomData_bmesh_set_default(&bm->edata, &e->head.data);
	
	bmesh_disk_append_edge(e, e->v1);
	bmesh_disk_append_edge(e, e->v2);
	
	if (example)
		BM_elem_attrs_copy(bm, bm, (BMEdge *)example, (BMEdge *)e);
	
	BM_CHECK_ELEMENT(bm, e);

	return (BMEdge *) e;
}

static BMLoop *bmesh_create_loop(BMesh *bm, BMVert *v, BMEdge *e, BMFace *f, const BMLoop *example)
{
	BMLoop *l = NULL;

	l = BLI_mempool_calloc(bm->lpool);
	l->next = l->prev = NULL;
	l->v = v;
	l->e = e;
	l->f = f;
	l->radial_next = l->radial_prev = NULL;
	l->head.data = NULL;
	l->head.htype = BM_LOOP;

	bm->totloop++;

	if (example)
		CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata, example->head.data, &l->head.data);
	else
		CustomData_bmesh_set_default(&bm->ldata, &l->head.data);

	return l;
}

static BMLoop *bm_face_boundry_add(BMesh *bm, BMFace *f, BMVert *startv, BMEdge *starte)
{
#ifdef USE_BMESH_HOLES
	BMLoopList *lst = BLI_mempool_calloc(bm->looplistpool);
#endif
	BMLoop *l = bmesh_create_loop(bm, startv, starte, f, NULL);
	
	bmesh_radial_append(starte, l);

#ifdef USE_BMESH_HOLES
	lst->first = lst->last = l;
	BLI_addtail(&f->loops, lst);
#else
	f->l_first = l;
#endif

	l->f = f;
	
	return l;
}

BMFace *BM_face_copy(BMesh *bm, BMFace *f, const short copyverts, const short copyedges)
{
	BMEdge **edges = NULL;
	BMVert **verts = NULL;
	BLI_array_staticdeclare(edges, BM_NGON_STACK_SIZE);
	BLI_array_staticdeclare(verts, BM_NGON_STACK_SIZE);
	BMLoop *l_iter;
	BMLoop *l_first;
	BMLoop *l2;
	BMFace *f2;
	int i;

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		if (copyverts) {
			BMVert *v = BM_vert_create(bm, l_iter->v->co, l_iter->v);
			BLI_array_append(verts, v);
		}
		else {
			BLI_array_append(verts, l_iter->v);
		}
	} while ((l_iter = l_iter->next) != l_first);

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	i = 0;
	do {
		if (copyedges) {
			BMEdge *e;
			BMVert *v1, *v2;
			
			if (l_iter->e->v1 == verts[i]) {
				v1 = verts[i];
				v2 = verts[(i + 1) % f->len];
			}
			else {
				v2 = verts[i];
				v1 = verts[(i + 1) % f->len];
			}
			
			e = BM_edge_create(bm,  v1, v2, l_iter->e, FALSE);
			BLI_array_append(edges, e);
		}
		else {
			BLI_array_append(edges, l_iter->e);
		}
		
		i++;
	} while ((l_iter = l_iter->next) != l_first);
	
	f2 = BM_face_create(bm, verts, edges, f->len, FALSE);
	
	BM_elem_attrs_copy(bm, bm, f, f2);
	
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	l2 = BM_FACE_FIRST_LOOP(f2);
	do {
		BM_elem_attrs_copy(bm, bm, l_iter, l2);
		l2 = l2->next;
	} while ((l_iter = l_iter->next) != l_first);
	
	return f2;
}

BMFace *BM_face_create(BMesh *bm, BMVert **verts, BMEdge **edges, const int len, int nodouble)
{
	BMFace *f = NULL;
	BMLoop *l, *startl, *lastl;
	int i, overlap;
	
	if (len == 0) {
		/* just return NULL for no */
		return NULL;
	}

	if (nodouble) {
		/* Check if face already exists */
		overlap = BM_face_exists(bm, verts, len, &f);
		if (overlap) {
			return f;
		}
		else {
			BLI_assert(f == NULL);
		}
	}
	
	f = BLI_mempool_calloc(bm->fpool);

#ifdef USE_DEBUG_INDEX_MEMCHECK
	DEBUG_MEMCHECK_INDEX_INVALIDATE(f)
#else
	BM_elem_index_set(f, -1); /* set_ok_invalid */
#endif

	bm->elem_index_dirty |= BM_FACE; /* may add to middle of the pool */

	bm->totface++;

	f->head.htype = BM_FACE;

	startl = lastl = bm_face_boundry_add(bm, (BMFace *)f, verts[0], edges[0]);
	
	startl->v = (BMVert *)verts[0];
	startl->e = (BMEdge *)edges[0];
	for (i = 1; i < len; i++) {
		l = bmesh_create_loop(bm, verts[i], edges[i], (BMFace *)f, edges[i]->l);
		
		l->f = (BMFace *) f;
		bmesh_radial_append(edges[i], l);

		l->prev = lastl;
		lastl->next = l;
		lastl = l;
	}
	
	/* allocate flag */
	f->oflags = BLI_mempool_calloc(bm->toolflagpool);

	CustomData_bmesh_set_default(&bm->pdata, &f->head.data);
	
	startl->prev = lastl;
	lastl->next = startl;
	
	f->len = len;

#ifdef USE_BMESH_HOLES
	f->totbounds = 0;
#endif
	
	BM_CHECK_ELEMENT(bm, f);

	return (BMFace *) f;
}

int bmesh_check_element(BMesh *UNUSED(bm), void *element, const char htype)
{
	BMHeader *head = element;
	int err = 0;

	if (!element)
		return 1;

	if (head->htype != htype)
		return 2;
	
	switch (htype) {
		case BM_VERT: {
			BMVert *v = element;
			if (v->e && v->e->head.htype != BM_EDGE) {
				err |= 4;
			}
			break;
		}
		case BM_EDGE: {
			BMEdge *e = element;
			if (e->l && e->l->head.htype != BM_LOOP)
				err |= 8;
			if (e->l && e->l->f->head.htype != BM_FACE)
				err |= 16;
			if (e->v1_disk_link.prev == NULL ||
			    e->v2_disk_link.prev == NULL ||
			    e->v1_disk_link.next == NULL ||
			    e->v2_disk_link.next == NULL)
			{
				err |= 32;
			}
			if (e->l && (e->l->radial_next == NULL || e->l->radial_prev == NULL))
				err |= 64;
			if (e->l && e->l->f->len <= 0)
				err |= 128;
			break;
		}
		case BM_LOOP: {
			BMLoop *l = element, *l2;
			int i;

			if (l->f->head.htype != BM_FACE)
				err |= 256;
			if (l->e->head.htype != BM_EDGE)
				err |= 512;
			if (l->v->head.htype !=  BM_VERT)
				err |= 1024;
			if (!BM_vert_in_edge(l->e, l->v)) {
				fprintf(stderr, "%s: fatal bmesh error (vert not in edge)! (bmesh internal error)\n", __func__);
				err |= 2048;
			}

			if (l->radial_next == NULL || l->radial_prev == NULL)
				err |= (1 << 12);
			if (l->f->len <= 0)
				err |= (1 << 13);

			/* validate boundary loop--invalid for hole loops, of course,
		 * but we won't be allowing those for a while ye */
			l2 = l;
			i = 0;
			do {
				if (i >= BM_NGON_MAX) {
					break;
				}

				i++;
			} while ((l2 = l2->next) != l);

			if (i != l->f->len || l2 != l)
				err |= (1 << 14);

			if (!bmesh_radial_validate(bmesh_radial_length(l), l))
				err |= (1 << 15);

			break;
		}
		case BM_FACE: {
			BMFace *f = element;
			BMLoop *l_iter;
			BMLoop *l_first;
			int len = 0;

#ifdef USE_BMESH_HOLES
			if (!f->loops.first)
#else
			if (!f->l_first)
#endif
			{
				err |= (1 << 16);
			}
			l_iter = l_first = BM_FACE_FIRST_LOOP(f);
			do {
				if (l_iter->f != f) {
					fprintf(stderr, "%s: loop inside one face points to another! (bmesh internal error)\n", __func__);
					err |= (1 << 17);
				}

				if (!l_iter->e)
					err |= (1 << 18);
				if (!l_iter->v)
					err |= (1 << 19);
				if (!BM_vert_in_edge(l_iter->e, l_iter->v) || !BM_vert_in_edge(l_iter->e, l_iter->next->v)) {
					err |= (1 << 20);
				}

				if (!bmesh_radial_validate(bmesh_radial_length(l_iter), l_iter))
					err |= (1 << 21);

				if (!bmesh_disk_count(l_iter->v) || !bmesh_disk_count(l_iter->next->v))
					err |= (1 << 22);

				len++;
			} while ((l_iter = l_iter->next) != l_first);

			if (len != f->len)
				err |= (1 << 23);
		}
	}

	if (err) {
		bmesh_error();
	}

	return err;
}

/* low level function, only free's,
 * does not change adjust surrounding geometry */
static void bmesh_kill_only_vert(BMesh *bm, BMVert *v)
{
	bm->totvert--;
	bm->elem_index_dirty |= BM_VERT;

	BM_select_history_remove(bm, v);
	if (v->head.data)
		CustomData_bmesh_free_block(&bm->vdata, &v->head.data);

	BLI_mempool_free(bm->toolflagpool, v->oflags);
	BLI_mempool_free(bm->vpool, v);
}

static void bmesh_kill_only_edge(BMesh *bm, BMEdge *e)
{
	bm->totedge--;
	bm->elem_index_dirty |= BM_EDGE;

	BM_select_history_remove(bm, e);

	if (e->head.data)
		CustomData_bmesh_free_block(&bm->edata, &e->head.data);

	BLI_mempool_free(bm->toolflagpool, e->oflags);
	BLI_mempool_free(bm->epool, e);
}

static void bmesh_kill_only_face(BMesh *bm, BMFace *f)
{
	if (bm->act_face == f)
		bm->act_face = NULL;

	bm->totface--;
	bm->elem_index_dirty |= BM_FACE;

	BM_select_history_remove(bm, f);

	if (f->head.data)
		CustomData_bmesh_free_block(&bm->pdata, &f->head.data);

	BLI_mempool_free(bm->toolflagpool, f->oflags);
	BLI_mempool_free(bm->fpool, f);
}

static void bmesh_kill_only_loop(BMesh *bm, BMLoop *l)
{
	bm->totloop--;
	if (l->head.data)
		CustomData_bmesh_free_block(&bm->ldata, &l->head.data);

	BLI_mempool_free(bm->lpool, l);
}

void BM_face_edges_kill(BMesh *bm, BMFace *f)
{
	BMEdge **edges = NULL;
	BLI_array_staticdeclare(edges, BM_NGON_STACK_SIZE);
	BMLoop *l_iter;
	BMLoop *l_first;
	int i;
	
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		BLI_array_append(edges, l_iter->e);
	} while ((l_iter = l_iter->next) != l_first);
	
	for (i = 0; i < BLI_array_count(edges); i++) {
		BM_edge_kill(bm, edges[i]);
	}
	
	BLI_array_free(edges);
}

void BM_face_verts_kill(BMesh *bm, BMFace *f)
{
	BMVert **verts = NULL;
	BLI_array_staticdeclare(verts, BM_NGON_STACK_SIZE);
	BMLoop *l_iter;
	BMLoop *l_first;
	int i;
	
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		BLI_array_append(verts, l_iter->v);
	} while ((l_iter = l_iter->next) != l_first);
	
	for (i = 0; i < BLI_array_count(verts); i++) {
		BM_vert_kill(bm, verts[i]);
	}
	
	BLI_array_free(verts);
}

void BM_face_kill(BMesh *bm, BMFace *f)
{
#ifdef USE_BMESH_HOLES
	BMLoopList *ls, *ls_next;
#endif

	BM_CHECK_ELEMENT(bm, f);

#ifdef USE_BMESH_HOLES
	for (ls = f->loops.first; ls; ls = ls_next)
#else
	if (f->l_first)
#endif
	{
		BMLoop *l_iter, *l_next, *l_first;

#ifdef USE_BMESH_HOLES
		ls_next = ls->next;
		l_iter = l_first = ls->first;
#else
		l_iter = l_first = f->l_first;
#endif

		do {
			l_next = l_iter->next;

			bmesh_radial_remove_loop(l_iter, l_iter->e);
			bmesh_kill_only_loop(bm, l_iter);

		} while ((l_iter = l_next) != l_first);

#ifdef USE_BMESH_HOLES
		BLI_mempool_free(bm->looplistpool, ls);
#endif
	}

	bmesh_kill_only_face(bm, f);
}

void BM_edge_kill(BMesh *bm, BMEdge *e)
{

	bmesh_disk_remove_edge(e, e->v1);
	bmesh_disk_remove_edge(e, e->v2);

	if (e->l) {
		BMLoop *l = e->l, *lnext, *startl = e->l;

		do {
			lnext = l->radial_next;
			if (lnext->f == l->f) {
				BM_face_kill(bm, l->f);
				break;
			}
			
			BM_face_kill(bm, l->f);

			if (l == lnext)
				break;
			l = lnext;
		} while (l != startl);
	}
	
	bmesh_kill_only_edge(bm, e);
}

void BM_vert_kill(BMesh *bm, BMVert *v)
{
	if (v->e) {
		BMEdge *e, *nexte;
		
		e = v->e;
		while (v->e) {
			nexte = bmesh_disk_nextedge(e, v);
			BM_edge_kill(bm, e);
			e = nexte;
		}
	}

	bmesh_kill_only_vert(bm, v);
}

/********** private disk and radial cycle functions ********** */

/**
 *			bmesh_loop_reverse
 *
 *	FLIP FACE EULER
 *
 *	Changes the winding order of a face from CW to CCW or vice versa.
 *	This euler is a bit peculiar in compairson to others as it is its
 *	own inverse.
 *
 *	BMESH_TODO: reinsert validation code.
 *
 *  Returns -
 *	1 for success, 0 for failure.
 */

static int bmesh_loop_length(BMLoop *l)
{
	BMLoop *l_first = l;
	int i = 0;

	do {
		i++;
	} while ((l = l->next) != l_first);

	return i;
}

static int bmesh_loop_reverse_loop(BMesh *bm, BMFace *f
#ifdef USE_BMESH_HOLES
                                   , BMLoopList *lst
#endif
                                   )
{

#ifdef USE_BMESH_HOLES
	BMLoop *l_first = lst->first;
#else
	BMLoop *l_first = f->l_first;
#endif

	BMLoop *l_iter, *oldprev, *oldnext;
	BMEdge **edar = NULL;
	MDisps *md;
	BLI_array_staticdeclare(edar, BM_NGON_STACK_SIZE);
	int i, j, edok, len = 0, do_disps = CustomData_has_layer(&bm->ldata, CD_MDISPS);

	len = bmesh_loop_length(l_first);

	for (i = 0, l_iter = l_first; i < len; i++, l_iter = l_iter->next) {
		BMEdge *curedge = l_iter->e;
		bmesh_radial_remove_loop(l_iter, curedge);
		BLI_array_append(edar, curedge);
	}

	/* actually reverse the loop */
	for (i = 0, l_iter = l_first; i < len; i++) {
		oldnext = l_iter->next;
		oldprev = l_iter->prev;
		l_iter->next = oldprev;
		l_iter->prev = oldnext;
		l_iter = oldnext;
		
		if (do_disps) {
			float (*co)[3];
			int x, y, sides;
			
			md = CustomData_bmesh_get(&bm->ldata, l_iter->head.data, CD_MDISPS);
			if (!md->totdisp || !md->disps)
				continue;

			sides = (int)sqrt(md->totdisp);
			co = md->disps;
			
			for (x = 0; x < sides; x++) {
				for (y = 0; y < x; y++) {
					swap_v3_v3(co[y * sides + x], co[sides * x + y]);
				}
			}
		}
	}

	if (len == 2) { /* two edged face */
		/* do some verification here! */
		l_first->e = edar[1];
		l_first->next->e = edar[0];
	}
	else {
		for (i = 0, l_iter = l_first; i < len; i++, l_iter = l_iter->next) {
			edok = 0;
			for (j = 0; j < len; j++) {
				edok = bmesh_verts_in_edge(l_iter->v, l_iter->next->v, edar[j]);
				if (edok) {
					l_iter->e = edar[j];
					break;
				}
			}
		}
	}
	/* rebuild radia */
	for (i = 0, l_iter = l_first; i < len; i++, l_iter = l_iter->next)
		bmesh_radial_append(l_iter->e, l_iter);

	/* validate radia */
	for (i = 0, l_iter = l_first; i < len; i++, l_iter = l_iter->next) {
		BM_CHECK_ELEMENT(bm, l_iter);
		BM_CHECK_ELEMENT(bm, l_iter->e);
		BM_CHECK_ELEMENT(bm, l_iter->v);
		BM_CHECK_ELEMENT(bm, l_iter->f);
	}

	BLI_array_free(edar);

	BM_CHECK_ELEMENT(bm, f);

	return 1;
}

int bmesh_loop_reverse(BMesh *bm, BMFace *f)
{
#ifdef USE_BMESH_HOLES
	return bmesh_loop_reverse_loop(bm, f, f->loops.first);
#else
	return bmesh_loop_reverse_loop(bm, f);
#endif
}

static void bmesh_systag_elements(BMesh *UNUSED(bm), void *veles, int tot, int flag)
{
	BMHeader **eles = veles;
	int i;

	for (i = 0; i < tot; i++) {
		BM_ELEM_API_FLAG_ENABLE((BMElemF *)eles[i], flag);
	}
}

static void bmesh_clear_systag_elements(BMesh *UNUSED(bm), void *veles, int tot, int flag)
{
	BMHeader **eles = veles;
	int i;

	for (i = 0; i < tot; i++) {
		BM_ELEM_API_FLAG_DISABLE((BMElemF *)eles[i], flag);
	}
}

#define FACE_MARK	(1 << 10)

static int count_flagged_radial(BMesh *bm, BMLoop *l, int flag)
{
	BMLoop *l2 = l;
	int i = 0, c = 0;

	do {
		if (!l2) {
			bmesh_error();
			goto error;
		}
		
		i += BM_ELEM_API_FLAG_TEST(l2->f, flag) ? 1 : 0;
		l2 = bmesh_radial_nextloop(l2);
		if (c >= BM_LOOP_RADIAL_MAX) {
			bmesh_error();
			goto error;
		}
		c++;
	} while (l2 != l);

	return i;

error:
	BMO_error_raise(bm, bm->currentop, BMERR_MESH_ERROR, NULL);
	return 0;
}

static int UNUSED_FUNCTION(count_flagged_disk)(BMVert *v, int flag)
{
	BMEdge *e = v->e;
	int i = 0;

	if (!e)
		return 0;

	do {
		i += BM_ELEM_API_FLAG_TEST(e, flag) ? 1 : 0;
		e = bmesh_disk_nextedge(e, v);
	} while (e != v->e);

	return i;
}

static int disk_is_flagged(BMVert *v, int flag)
{
	BMEdge *e = v->e;

	if (!e)
		return FALSE;

	do {
		BMLoop *l = e->l;

		if (!l) {
			return FALSE;
		}
		
		if (bmesh_radial_length(l) == 1)
			return FALSE;
		
		do {
			if (!BM_ELEM_API_FLAG_TEST(l->f, flag))
				return FALSE;

			l = l->radial_next;
		} while (l != e->l);

		e = bmesh_disk_nextedge(e, v);
	} while (e != v->e);

	return TRUE;
}

/* Midlevel Topology Manipulation Functions */

/*
 * BM_faces_join
 *
 * Joins a collected group of faces into one. Only restriction on
 * the input data is that the faces must be connected to each other.
 *
 * If a pair of faces share multiple edges, the pair of
 * faces will be joined at every edge.
 *
 * Returns a pointer to the combined face.
 */
BMFace *BM_faces_join(BMesh *bm, BMFace **faces, int totface)
{
	BMFace *f, *newf;
#ifdef USE_BMESH_HOLES
	BMLoopList *lst;
	ListBase holes = {NULL, NULL};
#endif
	BMLoop *l_iter;
	BMLoop *l_first;
	BMEdge **edges = NULL;
	BMEdge **deledges = NULL;
	BMVert **delverts = NULL;
	BLI_array_staticdeclare(edges,    BM_NGON_STACK_SIZE);
	BLI_array_staticdeclare(deledges, BM_NGON_STACK_SIZE);
	BLI_array_staticdeclare(delverts, BM_NGON_STACK_SIZE);
	BMVert *v1 = NULL, *v2 = NULL;
	const char *err = NULL;
	int i, tote = 0;

	if (!totface) {
		bmesh_error();
		return NULL;
	}

	if (totface == 1)
		return faces[0];

	bmesh_systag_elements(bm, faces, totface, _FLAG_JF);

	for (i = 0; i < totface; i++) {
		f = faces[i];
		l_iter = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			int rlen = count_flagged_radial(bm, l_iter, _FLAG_JF);

			if (rlen > 2) {
				err = "Input faces do not form a contiguous manifold region";
				goto error;
			}
			else if (rlen == 1) {
				BLI_array_append(edges, l_iter->e);

				if (!v1) {
					v1 = l_iter->v;
					v2 = BM_edge_other_vert(l_iter->e, l_iter->v);
				}
				tote++;
			}
			else if (rlen == 2) {
				int d1, d2;

				d1 = disk_is_flagged(l_iter->e->v1, _FLAG_JF);
				d2 = disk_is_flagged(l_iter->e->v2, _FLAG_JF);

				if (!d1 && !d2 && !BM_ELEM_API_FLAG_TEST(l_iter->e, _FLAG_JF)) {
					/* don't remove an edge it makes up the side of another face
					 * else this will remove the face as well - campbell */
					if (BM_edge_face_count(l_iter->e) <= 2) {
						BLI_array_append(deledges, l_iter->e);
						BM_ELEM_API_FLAG_ENABLE(l_iter->e, _FLAG_JF);
					}
				}
				else {
					if (d1 && !BM_ELEM_API_FLAG_TEST(l_iter->e->v1, _FLAG_JF)) {
						BLI_array_append(delverts, l_iter->e->v1);
						BM_ELEM_API_FLAG_ENABLE(l_iter->e->v1, _FLAG_JF);
					}

					if (d2 && !BM_ELEM_API_FLAG_TEST(l_iter->e->v2, _FLAG_JF)) {
						BLI_array_append(delverts, l_iter->e->v2);
						BM_ELEM_API_FLAG_ENABLE(l_iter->e->v2, _FLAG_JF);
					}
				}
			}
		} while ((l_iter = l_iter->next) != l_first);

#ifdef USE_BMESH_HOLES
		for (lst = f->loops.first; lst; lst = lst->next) {
			if (lst == f->loops.first) continue;
			
			BLI_remlink(&f->loops, lst);
			BLI_addtail(&holes, lst);
		}
#endif

	}

	/* create region fac */
	newf = BM_face_create_ngon(bm, v1, v2, edges, tote, FALSE);
	if (!newf || BMO_error_occurred(bm)) {
		if (!BMO_error_occurred(bm))
			err = "Invalid boundary region to join faces";
		goto error;
	}

	/* copy over loop data */
	l_iter = l_first = BM_FACE_FIRST_LOOP(newf);
	do {
		BMLoop *l2 = l_iter->radial_next;

		do {
			if (BM_ELEM_API_FLAG_TEST(l2->f, _FLAG_JF))
				break;
			l2 = l2->radial_next;
		} while (l2 != l_iter);

		if (l2 != l_iter) {
			/* I think this is correct */
			if (l2->v != l_iter->v) {
				l2 = l2->next;
			}

			BM_elem_attrs_copy(bm, bm, l2, l_iter);
		}
	} while ((l_iter = l_iter->next) != l_first);
	
	BM_elem_attrs_copy(bm, bm, faces[0], newf);

#ifdef USE_BMESH_HOLES
	/* add hole */
	BLI_movelisttolist(&newf->loops, &holes);
#endif

	/* update loop face pointer */
#ifdef USE_BMESH_HOLES
	for (lst = newf->loops.first; lst; lst = lst->next)
#endif
	{
#ifdef USE_BMESH_HOLES
		l_iter = l_first = lst->first;
#else
		l_iter = l_first = BM_FACE_FIRST_LOOP(newf);
#endif
		do {
			l_iter->f = newf;
		} while ((l_iter = l_iter->next) != l_first);
	}

	bmesh_clear_systag_elements(bm, faces, totface, _FLAG_JF);
	BM_ELEM_API_FLAG_DISABLE(newf, _FLAG_JF);

	/* handle multires data */
	if (CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
		l_iter = l_first = BM_FACE_FIRST_LOOP(newf);
		do {
			for (i = 0; i < totface; i++) {
				BM_loop_interp_multires(bm, l_iter, faces[i]);
			}
		} while ((l_iter = l_iter->next) != l_first);
	}

	/* delete old geometr */
	for (i = 0; i < BLI_array_count(deledges); i++) {
		BM_edge_kill(bm, deledges[i]);
	}

	for (i = 0; i < BLI_array_count(delverts); i++) {
		BM_vert_kill(bm, delverts[i]);
	}
	
	BLI_array_free(edges);
	BLI_array_free(deledges);
	BLI_array_free(delverts);

	BM_CHECK_ELEMENT(bm, newf);
	return newf;
error:
	bmesh_clear_systag_elements(bm, faces, totface, _FLAG_JF);
	BLI_array_free(edges);
	BLI_array_free(deledges);
	BLI_array_free(delverts);

	if (err) {
		BMO_error_raise(bm, bm->currentop, BMERR_DISSOLVEFACES_FAILED, err);
	}
	return NULL;
}

static BMFace *bmesh_addpolylist(BMesh *bm, BMFace *UNUSED(example))
{
	BMFace *f;
#ifdef USE_BMESH_HOLES
	BMLoopList *lst;
#endif

	f = BLI_mempool_calloc(bm->fpool);
#ifdef USE_BMESH_HOLES
	lst = BLI_mempool_calloc(bm->looplistpool);
#endif

	f->head.htype = BM_FACE;
#ifdef USE_BMESH_HOLES
	BLI_addtail(&f->loops, lst);
#endif

#ifdef USE_DEBUG_INDEX_MEMCHECK
	DEBUG_MEMCHECK_INDEX_INVALIDATE(f)
#else
	BM_elem_index_set(f, -1); /* set_ok_invalid */
#endif

	bm->elem_index_dirty |= BM_FACE; /* may add to middle of the pool */

	bm->totface++;

	/* allocate flag */
	f->oflags = BLI_mempool_calloc(bm->toolflagpool);

	CustomData_bmesh_set_default(&bm->pdata, &f->head.data);

	f->len = 0;

#ifdef USE_BMESH_HOLES
	f->totbounds = 1;
#endif

	return (BMFace *) f;
}

/**
 *			bmesh_SFME
 *
 *	SPLIT FACE MAKE EDGE:
 *
 *	Takes as input two vertices in a single face. An edge is created which divides the original face
 *	into two distinct regions. One of the regions is assigned to the original face and it is closed off.
 *	The second region has a new face assigned to it.
 *
 *	Examples:
 *
 *     Before:               After:
 *	 ----------           ----------
 *	 |        |           |        |
 *	 |        |           |   f1   |
 *	v1   f1   v2          v1======v2
 *	 |        |           |   f2   |
 *	 |        |           |        |
 *	 ----------           ----------
 *
 *	Note that the input vertices can be part of the same edge. This will
 *  result in a two edged face. This is desirable for advanced construction
 *  tools and particularly essential for edge bevel. Because of this it is
 *  up to the caller to decide what to do with the extra edge.
 *
 *  If holes is NULL, then both faces will lose
 *  all holes from the original face.  Also, you cannot split between
 *  a hole vert and a boundary vert; that case is handled by higher-
 *  level wrapping functions (when holes are fully implemented, anyway).
 *
 *  Note that holes represents which holes goes to the new face, and of
 *  course this requires removing them from the exitsing face first, since
 *  you cannot have linked list links inside multiple lists.
 *
 *	Returns -
 *  A BMFace pointer
 */
BMFace *bmesh_sfme(BMesh *bm, BMFace *f, BMVert *v1, BMVert *v2,
                   BMLoop **rl,
#ifdef USE_BMESH_HOLES
                   ListBase *holes,
#endif
                   BMEdge *example
                   )
{
#ifdef USE_BMESH_HOLES
	BMLoopList *lst, *lst2;
#endif

	BMFace *f2;
	BMLoop *l_iter, *l_first;
	BMLoop *v1loop = NULL, *v2loop = NULL, *f1loop = NULL, *f2loop = NULL;
	BMEdge *e;
	int i, len, f1len, f2len;

	/* verify that v1 and v2 are in face */
	len = f->len;
	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f); i < len; i++, l_iter = l_iter->next) {
		if (l_iter->v == v1) v1loop = l_iter;
		else if (l_iter->v == v2) v2loop = l_iter;
	}

	if (!v1loop || !v2loop) {
		return NULL;
	}

	/* allocate new edge between v1 and v2 */
	e = BM_edge_create(bm, v1, v2, example, FALSE);

	f2 = bmesh_addpolylist(bm, f);
	f1loop = bmesh_create_loop(bm, v2, e, f, v2loop);
	f2loop = bmesh_create_loop(bm, v1, e, f2, v1loop);

	f1loop->prev = v2loop->prev;
	f2loop->prev = v1loop->prev;
	v2loop->prev->next = f1loop;
	v1loop->prev->next = f2loop;

	f1loop->next = v1loop;
	f2loop->next = v2loop;
	v1loop->prev = f1loop;
	v2loop->prev = f2loop;

#ifdef USE_BMESH_HOLES
	lst = f->loops.first;
	lst2 = f2->loops.first;

	lst2->first = lst2->last = f2loop;
	lst->first = lst->last = f1loop;
#else
	f2->l_first = f2loop;
	f->l_first = f1loop;
#endif

	/* validate both loop */
	/* I dont know how many loops are supposed to be in each face at this point! FIXME */

	/* go through all of f2's loops and make sure they point to it properly */
	l_iter = l_first = BM_FACE_FIRST_LOOP(f2);
	f2len = 0;
	do {
		l_iter->f = f2;
		f2len++;
	} while ((l_iter = l_iter->next) != l_first);

	/* link up the new loops into the new edges radia */
	bmesh_radial_append(e, f1loop);
	bmesh_radial_append(e, f2loop);

	f2->len = f2len;

	f1len = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		f1len++;
	} while ((l_iter = l_iter->next) != l_first);

	f->len = f1len;

	if (rl) *rl = f2loop;

#ifdef USE_BMESH_HOLES
	if (holes) {
		BLI_movelisttolist(&f2->loops, holes);
	}
	else {
		/* this code is not significant until holes actually work */
		//printf("warning: call to split face euler without holes argument; holes will be tossed.\n");
		for (lst = f->loops.last; lst != f->loops.first; lst = lst2) {
			lst2 = lst->prev;
			BLI_mempool_free(bm->looplistpool, lst);
		}
	}
#endif

	BM_CHECK_ELEMENT(bm, e);
	BM_CHECK_ELEMENT(bm, f);
	BM_CHECK_ELEMENT(bm, f2);
	
	return f2;
}

/**
 *			bmesh_SEMV
 *
 *	SPLIT EDGE MAKE VERT:
 *	Takes a given edge and splits it into two, creating a new vert.
 *
 *
 *		Before:	OV---------TV
 *		After:	OV----NV---TV
 *
 *  Returns -
 *	BMVert pointer.
 *
 */

BMVert *bmesh_semv(BMesh *bm, BMVert *tv, BMEdge *e, BMEdge **re)
{
	BMLoop *nextl;
	BMEdge *ne;
	BMVert *nv, *ov;
	int i, edok, valence1 = 0, valence2 = 0;

	if (bmesh_vert_in_edge(e, tv) == 0) {
		return NULL;
	}
	ov = bmesh_edge_getothervert(e, tv);

	/* count valence of v1 */
	valence1 = bmesh_disk_count(ov);

	/* count valence of v2 */
	valence2 = bmesh_disk_count(tv);

	nv = BM_vert_create(bm, tv->co, tv);
	ne = BM_edge_create(bm, nv, tv, e, FALSE);

	bmesh_disk_remove_edge(ne, tv);
	bmesh_disk_remove_edge(ne, nv);

	/* remove e from v2's disk cycle */
	bmesh_disk_remove_edge(e, tv);

	/* swap out tv for nv in e */
	bmesh_edge_swapverts(e, tv, nv);

	/* add e to nv's disk cycl */
	bmesh_disk_append_edge(e, nv);

	/* add ne to nv's disk cycl */
	bmesh_disk_append_edge(ne, nv);

	/* add ne to tv's disk cycl */
	bmesh_disk_append_edge(ne, tv);

	/* verify disk cycle */
	edok = bmesh_disk_validate(valence1, ov->e, ov);
	if (!edok) bmesh_error();
	edok = bmesh_disk_validate(valence2, tv->e, tv);
	if (!edok) bmesh_error();
	edok = bmesh_disk_validate(2, nv->e, nv);
	if (!edok) bmesh_error();

	/* Split the radial cycle if presen */
	nextl = e->l;
	e->l = NULL;
	if (nextl) {
		BMLoop *nl, *l;
		int radlen = bmesh_radial_length(nextl);
		int first1 = 0, first2 = 0;

		/* Take the next loop. Remove it from radial. Split it. Append to appropriate radials */
		while (nextl) {
			l = nextl;
			l->f->len++;
			nextl = nextl != nextl->radial_next ? nextl->radial_next : NULL;
			bmesh_radial_remove_loop(l, NULL);

			nl = bmesh_create_loop(bm, NULL, NULL, l->f, l);
			nl->prev = l;
			nl->next = (l->next);
			nl->prev->next = nl;
			nl->next->prev = nl;
			nl->v = nv;

			/* assign the correct edge to the correct loo */
			if (bmesh_verts_in_edge(nl->v, nl->next->v, e)) {
				nl->e = e;
				l->e = ne;

				/* append l into ne's rad cycl */
				if (!first1) {
					first1 = 1;
					l->radial_next = l->radial_prev = NULL;
				}

				if (!first2) {
					first2 = 1;
					l->radial_next = l->radial_prev = NULL;
				}
				
				bmesh_radial_append(nl->e, nl);
				bmesh_radial_append(l->e, l);
			}
			else if (bmesh_verts_in_edge(nl->v, nl->next->v, ne)) {
				nl->e = ne;
				l->e = e;

				/* append l into ne's rad cycl */
				if (!first1) {
					first1 = 1;
					l->radial_next = l->radial_prev = NULL;
				}

				if (!first2) {
					first2 = 1;
					l->radial_next = l->radial_prev = NULL;
				}

				bmesh_radial_append(nl->e, nl);
				bmesh_radial_append(l->e, l);
			}

		}

		/* verify length of radial cycl */
		edok = bmesh_radial_validate(radlen, e->l);
		if (!edok) bmesh_error();
		edok = bmesh_radial_validate(radlen, ne->l);
		if (!edok) bmesh_error();

		/* verify loop->v and loop->next->v pointers for  */
		for (i = 0, l = e->l; i < radlen; i++, l = l->radial_next) {
			if (!(l->e == e)) bmesh_error();
			//if (!(l->radial_next == l)) bmesh_error();
			if (l->prev->e != ne && l->next->e != ne) {
				bmesh_error();
			}
			edok = bmesh_verts_in_edge(l->v, l->next->v, e);
			if (!edok)               bmesh_error();
			if (l->v == l->next->v)  bmesh_error();
			if (l->e == l->next->e)  bmesh_error();

			/* verify loop cycle for kloop-> */
			BM_CHECK_ELEMENT(bm, l);
			BM_CHECK_ELEMENT(bm, l->v);
			BM_CHECK_ELEMENT(bm, l->e);
			BM_CHECK_ELEMENT(bm, l->f);
		}
		/* verify loop->v and loop->next->v pointers for n */
		for (i = 0, l = ne->l; i < radlen; i++, l = l->radial_next) {
			if (!(l->e == ne)) bmesh_error();
			//if (!(l->radial_next == l)) bmesh_error();
			if (l->prev->e != e && l->next->e != e) bmesh_error();
			edok = bmesh_verts_in_edge(l->v, l->next->v, ne);
			if (!edok)                bmesh_error();
			if (l->v == l->next->v)  bmesh_error();
			if (l->e == l->next->e)  bmesh_error();

			BM_CHECK_ELEMENT(bm, l);
			BM_CHECK_ELEMENT(bm, l->v);
			BM_CHECK_ELEMENT(bm, l->e);
			BM_CHECK_ELEMENT(bm, l->f);
		}
	}

	BM_CHECK_ELEMENT(bm, ne);
	BM_CHECK_ELEMENT(bm, nv);
	BM_CHECK_ELEMENT(bm, ov);
	BM_CHECK_ELEMENT(bm, e);
	BM_CHECK_ELEMENT(bm, tv);

	if (re) *re = ne;
	return nv;
}

/**
 *			bmesh_JEKV
 *
 *	JOIN EDGE KILL VERT:
 *	Takes a an edge and pointer to one of its vertices and collapses
 *	the edge on that vertex.
 *
 *	Before:    OE      KE
 *             	 ------- -------
 *               |     ||      |
 *		OV     KV      TV
 *
 *
 *   After:             OE
 *             	 ---------------
 *               |             |
 *		OV             TV
 *
 *
 *	Restrictions:
 *	KV is a vertex that must have a valance of exactly two. Furthermore
 *  both edges in KV's disk cycle (OE and KE) must be unique (no double
 *  edges).
 *
 *	It should also be noted that this euler has the possibility of creating
 *	faces with just 2 edges. It is up to the caller to decide what to do with
 *  these faces.
 *
 *  Returns -
 *	1 for success, 0 for failure.
 */
int bmesh_jekv(BMesh *bm, BMEdge *ke, BMVert *kv)
{
	BMEdge *oe;
	BMVert *ov, *tv;
	BMLoop *killoop, *l;
	int len, radlen = 0, halt = 0, i, valence1, valence2, edok;

	if (bmesh_vert_in_edge(ke, kv) == 0) {
		return FALSE;
	}

	len = bmesh_disk_count(kv);
	
	if (len == 2) {
		oe = bmesh_disk_nextedge(ke, kv);
		tv = bmesh_edge_getothervert(ke, kv);
		ov = bmesh_edge_getothervert(oe, kv);
		halt = bmesh_verts_in_edge(kv, tv, oe); /* check for double edge */
		
		if (halt) {
			return FALSE;
		}
		else {
			/* For verification later, count valence of ov and t */
			valence1 = bmesh_disk_count(ov);
			valence2 = bmesh_disk_count(tv);
			
			/* remove oe from kv's disk cycl */
			bmesh_disk_remove_edge(oe, kv);
			/* relink oe->kv to be oe->t */
			bmesh_edge_swapverts(oe, kv, tv);
			/* append oe to tv's disk cycl */
			bmesh_disk_append_edge(oe, tv);
			/* remove ke from tv's disk cycl */
			bmesh_disk_remove_edge(ke, tv);

			/* deal with radial cycle of k */
			radlen = bmesh_radial_length(ke->l);
			if (ke->l) {
				/* first step, fix the neighboring loops of all loops in ke's radial cycl */
				for (i = 0, killoop = ke->l; i < radlen; i++, killoop = bmesh_radial_nextloop(killoop)) {
					/* relink loops and fix vertex pointer */
					if (killoop->next->v == kv) {
						killoop->next->v = tv;
					}

					killoop->next->prev = killoop->prev;
					killoop->prev->next = killoop->next;
					if (BM_FACE_FIRST_LOOP(killoop->f) == killoop) {
						BM_FACE_FIRST_LOOP(killoop->f) = killoop->next;
					}
					killoop->next = NULL;
					killoop->prev = NULL;

					/* fix len attribute of fac */
					killoop->f->len--;
				}
				/* second step, remove all the hanging loops attached to k */
				radlen = bmesh_radial_length(ke->l);

				if (LIKELY(radlen)) {
					BMLoop **loops = NULL;
					BLI_array_fixedstack_declare(loops, BM_NGON_STACK_SIZE, radlen, __func__);

					killoop = ke->l;

					/* this should be wrapped into a bme_free_radial function to be used by bmesh_KF as well.. */
					for (i = 0; i < radlen; i++) {
						loops[i] = killoop;
						killoop = bmesh_radial_nextloop(killoop);
					}
					for (i = 0; i < radlen; i++) {
						bm->totloop--;
						BLI_mempool_free(bm->lpool, loops[i]);
					}
					BLI_array_fixedstack_free(loops);
				}

				/* Validate radial cycle of o */
				edok = bmesh_radial_validate(radlen, oe->l);
				if (!edok) {
					bmesh_error();
				}
			}

			/* deallocate edg */
			bmesh_kill_only_edge(bm, ke);

			/* deallocate verte */
			bmesh_kill_only_vert(bm, kv);

			/* Validate disk cycle lengths of ov, tv are unchange */
			edok = bmesh_disk_validate(valence1, ov->e, ov);
			if (!edok) bmesh_error();
			edok = bmesh_disk_validate(valence2, tv->e, tv);
			if (!edok) bmesh_error();

			/* Validate loop cycle of all faces attached to o */
			for (i = 0, l = oe->l; i < radlen; i++, l = bmesh_radial_nextloop(l)) {
				if (l->e != oe) bmesh_error();
				edok = bmesh_verts_in_edge(l->v, l->next->v, oe);
				if (!edok) bmesh_error();
				edok = bmesh_loop_validate(l->f);
				if (!edok) bmesh_error();

				BM_CHECK_ELEMENT(bm, l);
				BM_CHECK_ELEMENT(bm, l->v);
				BM_CHECK_ELEMENT(bm, l->e);
				BM_CHECK_ELEMENT(bm, l->f);
			}

			BM_CHECK_ELEMENT(bm, ov);
			BM_CHECK_ELEMENT(bm, tv);
			BM_CHECK_ELEMENT(bm, oe);

			return TRUE;
		}
	}
	return FALSE;
}

/**
 *			bmesh_JFKE
 *
 *	JOIN FACE KILL EDGE:
 *
 *	Takes two faces joined by a single 2-manifold edge and fuses them togather.
 *	The edge shared by the faces must not be connected to any other edges which have
 *	Both faces in its radial cycle
 *
 *	Examples:
 *
 *        A                   B
 *	 ----------           ----------
 *	 |        |           |        |
 *	 |   f1   |           |   f1   |
 *	v1========v2 = Ok!    v1==V2==v3 == Wrong!
 *	 |   f2   |           |   f2   |
 *	 |        |           |        |
 *	 ----------           ----------
 *
 *	In the example A, faces f1 and f2 are joined by a single edge, and the euler can safely be used.
 *	In example B however, f1 and f2 are joined by multiple edges and will produce an error. The caller
 *	in this case should call bmesh_JEKV on the extra edges before attempting to fuse f1 and f2.
 *
 *	Also note that the order of arguments decides whether or not certain per-face attributes are present
 *	in the resultant face. For instance vertex winding, material index, smooth flags, ect are inherited
 *	from f1, not f2.
 *
 *  Returns -
 *	A BMFace pointer
 */
BMFace *bmesh_jfke(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e)
{
	BMLoop *l_iter, *f1loop = NULL, *f2loop = NULL;
	int newlen = 0, i, f1len = 0, f2len = 0, radlen = 0, edok, shared;
	BMIter iter;

	/* can't join a face to itsel */
	if (f1 == f2) {
		return NULL;
	}

	/* verify that e is in both f1 and f2 */
	f1len = f1->len;
	f2len = f2->len;
	BM_ITER(l_iter, &iter, bm, BM_LOOPS_OF_FACE, f1) {
		if (l_iter->e == e) {
			f1loop = l_iter;
			break;
		}
	}
	BM_ITER(l_iter, &iter, bm, BM_LOOPS_OF_FACE, f2) {
		if (l_iter->e == e) {
			f2loop = l_iter;
			break;
		}
	}
	if (!(f1loop && f2loop)) {
		return NULL;
	}
	
	/* validate that edge is 2-manifold edg */
	radlen = bmesh_radial_length(f1loop);
	if (radlen != 2) {
		return NULL;
	}

	/* validate direction of f2's loop cycle is compatible */
	if (f1loop->v == f2loop->v) {
		return NULL;
	}

	/* validate that for each face, each vertex has another edge in its disk cycle that is
	 * not e, and not shared. */
	if ( bmesh_radial_find_face(f1loop->next->e, f2) ||
	     bmesh_radial_find_face(f1loop->prev->e, f2) ||
	     bmesh_radial_find_face(f2loop->next->e, f1) ||
	     bmesh_radial_find_face(f2loop->prev->e, f1) )
	{
		return NULL;
	}

	/* validate only one shared edg */
	shared = BM_face_share_edges(f1, f2);
	if (shared > 1) {
		return NULL;
	}

	/* validate no internal join */
	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f1); i < f1len; i++, l_iter = l_iter->next) {
		BM_elem_flag_disable(l_iter->v, BM_ELEM_TAG);
	}
	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f2); i < f2len; i++, l_iter = l_iter->next) {
		BM_elem_flag_disable(l_iter->v, BM_ELEM_TAG);
	}

	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f1); i < f1len; i++, l_iter = l_iter->next) {
		if (l_iter != f1loop) {
			BM_elem_flag_enable(l_iter->v, BM_ELEM_TAG);
		}
	}
	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f2); i < f2len; i++, l_iter = l_iter->next) {
		if (l_iter != f2loop) {
			/* as soon as a duplicate is found, bail out */
			if (BM_elem_flag_test(l_iter->v, BM_ELEM_TAG)) {
				return NULL;
			}
		}
	}

	/* join the two loop */
	f1loop->prev->next = f2loop->next;
	f2loop->next->prev = f1loop->prev;
	
	f1loop->next->prev = f2loop->prev;
	f2loop->prev->next = f1loop->next;
	
	/* if f1loop was baseloop, make f1loop->next the base. */
	if (BM_FACE_FIRST_LOOP(f1) == f1loop)
		BM_FACE_FIRST_LOOP(f1) = f1loop->next;

	/* increase length of f1 */
	f1->len += (f2->len - 2);

	/* make sure each loop points to the proper fac */
	newlen = f1->len;
	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f1); i < newlen; i++, l_iter = l_iter->next)
		l_iter->f = f1;
	
	/* remove edge from the disk cycle of its two vertices */
	bmesh_disk_remove_edge(f1loop->e, f1loop->e->v1);
	bmesh_disk_remove_edge(f1loop->e, f1loop->e->v2);
	
	/* deallocate edge and its two loops as well as f2 */
	BLI_mempool_free(bm->toolflagpool, f1loop->e->oflags);
	BLI_mempool_free(bm->epool, f1loop->e);
	bm->totedge--;
	BLI_mempool_free(bm->lpool, f1loop);
	bm->totloop--;
	BLI_mempool_free(bm->lpool, f2loop);
	bm->totloop--;
	BLI_mempool_free(bm->toolflagpool, f2->oflags);
	BLI_mempool_free(bm->fpool, f2);
	bm->totface--;
	/* account for both above */
	bm->elem_index_dirty |= BM_EDGE | BM_FACE;

	BM_CHECK_ELEMENT(bm, f1);

	/* validate the new loop cycle */
	edok = bmesh_loop_validate(f1);
	if (!edok) bmesh_error();
	
	return f1;
}

/*
 * BMESH SPLICE VERT
 *
 * merges two verts into one (v into vtarget).
 */
static int bmesh_splicevert(BMesh *bm, BMVert *v, BMVert *vtarget)
{
	BMEdge *e;
	BMLoop *l;
	BMIter liter;

	/* verts already spliced */
	if (v == vtarget) {
		return FALSE;
	}

	/* retarget all the loops of v to vtarget */
	BM_ITER(l, &liter, bm, BM_LOOPS_OF_VERT, v) {
		l->v = vtarget;
	}

	/* move all the edges from v's disk to vtarget's disk */
	e = v->e;
	while (e != NULL) {
		bmesh_disk_remove_edge(e, v);
		bmesh_edge_swapverts(e, v, vtarget);
		bmesh_disk_append_edge(e, vtarget);
		e = v->e;
	}

	BM_CHECK_ELEMENT(bm, v);
	BM_CHECK_ELEMENT(bm, vtarget);

	/* v is unused now, and can be killed */
	BM_vert_kill(bm, v);

	return TRUE;
}

/* BMESH CUT VERT
 *
 * cut all disjoint fans that meet at a vertex, making a unique
 * vertex for each region. returns an array of all resulting
 * vertices.
 */
static int bmesh_cutvert(BMesh *bm, BMVert *v, BMVert ***vout, int *len)
{
	BMEdge **stack = NULL;
	BLI_array_declare(stack);
	BMVert **verts = NULL;
	GHash *visithash;
	BMIter eiter, liter;
	BMLoop *l;
	BMEdge *e;
	int i, maxindex;
	BMLoop *nl;

	visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, "bmesh_cutvert visithash");

	maxindex = 0;
	BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
		if (BLI_ghash_haskey(visithash, e)) {
			continue;
		}

		/* Prime the stack with this unvisited edge */
		BLI_array_append(stack, e);

		/* Considering only edges and faces incident on vertex v, walk
		 * the edges & faces and assign an index to each connected set */
		while ((e = BLI_array_pop(stack))) {
			BLI_ghash_insert(visithash, e, SET_INT_IN_POINTER(maxindex));

			BM_ITER(l, &liter, bm, BM_LOOPS_OF_EDGE, e) {
				nl = (l->v == v) ? l->prev : l->next;
				if (!BLI_ghash_haskey(visithash, nl->e)) {
					BLI_array_append(stack, nl->e);
				}
			}
		}

		maxindex++;
	}

	/* Make enough verts to split v for each group */
	verts = MEM_callocN(sizeof(BMVert *) * maxindex, "bmesh_cutvert");
	verts[0] = v;
	for (i = 1; i < maxindex; i++) {
		verts[i] = BM_vert_create(bm, v->co, v);
	}

	/* Replace v with the new verts in each group */
	BM_ITER(l, &liter, bm, BM_LOOPS_OF_VERT, v) {
		i = GET_INT_FROM_POINTER(BLI_ghash_lookup(visithash, l->e));
		if (i == 0) {
			continue;
		}

		/* Loops here should alway refer to an edge that has v as an
		 * endpoint. For each appearance of this vert in a face, there
		 * will actually be two iterations: one for the loop heading
		 * towards vertex v, and another for the loop heading out from
		 * vertex v. Only need to swap the vertex on one of those times,
		 * on the outgoing loop. */
		if (l->v == v) {
			l->v = verts[i];
		}
	}

	BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, v) {
		i = GET_INT_FROM_POINTER(BLI_ghash_lookup(visithash, e));
		if (i == 0) {
			continue;
		}

		BLI_assert(e->v1 == v || e->v2 == v);
		bmesh_disk_remove_edge(e, v);
		bmesh_edge_swapverts(e, v, verts[i]);
		bmesh_disk_append_edge(e, verts[i]);
	}

	BLI_ghash_free(visithash, NULL, NULL);
	BLI_array_free(stack);

	for (i = 0; i < maxindex; i++) {
		BM_CHECK_ELEMENT(bm, verts[i]);
	}

	if (len != NULL) {
		*len = maxindex;
	}

	if (vout != NULL) {
		*vout = verts;
	}
	else {
		MEM_freeN(verts);
	}

	return TRUE;
}

/* BMESH SPLICE EDGE
 *
 * splice two unique edges which share the same two vertices into one edge.
 *
 * edges must already have the same vertices
 */
static int UNUSED_FUNCTION(bmesh_spliceedge)(BMesh *bm, BMEdge *e, BMEdge *etarget)
{
	BMLoop *l;

	if (!BM_vert_in_edge(e, etarget->v1) || !BM_vert_in_edge(e, etarget->v2)) {
		/* not the same vertices can't splice */
		return FALSE;
	}

	while (e->l) {
		l = e->l;
		BLI_assert(BM_vert_in_edge(etarget, l->v));
		BLI_assert(BM_vert_in_edge(etarget, l->next->v));
		bmesh_radial_remove_loop(l, e);
		bmesh_radial_append(etarget, l);
	}

	BLI_assert(bmesh_radial_length(e->l) == 0);

	BM_CHECK_ELEMENT(bm, e);
	BM_CHECK_ELEMENT(bm, etarget);

	BM_edge_kill(bm, e);

	return TRUE;
}

/*
 * BMESH CUT EDGE
 *
 * Cuts a single edge into two edge: the original edge and
 * a new edge that has only "cutl" in its radial.
 *
 * Does nothing if cutl is already the only loop in the
 * edge radial.
 */
static int bmesh_cutedge(BMesh *bm, BMEdge *e, BMLoop *cutl)
{
	BMEdge *ne;
	int radlen;

	BLI_assert(cutl->e == e);
	BLI_assert(e->l);
	
	radlen = bmesh_radial_length(e->l);
	if (radlen < 2) {
		/* no cut required */
		return TRUE;
	}

	if (cutl == e->l) {
		e->l = cutl->radial_next;
	}

	ne = BM_edge_create(bm, e->v1, e->v2, e, FALSE);
	bmesh_radial_remove_loop(cutl, e);
	bmesh_radial_append(ne, cutl);
	cutl->e = ne;

	BLI_assert(bmesh_radial_length(e->l) == radlen - 1);
	BLI_assert(bmesh_radial_length(ne->l) == 1);

	BM_CHECK_ELEMENT(bm, ne);
	BM_CHECK_ELEMENT(bm, e);

	return TRUE;
}

/*
 * BMESH UNGLUE REGION MAKE VERT
 *
 * Disconnects a face from its vertex fan at loop sl.
 */
static BMVert *bmesh_urmv_loop(BMesh *bm, BMLoop *sl)
{
	BMVert **vtar;
	int len, i;
	BMVert *nv = NULL;
	BMVert *sv = sl->v;

	/* peel the face from the edge radials on both sides of the
	 * loop vert, disconnecting the face from its fan */
	bmesh_cutedge(bm, sl->e, sl);
	bmesh_cutedge(bm, sl->prev->e, sl->prev);

	if (bmesh_disk_count(sv) == 2) {
		/* If there are still only two edges out of sv, then
		 * this whole URMV was just a no-op, so exit now. */
		return sv;
	}

	/* Update the disk start, so that v->e points to an edge
	 * not touching the split loop. This is so that bmesh_cutvert
	 * will leave the original sv on some *other* fan (not the
	 * one-face fan that holds the unglue face). */
	while (sv->e == sl->e || sv->e == sl->prev->e) {
		sv->e = bmesh_disk_nextedge(sv->e, sv);
	}

	/* Split all fans connected to the vert, duplicating it for
	 * each fans. */
	bmesh_cutvert(bm, sv, &vtar, &len);

	/* There should have been at least two fans cut apart here,
	 * otherwise the early exit would have kicked in. */
	BLI_assert(len >= 2);

	nv = sl->v;

	/* Desired result here is that a new vert should always be
	 * created for the unglue face. This is so we can glue any
	 * extras back into the original vert. */
	BLI_assert(nv != sv);
	BLI_assert(sv == vtar[0]);

	/* If there are more than two verts as a result, glue together
	 * all the verts except the one this URMV intended to create */
	if (len > 2) {
		for (i = 0; i < len; i++) {
			if (vtar[i] == nv) {
				break;
			}
		}

		if (i != len) {
			/* Swap the single vert that was needed for the
			 * unglue into the last array slot */
			SWAP(BMVert *, vtar[i], vtar[len - 1]);

			/* And then glue the rest back together */
			for (i = 1; i < len - 1; i++) {
				bmesh_splicevert(bm, vtar[i], vtar[0]);
			}
		}
	}

	MEM_freeN(vtar);

	return nv;
}

/*
 * BMESH UNGLUE REGION MAKE VERT
 *
 * Disconnects sf from the vertex fan at sv
 */
BMVert *bmesh_urmv(BMesh *bm, BMFace *sf, BMVert *sv)
{
	BMLoop *l_first;
	BMLoop *l_iter;

	l_iter = l_first = BM_FACE_FIRST_LOOP(sf);
	do {
		if (l_iter->v == sv) {
			break;
		}
	} while ((l_iter = l_iter->next) != l_first);

	if (l_iter->v != sv) {
		/* sv is not part of sf */
		return NULL;
	}

	return bmesh_urmv_loop(bm, l_iter);
}
