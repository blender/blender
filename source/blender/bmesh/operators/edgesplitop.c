/**
 * $Id:
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.	
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): Joseph Eagar
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"
#include "BLI_array.h"

#include "DNA_object_types.h"

#include "ED_mesh.h"

#include "bmesh.h"
#include "mesh_intern.h"
#include "subdivideop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct EdgeTag {
	BMVert *newv1, *newv2;
	BMEdge *newe1, *newe2;
	int tag;
} EdgeTag;

#define EDGE_SEAM	1
#define EDGE_DEL	2
#define EDGE_MARK	4
#define EDGE_RET1	8
#define EDGE_RET2	16

#define FACE_DEL	1
#define FACE_NEW	2

static BMFace *remake_face(BMesh *bm, EdgeTag *etags, BMFace *f, BMVert **verts)
{
	BMIter liter1, liter2;
	EdgeTag *et;
	BMFace *f2;
	BMLoop *l, *l2;
	BMEdge **edges = (BMEdge**) verts; /*he he, can reuse this, sneaky! ;)*/
	BMVert *lastv1, *lastv2, *v1, *v2;
	int i;

	/*we do final edge last*/
	lastv1 = verts[f->len-1];
	lastv2 = verts[0];
	v1 = verts[0];
	v2 = verts[1];
	for (i=0; i<f->len-1; i++) {
		edges[i] = BM_Make_Edge(bm, verts[i], verts[i+1], NULL, 1);

		if (!edges[i])
			return NULL;
	}
	
	edges[i] = BM_Make_Edge(bm, lastv1, lastv2, NULL, 1);

	f2 = BM_Make_Ngon(bm, v1, v2, edges, f->len, 0);
	if (!f2)
		return NULL;
	
	BM_Copy_Attributes(bm, bm, f, f2);

	l = BMIter_New(&liter1, bm, BM_LOOPS_OF_FACE, f);
	l2 = BMIter_New(&liter2, bm, BM_LOOPS_OF_FACE, f2);
	for (; l && l2; l=BMIter_Step(&liter1), l2=BMIter_Step(&liter2)) {
		BM_Copy_Attributes(bm, bm, l, l2);
		if (l->e != l2->e) {
			/*set up data for figuring out the two sides of
			  the splits*/
			BMINDEX_SET(l2->e, BMINDEX_GET(l->e));
			et = etags + BMINDEX_GET(l->e);
			
			if (!et->newe1) et->newe1 = l2->e;
			else et->newe2 = l2->e;

			if (BMO_TestFlag(bm, l->e, EDGE_SEAM))
				BMO_SetFlag(bm, l2->e, EDGE_SEAM);

			BM_Copy_Attributes(bm, bm, l->e, l2->e);
		}

		BMO_SetFlag(bm, l->e, EDGE_MARK);
		BMO_SetFlag(bm, l2->e, EDGE_MARK);
	}

	return f2;
}

void tag_out_edges(BMesh *bm, EdgeTag *etags, BMOperator *op)
{
	EdgeTag *et;
	BMIter iter;
	BMLoop *l, *startl;
	BMEdge *e;
	BMVert *v;
	int i;

	while (1) {
		BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
			if (!BMO_TestFlag(bm, e, EDGE_SEAM))
				continue;

			et = etags + BMINDEX_GET(e);
			if (!et->tag && e->loop) {
				break;
			}
		}
		
		if (!e)
			break;

		/*ok we found an edge, part of a region of splits we need
		  to identify.  now walk along it.*/
		for (i=0; i<2; i++) {
			l = e->loop;
			
			v = i ? ((BMLoop*)l->head.next)->v : l->v;

			while (1) {
				et = etags + BMINDEX_GET(l->e);
				if (et->newe1 == l->e) {
					if (et->newe1) {
						BMO_SetFlag(bm, et->newe1, EDGE_RET1);
						BMO_ClearFlag(bm, et->newe1, EDGE_SEAM);
					}
					if (et->newe2) {
						BMO_SetFlag(bm, et->newe2, EDGE_RET2);
						BMO_ClearFlag(bm, et->newe2, EDGE_SEAM);
					}
				} else {
					if (et->newe1) {
						BMO_SetFlag(bm, et->newe1, EDGE_RET2);
						BMO_ClearFlag(bm, et->newe1, EDGE_SEAM);
					}
					if (et->newe2) {
						BMO_SetFlag(bm, et->newe2, EDGE_RET1);
						BMO_ClearFlag(bm, et->newe2, EDGE_SEAM);
					}
				}

				startl = l;
				do {
					l = BM_OtherFaceLoop(l->e, l->f, v);
					if (BM_Edge_FaceCount(l->e) != 2)
						break;
					l = (BMLoop*) l->radial.next->data;
				} while (l != startl && !BMO_TestFlag(bm, l->e, EDGE_SEAM));
				
				if (l == startl || !BMO_TestFlag(bm, l->e, EDGE_SEAM))
					break;

				if (l->v == v) {
					v = ((BMLoop*)l->head.next)->v;
				} else v = l->v;
			}
		}
	}
}

void bmesh_edgesplitop_exec(BMesh *bm, BMOperator *op)
{
	EdgeTag *etags, *et;
	BMIter iter, liter;
	BMOIter siter;
	BMFace *f, *f2;
	BMLoop *l, *nextl, *prevl, *l2, *l3;
	BMEdge *e, *e2;
	BLI_array_declare(verts);
	BMVert *v, *v2, **verts = NULL;
	int i, j;

	BMO_Flag_Buffer(bm, op, "edges", EDGE_SEAM, BM_EDGE);
	
	/*single marked edges unconnected to any other marked edges
	  are illegal, go through and unmark them*/
	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		for (i=0; i<2; i++) {
			BM_ITER(e2, &iter, bm, BM_EDGES_OF_VERT, i ? e->v2 : e->v1) {
				if (e != e2 && BMO_TestFlag(bm, e2, EDGE_SEAM))
					break;
			}
			if (e2)
				break;
		}
		if (!e2)
			BMO_ClearFlag(bm, e, EDGE_SEAM);
	}

	etags = MEM_callocN(sizeof(EdgeTag)*bm->totedge, "EdgeTag");
	
	i = 0;
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		BMINDEX_SET(e, i);
		i++;
	}

#ifdef ETV
#undef ETV
#endif
#ifdef SETETV
#undef SETETV
#endif

#define ETV(et, v, l) (l->e->v1 == v ? et->newv1 : et->newv2)
#define SETETV(et, v, l, vs) l->e->v1 == v ? (et->newv1 = vs) : (et->newv2 = vs)

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		if (BMO_TestFlag(bm, f, FACE_NEW))
			continue;
		
		BLI_array_empty(verts);
		BLI_array_growitems(verts, f->len);
		memset(verts, 0, sizeof(BMVert*)*f->len);
		
		i = 0;
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			if (!BMO_TestFlag(bm, l->e, EDGE_SEAM)) {
				if (!verts[i]) {
					et = etags + BMINDEX_GET(l->e);
					if (ETV(et, l->v, l))
						verts[i] = ETV(et, l->v, l);
					else verts[i] = l->v;
				}
				i++;
				continue;
			}

			BMO_SetFlag(bm, l->e, EDGE_DEL);

			nextl = (BMLoop*) l->head.next;
			prevl = (BMLoop*) l->head.prev;
			
			for (j=0; j<2; j++) {
				l2 = j ? nextl : prevl;
				v = j ? l2->v : l->v;

				if (BMO_TestFlag(bm, l2->e, EDGE_SEAM)) {
					if (!verts[j ? (i+1) % f->len : i]) {
						/*make unique vert here for this face only*/
						v2 = BM_Make_Vert(bm, v->co, NULL);
						VECCOPY(v2->no, v->no);
						BM_Copy_Attributes(bm, bm, v, v2);

						verts[j ? (i+1) % f->len : i] = v2;
					} else v2 = verts[j ? (i+1) % f->len : i];
				} else {
					/*generate unique vert for non-seam edge(s)
					  around the manifold vert fan if necassary*/

					/*first check that we have two seam edges
					  somewhere within this fan*/
					l3 = l2;
					do {
						if (BM_Edge_FaceCount(l3->e) != 2) {
							/*if we hit a boundary edge, tag
							  l3 as null so we know to disconnect
							  it*/
							if (BM_Edge_FaceCount(l3->e) == 1)
								l3 = NULL;
							break;
						}

						l3 = (BMLoop*)l3->radial.next->data;
						l3 = BM_OtherFaceLoop(l3->e, l3->f, v);
					} while (l3 != l2 && !BMO_TestFlag(bm, l3->e, EDGE_SEAM));

					if (l3 == NULL || (BMO_TestFlag(bm, l3->e, EDGE_SEAM) && l3->e != l->e)) {
						et = etags + BMINDEX_GET(l2->e);
						if (ETV(et, v, l2) == NULL) {
							v2 = BM_Make_Vert(bm, v->co, NULL);
							VECCOPY(v2->no, v->no);
							BM_Copy_Attributes(bm, bm, v, v2);
							
							l3 = l2;
							do {
								SETETV(et, v, l3, v2);
								if (BM_Edge_FaceCount(l3->e) != 2)
									break;

								l3 = (BMLoop*)l3->radial.next->data;
								l3 = BM_OtherFaceLoop(l3->e, l3->f, v);
								
								et = etags + BMINDEX_GET(l3->e);
							} while (l3 != l2 && !BMO_TestFlag(bm, l3->e, EDGE_SEAM));
						} else v2 = ETV(et, v, l2);

						verts[j ? (i+1) % f->len : i] = v2;
					} else verts[j ? (i+1) % f->len : i] = v;
				}
			}

			i++;
		}

		f2 = remake_face(bm, etags, f, verts);
		if (!f2)
			continue;

		BMO_SetFlag(bm, f, FACE_DEL);
		BMO_SetFlag(bm, f2, FACE_NEW);
	}
	
	BMO_CallOpf(bm, "del geom=%ff context=%i", FACE_DEL, DEL_ONLYFACES);

	/*test EDGE_MARK'd edges if we need to delete them, EDGE_MARK
	  is set in remake_face*/
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if (BMO_TestFlag(bm, e, EDGE_MARK)) {
			if (!e->loop)
				BMO_SetFlag(bm, e, EDGE_DEL);
		}
	}

	BMO_CallOpf(bm, "del geom=%fe context=%i", EDGE_DEL, DEL_EDGES);
	
	tag_out_edges(bm, etags, op);
	BMO_Flag_To_Slot(bm, op, "edgeout1", EDGE_RET1, BM_EDGE);
	BMO_Flag_To_Slot(bm, op, "edgeout2", EDGE_RET2, BM_EDGE);

	BLI_array_free(verts);
	if (etags) MEM_freeN(etags);
}

#undef ETV
#undef SETETV
