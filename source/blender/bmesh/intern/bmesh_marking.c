#include <string.h>
#include "bmesh.h"
#include "bmesh_private.h"


/*
 * BM_MARK.C
 *
 * Selection routines for bmesh structures.
 * This is actually all old code ripped from
 * editmesh_lib.c and slightly modified to work
 * for bmesh's. This also means that it has some
 * of the same problems.... something that
 * that should be addressed eventually.
 *
*/


/*
 * BMESH SELECTMODE FLUSH
 *
 * Makes sure to flush selections 
 * 'upwards' (ie: all verts of an edge
 * selects the edge and so on). This 
 * should only be called by system and not
 * tool authors.
 *
*/

void BM_SelectMode_Flush(BMesh *bm)
{
	BMEdge *e;
	BMLoop *l;
	BMFace *f;
	BMHeader *ele;

	BMIter edges;
	BMIter faces;
	BMIter iter;
	
	int types[3] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH};
	int *tots[3];
	int i;
	int totsel;

	if(bm->selectmode & BM_VERT){
		for(e = BMIter_New(&edges, bm, BM_EDGES_OF_MESH, bm ); e; e= BMIter_Step(&edges)){
			if(BM_TestHFlag(e->v1, BM_SELECT) && BM_TestHFlag(e->v2, BM_SELECT)) BM_SetHFlag(e, 1);
			else BM_ClearHFlag(e, 0);
		}
		for(f = BMIter_New(&faces, bm, BM_FACES_OF_MESH, bm ); f; f= BMIter_Step(&faces)){
			totsel = 0;
			l=f->loopbase;
			do{
				if(BM_TestHFlag(l->v, BM_SELECT)) 
					totsel++;
				l = ((BMLoop*)(l->head.next));
			} while(l != f->loopbase);
			
			if(totsel == f->len) 
				BM_SetHFlag(f, 1);
			else
				BM_ClearHFlag(f, 0);
		}
	}
	else if(bm->selectmode & BM_EDGE) {
		for(f = BMIter_New(&faces, bm, BM_FACES_OF_MESH, bm ); f; f= BMIter_Step(&faces)){
			totsel = 0;
			l=f->loopbase;
			do{
				if(bmesh_test_sysflag(&(l->e->head), BM_SELECT)) 
					totsel++;
				l = ((BMLoop*)(l->head.next));
			}while(l!=f->loopbase);
			
			if(totsel == f->len) 
				BM_SetHFlag(f, 1);
			else 
				BM_ClearHFlag(f, 0);
		}
	}

	/*recount tot*sel variables*/
	bm->totvertsel = bm->totedgesel = bm->totfacesel = 0;
	tots[0] = &bm->totvertsel;
	tots[1] = &bm->totedgesel;
	tots[2] = &bm->totfacesel;

	for (i=0; i<3; i++) {
		ele = BMIter_New(&iter, bm, types[i], NULL);
		for ( ; ele; ele=BMIter_Step(&iter)) {
			if (BM_TestHFlag(ele, BM_SELECT)) *tots[i] += 1;
		}
	}
}

/*
 * BMESH SELECT VERT
 *
 * Changes selection state of a single vertex 
 * in a mesh
 *
*/

void BM_Select_Vert(BMesh *bm, BMVert *v, int select)
{
	if(select) {
		if (!BM_TestHFlag(v, BM_SELECT)) bm->totvertsel += 1;
		BM_SetHFlag(v, BM_SELECT);
	} else {
		if (BM_TestHFlag(v, BM_SELECT)) bm->totvertsel -= 1;
		BM_ClearHFlag(v, BM_SELECT);
	}
}

/*
 * BMESH SELECT EDGE
 *
 * Changes selection state of a single edge
 * in a mesh. Note that this is actually not
 * 100 percent reliable. Deselecting an edge
 * will also deselect both its vertices
 * regardless of the selection state of
 * other edges incident upon it. Fixing this
 * issue breaks multi-select mode though...
 *
*/

void BM_Select_Edge(BMesh *bm, BMEdge *e, int select)
{
	int candesel;
	int testiso = 1;

	/*I might move this logic to bmeshutils_mods.c, where it'd be invoked
	  by the selection tools.  in that case, we'd still retain the checks
	  for if an edge's verts can be deselected.*/

	/*ensure vert selections are valid, only if not in a multiselect
	  mode that shares SCE_SELECT_VERT*/
	if (bm->selectmode & (SCE_SELECT_VERTEX|SCE_SELECT_EDGE)) testiso = 1;
	else if (bm->selectmode & (SCE_SELECT_VERTEX|SCE_SELECT_FACE)) testiso = 1;
	
	if (testiso && !select) {
		BMIter eiter;
		BMEdge *e2;
		int i;

		for (i=0; i<2; i++) {
			candesel = 1;
			e2 = BMIter_New(&eiter, bm, BM_EDGES_OF_VERT, !i?e->v1:e->v2);
			for (; e2; e2=BMIter_Step(&eiter)) {
				if (e2 == e) continue;
				if (BM_TestHFlag(e2, BM_SELECT)) {
					candesel = 0;
					break;
				}
			}

			if (candesel) BM_Select_Vert(bm, !i?e->v1:e->v2, 0);			
		}
	}

	if(select) { 
		if (!BM_TestHFlag(e, BM_SELECT)) bm->totedgesel += 1;

		BM_SetHFlag(&(e->head), BM_SELECT);
		BM_SetHFlag(&(e->v1->head), BM_SELECT);
		BM_SetHFlag(&(e->v2->head), BM_SELECT);
	}
	else{ 
		if (BM_TestHFlag(e, BM_SELECT)) bm->totedgesel -= 1;

		BM_ClearHFlag(&(e->head), BM_SELECT);
	}
}

/*
 *
 * BMESH SELECT FACE
 *
 * Changes selection state of a single
 * face in a mesh. This (might) suffer
 * from same problems as edge select
 * code...
 *
*/

void BM_Select_Face(BMesh *bm, BMFace *f, int select)
{
	BMLoop *l;

	if(select){ 
		if (!BM_TestHFlag(f, BM_SELECT)) bm->totfacesel += 1;

		BM_SetHFlag(&(f->head), BM_SELECT);
		l = f->loopbase;
		do{
			BM_SetHFlag(&(l->v->head), BM_SELECT);
			BM_SetHFlag(&(l->e->head), BM_SELECT);
			l = ((BMLoop*)(l->head.next));
		}while(l != f->loopbase);
	}
	else{ 
		if (BM_TestHFlag(f, BM_SELECT)) bm->totfacesel -= 1;

		BM_ClearHFlag(&(f->head), BM_SELECT);
		l = f->loopbase;
		do{
			BM_ClearHFlag(&(l->v->head), BM_SELECT);
			BM_ClearHFlag(&(l->e->head), BM_SELECT);
			l = ((BMLoop*)(l->head.next));
		}while(l != f->loopbase);
	}
}

/*
 * BMESH SELECTMODE SET
 *
 * Sets the selection mode for the bmesh
 *
*/

void BM_Selectmode_Set(BMesh *bm, int selectmode)
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;
	
	BMIter verts;
	BMIter edges;
	BMIter faces;
	
	bm->selectmode = selectmode;

	if(bm->selectmode & BM_VERT){
		for(e = BMIter_New(&edges, bm, BM_EDGES_OF_MESH, bm ); e; e= BMIter_Step(&edges))
			BM_Select_Edge(bm, e, 0);
		for(f = BMIter_New(&faces, bm, BM_FACES_OF_MESH, bm ); f; f= BMIter_Step(&faces))
			BM_Select_Face(bm, f, 0);
		BM_SelectMode_Flush(bm);
	}
	else if(bm->selectmode & BM_EDGE){
		for(v= BMIter_New(&verts, bm, BM_VERTS_OF_MESH, bm ); v; v= BMIter_Step(&verts))
			BM_Select_Vert(bm, v, 0);
		for(e= BMIter_New(&edges, bm, BM_EDGES_OF_MESH, bm ); e; e= BMIter_Step(&edges)){
			if(BM_TestHFlag(&(e->head), BM_SELECT))
				BM_Select_Edge(bm, e, 1);
		}
		BM_SelectMode_Flush(bm);
	}
	else if(bm->selectmode & BM_FACE){
		for(e = BMIter_New(&edges, bm, BM_EDGES_OF_MESH, bm ); e; e= BMIter_Step(&edges))
			BM_Select_Edge(bm, e, 0);
		for(f = BMIter_New(&faces, bm, BM_FACES_OF_MESH, bm ); f; f= BMIter_Step(&faces)){
			if(BM_TestHFlag(&(f->head), BM_SELECT))
				BM_Select_Face(bm, f, 1);
		}
	}
}


int BM_CountFlag(struct BMesh *bm, int type, int flag)
{
	BMHeader *head;
	BMIter iter;
	int tot = 0;

	if (type & BM_VERT) {
		for (head = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL); head; head=BMIter_Step(&iter)) {
			if (head->flag & flag) tot++;
		}
	}
	if (type & BM_EDGE) {
		for (head = BMIter_New(&iter, bm, BM_EDGES_OF_MESH, NULL); head; head=BMIter_Step(&iter)) {
			if (head->flag & flag) tot++;
		}
	}
	if (type & BM_FACE) {
		for (head = BMIter_New(&iter, bm, BM_FACES_OF_MESH, NULL); head; head=BMIter_Step(&iter)) {
			if (head->flag & flag) tot++;
		}
	}

	return tot;
}

void BM_Select(struct BMesh *bm, void *element, int select)
{
	BMHeader *head = element;

	if(head->type == BM_VERT) BM_Select_Vert(bm, (BMVert*)element, select);
	else if(head->type == BM_EDGE) BM_Select_Edge(bm, (BMEdge*)element, select);
	else if(head->type == BM_FACE) BM_Select_Face(bm, (BMFace*)element, select);
}

int BM_Is_Selected(BMesh *bm, void *element)
{
	BMHeader *head = element;
	return BM_TestHFlag(head, BM_SELECT);
}