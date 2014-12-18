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
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/tools/bmesh_triangulate.c
 *  \ingroup bmesh
 *
 * Triangulate.
 *
 */

#include "DNA_modifier_types.h"  /* for MOD_TRIANGULATE_NGON_BEAUTY only */

#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"
#include "BLI_alloca.h"
#include "BLI_memarena.h"
#include "BLI_listbase.h"
#include "BLI_heap.h"
#include "BLI_edgehash.h"

/* only for defines */
#include "BLI_polyfill2d.h"
#include "BLI_polyfill2d_beautify.h"

#include "bmesh.h"

#include "bmesh_triangulate.h"  /* own include */

/**
 * a version of #BM_face_triangulate that maps to #BMOpSlot
 */
static void bm_face_triangulate_mapping(
        BMesh *bm, BMFace *face,
        const int quad_method, const int ngon_method,
        const bool use_tag,
        BMOperator *op, BMOpSlot *slot_facemap_out,

        MemArena *pf_arena,
        /* use for MOD_TRIANGULATE_NGON_BEAUTY only! */
        struct Heap *pf_heap, struct EdgeHash *pf_ehash)
{
	int faces_array_tot = face->len - 3;
	BMFace  **faces_array = BLI_array_alloca(faces_array, faces_array_tot);
	BLI_assert(face->len > 3);

	BM_face_triangulate(
	        bm, face, faces_array, &faces_array_tot,
	        quad_method, ngon_method, use_tag,
	        pf_arena,
	        pf_heap, pf_ehash);

	if (faces_array_tot) {
		int i;
		BMO_slot_map_elem_insert(op, slot_facemap_out, face, face);
		for (i = 0; i < faces_array_tot; i++) {
			BMO_slot_map_elem_insert(op, slot_facemap_out, faces_array[i], face);
		}
	}
}


void BM_mesh_triangulate(
        BMesh *bm, const int quad_method, const int ngon_method, const bool tag_only,
        BMOperator *op, BMOpSlot *slot_facemap_out)
{
	BMIter iter;
	BMFace *face;
	MemArena *pf_arena;
	Heap *pf_heap;
	EdgeHash *pf_ehash;

	pf_arena = BLI_memarena_new(BLI_POLYFILL_ARENA_SIZE, __func__);

	if (ngon_method == MOD_TRIANGULATE_NGON_BEAUTY) {
		pf_heap = BLI_heap_new_ex(BLI_POLYFILL_ALLOC_NGON_RESERVE);
		pf_ehash = BLI_edgehash_new_ex(__func__, BLI_POLYFILL_ALLOC_NGON_RESERVE);
	}
	else {
		pf_heap = NULL;
		pf_ehash = NULL;
	}

	if (slot_facemap_out) {
		/* same as below but call: bm_face_triangulate_mapping() */
		BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
			if (face->len > 3) {
				if (tag_only == false || BM_elem_flag_test(face, BM_ELEM_TAG)) {
					bm_face_triangulate_mapping(
					        bm, face, quad_method,
					        ngon_method, tag_only,
					        op, slot_facemap_out,

					        pf_arena,
					        pf_heap, pf_ehash);
				}
			}
		}
	}
	else {
		BM_ITER_MESH (face, &iter, bm, BM_FACES_OF_MESH) {
			if (face->len > 3) {
				if (tag_only == false || BM_elem_flag_test(face, BM_ELEM_TAG)) {
					BM_face_triangulate(
					        bm, face, NULL, NULL,
					        quad_method, ngon_method, tag_only,
					        pf_arena,
					        pf_heap, pf_ehash);
				}
			}
		}
	}

	BLI_memarena_free(pf_arena);

	if (ngon_method == MOD_TRIANGULATE_NGON_BEAUTY) {
		BLI_heap_free(pf_heap, NULL);
		BLI_edgehash_free(pf_ehash, NULL);
	}
}
