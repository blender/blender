
#include "MEM_guardedalloc.h"
#include "BKE_customdata.h" 
#include "DNA_listBase.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
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
 * MESH CONV.C
 *
 * This file contains functions
 * for converting a Mesh
 * into a Bmesh.  will not support non-ngon
 * meshes at first, use the editmesh functions
 * until it's implemented, and remove this
 * comment if it already is. -joeedh
 *
*/

void mesh_to_bmesh_exec(BMesh *bm, BMOperator *op) {
	Mesh *me = BMO_Get_Pnt(op, "mesh");
	MVert *mvert;
	MEdge *medge;
	MLoop *ml;
	MPoly *mpoly;
	BMVert *v, **vt=NULL;
	BMEdge *e, **fedges=NULL, **et;
	V_DECLARE(fedges);
	BMFace *f;
	int i, j, li, allocsize[4] = {512, 512, 2048, 512};

	if (!me || !me->totvert) return; /*sanity check*/
	
	mvert = me->mvert;

	vt = MEM_mallocN(sizeof(void**)*me->totvert, "mesh to bmesh vtable");

	CustomData_copy(&me->vdata, &bm->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&me->edata, &bm->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&me->ldata, &bm->ldata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&me->pdata, &bm->pdata, CD_MASK_BMESH, CD_CALLOC, 0);

	CustomData_bmesh_init_pool(&bm->vdata, allocsize[0]);
	CustomData_bmesh_init_pool(&bm->edata, allocsize[1]);
	CustomData_bmesh_init_pool(&bm->ldata, allocsize[2]);
	CustomData_bmesh_init_pool(&bm->pdata, allocsize[3]);

	for (i=0; i<me->totvert; i++, mvert++) {
		v = BM_Make_Vert(bm, mvert->co, NULL);
		VECCOPY(v->no, mvert->no);

		vt[i] = v;
		BMINDEX_SET(v, i);

		/*transfer flags*/
		v->head.flag = (mvert->flag & ME_HIDE) ? BM_HIDDEN : 0;
		if(mvert->flag & SELECT) BM_Select_Vert(bm, v, 1);
		v->bweight = (float)mvert->bweight / 255.0f;

		/*Copy Custom Data*/
		CustomData_to_bmesh_block(&me->vdata, &bm->vdata, i, &v->head.data);

		v->head.flag = MEFlags_To_BMFlags(mvert->flag, BM_VERT);
	}

	if (!me->totedge) return;

	et = MEM_mallocN(sizeof(void**)*me->totedge, "mesh to bmesh etable");

	medge = me->medge;
	for (i=0; i<me->totedge; i++, medge++) {
		e = BM_Make_Edge(bm, vt[medge->v1], vt[medge->v2], NULL, 0);
		et[i] = e;
		
		/*Copy Custom Data*/
		CustomData_to_bmesh_block(&me->edata, &bm->edata, i, &e->head.data);
		
		e->crease = (float)medge->crease / 255.0f;
		e->bweight = (float)medge->bweight / 255.0f;

		e->head.flag = MEFlags_To_BMFlags(medge->flag, BM_EDGE);
	}
	
	if (!me->totpoly) return;

	mpoly = me->mpoly;
	li = 0;
	for (i=0; i<me->totpoly; i++, mpoly++) {
		BMVert *v1, *v2;
		BMIter iter;
		BMLoop *l;

		V_RESET(fedges);
		for (j=0; j<mpoly->totloop; j++) {
			ml = &me->mloop[mpoly->loopstart+j];
			v = vt[ml->v];
			e = et[ml->e];

			V_GROW(fedges);

			fedges[j] = e;
		}
		
		v1 = vt[me->mloop[mpoly->loopstart].v];
		v2 = vt[me->mloop[mpoly->loopstart+1].v];

		if (v1 == fedges[0]->v1) v2 = fedges[0]->v2;
		else {
			v1 = fedges[0]->v2;
			v2 = fedges[0]->v1;
		}

		f = BM_Make_Ngon(bm, v1, v2, fedges, mpoly->totloop, 0);
		
		f->head.flag = MEFlags_To_BMFlags(mpoly->flag, BM_FACE);
		f->mat_nr = mpoly->mat_nr;
		if (i == me->act_face) bm->act_face = f;

		/*Copy over loop customdata*/
		BM_ITER(l, &iter, bm, BM_LOOPS_OF_FACE, f) {
			CustomData_to_bmesh_block(&me->ldata, &bm->ldata, li, &l->head.data);
			li++;
		}

		/*Copy Custom Data*/
		CustomData_to_bmesh_block(&me->pdata, &bm->pdata, i, &f->head.data);
	}

	V_FREE(fedges);
}

void bmesh_to_mesh_exec(BMesh *bm, BMOperator *op) {
	BMesh *bmtess;
	Object *ob = BMO_Get_Pnt(op, "object");
	Scene *scene = BMO_Get_Pnt(op, "scene");
	Mesh *me = ob->data;
	MLoop *mloop;
	MPoly *mpoly;
	MVert *mvert, *oldverts;
	MEdge *medge;
	MFace *mface;
	BMVert *v;
	BMEdge *e;
	BMLoop *l;
	BMFace *f;
	BMIter iter, liter;
	int i, j, ototvert, totloop, numTex, numCol;
	
	numTex = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	numCol = CustomData_number_of_layers(&bm->ldata, CD_MLOOPCOL);

	bmtess = BM_Copy_Mesh(bm);
	BMO_CallOpf(bmtess, "makefgon trifan=%i", 0);
	
	/* new Vertex block */
	if(bm->totvert==0) mvert= NULL;
	else mvert= MEM_callocN(bm->totvert*sizeof(MVert), "loadeditbMesh vert");

	/* new Edge block */
	if(bm->totedge==0) medge= NULL;
	else medge= MEM_callocN(bm->totedge*sizeof(MEdge), "loadeditbMesh edge");
	
	/* new Face block */
	if(bmtess->totface==0) mface= NULL;
	else mface= MEM_callocN(bmtess->totface*sizeof(MFace), "loadeditbMesh face");

	/*build ngon data*/
	/* new Ngon Face block */
	if(bm->totface==0) mpoly = NULL;
	else mpoly= MEM_callocN(bm->totface*sizeof(MPoly), "loadeditbMesh poly");
	
	/*find number of loops to allocate*/
	totloop = 0;
	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		totloop += f->len;
	}

	if (totloop==0) mloop = NULL;
	else mloop = MEM_callocN(totloop*sizeof(MLoop), "loadeditbMesh loop");

	/* lets save the old verts just in case we are actually working on
	 * a key ... we now do processing of the keys at the end */
	oldverts= me->mvert;

	/* don't free this yet */
	CustomData_set_layer(&me->vdata, CD_MVERT, NULL);

	/* free custom data */
	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);
	CustomData_free(&me->ldata, me->totloop);
	CustomData_free(&me->pdata, me->totpoly);

	/* add new custom data */
	me->totvert= bm->totvert;
	me->totedge= bm->totedge;
	me->totface= bmtess->totface;
	me->totloop= totloop;
	me->totpoly= bm->totface;

	CustomData_copy(&bm->vdata, &me->vdata, CD_MASK_MESH, CD_CALLOC, me->totvert);
	CustomData_copy(&bm->edata, &me->edata, CD_MASK_MESH, CD_CALLOC, me->totedge);
	CustomData_copy(&bm->ldata, &me->ldata, CD_MASK_MESH, CD_CALLOC, me->totloop);
	CustomData_copy(&bm->pdata, &me->pdata, CD_MASK_MESH, CD_CALLOC, me->totpoly);

	CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, mvert, me->totvert);
	CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, medge, me->totedge);
	CustomData_add_layer(&me->fdata, CD_MFACE, CD_ASSIGN, mface, me->totface);
	CustomData_add_layer(&me->ldata, CD_MLOOP, CD_ASSIGN, mloop, me->totloop);
	CustomData_add_layer(&me->pdata, CD_MPOLY, CD_ASSIGN, mpoly, me->totpoly);

	CustomData_from_bmeshpoly(&me->fdata, &bmtess->pdata, &bmtess->ldata, bmtess->totface);

	mesh_update_customdata_pointers(me);
	
	/*set indices*/
	i = 0;
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		VECCOPY(mvert->co, v->co);

		mvert->no[0] = (unsigned char) (v->no[0]*255.0f);
		mvert->no[1] = (unsigned char) (v->no[1]*255.0f);
		mvert->no[2] = (unsigned char) (v->no[2]*255.0f);
		
		mvert->flag = BMFlags_To_MEFlags(v);

		BMINDEX_SET(v, i);

		/*copy over customdata*/
		CustomData_from_bmesh_block(&bm->vdata, &me->vdata, v->head.data, i);

		i++;
		mvert++;
	}

	i = 0;
	BM_ITER(v, &iter, bmtess, BM_VERTS_OF_MESH, NULL) {
		BMINDEX_SET(v, i);
		i++;
	}

	i = 0;
	BM_ITER(e, &iter, bm, BM_EDGES_OF_MESH, NULL) {
		medge->v1 = BMINDEX_GET(e->v1);
		medge->v2 = BMINDEX_GET(e->v2);

		medge->flag = BMFlags_To_MEFlags(e);

		BMINDEX_SET(e, i);

		/*copy over customdata*/
		CustomData_from_bmesh_block(&bm->edata, &me->edata, e->head.data, i);

		i++;
		medge++;
	}
	
	i = 0;
	BM_ITER(f, &iter, bmtess, BM_FACES_OF_MESH, NULL) {
		mface->mat_nr = f->mat_nr;
		mface->flag = BMFlags_To_MEFlags(f);
		
		mface->v1 = BMINDEX_GET(f->loopbase->v);
		mface->v2 = BMINDEX_GET(((BMLoop*)f->loopbase->head.next)->v);
		if (f->len < 3) { 
			mface++;
			i++; 
			continue;
		}

		mface->v3 = BMINDEX_GET(((BMLoop*)f->loopbase->head.next->next)->v);
		if (f->len < 4) { 
			mface->v4 = 0;
			mface++;
			i++; 
			continue;
		}

		mface->v4 = BMINDEX_GET(((BMLoop*)f->loopbase->head.next->next->next)->v);
		test_index_face(mface, &me->fdata, i, 1);
		
		BM_loops_to_corners(bmtess, me, i, f, numTex, numCol);

		mface++;
		i++;
	}

	i = 0;
	j = 0;
	BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
		mpoly->loopstart = j;
		mpoly->totloop = f->len;
		mpoly->mat_nr = f->mat_nr;
		mpoly->flag = BMFlags_To_MEFlags(f);

		l = BMIter_New(&liter, bm, BM_LOOPS_OF_FACE, f);
		for ( ; l; l=BMIter_Step(&liter), j++, mloop++) {
			mloop->e = BMINDEX_GET(l->e);
			mloop->v = BMINDEX_GET(l->v);

			/*copy over customdata*/
			CustomData_from_bmesh_block(&bm->ldata, &me->ldata, l->head.data, j);
		}
		
		if (f == bm->act_face) me->act_face = i;

		/*copy over customdata*/
		CustomData_from_bmesh_block(&bm->pdata, &me->pdata, f->head.data, i);

		i++;
		mpoly++;
	}

	BM_Free_Mesh(bmtess);
}