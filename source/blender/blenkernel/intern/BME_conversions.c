/**
 * BME_mesh.c    jan 2007
 *
 *	BMesh mesh level functions.
 *
 * $Id$
 *
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 * about this.	
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
 * The Original Code is Copyright (C) 2007 Blender Foundation.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Geoffrey Bantle, Levi Schooley.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"
#include "BKE_customdata.h" 

#include "DNA_listBase.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_utildefines.h"
#include "BKE_mesh.h"
#include "BKE_bmesh.h"
#include "BKE_global.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_edgehash.h"
//XXX #include "BIF_editmesh.h"
//XXX #include "editmesh.h"
#include "bmesh_private.h"

//XXX #include "BSE_edit.h"

/* XXX IMPORTANT: editmesh stuff doesn't belong in kernel! (ton) */

/*merge these functions*/
static void BME_DMcorners_to_loops(BME_Mesh *bm, CustomData *facedata, int index, BME_Poly *f, int numCol, int numTex){
	int i, j;
	BME_Loop *l;
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;

	for(i=0; i< numTex; i++){
		texface = CustomData_get_layer_n(facedata, CD_MTFACE, i);
		texpoly = CustomData_bmesh_get_n(&bm->pdata, f->data, CD_MTEXPOLY, i);

		texpoly->tpage = texface[index].tpage;
		texpoly->flag = texface[index].flag;
		texpoly->transp = texface[index].transp;
		texpoly->mode = texface[index].mode;
		texpoly->tile = texface[index].tile;
		texpoly->unwrap = texface[index].unwrap;

		j = 0;
		l = f->loopbase;
		do{
			mloopuv = CustomData_bmesh_get_n(&bm->ldata, l->data, CD_MLOOPUV, i);
			mloopuv->uv[0] = texface[index].uv[j][0];
			mloopuv->uv[1] = texface[index].uv[j][1];
			j++;
			l = l->next;
		}while(l!=f->loopbase);
	}

	for(i=0; i < numCol; i++){
		mcol = CustomData_get_layer_n(facedata, CD_MCOL, i);
		j = 0;
		l = f->loopbase;
		do{
			mloopcol = CustomData_bmesh_get_n(&bm->ldata, l->data, CD_MLOOPCOL, i);
			mloopcol->r = mcol[(index*4)+j].r;
			mloopcol->g = mcol[(index*4)+j].g;
			mloopcol->b = mcol[(index*4)+j].b;
			mloopcol->a = mcol[(index*4)+j].a;
			j++;
			l = l->next;
		}while(l!=f->loopbase);
	}
}

static void BME_DMloops_to_corners(BME_Mesh *bm, CustomData *facedata, int index, BME_Poly *f,int numCol, int numTex){
	int i, j;
	BME_Loop *l;
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;

	for(i=0; i < numTex; i++){
		texface = CustomData_get_layer_n(facedata, CD_MTFACE, i);
		texpoly = CustomData_bmesh_get_n(&bm->pdata, f->data, CD_MTEXPOLY, i);
		
		texface[index].tpage = texpoly->tpage;
		texface[index].flag = texpoly->flag;
		texface[index].transp = texpoly->transp;
		texface[index].mode = texpoly->mode;
		texface[index].tile = texpoly->tile;
		texface[index].unwrap = texpoly->unwrap;

		j = 0;
		l = f->loopbase;
		do{
			mloopuv = CustomData_bmesh_get_n(&bm->ldata, l->data, CD_MLOOPUV, i);
			texface[index].uv[j][0] = mloopuv->uv[0];
			texface[index].uv[j][1] = mloopuv->uv[1];
			j++;
			l = l->next;
		}while(l!=f->loopbase);

	}
	for(i=0; i < numCol; i++){
		mcol = CustomData_get_layer_n(facedata,CD_MCOL, i);
		j = 0;
		l = f->loopbase;
		do{
			mloopcol = CustomData_bmesh_get_n(&bm->ldata, l->data, CD_MLOOPCOL, i);
			mcol[(index*4) + j].r = mloopcol->r;
			mcol[(index*4) + j].g = mloopcol->g;
			mcol[(index*4) + j].b = mloopcol->b;
			mcol[(index*4) + j].a = mloopcol->a;
			j++;
			l = l->next;
		}while(l!=f->loopbase);
	}
}


static void BME_corners_to_loops(BME_Mesh *bm, CustomData *facedata, void *face_block, BME_Poly *f,int numCol, int numTex){
	int i, j;
	BME_Loop *l;
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;

	for(i=0; i < numTex; i++){
		texface = CustomData_em_get_n(facedata, face_block, CD_MTFACE, i);
		texpoly = CustomData_bmesh_get_n(&bm->pdata, f->data, CD_MTEXPOLY, i);
		
		texpoly->tpage = texface->tpage;
		texpoly->flag = texface->flag;
		texpoly->transp = texface->transp;
		texpoly->mode = texface->mode;
		texpoly->tile = texface->tile;
		texpoly->unwrap = texface->unwrap;

		j = 0;
		l = f->loopbase;
		do{
			mloopuv = CustomData_bmesh_get_n(&bm->ldata, l->data, CD_MLOOPUV, i);
			mloopuv->uv[0] = texface->uv[j][0];
			mloopuv->uv[1] = texface->uv[j][1];
			j++;
			l = l->next;
		}while(l!=f->loopbase);

	}
	for(i=0; i < numCol; i++){
		mcol = CustomData_em_get_n(facedata, face_block, CD_MCOL, i);
		j = 0;
		l = f->loopbase;
		do{
			mloopcol = CustomData_bmesh_get_n(&bm->ldata, l->data, CD_MLOOPCOL, i);
			mloopcol->r = mcol[j].r;
			mloopcol->g = mcol[j].g;
			mloopcol->b = mcol[j].b;
			mloopcol->a = mcol[j].a;
			j++;
			l = l->next;
		}while(l!=f->loopbase);
	}
}

static void BME_loops_to_corners(BME_Mesh *bm, CustomData *facedata, void *face_block, BME_Poly *f,int numCol, int numTex){
	int i, j;
	BME_Loop *l;
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
			l = l->next;
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
			l = l->next;
		}while(l!=f->loopbase);
	}
}
/*move the EditMesh conversion functions to editmesh_tools.c*/
BME_Mesh *BME_editmesh_to_bmesh(EditMesh *em) {
	BME_Mesh *bm;
	int allocsize[4] = {512,512,2048,512}, numTex, numCol;
	BME_Vert *v1, *v2;
	BME_Edge *e, *edar[4];
	BME_Poly *f;

	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;

	int len;
	bm = BME_make_mesh(allocsize);

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

	BME_model_begin(bm);
	/*add verts*/
	eve= em->verts.first;
	while(eve) {
		v1 = BME_MV(bm,eve->co);
		VECCOPY(v1->no,eve->no);
		v1->flag = eve->f;
		v1->h = eve->h;
		v1->bweight = eve->bweight;
		/*Copy Custom Data*/
		CustomData_bmesh_copy_data(&em->vdata, &bm->vdata, eve->data, &v1->data);
		eve->tmp.v = (EditVert*)v1;
		eve = eve->next;
	}
	
	/*add edges*/
	eed= em->edges.first;
	while(eed) {
		v1 = (BME_Vert*)eed->v1->tmp.v;
		v2 = (BME_Vert*)eed->v2->tmp.v;
		e = BME_ME(bm, v1, v2);
		e->crease = eed->crease;
		e->bweight = eed->bweight;
		e->flag = eed->f & SELECT;
		if(eed->sharp) e->flag |= ME_SHARP;
		if(eed->seam) e->flag |= ME_SEAM;
		//XXX if(eed->h & EM_FGON) e->flag |= ME_FGON;
		if(eed->h & 1) e->flag |= ME_HIDE;
		eed->tmp.e = (EditEdge*)e;
		CustomData_bmesh_copy_data(&em->edata, &bm->edata, eed->data, &e->data);
		eed = eed->next;
	}
	/*add faces.*/
	efa= em->faces.first;
	while(efa) {
		if(efa->v4) len = 4;
		else len = 3;
		
		edar[0] = (BME_Edge*)efa->e1->tmp.e;
		edar[1] = (BME_Edge*)efa->e2->tmp.e;
		edar[2] = (BME_Edge*)efa->e3->tmp.e;
		if(len == 4){
			edar[3] = (BME_Edge*)efa->e4->tmp.e;
		}
		
		/*find v1 and v2*/
		v1 = (BME_Vert*)efa->v1->tmp.v;
		v2 = (BME_Vert*)efa->v2->tmp.v;
		
		f = BME_MF(bm,v1,v2,edar,len);
		f->mat_nr = efa->mat_nr;
		f->flag = efa->flag;
		if(efa->h) {
			f->flag |= ME_HIDE;
			f->flag &= ~ME_FACE_SEL;
		}
		else {
			if(efa->f & 1) f->flag |= ME_FACE_SEL;
			else f->flag &= ~ME_FACE_SEL;
		}
		CustomData_bmesh_copy_data(&em->fdata, &bm->pdata, efa->data, &f->data);
		BME_corners_to_loops(bm, &em->fdata, efa->data, f,numCol,numTex);
		efa = efa->next;
	}
	BME_model_end(bm);
	return bm;
}
/* adds the geometry in the bmesh to editMesh (does not free editMesh)
 * if td != NULL, the transdata will be mapped to the EditVert's co */
void BME_bmesh_to_editmesh(BME_Mesh *bm, BME_TransData_Head *td, EditMesh *em) {
	BME_Vert *v1;
	BME_Edge *e;
	BME_Poly *f;
	
	BME_TransData *vtd;

	EditVert *eve1, *eve2, *eve3, *eve4, **evlist;
	EditEdge *eed;
	EditFace *efa;

	int totvert, len, i, numTex, numCol;

	if (em == NULL) return;

	CustomData_copy(&bm->vdata, &em->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm->edata, &em->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&bm->pdata, &em->fdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_from_bmeshpoly(&em->fdata, &bm->pdata, &bm->ldata,0);
	numTex = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	numCol = CustomData_number_of_layers(&bm->ldata, CD_MLOOPCOL);


	/* convert to EditMesh */
	/* make editverts */
	totvert = BLI_countlist(&(bm->verts));
	evlist= (EditVert **)MEM_mallocN(totvert*sizeof(void *),"evlist");
	for (i=0,v1=bm->verts.first;v1;v1=v1->next,i++) {
		v1->tflag1 = i;
		eve1 = NULL; //XXX addvertlist(v1->co,NULL);
		if (td && (vtd = BME_get_transdata(td,v1))) {
			vtd->loc = eve1->co;
		}
		eve1->keyindex = i;
		evlist[i]= eve1;
		eve1->f = (unsigned char)v1->flag;
		eve1->h = (unsigned char)v1->h;
		eve1->bweight = v1->bweight;
		CustomData_em_copy_data(&bm->vdata, &em->vdata, v1->data, &eve1->data);
	}
	
	/* make edges */
	for (e=bm->edges.first;e;e=e->next) {
		if(0) { //XXX if(!(findedgelist(evlist[e->v1->tflag1], evlist[e->v2->tflag1]))){
			eed= NULL; //XXX addedgelist(evlist[e->v1->tflag1], evlist[e->v2->tflag1], NULL);
			eed->crease = e->crease;
			eed->bweight = e->bweight;
			if(e->flag & ME_SEAM) eed->seam = 1;
			if(e->flag & ME_SHARP) eed->sharp = 1;
			if(e->flag & SELECT) eed->f |= SELECT;
			//XXX if(e->flag & ME_FGON) eed->h= EM_FGON; // 2 different defines!
			if(e->flag & ME_HIDE) eed->h |= 1;
			if(em->selectmode==SCE_SELECT_EDGE) 
				; //XXX EM_select_edge(eed, eed->f & SELECT);
		
			CustomData_em_copy_data(&bm->edata, &em->edata, e->data, &eed->data);
		}
	}

	/* make faces */
	for (f=bm->polys.first;f;f=f->next) {
		len = BME_cycle_length(f->loopbase);
		if (len==3 || len==4) {
			eve1= evlist[f->loopbase->v->tflag1];
			eve2= evlist[f->loopbase->next->v->tflag1];
			eve3= evlist[f->loopbase->next->next->v->tflag1];
			if (len == 4) {
				eve4= evlist[f->loopbase->prev->v->tflag1];
			}
			else {
				eve4= NULL;
			}

			efa = NULL; //XXX addfacelist(eve1, eve2, eve3, eve4, NULL, NULL);
			efa->mat_nr = (unsigned char)f->mat_nr;
			efa->flag= f->flag & ~ME_HIDE;
			if(f->flag & ME_FACE_SEL) {
				efa->f |= SELECT;
			}
			if(f->flag & ME_HIDE) efa->h= 1;
			// XXX flag depricated
			// if((G.f & G_FACESELECT) && (efa->f & SELECT))
				//XXX EM_select_face(efa, 1); /* flush down */
			CustomData_em_copy_data(&bm->pdata, &em->fdata, f->data, &efa->data);
			BME_loops_to_corners(bm, &em->fdata, efa->data, f,numCol,numTex);
		}
	}

	MEM_freeN(evlist);

}

/* Adds the geometry found in dm to bm
  */
BME_Mesh *BME_derivedmesh_to_bmesh(DerivedMesh *dm)
{
	
	BME_Mesh *bm;
	int allocsize[4] = {512,512,2048,512};
	MVert *mvert, *mv;
	MEdge *medge, *me;
	MFace *mface, *mf;
	int totface,totedge,totvert,i,len, numTex, numCol;
	BME_Vert *v1=NULL,*v2=NULL, **vert_array;
	BME_Edge *e=NULL;
	BME_Poly *f=NULL;
	
	EdgeHash *edge_hash = BLI_edgehash_new();

	bm = BME_make_mesh(allocsize);
	/*copy custom data layout*/
	CustomData_copy(&dm->vertData, &bm->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&dm->edgeData, &bm->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&dm->faceData, &bm->pdata, CD_MASK_BMESH, CD_CALLOC, 0);

	/*copy face corner data*/
	CustomData_to_bmeshpoly(&dm->faceData, &bm->pdata, &bm->ldata);
	/*initialize memory pools*/
	CustomData_bmesh_init_pool(&bm->vdata, allocsize[0]);
	CustomData_bmesh_init_pool(&bm->edata, allocsize[1]);
	CustomData_bmesh_init_pool(&bm->ldata, allocsize[2]);
	CustomData_bmesh_init_pool(&bm->pdata, allocsize[3]);
	/*needed later*/
	numTex = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	numCol = CustomData_number_of_layers(&bm->ldata, CD_MLOOPCOL);

	totvert = dm->getNumVerts(dm);
	totedge = dm->getNumEdges(dm);
	totface = dm->getNumFaces(dm);
	mvert = dm->getVertArray(dm);
	medge = dm->getEdgeArray(dm);
	mface = dm->getFaceArray(dm);

	vert_array = MEM_mallocN(sizeof(*vert_array)*totvert,"BME_derivedmesh_to_bmesh BME_Vert* array");

	BME_model_begin(bm);
	/*add verts*/
	for(i=0,mv = mvert; i < totvert;i++,mv++){
		v1 = BME_MV(bm,mv->co);
		vert_array[i] = v1;
		v1->flag = mv->flag;
		v1->bweight = mv->bweight/255.0f;
		CustomData_to_bmesh_block(&dm->vertData, &bm->vdata, i, &v1->data);
	}
	/*add edges*/
	for(i=0,me = medge; i < totedge;i++,me++){
		v1 = vert_array[me->v1];
		v2 = vert_array[me->v2];
		e = BME_ME(bm, v1, v2);
		e->crease = me->crease/255.0f;
		e->bweight = me->bweight/255.0f;
		e->flag = (unsigned char)me->flag;
		BLI_edgehash_insert(edge_hash,me->v1,me->v2,e);
		CustomData_to_bmesh_block(&dm->edgeData, &bm->edata, i, &e->data);
	}
	/*add faces.*/
	for(i=0,mf = mface; i < totface;i++,mf++){
		BME_Edge *edar[4];
		if(mf->v4) len = 4;
		else len = 3;
		
		edar[0] = BLI_edgehash_lookup(edge_hash,mf->v1,mf->v2);
		edar[1] = BLI_edgehash_lookup(edge_hash,mf->v2,mf->v3);
		if(len == 4){
			edar[2] = BLI_edgehash_lookup(edge_hash,mf->v3,mf->v4);
			edar[3] = BLI_edgehash_lookup(edge_hash,mf->v4,mf->v1);
		}
		else
			edar[2] = BLI_edgehash_lookup(edge_hash,mf->v3,mf->v1);
		
		/*find v1 and v2*/
		v1 = vert_array[mf->v1];
		v2 = vert_array[mf->v2];
		
		f = BME_MF(bm,v1,v2,edar,len);
		f->mat_nr = mf->mat_nr;
		f->flag = mf->flag;
		CustomData_to_bmesh_block(&dm->faceData,&bm->pdata,i,&f->data);
		BME_DMcorners_to_loops(bm, &dm->faceData,i,f, numCol,numTex);
	}
	
	BME_model_end(bm);
	BLI_edgehash_free(edge_hash, NULL);
	MEM_freeN(vert_array);
	return bm;
}

DerivedMesh *BME_bmesh_to_derivedmesh(BME_Mesh *bm, DerivedMesh *dm)
{
	MFace *mface, *mf;
	MEdge *medge, *me;
	MVert *mvert, *mv;
	int totface,totedge,totvert,i,bmeshok,len, numTex, numCol;

	BME_Vert *v1=NULL;
	BME_Edge *e=NULL, *oe=NULL;
	BME_Poly *f=NULL;
	
	DerivedMesh *result;
	EdgeHash *edge_hash = BLI_edgehash_new();

	totvert = BLI_countlist(&(bm->verts));
	totedge = 0;
	
	/*we cannot have double edges in a derived mesh!*/
	for(i=0, v1=bm->verts.first; v1; v1=v1->next, i++) v1->tflag1 = i;
	for(e=bm->edges.first; e; e=e->next){
		oe = BLI_edgehash_lookup(edge_hash,e->v1->tflag1, e->v2->tflag1);
		if(!oe){
			totedge++;
			BLI_edgehash_insert(edge_hash,e->v1->tflag1,e->v2->tflag1,e);
			e->tflag2 = 1;
		}
		else{
			e->tflag2 = 0;
		}
	}
	
	/*count quads and tris*/
	totface = 0;
	bmeshok = 1;
	for(f=bm->polys.first;f;f=f->next){
		len = BME_cycle_length(f->loopbase);
		if(len == 3 || len == 4) totface++;
	}
	
	/*convert back to mesh*/
	result = CDDM_from_template(dm,totvert,totedge,totface);
	CustomData_merge(&bm->vdata, &result->vertData, CD_MASK_BMESH, CD_CALLOC, totvert);
	CustomData_merge(&bm->edata, &result->edgeData, CD_MASK_BMESH, CD_CALLOC, totedge);
	CustomData_merge(&bm->pdata, &result->faceData, CD_MASK_BMESH, CD_CALLOC, totface);
	CustomData_from_bmeshpoly(&result->faceData, &bm->pdata, &bm->ldata,totface);
	numTex = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	numCol = CustomData_number_of_layers(&bm->ldata, CD_MLOOPCOL);


	/*Make Verts*/
	mvert = CDDM_get_verts(result);
	for(i=0,v1=bm->verts.first,mv=mvert;v1;v1=v1->next,i++,mv++){
		VECCOPY(mv->co,v1->co);
		mv->flag = (unsigned char)v1->flag;
		mv->bweight = (char)(255.0*v1->bweight);
		CustomData_from_bmesh_block(&bm->vdata, &result->vertData, &v1->data, i);
	}
	medge = CDDM_get_edges(result);
	i=0;
	for(e=bm->edges.first,me=medge;e;e=e->next){
		if(e->tflag2){
			if(e->v1->tflag1 < e->v2->tflag1){
				me->v1 = e->v1->tflag1;
				me->v2 = e->v2->tflag1;
			}
			else{
				me->v1 = e->v2->tflag1;
				me->v2 = e->v1->tflag1;
			}
		
			me->crease = (char)(255.0*e->crease);
			me->bweight = (char)(255.0*e->bweight);
			me->flag = e->flag;
			CustomData_from_bmesh_block(&bm->edata, &result->edgeData, &e->data, i);
			me++;
			i++;
		}
	}
	if(totface){
		mface = CDDM_get_faces(result);
		/*make faces*/
		for(i=0,f=bm->polys.first;f;f=f->next){
			mf = &mface[i];
			len = BME_cycle_length(f->loopbase);
			if(len==3 || len==4){
				mf->v1 = f->loopbase->v->tflag1;
				mf->v2 = f->loopbase->next->v->tflag1;
				mf->v3 = f->loopbase->next->next->v->tflag1;
				if(len == 4){
					mf->v4 = f->loopbase->prev->v->tflag1;
				}
				/* test and rotate indexes if necessary so that verts 3 and 4 aren't index 0 */
				if(mf->v3 == 0 || (len == 4 && mf->v4 == 0)){
					test_index_face(mf, NULL, i, len);
				}
				mf->mat_nr = (unsigned char)f->mat_nr;
				mf->flag = (unsigned char)f->flag;
				CustomData_from_bmesh_block(&bm->pdata, &result->faceData, &f->data, i);
				BME_DMloops_to_corners(bm, &result->faceData, i, f,numCol,numTex);
				i++;
			}
		}
	}
	BLI_edgehash_free(edge_hash, NULL);
	return result;
}
