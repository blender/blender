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
 * Contributor(s): Joseph Eagar.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/bmesh/operators/bmo_subdivide.c
 *  \ingroup bmesh
 */

#include "MEM_guardedalloc.h"
#include "DNA_listBase.h"

#include "BLI_math.h"
#include "BLI_array.h"
#include "BLI_smallhash.h"
#include "BLI_scanfill.h"

#include "bmesh.h"
#include "intern/bmesh_private.h"

#include "intern/bmesh_operators_private.h" /* own include */

#define EDGE_NEW	1
#define FACE_NEW	1

#define ELE_NEW		1
#define FACE_MARK	2
#define EDGE_MARK	4

void bmo_triangulate_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMFace *face, **newfaces = NULL;
	BLI_array_declare(newfaces);
	float (*projectverts)[3] = NULL;
	BLI_array_declare(projectverts);
	int i, lastlen = 0 /* , count = 0 */;
	const int use_beauty = BMO_slot_bool_get(op, "use_beauty");

	face = BMO_iter_new(&siter, bm, op, "faces", BM_FACE);
	for ( ; face; face = BMO_iter_step(&siter)) {
		if (lastlen < face->len) {
			BLI_array_empty(projectverts);
			BLI_array_empty(newfaces);
			for (lastlen = 0; lastlen < face->len; lastlen++) {
				BLI_array_growone(projectverts);
				BLI_array_growone(projectverts);
				BLI_array_growone(projectverts);
				BLI_array_growone(newfaces);
			}
		}

		BM_face_triangulate(bm, face, projectverts, EDGE_NEW, FACE_NEW, newfaces, use_beauty);

		BMO_slot_map_ptr_insert(bm, op, "facemap", face, face);
		for (i = 0; newfaces[i]; i++) {
			BMO_slot_map_ptr_insert(bm, op, "facemap",
			                        newfaces[i], face);

		}
	}
	
	BMO_slot_buffer_from_enabled_flag(bm, op, "edgeout", BM_EDGE, EDGE_NEW);
	BMO_slot_buffer_from_enabled_flag(bm, op, "faceout", BM_FACE, FACE_NEW);
	
	BLI_array_free(projectverts);
	BLI_array_free(newfaces);
}

void bmo_beautify_fill_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter iter;
	BMFace *f;
	BMEdge *e;
	int stop = 0;
	
	BMO_slot_buffer_flag_enable(bm, op, "constrain_edges", BM_EDGE, EDGE_MARK);
	
	BMO_ITER (f, &siter, bm, op, "faces", BM_FACE) {
		if (f->len == 3) {
			BMO_elem_flag_enable(bm, f, FACE_MARK);
		}
	}

	while (!stop) {
		stop = 1;
		
		BM_ITER (e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			BMVert *v1, *v2, *v3, *v4;
			
			if (!BM_edge_is_manifold(e) || BMO_elem_flag_test(bm, e, EDGE_MARK)) {
				continue;
			}

			if (!BMO_elem_flag_test(bm, e->l->f, FACE_MARK) ||
			    !BMO_elem_flag_test(bm, e->l->radial_next->f, FACE_MARK))
			{
				continue;
			}
			
			v1 = e->l->prev->v;
			v2 = e->l->v;
			v3 = e->l->radial_next->prev->v;
			v4 = e->l->next->v;
			
			if (is_quad_convex_v3(v1->co, v2->co, v3->co, v4->co)) {
				float len1, len2, len3, len4, len5, len6, opp1, opp2, fac1, fac2;
				/* testing rule:
				 * the area divided by the total edge lengths
				 */
				len1 = len_v3v3(v1->co, v2->co);
				len2 = len_v3v3(v2->co, v3->co);
				len3 = len_v3v3(v3->co, v4->co);
				len4 = len_v3v3(v4->co, v1->co);
				len5 = len_v3v3(v1->co, v3->co);
				len6 = len_v3v3(v2->co, v4->co);

				opp1 = area_tri_v3(v1->co, v2->co, v3->co);
				opp2 = area_tri_v3(v1->co, v3->co, v4->co);

				fac1 = opp1 / (len1 + len2 + len5) + opp2 / (len3 + len4 + len5);

				opp1 = area_tri_v3(v2->co, v3->co, v4->co);
				opp2 = area_tri_v3(v2->co, v4->co, v1->co);

				fac2 = opp1 / (len2 + len3 + len6) + opp2 / (len4 + len1 + len6);
				
				if (fac1 > fac2) {
					e = BM_edge_rotate(bm, e, FALSE, BM_EDGEROT_CHECK_EXISTS);
					if (e) {
						BMO_elem_flag_enable(bm, e, ELE_NEW);

						BMO_elem_flag_enable(bm, e->l->f, FACE_MARK|ELE_NEW);
						BMO_elem_flag_enable(bm, e->l->radial_next->f, FACE_MARK|ELE_NEW);
						stop = 0;
					}
				}
			}
		}
	}
	
	BMO_slot_buffer_from_enabled_flag(bm, op, "geomout", BM_EDGE|BM_FACE, ELE_NEW);
}

void bmo_triangle_fill_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMEdge *e;
	BMOperator bmop;
	ScanFillContext sf_ctx;
	/* ScanFillEdge *eed; */ /* UNUSED */
	ScanFillVert *eve, *v1, *v2;
	ScanFillFace *efa;
	SmallHash hash;

	BLI_smallhash_init(&hash);
	
	BLI_begin_edgefill(&sf_ctx);
	
	BMO_ITER (e, &siter, bm, op, "edges", BM_EDGE) {
		BMO_elem_flag_enable(bm, e, EDGE_MARK);
		
		if (!BLI_smallhash_haskey(&hash, (uintptr_t)e->v1)) {
			eve = BLI_addfillvert(&sf_ctx, e->v1->co);
			eve->tmp.p = e->v1;
			BLI_smallhash_insert(&hash, (uintptr_t)e->v1, eve);
		}
		
		if (!BLI_smallhash_haskey(&hash, (uintptr_t)e->v2)) {
			eve = BLI_addfillvert(&sf_ctx, e->v2->co);
			eve->tmp.p = e->v2;
			BLI_smallhash_insert(&hash, (uintptr_t)e->v2, eve);
		}
		
		v1 = BLI_smallhash_lookup(&hash, (uintptr_t)e->v1);
		v2 = BLI_smallhash_lookup(&hash, (uintptr_t)e->v2);
		/* eed = */ BLI_addfilledge(&sf_ctx, v1, v2);
		/* eed->tmp.p = e; */ /* UNUSED */
	}
	
	BLI_edgefill(&sf_ctx, FALSE);
	
	for (efa = sf_ctx.fillfacebase.first; efa; efa = efa->next) {
		BMFace *f = BM_face_create_quad_tri(bm,
		                                    efa->v1->tmp.p, efa->v2->tmp.p, efa->v3->tmp.p, NULL,
		                                    NULL, TRUE);
		BMLoop *l;
		BMIter liter;
		
		BMO_elem_flag_enable(bm, f, ELE_NEW);
		BM_ITER (l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			if (!BMO_elem_flag_test(bm, l->e, EDGE_MARK)) {
				BMO_elem_flag_enable(bm, l->e, ELE_NEW);
			}
		}
	}
	
	BLI_end_edgefill(&sf_ctx);
	BLI_smallhash_release(&hash);
	
	/* clean up fill */
	BMO_op_initf(bm, &bmop, "beautify_fill faces=%ff constrain_edges=%fe", ELE_NEW, EDGE_MARK);
	BMO_op_exec(bm, &bmop);
	BMO_slot_buffer_flag_enable(bm, &bmop, "geomout", BM_FACE|BM_EDGE, ELE_NEW);
	BMO_op_finish(bm, &bmop);
	
	BMO_slot_buffer_from_enabled_flag(bm, op, "geomout", BM_EDGE|BM_FACE, ELE_NEW);
}
