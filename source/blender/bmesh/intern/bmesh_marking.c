#include "MEM_guardedalloc.h"

#include "BKE_utildefines.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_array.h"

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
	switch (bm->selectmode) {
		case SCE_SELECT_VERTEX:
		case SCE_SELECT_EDGE:
		case SCE_SELECT_FACE:
		case SCE_SELECT_EDGE|SCE_SELECT_FACE:
			testiso = 1;
			break;
		default:
			testiso = 0;
			break;
	}

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
		BM_Select(bm, e->v1, 1);
		BM_Select(bm, e->v2, 1);
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
			BM_Select_Vert(bm, l->v, 1);
			BM_Select_Edge(bm, l->e, 1);
			l = ((BMLoop*)(l->head.next));
		}while(l != f->loopbase);
	}
	else{ 
		BMIter liter, fiter, eiter;
		BMFace *f2;
		BMLoop *l;
		BMEdge *e;

		if (BM_TestHFlag(f, BM_SELECT)) bm->totfacesel -= 1;
		BM_ClearHFlag(&(f->head), BM_SELECT);

		/*flush down to edges*/
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			BM_ITER(f2, &fiter, bm, BM_FACES_OF_EDGE, l->e) {
				if (BM_TestHFlag(f2, BM_SELECT))
					break;
			}

			if (!f2) {
				BM_Select(bm, l->e, 0);
			}
		}

		/*flush down to verts*/
		BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
			BM_ITER(e, &eiter, bm, BM_EDGES_OF_VERT, l->v) {
				if (BM_TestHFlag(e, BM_SELECT))
					break;
			}

			if (!e) {
				BM_Select(bm, l->v, 0);
			}
		}
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


int BM_CountFlag(struct BMesh *bm, int type, int flag, int respecthide)
{
	BMHeader *head;
	BMIter iter;
	int tot = 0;

	if (type & BM_VERT) {
		for (head = BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL); head; head=BMIter_Step(&iter)) {
			if (respecthide && BM_TestHFlag(head, BM_HIDDEN)) continue;
			if (head->flag & flag) tot++;
		}
	}
	if (type & BM_EDGE) {
		for (head = BMIter_New(&iter, bm, BM_EDGES_OF_MESH, NULL); head; head=BMIter_Step(&iter)) {
			if (respecthide && BM_TestHFlag(head, BM_HIDDEN)) continue;
			if (head->flag & flag) tot++;
		}
	}
	if (type & BM_FACE) {
		for (head = BMIter_New(&iter, bm, BM_FACES_OF_MESH, NULL); head; head=BMIter_Step(&iter)) {
			if (respecthide && BM_TestHFlag(head, BM_HIDDEN)) continue;
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

int BM_Selected(BMesh *bm, void *element)
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
		copy_v3_v3(center, eve->co);
	} else if (ese->type==BM_EDGE) {
		BMEdge *eed= ese->data;
		add_v3_v3v3(center, eed->v1->co, eed->v2->co);
		mul_v3_fl(center, 0.5);
	} else if (ese->type==BM_FACE) {
		BMFace *efa= ese->data;
		BM_Compute_Face_Center(em, efa, center);
	}
}

void BM_editselection_normal(float *normal, BMEditSelection *ese)
{
	if (ese->type==BM_VERT) {
		BMVert *eve= ese->data;
		copy_v3_v3(normal, eve->no);
	} else if (ese->type==BM_EDGE) {
		BMEdge *eed= ese->data;
		float plane[3]; /* need a plane to correct the normal */
		float vec[3]; /* temp vec storage */
		
		add_v3_v3v3(normal, eed->v1->no, eed->v2->no);
		sub_v3_v3v3(plane, eed->v2->co, eed->v1->co);
		
		/* the 2 vertex normals will be close but not at rightangles to the edge
		for rotate about edge we want them to be at right angles, so we need to
		do some extra colculation to correct the vert normals,
		we need the plane for this */
		cross_v3_v3v3(vec, normal, plane);
		cross_v3_v3v3(normal, plane, vec); 
		normalize_v3(normal);
		
	} else if (ese->type==BM_FACE) {
		BMFace *efa= ese->data;
		copy_v3_v3(normal, efa->no);
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
			sub_v3_v3v3(plane, vec, eve->co);
		} else {
			/* make a fake  plane thats at rightangles to the normal
			we cant make a crossvec from a vec thats the same as the vec
			unlikely but possible, so make sure if the normal is (0,0,1)
			that vec isnt the same or in the same direction even.*/
			if (eve->no[0]<0.5)		vec[0]=1;
			else if (eve->no[1]<0.5)	vec[1]=1;
			else				vec[2]=1;
			cross_v3_v3v3(plane, eve->no, vec);
		}
	} else if (ese->type==BM_EDGE) {
		BMEdge *eed= ese->data;

		/*the plane is simple, it runs allong the edge
		however selecting different edges can swap the direction of the y axis.
		this makes it less likely for the y axis of the manipulator
		(running along the edge).. to flip less often.
		at least its more pradictable */
		if (eed->v2->co[1] > eed->v1->co[1]) /*check which to do first */
			sub_v3_v3v3(plane, eed->v2->co, eed->v1->co);
		else
			sub_v3_v3v3(plane, eed->v1->co, eed->v2->co);
		
	} else if (ese->type==BM_FACE) {
		BMFace *efa= ese->data;
		float vec[3] = {0.0f, 0.0f, 0.0f};
		
		/*for now, use face normal*/

		/* make a fake plane thats at rightangles to the normal
		we cant make a crossvec from a vec thats the same as the vec
		unlikely but possible, so make sure if the normal is (0,0,1)
		that vec isnt the same or in the same direction even.*/
		if (efa->no[0]<0.5)		vec[0]=1.0f;
		else if (efa->no[1]<0.5)	vec[1]=1.0f;
		else				vec[2]=1.0f;
		cross_v3_v3v3(plane, efa->no, vec);
#if 0 //BMESH_TODO

		if (efa->v4) { /*if its a quad- set the plane along the 2 longest edges.*/
			float vecA[3], vecB[3];
			sub_v3_v3v3(vecA, efa->v4->co, efa->v3->co);
			sub_v3_v3v3(vecB, efa->v1->co, efa->v2->co);
			add_v3_v3v3(plane, vecA, vecB);
			
			sub_v3_v3v3(vecA, efa->v1->co, efa->v4->co);
			sub_v3_v3v3(vecB, efa->v2->co, efa->v3->co);
			add_v3_v3v3(vec, vecA, vecB);						
			/*use the biggest edge length*/
			if (plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2] < vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2])
				copy_v3_v3(plane, vec);
		} else {
			/*start with v1-2 */
			sub_v3_v3v3(plane, efa->v1->co, efa->v2->co);
			
			/*test the edge between v2-3, use if longer */
			sub_v3_v3v3(vec, efa->v2->co, efa->v3->co);
			if (plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2] < vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2])
				copy_v3_v3(plane, vec);
			
			/*test the edge between v1-3, use if longer */
			sub_v3_v3v3(vec, efa->v3->co, efa->v1->co);
			if (plane[0]*plane[0]+plane[1]*plane[1]+plane[2]*plane[2] < vec[0]*vec[0]+vec[1]*vec[1]+vec[2]*vec[2])
				copy_v3_v3(plane, vec);
		}
#endif
	}
	normalize_v3(plane);
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

void BM_clear_selection_history(BMesh *em)
{
	BLI_freelistN(&em->selected);
	em->selected.first = em->selected.last = NULL;
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

void BM_clear_flag_all(BMesh *bm, int flag)
{
	BMIter iter;
	BMHeader *ele;
	int i, type;

	if (flag & BM_SELECT)
		BM_clear_selection_history(bm);

	for (i=0; i<3; i++) {
		switch (i) {
			case 0:
				type = BM_VERTS_OF_MESH;
				break;
			case 1:
				type = BM_EDGES_OF_MESH;
				break;
			case 2:
				type = BM_FACES_OF_MESH;
				break;
		}
		
		ele = BMIter_New(&iter, bm, type, NULL);
		for ( ; ele; ele=BMIter_Step(&iter)) {
			if (flag & BM_SELECT) BM_Select(bm, ele, 0);
			BM_ClearHFlag(ele, flag);
		}
	}
}


/***************** Pinning **************/

#define SETPIN(ele) pin ? BM_SetHFlag(ele, BM_PINNED) : BM_ClearHFlag(ele, BM_PINNED);


void BM_Pin_Vert(BMesh *bm, BMVert *v, int pin)
{
	SETPIN(v);
}

void BM_Pin_Edge(BMesh *bm, BMEdge *e, int pin)
{
	SETPIN(e->v1);
	SETPIN(e->v2);
}

void BM_Pin_Face(BMesh *bm, BMFace *f, int pin)
{
	BMIter vfiter;
	BMVert *vf;

	BM_ITER(vf, &vfiter, bm, BM_VERTS_OF_FACE, f) {
		SETPIN(vf);
	}
}

void BM_Pin(BMesh *bm, void *element, int pin)
{
	BMHeader *h = element;

	switch (h->type) {
		case BM_VERT:
			BM_Pin_Vert(bm, element, pin);
			break;
		case BM_EDGE:
			BM_Pin_Edge(bm, element, pin);
			break;
		case BM_FACE:
			BM_Pin_Face(bm, element, pin);
			break;
	}
}



/***************** Mesh Hiding stuff *************/

#define SETHIDE(ele) hide ? BM_SetHFlag(ele, BM_HIDDEN) : BM_ClearHFlag(ele, BM_HIDDEN);

static void vert_flush_hide(BMesh *bm, BMVert *v) {
	BMIter iter;
	BMEdge *e;

	BM_ITER(e, &iter, bm, BM_EDGES_OF_VERT, v) {
		if (!BM_TestHFlag(e, BM_HIDDEN))
			return;
	}

	BM_SetHFlag(v, BM_HIDDEN);
}

static void edge_flush_hide(BMesh *bm, BMEdge *e) {
	BMIter iter;
	BMFace *f;

	BM_ITER(f, &iter, bm, BM_FACES_OF_EDGE, e) {
		if (!BM_TestHFlag(f, BM_HIDDEN))
			return;
	}

	BM_SetHFlag(e, BM_HIDDEN);
}

void BM_Hide_Vert(BMesh *bm, BMVert *v, int hide)
{
	/*vert hiding: vert + surrounding edges and faces*/
	BMIter iter, fiter;
	BMEdge *e;
	BMFace *f;

	SETHIDE(v);

	BM_ITER(e, &iter, bm, BM_EDGES_OF_VERT, v) {
		SETHIDE(e);

		BM_ITER(f, &fiter, bm, BM_FACES_OF_EDGE, e) {
			SETHIDE(f);
		}
	}
}

void BM_Hide_Edge(BMesh *bm, BMEdge *e, int hide)
{
	BMIter iter;
	BMFace *f;
	BMVert *v;

	/*edge hiding: faces around the edge*/
	BM_ITER(f, &iter, bm, BM_FACES_OF_EDGE, e) {
		SETHIDE(f);
	}
	
	SETHIDE(e);

	/*hide vertices if necassary*/
	vert_flush_hide(bm, e->v1);
	vert_flush_hide(bm, e->v2);
}

void BM_Hide_Face(BMesh *bm, BMFace *f, int hide)
{
	BMIter iter;
	BMLoop *l;

	/**/
	SETHIDE(f);

	BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
		edge_flush_hide(bm, l->e);
	}

	BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
		vert_flush_hide(bm, l->v);
	}
}

void BM_Hide(BMesh *bm, void *element, int hide)
{
	BMHeader *h = element;

	switch (h->type) {
		case BM_VERT:
			BM_Hide_Vert(bm, element, hide);
			break;
		case BM_EDGE:
			BM_Hide_Edge(bm, element, hide);
			break;
		case BM_FACE:
			BM_Hide_Face(bm, element, hide);
			break;
	}
}


