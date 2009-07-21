#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "bmesh.h"
#include "bmesh_private.h"

#include <string.h>

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

static void recount_totsels(BMesh *bm)
{
	BMIter iter;
	BMHeader *ele;
	int types[3] = {BM_VERTS_OF_MESH, BM_EDGES_OF_MESH, BM_FACES_OF_MESH};
	int *tots[3];
	int i;

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

void BM_SelectMode_Flush(BMesh *bm)
{
	BMEdge *e;
	BMLoop *l;
	BMFace *f;

	BMIter edges;
	BMIter faces;

	int totsel;

	if(bm->selectmode & SCE_SELECT_VERTEX) {
		for(e = BMIter_New(&edges, bm, BM_EDGES_OF_MESH, bm ); e; e= BMIter_Step(&edges)) {
			if(BM_TestHFlag(e->v1, BM_SELECT) && BM_TestHFlag(e->v2, BM_SELECT)) BM_SetHFlag(e, BM_SELECT);
			else BM_ClearHFlag(e, BM_SELECT);
		}
		for(f = BMIter_New(&faces, bm, BM_FACES_OF_MESH, bm ); f; f= BMIter_Step(&faces)) {
			totsel = 0;
			l=f->loopbase;
			do{
				if(BM_TestHFlag(l->v, BM_SELECT)) 
					totsel++;
				l = ((BMLoop*)(l->head.next));
			} while(l != f->loopbase);
			
			if(totsel == f->len) 
				BM_SetHFlag(f, BM_SELECT);
			else
				BM_ClearHFlag(f, BM_SELECT);
		}
	}
	else if(bm->selectmode & SCE_SELECT_EDGE) {
		for(f = BMIter_New(&faces, bm, BM_FACES_OF_MESH, bm ); f; f= BMIter_Step(&faces)) {
			totsel = 0;
			l=f->loopbase;
			do{
				if(bmesh_test_sysflag(&(l->e->head), BM_SELECT)) 
					totsel++;
				l = ((BMLoop*)(l->head.next));
			}while(l!=f->loopbase);
			
			if(totsel == f->len) 
				BM_SetHFlag(f, BM_SELECT);
			else 
				BM_ClearHFlag(f, BM_SELECT);
		}
	}

	recount_totsels(bm);
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
		BM_SetHFlag(e->v1, BM_SELECT);
		BM_SetHFlag(e->v2, BM_SELECT);
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
		do {
			BM_ClearHFlag(&(l->v->head), BM_SELECT);
			BM_ClearHFlag(&(l->e->head), BM_SELECT);
			l = ((BMLoop*)(l->head.next));
		} while(l != f->loopbase);
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

	if(bm->selectmode & SCE_SELECT_VERTEX) {
		for(e = BMIter_New(&edges, bm, BM_EDGES_OF_MESH, bm ); e; e= BMIter_Step(&edges))
			BM_ClearHFlag(e, 0);
		for(f = BMIter_New(&faces, bm, BM_FACES_OF_MESH, bm ); f; f= BMIter_Step(&faces))
			BM_ClearHFlag(f, 0);
		BM_SelectMode_Flush(bm);
	}
	else if(bm->selectmode & SCE_SELECT_EDGE) {
		for(v= BMIter_New(&verts, bm, BM_VERTS_OF_MESH, bm ); v; v= BMIter_Step(&verts))
			BM_ClearHFlag(v, 0);
		for(e= BMIter_New(&edges, bm, BM_EDGES_OF_MESH, bm ); e; e= BMIter_Step(&edges)){
			if(BM_TestHFlag(&(e->head), BM_SELECT))
				BM_Select_Edge(bm, e, 1);
		}
		BM_SelectMode_Flush(bm);
	}
	else if(bm->selectmode & SCE_SELECT_FACE) {
		for(e = BMIter_New(&edges, bm, BM_EDGES_OF_MESH, bm ); e; e= BMIter_Step(&edges))
			BM_ClearHFlag(e, 0);
		for(f = BMIter_New(&faces, bm, BM_FACES_OF_MESH, bm ); f; f= BMIter_Step(&faces)){
			if(BM_TestHFlag(&(f->head), BM_SELECT))
				BM_Select_Face(bm, f, 1);
		}
		BM_SelectMode_Flush(bm);
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

/*note: by design, this will not touch the editselection history stuff*/
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


/* generic way to get data from an EditSelection type 
These functions were written to be used by the Modifier widget when in Rotate about active mode,
but can be used anywhere.
EM_editselection_center
EM_editselection_normal
EM_editselection_plane
*/
void BM_editselection_center(BMesh *em, float *center, BMEditSelection *ese)
{
	if (ese->type==BM_VERT) {
		BMVert *eve= ese->data;
		VecCopyf(center, eve->co);
	} else if (ese->type==BM_EDGE) {
		BMEdge *eed= ese->data;
		VecAddf(center, eed->v1->co, eed->v2->co);
		VecMulf(center, 0.5);
	} else if (ese->type==BM_FACE) {
		BMFace *efa= ese->data;
		BM_Compute_Face_Center(em, efa, center);
	}
}

void BM_editselection_normal(float *normal, BMEditSelection *ese)
{
	if (ese->type==BM_VERT) {
		BMVert *eve= ese->data;
		VecCopyf(normal, eve->no);
	} else if (ese->type==BM_EDGE) {
		BMEdge *eed= ese->data;
		float plane[3]; /* need a plane to correct the normal */
		float vec[3]; /* temp vec storage */
		
		VecAddf(normal, eed->v1->no, eed->v2->no);
		VecSubf(plane, eed->v2->co, eed->v1->co);
		
		/* the 2 vertex normals will be close but not at rightangles to the edge
		for rotate about edge we want them to be at right angles, so we need to
		do some extra colculation to correct the vert normals,
		we need the plane for this */
		Crossf(vec, normal, plane);
		Crossf(normal, plane, vec); 
		Normalize(normal);
		
	} else if (ese->type==BM_FACE) {
		BMFace *efa= ese->data;
		VecCopyf(normal, efa->no);
	}
}

/* Calculate a plane that is rightangles to the edge/vert/faces normal
also make the plane run allong an axis that is related to the geometry,
because this is used for the manipulators Y axis.*/
void BM_editselection_plane(BMesh *em, float *plane, BMEditSelection *ese)
{
	if (ese->type==BM_VERT) {
		BMVert *eve= ese->data;
		float vec[3]={0,0,0};
		
		if (ese->prev) { /*use previously selected data to make a usefull vertex plane */
			BM_editselection_center(em, vec, ese->prev);
			VecSubf(plane, vec, eve->co);
		} else {
			/* make a fake  plane thats at rightangles to the normal
			we cant make a crossvec from a vec thats the same as the vec
			unlikely but possible, so make sure if the normal is (0,0,1)
			that vec isnt the same or in the same direction even.*/
			if (eve->no[0]<0.5)		vec[0]=1;
			else if (eve->no[1]<0.5)	vec[1]=1;
			else				vec[2]=1;
			Crossf(plane, eve->no, vec);
		}
	} else if (ese->type==BM_EDGE) {
		BMEdge *eed= ese->data;

		/*the plane is simple, it runs allong the edge
		however selecting different edges can swap the direction of the y axis.
		this makes it less likely for the y axis of the manipulator
		(running along the edge).. to flip less often.
		at least its more pradictable */
		if (eed->v2->co[1] > eed->v1->co[1]) /*check which to do first */
			VecSubf(plane, eed->v2->co, eed->v1->co);
		else
			VecSubf(plane, eed->v1->co, eed->v2->co);
		
	} else if (ese->type==BM_FACE) {
		BMFace *efa= ese->data;
		float vec[3] = {0.0f, 0.0f, 0.0f};
		
		/*for now, use face normal*/

		/* make a fake  plane thats at rightangles to the normal
		we cant make a crossvec from a vec thats the same as the vec
		unlikely but possible, so make sure if the normal is (0,0,1)
		that vec isnt the same or in the same direction even.*/
		if (efa->no[0]<0.5)		vec[0]=1.0f;
		else if (efa->no[1]<0.5)	vec[1]=1.0f;
		else				vec[2]=1.0f;
		Crossf(plane, efa->no, vec);
#if 0

		if (efa->v4) { /*if its a quad- set the plane along the 2 longest edges.*/
			float vecA[3], vecB[3];
			VecSubf(vecA, efa->v4->co, efa->v3->co);
			VecSubf(vecB, efa->v1->co, efa->v2->co);
			VecAddf(plane, vecA, vecB);
			
			VecSubf(vecA, efa->v1->co, efa->v4->co);
			VecSubf(vecB, efa->v2->co, efa->v3->co);
			VecAddf(vec, vecA, vecB);						
			/*use the biggest edge length*/
			if (plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2] < vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2])
				VecCopyf(plane, vec);
		} else {
			/*start with v1-2 */
			VecSubf(plane, efa->v1->co, efa->v2->co);
			
			/*test the edge between v2-3, use if longer */
			VecSubf(vec, efa->v2->co, efa->v3->co);
			if (plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2] < vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2])
				VecCopyf(plane, vec);
			
			/*test the edge between v1-3, use if longer */
			VecSubf(vec, efa->v3->co, efa->v1->co);
			if (plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2] < vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2])
				VecCopyf(plane, vec);
		}
#endif
	}
	Normalize(plane);
}

static int BM_check_selection(BMesh *em, void *data)
{
	BMEditSelection *ese;
	
	for(ese = em->selected.first; ese; ese = ese->next){
		if(ese->data == data) return 1;
	}
	
	return 0;
}

void BM_remove_selection(BMesh *em, void *data)
{
	BMEditSelection *ese;
	for(ese=em->selected.first; ese; ese = ese->next){
		if(ese->data == data){
			BLI_freelinkN(&(em->selected),ese);
			break;
		}
	}
}

void BM_store_selection(BMesh *em, void *data)
{
	BMEditSelection *ese;
	if(!BM_check_selection(em, data)){
		ese = (BMEditSelection*) MEM_callocN( sizeof(BMEditSelection), "BMEdit Selection");
		ese->type = ((BMHeader*)data)->type;
		ese->data = data;
		BLI_addtail(&(em->selected),ese);
	}
}

void BM_validate_selections(BMesh *em)
{
	BMEditSelection *ese, *nextese;

	ese = em->selected.first;

	while(ese){
		nextese = ese->next;
		if (!BM_TestHFlag(ese->data, BM_SELECT)) BLI_freelinkN(&(em->selected), ese);
		ese = nextese;
	}
}
