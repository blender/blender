#include "MEM_guardedalloc.h"
#include "BKE_customdata.h" 
#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include <string.h>
#include "BKE_utildefines.h"
#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"

#include "BLI_editVert.h"
#include "mesh_intern.h"
#include "ED_mesh.h"

#include "BLI_blenlib.h"
#include "BLI_edgehash.h"

#include "bmesh.h"

/*
 * EDITMESH TO BMESH.C
 *
 * This file contains functions
 * for converting an editmesh
 * into a Bmesh
 *
*/

/*
 * EDITMESH CORNERS TO LOOPS
 * 
 * Converts editmesh face corner data
 * (UVs, Vert colors, ect) to N-Gon 
 * face-edge ('loop') data.
 *
*/

static void editmesh_corners_to_loops(BMesh *bm, CustomData *facedata, void *face_block, BMFace *f,int numCol, int numTex){
	int i, j;
	BMLoop *l;
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;
	BMIter iter;

	for(i=0; i < numTex; i++){
		texface = CustomData_em_get_n(facedata, face_block, CD_MTFACE, i);
		texpoly = CustomData_bmesh_get_n(&bm->pdata, f->data, CD_MTEXPOLY, i);
		
		texpoly->tpage = texface->tpage;
		texpoly->flag = texface->flag;
		texpoly->transp = texface->transp;
		texpoly->mode = texface->mode;
		texpoly->tile = texface->tile;
		texpoly->unwrap = texface->unwrap;
		
		for (j=0, l=BMIter_New(&iter, bm, BM_LOOPS_OF_FACE, f); l; j++, l=BMIter_Step(&iter)) {
			mloopuv = CustomData_bmesh_get_n(&bm->ldata, l->data, CD_MLOOPUV, i);
			mloopuv->uv[0] = texface->uv[j][0];
			mloopuv->uv[1] = texface->uv[j][1];
		}

	}
	for(i=0; i < numCol; i++){
		mcol = CustomData_em_get_n(facedata, face_block, CD_MCOL, i);
		for (j=0, l=BMIter_New(&iter, bm, BM_LOOPS_OF_FACE, f); l; j++, l=BMIter_Step(&iter)) {
			mloopcol = CustomData_bmesh_get_n(&bm->ldata, l->data, CD_MLOOPCOL, i);
			mloopcol->r = mcol[j].r;
			mloopcol->g = mcol[j].g;
			mloopcol->b = mcol[j].b;
			mloopcol->a = mcol[j].a;
		}
	}
}

/*
 * EDITVERT TO BMVert
 *
 * Converts an editvert to
 * a BMVert.
 *
*/

static BMVert *editvert_to_BMVert(BMesh *bm, EditMesh *em, EditVert *eve)
{
		BMVert *v = NULL;

		v = BM_Make_Vert(bm, eve->co, NULL);
		VECCOPY(v->no, eve->no);

		/*transfer flags*/
		v->head.flag = eve->h ? BM_HIDDEN : 0;
		if(eve->f & SELECT) BM_Select_Vert(bm, v, 1);
		v->bweight = eve->bweight;

		/*Copy Custom Data*/
		CustomData_bmesh_copy_data(&em->vdata, &bm->vdata, eve->data, &v->data);
		
		return v;
}	

/*
 * EDITEDGE TO BMEdge
 *
 * Converts an editedge to 
 * a BMEdge
 *
*/

static void editedge_to_BMEdge_internal(BMesh *bm, EditMesh *em, BMEdge *e, EditEdge *eed)
{
	e->crease = eed->crease;
	e->bweight = eed->bweight;
	
	e->head.flag = eed->f & SELECT ? BM_SELECT : 0;
	e->head.flag |= eed->seam ? BM_SEAM : 0;
	e->head.flag |= eed->h & 1 ? BM_HIDDEN : 0;
	e->head.flag |= eed->h & EM_FGON ? BM_FGON : 0;
	e->head.flag |= eed->sharp ? BM_SHARP : 0;

	CustomData_bmesh_copy_data(&em->edata, &bm->edata, eed->data, &e->data);
}

static BMEdge *editedge_to_BMEdge(BMesh *bm, EditMesh *em, EditEdge *eed)
{
		BMVert *v1 = NULL, *v2 = NULL;
		BMEdge *e = NULL;
		
		v1 = eed->v1->tmp.p;
		v2 = eed->v2->tmp.p;
	
		e = BM_Make_Edge(bm, v1, v2,NULL, 0); 

		editedge_to_BMEdge_internal(bm, em, e, eed);

		return e;
}
/*
 * EDITFACE TO BMFace
 *
 * Converts an editface to a BMFace.
 * Note that this also convert per-face
 * corner data as well.
 *
*/

static BMFace *editface_to_BMFace(BMesh *bm, EditMesh *em, EditFace *efa, int numCol, int numTex)
{
		BMVert *v1 = NULL, *v2 = NULL;
		BMFace *f = NULL;
		BMEdge *edar[4];
		int len;

		edar[0] = BM_Make_Edge(bm, efa->v1->tmp.p, efa->v2->tmp.p, NULL, 1); 
		edar[1] = BM_Make_Edge(bm, efa->v2->tmp.p, efa->v3->tmp.p, NULL, 1); 
		if(efa->v4){
			edar[2] = BM_Make_Edge(bm, efa->v3->tmp.p, efa->v4->tmp.p, NULL, 1); 
			edar[3] = BM_Make_Edge(bm, efa->v4->tmp.p, efa->v1->tmp.p, NULL, 1); 
		}else{
			edar[2] = BM_Make_Edge(bm, efa->v3->tmp.p, efa->v1->tmp.p, NULL, 1); 
		}

		editedge_to_BMEdge_internal(bm, em, edar[0], efa->e1);
		editedge_to_BMEdge_internal(bm, em, edar[1], efa->e2);
		editedge_to_BMEdge_internal(bm, em, edar[2], efa->e3);
		if(efa->v4)
			editedge_to_BMEdge_internal(bm, em, edar[3], efa->e4);


		if(efa->e1->fgoni) edar[0]->head.flag |= BM_FGON;
		if(efa->e2->fgoni) edar[1]->head.flag |= BM_FGON;
		if(efa->e3->fgoni) edar[2]->head.flag |= BM_FGON;
		if(efa->v4 && efa->e4->fgoni) edar[3]->head.flag |= BM_FGON;

		if(efa->v4) len = 4;
		else len = 3;

		/*find v1 and v2*/
		v1 = efa->v1->tmp.p;
		v2 = efa->v2->tmp.p;

		f = BM_Make_Ngon(bm, v1, v2, edar, len, 0);
		f->head.flag = 0;
		f->mat_nr = efa->mat_nr;
		if(efa->f & SELECT) BM_Select_Face(bm, f, 1);
		if(efa->h) f->head.flag |= BM_HIDDEN;
		
		CustomData_bmesh_copy_data(&em->fdata, &bm->pdata, efa->data, &f->data);
		editmesh_corners_to_loops(bm, &em->fdata, efa->data, f,numCol,numTex);

		return f;
}

/*
 * BMESH FGONCONVERT
 *
 * This function and its associated structures
 * /helpers (fgonsort, sortfgon, fuse_fgon) are
 * used to convert f-gons to bmesh n-gons. This
 * is accomplished by sorting a list of fgon faces
 * such that faces that are part of the same fgon
 * are next to each other. These faces are then
 * converted as is into bmesh faces and
 * fused togather.
 *
 * Note that currently, there is no support for 
 * holes in faces in the bmesh structure, so 
 * f-gons with holes will only partially convert.
 *
*/

typedef struct fgonsort {
	unsigned long x;
	struct EditFace *efa;
	struct BMFace *f;
	int done;
}fgonsort;

static int sortfgon(const void *v1, const void *v2)
{
	const struct fgonsort *x1=v1, *x2=v2;
	
	if( x1->x > x2->x ) return 1;
	else if( x1->x < x2->x) return -1;
	return 0;
}

static void fuse_fgon(BMesh *bm, BMFace *f)
{
	BMFace *sf;
	BMLoop *l;
	int done;

	sf = f;
	done = 0;
	while(!done){
		done = 1;
		l = sf->loopbase;
		do{
			if(l->e->head.flag & BM_FGON){ 
				sf = BM_Join_Faces(bm,l->f, ((BMLoop*)l->radial.next->data)->f, l->e, 0,0);
				if(sf){
					done = 0;
					break;
				} else { /*we have to get out of here...*/
					return;
				}
			}
			l = ((BMLoop*)(l->head.next));
		}while(l != sf->loopbase);
	}
}

static BM_fgonconvert(BMesh *bm, EditMesh *em, int numCol, int numTex)
{
	EditFace *efa;
	BMFace *f;
	BMIter iter;
	struct fgonsort *sortblock, *sb, *sb1;
	int a, b, amount=0;
	
	/*
	for (efa=em->faces.first; efa; efa=efa->next) {
		f = editface_to_BMFace(bm, em, efa, numCol, numTex);
	}

	for (f=bm->polys.first; f; f=f->head.next) {
		fuse_fgon(bm, f);
	}

	return;*/

	EM_fgon_flags(em);

	/*zero out efa->tmp, we store fgon index here*/
	for(efa = em->faces.first; efa; efa = efa->next){ 
		efa->tmp.l = 0;
		amount++;
	}
	/*go through and give each editface an fgon index*/
	for(efa = em->faces.first; efa; efa = efa->next){
		if(efa->e1->fgoni) efa->tmp.l = efa->e1->fgoni;
		else if(efa->e2->fgoni) efa->tmp.l = efa->e2->fgoni;
		else if(efa->e3->fgoni) efa->tmp.l = efa->e3->fgoni;
		else if(efa->e4 && efa->e4->fgoni) efa->tmp.l = efa->e4->fgoni;
	}

	sb= sortblock= MEM_mallocN(sizeof(fgonsort)* amount,"fgon sort block");

	for(efa = em->faces.first; efa; efa=efa->next){
		sb->x = efa->tmp.l;
		sb->efa = efa;
		sb->done = 0;
		sb++;
	}

	qsort(sortblock, amount, sizeof(fgonsort), sortfgon);

	sb = sortblock;
	for(a=0; a<amount; a++, sb++) {
		if(sb->x && sb->done == 0){
			/*first pass: add in faces for this fgon*/
			for(b=a, sb1 = sb; b<amount && sb1->x == sb->x; b++, sb1++){
				efa = sb1->efa;
				sb1->f = editface_to_BMFace(bm, em, efa, numCol, numTex);
				sb1->done = 1;
			}
			/*fuse fgon*/
			fuse_fgon(bm, sb->f);
		}
	}
	MEM_freeN(sortblock);
}

/*
 * TAG WIRE EDGES
 *
 * Flags editedges 'f1' member
 * if the edge has no faces.
 *
*/

static void tag_wire_edges(EditMesh *em){
	EditFace *efa;
	EditEdge *eed;
	for(eed = em->edges.first; eed; eed = eed->next) eed->f1 = 1;
	for(efa = em->faces.first; efa; efa = efa->next){
		efa->e1->f1 = 0;
		efa->e2->f1 = 0;
		efa->e3->f1 = 0;
		if(efa->e4) efa->e4->f1 = 0;
	}
}

/*
 * EDITMESH TO BMESH
 *
 * Function to convert an editmesh to a bmesh
 * Currently all custom data as well as 
 * f-gons should be converted correctly.
 *
*/

BMesh *editmesh_to_bmesh_intern(EditMesh *em, BMesh *bm) {
	BMVert *v;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	int allocsize[4] = {512,512,2048,512}, numTex, numCol;

	/*make sure to update FGon flags*/
	EM_fgon_flags(em);

	/*copy custom data layout*/
	CustomData_copy(&em->vdata, &bm->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&em->edata, &bm->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&em->fdata, &bm->pdata, CD_MASK_BMESH, CD_CALLOC, 0);

	/*copy face corner data*/
	CustomData_to_bmeshpoly(&em->fdata, &bm->pdata, &bm->ldata);
	/*initialize memory pools*/
	CustomData_bmesh_init_pool(&bm->vdata, allocsize[0]);
	CustomData_bmesh_init_pool(&bm->edata, allocsize[1]);
	CustomData_bmesh_init_pool(&bm->ldata, allocsize[2]);
	CustomData_bmesh_init_pool(&bm->pdata, allocsize[3]);
	/*needed later*/
	numTex = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	numCol = CustomData_number_of_layers(&bm->ldata, CD_MLOOPCOL);

	/*copy over selection mode*/
	bm->selectmode = 0;
	if(em->selectmode & SCE_SELECT_VERTEX) bm->selectmode |= BM_VERT;
	if(em->selectmode & SCE_SELECT_EDGE) bm->selectmode |= BM_EDGE;
	if(em->selectmode & SCE_SELECT_FACE) bm->selectmode |= BM_FACE;


	/*begin editloop*/
	//BM_Begin_Edit(bm);

	/*tag wire edges*/
	tag_wire_edges(em);

	/*add verts*/
	for(eve = em->verts.first; eve; eve = eve->next){
		v = editvert_to_BMVert(bm, em, eve);
		eve->tmp.p = v;
	}
	/*convert f-gons*/
	BM_fgonconvert(bm, em, numCol, numTex);
	
	/*do quads + triangles*/
	for(efa = em->faces.first; efa; efa = efa->next){
		if(!efa->tmp.l) editface_to_BMFace(bm, em, efa, numCol, numTex);
	}

	/*add wire edges*/	
	for(eed = em->edges.first; eed; eed = eed->next){
		if(eed->f1) editedge_to_BMEdge(bm, em, eed);
	}
	//BM_end_edit(bm, BM_CALC_NORM);
	return bm;
}

void edit2bmesh_exec(BMesh *bmesh, BMOperator *op)
{
	editmesh_to_bmesh_intern(op->slots[BMOP_FROM_EDITMESH_EM].data.p, bmesh);
}

BMesh *editmesh_to_bmesh(EditMesh *em)
{
	BMOperator conv;
	BMesh *bm;
	int allocsize[4] = {512,512,2048,512}, numTex, numCol;

	/*allocate a bmesh*/
	bm = BM_Make_Mesh(allocsize);

	BMO_Init_Op(&conv, BMOP_FROM_EDITMESH);
	BMO_Set_Pnt(&conv, BMOP_FROM_EDITMESH_EM, em);
	BMO_Exec_Op(bm, &conv);
	BMO_Finish_Op(bm, &conv);

	return bm;
}