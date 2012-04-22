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

/** \file blender/bmesh/intern/bmesh_core.c
 *  \ingroup bmesh
 *
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"

#include "BKE_DerivedMesh.h"

#include "BLI_listbase.h"
#include "BLI_array.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

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

BMVert *BM_vert_create(BMesh *bm, const float co[3], const BMVert *example)
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
	if (co) {
		copy_v3_v3(v->co, co);
	}

	/* allocate flag */
	v->oflags = BLI_mempool_calloc(bm->toolflagpool);

	CustomData_bmesh_set_default(&bm->vdata, &v->head.data);
	
	if (example) {
		BM_elem_attrs_copy(bm, bm, example, v);
	}

	BM_CHECK_ELEMENT(v);

	return v;
}

BMEdge *BM_edge_create(BMesh *bm, BMVert *v1, BMVert *v2, const BMEdge *example, int nodouble)
{
	BMEdge *e;
	
	if (nodouble && (e = BM_edge_exists(v1, v2)))
		return e;
	
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

	e->v1 = v1;
	e->v2 = v2;
	
	BM_elem_flag_enable(e, BM_ELEM_SMOOTH);
	
	CustomData_bmesh_set_default(&bm->edata, &e->head.data);
	
	bmesh_disk_edge_append(e, e->v1);
	bmesh_disk_edge_append(e, e->v2);
	
	if (example)
		BM_elem_attrs_copy(bm, bm, example, e);
	
	BM_CHECK_ELEMENT(e);

	return e;
}

static BMLoop *bm_loop_create(BMesh *bm, BMVert *v, BMEdge *e, BMFace *f, const BMLoop *example)
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

	if (example) {
		CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata, example->head.data, &l->head.data);
	}
	else {
		CustomData_bmesh_set_default(&bm->ldata, &l->head.data);
	}

	return l;
}

static BMLoop *bm_face_boundary_add(BMesh *bm, BMFace *f, BMVert *startv, BMEdge *starte)
{
#ifdef USE_BMESH_HOLES
	BMLoopList *lst = BLI_mempool_calloc(bm->looplistpool);
#endif
	BMLoop *l = bm_loop_create(bm, startv, starte, f, NULL);
	
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

/**
 * only create the face, since this calloc's the length is initialized to 0,
 * leave adding loops to the caller.
 */
BLI_INLINE BMFace *bm_face_create__internal(BMesh *bm)
{
	BMFace *f;

	f = BLI_mempool_calloc(bm->fpool);

#ifdef USE_DEBUG_INDEX_MEMCHECK
	DEBUG_MEMCHECK_INDEX_INVALIDATE(f)
#else
	BM_elem_index_set(f, -1); /* set_ok_invalid */
#endif

	bm->elem_index_dirty |= BM_FACE; /* may add to middle of the pool */

	bm->totface++;

	f->head.htype = BM_FACE;

	/* allocate flag */
	f->oflags = BLI_mempool_calloc(bm->toolflagpool);

	CustomData_bmesh_set_default(&bm->pdata, &f->head.data);

#ifdef USE_BMESH_HOLES
	f->totbounds = 0;
#endif

	return f;
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

	f = bm_face_create__internal(bm);

	startl = lastl = bm_face_boundary_add(bm, f, verts[0], edges[0]);
	
	startl->v = verts[0];
	startl->e = edges[0];
	for (i = 1; i < len; i++) {
		l = bm_loop_create(bm, verts[i], edges[i], f, edges[i]->l);
		
		l->f = f;
		bmesh_radial_append(edges[i], l);

		l->prev = lastl;
		lastl->next = l;
		lastl = l;
	}
	
	startl->prev = lastl;
	lastl->next = startl;
	
	f->len = len;
	
	BM_CHECK_ELEMENT(f);

	return f;
}

int bmesh_elem_check(void *element, const char htype)
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

			/* validate boundary loop -- invalid for hole loops, of course,
			 * but we won't be allowing those for a while yet */
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

	BMESH_ASSERT(err == 0);

	return err;
}

/**
 * low level function, only free's,
 * does not change adjust surrounding geometry */
static void bm_kill_only_vert(BMesh *bm, BMVert *v)
{
	bm->totvert--;
	bm->elem_index_dirty |= BM_VERT;

	BM_select_history_remove(bm, (BMElem *)v);
	if (v->head.data)
		CustomData_bmesh_free_block(&bm->vdata, &v->head.data);

	BLI_mempool_free(bm->toolflagpool, v->oflags);
	BLI_mempool_free(bm->vpool, v);
}

static void bm_kill_only_edge(BMesh *bm, BMEdge *e)
{
	bm->totedge--;
	bm->elem_index_dirty |= BM_EDGE;

	BM_select_history_remove(bm, (BMElem *)e);

	if (e->head.data)
		CustomData_bmesh_free_block(&bm->edata, &e->head.data);

	BLI_mempool_free(bm->toolflagpool, e->oflags);
	BLI_mempool_free(bm->epool, e);
}

static void bm_kill_only_face(BMesh *bm, BMFace *f)
{
	if (bm->act_face == f)
		bm->act_face = NULL;

	bm->totface--;
	bm->elem_index_dirty |= BM_FACE;

	BM_select_history_remove(bm, (BMElem *)f);

	if (f->head.data)
		CustomData_bmesh_free_block(&bm->pdata, &f->head.data);

	BLI_mempool_free(bm->toolflagpool, f->oflags);
	BLI_mempool_free(bm->fpool, f);
}

static void bm_kill_only_loop(BMesh *bm, BMLoop *l)
{
	bm->totloop--;
	if (l->head.data)
		CustomData_bmesh_free_block(&bm->ldata, &l->head.data);

	BLI_mempool_free(bm->lpool, l);
}

/**
 * kills all edges associated with f, along with any other faces containing
 * those edges
 */
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

/**
 * kills all verts associated with f, along with any other faces containing
 * those vertices
 */
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

	BM_CHECK_ELEMENT(f);

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

			bmesh_radial_loop_remove(l_iter, l_iter->e);
			bm_kill_only_loop(bm, l_iter);

		} while ((l_iter = l_next) != l_first);

#ifdef USE_BMESH_HOLES
		BLI_mempool_free(bm->looplistpool, ls);
#endif
	}

	bm_kill_only_face(bm, f);
}

void BM_edge_kill(BMesh *bm, BMEdge *e)
{

	bmesh_disk_edge_remove(e, e->v1);
	bmesh_disk_edge_remove(e, e->v2);

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
	
	bm_kill_only_edge(bm, e);
}

void BM_vert_kill(BMesh *bm, BMVert *v)
{
	if (v->e) {
		BMEdge *e, *nexte;
		
		e = v->e;
		while (v->e) {
			nexte = bmesh_disk_edge_next(e, v);
			BM_edge_kill(bm, e);
			e = nexte;
		}
	}

	bm_kill_only_vert(bm, v);
}

/********** private disk and radial cycle functions ********** */

static int bm_loop_length(BMLoop *l)
{
	BMLoop *l_first = l;
	int i = 0;

	do {
		i++;
	} while ((l = l->next) != l_first);

	return i;
}

/**
 * \brief Loop Reverse
 *
 * Changes the winding order of a face from CW to CCW or vice versa.
 * This euler is a bit peculiar in comparison to others as it is its
 * own inverse.
 *
 * BMESH_TODO: reinsert validation code.
 *
 * \return Success
 */
static int bm_loop_reverse_loop(BMesh *bm, BMFace *f
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

	len = bm_loop_length(l_first);

	for (i = 0, l_iter = l_first; i < len; i++, l_iter = l_iter->next) {
		BMEdge *curedge = l_iter->e;
		bmesh_radial_loop_remove(l_iter, curedge);
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
		BM_CHECK_ELEMENT(l_iter);
		BM_CHECK_ELEMENT(l_iter->e);
		BM_CHECK_ELEMENT(l_iter->v);
		BM_CHECK_ELEMENT(l_iter->f);
	}

	BLI_array_free(edar);

	BM_CHECK_ELEMENT(f);

	return 1;
}

int bmesh_loop_reverse(BMesh *bm, BMFace *f)
{
#ifdef USE_BMESH_HOLES
	return bmesh_loop_reverse_loop(bm, f, f->loops.first);
#else
	return bm_loop_reverse_loop(bm, f);
#endif
}

static void bm_elements_systag_enable(void *veles, int tot, int flag)
{
	BMHeader **eles = veles;
	int i;

	for (i = 0; i < tot; i++) {
		BM_ELEM_API_FLAG_ENABLE((BMElemF *)eles[i], flag);
	}
}

static void bm_elements_systag_disable(void *veles, int tot, int flag)
{
	BMHeader **eles = veles;
	int i;

	for (i = 0; i < tot; i++) {
		BM_ELEM_API_FLAG_DISABLE((BMElemF *)eles[i], flag);
	}
}

#define FACE_MARK  (1 << 10)

static int count_flagged_radial(BMesh *bm, BMLoop *l, int flag)
{
	BMLoop *l2 = l;
	int i = 0, c = 0;

	do {
		if (UNLIKELY(!l2)) {
			BMESH_ASSERT(0);
			goto error;
		}
		
		i += BM_ELEM_API_FLAG_TEST(l2->f, flag) ? 1 : 0;
		l2 = l2->radial_next;
		if (UNLIKELY(c >= BM_LOOP_RADIAL_MAX)) {
			BMESH_ASSERT(0);
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
	} while ((e = bmesh_disk_edge_next(e, v)) != v->e);

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

		e = bmesh_disk_edge_next(e, v);
	} while (e != v->e);

	return TRUE;
}

/* Mid-level Topology Manipulation Functions */

/**
 * \brief Join Connected Faces
 *
 * Joins a collected group of faces into one. Only restriction on
 * the input data is that the faces must be connected to each other.
 *
 * \return The newly created combine BMFace.
 *
 * \note If a pair of faces share multiple edges,
 * the pair of faces will be joined at every edge.
 *
 * \note this is a generic, flexible join faces function,
 * almost everything uses this, including #BM_faces_join_pair
 */
BMFace *BM_faces_join(BMesh *bm, BMFace **faces, int totface, const short do_del)
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

	if (UNLIKELY(!totface)) {
		BMESH_ASSERT(0);
		return NULL;
	}

	if (totface == 1)
		return faces[0];

	bm_elements_systag_enable(faces, totface, _FLAG_JF);

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
						if (do_del) {
							BLI_array_append(deledges, l_iter->e);
						}
						BM_ELEM_API_FLAG_ENABLE(l_iter->e, _FLAG_JF);
					}
				}
				else {
					if (d1 && !BM_ELEM_API_FLAG_TEST(l_iter->e->v1, _FLAG_JF)) {
						if (do_del) {
							BLI_array_append(delverts, l_iter->e->v1);
						}
						BM_ELEM_API_FLAG_ENABLE(l_iter->e->v1, _FLAG_JF);
					}

					if (d2 && !BM_ELEM_API_FLAG_TEST(l_iter->e->v2, _FLAG_JF)) {
						if (do_del) {
							BLI_array_append(delverts, l_iter->e->v2);
						}
						BM_ELEM_API_FLAG_ENABLE(l_iter->e->v2, _FLAG_JF);
					}
				}
			}
		} while ((l_iter = l_iter->next) != l_first);

#ifdef USE_BMESH_HOLES
		for (lst = f->loops.first; lst; lst = lst->next) {
			if (lst == f->loops.first) {
				continue;
			}

			BLI_remlink(&f->loops, lst);
			BLI_addtail(&holes, lst);
		}
#endif

	}

	/* create region face */
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

	bm_elements_systag_disable(faces, totface, _FLAG_JF);
	BM_ELEM_API_FLAG_DISABLE(newf, _FLAG_JF);

	/* handle multi-res data */
	if (CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
		l_iter = l_first = BM_FACE_FIRST_LOOP(newf);
		do {
			for (i = 0; i < totface; i++) {
				BM_loop_interp_multires(bm, l_iter, faces[i]);
			}
		} while ((l_iter = l_iter->next) != l_first);
	}

	/* delete old geometry */
	if (do_del) {
		for (i = 0; i < BLI_array_count(deledges); i++) {
			BM_edge_kill(bm, deledges[i]);
		}

		for (i = 0; i < BLI_array_count(delverts); i++) {
			BM_vert_kill(bm, delverts[i]);
		}
	}
	else {
		/* otherwise we get both old and new faces */
		for (i = 0; i < totface; i++) {
			BM_face_kill(bm, faces[i]);
		}
	}
	
	BLI_array_free(edges);
	BLI_array_free(deledges);
	BLI_array_free(delverts);

	BM_CHECK_ELEMENT(newf);
	return newf;

error:
	bm_elements_systag_disable(faces, totface, _FLAG_JF);
	BLI_array_free(edges);
	BLI_array_free(deledges);
	BLI_array_free(delverts);

	if (err) {
		BMO_error_raise(bm, bm->currentop, BMERR_DISSOLVEFACES_FAILED, err);
	}
	return NULL;
}

static BMFace *bm_face_create__sfme(BMesh *bm, BMFace *UNUSED(example))
{
	BMFace *f;
#ifdef USE_BMESH_HOLES
	BMLoopList *lst;
#endif

	f = bm_face_create__internal(bm);

#ifdef USE_BMESH_HOLES
	lst = BLI_mempool_calloc(bm->looplistpool);
	BLI_addtail(&f->loops, lst);
#endif

#ifdef USE_BMESH_HOLES
	f->totbounds = 1;
#endif

	return f;
}

/**
 * \brief Split Face Make Edge (SFME)
 *
 * Takes as input two vertices in a single face. An edge is created which divides the original face
 * into two distinct regions. One of the regions is assigned to the original face and it is closed off.
 * The second region has a new face assigned to it.
 *
 * \par Examples:
 *
 *     Before:               After:
 *      +--------+           +--------+
 *      |        |           |        |
 *      |        |           |   f1   |
 *     v1   f1   v2          v1======v2
 *      |        |           |   f2   |
 *      |        |           |        |
 *      +--------+           +--------+
 *
 * \note the input vertices can be part of the same edge. This will
 * result in a two edged face. This is desirable for advanced construction
 * tools and particularly essential for edge bevel. Because of this it is
 * up to the caller to decide what to do with the extra edge.
 *
 * \note If \a holes is NULL, then both faces will lose
 * all holes from the original face.  Also, you cannot split between
 * a hole vert and a boundary vert; that case is handled by higher-
 * level wrapping functions (when holes are fully implemented, anyway).
 *
 * \note that holes represents which holes goes to the new face, and of
 * course this requires removing them from the existing face first, since
 * you cannot have linked list links inside multiple lists.
 *
 * \return A BMFace pointer
 */
BMFace *bmesh_sfme(BMesh *bm, BMFace *f, BMVert *v1, BMVert *v2,
                   BMLoop **r_l,
#ifdef USE_BMESH_HOLES
                   ListBase *holes,
#endif
                   BMEdge *example,
                   const short nodouble
                   )
{
#ifdef USE_BMESH_HOLES
	BMLoopList *lst, *lst2;
#endif

	BMFace *f2;
	BMLoop *l_iter, *l_first;
	BMLoop *v1loop = NULL, *v2loop = NULL, *f1loop = NULL, *f2loop = NULL;
	BMEdge *e;
	int i, len, f1len, f2len, first_loop_f1;

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
	e = BM_edge_create(bm, v1, v2, example, nodouble);

	f2 = bm_face_create__sfme(bm, f);
	f1loop = bm_loop_create(bm, v2, e, f, v2loop);
	f2loop = bm_loop_create(bm, v1, e, f2, v1loop);

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
	/* find which of the faces the original first loop is in */
	l_iter = l_first = f1loop;
	first_loop_f1 = 0;
	do {
		if (l_iter == f->l_first)
			first_loop_f1 = 1;
	} while ((l_iter = l_iter->next) != l_first);

	if (first_loop_f1) {
		/* original first loop was in f1, find a suitable first loop for f2
		 * which is as similar as possible to f1. the order matters for tools
		 * such as duplifaces. */
		if (f->l_first->prev == f1loop)
			f2->l_first = f2loop->prev;
		else if (f->l_first->next == f1loop)
			f2->l_first = f2loop->next;
		else
			f2->l_first = f2loop;
	}
	else {
		/* original first loop was in f2, further do same as above */
		f2->l_first = f->l_first;

		if (f->l_first->prev == f2loop)
			f->l_first = f1loop->prev;
		else if (f->l_first->next == f2loop)
			f->l_first = f1loop->next;
		else
			f->l_first = f1loop;
	}
#endif

	/* validate both loop */
	/* I don't know how many loops are supposed to be in each face at this point! FIXME */

	/* go through all of f2's loops and make sure they point to it properly */
	l_iter = l_first = BM_FACE_FIRST_LOOP(f2);
	f2len = 0;
	do {
		l_iter->f = f2;
		f2len++;
	} while ((l_iter = l_iter->next) != l_first);

	/* link up the new loops into the new edges radial */
	bmesh_radial_append(e, f1loop);
	bmesh_radial_append(e, f2loop);

	f2->len = f2len;

	f1len = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		f1len++;
	} while ((l_iter = l_iter->next) != l_first);

	f->len = f1len;

	if (r_l) *r_l = f2loop;

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

	BM_CHECK_ELEMENT(e);
	BM_CHECK_ELEMENT(f);
	BM_CHECK_ELEMENT(f2);
	
	return f2;
}

/**
 * \brief Split Edge Make Vert (SEMV)
 *
 * Takes \a e edge and splits it into two, creating a new vert.
 * \a tv should be one end of \a e : the newly created edge
 * will be attached to that end and is returned in \a r_e.
 *
 * \par Examples:
 *
 *                     E
 *     Before: OV-------------TV
 *
 *                 E       RE
 *     After:  OV------NV-----TV
 *
 * \return The newly created BMVert pointer.
 */
BMVert *bmesh_semv(BMesh *bm, BMVert *tv, BMEdge *e, BMEdge **r_e)
{
	BMLoop *nextl;
	BMEdge *ne;
	BMVert *nv, *ov;
	int i, edok, valence1 = 0, valence2 = 0;

	BLI_assert(bmesh_vert_in_edge(e, tv) != FALSE);

	ov = bmesh_edge_other_vert_get(e, tv);

	valence1 = bmesh_disk_count(ov);

	valence2 = bmesh_disk_count(tv);

	nv = BM_vert_create(bm, tv->co, tv);
	ne = BM_edge_create(bm, nv, tv, e, FALSE);

	bmesh_disk_edge_remove(ne, tv);
	bmesh_disk_edge_remove(ne, nv);

	/* remove e from tv's disk cycle */
	bmesh_disk_edge_remove(e, tv);

	/* swap out tv for nv in e */
	bmesh_edge_swapverts(e, tv, nv);

	/* add e to nv's disk cycle */
	bmesh_disk_edge_append(e, nv);

	/* add ne to nv's disk cycle */
	bmesh_disk_edge_append(ne, nv);

	/* add ne to tv's disk cycle */
	bmesh_disk_edge_append(ne, tv);

	/* verify disk cycle */
	edok = bmesh_disk_validate(valence1, ov->e, ov);
	BMESH_ASSERT(edok != FALSE);
	edok = bmesh_disk_validate(valence2, tv->e, tv);
	BMESH_ASSERT(edok != FALSE);
	edok = bmesh_disk_validate(2, nv->e, nv);
	BMESH_ASSERT(edok != FALSE);

	/* Split the radial cycle if present */
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
			bmesh_radial_loop_remove(l, NULL);

			nl = bm_loop_create(bm, NULL, NULL, l->f, l);
			nl->prev = l;
			nl->next = (l->next);
			nl->prev->next = nl;
			nl->next->prev = nl;
			nl->v = nv;

			/* assign the correct edge to the correct loop */
			if (bmesh_verts_in_edge(nl->v, nl->next->v, e)) {
				nl->e = e;
				l->e = ne;

				/* append l into ne's rad cycle */
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

				/* append l into ne's rad cycle */
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

		/* verify length of radial cycle */
		edok = bmesh_radial_validate(radlen, e->l);
		BMESH_ASSERT(edok != FALSE);
		edok = bmesh_radial_validate(radlen, ne->l);
		BMESH_ASSERT(edok != FALSE);

		/* verify loop->v and loop->next->v pointers for e */
		for (i = 0, l = e->l; i < radlen; i++, l = l->radial_next) {
			BMESH_ASSERT(l->e == e);
			//BMESH_ASSERT(l->radial_next == l);
			BMESH_ASSERT(!(l->prev->e != ne && l->next->e != ne));

			edok = bmesh_verts_in_edge(l->v, l->next->v, e);
			BMESH_ASSERT(edok != FALSE);
			BMESH_ASSERT(l->v != l->next->v);
			BMESH_ASSERT(l->e != l->next->e);

			/* verify loop cycle for kloop-> */
			BM_CHECK_ELEMENT(l);
			BM_CHECK_ELEMENT(l->v);
			BM_CHECK_ELEMENT(l->e);
			BM_CHECK_ELEMENT(l->f);
		}
		/* verify loop->v and loop->next->v pointers for ne */
		for (i = 0, l = ne->l; i < radlen; i++, l = l->radial_next) {
			BMESH_ASSERT(l->e == ne);
			// BMESH_ASSERT(l->radial_next == l);
			BMESH_ASSERT(!(l->prev->e != e && l->next->e != e));
			edok = bmesh_verts_in_edge(l->v, l->next->v, ne);
			BMESH_ASSERT(edok != FALSE);
			BMESH_ASSERT(l->v != l->next->v);
			BMESH_ASSERT(l->e != l->next->e);

			BM_CHECK_ELEMENT(l);
			BM_CHECK_ELEMENT(l->v);
			BM_CHECK_ELEMENT(l->e);
			BM_CHECK_ELEMENT(l->f);
		}
	}

	BM_CHECK_ELEMENT(ne);
	BM_CHECK_ELEMENT(nv);
	BM_CHECK_ELEMENT(ov);
	BM_CHECK_ELEMENT(e);
	BM_CHECK_ELEMENT(tv);

	if (r_e) *r_e = ne;
	return nv;
}

/**
 * \brief Join Edge Kill Vert (JEKV)
 *
 * Takes an edge \a ke and pointer to one of its vertices \a kv
 * and collapses the edge on that vertex.
 *
 * \par Examples:
 *
 *     Before:         OE      KE
 *                   ------- -------
 *                   |     ||      |
 *                  OV     KV      TV
 *
 *
 *     After:              OE
 *                   ---------------
 *                   |             |
 *                  OV             TV
 *
 * \par Restrictions:
 * KV is a vertex that must have a valance of exactly two. Furthermore
 * both edges in KV's disk cycle (OE and KE) must be unique (no double edges).
 *
 * \return The resulting edge, NULL for failure.
 *
 * \note This euler has the possibility of creating
 * faces with just 2 edges. It is up to the caller to decide what to do with
 * these faces.
 */
BMEdge *bmesh_jekv(BMesh *bm, BMEdge *ke, BMVert *kv, const short check_edge_double)
{
	BMEdge *oe;
	BMVert *ov, *tv;
	BMLoop *killoop, *l;
	int len, radlen = 0, halt = 0, i, valence1, valence2, edok;

	if (bmesh_vert_in_edge(ke, kv) == 0) {
		return NULL;
	}

	len = bmesh_disk_count(kv);
	
	if (len == 2) {
		oe = bmesh_disk_edge_next(ke, kv);
		tv = bmesh_edge_other_vert_get(ke, kv);
		ov = bmesh_edge_other_vert_get(oe, kv);
		halt = bmesh_verts_in_edge(kv, tv, oe); /* check for double edge */
		
		if (halt) {
			return NULL;
		}
		else {
			BMEdge *e_splice;

			/* For verification later, count valence of ov and t */
			valence1 = bmesh_disk_count(ov);
			valence2 = bmesh_disk_count(tv);

			if (check_edge_double) {
				e_splice = BM_edge_exists(tv, ov);
			}

			/* remove oe from kv's disk cycle */
			bmesh_disk_edge_remove(oe, kv);
			/* relink oe->kv to be oe->tv */
			bmesh_edge_swapverts(oe, kv, tv);
			/* append oe to tv's disk cycle */
			bmesh_disk_edge_append(oe, tv);
			/* remove ke from tv's disk cycle */
			bmesh_disk_edge_remove(ke, tv);

			/* deal with radial cycle of ke */
			radlen = bmesh_radial_length(ke->l);
			if (ke->l) {
				/* first step, fix the neighboring loops of all loops in ke's radial cycle */
				for (i = 0, killoop = ke->l; i < radlen; i++, killoop = killoop->radial_next) {
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

					/* fix len attribute of face */
					killoop->f->len--;
				}
				/* second step, remove all the hanging loops attached to ke */
				radlen = bmesh_radial_length(ke->l);

				if (LIKELY(radlen)) {
					BMLoop **loops = NULL;
					BLI_array_fixedstack_declare(loops, BM_NGON_STACK_SIZE, radlen, __func__);

					killoop = ke->l;

					/* this should be wrapped into a bme_free_radial function to be used by bmesh_KF as well... */
					for (i = 0; i < radlen; i++) {
						loops[i] = killoop;
						killoop = killoop->radial_next;
					}
					for (i = 0; i < radlen; i++) {
						bm->totloop--;
						BLI_mempool_free(bm->lpool, loops[i]);
					}
					BLI_array_fixedstack_free(loops);
				}

				/* Validate radial cycle of oe */
				edok = bmesh_radial_validate(radlen, oe->l);
				BMESH_ASSERT(edok != FALSE);
			}

			/* deallocate edg */
			bm_kill_only_edge(bm, ke);

			/* deallocate verte */
			bm_kill_only_vert(bm, kv);

			/* Validate disk cycle lengths of ov, tv are unchanged */
			edok = bmesh_disk_validate(valence1, ov->e, ov);
			BMESH_ASSERT(edok != FALSE);
			edok = bmesh_disk_validate(valence2, tv->e, tv);
			BMESH_ASSERT(edok != FALSE);

			/* Validate loop cycle of all faces attached to oe */
			for (i = 0, l = oe->l; i < radlen; i++, l = l->radial_next) {
				BMESH_ASSERT(l->e == oe);
				edok = bmesh_verts_in_edge(l->v, l->next->v, oe);
				BMESH_ASSERT(edok != FALSE);
				edok = bmesh_loop_validate(l->f);
				BMESH_ASSERT(edok != FALSE);

				BM_CHECK_ELEMENT(l);
				BM_CHECK_ELEMENT(l->v);
				BM_CHECK_ELEMENT(l->e);
				BM_CHECK_ELEMENT(l->f);
			}

			if (check_edge_double) {
				if (e_splice) {
					/* removes e_splice */
					BM_edge_splice(bm, e_splice, oe);
				}
			}

			BM_CHECK_ELEMENT(ov);
			BM_CHECK_ELEMENT(tv);
			BM_CHECK_ELEMENT(oe);

			return oe;
		}
	}
	return NULL;
}

/**
 * \brief Join Face Kill Edge (JFKE)
 *
 * Takes two faces joined by a single 2-manifold edge and fuses them together.
 * The edge shared by the faces must not be connected to any other edges which have
 * Both faces in its radial cycle
 *
 * \par Examples:
 *
 *           A                   B
 *      +--------+           +--------+
 *      |        |           |        |
 *      |   f1   |           |   f1   |
 *     v1========v2 = Ok!    v1==V2==v3 == Wrong!
 *      |   f2   |           |   f2   |
 *      |        |           |        |
 *      +--------+           +--------+
 *
 * In the example A, faces \a f1 and \a f2 are joined by a single edge,
 * and the euler can safely be used.
 * In example B however, \a f1 and \a f2 are joined by multiple edges and will produce an error.
 * The caller in this case should call #bmesh_jekv on the extra edges
 * before attempting to fuse \a f1 and \a f2.
 *
 * \note The order of arguments decides whether or not certain per-face attributes are present
 * in the resultant face. For instance vertex winding, material index, smooth flags, etc are inherited
 * from \a f1, not \a f2.
 *
 * \return A BMFace pointer
 */
BMFace *bmesh_jfke(BMesh *bm, BMFace *f1, BMFace *f2, BMEdge *e)
{
	BMLoop *l_iter, *f1loop = NULL, *f2loop = NULL;
	int newlen = 0, i, f1len = 0, f2len = 0, edok;

	/* can't join a face to itself */
	if (f1 == f2) {
		return NULL;
	}

	/* validate that edge is 2-manifold edge */
	if (!BM_edge_is_manifold(e)) {
		return NULL;
	}

	/* verify that e is in both f1 and f2 */
	f1len = f1->len;
	f2len = f2->len;

	if (!((f1loop = BM_face_edge_share_loop(f1, e)) &&
	      (f2loop = BM_face_edge_share_loop(f2, e))))
	{
		return NULL;
	}

	/* validate direction of f2's loop cycle is compatible */
	if (f1loop->v == f2loop->v) {
		return NULL;
	}

	/* validate that for each face, each vertex has another edge in its disk cycle that is
	 * not e, and not shared. */
	if (bmesh_radial_face_find(f1loop->next->e, f2) ||
	    bmesh_radial_face_find(f1loop->prev->e, f2) ||
	    bmesh_radial_face_find(f2loop->next->e, f1) ||
	    bmesh_radial_face_find(f2loop->prev->e, f1) )
	{
		return NULL;
	}

	/* validate only one shared edge */
	if (BM_face_share_edge_count(f1, f2) > 1) {
		return NULL;
	}

	/* validate no internal join */
	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f1); i < f1len; i++, l_iter = l_iter->next) {
		BM_elem_flag_disable(l_iter->v, BM_ELEM_INTERNAL_TAG);
	}
	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f2); i < f2len; i++, l_iter = l_iter->next) {
		BM_elem_flag_disable(l_iter->v, BM_ELEM_INTERNAL_TAG);
	}

	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f1); i < f1len; i++, l_iter = l_iter->next) {
		if (l_iter != f1loop) {
			BM_elem_flag_enable(l_iter->v, BM_ELEM_INTERNAL_TAG);
		}
	}
	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f2); i < f2len; i++, l_iter = l_iter->next) {
		if (l_iter != f2loop) {
			/* as soon as a duplicate is found, bail out */
			if (BM_elem_flag_test(l_iter->v, BM_ELEM_INTERNAL_TAG)) {
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

	/* make sure each loop points to the proper face */
	newlen = f1->len;
	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f1); i < newlen; i++, l_iter = l_iter->next)
		l_iter->f = f1;
	
	/* remove edge from the disk cycle of its two vertices */
	bmesh_disk_edge_remove(f1loop->e, f1loop->e->v1);
	bmesh_disk_edge_remove(f1loop->e, f1loop->e->v2);
	
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

	BM_CHECK_ELEMENT(f1);

	/* validate the new loop cycle */
	edok = bmesh_loop_validate(f1);
	BMESH_ASSERT(edok != FALSE);
	
	return f1;
}

/**
 * \brief Splice Vert
 *
 * Merges two verts into one (\a v into \a vtarget).
 *
 * \return Success
 */
int BM_vert_splice(BMesh *bm, BMVert *v, BMVert *vtarget)
{
	BMEdge *e;
	BMLoop *l;
	BMIter liter;

	/* verts already spliced */
	if (v == vtarget) {
		return FALSE;
	}

	/* retarget all the loops of v to vtarget */
	BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
		l->v = vtarget;
	}

	/* move all the edges from v's disk to vtarget's disk */
	while ((e = v->e)) {
		bmesh_disk_edge_remove(e, v);
		bmesh_edge_swapverts(e, v, vtarget);
		bmesh_disk_edge_append(e, vtarget);
	}

	BM_CHECK_ELEMENT(v);
	BM_CHECK_ELEMENT(vtarget);

	/* v is unused now, and can be killed */
	BM_vert_kill(bm, v);

	return TRUE;
}

/**
 * \brief Separate Vert
 *
 * Separates all disjoint fans that meet at a vertex, making a unique
 * vertex for each region. returns an array of all resulting vertices.
 *
 * \note this is a low level function, bm_edge_separate needs to run on edges first
 * or, the faces sharing verts must not be sharing edges for them to split at least.
 *
 * \return Success
 */
int bmesh_vert_separate(BMesh *bm, BMVert *v, BMVert ***r_vout, int *r_vout_len)
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

	visithash = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp, __func__);

	maxindex = 0;
	BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
		if (BLI_ghash_haskey(visithash, e)) {
			continue;
		}

		/* Prime the stack with this unvisited edge */
		BLI_array_append(stack, e);

		/* Considering only edges and faces incident on vertex v, walk
		 * the edges & faces and assign an index to each connected set */
		while ((e = BLI_array_pop(stack))) {
			BLI_ghash_insert(visithash, e, SET_INT_IN_POINTER(maxindex));

			BM_ITER_ELEM (l, &liter, e, BM_LOOPS_OF_EDGE) {
				nl = (l->v == v) ? l->prev : l->next;
				if (!BLI_ghash_haskey(visithash, nl->e)) {
					BLI_array_append(stack, nl->e);
				}
			}
		}

		maxindex++;
	}

	/* Make enough verts to split v for each group */
	verts = MEM_callocN(sizeof(BMVert *) * maxindex, __func__);
	verts[0] = v;
	for (i = 1; i < maxindex; i++) {
		verts[i] = BM_vert_create(bm, v->co, v);
	}

	/* Replace v with the new verts in each group */
#if 0
	BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
		/* call first since its faster then a hash lookup */
		if (l->v != v) {
			continue;
		}
		i = GET_INT_FROM_POINTER(BLI_ghash_lookup(visithash, l->e));
		if (i == 0) {
			continue;
		}

		/* Loops here should always refer to an edge that has v as an
		 * endpoint. For each appearance of this vert in a face, there
		 * will actually be two iterations: one for the loop heading
		 * towards vertex v, and another for the loop heading out from
		 * vertex v. Only need to swap the vertex on one of those times,
		 * on the outgoing loop. */

		/* XXX - because this clobbers the iterator, this *whole* block is commented, see below */
		l->v = verts[i];
	}
#else
	/* note: this is the same as the commented code above *except* that it doesn't break iterator
	 * by modifying data it loops over [#30632], this re-uses the 'stack' variable which is a bit
	 * bad practice but save alloc'ing a new array - note, the comment above is useful, keep it
	 * if you are tidying up code - campbell */
	BLI_array_empty(stack);
	BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
		if (l->v == v) {
			BLI_array_append(stack, (BMEdge *)l);
		}
	}
	while ((l = (BMLoop *)(BLI_array_pop(stack)))) {
		if ((i = GET_INT_FROM_POINTER(BLI_ghash_lookup(visithash, l->e)))) {
			l->v = verts[i];
		}
	}
#endif

	BLI_array_free(stack);

	BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
		i = GET_INT_FROM_POINTER(BLI_ghash_lookup(visithash, e));
		if (i == 0) {
			continue;
		}

		BLI_assert(e->v1 == v || e->v2 == v);
		bmesh_disk_edge_remove(e, v);
		bmesh_edge_swapverts(e, v, verts[i]);
		bmesh_disk_edge_append(e, verts[i]);
	}

	BLI_ghash_free(visithash, NULL, NULL);

	for (i = 0; i < maxindex; i++) {
		BM_CHECK_ELEMENT(verts[i]);
	}

	if (r_vout_len != NULL) {
		*r_vout_len = maxindex;
	}

	if (r_vout != NULL) {
		*r_vout = verts;
	}
	else {
		MEM_freeN(verts);
	}

	return TRUE;
}

/**
 * High level function which wraps both #bm_vert_separate and #bm_edge_separate
 */
int BM_vert_separate(BMesh *bm, BMVert *v, BMVert ***r_vout, int *r_vout_len,
                     BMEdge **e_in, int e_in_len)
{
	int i;

	for (i = 0; i < e_in_len; i++) {
		BMEdge *e = e_in[i];
		if (e->l && BM_vert_in_edge(e, v)) {
			bmesh_edge_separate(bm, e, e->l);
		}
	}

	return bmesh_vert_separate(bm, v, r_vout, r_vout_len);
}

/**
 * \brief Splice Edge
 *
 * Splice two unique edges which share the same two vertices into one edge.
 *
 * \return Success
 *
 * \note Edges must already have the same vertices.
 */
int BM_edge_splice(BMesh *bm, BMEdge *e, BMEdge *etarget)
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
		bmesh_radial_loop_remove(l, e);
		bmesh_radial_append(etarget, l);
	}

	BLI_assert(bmesh_radial_length(e->l) == 0);

	BM_CHECK_ELEMENT(e);
	BM_CHECK_ELEMENT(etarget);

	/* removes from disks too */
	BM_edge_kill(bm, e);

	return TRUE;
}

/**
 * \brief Separate Edge
 *
 * Separates a single edge into two edge: the original edge and
 * a new edge that has only \a l_sep in its radial.
 *
 * \return Success
 *
 * \note Does nothing if \a l_sep is already the only loop in the
 * edge radial.
 */
int bmesh_edge_separate(BMesh *bm, BMEdge *e, BMLoop *l_sep)
{
	BMEdge *ne;
	int radlen;

	BLI_assert(l_sep->e == e);
	BLI_assert(e->l);
	
	radlen = bmesh_radial_length(e->l);
	if (radlen < 2) {
		/* no cut required */
		return TRUE;
	}

	if (l_sep == e->l) {
		e->l = l_sep->radial_next;
	}

	ne = BM_edge_create(bm, e->v1, e->v2, e, FALSE);
	bmesh_radial_loop_remove(l_sep, e);
	bmesh_radial_append(ne, l_sep);
	l_sep->e = ne;

	BLI_assert(bmesh_radial_length(e->l) == radlen - 1);
	BLI_assert(bmesh_radial_length(ne->l) == 1);

	BM_CHECK_ELEMENT(ne);
	BM_CHECK_ELEMENT(e);

	return TRUE;
}

/**
 * \brief Unglue Region Make Vert (URMV)
 *
 * Disconnects a face from its vertex fan at loop \a sl
 *
 * \return The newly created BMVert
 */
BMVert *bmesh_urmv_loop(BMesh *bm, BMLoop *sl)
{
	BMVert **vtar;
	int len, i;
	BMVert *nv = NULL;
	BMVert *sv = sl->v;

	/* peel the face from the edge radials on both sides of the
	 * loop vert, disconnecting the face from its fan */
	bmesh_edge_separate(bm, sl->e, sl);
	bmesh_edge_separate(bm, sl->prev->e, sl->prev);

	if (bmesh_disk_count(sv) == 2) {
		/* If there are still only two edges out of sv, then
		 * this whole URMV was just a no-op, so exit now. */
		return sv;
	}

	/* Update the disk start, so that v->e points to an edge
	 * not touching the split loop. This is so that BM_vert_split
	 * will leave the original sv on some *other* fan (not the
	 * one-face fan that holds the unglue face). */
	while (sv->e == sl->e || sv->e == sl->prev->e) {
		sv->e = bmesh_disk_edge_next(sv->e, sv);
	}

	/* Split all fans connected to the vert, duplicating it for
	 * each fans. */
	bmesh_vert_separate(bm, sv, &vtar, &len);

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
				BM_vert_splice(bm, vtar[i], vtar[0]);
			}
		}
	}

	MEM_freeN(vtar);

	return nv;
}

/**
 * \brief Unglue Region Make Vert (URMV)
 *
 * Disconnects sf from the vertex fan at \a sv
 *
 * \return The newly created BMVert
 */
BMVert *bmesh_urmv(BMesh *bm, BMFace *sf, BMVert *sv)
{
	BMLoop *l = BM_face_vert_share_loop(sf, sv);
	return bmesh_urmv_loop(bm, l);
}
