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
#include "BIF_editmesh.h"
#include "editmesh.h"
#include "BLI_blenlib.h"
#include "BLI_edgehash.h"

#include "bmesh.h"

/*
 * BMESH TO EDITMESH
 *
 * This file contains functions for converting 
 * from a bmesh to an editmesh
 *
*/

/*
 * LOOPS TO EDITMESH CORNERS
 *
 * Converts N-Gon loop (face-edge)
 * data (UVs, Verts Colors, ect) to
 * face corner data.
 *
*/

static void loops_to_editmesh_corners(BMesh *bm, CustomData *facedata, void *face_block, BMFace *f,int numCol, int numTex){
	int i, j;
	BMLoop *l;
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;

	for(i=0; i < numTex; i++){
		texface = CustomData_em_get_n(facedata, face_block, CD_MTFACE, i);
		texpoly = CustomData_bmesh_get_n(&bm->pdata, f->data, CD_MTEXPOLY, i);
		
		texface->tpage = texpoly->tpage;
		texface->flag = texpoly->flag;
		texface->transp = texpoly->transp;
		texface->mode = texpoly->mode;
		texface->tile = texpoly->tile;
		texface->unwrap = texpoly->unwrap;

		j = 0;
		l = f->loopbase;
		do{
			mloopuv = CustomData_bmesh_get_n(&bm->ldata, l->data, CD_MLOOPUV, i);
			texface->uv[j][0] = mloopuv->uv[0];
			texface->uv[j][1] = mloopuv->uv[1];
			j++;
			l = ((BMLoop*)(l->head.next));
		}while(l!=f->loopbase);

	}
	for(i=0; i < numCol; i++){
		mcol = CustomData_em_get_n(facedata, face_block, CD_MCOL, i);
		j = 0;
		l = f->loopbase;
		do{
			mloopcol = CustomData_bmesh_get_n(&bm->ldata, l->data, CD_MLOOPCOL, i);
			mcol[j].r = mloopcol->r;
			mcol[j].g = mloopcol->g;
			mcol[j].b = mloopcol->b;
			mcol[j].a = mloopcol->a;
			j++;
			l = ((BMLoop*)(l->head.next));
		}while(l!=f->loopbase);
	}
}

static EditVert *bmeshvert_to_editvert(BMesh *bm, EditMesh *em, BMVert *v, int index, EditVert **evlist)
{
	EditVert *eve = NULL;

	v->head.eflag1 = index; /*abuse!*/
	eve = addvertlist(v->co,NULL);
	eve->keyindex = index;
	evlist[index]= eve;
	if(BM_Is_Selected(bm, v)) eve->f |= SELECT;
	if(v->head.flag & BM_HIDDEN) eve->h = 1;
	eve->bweight = v->bweight;
	CustomData_em_copy_data(&bm->vdata, &em->vdata, v->data, &eve->data);
	/*copy normal*/
	eve->no[0] = v->no[0];
	eve->no[1] = v->no[1];
	eve->no[2] = v->no[2];

	return eve;
}

static void bmeshedge_to_editedge_internal(BMesh *bm, EditMesh *em, BMEdge *e, EditEdge *eed)
{
	eed->crease = e->crease;
	eed->bweight = e->bweight;
	
	//copy relavent flags
	eed->f = e->head.flag & 65535;
	if (e->head.flag & BM_SEAM) eed->seam = 1;
	if (e->head.flag & BM_SHARP) eed->sharp = 1;
	if (e->head.flag & BM_HIDDEN) eed->h = 1;
	if (e->head.flag & BM_FGON) eed->h |= EM_FGON;
	
	CustomData_em_copy_data(&bm->edata, &em->edata, e->data, &eed->data);
}

static EditEdge *bmeshedge_to_editedge(BMesh *bm, EditMesh *em, BMEdge *e, EditVert **evlist)
{
	EditEdge *eed = NULL;

	if(!(findedgelist(evlist[e->v1->head.eflag1], evlist[e->v2->head.eflag1]))){
		eed= addedgelist(evlist[e->v1->head.eflag1], evlist[e->v2->head.eflag1], NULL);
		bmeshedge_to_editedge_internal(bm, em, e, eed);
	}

	return eed;
}

static EditFace *bmeshface_to_editface(BMesh *bm, EditMesh *em, BMFace *f, EditVert **evlist, int numCol, int numTex)
{
	EditVert *eve1, *eve2, *eve3, *eve4;
	EditFace *efa = NULL;
	int len;
	
	len = f->len;

	eve1= evlist[f->loopbase->v->head.eflag1];
	eve2= evlist[((BMLoop*)(f->loopbase->head.next))->v->head.eflag1];
	eve3= evlist[((BMLoop*)(f->loopbase->head.next->next))->v->head.eflag1];
	if (len >= 4) {
		eve4= evlist[ ((BMLoop*)(f->loopbase->head.prev))->v->head.eflag1];
	}
	else {
		eve4= NULL;
	}

	efa = addfacelist(eve1, eve2, eve3, eve4, NULL, NULL);

	bmeshedge_to_editedge_internal(bm, em, f->loopbase->e, efa->e1);
	bmeshedge_to_editedge_internal(bm, em, ((BMLoop*)(f->loopbase->head.next))->e, efa->e2);
	bmeshedge_to_editedge_internal(bm, em, ((BMLoop*)(f->loopbase->head.next->next))->e, efa->e3);
	if(eve4)
		bmeshedge_to_editedge_internal(bm, em, ((BMLoop*)(f->loopbase->head.prev))->e, efa->e4);

	efa->mat_nr = (unsigned char)f->mat_nr;


	/*Copy normal*/
	efa->n[0] = f->no[0];
	efa->n[1] = f->no[1];
	efa->n[2] = f->no[2];
	
	//copy relavent original flags
	efa->f = f->head.flag & 255;
	if (f->head.flag & BM_HIDDEN) efa->h = 1;
	if (f->head.flag & BM_SMOOTH) efa->flag |= ME_SMOOTH;

	CustomData_em_copy_data(&bm->pdata, &em->fdata, f->data, &efa->data);
	loops_to_editmesh_corners(bm, &em->fdata, efa->data, f, numCol,numTex);
	
	return efa;
}

EditMesh *bmesh_to_editmesh(BMesh *bm) 
{
	BMVert *v;
	BMEdge *e;
	BMFace *f;

	BMIter verts;
	BMIter edges;
	BMIter faces;

	EditMesh *em;
	EditVert *eve, **evlist;

	int totvert, i, numTex, numCol;

	em = G.editMesh;

	if (em == NULL) return NULL; //what?
	em->act_face = NULL ;

	CustomData_copy(&bm->vdata, &em->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm->edata, &em->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm->pdata, &em->fdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_from_bmeshpoly(&em->fdata, &bm->pdata, &bm->ldata,0);
	numTex = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	numCol = CustomData_number_of_layers(&bm->ldata, CD_MLOOPCOL);

	totvert = BM_Count_Element(bm, BM_VERT);
	evlist= MEM_mallocN(totvert*sizeof(EditVert *),"evlist");

	/* make vertices */
	for(i=0, v = BMIter_New(&verts, bm, BM_VERTS, bm); v; v = BMIter_Step(&verts), i++) 
		eve = bmeshvert_to_editvert(bm, em, v, i, evlist);

	/* make edges */
	for(e = BMIter_New(&edges, bm, BM_EDGES, bm); e; e = BMIter_Step(&edges))
		bmeshedge_to_editedge(bm, em, e, evlist);

	/* make faces */
	for(f = BMIter_New(&faces, bm, BM_FACES, bm); f; f = BMIter_Step(&faces))
		bmeshface_to_editface(bm, em, f, evlist, numCol, numTex);
			
	MEM_freeN(evlist);
	countall();
	return em;
}