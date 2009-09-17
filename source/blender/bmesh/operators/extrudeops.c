#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_array.h"

#include "bmesh.h"
#include "bmesh_operators_private.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXT_INPUT	1
#define EXT_KEEP	2
#define EXT_DEL		4

void bmesh_extrude_face_indiv_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMIter liter, liter2;
	BMFace *f, *f2, *f3;
	BMLoop *l, *l2, *l3, *l4;
	BMEdge **edges = NULL, *e, *laste;
	BMVert *v, *lastv, *firstv;
	BLI_array_declare(edges);
	int i;

	BMO_ITER(f, &siter, bm, op, "faces", BM_FACE) {
		BLI_array_empty(edges);
		i = 0;
		firstv = lastv = NULL;
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			BLI_array_growone(edges);

			v = BM_Make_Vert(bm, l->v->co, NULL);
			BM_Copy_Attributes(bm, bm, l->v, v);

			if (lastv) {
				e = BM_Make_Edge(bm, lastv, v, l->e, 0);
				edges[i++] = e;
			}

			lastv = v;
			laste = l->e;
			if (!firstv) firstv = v;
		}

		BLI_array_growone(edges);
		e = BM_Make_Edge(bm, v, firstv, laste, 0);
		edges[i++] = e;

		BMO_SetFlag(bm, f, EXT_DEL);

		f2 = BM_Make_Ngon(bm, edges[0]->v1, edges[0]->v2, edges, f->len, 0);
		BMO_SetFlag(bm, f2, EXT_KEEP);
		BM_Copy_Attributes(bm, bm, f, f2);

		l2 = BMIter_New(&liter2, bm, BM_LOOPS_OF_FACE, f2);
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			BM_Copy_Attributes(bm, bm, l, l2);
			l3 = l->head.next;
			l4 = l2->head.next;

			f3 = BM_Make_QuadTri(bm, l3->v, l4->v, l2->v, l->v, f, 0);
			
			BM_Copy_Attributes(bm, bm, l->head.next, f3->loopbase);
			BM_Copy_Attributes(bm, bm, l->head.next, f3->loopbase->head.next);
			BM_Copy_Attributes(bm, bm, l, f3->loopbase->head.next->next);
			BM_Copy_Attributes(bm, bm, l, f3->loopbase->head.next->next->next);

			l2 = BMIter_Step(&liter2);
		}
	}

	BMO_CallOpf(bm, "del geom=%ff context=%d", EXT_DEL, DEL_ONLYFACES);

	BMO_Flag_To_Slot(bm, op, "faceout", EXT_KEEP, BM_FACE);
}

void bmesh_extrude_onlyedge_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMOperator dupeop;
	BMVert *v1, *v2, *v3, *v4;
	BMEdge *e, *e2;
	BMFace *f;
	
	BMO_ITER(e, &siter, bm, op, "edges", BM_EDGE) {
		BMO_SetFlag(bm, e, EXT_INPUT);
		BMO_SetFlag(bm, e->v1, EXT_INPUT);
		BMO_SetFlag(bm, e->v2, EXT_INPUT);
	}

	BMO_InitOpf(bm, &dupeop, "dupe geom=%fve", EXT_INPUT);
	BMO_Exec_Op(bm, &dupeop);

	e = BMO_IterNew(&siter, bm, &dupeop, "boundarymap", 0);
	for (; e; e=BMO_IterStep(&siter)) {
		e2 = BMO_IterMapVal(&siter);
		e2 = *(BMEdge**)e2;

		if (e->loop && e->v1 != e->loop->v) {
			v1 = e->v1;
			v2 = e->v2;
			v3 = e2->v2;
			v4 = e2->v1;
		} else {
			v1 = e2->v1;
			v2 = e2->v2;
			v3 = e->v2;
			v4 = e->v1;
		}
			/*not sure what to do about example face, pass	 NULL for now.*/
		f = BM_Make_QuadTri(bm, v1, v2, v3, v4, NULL, 0);		
		
		if (BMO_TestFlag(bm, e, EXT_INPUT))
			e = e2;
		
		BMO_SetFlag(bm, f, EXT_KEEP);
		BMO_SetFlag(bm, e, EXT_KEEP);
		BMO_SetFlag(bm, e->v1, EXT_KEEP);
		BMO_SetFlag(bm, e->v2, EXT_KEEP);
		
	}

	BMO_Finish_Op(bm, &dupeop);

	BMO_Flag_To_Slot(bm, op, "geomout", EXT_KEEP, BM_ALL);
}

void extrude_vert_indiv_exec(BMesh *bm, BMOperator *op)
{
	BMOIter siter;
	BMVert *v, *dupev;
	BMEdge *e;

	v = BMO_IterNew(&siter, bm, op, "verts", BM_VERT);
	for (; v; v=BMO_IterStep(&siter)) {
		dupev = BM_Make_Vert(bm, v->co, NULL);
		VECCOPY(dupev->no, v->no);
		BM_Copy_Attributes(bm, bm, v, dupev);

		e = BM_Make_Edge(bm, v, dupev, NULL, 0);

		BMO_SetFlag(bm, e, EXT_KEEP);
		BMO_SetFlag(bm, dupev, EXT_KEEP);
	}

	BMO_Flag_To_Slot(bm, op, "vertout", EXT_KEEP, BM_VERT);
	BMO_Flag_To_Slot(bm, op, "edgeout", EXT_KEEP, BM_EDGE);
}

void extrude_edge_context_exec(BMesh *bm, BMOperator *op)
{
	BMOperator dupeop, delop;
	BMOIter siter;
	BMIter iter, fiter, viter;
	BMEdge *e, *newedge, *e2, *ce;
	BMLoop *l, *l2;
	BMVert *verts[4], *v, *v2;
	BMFace *f;
	int rlen, found, delorig=0, i;

	/*initialize our sub-operators*/
	BMO_Init_Op(&dupeop, "dupe");
	
	BMO_Flag_Buffer(bm, op, "edgefacein", EXT_INPUT, BM_EDGE|BM_FACE);
	
	/*if one flagged face is bordered by an unflagged face, then we delete
	  original geometry.*/
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		if (!BMO_TestFlag(bm, e, EXT_INPUT)) continue;

		found = 0;
		f = BMIter_New(&fiter, bm, BM_FACES_OF_EDGE, e);
		for (rlen=0; f; f=BMIter_Step(&fiter), rlen++) {
			if (!BMO_TestFlag(bm, f, EXT_INPUT)) {
				found = 1;
				delorig = 1;
				break;
			}
		}
		
		if (!found && (rlen > 1)) BMO_SetFlag(bm, e, EXT_DEL);
	}

	/*calculate verts to delete*/
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		found = 0;

		BM_ITER(e, &viter, bm, BM_EDGES_OF_VERT, v) {
			if (!BMO_TestFlag(bm, e, EXT_INPUT)) {
				found = 1;
				break;
			}
		}
		
		BM_ITER(f, &viter, bm, BM_FACES_OF_VERT, v) {
			if (!BMO_TestFlag(bm, f, EXT_INPUT)) {
				found = 1;
				break;
			}
		}

		if (!found) {
			BMO_SetFlag(bm, v, EXT_DEL);
		}
	}
	
	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		if (BMO_TestFlag(bm, f, EXT_INPUT))
			BMO_SetFlag(bm, f, EXT_DEL);
	}
	if (delorig) BMO_InitOpf(bm, &delop, "del geom=%fvef context=%d", 
	                         EXT_DEL, DEL_ONLYTAGGED);

	BMO_CopySlot(op, &dupeop, "edgefacein", "geom");
	BMO_Exec_Op(bm, &dupeop);

	if (bm->act_face && BMO_TestFlag(bm, bm->act_face, EXT_INPUT))
		bm->act_face = BMO_Get_MapPointer(bm, &dupeop, "facemap", bm->act_face);

	if (delorig) BMO_Exec_Op(bm, &delop);
	
	/*if not delorig, reverse loops of original faces*/
	if (!delorig) {
		for (f=BMIter_New(&iter, bm, BM_FACES_OF_MESH, NULL); f; f=BMIter_Step(&iter)) {
			if (BMO_TestFlag(bm, f, EXT_INPUT)) {
				BM_flip_normal(bm, f);
			}
		}
	}
	
	BMO_CopySlot(&dupeop, op, "newout", "geomout");
	e = BMO_IterNew(&siter, bm, &dupeop, "boundarymap", 0);
	for (; e; e=BMO_IterStep(&siter)) {
		if (BMO_InMap(bm, op, "exclude", e)) continue;

		newedge = BMO_IterMapVal(&siter);
		newedge = *(BMEdge**)newedge;
		if (!newedge) continue;
		if (!newedge->loop) ce = e;
		else ce = newedge;
		
		if (ce->loop && (ce->loop->v == ce->v1)) {
			verts[0] = e->v1;
			verts[1] = e->v2;
			verts[2] = newedge->v2;
			verts[3] = newedge->v1;
		} else {
			verts[3] = e->v1;
			verts[2] = e->v2;
			verts[1] = newedge->v2;
			verts[0] = newedge->v1;
		}

		/*not sure what to do about example face, pass NULL for now.*/
		f = BM_Make_Quadtriangle(bm, verts, NULL, 4, NULL, 0);		

		/*copy attributes*/
		l=BMIter_New(&iter, bm, BM_LOOPS_OF_FACE, f);
		for (; l; l=BMIter_Step(&iter)) {
			if (l->e != e && l->e != newedge) continue;
			l2 = l->radial.next->data;
			
			if (l2 == l) {
				l2 = newedge->loop;
				BM_Copy_Attributes(bm, bm, l2->f, l->f);

				BM_Copy_Attributes(bm, bm, l2, l);
				l2 = (BMLoop*) l2->head.next;
				l = (BMLoop*) l->head.next;
				BM_Copy_Attributes(bm, bm, l2, l);
			} else {
				BM_Copy_Attributes(bm, bm, l2->f, l->f);

				/*copy data*/
				if (l2->v == l->v) {
					BM_Copy_Attributes(bm, bm, l2, l);
					l2 = (BMLoop*) l2->head.next;
					l = (BMLoop*) l->head.next;
					BM_Copy_Attributes(bm, bm, l2, l);
				} else {
					l2 = (BMLoop*) l2->head.next;
					BM_Copy_Attributes(bm, bm, l2, l);
					l2 = (BMLoop*) l2->head.prev;
					l = (BMLoop*) l->head.next;
					BM_Copy_Attributes(bm, bm, l2, l);
				}
			}
		}
	}

	/*link isolated verts*/
	v = BMO_IterNew(&siter, bm, &dupeop, "isovertmap", 0);
	for (; v; v=BMO_IterStep(&siter)) {
		v2 = *((void**)BMO_IterMapVal(&siter));
		BM_Make_Edge(bm, v, v2, v->edge, 1);
	}

	/*cleanup*/
	if (delorig) BMO_Finish_Op(bm, &delop);
	BMO_Finish_Op(bm, &dupeop);
}
