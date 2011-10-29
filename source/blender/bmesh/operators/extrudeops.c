#include "MEM_guardedalloc.h"

#include "BLI_utildefines.h"

#include "BLI_ghash.h"
#include "BLI_memarena.h"
#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_array.h"

#include "bmesh.h"
#include "bmesh_private.h"
#include "bmesh_operators_private.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define EXT_INPUT 1
#define EXT_KEEP  2
#define EXT_DEL   4

#define VERT_MARK 1
#define EDGE_MARK 1
#define FACE_MARK 1
#define VERT_NONMAN 2
#define EDGE_NONMAN 2

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

			v = BM_Make_Vert(bm, l->v->co, l->v);

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

		f2 = BM_Make_Ngon(bm, firstv, BM_OtherEdgeVert(edges[0], firstv), edges, f->len, 0);
		if (!f2) {
			BMO_RaiseError(bm, op, BMERR_MESH_ERROR, "Extrude failed; could not create face");
			BLI_array_free(edges);
			return;
		}
		
		BMO_SetFlag(bm, f2, EXT_KEEP);
		BM_Copy_Attributes(bm, bm, f, f2);

		l2 = BMIter_New(&liter2, bm, BM_LOOPS_OF_FACE, f2);
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			BM_Copy_Attributes(bm, bm, l, l2);

			l3 = l->next;
			l4 = l2->next;

			f3 = BM_Make_QuadTri(bm, l3->v, l4->v, l2->v, l->v, f, 0);
			
			BM_Copy_Attributes(bm, bm, l->next, bm_firstfaceloop(f3));
			BM_Copy_Attributes(bm, bm, l->next, bm_firstfaceloop(f3)->next);
			BM_Copy_Attributes(bm, bm, l, bm_firstfaceloop(f3)->next->next);
			BM_Copy_Attributes(bm, bm, l, bm_firstfaceloop(f3)->next->next->next);

			l2 = BMIter_Step(&liter2);
		}
	}

	BLI_array_free(edges);

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

		if (e->l && e->v1 != e->l->v) {
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
		dupev = BM_Make_Vert(bm, v->co, v);

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
	BMEdge *e, *newedge;
	BMLoop *l, *l2;
	BMVert *verts[4], *v, *v2;
	BMFace *f;
	int rlen, found, fwd, delorig=0;

	/*initialize our sub-operators*/
	BMO_Init_Op(&dupeop, "dupe");
	
	BMO_Flag_Buffer(bm, op, "edgefacein", EXT_INPUT, BM_EDGE|BM_FACE);
	
	/*if one flagged face is bordered by an unflagged face, then we delete
	  original geometry unless caller explicitly asked to keep it. */
	if (!BMO_Get_Int(op, "alwayskeeporig")) {
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
	}

	/*calculate verts to delete*/
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		found = 0;

		BM_ITER(e, &viter, bm, BM_EDGES_OF_VERT, v) {
			if (!BMO_TestFlag(bm, e, EXT_INPUT) || !BMO_TestFlag(bm, e, EXT_DEL)){
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

	if (delorig) {
		BMO_InitOpf(bm, &delop, "del geom=%fvef context=%d", 
		            EXT_DEL, DEL_ONLYTAGGED);
	}

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

		/* orient loop to give same normal as a loop of newedge
		if it exists (will be an extruded face),
		else same normal as a loop of e, if it exists */
		if (!newedge->l)
			fwd = !e->l || !(e->l->v == e->v1);
		else
			fwd = (newedge->l->v == newedge->v1);

		
		if (fwd) {
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
			l2 = l->radial_next;
			
			if (l2 == l) {
				l2 = newedge->l;
				BM_Copy_Attributes(bm, bm, l2->f, l->f);

				BM_Copy_Attributes(bm, bm, l2, l);
				l2 = l2->next;
				l = l->next;
				BM_Copy_Attributes(bm, bm, l2, l);
			} else {
				BM_Copy_Attributes(bm, bm, l2->f, l->f);

				/*copy data*/
				if (l2->v == l->v) {
					BM_Copy_Attributes(bm, bm, l2, l);
					l2 = l2->next;
					l = l->next;
					BM_Copy_Attributes(bm, bm, l2, l);
				} else {
					l2 = l2->next;
					BM_Copy_Attributes(bm, bm, l2, l);
					l2 = l2->prev;
					l = l->next;
					BM_Copy_Attributes(bm, bm, l2, l);
				}
			}
		}
	}

	/*link isolated verts*/
	v = BMO_IterNew(&siter, bm, &dupeop, "isovertmap", 0);
	for (; v; v=BMO_IterStep(&siter)) {
		v2 = *((void**)BMO_IterMapVal(&siter));
		BM_Make_Edge(bm, v, v2, v->e, 1);
	}

	/*cleanup*/
	if (delorig) BMO_Finish_Op(bm, &delop);
	BMO_Finish_Op(bm, &dupeop);
}

/*
 * Compute higher-quality vertex normals used by solidify.
 * Only considers geometry in the marked solidify region.
 * Note that this does not work so well for non-manifold
 * regions.
 */
static void calc_solidify_normals(BMesh *bm)
{
	BMIter viter, eiter, fiter;
	BMVert *v;
	BMEdge *e;
	BMFace *f, *f1, *f2;
	float edge_normal[3];
	int i;

	/* Clear indices of verts & edges */
	BM_ITER(v, &viter, bm, BM_VERTS_OF_MESH, NULL) {
		BM_SetIndex(v, 0);
	}
	BM_ITER(e, &eiter, bm, BM_EDGES_OF_MESH, NULL) {
		BM_SetIndex(e, 0);
	}

	BM_ITER(f, &fiter, bm, BM_FACES_OF_MESH, NULL) {
		if (!BMO_TestFlag(bm, f, FACE_MARK)) {
			continue;
		}

		BM_ITER(e, &eiter, bm, BM_EDGES_OF_FACE, f) {
			/* Count number of marked faces using e */
			i = BM_GetIndex(e);
			BM_SetIndex(e, i+1);

			/* And mark all edges and vertices on the
			   marked faces */
			BMO_SetFlag(bm, e, EDGE_MARK);
			BMO_SetFlag(bm, e->v1, VERT_MARK);
			BMO_SetFlag(bm, e->v2, VERT_MARK);
		}
	}

	BM_ITER(e, &eiter, bm, BM_EDGES_OF_MESH, NULL) {
		if (!BMO_TestFlag(bm, e, EDGE_MARK)) {
			continue;
		}

		i = BM_GetIndex(e);

		if (i == 0 || i > 2) {
			/* Edge & vertices are non-manifold even when considering
			   only marked faces */
			BMO_SetFlag(bm, e, EDGE_NONMAN);
			BMO_SetFlag(bm, e->v1, VERT_NONMAN);
			BMO_SetFlag(bm, e->v2, VERT_NONMAN);
		}
	}

	BM_ITER(v, &viter, bm, BM_VERTS_OF_MESH, NULL) {
		if (BM_Nonmanifold_Vert(bm, v)) {
			BMO_SetFlag(bm, v, VERT_NONMAN);
			continue;
		}

		if (BMO_TestFlag(bm, v, VERT_MARK)) {
			zero_v3(v->no);
		}
	}

	BM_ITER(e, &eiter, bm, BM_EDGES_OF_MESH, NULL) {

		/* If the edge is not part of a the solidify region
		   its normal should not be considered */
		if (!BMO_TestFlag(bm, e, EDGE_MARK)) {
			continue;
		}

		/* If the edge joins more than two marked faces high
		   quality normal computation won't work */
		if (BMO_TestFlag(bm, e, EDGE_NONMAN)) {
			continue;
		}

		f1 = f2 = NULL;

		BM_ITER(f, &fiter, bm, BM_FACES_OF_EDGE, e) {
			if (BMO_TestFlag(bm, f, FACE_MARK)) {
				if (f1 == NULL) {
					f1 = f;
				}
				else {
					BLI_assert(f2 == NULL);
					f2 = f;
				}
			}
		}

		BLI_assert(f1 != NULL);

		if (f2 != NULL) {
			const float angle = angle_normalized_v3v3(f1->no, f2->no);

			if (angle > 0.0f) {
				/* two faces using this edge, calculate the edge normal
				 * using the angle between the faces as a weighting */
				add_v3_v3v3(edge_normal, f1->no, f2->no);
				normalize_v3(edge_normal);
				mul_v3_fl(edge_normal, angle);
			}
			else {
				/* can't do anything useful here!
				   Set the face index for a vert incase it gets a zero normal */
				BM_SetIndex(e->v1, -1);
				BM_SetIndex(e->v2, -1);
				continue;
			}
		}
		else {
			/* only one face attached to that edge */
			/* an edge without another attached- the weight on this is
			 * undefined, M_PI/2 is 90d in radians and that seems good enough */
			copy_v3_v3(edge_normal, f1->no);
			mul_v3_fl(edge_normal, M_PI/2);
		}

		add_v3_v3(e->v1->no, edge_normal);
		add_v3_v3(e->v2->no, edge_normal);
	}

	/* normalize accumulated vertex normals*/
	BM_ITER(v, &viter, bm, BM_VERTS_OF_MESH, NULL) {
		if (!BMO_TestFlag(bm, v, VERT_MARK)) {
			continue;
		}

		if (BMO_TestFlag(bm, v, VERT_NONMAN)) {
			/* use standard normals for vertices connected to non-manifold
			   edges */
			BM_Vert_UpdateNormal(bm, v);
		}
		else if (normalize_v3(v->no) == 0.0f && BM_GetIndex(v) < 0) {
			/* exceptional case, totally flat. use the normal
			   of any marked face around the vertex */
			BM_ITER(f, &fiter, bm, BM_FACES_OF_VERT, v) {
				if (BMO_TestFlag(bm, f, FACE_MARK)) {
					break;
				}
			}
			copy_v3_v3(v->no, f->no);
		}	
	}
}

static void solidify_add_thickness(BMesh *bm, float dist)
{
	BMFace *f;
	BMVert *v;
	BMLoop *l;
	BMIter iter, loopIter;
	float *vert_angles = MEM_callocN(sizeof(float) * bm->totvert * 2, "solidify"); /* 2 in 1 */
	float *vert_accum = vert_angles + bm->totvert;
	float angle;
	int i, index;
	float maxdist = dist * sqrtf(3.0f);

	/* array for passing verts to angle_poly_v3 */
	float **verts = NULL;
	BLI_array_staticdeclare(verts, 16);
	/* array for receiving angles from angle_poly_v3 */
	float *angles = NULL;
	BLI_array_staticdeclare(angles, 16);

	i = 0;
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		BM_SetIndex(v, i++);
	}

	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		if(!BMO_TestFlag(bm, f, FACE_MARK)) {
			continue;
		}

		BM_ITER(l, &loopIter, bm, BM_LOOPS_OF_FACE, f) {
			BLI_array_append(verts, l->v->co);
			BLI_array_growone(angles);
		}

		angle_poly_v3(angles, (const float **)verts, f->len);

		i = 0;
		BM_ITER(l, &loopIter, bm, BM_LOOPS_OF_FACE, f) {
			v = l->v;
			index = BM_GetIndex(v);
			angle = angles[i];
			vert_accum[index] += angle;
			vert_angles[index] += shell_angle_to_dist(angle_normalized_v3v3(v->no, f->no)) * angle;
			i++;
		}

		BLI_array_empty(verts);
		BLI_array_empty(angles);
	}

	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		index = BM_GetIndex(v);
		if(vert_accum[index]) { /* zero if unselected */
			float vdist = MIN2(maxdist, dist * vert_angles[index] / vert_accum[index]);
			madd_v3_v3fl(v->co, v->no, vdist);
		}
	}

	MEM_freeN(vert_angles);
}

void bmesh_solidify_face_region_exec(BMesh *bm, BMOperator *op)
{
	BMOperator extrudeop;
	BMOperator reverseop;
	float thickness;
  
	thickness = BMO_Get_Float(op, "thickness");

	/* Flip original faces (so the shell is extruded inward) */
	BMO_Init_Op(&reverseop, "reversefaces");
	BMO_CopySlot(op, &reverseop, "geom", "faces");
	BMO_Exec_Op(bm, &reverseop);
	BMO_Finish_Op(bm, &reverseop);

	/* Extrude the region */
	BMO_InitOpf(bm, &extrudeop, "extrudefaceregion alwayskeeporig=%i", 1);
	BMO_CopySlot(op, &extrudeop, "geom", "edgefacein");
	BMO_Exec_Op(bm, &extrudeop);

	/* Push the verts of the extruded faces inward to create thickness */
	BMO_Flag_Buffer(bm, &extrudeop, "geomout", FACE_MARK, BM_FACE);
	calc_solidify_normals(bm);
	solidify_add_thickness(bm, thickness);

	BMO_CopySlot(&extrudeop, op, "geomout", "geomout");

	BMO_Finish_Op(bm, &extrudeop);
}
