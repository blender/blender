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

#include <string.h> /* for memcpy */

#include "MEM_guardedalloc.h"

#include "BLI_array.h"

#include "bmesh.h"

#include "bmesh_operators_private.h" /* own include */

typedef struct EdgeTag {
	BMVert *newv1, *newv2;
	BMEdge *newe1, *newe2;
	int tag;
} EdgeTag;

/* (EDGE_DEL == FACE_DEL) - this must be the case */
#define EDGE_DEL	1
#define EDGE_SEAM	2
#define EDGE_MARK	4
#define EDGE_RET1	8
#define EDGE_RET2	16

#define FACE_DEL	1
#define FACE_NEW	2

static BMFace *remake_face(BMesh *bm, EdgeTag *etags, BMFace *f, BMVert **f_verts, BMEdge **edges_tmp)
{
	BMIter liter1, liter2;
	EdgeTag *et;
	BMFace *f2;
	BMLoop *l, *l2;
	BMEdge *e;
	BMVert *lastv1, *lastv2 /* , *v1, *v2 */ /* UNUSED */;
	int i;

	/* we do final edge last */
	lastv1 = f_verts[f->len - 1];
	lastv2 = f_verts[0];
	/* v1 = f_verts[0]; */ /* UNUSED */
	/* v2 = f_verts[1]; */ /* UNUSED */
	for (i = 0; i < f->len - 1; i++) {
		e = BM_edge_create(bm, f_verts[i], f_verts[i + 1], NULL, TRUE);
		if (!e) {
			return NULL;
		}
		edges_tmp[i] = e;
	}

	edges_tmp[i] = BM_edge_create(bm, lastv1, lastv2, NULL, TRUE);

	f2 = BM_face_create(bm, f_verts, edges_tmp, f->len, FALSE);
	if (!f2) {
		return NULL;
	}

	BM_elem_attrs_copy(bm, bm, f, f2);

	l = BM_iter_new(&liter1, bm, BM_LOOPS_OF_FACE, f);
	l2 = BM_iter_new(&liter2, bm, BM_LOOPS_OF_FACE, f2);
	for ( ; l && l2; l = BM_iter_step(&liter1), l2 = BM_iter_step(&liter2)) {
		BM_elem_attrs_copy(bm, bm, l, l2);
		if (l->e != l2->e) {
			/* set up data for figuring out the two sides of
			 * the split */

			/* set edges index as dirty after running all */
			BM_elem_index_set(l2->e, BM_elem_index_get(l->e)); /* set_dirty! */
			et = &etags[BM_elem_index_get(l->e)];
			
			if (!et->newe1) {
				et->newe1 = l2->e;
			}
			else if (!et->newe2) {
				et->newe2 = l2->e;
			}
			else {
				/* Only two new edges should be created from each original edge
				 *  for edge split operation */

				//BLI_assert(et->newe1 == l2->e || et->newe2 == l2->e);
				et->newe2 = l2->e;
			}

			if (BMO_elem_flag_test(bm, l->e, EDGE_SEAM)) {
				BMO_elem_flag_enable(bm, l2->e, EDGE_SEAM);
			}

			BM_elem_attrs_copy(bm, bm, l->e, l2->e);
		}

		BMO_elem_flag_enable(bm, l->e, EDGE_MARK);
		BMO_elem_flag_enable(bm, l2->e, EDGE_MARK);
	}

	return f2;
}

static void tag_out_edges(BMesh *bm, EdgeTag *etags, BMOperator *UNUSED(op))
{
	EdgeTag *et;
	BMIter iter;
	BMLoop *l, *l_start, *l_prev;
	BMEdge *e;
	BMVert *v;
	int i, ok;
	
	ok = 0;
	while (ok++ < 100000) {
		BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			if (!BMO_elem_flag_test(bm, e, EDGE_SEAM))
				continue;

			et = &etags[BM_elem_index_get(e)];
			if (!et->tag && e->l) {
				break;
			}
		}
		
		if (!e) {
			break;
		}

		/* ok we found an edge, part of a region of splits we need
		 * to identify.  now walk along it */
		for (i = 0; i < 2; i++) {
			l = e->l;
			
			v = i ? l->next->v : l->v;

			while (1) {
				et = &etags[BM_elem_index_get(l->e)];
				if (et->newe1 == l->e) {
					if (et->newe1) {
						BMO_elem_flag_enable(bm, et->newe1, EDGE_RET1);
						BMO_elem_flag_disable(bm, et->newe1, EDGE_SEAM);
					}
					if (et->newe2) {
						BMO_elem_flag_enable(bm, et->newe2, EDGE_RET2);
						BMO_elem_flag_disable(bm, et->newe2, EDGE_SEAM);
					}
				}
				else {
					if (et->newe1) {
						BMO_elem_flag_enable(bm, et->newe1, EDGE_RET2);
						BMO_elem_flag_disable(bm, et->newe1, EDGE_SEAM);
					}
					if (et->newe2) {
						BMO_elem_flag_enable(bm, et->newe2, EDGE_RET1);
						BMO_elem_flag_disable(bm, et->newe2, EDGE_SEAM);
					}
				}

				/* If the original edge was non-manifold edges, then it is
				 * possible l->e is not et->newe1 or et->newe2. So always clear
				 * the flag on l->e as well, to prevent infinite looping. */
				BMO_elem_flag_disable(bm, l->e, EDGE_SEAM);
				l_start = l;

				do {
					/* l_prev checks stops us from looping over the same edge forever [#30459] */
					l_prev = l;
					l = BM_face_other_edge_loop(l->f, l->e, v);
					if (l == l_start || BM_edge_face_count(l->e) != 2) {
						break;
					}
					l = l->radial_next;
				} while (l != l_start && l != l_prev && !BMO_elem_flag_test(bm, l->e, EDGE_SEAM));
				
				if (l == l_start || !BMO_elem_flag_test(bm, l->e, EDGE_SEAM)) {
					break;
				}

				v = (l->v == v) ? l->next->v : l->v;
			}
		}
	}
}

/* helper functions for edge tag's */
BM_INLINE BMVert *bm_edge_tag_vert_get(EdgeTag *et, BMVert *v, BMLoop *l)
{
	return (l->e->v1 == v) ? et->newv1 : et->newv2;
}

BM_INLINE void bm_edge_tag_vert_set(EdgeTag *et, BMVert *v, BMLoop *l, BMVert *vset)
{
	if (l->e->v1 == v) {
		et->newv1 = vset;
	}
	else {
		et->newv2 = vset;
	}
}

void bmo_edgesplit_exec(BMesh *bm, BMOperator *op)
{
	EdgeTag *etags, *et; /* edge aligned array of tags */
	BMIter iter, liter;
	BMOIter siter;
	BMFace *f, *f2;
	BMLoop *l, *l2, *l3;
	BMLoop *l_next, *l_prev;
	BMEdge *e;
	BMVert *v, *v2;

	/* face/vert aligned vert array */
	BMVert **f_verts = NULL;
	BLI_array_declare(f_verts);

	BMEdge **edges_tmp = NULL;
	BLI_array_declare(edges_tmp);
	int i, j;

	BMO_slot_buffer_flag_enable(bm, op, "edges", EDGE_SEAM, BM_EDGE);

	/* untag edges not connected to other tagged edges */
	{
		unsigned char *vtouch;

		BM_mesh_elem_index_ensure(bm, BM_VERT);

		vtouch = MEM_callocN(sizeof(char) * bm->totvert, __func__);

		/* single marked edges unconnected to any other marked edges
		 * are illegal, go through and unmark them */
		BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
			/* lame, but we dont want the count to exceed 255,
			 * so just count to 2, its all we need */
			unsigned char *c;
			c = &vtouch[BM_elem_index_get(e->v1)]; if (*c < 2) (*c)++;
			c = &vtouch[BM_elem_index_get(e->v2)]; if (*c < 2) (*c)++;
		}
		BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
			if (vtouch[BM_elem_index_get(e->v1)] == 1 &&
			    vtouch[BM_elem_index_get(e->v2)] == 1)
			{
				BMO_elem_flag_disable(bm, e, EDGE_SEAM);
			}
		}

		MEM_freeN(vtouch);
	}

	etags = MEM_callocN(sizeof(EdgeTag) * bm->totedge, "EdgeTag");

	BM_mesh_elem_index_ensure(bm, BM_EDGE);

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {

		if (BMO_elem_flag_test(bm, f, FACE_NEW)) {
			continue;
		}

		BLI_array_empty(f_verts);
		BLI_array_growitems(f_verts, f->len);
		memset(f_verts, 0, sizeof(BMVert *) * f->len);

		/* this is passed onto remake_face() so it doesnt need to allocate
		 * a new array on each call. */
		BLI_array_empty(edges_tmp);
		BLI_array_growitems(edges_tmp, f->len);

		i = 0;
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			if (!BMO_elem_flag_test(bm, l->e, EDGE_SEAM)) {
				if (!f_verts[i]) {

					et = &etags[BM_elem_index_get(l->e)];
					if (bm_edge_tag_vert_get(et, l->v, l)) {
						f_verts[i] = bm_edge_tag_vert_get(et, l->v, l);
					}
					else {
						f_verts[i] = l->v;
					}
				}
				i++;
				continue;
			}

			l_next = l->next;
			l_prev = l->prev;
			
			for (j = 0; j < 2; j++) {
				/* correct as long as i & j dont change during the loop */
				const int fv_index = j ? (i + 1) % f->len : i; /* face vert index */
				l2 = j ? l_next : l_prev;
				v = j ? l2->v : l->v;

				if (BMO_elem_flag_test(bm, l2->e, EDGE_SEAM)) {
					if (f_verts[fv_index] == NULL) {
						/* make unique vert here for this face only */
						v2 = BM_vert_create(bm, v->co, v);
						f_verts[fv_index] = v2;
					}
					else {
						v2 = f_verts[fv_index];
					}
				}
				else {
					/* generate unique vert for non-seam edge(s)
					 * around the manifold vert fan if necessary */

					/* first check that we have two seam edges
					 * somewhere within this fa */
					l3 = l2;
					do {
						if (BM_edge_face_count(l3->e) != 2) {
							/* if we hit a boundary edge, tag
							 * l3 as null so we know to disconnect
							 * it */
							if (BM_edge_face_count(l3->e) == 1) {
								l3 = NULL;
							}
							break;
						}

						l3 = l3->radial_next;
						l3 = BM_face_other_edge_loop(l3->f, l3->e, v);
					} while (l3 != l2 && !BMO_elem_flag_test(bm, l3->e, EDGE_SEAM));

					if (l3 == NULL || (BMO_elem_flag_test(bm, l3->e, EDGE_SEAM) && l3->e != l->e)) {
						et = &etags[BM_elem_index_get(l2->e)];
						if (bm_edge_tag_vert_get(et, v, l2) == NULL) {
							v2 = BM_vert_create(bm, v->co, v);
							
							l3 = l2;
							do {
								bm_edge_tag_vert_set(et, v, l3, v2);
								if (BM_edge_face_count(l3->e) != 2) {
									break;
								}

								l3 = l3->radial_next;
								l3 = BM_face_other_edge_loop(l3->f, l3->e, v);
								
								et = &etags[BM_elem_index_get(l3->e)];
							} while (l3 != l2 && !BMO_elem_flag_test(bm, l3->e, EDGE_SEAM));
						}
						else {
							v2 = bm_edge_tag_vert_get(et, v, l2);
						}

						f_verts[fv_index] = v2;
					}
					else {
						f_verts[fv_index] = v;
					}
				}
			}

			i++;
		}

		/* debugging code, quick way to find the face/vert combination
		 * which is failing assuming quads start planer - campbell */
#if 0
		if (f->len == 4) {
			float no1[3];
			float no2[3];
			float angle_error;
			printf(" ** found QUAD\n");
			normal_tri_v3(no1, f_verts[0]->co, f_verts[1]->co, f_verts[2]->co);
			normal_tri_v3(no2, f_verts[0]->co, f_verts[2]->co, f_verts[3]->co);
			if ((angle_error = angle_v3v3(no1, no2)) > 0.05) {
				printf("     ERROR %.4f\n", angle_error);
				print_v3("0", f_verts[0]->co);
				print_v3("1", f_verts[1]->co);
				print_v3("2", f_verts[2]->co);
				print_v3("3", f_verts[3]->co);

			}
		}
		else {
			printf(" ** fount %d len face\n", f->len);
		}
#endif

		f2 = remake_face(bm, etags, f, f_verts, edges_tmp);
		if (f2) {
			BMO_elem_flag_enable(bm, f, FACE_DEL);
			BMO_elem_flag_enable(bm, f2, FACE_NEW);
		}
		/* else { ... should we raise an error here, or an assert? - campbell */
	}
	
	/* remake_face() sets invalid indices,
	 * likely these will be corrected on operator exit anyway */
	bm->elem_index_dirty &= ~BM_EDGE;

	/* cant call the operator because 'tag_out_edges'
	 * relies on original index values, from before editing geometry */

#if 0
	BMO_op_callf(bm, "del geom=%ff context=%i", FACE_DEL, DEL_ONLYFACES);
#else
	BMO_remove_tagged_context(bm, FACE_DEL, DEL_ONLYFACES);
#endif

	/* test EDGE_MARK'd edges if we need to delete them, EDGE_MARK
	 * is set in remake_face */
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if (BMO_elem_flag_test(bm, e, EDGE_MARK)) {
			if (!e->l) {
				BMO_elem_flag_enable(bm, e, EDGE_DEL);
			}
		}
	}

#if 0
	BMO_op_callf(bm, "del geom=%fe context=%i", EDGE_DEL, DEL_EDGES);
#else
	BMO_remove_tagged_context(bm, EDGE_DEL, DEL_EDGES);
#endif
	
	tag_out_edges(bm, etags, op);
	BMO_slot_buffer_from_flag(bm, op, "edgeout1", EDGE_RET1, BM_EDGE);
	BMO_slot_buffer_from_flag(bm, op, "edgeout2", EDGE_RET2, BM_EDGE);

	BLI_array_free(f_verts);
	BLI_array_free(edges_tmp);
	if (etags) MEM_freeN(etags);
}
