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
 * Core BMesh functions for adding, removing BMesh elements.
 */

#include "MEM_guardedalloc.h"

#include "BLI_math_vector.h"
#include "BLI_array.h"
#include "BLI_alloca.h"
#include "BLI_smallhash.h"
#include "BLI_stackdefines.h"

#include "BLF_translation.h"

#include "BKE_DerivedMesh.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

/* use so valgrinds memcheck alerts us when undefined index is used.
 * TESTING ONLY! */
// #define USE_DEBUG_INDEX_MEMCHECK

#ifdef USE_DEBUG_INDEX_MEMCHECK
#define DEBUG_MEMCHECK_INDEX_INVALIDATE(ele)                                  \
	{                                                                         \
		int undef_idx;                                                        \
		BM_elem_index_set(ele, undef_idx); /* set_ok_invalid */               \
	} (void)0

#endif

/**
 * \brief Main function for creating a new vertex.
 */
BMVert *BM_vert_create(BMesh *bm, const float co[3],
                       const BMVert *v_example, const eBMCreateFlag create_flag)
{
	BMVert *v = BLI_mempool_alloc(bm->vpool);


	/* --- assign all members --- */
	v->head.data = NULL;

#ifdef USE_DEBUG_INDEX_MEMCHECK
	DEBUG_MEMCHECK_INDEX_INVALIDATE(v)
#else
	BM_elem_index_set(v, -1); /* set_ok_invalid */
#endif

	v->head.htype = BM_VERT;
	v->head.hflag = 0;
	v->head.api_flag = 0;

	/* allocate flags */
	v->oflags = bm->vtoolflagpool ? BLI_mempool_calloc(bm->vtoolflagpool) : NULL;

	/* 'v->no' is handled by BM_elem_attrs_copy */
	if (co) {
		copy_v3_v3(v->co, co);
	}
	else {
		zero_v3(v->co);
	}
	zero_v3(v->no);

	v->e = NULL;
	/* --- done --- */


	/* disallow this flag for verts - its meaningless */
	BLI_assert((create_flag & BM_CREATE_NO_DOUBLE) == 0);

	/* may add to middle of the pool */
	bm->elem_index_dirty |= BM_VERT;
	bm->elem_table_dirty |= BM_VERT;

	bm->totvert++;

	if (!(create_flag & BM_CREATE_SKIP_CD)) {
		if (v_example) {
			int *keyi;

			BM_elem_attrs_copy(bm, bm, v_example, v);

			/* exception: don't copy the original shapekey index */
			keyi = CustomData_bmesh_get(&bm->vdata, v->head.data, CD_SHAPE_KEYINDEX);
			if (keyi) {
				*keyi = ORIGINDEX_NONE;
			}
		}
		else {
			CustomData_bmesh_set_default(&bm->vdata, &v->head.data);
		}
	}

	BM_CHECK_ELEMENT(v);

	return v;
}

/**
 * \brief Main function for creating a new edge.
 *
 * \note Duplicate edges are supported by the API however users should _never_ see them.
 * so unless you need a unique edge or know the edge won't exist, you should call with \a no_double = true
 */
BMEdge *BM_edge_create(BMesh *bm, BMVert *v1, BMVert *v2,
                       const BMEdge *e_example, const eBMCreateFlag create_flag)
{
	BMEdge *e;

	BLI_assert(v1 != v2);
	BLI_assert(v1->head.htype == BM_VERT && v2->head.htype == BM_VERT);

	if ((create_flag & BM_CREATE_NO_DOUBLE) && (e = BM_edge_exists(v1, v2)))
		return e;
	
	e = BLI_mempool_alloc(bm->epool);


	/* --- assign all members --- */
	e->head.data = NULL;

#ifdef USE_DEBUG_INDEX_MEMCHECK
	DEBUG_MEMCHECK_INDEX_INVALIDATE(e)
#else
	BM_elem_index_set(e, -1); /* set_ok_invalid */
#endif

	e->head.htype = BM_EDGE;
	e->head.hflag = BM_ELEM_SMOOTH | BM_ELEM_DRAW;
	e->head.api_flag = 0;

	/* allocate flags */
	e->oflags = bm->etoolflagpool ? BLI_mempool_calloc(bm->etoolflagpool) : NULL;

	e->v1 = v1;
	e->v2 = v2;
	e->l = NULL;

	memset(&e->v1_disk_link, 0, sizeof(BMDiskLink) * 2);
	/* --- done --- */


	bmesh_disk_edge_append(e, e->v1);
	bmesh_disk_edge_append(e, e->v2);

	/* may add to middle of the pool */
	bm->elem_index_dirty |= BM_EDGE;
	bm->elem_table_dirty |= BM_EDGE;

	bm->totedge++;

	if (!(create_flag & BM_CREATE_SKIP_CD)) {
		if (e_example) {
			BM_elem_attrs_copy(bm, bm, e_example, e);
		}
		else {
			CustomData_bmesh_set_default(&bm->edata, &e->head.data);
		}
	}

	BM_CHECK_ELEMENT(e);

	return e;
}

static BMLoop *bm_loop_create(BMesh *bm, BMVert *v, BMEdge *e, BMFace *f,
                              const BMLoop *example, const eBMCreateFlag create_flag)
{
	BMLoop *l = NULL;

	l = BLI_mempool_alloc(bm->lpool);

	/* --- assign all members --- */
	l->head.data = NULL;

#ifdef USE_DEBUG_INDEX_MEMCHECK
	DEBUG_MEMCHECK_INDEX_INVALIDATE(l)
#else
	BM_elem_index_set(l, -1); /* set_ok_invalid */
#endif

	l->head.hflag = 0;
	l->head.htype = BM_LOOP;
	l->head.api_flag = 0;

	l->v = v;
	l->e = e;
	l->f = f;

	l->radial_next = NULL;
	l->radial_prev = NULL;
	l->next = NULL;
	l->prev = NULL;
	/* --- done --- */

	/* may add to middle of the pool */
	bm->elem_index_dirty |= BM_LOOP;

	bm->totloop++;

	if (!(create_flag & BM_CREATE_SKIP_CD)) {
		if (example) {
			CustomData_bmesh_copy_data(&bm->ldata, &bm->ldata, example->head.data, &l->head.data);
		}
		else {
			CustomData_bmesh_set_default(&bm->ldata, &l->head.data);
		}
	}

	return l;
}

static BMLoop *bm_face_boundary_add(BMesh *bm, BMFace *f, BMVert *startv, BMEdge *starte,
                                    const eBMCreateFlag create_flag)
{
#ifdef USE_BMESH_HOLES
	BMLoopList *lst = BLI_mempool_calloc(bm->looplistpool);
#endif
	BMLoop *l = bm_loop_create(bm, startv, starte, f, starte->l, create_flag);
	
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

BMFace *BM_face_copy(BMesh *bm_dst, BMesh *bm_src, BMFace *f,
                     const bool copy_verts, const bool copy_edges)
{
	BMVert **verts = BLI_array_alloca(verts, f->len);
	BMEdge **edges = BLI_array_alloca(edges, f->len);
	BMLoop *l_iter;
	BMLoop *l_first;
	BMLoop *l_copy;
	BMFace *f_copy;
	int i;

	BLI_assert((bm_dst == bm_src) || (copy_verts && copy_edges));

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	i = 0;
	do {
		if (copy_verts) {
			verts[i] = BM_vert_create(bm_dst, l_iter->v->co, l_iter->v, BM_CREATE_NOP);
		}
		else {
			verts[i] = l_iter->v;
		}
		i++;
	} while ((l_iter = l_iter->next) != l_first);

	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	i = 0;
	do {
		if (copy_edges) {
			BMVert *v1, *v2;
			
			if (l_iter->e->v1 == verts[i]) {
				v1 = verts[i];
				v2 = verts[(i + 1) % f->len];
			}
			else {
				v2 = verts[i];
				v1 = verts[(i + 1) % f->len];
			}
			
			edges[i] = BM_edge_create(bm_dst, v1, v2, l_iter->e, BM_CREATE_NOP);
		}
		else {
			edges[i] = l_iter->e;
		}
		i++;
	} while ((l_iter = l_iter->next) != l_first);
	
	f_copy = BM_face_create(bm_dst, verts, edges, f->len, NULL, BM_CREATE_SKIP_CD);
	
	BM_elem_attrs_copy(bm_src, bm_dst, f, f_copy);
	
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	l_copy = BM_FACE_FIRST_LOOP(f_copy);
	do {
		BM_elem_attrs_copy(bm_src, bm_dst, l_iter, l_copy);
		l_copy = l_copy->next;
	} while ((l_iter = l_iter->next) != l_first);

	return f_copy;
}

/**
 * only create the face, since this calloc's the length is initialized to 0,
 * leave adding loops to the caller.
 *
 * \note, caller needs to handle customdata.
 */
BLI_INLINE BMFace *bm_face_create__internal(BMesh *bm)
{
	BMFace *f;

	f = BLI_mempool_alloc(bm->fpool);


	/* --- assign all members --- */
	f->head.data = NULL;
#ifdef USE_DEBUG_INDEX_MEMCHECK
	DEBUG_MEMCHECK_INDEX_INVALIDATE(f)
#else
	BM_elem_index_set(f, -1); /* set_ok_invalid */
#endif

	f->head.htype = BM_FACE;
	f->head.hflag = 0;
	f->head.api_flag = 0;

	/* allocate flags */
	f->oflags = bm->ftoolflagpool ? BLI_mempool_calloc(bm->ftoolflagpool) : NULL;

#ifdef USE_BMESH_HOLES
	BLI_listbase_clear(&f->loops);
#else
	f->l_first = NULL;
#endif
	f->len = 0;
	zero_v3(f->no);
	f->mat_nr = 0;
	/* --- done --- */


	/* may add to middle of the pool */
	bm->elem_index_dirty |= BM_FACE;
	bm->elem_table_dirty |= BM_FACE;

	bm->totface++;

#ifdef USE_BMESH_HOLES
	f->totbounds = 0;
#endif

	return f;
}

/**
 * Main face creation function
 *
 * \param bm  The mesh
 * \param verts  A sorted array of verts size of len
 * \param edges  A sorted array of edges size of len
 * \param len  Length of the face
 * \param create_flag  Options for creating the face
 */
BMFace *BM_face_create(BMesh *bm, BMVert **verts, BMEdge **edges, const int len,
                       const BMFace *f_example, const eBMCreateFlag create_flag)
{
	BMFace *f = NULL;
	BMLoop *l, *startl, *lastl;
	int i;
	
	if (len == 0) {
		/* just return NULL for now */
		return NULL;
	}

	if (create_flag & BM_CREATE_NO_DOUBLE) {
		/* Check if face already exists */
		const bool is_overlap = BM_face_exists(verts, len, &f);
		if (is_overlap) {
			return f;
		}
		else {
			BLI_assert(f == NULL);
		}
	}

	f = bm_face_create__internal(bm);

	startl = lastl = bm_face_boundary_add(bm, f, verts[0], edges[0], create_flag);
	
	startl->v = verts[0];
	startl->e = edges[0];
	for (i = 1; i < len; i++) {
		l = bm_loop_create(bm, verts[i], edges[i], f, edges[i]->l, create_flag);
		
		l->f = f;
		bmesh_radial_append(edges[i], l);

		l->prev = lastl;
		lastl->next = l;
		lastl = l;
	}
	
	startl->prev = lastl;
	lastl->next = startl;
	
	f->len = len;
	
	if (!(create_flag & BM_CREATE_SKIP_CD)) {
		if (f_example) {
			BM_elem_attrs_copy(bm, bm, f_example, f);
		}
		else {
			CustomData_bmesh_set_default(&bm->pdata, &f->head.data);
		}
	}

	BM_CHECK_ELEMENT(f);

	return f;
}

/**
 * Wrapper for #BM_face_create when you don't have an edge array
 */
BMFace *BM_face_create_verts(BMesh *bm, BMVert **vert_arr, const int len,
                             const BMFace *f_example, const eBMCreateFlag create_flag, const bool create_edges)
{
	BMEdge **edge_arr = BLI_array_alloca(edge_arr, len);
	int i, i_prev = len - 1;

	if (create_edges) {
		for (i = 0; i < len; i++) {
			edge_arr[i_prev] = BM_edge_create(bm, vert_arr[i_prev], vert_arr[i], NULL, BM_CREATE_NO_DOUBLE);
			i_prev = i;
		}
	}
	else {
		for (i = 0; i < len; i++) {
			edge_arr[i_prev] = BM_edge_exists(vert_arr[i_prev], vert_arr[i]);
			if (edge_arr[i_prev] == NULL) {
				return NULL;
			}
			i_prev = i;
		}
	}

	return BM_face_create(bm, vert_arr, edge_arr, len, f_example, create_flag);
}

#ifndef NDEBUG

/**
 * Check the element is valid.
 *
 * BMESH_TODO, when this raises an error the output is incredible confusing.
 * need to have some nice way to print/debug what the hecks going on.
 */
int bmesh_elem_check(void *element, const char htype)
{
	BMHeader *head = element;
	int err = 0;

	if (!element)
		return 1;

	if (head->htype != htype)
		return 2;
	
	switch (htype) {
		case BM_VERT:
		{
			BMVert *v = element;
			if (v->e && v->e->head.htype != BM_EDGE) {
				err |= 4;
			}
			break;
		}
		case BM_EDGE:
		{
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
		case BM_LOOP:
		{
			BMLoop *l = element, *l2;
			int i;

			if (l->f->head.htype != BM_FACE)
				err |= 256;
			if (l->e->head.htype != BM_EDGE)
				err |= 512;
			if (l->v->head.htype != BM_VERT)
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
		case BM_FACE:
		{
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
			break;
		}
		default:
			BLI_assert(0);
			break;
	}

	BMESH_ASSERT(err == 0);

	return err;
}

#endif /* NDEBUG */

/**
 * low level function, only frees the vert,
 * doesn't change or adjust surrounding geometry
 */
static void bm_kill_only_vert(BMesh *bm, BMVert *v)
{
	bm->totvert--;
	bm->elem_index_dirty |= BM_VERT;
	bm->elem_table_dirty |= BM_VERT;

	BM_select_history_remove(bm, v);

	if (v->head.data)
		CustomData_bmesh_free_block(&bm->vdata, &v->head.data);

	if (bm->vtoolflagpool) {
		BLI_mempool_free(bm->vtoolflagpool, v->oflags);
	}
	BLI_mempool_free(bm->vpool, v);
}

/**
 * low level function, only frees the edge,
 * doesn't change or adjust surrounding geometry
 */
static void bm_kill_only_edge(BMesh *bm, BMEdge *e)
{
	bm->totedge--;
	bm->elem_index_dirty |= BM_EDGE;
	bm->elem_table_dirty |= BM_EDGE;

	BM_select_history_remove(bm, (BMElem *)e);

	if (e->head.data)
		CustomData_bmesh_free_block(&bm->edata, &e->head.data);

	if (bm->etoolflagpool) {
		BLI_mempool_free(bm->etoolflagpool, e->oflags);
	}
	BLI_mempool_free(bm->epool, e);
}

/**
 * low level function, only frees the face,
 * doesn't change or adjust surrounding geometry
 */
static void bm_kill_only_face(BMesh *bm, BMFace *f)
{
	if (bm->act_face == f)
		bm->act_face = NULL;

	bm->totface--;
	bm->elem_index_dirty |= BM_FACE;
	bm->elem_table_dirty |= BM_FACE;

	BM_select_history_remove(bm, (BMElem *)f);

	if (f->head.data)
		CustomData_bmesh_free_block(&bm->pdata, &f->head.data);

	if (bm->ftoolflagpool) {
		BLI_mempool_free(bm->ftoolflagpool, f->oflags);
	}
	BLI_mempool_free(bm->fpool, f);
}

/**
 * low level function, only frees the loop,
 * doesn't change or adjust surrounding geometry
 */
static void bm_kill_only_loop(BMesh *bm, BMLoop *l)
{
	bm->totloop--;
	bm->elem_index_dirty |= BM_LOOP;
	if (l->head.data)
		CustomData_bmesh_free_block(&bm->ldata, &l->head.data);

	BLI_mempool_free(bm->lpool, l);
}

/**
 * kills all edges associated with \a f, along with any other faces containing
 * those edges
 */
void BM_face_edges_kill(BMesh *bm, BMFace *f)
{
	BMEdge **edges = BLI_array_alloca(edges, f->len);
	BMLoop *l_iter;
	BMLoop *l_first;
	int i = 0;
	
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		edges[i++] = l_iter->e;
	} while ((l_iter = l_iter->next) != l_first);
	
	for (i = 0; i < f->len; i++) {
		BM_edge_kill(bm, edges[i]);
	}
}

/**
 * kills all verts associated with \a f, along with any other faces containing
 * those vertices
 */
void BM_face_verts_kill(BMesh *bm, BMFace *f)
{
	BMVert **verts = BLI_array_alloca(verts, f->len);
	BMLoop *l_iter;
	BMLoop *l_first;
	int i = 0;
	
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		verts[i++] = l_iter->v;
	} while ((l_iter = l_iter->next) != l_first);
	
	for (i = 0; i < f->len; i++) {
		BM_vert_kill(bm, verts[i]);
	}
}

/**
 * Kills \a f and its loops.
 */
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
/**
 * kills \a e and all faces that use it.
 */
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

/**
 * kills \a v and all edges that use it.
 */
void BM_vert_kill(BMesh *bm, BMVert *v)
{
	if (v->e) {
		BMEdge *e, *e_next;
		
		e = v->e;
		while (v->e) {
			e_next = bmesh_disk_edge_next(e, v);
			BM_edge_kill(bm, e);
			e = e_next;
		}
	}

	bm_kill_only_vert(bm, v);
}

/********** private disk and radial cycle functions ********** */

/**
 * return the length of the face, should always equal \a l->f->len
 */
static int UNUSED_FUNCTION(bm_loop_length)(BMLoop *l)
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
static bool bm_loop_reverse_loop(BMesh *bm, BMFace *f
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

	const int len = f->len;
	const bool do_disps = CustomData_has_layer(&bm->ldata, CD_MDISPS);
	BMLoop *l_iter, *oldprev, *oldnext;
	BMEdge **edar = BLI_array_alloca(edar, len);
	int i, j, edok;

	for (i = 0, l_iter = l_first; i < len; i++, l_iter = l_iter->next) {
		bmesh_radial_loop_remove(l_iter, (edar[i] = l_iter->e));
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
			MDisps *md;
			
			md = CustomData_bmesh_get(&bm->ldata, l_iter->head.data, CD_MDISPS);
			if (!md->totdisp || !md->disps)
				continue;

			sides = (int)sqrt(md->totdisp);
			co = md->disps;
			
			for (x = 0; x < sides; x++) {
				for (y = 0; y < x; y++) {
					swap_v3_v3(co[y * sides + x], co[sides * x + y]);
					SWAP(float, co[y * sides + x][0], co[y * sides + x][1]);
					SWAP(float, co[x * sides + y][0], co[x * sides + y][1]);
				}
				SWAP(float, co[x * sides + x][0], co[x * sides + x][1]);
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
				edok = BM_verts_in_edge(l_iter->v, l_iter->next->v, edar[j]);
				if (edok) {
					l_iter->e = edar[j];
					break;
				}
			}
		}
	}
	/* rebuild radial */
	for (i = 0, l_iter = l_first; i < len; i++, l_iter = l_iter->next)
		bmesh_radial_append(l_iter->e, l_iter);

	/* validate radial */
	for (i = 0, l_iter = l_first; i < len; i++, l_iter = l_iter->next) {
		BM_CHECK_ELEMENT(l_iter);
		BM_CHECK_ELEMENT(l_iter->e);
		BM_CHECK_ELEMENT(l_iter->v);
		BM_CHECK_ELEMENT(l_iter->f);
	}

	BM_CHECK_ELEMENT(f);

	/* Loop indices are no more valid! */
	bm->elem_index_dirty |= BM_LOOP;

	return true;
}

/**
 * \brief Flip the faces direction
 */
bool bmesh_loop_reverse(BMesh *bm, BMFace *f)
{
#ifdef USE_BMESH_HOLES
	return bm_loop_reverse_loop(bm, f, f->loops.first);
#else
	return bm_loop_reverse_loop(bm, f);
#endif
}

static void bm_elements_systag_enable(void *veles, int tot, const char api_flag)
{
	BMHeader **eles = veles;
	int i;

	for (i = 0; i < tot; i++) {
		BM_ELEM_API_FLAG_ENABLE((BMElemF *)eles[i], api_flag);
	}
}

static void bm_elements_systag_disable(void *veles, int tot, const char api_flag)
{
	BMHeader **eles = veles;
	int i;

	for (i = 0; i < tot; i++) {
		BM_ELEM_API_FLAG_DISABLE((BMElemF *)eles[i], api_flag);
	}
}

static int bm_loop_systag_count_radial(BMLoop *l, const char api_flag)
{
	BMLoop *l_iter = l;
	int i = 0;
	do {
		i += BM_ELEM_API_FLAG_TEST(l_iter->f, api_flag) ? 1 : 0;
	} while ((l_iter = l_iter->radial_next) != l);

	return i;
}

static int UNUSED_FUNCTION(bm_vert_systag_count_disk)(BMVert *v, const char api_flag)
{
	BMEdge *e = v->e;
	int i = 0;

	if (!e)
		return 0;

	do {
		i += BM_ELEM_API_FLAG_TEST(e, api_flag) ? 1 : 0;
	} while ((e = bmesh_disk_edge_next(e, v)) != v->e);

	return i;
}

static bool disk_is_flagged(BMVert *v, const char api_flag)
{
	BMEdge *e = v->e;

	if (!e)
		return false;

	do {
		BMLoop *l = e->l;

		if (!l) {
			return false;
		}
		
		if (bmesh_radial_length(l) == 1)
			return false;
		
		do {
			if (!BM_ELEM_API_FLAG_TEST(l->f, api_flag))
				return false;
		} while ((l = l->radial_next) != e->l);
	} while ((e = bmesh_disk_edge_next(e, v)) != v->e);

	return true;
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
BMFace *BM_faces_join(BMesh *bm, BMFace **faces, int totface, const bool do_del)
{
	BMFace *f, *f_new;
#ifdef USE_BMESH_HOLES
	BMLoopList *lst;
	ListBase holes = {NULL, NULL};
#endif
	BMLoop *l_iter;
	BMLoop *l_first;
	BMEdge **edges = NULL;
	BMEdge **deledges = NULL;
	BMVert **delverts = NULL;
	BLI_array_staticdeclare(edges,    BM_DEFAULT_NGON_STACK_SIZE);
	BLI_array_staticdeclare(deledges, BM_DEFAULT_NGON_STACK_SIZE);
	BLI_array_staticdeclare(delverts, BM_DEFAULT_NGON_STACK_SIZE);
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
			int rlen = bm_loop_systag_count_radial(l_iter, _FLAG_JF);

			if (rlen > 2) {
				err = N_("Input faces do not form a contiguous manifold region");
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
	f_new = tote ? BM_face_create_ngon(bm, v1, v2, edges, tote, faces[0], BM_CREATE_NOP) : NULL;
	if (UNLIKELY(!f_new || BMO_error_occurred(bm))) {
		if (!BMO_error_occurred(bm))
			err = N_("Invalid boundary region to join faces");
		goto error;
	}

	/* copy over loop data */
	l_iter = l_first = BM_FACE_FIRST_LOOP(f_new);
	do {
		BMLoop *l2 = l_iter->radial_next;

		do {
			if (BM_ELEM_API_FLAG_TEST(l2->f, _FLAG_JF))
				break;
			l2 = l2->radial_next;
		} while (l2 != l_iter);

		if (l2 != l_iter) {
			/* I think this is correct? */
			if (l2->v != l_iter->v) {
				l2 = l2->next;
			}

			BM_elem_attrs_copy(bm, bm, l2, l_iter);
		}
	} while ((l_iter = l_iter->next) != l_first);

#ifdef USE_BMESH_HOLES
	/* add holes */
	BLI_movelisttolist(&f_new->loops, &holes);
#endif

	/* update loop face pointer */
#ifdef USE_BMESH_HOLES
	for (lst = f_new->loops.first; lst; lst = lst->next)
#endif
	{
#ifdef USE_BMESH_HOLES
		l_iter = l_first = lst->first;
#else
		l_iter = l_first = BM_FACE_FIRST_LOOP(f_new);
#endif
		do {
			l_iter->f = f_new;
		} while ((l_iter = l_iter->next) != l_first);
	}

	bm_elements_systag_disable(faces, totface, _FLAG_JF);
	BM_ELEM_API_FLAG_DISABLE(f_new, _FLAG_JF);

	/* handle multi-res data */
	if (CustomData_has_layer(&bm->ldata, CD_MDISPS)) {
		l_iter = l_first = BM_FACE_FIRST_LOOP(f_new);
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

	BM_CHECK_ELEMENT(f_new);
	return f_new;

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

static BMFace *bm_face_create__sfme(BMesh *bm, BMFace *f_example)
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

	BM_elem_attrs_copy(bm, bm, f_example, f);

	return f;
}

/**
 * \brief Split Face Make Edge (SFME)
 *
 * \warning this is a low level function, most likely you want to use #BM_face_split()
 *
 * Takes as input two vertices in a single face. An edge is created which divides the original face
 * into two distinct regions. One of the regions is assigned to the original face and it is closed off.
 * The second region has a new face assigned to it.
 *
 * \par Examples:
 * <pre>
 *     Before:               After:
 *      +--------+           +--------+
 *      |        |           |        |
 *      |        |           |   f1   |
 *     v1   f1   v2          v1======v2
 *      |        |           |   f2   |
 *      |        |           |        |
 *      +--------+           +--------+
 * </pre>
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
BMFace *bmesh_sfme(BMesh *bm, BMFace *f, BMLoop *l_v1, BMLoop *l_v2,
                   BMLoop **r_l,
#ifdef USE_BMESH_HOLES
                   ListBase *holes,
#endif
                   BMEdge *example,
                   const bool no_double
                   )
{
#ifdef USE_BMESH_HOLES
	BMLoopList *lst, *lst2;
#else
	int first_loop_f1;
#endif

	BMFace *f2;
	BMLoop *l_iter, *l_first;
	BMLoop *l_f1 = NULL, *l_f2 = NULL;
	BMEdge *e;
	BMVert *v1 = l_v1->v, *v2 = l_v2->v;
	int f1len, f2len;

	BLI_assert(f == l_v1->f && f == l_v2->f);

	/* allocate new edge between v1 and v2 */
	e = BM_edge_create(bm, v1, v2, example, no_double ? BM_CREATE_NO_DOUBLE : BM_CREATE_NOP);

	f2 = bm_face_create__sfme(bm, f);
	l_f1 = bm_loop_create(bm, v2, e, f, l_v2, 0);
	l_f2 = bm_loop_create(bm, v1, e, f2, l_v1, 0);

	l_f1->prev = l_v2->prev;
	l_f2->prev = l_v1->prev;
	l_v2->prev->next = l_f1;
	l_v1->prev->next = l_f2;

	l_f1->next = l_v1;
	l_f2->next = l_v2;
	l_v1->prev = l_f1;
	l_v2->prev = l_f2;

#ifdef USE_BMESH_HOLES
	lst = f->loops.first;
	lst2 = f2->loops.first;

	lst2->first = lst2->last = l_f2;
	lst->first = lst->last = l_f1;
#else
	/* find which of the faces the original first loop is in */
	l_iter = l_first = l_f1;
	first_loop_f1 = 0;
	do {
		if (l_iter == f->l_first)
			first_loop_f1 = 1;
	} while ((l_iter = l_iter->next) != l_first);

	if (first_loop_f1) {
		/* original first loop was in f1, find a suitable first loop for f2
		 * which is as similar as possible to f1. the order matters for tools
		 * such as duplifaces. */
		if (f->l_first->prev == l_f1)
			f2->l_first = l_f2->prev;
		else if (f->l_first->next == l_f1)
			f2->l_first = l_f2->next;
		else
			f2->l_first = l_f2;
	}
	else {
		/* original first loop was in f2, further do same as above */
		f2->l_first = f->l_first;

		if (f->l_first->prev == l_f2)
			f->l_first = l_f1->prev;
		else if (f->l_first->next == l_f2)
			f->l_first = l_f1->next;
		else
			f->l_first = l_f1;
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
	bmesh_radial_append(e, l_f1);
	bmesh_radial_append(e, l_f2);

	f2->len = f2len;

	f1len = 0;
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		f1len++;
	} while ((l_iter = l_iter->next) != l_first);

	f->len = f1len;

	if (r_l) *r_l = l_f2;

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
 * <pre>
 *                     E
 *     Before: OV-------------TV
 *                 E       RE
 *     After:  OV------NV-----TV
 * </pre>
 *
 * \return The newly created BMVert pointer.
 */
BMVert *bmesh_semv(BMesh *bm, BMVert *tv, BMEdge *e, BMEdge **r_e)
{
	BMLoop *l_next;
	BMEdge *e_new;
	BMVert *v_new, *v_old;
#ifndef NDEBUG
	int valence1, valence2;
	bool edok;
	int i;
#endif

	BLI_assert(BM_vert_in_edge(e, tv) != false);

	v_old = BM_edge_other_vert(e, tv);

#ifndef NDEBUG
	valence1 = bmesh_disk_count(v_old);
	valence2 = bmesh_disk_count(tv);
#endif

	v_new = BM_vert_create(bm, tv->co, tv, BM_CREATE_NOP);
	e_new = BM_edge_create(bm, v_new, tv, e, BM_CREATE_NOP);

	bmesh_disk_edge_remove(e_new, tv);
	bmesh_disk_edge_remove(e_new, v_new);

	/* remove e from tv's disk cycle */
	bmesh_disk_edge_remove(e, tv);

	/* swap out tv for v_new in e */
	bmesh_edge_swapverts(e, tv, v_new);

	/* add e to v_new's disk cycle */
	bmesh_disk_edge_append(e, v_new);

	/* add e_new to v_new's disk cycle */
	bmesh_disk_edge_append(e_new, v_new);

	/* add e_new to tv's disk cycle */
	bmesh_disk_edge_append(e_new, tv);

#ifndef NDEBUG
	/* verify disk cycles */
	edok = bmesh_disk_validate(valence1, v_old->e, v_old);
	BMESH_ASSERT(edok != false);
	edok = bmesh_disk_validate(valence2, tv->e, tv);
	BMESH_ASSERT(edok != false);
	edok = bmesh_disk_validate(2, v_new->e, v_new);
	BMESH_ASSERT(edok != false);
#endif

	/* Split the radial cycle if present */
	l_next = e->l;
	e->l = NULL;
	if (l_next) {
		BMLoop *l_new, *l;
#ifndef NDEBUG
		int radlen = bmesh_radial_length(l_next);
#endif
		int first1 = 0, first2 = 0;

		/* Take the next loop. Remove it from radial. Split it. Append to appropriate radials */
		while (l_next) {
			l = l_next;
			l->f->len++;
			l_next = l_next != l_next->radial_next ? l_next->radial_next : NULL;
			bmesh_radial_loop_remove(l, NULL);

			l_new = bm_loop_create(bm, NULL, NULL, l->f, l, 0);
			l_new->prev = l;
			l_new->next = (l->next);
			l_new->prev->next = l_new;
			l_new->next->prev = l_new;
			l_new->v = v_new;

			/* assign the correct edge to the correct loop */
			if (BM_verts_in_edge(l_new->v, l_new->next->v, e)) {
				l_new->e = e;
				l->e = e_new;

				/* append l into e_new's rad cycle */
				if (!first1) {
					first1 = 1;
					l->radial_next = l->radial_prev = NULL;
				}

				if (!first2) {
					first2 = 1;
					l->radial_next = l->radial_prev = NULL;
				}
				
				bmesh_radial_append(l_new->e, l_new);
				bmesh_radial_append(l->e, l);
			}
			else if (BM_verts_in_edge(l_new->v, l_new->next->v, e_new)) {
				l_new->e = e_new;
				l->e = e;

				/* append l into e_new's rad cycle */
				if (!first1) {
					first1 = 1;
					l->radial_next = l->radial_prev = NULL;
				}

				if (!first2) {
					first2 = 1;
					l->radial_next = l->radial_prev = NULL;
				}

				bmesh_radial_append(l_new->e, l_new);
				bmesh_radial_append(l->e, l);
			}

		}

#ifndef NDEBUG
		/* verify length of radial cycle */
		edok = bmesh_radial_validate(radlen, e->l);
		BMESH_ASSERT(edok != false);
		edok = bmesh_radial_validate(radlen, e_new->l);
		BMESH_ASSERT(edok != false);

		/* verify loop->v and loop->next->v pointers for e */
		for (i = 0, l = e->l; i < radlen; i++, l = l->radial_next) {
			BMESH_ASSERT(l->e == e);
			//BMESH_ASSERT(l->radial_next == l);
			BMESH_ASSERT(!(l->prev->e != e_new && l->next->e != e_new));

			edok = BM_verts_in_edge(l->v, l->next->v, e);
			BMESH_ASSERT(edok != false);
			BMESH_ASSERT(l->v != l->next->v);
			BMESH_ASSERT(l->e != l->next->e);

			/* verify loop cycle for kloop->f */
			BM_CHECK_ELEMENT(l);
			BM_CHECK_ELEMENT(l->v);
			BM_CHECK_ELEMENT(l->e);
			BM_CHECK_ELEMENT(l->f);
		}
		/* verify loop->v and loop->next->v pointers for e_new */
		for (i = 0, l = e_new->l; i < radlen; i++, l = l->radial_next) {
			BMESH_ASSERT(l->e == e_new);
			// BMESH_ASSERT(l->radial_next == l);
			BMESH_ASSERT(!(l->prev->e != e && l->next->e != e));
			edok = BM_verts_in_edge(l->v, l->next->v, e_new);
			BMESH_ASSERT(edok != false);
			BMESH_ASSERT(l->v != l->next->v);
			BMESH_ASSERT(l->e != l->next->e);

			BM_CHECK_ELEMENT(l);
			BM_CHECK_ELEMENT(l->v);
			BM_CHECK_ELEMENT(l->e);
			BM_CHECK_ELEMENT(l->f);
		}
#endif
	}

	BM_CHECK_ELEMENT(e_new);
	BM_CHECK_ELEMENT(v_new);
	BM_CHECK_ELEMENT(v_old);
	BM_CHECK_ELEMENT(e);
	BM_CHECK_ELEMENT(tv);

	if (r_e) *r_e = e_new;
	return v_new;
}

/**
 * \brief Join Edge Kill Vert (JEKV)
 *
 * Takes an edge \a e_kill and pointer to one of its vertices \a v_kill
 * and collapses the edge on that vertex.
 *
 * \par Examples:
 *
 * <pre>
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
 * </pre>
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
BMEdge *bmesh_jekv(BMesh *bm, BMEdge *e_kill, BMVert *v_kill,
                   const bool do_del, const bool check_edge_double)
{
	BMEdge *e_old;
	BMVert *v_old, *tv;
	BMLoop *l_kill;
	int len, radlen = 0, i;
	bool halt = false;
#ifndef NDEBUG
	bool edok;
#endif

	BLI_assert(BM_vert_in_edge(e_kill, v_kill));

	if (BM_vert_in_edge(e_kill, v_kill) == 0) {
		return NULL;
	}

	len = bmesh_disk_count(v_kill);
	
	if (len == 2) {
#ifndef NDEBUG
		int valence1, valence2;
		BMLoop *l;
#endif

		e_old = bmesh_disk_edge_next(e_kill, v_kill);
		tv = BM_edge_other_vert(e_kill, v_kill);
		v_old = BM_edge_other_vert(e_old, v_kill);
		halt = BM_verts_in_edge(v_kill, tv, e_old); /* check for double edges */
		
		if (halt) {
			return NULL;
		}
		else {
			BMEdge *e_splice;

#ifndef NDEBUG
			/* For verification later, count valence of v_old and tv */
			valence1 = bmesh_disk_count(v_old);
			valence2 = bmesh_disk_count(tv);
#endif

			if (check_edge_double) {
				e_splice = BM_edge_exists(tv, v_old);
			}

			/* remove e_old from v_kill's disk cycle */
			bmesh_disk_edge_remove(e_old, v_kill);
			/* relink e_old->v_kill to be e_old->tv */
			bmesh_edge_swapverts(e_old, v_kill, tv);
			/* append e_old to tv's disk cycle */
			bmesh_disk_edge_append(e_old, tv);
			/* remove e_kill from tv's disk cycle */
			bmesh_disk_edge_remove(e_kill, tv);

			/* deal with radial cycle of e_kill */
			radlen = bmesh_radial_length(e_kill->l);
			if (e_kill->l) {
				/* first step, fix the neighboring loops of all loops in e_kill's radial cycle */
				for (i = 0, l_kill = e_kill->l; i < radlen; i++, l_kill = l_kill->radial_next) {
					/* relink loops and fix vertex pointer */
					if (l_kill->next->v == v_kill) {
						l_kill->next->v = tv;
					}

					l_kill->next->prev = l_kill->prev;
					l_kill->prev->next = l_kill->next;
					if (BM_FACE_FIRST_LOOP(l_kill->f) == l_kill) {
						BM_FACE_FIRST_LOOP(l_kill->f) = l_kill->next;
					}
					l_kill->next = NULL;
					l_kill->prev = NULL;

					/* fix len attribute of face */
					l_kill->f->len--;
				}
				/* second step, remove all the hanging loops attached to e_kill */
				radlen = bmesh_radial_length(e_kill->l);

				if (LIKELY(radlen)) {
					BMLoop **loops = BLI_array_alloca(loops, radlen);

					l_kill = e_kill->l;

					/* this should be wrapped into a bme_free_radial function to be used by bmesh_KF as well... */
					for (i = 0; i < radlen; i++) {
						loops[i] = l_kill;
						l_kill = l_kill->radial_next;
					}
					for (i = 0; i < radlen; i++) {
						bm->totloop--;
						BLI_mempool_free(bm->lpool, loops[i]);
					}
				}
#ifndef NDEBUG
				/* Validate radial cycle of e_old */
				edok = bmesh_radial_validate(radlen, e_old->l);
				BMESH_ASSERT(edok != false);
#endif
			}
			/* deallocate edge */
			bm_kill_only_edge(bm, e_kill);

			/* deallocate vertex */
			if (do_del) {
				bm_kill_only_vert(bm, v_kill);
			}
			else {
				v_kill->e = NULL;
			}

#ifndef NDEBUG
			/* Validate disk cycle lengths of v_old, tv are unchanged */
			edok = bmesh_disk_validate(valence1, v_old->e, v_old);
			BMESH_ASSERT(edok != false);
			edok = bmesh_disk_validate(valence2, tv->e, tv);
			BMESH_ASSERT(edok != false);

			/* Validate loop cycle of all faces attached to 'e_old' */
			for (i = 0, l = e_old->l; i < radlen; i++, l = l->radial_next) {
				BMESH_ASSERT(l->e == e_old);
				edok = BM_verts_in_edge(l->v, l->next->v, e_old);
				BMESH_ASSERT(edok != false);
				edok = bmesh_loop_validate(l->f);
				BMESH_ASSERT(edok != false);

				BM_CHECK_ELEMENT(l);
				BM_CHECK_ELEMENT(l->v);
				BM_CHECK_ELEMENT(l->e);
				BM_CHECK_ELEMENT(l->f);
			}
#endif

			if (check_edge_double) {
				if (e_splice) {
					/* removes e_splice */
					BM_edge_splice(bm, e_splice, e_old);
				}
			}

			BM_CHECK_ELEMENT(v_old);
			BM_CHECK_ELEMENT(tv);
			BM_CHECK_ELEMENT(e_old);

			return e_old;
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
 * <pre>
 *           A                   B
 *      +--------+           +--------+
 *      |        |           |        |
 *      |   f1   |           |   f1   |
 *     v1========v2 = Ok!    v1==V2==v3 == Wrong!
 *      |   f2   |           |   f2   |
 *      |        |           |        |
 *      +--------+           +--------+
 * </pre>
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
	BMLoop *l_iter, *l_f1 = NULL, *l_f2 = NULL;
	int newlen = 0, i, f1len = 0, f2len = 0;
	bool edok;
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

	if (!((l_f1 = BM_face_edge_share_loop(f1, e)) &&
	      (l_f2 = BM_face_edge_share_loop(f2, e))))
	{
		return NULL;
	}

	/* validate direction of f2's loop cycle is compatible */
	if (l_f1->v == l_f2->v) {
		return NULL;
	}

	/* validate that for each face, each vertex has another edge in its disk cycle that is
	 * not e, and not shared. */
	if (BM_edge_in_face(l_f1->next->e, f2) ||
	    BM_edge_in_face(l_f1->prev->e, f2) ||
	    BM_edge_in_face(l_f2->next->e, f1) ||
	    BM_edge_in_face(l_f2->prev->e, f1) )
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
		if (l_iter != l_f1) {
			BM_elem_flag_enable(l_iter->v, BM_ELEM_INTERNAL_TAG);
		}
	}
	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f2); i < f2len; i++, l_iter = l_iter->next) {
		if (l_iter != l_f2) {
			/* as soon as a duplicate is found, bail out */
			if (BM_elem_flag_test(l_iter->v, BM_ELEM_INTERNAL_TAG)) {
				return NULL;
			}
		}
	}

	/* join the two loop */
	l_f1->prev->next = l_f2->next;
	l_f2->next->prev = l_f1->prev;
	
	l_f1->next->prev = l_f2->prev;
	l_f2->prev->next = l_f1->next;
	
	/* if l_f1 was baseloop, make l_f1->next the base. */
	if (BM_FACE_FIRST_LOOP(f1) == l_f1)
		BM_FACE_FIRST_LOOP(f1) = l_f1->next;

	/* increase length of f1 */
	f1->len += (f2->len - 2);

	/* make sure each loop points to the proper face */
	newlen = f1->len;
	for (i = 0, l_iter = BM_FACE_FIRST_LOOP(f1); i < newlen; i++, l_iter = l_iter->next)
		l_iter->f = f1;
	
	/* remove edge from the disk cycle of its two vertices */
	bmesh_disk_edge_remove(l_f1->e, l_f1->e->v1);
	bmesh_disk_edge_remove(l_f1->e, l_f1->e->v2);
	
	/* deallocate edge and its two loops as well as f2 */
	if (bm->etoolflagpool) {
		BLI_mempool_free(bm->etoolflagpool, l_f1->e->oflags);
	}
	BLI_mempool_free(bm->epool, l_f1->e);
	bm->totedge--;
	BLI_mempool_free(bm->lpool, l_f1);
	bm->totloop--;
	BLI_mempool_free(bm->lpool, l_f2);
	bm->totloop--;
	if (bm->ftoolflagpool) {
		BLI_mempool_free(bm->ftoolflagpool, f2->oflags);
	}
	BLI_mempool_free(bm->fpool, f2);
	bm->totface--;
	/* account for both above */
	bm->elem_index_dirty |= BM_EDGE | BM_LOOP | BM_FACE;

	BM_CHECK_ELEMENT(f1);

	/* validate the new loop cycle */
	edok = bmesh_loop_validate(f1);
	BMESH_ASSERT(edok != false);
	
	return f1;
}

/**
 * \brief Splice Vert
 *
 * Merges two verts into one (\a v into \a vtarget).
 *
 * \return Success
 *
 * \warning This does't work for collapsing edges,
 * where \a v and \a vtarget are connected by an edge
 * (assert checks for this case).
 */
bool BM_vert_splice(BMesh *bm, BMVert *v, BMVert *v_target)
{
	BMEdge *e;

	/* verts already spliced */
	if (v == v_target) {
		return false;
	}

	/* move all the edges from v's disk to vtarget's disk */
	while ((e = v->e)) {

		/* loop  */
		BMLoop *l_first;
		if ((l_first = e->l)) {
			BMLoop *l_iter = l_first;
			do {
				if (l_iter->v == v) {
					l_iter->v = v_target;
				}
				/* else if (l_iter->prev->v == v) {...}
				 * (this case will be handled by a different edge) */
			} while ((l_iter = l_iter->radial_next) != l_first);
		}

		/* disk */
		bmesh_disk_edge_remove(e, v);
		bmesh_edge_swapverts(e, v, v_target);
		bmesh_disk_edge_append(e, v_target);
		BLI_assert(e->v1 != e->v2);
	}

	BM_CHECK_ELEMENT(v);
	BM_CHECK_ELEMENT(v_target);

	/* v is unused now, and can be killed */
	BM_vert_kill(bm, v);

	return true;
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
void bmesh_vert_separate(BMesh *bm, BMVert *v, BMVert ***r_vout, int *r_vout_len,
                         const bool copy_select)
{
	const int v_edgetot = BM_vert_face_count(v);
	BMEdge **stack = BLI_array_alloca(stack, v_edgetot);
	STACK_DECLARE(stack);

	SmallHash visithash;
	BMVert **verts = NULL;
	BMIter eiter, liter;
	BMLoop *l;
	BMEdge *e;
	int i, maxindex;
	BMLoop *l_new;

	BLI_smallhash_init_ex(&visithash, v_edgetot);

	STACK_INIT(stack, v_edgetot);

	maxindex = 0;
	BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
		if (BLI_smallhash_haskey(&visithash, (uintptr_t)e)) {
			continue;
		}

		/* Considering only edges and faces incident on vertex v, walk
		 * the edges & faces and assign an index to each connected set */
		do {
			BLI_smallhash_insert(&visithash, (uintptr_t)e, SET_INT_IN_POINTER(maxindex));

			if (e->l) {
				BMLoop *l_iter, *l_first;
				l_iter = l_first = e->l;
				do {
					l_new = (l_iter->v == v) ? l_iter->prev : l_iter->next;
					if (!BLI_smallhash_haskey(&visithash, (uintptr_t)l_new->e)) {
						STACK_PUSH(stack, l_new->e);
					}
				} while ((l_iter = l_iter->radial_next) != l_first);
			}
		} while ((e = STACK_POP(stack)));

		maxindex++;
	}

	/* Make enough verts to split v for each group */
	if (r_vout != NULL) {
		verts = MEM_callocN(sizeof(BMVert *) * maxindex, __func__);
	}
	else {
		verts = BLI_array_alloca(verts, maxindex);
	}

	verts[0] = v;
	for (i = 1; i < maxindex; i++) {
		verts[i] = BM_vert_create(bm, v->co, v, BM_CREATE_NOP);
		if (copy_select) {
			BM_elem_select_copy(bm, bm, verts[i], v);
		}
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
	STACK_INIT(stack, v_edgetot);
	BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
		if (l->v == v) {
			STACK_PUSH(stack, (BMEdge *)l);
		}
	}
	while ((l = (BMLoop *)(STACK_POP(stack)))) {
		if ((i = GET_INT_FROM_POINTER(BLI_smallhash_lookup(&visithash, (uintptr_t)l->e)))) {
			l->v = verts[i];
		}
	}
#endif

	BM_ITER_ELEM (e, &eiter, v, BM_EDGES_OF_VERT) {
		i = GET_INT_FROM_POINTER(BLI_smallhash_lookup(&visithash, (uintptr_t)e));
		if (i == 0) {
			continue;
		}

		BLI_assert(e->v1 == v || e->v2 == v);
		bmesh_disk_edge_remove(e, v);
		bmesh_edge_swapverts(e, v, verts[i]);
		bmesh_disk_edge_append(e, verts[i]);
	}

	BLI_smallhash_release(&visithash);

	for (i = 0; i < maxindex; i++) {
		BM_CHECK_ELEMENT(verts[i]);
	}

	if (r_vout_len != NULL) {
		*r_vout_len = maxindex;
	}

	if (r_vout != NULL) {
		*r_vout = verts;
	}
}

/**
 * High level function which wraps both #bmesh_vert_separate and #bmesh_edge_separate
 */
void BM_vert_separate(BMesh *bm, BMVert *v, BMVert ***r_vout, int *r_vout_len,
                     BMEdge **e_in, int e_in_len)
{
	int i;

	for (i = 0; i < e_in_len; i++) {
		BMEdge *e = e_in[i];
		if (e->l && BM_vert_in_edge(e, v)) {
			bmesh_edge_separate(bm, e, e->l, false);
		}
	}

	bmesh_vert_separate(bm, v, r_vout, r_vout_len, false);
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
bool BM_edge_splice(BMesh *bm, BMEdge *e, BMEdge *e_target)
{
	BMLoop *l;

	if (!BM_vert_in_edge(e, e_target->v1) || !BM_vert_in_edge(e, e_target->v2)) {
		/* not the same vertices can't splice */

		/* the caller should really make sure this doesn't happen ever
		 * so assert on release builds */
		BLI_assert(0);

		return false;
	}

	while (e->l) {
		l = e->l;
		BLI_assert(BM_vert_in_edge(e_target, l->v));
		BLI_assert(BM_vert_in_edge(e_target, l->next->v));
		bmesh_radial_loop_remove(l, e);
		bmesh_radial_append(e_target, l);
	}

	BLI_assert(bmesh_radial_length(e->l) == 0);

	BM_CHECK_ELEMENT(e);
	BM_CHECK_ELEMENT(e_target);

	/* removes from disks too */
	BM_edge_kill(bm, e);

	return true;
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
void bmesh_edge_separate(BMesh *bm, BMEdge *e, BMLoop *l_sep,
                         const bool copy_select)
{
	BMEdge *e_new;
#ifndef NDEBUG
	const int radlen = bmesh_radial_length(e->l);
#endif

	BLI_assert(l_sep->e == e);
	BLI_assert(e->l);
	
	if (BM_edge_is_boundary(e)) {
		/* no cut required */
		return;
	}

	if (l_sep == e->l) {
		e->l = l_sep->radial_next;
	}

	e_new = BM_edge_create(bm, e->v1, e->v2, e, BM_CREATE_NOP);
	bmesh_radial_loop_remove(l_sep, e);
	bmesh_radial_append(e_new, l_sep);
	l_sep->e = e_new;

	if (copy_select) {
		BM_elem_select_copy(bm, bm, e_new, e);
	}

	BLI_assert(bmesh_radial_length(e->l) == radlen - 1);
	BLI_assert(bmesh_radial_length(e_new->l) == 1);

	BM_CHECK_ELEMENT(e_new);
	BM_CHECK_ELEMENT(e);
}

/**
 * \brief Un-glue Region Make Vert (URMV)
 *
 * Disconnects a face from its vertex fan at loop \a l_sep
 *
 * \return The newly created BMVert
 */
BMVert *bmesh_urmv_loop(BMesh *bm, BMLoop *l_sep)
{
	BMVert **vtar;
	int len, i;
	BMVert *v_new = NULL;
	BMVert *v_sep = l_sep->v;

	/* peel the face from the edge radials on both sides of the
	 * loop vert, disconnecting the face from its fan */
	bmesh_edge_separate(bm, l_sep->e, l_sep, false);
	bmesh_edge_separate(bm, l_sep->prev->e, l_sep->prev, false);

	if (bmesh_disk_count(v_sep) == 2) {
		/* If there are still only two edges out of v_sep, then
		 * this whole URMV was just a no-op, so exit now. */
		return v_sep;
	}

	/* Update the disk start, so that v->e points to an edge
	 * not touching the split loop. This is so that BM_vert_split
	 * will leave the original v_sep on some *other* fan (not the
	 * one-face fan that holds the unglue face). */
	while (v_sep->e == l_sep->e || v_sep->e == l_sep->prev->e) {
		v_sep->e = bmesh_disk_edge_next(v_sep->e, v_sep);
	}

	/* Split all fans connected to the vert, duplicating it for
	 * each fans. */
	bmesh_vert_separate(bm, v_sep, &vtar, &len, false);

	/* There should have been at least two fans cut apart here,
	 * otherwise the early exit would have kicked in. */
	BLI_assert(len >= 2);

	v_new = l_sep->v;

	/* Desired result here is that a new vert should always be
	 * created for the unglue face. This is so we can glue any
	 * extras back into the original vert. */
	BLI_assert(v_new != v_sep);
	BLI_assert(v_sep == vtar[0]);

	/* If there are more than two verts as a result, glue together
	 * all the verts except the one this URMV intended to create */
	if (len > 2) {
		for (i = 0; i < len; i++) {
			if (vtar[i] == v_new) {
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

	return v_new;
}

/**
 * \brief Unglue Region Make Vert (URMV)
 *
 * Disconnects f_sep from the vertex fan at \a v_sep
 *
 * \return The newly created BMVert
 */
BMVert *bmesh_urmv(BMesh *bm, BMFace *f_sep, BMVert *v_sep)
{
	BMLoop *l = BM_face_vert_share_loop(f_sep, v_sep);
	return bmesh_urmv_loop(bm, l);
}

/**
 * Avoid calling this where possible,
 * low level function so both face pointers remain intact but point to swapped data.
 * \note must be from the same bmesh.
 */
void bmesh_face_swap_data(BMFace *f_a, BMFace *f_b)
{
	BMLoop *l_iter, *l_first;

	BLI_assert(f_a != f_b);

	l_iter = l_first = BM_FACE_FIRST_LOOP(f_a);
	do {
		l_iter->f = f_b;
	} while ((l_iter = l_iter->next) != l_first);

	l_iter = l_first = BM_FACE_FIRST_LOOP(f_b);
	do {
		l_iter->f = f_a;
	} while ((l_iter = l_iter->next) != l_first);

	SWAP(BMFace, (*f_a), (*f_b));

	/* swap back */
	SWAP(void *, f_a->head.data, f_b->head.data);
	SWAP(int, f_a->head.index, f_b->head.index);
}
