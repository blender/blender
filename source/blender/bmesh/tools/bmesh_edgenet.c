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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/tools/bmesh_edgenet.c
 *  \ingroup bmesh
 *
 * Edgenet Fill.
 *
 */

#include <limits.h>

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_mempool.h"
#include "BLI_linklist.h"

#include "bmesh.h"
#include "bmesh_edgenet.h"  /* own include */

#include "BLI_strict_flags.h"  /* keep last */


/* Struct for storing a path of verts walked over */
typedef struct VertNetInfo {
	BMVert *prev;               /* previous vertex */
	int pass;                   /* path scanning pass value, for internal calculation */
	int face;                   /* face index connected to the edge between this and the previous vert */
	int flag;                   /* flag */
} VertNetInfo;

enum {
	VNINFO_FLAG_IS_MIXFACE = (1 << 0),
};

/**
 * Check if this edge can be used in a path.
 */
static bool bm_edge_step_ok(BMEdge *e)
{
	return BM_elem_flag_test(e, BM_ELEM_TAG) && ((e->l == NULL) || (e->l->radial_next == e->l));
}

static int bm_edge_face(BMEdge *e)
{
	return e->l ? BM_elem_index_get(e->l->f) : -1;
}

/**
 * Get the next available edge we can use to attempt tp calculate a path from.
 */
static BMEdge *bm_edgenet_edge_get_next(
        BMesh *bm,
        LinkNode **edge_queue, BLI_mempool *edge_queue_pool)
{
	BMEdge *e;
	BMIter iter;

	while (*edge_queue) {
		e = BLI_linklist_pop_pool(edge_queue, edge_queue_pool);
		if (bm_edge_step_ok(e)) {
			return e;
		}
	}

	BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
		if (bm_edge_step_ok(e)) {
			return e;
		}
	}

	return NULL;
}


/**
 * Edge loops are built up using links to the 'prev' member.
 * with each side of the loop having its own pass (negated from the other).
 *
 * This function returns half a loop, the caller needs to run twice to get both sides.
 */
static unsigned int bm_edgenet_path_from_pass(
        BMVert *v, LinkNode **v_ls,
        VertNetInfo *vnet_info, BLI_mempool *path_pool)
{
	VertNetInfo *vn = &vnet_info[BM_elem_index_get(v)];
	const int pass = vn->pass;
	unsigned int v_ls_tot = 0;

	do {
		BLI_linklist_prepend_pool(v_ls, v, path_pool);
		v_ls_tot += 1;
		v = vn->prev;
		vn = &vnet_info[BM_elem_index_get(v)];
	} while (vn->pass == pass);

	return v_ls_tot;
}

/**
 * Specialized wrapper for #BM_face_exists_overlap_subset
 * that gets the verts from a path before we allocate it in the correct order.
 */
static bool bm_edgenet_path_check_overlap(
        BMVert *v1, BMVert *v2,
        VertNetInfo *vnet_info)
{
	/* vert order doesn't matter */
	unsigned int v_ls_tot = 0;
	LinkNode *v_ls = NULL;
	BMVert *v_pair[2] = {v1, v2};
	unsigned int i;

	for (i = 0; i < 2; i++) {
		BMVert *v = v_pair[i];
		VertNetInfo *vn = &vnet_info[BM_elem_index_get(v)];
		const int pass = vn->pass;
		do {
			BLI_linklist_prepend_alloca(&v_ls, v);
			v_ls_tot += 1;
			v = vn->prev;
			vn = &vnet_info[BM_elem_index_get(v)];
		} while (vn->pass == pass);
	}

	if (v_ls_tot) {
		BMVert **vert_arr = BLI_array_alloca(vert_arr, v_ls_tot);
		LinkNode *v_lnk;
		for (i = 0, v_lnk = v_ls; i < v_ls_tot; v_lnk = v_lnk->next, i++) {
			vert_arr[i] = v_lnk->link;
		}

		return BM_face_exists_overlap_subset(vert_arr, (int)v_ls_tot);
	}
	else {
		return false;
	}
}

/**
 * Create a face from the path.
 */
static BMFace *bm_edgenet_face_from_path(
        BMesh *bm, LinkNode *path, const unsigned int path_len)
{
	BMFace *f;
	LinkNode *v_lnk;
	unsigned int i;
	unsigned int i_prev;

	BMVert **vert_arr = BLI_array_alloca(vert_arr, path_len);
	BMEdge **edge_arr = BLI_array_alloca(edge_arr, path_len);

	for (v_lnk = path, i = 0; v_lnk; v_lnk = v_lnk->next, i++) {
		vert_arr[i] = v_lnk->link;
	}

	i_prev = path_len - 1;
	for (i = 0; i < path_len; i++) {
		edge_arr[i_prev] = BM_edge_exists(vert_arr[i], vert_arr[i_prev]);
		i_prev = i;
	}

	/* no need for this, we do overlap checks before allowing the path to be used */
#if 0
	if (BM_face_exists_multi(vert_arr, edge_arr, path_len)) {
		return NULL;
	}
#endif

	f = BM_face_create(bm, vert_arr, edge_arr, (int)path_len, NULL, BM_CREATE_NOP);

	return f;
}

/**
 * Step along the path from \a v_curr to any vert not already in the path.
 *
 * \return The connecting edge if the path is found, otherwise NULL.
 */
static BMEdge *bm_edgenet_path_step(
        BMVert *v_curr, LinkNode **v_ls,
        VertNetInfo *vnet_info, BLI_mempool *path_pool)
{
	const VertNetInfo *vn_curr = &vnet_info[BM_elem_index_get(v_curr)];

	BMEdge *e;
	BMIter iter;
	unsigned int tot = 0;
	unsigned int v_ls_tot = 0;

	BM_ITER_ELEM (e, &iter, v_curr, BM_EDGES_OF_VERT) {
		BMVert *v_next = BM_edge_other_vert(e, v_curr);
		if (v_next != vn_curr->prev) {
			if (bm_edge_step_ok(e)) {
				VertNetInfo *vn_next = &vnet_info[BM_elem_index_get(v_next)];

				/* check we're not looping back on ourselves */
				if (vn_curr->pass != vn_next->pass) {

					if (vn_curr->pass == -vn_next->pass) {
						if ((vn_curr->flag & VNINFO_FLAG_IS_MIXFACE) ||
						    (vn_next->flag & VNINFO_FLAG_IS_MIXFACE))
						{
							/* found connecting edge */
							if (bm_edgenet_path_check_overlap(v_curr, v_next, vnet_info) == false) {
								return e;
							}
						}
					}
					else {
						vn_next->face = bm_edge_face(e);
						vn_next->pass = vn_curr->pass;
						vn_next->prev = v_curr;

						/* flush flag down the path */
						vn_next->flag &= ~VNINFO_FLAG_IS_MIXFACE;
						if ((vn_curr->flag & VNINFO_FLAG_IS_MIXFACE) ||
						    (vn_next->face == -1) ||
						    (vn_next->face != vn_curr->face))
						{
							vn_next->flag |= VNINFO_FLAG_IS_MIXFACE;
						}

						/* add to the list! */
						BLI_linklist_prepend_pool(v_ls, v_next, path_pool);
						v_ls_tot += 1;
					}
				}
			}
			tot += 1;
		}
	}

	/* trick to walk along wire-edge paths */
	if (v_ls_tot == 1 && tot == 1) {
		v_curr = BLI_linklist_pop_pool(v_ls, path_pool);
		bm_edgenet_path_step(v_curr, v_ls, vnet_info, path_pool);
	}

	return NULL;
}

/**
 * Given an edge, find the first path that can form a face.
 *
 * \return A linked list of verts.
 */
static LinkNode *bm_edgenet_path_calc(
        BMEdge *e, const int pass_nr, const unsigned int path_cost_max,
        unsigned int *r_path_len, unsigned int *r_path_cost,
        VertNetInfo *vnet_info, BLI_mempool *path_pool)
{
	VertNetInfo *vn_1, *vn_2;
	const int f_index = bm_edge_face(e);
	bool found;

	LinkNode *v_ls_prev = NULL;
	LinkNode *v_ls_next = NULL;

	unsigned int path_cost_accum = 0;

	BLI_assert(bm_edge_step_ok(e));

	*r_path_len = 0;
	*r_path_cost = 0;

	vn_1 = &vnet_info[BM_elem_index_get(e->v1)];
	vn_2 = &vnet_info[BM_elem_index_get(e->v2)];

	vn_1->pass =  pass_nr;
	vn_2->pass = -pass_nr;

	vn_1->prev = e->v2;
	vn_2->prev = e->v1;

	vn_1->face =
	vn_2->face = f_index;

	vn_1->flag =
	vn_2->flag = (f_index == -1) ? VNINFO_FLAG_IS_MIXFACE : 0;

	/* prime the searchlist */
	BLI_linklist_prepend_pool(&v_ls_prev, e->v1, path_pool);
	BLI_linklist_prepend_pool(&v_ls_prev, e->v2, path_pool);

	do {
		found = false;

		/* no point to continue, we're over budget */
		if (path_cost_accum >= path_cost_max) {
			BLI_linklist_free_pool(v_ls_next, NULL, path_pool);
			BLI_linklist_free_pool(v_ls_prev, NULL, path_pool);
			return NULL;
		}

		while (v_ls_prev) {
			const LinkNode *v_ls_next_old = v_ls_next;
			BMVert *v = BLI_linklist_pop_pool(&v_ls_prev, path_pool);
			BMEdge *e_found = bm_edgenet_path_step(v, &v_ls_next, vnet_info, path_pool);

			if (e_found) {
				LinkNode *path = NULL;
				unsigned int path_len;
				BLI_linklist_free_pool(v_ls_next, NULL, path_pool);
				BLI_linklist_free_pool(v_ls_prev, NULL, path_pool);

				// BLI_assert(BLI_mempool_count(path_pool) == 0);

				path_len = bm_edgenet_path_from_pass(e_found->v1, &path, vnet_info, path_pool);
				BLI_linklist_reverse(&path);
				path_len += bm_edgenet_path_from_pass(e_found->v2, &path, vnet_info, path_pool);
				*r_path_len = path_len;
				*r_path_cost = path_cost_accum;
				return path;
			}
			else {
				/* check if a change was made */
				if (v_ls_next_old != v_ls_next) {
					found = true;
				}
			}

		}
		BLI_assert(v_ls_prev == NULL);

		path_cost_accum++;

		/* swap */
		v_ls_prev = v_ls_next;
		v_ls_next = NULL;

	} while (found);

	BLI_assert(v_ls_prev == NULL);
	BLI_assert(v_ls_next == NULL);

	/* tag not to search again */
	BM_elem_flag_disable(e, BM_ELEM_TAG);

	return NULL;
}

/**
 * Wrapper for #bm_edgenet_path_calc which ensures all included edges
 * _don't_ have a better option.
 */
static LinkNode *bm_edgenet_path_calc_best(
        BMEdge *e, int *pass_nr, unsigned int path_cost_max,
        unsigned int *r_path_len, unsigned int *r_path_cost,
        VertNetInfo *vnet_info, BLI_mempool *path_pool)
{
	LinkNode *path;
	unsigned int path_cost;

	path = bm_edgenet_path_calc(e, *pass_nr, path_cost_max,
	                            r_path_len, &path_cost,
	                            vnet_info, path_pool);
	(*pass_nr)++;

	if (path == NULL) {
		return NULL;
	}
	else if (path_cost <= 1) {
		/* any face that takes 1-2 iterations to find we consider valid */
		return path;
	}
	else {
		/* Check every edge to see if any can give a better path.
		 * This avoids very strange/long paths from being created. */

		const unsigned int path_len = *r_path_len;
		unsigned int i, i_prev;
		BMVert **vert_arr = BLI_array_alloca(vert_arr, path_len);
		LinkNode *v_lnk;

		for (v_lnk = path, i = 0; v_lnk; v_lnk = v_lnk->next, i++) {
			vert_arr[i] = v_lnk->link;
		}

		i_prev = path_len - 1;
		for (i = 0; i < path_len; i++) {
			BMEdge *e_other = BM_edge_exists(vert_arr[i], vert_arr[i_prev]);
			if (e_other != e) {
				LinkNode *path_test;
				unsigned int path_len_test;
				unsigned int path_cost_test;

				path_test = bm_edgenet_path_calc(e_other, *pass_nr, path_cost,
				                                 &path_len_test, &path_cost_test,
				                                 vnet_info, path_pool);
				(*pass_nr)++;

				if (path_test) {
					BLI_assert(path_cost_test < path_cost);

					BLI_linklist_free_pool(path, NULL, path_pool);
					path = path_test;
					*r_path_len = path_len_test;
					*r_path_cost = path_cost_test;
					path_cost = path_cost_test;
				}
			}

			i_prev = i;
		}
	}
	return path;
}

/**
 * Fill in faces from an edgenet made up of boundary and wire edges.
 *
 * \note New faces currently don't have their normals calculated and are flipped randomly.
 *       The caller needs to flip faces correctly.
 *
 * \param bm  The mesh to operate on.
 * \param use_edge_tag  Only fill tagged edges.
 * \param face_oflag  if nonzero, apply all new faces with this bmo flag.
 */
void BM_mesh_edgenet(BMesh *bm,
                     const bool use_edge_tag, const bool use_new_face_tag)
{
	VertNetInfo *vnet_info = MEM_callocN(sizeof(*vnet_info) * (size_t)bm->totvert, __func__);
	BLI_mempool *edge_queue_pool = BLI_mempool_create(sizeof(LinkNode), 0, 512, BLI_MEMPOOL_NOP);
	BLI_mempool *path_pool = BLI_mempool_create(sizeof(LinkNode), 0, 512, BLI_MEMPOOL_NOP);
	LinkNode *edge_queue = NULL;

	BMEdge *e;
	BMIter iter;

	int pass_nr = 1;

	if (use_edge_tag == false) {
		BM_ITER_MESH (e, &iter, bm, BM_EDGES_OF_MESH) {
			BM_elem_flag_enable(e, BM_ELEM_TAG);
			BM_elem_flag_set(e, BM_ELEM_TAG, bm_edge_step_ok(e));
		}
	}

	BM_mesh_elem_index_ensure(bm, BM_VERT | BM_FACE);

	while (true) {
		LinkNode *path = NULL;
		unsigned int path_len;
		unsigned int path_cost;

		e = bm_edgenet_edge_get_next(bm, &edge_queue, edge_queue_pool);
		if (e == NULL) {
			break;
		}

		BLI_assert(bm_edge_step_ok(e) == true);

		path = bm_edgenet_path_calc_best(e, &pass_nr, UINT_MAX,
		                                 &path_len, &path_cost,
		                                 vnet_info, path_pool);

		if (path) {
			BMFace *f = bm_edgenet_face_from_path(bm, path, path_len);
			/* queue edges to operate on */
			BMLoop *l_first, *l_iter;
			l_iter = l_first = BM_FACE_FIRST_LOOP(f);
			do {
				if (bm_edge_step_ok(l_iter->e)) {
					BLI_linklist_prepend_pool(&edge_queue, l_iter->e, edge_queue_pool);
				}
			} while ((l_iter = l_iter->next) != l_first);

			if (use_new_face_tag) {
				BM_elem_flag_enable(f, BM_ELEM_TAG);
			}

			/* the face index only needs to be unique, not kept valid */
			BM_elem_index_set(f, bm->totface - 1);  /* set_dirty */
		}

		BLI_linklist_free_pool(path, NULL, path_pool);
		BLI_assert(BLI_mempool_count(path_pool) == 0);
	}

	bm->elem_index_dirty |= BM_FACE | BM_LOOP;

	BLI_mempool_destroy(edge_queue_pool);
	BLI_mempool_destroy(path_pool);
	MEM_freeN(vnet_info);
}
