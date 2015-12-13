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

/** \file blender/editors/mesh/editmesh_intersect.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_math.h"
#include "BLI_memarena.h"
#include "BLI_stack.h"
#include "BLI_buffer.h"
#include "BLI_kdopbvh.h"
#include "BLI_linklist_stack.h"

#include "BKE_editmesh_bvh.h"
#include "BKE_context.h"
#include "BKE_report.h"
#include "BKE_editmesh.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_screen.h"

#include "intern/bmesh_private.h"

#include "mesh_intern.h"  /* own include */

#include "tools/bmesh_intersect.h"


/* detect isolated holes and fill them */
#define USE_NET_ISLAND_CONNECT

/**
 * Compare selected with its self.
 */
static int bm_face_isect_self(BMFace *f, void *UNUSED(user_data))
{
	if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
		return 0;
	}
	else {
		return -1;
	}
}

/**
 * Compare selected/unselected.
 */
static int bm_face_isect_pair(BMFace *f, void *UNUSED(user_data))
{
	if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
		return -1;
	}
	else if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
		return 1;
	}
	else {
		return 0;
	}
}

/**
 * A flipped version of #bm_face_isect_pair
 * use for boolean 'difference', which depends on order.
 */
static int bm_face_isect_pair_swap(BMFace *f, void *UNUSED(user_data))
{
	if (BM_elem_flag_test(f, BM_ELEM_HIDDEN)) {
		return -1;
	}
	else if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
		return 0;
	}
	else {
		return 1;
	}
}

/**
 * Use for intersect and boolean.
 */
static void edbm_intersect_select(BMEditMesh *em)
{
	BM_mesh_elem_hflag_disable_all(em->bm, BM_VERT | BM_EDGE | BM_FACE, BM_ELEM_SELECT, false);

	if (em->bm->selectmode & (SCE_SELECT_VERTEX | SCE_SELECT_EDGE)) {
		BMIter iter;
		BMEdge *e;

		BM_ITER_MESH (e, &iter, em->bm, BM_EDGES_OF_MESH) {
			if (BM_elem_flag_test(e, BM_ELEM_TAG)) {
				BM_edge_select_set(em->bm, e, true);
			}
		}
	}

	EDBM_mesh_normals_update(em);
	EDBM_update_generic(em, true, true);

}

/* -------------------------------------------------------------------- */
/* Cut intersections into geometry */

/** \name Simple Intersect (self-intersect)
 * \{
 */

enum {
	ISECT_SEL           = 0,
	ISECT_SEL_UNSEL     = 1,
};

static int edbm_intersect_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	const int mode = RNA_enum_get(op->ptr, "mode");
	int (*test_fn)(BMFace *, void *);
	bool use_separate = RNA_boolean_get(op->ptr, "use_separate");
	const float eps = RNA_float_get(op->ptr, "threshold");
	bool use_self;
	bool has_isect;

	switch (mode) {
		case ISECT_SEL:
			test_fn = bm_face_isect_self;
			use_self = true;
			break;
		default:  /* ISECT_SEL_UNSEL */
			test_fn = bm_face_isect_pair;
			use_self = false;
			break;
	}


	has_isect = BM_mesh_intersect(
	        bm,
	        em->looptris, em->tottri,
	        test_fn, NULL,
	        use_self, use_separate, true, true,
	        -1,
	        eps);


	if (has_isect) {
		edbm_intersect_select(em);
	}
	else {
		BKE_report(op->reports, RPT_WARNING, "No intersections found");
	}

	return OPERATOR_FINISHED;
}

void MESH_OT_intersect(struct wmOperatorType *ot)
{
	static EnumPropertyItem isect_mode_items[] = {
		{ISECT_SEL, "SELECT", 0, "Self Intersect",
		 "Self intersect selected faces"},
		{ISECT_SEL_UNSEL, "SELECT_UNSELECT", 0, "Selected/Unselected",
		 "Intersect selected with unselected faces"},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Intersect";
	ot->description = "Cut an intersection into faces";
	ot->idname = "MESH_OT_intersect";

	/* api callbacks */
	ot->exec = edbm_intersect_exec;
	ot->poll = ED_operator_editmesh;

	/* props */
	RNA_def_enum(ot->srna, "mode", isect_mode_items, ISECT_SEL_UNSEL, "Source", "");
	RNA_def_boolean(ot->srna, "use_separate", true, "Separate", "");
	RNA_def_float_distance(ot->srna, "threshold", 0.000001f, 0.0, 0.01, "Merge threshold", "", 0.0, 0.001);

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */


/* -------------------------------------------------------------------- */
/* Boolean (a kind of intersect) */

/** \name Boolean Intersect
 *
 * \note internally this is nearly exactly the same as 'MESH_OT_intersect',
 * however from a user perspective they are quite different, so expose as different tools.
 *
 * \{
 */

static int edbm_intersect_boolean_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	const int boolean_operation = RNA_enum_get(op->ptr, "operation");
	bool use_swap = RNA_boolean_get(op->ptr, "use_swap");
	const float eps = RNA_float_get(op->ptr, "threshold");
	int (*test_fn)(BMFace *, void *);
	bool has_isect;

	test_fn = use_swap ? bm_face_isect_pair_swap : bm_face_isect_pair;

	has_isect = BM_mesh_intersect(
	        bm,
	        em->looptris, em->tottri,
	        test_fn, NULL,
	        false, false, true, true,
	        boolean_operation,
	        eps);


	if (has_isect) {
		edbm_intersect_select(em);
	}
	else {
		BKE_report(op->reports, RPT_WARNING, "No intersections found");
	}

	return OPERATOR_FINISHED;
}

void MESH_OT_intersect_boolean(struct wmOperatorType *ot)
{
	static EnumPropertyItem isect_boolean_operation_items[] = {
		{BMESH_ISECT_BOOLEAN_ISECT, "INTERSECT", 0, "Intersect", ""},
		{BMESH_ISECT_BOOLEAN_UNION, "UNION", 0, "Union", ""},
		{BMESH_ISECT_BOOLEAN_DIFFERENCE, "DIFFERENCE", 0, "Difference", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Boolean Intersect";
	ot->description = "Cut solid geometry from selected to unselected";
	ot->idname = "MESH_OT_intersect_boolean";

	/* api callbacks */
	ot->exec = edbm_intersect_boolean_exec;
	ot->poll = ED_operator_editmesh;

	/* props */
	RNA_def_enum(ot->srna, "operation", isect_boolean_operation_items, BMESH_ISECT_BOOLEAN_DIFFERENCE, "Boolean", "");
	RNA_def_boolean(ot->srna, "use_swap", false, "Swap", "Use with difference intersection to swap which side is kept");
	RNA_def_float_distance(ot->srna, "threshold", 0.000001f, 0.0, 0.01, "Merge threshold", "", 0.0, 0.001);

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/* Face Split by Edges */

/** \name Face/Edge Split
 * \{ */

static void bm_face_split_by_edges(
        BMesh *bm, BMFace *f, const char hflag,
        /* reusable memory buffer */
        BLI_Buffer *edge_net_temp_buf)
{
	const int f_index = BM_elem_index_get(f);

	BMLoop *l_iter;
	BMLoop *l_first;
	BMVert *v;

	BMFace **face_arr;
	int face_arr_len;

	/* likely this will stay very small
	 * all verts pushed into this stack _must_ have their previous edges set! */
	BLI_SMALLSTACK_DECLARE(vert_stack, BMVert *);
	BLI_SMALLSTACK_DECLARE(vert_stack_next, BMVert *);

	BLI_assert(edge_net_temp_buf->count == 0);

	/* collect all edges */
	l_iter = l_first = BM_FACE_FIRST_LOOP(f);
	do {
		BMIter iter;
		BMEdge *e;

		BM_ITER_ELEM (e, &iter, l_iter->v, BM_EDGES_OF_VERT) {
			if (BM_elem_flag_test(e, hflag) &&
			    (BM_elem_index_get(e) == f_index))
			{
				v = BM_edge_other_vert(e, l_iter->v);
				v->e = e;

				BLI_SMALLSTACK_PUSH(vert_stack, v);
				BLI_buffer_append(edge_net_temp_buf, BMEdge *, e);
			}
		}
	} while ((l_iter = l_iter->next) != l_first);



	/* now assign all */
	/* pop free values into the next stack */
	while ((v = BLI_SMALLSTACK_POP_EX(vert_stack, vert_stack_next))) {
		BMIter eiter;
		BMEdge *e_next;

		BM_ITER_ELEM (e_next, &eiter, v, BM_EDGES_OF_VERT) {
			if (BM_elem_flag_test(e_next, hflag) &&
			    (BM_elem_index_get(e_next) == -1))
			{
				BMVert *v_next;
				v_next = BM_edge_other_vert(e_next, v);
				BM_elem_index_set(e_next, f_index);
				BLI_SMALLSTACK_PUSH(vert_stack_next, v_next);
				BLI_buffer_append(edge_net_temp_buf, BMEdge *, e_next);
			}
		}

		if (BLI_SMALLSTACK_IS_EMPTY(vert_stack)) {
			BLI_SMALLSTACK_SWAP(vert_stack, vert_stack_next);
		}
	}

	BM_face_split_edgenet(
	        bm, f, edge_net_temp_buf->data, edge_net_temp_buf->count,
	        &face_arr, &face_arr_len);

	BLI_buffer_empty(edge_net_temp_buf);

	if (face_arr_len) {
		int i;
		for (i = 0; i < face_arr_len; i++) {
			BM_face_select_set(bm, face_arr[i], true);
			BM_elem_flag_disable(face_arr[i], hflag);
		}
	}

	if (face_arr) {
		MEM_freeN(face_arr);
	}
}

#ifdef USE_NET_ISLAND_CONNECT

struct LinkBase {
	LinkNode    *list;
	unsigned int list_len;
};

static void ghash_insert_face_edge_link(
        GHash *gh, BMFace *f_key, BMEdge *e_val,
        MemArena *mem_arena)
{
	void           **ls_base_p;
	struct LinkBase *ls_base;
	LinkNode *ls;

	if (!BLI_ghash_ensure_p(gh, f_key, &ls_base_p)) {
		ls_base = *ls_base_p = BLI_memarena_alloc(mem_arena, sizeof(*ls_base));
		ls_base->list     = NULL;
		ls_base->list_len = 0;
	}
	else {
		ls_base = *ls_base_p;
	}

	ls = BLI_memarena_alloc(mem_arena, sizeof(*ls));
	ls->next = ls_base->list;
	ls->link = e_val;
	ls_base->list = ls;
	ls_base->list_len += 1;
}

static void bm_face_split_by_edges_island_connect(
        BMesh *bm, BMFace *f,
        LinkNode *e_link, const int e_link_len,
        MemArena *mem_arena_edgenet)
{
	BMEdge **edge_arr = BLI_memarena_alloc(mem_arena_edgenet, sizeof(BMEdge **) * e_link_len);
	int edge_arr_len = 0;

	while (e_link) {
		edge_arr[edge_arr_len++] = e_link->link;
		e_link = e_link->next;
	}

	{
		unsigned int edge_arr_holes_len;
		BMEdge **edge_arr_holes;
		if (BM_face_split_edgenet_connect_islands(
		        bm, f,
		        edge_arr, e_link_len,
		        mem_arena_edgenet,
		        &edge_arr_holes, &edge_arr_holes_len))
		{
			edge_arr_len = edge_arr_holes_len;
			edge_arr = edge_arr_holes;  /* owned by the arena */
		}
	}

	BM_face_split_edgenet(
	        bm, f, edge_arr, edge_arr_len,
	        NULL, NULL);
}

#endif  /* USE_NET_ISLAND_CONNECT */


static int edbm_face_split_by_edges_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BMesh *bm = em->bm;
	const char hflag = BM_ELEM_TAG;

	BMVert *v;
	BMEdge *e;
	BMFace *f;
	BMIter iter;

	BLI_SMALLSTACK_DECLARE(loop_stack, BMLoop *);

	BM_ITER_MESH (v, &iter, bm, BM_VERTS_OF_MESH) {
		BM_elem_flag_disable(v, hflag);
	}

	/* edge index is set to -1 then used to assosiate them with faces */
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, BM_ELEM_SELECT) && BM_edge_is_wire(e)) {
			BM_elem_flag_enable(e, hflag);

			BM_elem_flag_enable(e->v1, hflag);
			BM_elem_flag_enable(e->v2, hflag);

		}
		else {
			BM_elem_flag_disable(e, hflag);
		}
		BM_elem_index_set(e, -1);  /* set_dirty */
	}

	BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
		if (BM_elem_flag_test(f, BM_ELEM_SELECT)) {
			BM_elem_flag_enable(f, hflag);
		}
		else {
			BM_elem_flag_disable(f, hflag);
		}
	}

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_flag_test(e, hflag)) {
			BMIter viter;
			BM_ITER_ELEM (v, &viter, e, BM_VERTS_OF_EDGE) {
				BMIter liter;
				BMLoop *l;

				unsigned int loop_stack_len;
				BMLoop *l_best = NULL;

				BLI_assert(BLI_SMALLSTACK_IS_EMPTY(loop_stack));
				loop_stack_len = 0;

				BM_ITER_ELEM (l, &liter, v, BM_LOOPS_OF_VERT) {
					if (BM_elem_flag_test(l->f, hflag)) {
						BLI_SMALLSTACK_PUSH(loop_stack, l);
						loop_stack_len++;
					}
				}

				if (loop_stack_len == 0) {
					/* pass */
				}
				else if (loop_stack_len == 1) {
					l_best = BLI_SMALLSTACK_POP(loop_stack);
				}
				else {
					/* complicated case, match the edge with a face-loop */

					BMVert *v_other = BM_edge_other_vert(e, v);
					float e_dir[3];

					/* we want closest to zero */
					float dot_best = FLT_MAX;

					sub_v3_v3v3(e_dir, v_other->co, v->co);
					normalize_v3(e_dir);

					while ((l = BLI_SMALLSTACK_POP(loop_stack))) {
						float dot_test;

						/* Check dot first to save on expensive angle-comparison.
						 * ideal case is 90d difference == 0.0 dot */
						dot_test = fabsf(dot_v3v3(e_dir, l->f->no));
						if (dot_test < dot_best) {

							/* check we're in the correct corner (works with convex loops too) */
							if (angle_signed_on_axis_v3v3v3_v3(l->prev->v->co, l->v->co, v_other->co,    l->f->no) <
							    angle_signed_on_axis_v3v3v3_v3(l->prev->v->co, l->v->co, l->next->v->co, l->f->no))
							{
								dot_best = dot_test;
								l_best = l;
							}
						}
					}
				}

				if (l_best) {
					BM_elem_index_set(e, BM_elem_index_get(l_best->f));  /* set_dirty */
				}
			}
		}
	}

	bm->elem_index_dirty |= BM_EDGE;

	{
		BLI_buffer_declare_static(BMEdge **, edge_net_temp_buf, 0, 128);

		BM_ITER_MESH (f, &iter, bm, BM_FACES_OF_MESH) {
			if (BM_elem_flag_test(f, hflag)) {
				bm_face_split_by_edges(bm, f, hflag, &edge_net_temp_buf);
			}
		}
		BLI_buffer_free(&edge_net_temp_buf);
	}

#ifdef USE_NET_ISLAND_CONNECT
	/* before overwriting edge index values, collect edges left untouched */
	BLI_Stack *edges_loose = BLI_stack_new(sizeof(BMEdge * ), __func__);
	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (BM_elem_index_get(e) == -1 && BM_edge_is_wire(e)) {
			BLI_stack_push(edges_loose, &e);
		}
	}
#endif

	EDBM_mesh_normals_update(em);
	EDBM_update_generic(em, true, true);


#ifdef USE_NET_ISLAND_CONNECT
	/* we may have remaining isolated regions remaining,
	 * these will need to have connecting edges created */
	if (!BLI_stack_is_empty(edges_loose)) {
		GHash *face_edge_map = BLI_ghash_ptr_new(__func__);

		MemArena *mem_arena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

		{
			BMBVHTree *bmbvh = BKE_bmbvh_new(bm, em->looptris, em->tottri, BMBVH_RESPECT_SELECT, NULL, NULL);

			while (!BLI_stack_is_empty(edges_loose)) {
				BLI_stack_pop(edges_loose, &e);
				float e_center[3];
				mid_v3_v3v3(e_center, e->v1->co, e->v2->co);

				f = BKE_bmbvh_find_face_closest(bmbvh, e_center, FLT_MAX);
				if (f) {
					ghash_insert_face_edge_link(face_edge_map, f, e, mem_arena);
				}
			}

			BKE_bmbvh_free(bmbvh);
		}

		{
			MemArena *mem_arena_edgenet = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE, __func__);

			GHashIterator gh_iter;

			GHASH_ITER(gh_iter, face_edge_map) {
				f = BLI_ghashIterator_getKey(&gh_iter);
				struct LinkBase *e_ls_base = BLI_ghashIterator_getValue(&gh_iter);

				bm_face_split_by_edges_island_connect(
				        bm, f,
				        e_ls_base->list, e_ls_base->list_len,
				        mem_arena_edgenet);

				BLI_memarena_clear(mem_arena_edgenet);
			}

			BLI_memarena_free(mem_arena_edgenet);
		}

		BLI_memarena_free(mem_arena);

		BLI_ghash_free(face_edge_map, NULL, NULL);

		EDBM_mesh_normals_update(em);
		EDBM_update_generic(em, true, true);
	}

	BLI_stack_free(edges_loose);
#endif  /* USE_NET_ISLAND_CONNECT */

	return OPERATOR_FINISHED;
}


void MESH_OT_face_split_by_edges(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Split by Edges";
	ot->description = "Split faces by loose edges";
	ot->idname = "MESH_OT_face_split_by_edges";

	/* api callbacks */
	ot->exec = edbm_face_split_by_edges_exec;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */
