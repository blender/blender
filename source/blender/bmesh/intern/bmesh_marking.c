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

void bmesh_selectmode_flush(BMesh *bm)
{
	BMEdge *e;
	BMLoop *l;
	BMFace *f;

	BMIter edges;
	BMIter faces;

	int totsel;

	if(bm->selectmode & BM_VERT){
		for(e = BMIter_New(&edges, bm, BM_EDGES_OF_MESH, bm ); e; e= BMIter_Step(&edges)){
			if(bmesh_test_sysflag(&(e->v1->head), BM_SELECT) && bmesh_test_sysflag(&(e->v2->head), BM_SELECT)) bmesh_set_sysflag(&(e->head), BM_SELECT);
			else bmesh_clear_sysflag(&(e->head), BM_SELECT);
		}
		for(f = BMIter_New(&faces, bm, BM_FACES_OF_MESH, bm ); f; f= BMIter_Step(&faces)){
			totsel = 0;
			l=f->loopbase;
			do{
				if(bmesh_test_sysflag(&(l->v->head), BM_SELECT)) 
					totsel++;
				l = ((BMLoop*)(l->head.next));
			}while(l!=f->loopbase);
			
			if(totsel == f->len) 
				bmesh_set_sysflag(&(f->head), BM_SELECT);
			else
				bmesh_clear_sysflag(&(f->head), BM_SELECT);
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
				bmesh_set_sysflag(&(f->head), BM_SELECT);
			else 
				bmesh_clear_sysflag(&(f->head), BM_SELECT);
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
	if(select)
		bmesh_set_sysflag(&(v->head), BM_SELECT);
	else 
		bmesh_clear_sysflag(&(v->head), BM_SELECT);
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
	if(select){ 
		bmesh_set_sysflag(&(e->head), BM_SELECT);
		bmesh_set_sysflag(&(e->v1->head), BM_SELECT);
		bmesh_set_sysflag(&(e->v2->head), BM_SELECT);
	}
	else{ 
		bmesh_clear_sysflag(&(e->head), BM_SELECT);
		bmesh_clear_sysflag(&(e->v1->head), BM_SELECT);
		bmesh_clear_sysflag(&(e->v2->head), BM_SELECT);
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
		bmesh_set_sysflag(&(f->head), BM_SELECT);
		l = f->loopbase;
		do{
			bmesh_set_sysflag(&(l->v->head), BM_SELECT);
			bmesh_set_sysflag(&(l->e->head), BM_SELECT);
			l = ((BMLoop*)(l->head.next));
		}while(l != f->loopbase);
	}
	else{ 
		bmesh_clear_sysflag(&(f->head), BM_SELECT);
		l = f->loopbase;
		do{
			bmesh_clear_sysflag(&(l->v->head), BM_SELECT);
			bmesh_clear_sysflag(&(l->e->head), BM_SELECT);
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
			bmesh_clear_sysflag(&(f->head), 0);
		bmesh_selectmode_flush(bm);
	}
	else if(bm->selectmode & BM_EDGE){
		for(v= BMIter_New(&verts, bm, BM_VERTS_OF_MESH, bm ); v; v= BMIter_Step(&verts))
			BM_Select_Vert(bm, v, 0);
		for(e= BMIter_New(&edges, bm, BM_EDGES_OF_MESH, bm ); e; e= BMIter_Step(&edges)){
			if(bmesh_test_sysflag(&(e->head), BM_SELECT))
				BM_Select_Edge(bm, e, 1);
		}
		bmesh_selectmode_flush(bm);
	}
	else if(bm->selectmode & BM_FACE){
		for(e = BMIter_New(&edges, bm, BM_EDGES_OF_MESH, bm ); e; e= BMIter_Step(&edges))
			BM_Select_Edge(bm, e, 0);
		for(f = BMIter_New(&faces, bm, BM_FACES_OF_MESH, bm ); f; f= BMIter_Step(&faces)){
			if(bmesh_test_sysflag(&(f->head), BM_SELECT))
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
	return bmesh_test_sysflag(head, BM_SELECT);
}