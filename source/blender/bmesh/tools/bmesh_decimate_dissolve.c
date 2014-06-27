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

/** \file blender/bmesh/tools/bmesh_decimate_dissolve.c
 *  \ingroup bmesh
 *
 * BMesh decimator that dissolves flat areas into polygons (ngons).
 */


#include "MEM_guardedalloc.h"

#include "BLI_math.h"
#include "BLI_heap.h"

#include "bmesh.h"
#include "bmesh_decimate.h"  /* own include */

#define COST_INVALID FLT_MAX


/* multiply vertex edge angle by face angle
 * this means we are not left with sharp corners between _almost_ planer faces
 * convert angles [0-PI/2] -> [0-1], multiply together, then convert back to radians. */
static float bm_vert_edge_face_angle(BMVert *v)
{
#define UNIT_TO_ANGLE DEG2RADF(90.0f)
#define ANGLE_TO_UNIT (1.0f / UNIT_TO_ANGLE)

	const float angle = BM_vert_calc_edge_angle(v);
	/* note: could be either edge, it doesn't matter */
	if (v->e && BM_edge_is_manifold(v->e)) {
		return ((angle * ANGLE_TO_UNIT) * (BM_edge_calc_face_angle(v->e) * ANGLE_TO_UNIT)) * UNIT_TO_ANGLE;
	}
	else {
		return angle;
	}

#undef UNIT_TO_ANGLE
#undef ANGLE_TO_UNIT
}

static float bm_edge_calc_dissolve_error(const BMEdge *e, const BMO_Delimit delimit)
{
	const bool is_contig = BM_edge_is_contiguous(e);
	float angle;

	if (!BM_edge_is_manifold(e)) {
		goto fail;
	}

	if ((delimit & BMO_DELIM_SEAM) &&
	    (BM_elem_flag_test(e, BM_ELEM_SEAM)))
	{
		goto fail;
	}

	if ((delimit & BMO_DELIM_MATERIAL) &&
	    (e->l->f->mat_nr != e->l->radial_next->f->mat_nr))
	{
		goto fail;
	}

	if ((delimit & BMO_DELIM_NORMAL) &&
	    (is_contig == false))
	{
		goto fail;
	}

	angle = BM_edge_calc_face_angle(e);
	if (is_contig == false) {
		angle = (float)M_PI - angle;
	}

	return angle;

fail:
	return COST_INVALID;
}


void BM_mesh_decimate_dissolve_ex(BMesh *bm, const float angle_limit, const bool do_dissolve_boundaries,
                                  const BMO_Delimit delimit,
                                  BMVert **vinput_arr, const int vinput_len,
                                  BMEdge **einput_arr, const int einput_len,
                                  const short oflag_out)
{
	const int eheap_table_len = do_dissolve_boundaries ? einput_len : max_ii(einput_len, vinput_len);
	void *_heap_table = MEM_mallocN(sizeof(HeapNode *) * eheap_table_len, __func__);

	int i;

	/* --- first edges --- */
	if (1) {
		BMEdge **earray;
		Heap *eheap;
		HeapNode **eheap_table = _heap_table;
		HeapNode *enode_top;
		int *vert_reverse_lookup;
		BMIter iter;
		BMEdge *e_iter;

		/* --- setup heap --- */
		eheap = BLI_heap_new_ex(einput_len);
		eheap_table = _heap_table;

		/* wire -> tag */
		BM_ITER_MESH (e_iter, &iter, bm, BM_EDGES_OF_MESH) {
			BM_elem_flag_set(e_iter, BM_ELEM_TAG, BM_edge_is_wire(e_iter));
			BM_elem_index_set(e_iter, -1);  /* set dirty */
		}
		bm->elem_index_dirty |= BM_EDGE;

		/* build heap */
		for (i = 0; i < einput_len; i++) {
			BMEdge *e = einput_arr[i];
			const float cost = bm_edge_calc_dissolve_error(e, delimit);
			eheap_table[i] = BLI_heap_insert(eheap, cost, e);
			BM_elem_index_set(e, i);  /* set dirty */
		}

		while ((BLI_heap_is_empty(eheap) == false) &&
		       (BLI_heap_node_value((enode_top = BLI_heap_top(eheap))) < angle_limit))
		{
			BMFace *f_new = NULL;
			BMEdge *e;

			e = BLI_heap_node_ptr(enode_top);
			i = BM_elem_index_get(e);

			if (BM_edge_is_manifold(e)) {
				f_new = BM_faces_join_pair(bm, e->l->f,
				                           e->l->radial_next->f, e,
				                           false); /* join faces */

				if (f_new) {
					BMLoop *l_first, *l_iter;

					BLI_heap_remove(eheap, enode_top);
					eheap_table[i] = NULL;

					/* update normal */
					BM_face_normal_update(f_new);
					if (oflag_out) {
						BMO_elem_flag_enable(bm, f_new, oflag_out);
					}

					/* re-calculate costs */
					l_iter = l_first = BM_FACE_FIRST_LOOP(f_new);
					do {
						const int j = BM_elem_index_get(l_iter->e);
						if (j != -1 && eheap_table[j]) {
							const float cost = bm_edge_calc_dissolve_error(l_iter->e, delimit);
							BLI_heap_remove(eheap, eheap_table[j]);
							eheap_table[j] = BLI_heap_insert(eheap, cost, l_iter->e);
						}
					} while ((l_iter = l_iter->next) != l_first);
				}
				else {
					BMO_error_clear(bm);
				}
			}

			if (UNLIKELY(f_new == NULL)) {
				BLI_heap_remove(eheap, enode_top);
				eheap_table[i] = BLI_heap_insert(eheap, COST_INVALID, e);
			}
		}

		/* prepare for cleanup */
		BM_mesh_elem_index_ensure(bm, BM_VERT);
		vert_reverse_lookup = MEM_mallocN(sizeof(int) * bm->totvert, __func__);
		fill_vn_i(vert_reverse_lookup, bm->totvert, -1);
		for (i = 0; i < vinput_len; i++) {
			BMVert *v = vinput_arr[i];
			vert_reverse_lookup[BM_elem_index_get(v)] = i;
		}

		/* --- cleanup --- */
		earray = MEM_mallocN(sizeof(BMEdge *) * bm->totedge, __func__);
		BM_ITER_MESH_INDEX (e_iter, &iter, bm, BM_EDGES_OF_MESH, i) {
			earray[i] = e_iter;
		}
		/* remove all edges/verts left behind from dissolving, NULL'ing the vertex array so we dont re-use */
		for (i = bm->totedge - 1; i != -1; i--) {
			e_iter = earray[i];

			if (BM_edge_is_wire(e_iter) && (BM_elem_flag_test(e_iter, BM_ELEM_TAG) == false)) {
				/* edge has become wire */
				int vidx_reverse;
				BMVert *v1 = e_iter->v1;
				BMVert *v2 = e_iter->v2;
				BM_edge_kill(bm, e_iter);
				if (v1->e == NULL) {
					vidx_reverse = vert_reverse_lookup[BM_elem_index_get(v1)];
					if (vidx_reverse != -1) vinput_arr[vidx_reverse] = NULL;
					BM_vert_kill(bm, v1);
				}
				if (v2->e == NULL) {
					vidx_reverse = vert_reverse_lookup[BM_elem_index_get(v2)];
					if (vidx_reverse != -1) vinput_arr[vidx_reverse] = NULL;
					BM_vert_kill(bm, v2);
				}
			}
		}
		MEM_freeN(vert_reverse_lookup);
		MEM_freeN(earray);

		BLI_heap_free(eheap, NULL);
	}


	/* --- second verts --- */
	if (do_dissolve_boundaries) {
		/* simple version of the branch below, since we will dissolve _all_ verts that use 2 edges */
		for (i = 0; i < vinput_len; i++) {
			BMVert *v = vinput_arr[i];
			if (LIKELY(v != NULL) &&
			    BM_vert_is_edge_pair(v))
			{
				BM_vert_collapse_edge(bm, v->e, v, true, true);  /* join edges */
			}
		}
	}
	else {
		Heap *vheap;
		HeapNode **vheap_table = _heap_table;
		HeapNode *vnode_top;

		BMVert *v_iter;
		BMIter iter;

		BM_ITER_MESH (v_iter, &iter, bm, BM_VERTS_OF_MESH) {
			BM_elem_index_set(v_iter, -1);  /* set dirty */
		}
		bm->elem_index_dirty |= BM_VERT;

		vheap = BLI_heap_new_ex(vinput_len);

		for (i = 0; i < vinput_len; i++) {
			BMVert *v = vinput_arr[i];
			if (LIKELY(v != NULL)) {
				const float cost = bm_vert_edge_face_angle(v);
				vheap_table[i] = BLI_heap_insert(vheap, cost, v);
				BM_elem_index_set(v, i);  /* set dirty */
			}
		}

		while ((BLI_heap_is_empty(vheap) == false) &&
		       (BLI_heap_node_value((vnode_top = BLI_heap_top(vheap))) < angle_limit))
		{
			BMEdge *e_new = NULL;
			BMVert *v;

			v = BLI_heap_node_ptr(vnode_top);
			i = BM_elem_index_get(v);

			if (BM_vert_is_edge_pair(v)) {
				e_new = BM_vert_collapse_edge(bm, v->e, v, true, true);  /* join edges */

				if (e_new) {

					BLI_heap_remove(vheap, vnode_top);
					vheap_table[i] = NULL;

					/* update normal */
					if (e_new->l) {
						BMLoop *l_first, *l_iter;
						l_iter = l_first = e_new->l;
						do {
							BM_face_normal_update(l_iter->f);
						} while ((l_iter = l_iter->radial_next) != l_first);

					}

					/* re-calculate costs */
					BM_ITER_ELEM (v_iter, &iter, e_new, BM_VERTS_OF_EDGE) {
						const int j = BM_elem_index_get(v_iter);
						if (j != -1 && vheap_table[j]) {
							const float cost = bm_vert_edge_face_angle(v_iter);
							BLI_heap_remove(vheap, vheap_table[j]);
							vheap_table[j] = BLI_heap_insert(vheap, cost, v_iter);
						}
					}
				}
			}

			if (UNLIKELY(e_new == NULL)) {
				BLI_heap_remove(vheap, vnode_top);
				vheap_table[i] = BLI_heap_insert(vheap, COST_INVALID, v);
			}
		}

		BLI_heap_free(vheap, NULL);
	}

	MEM_freeN(_heap_table);
}

void BM_mesh_decimate_dissolve(BMesh *bm, const float angle_limit, const bool do_dissolve_boundaries,
                               const BMO_Delimit delimit)
{
	int vinput_len;
	int einput_len;

	BMVert **vinput_arr = BM_iter_as_arrayN(bm, BM_VERTS_OF_MESH, NULL, &vinput_len, NULL, 0);
	BMEdge **einput_arr = BM_iter_as_arrayN(bm, BM_EDGES_OF_MESH, NULL, &einput_len, NULL, 0);


	BM_mesh_decimate_dissolve_ex(bm, angle_limit, do_dissolve_boundaries,
	                             delimit,
	                             vinput_arr, vinput_len,
	                             einput_arr, einput_len,
	                             0);

	MEM_freeN(vinput_arr);
	MEM_freeN(einput_arr);
}
