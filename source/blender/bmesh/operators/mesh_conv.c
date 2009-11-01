#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_listBase.h"
#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_customdata.h" 
#include "BKE_utildefines.h"
#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"
#include "BKE_key.h"
#include "BKE_main.h"

#include "BLI_blenlib.h"
#include "BLI_edgehash.h"
#include "BLI_editVert.h"
#include "BLI_scanfill.h"
#include "BLI_array.h"

#include "ED_mesh.h"

#include "mesh_intern.h"
#include "bmesh.h"

/*
  HACK: we're using the old keyindex shape key hack for now, where each
  vertex is assigned an index on entering editmode, which is used to retrieve
  the original shapekey coordinates on exiting editmode.

  this should be replaced by proper use of customdata layers, but I'm not
  sure if that's such a good idea before merging with trunk or not.
*/

/*
 * MESH CONV.C
 *
 * This file contains functions
 * for converting a Mesh
 * into a Bmesh, and back again.
 *
*/

void mesh_to_bmesh_exec(BMesh *bm, BMOperator *op) {
	Object *ob = BMO_Get_Pnt(op, "object");
	Mesh *me = BMO_Get_Pnt(op, "mesh");
	MVert *mvert;
	MEdge *medge;
	MLoop *ml;
	MPoly *mpoly;
	KeyBlock *actkey;
	BMVert *v, **vt=NULL;
	BMEdge *e, **fedges=NULL, **et = NULL;
	BMFace *f;
	BLI_array_declare(fedges);
	float (*keyco)[3]= NULL;
	int *keyi;
	int i, j, li, allocsize[4] = {512, 512, 2048, 512};

	if (!me || !me->totvert) return; /*sanity check*/
	
	mvert = me->mvert;
	vt = MEM_mallocN(sizeof(void**)*me->totvert, "mesh to bmesh vtable");

	CustomData_copy(&me->vdata, &bm->vdata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&me->edata, &bm->edata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&me->ldata, &bm->ldata, CD_MASK_BMESH, CD_CALLOC, 0);
	CustomData_copy(&me->pdata, &bm->pdata, CD_MASK_BMESH, CD_CALLOC, 0);

	CustomData_add_layer(&bm->vdata, CD_SHAPE_KEYINDEX, CD_ASSIGN, NULL, 0);

	CustomData_bmesh_init_pool(&bm->vdata, allocsize[0]);
	CustomData_bmesh_init_pool(&bm->edata, allocsize[1]);
	CustomData_bmesh_init_pool(&bm->ldata, allocsize[2]);
	CustomData_bmesh_init_pool(&bm->pdata, allocsize[3]);
 
	actkey = ob_get_keyblock(ob);
	if(actkey && actkey->totelem == me->totvert) {
		keyco= actkey->data;
		bm->shapenr= ob->shapenr;
	} else if (actkey) {
		printf("shapekey<->mesh mismatch!\n");
	}
	
	for (i=0; i<me->totvert; i++, mvert++) {
		v = BM_Make_Vert(bm, keyco ? keyco[i] : mvert->co, NULL);
		VECCOPY(v->no, mvert->no);

		vt[i] = v;
		BMINDEX_SET(v, i);

		/*this is necassary for selection counts to work properly*/
		if(v->head.flag & BM_SELECT) BM_Select_Vert(bm, v, 1);

		/*transfer flags*/
		v->head.flag = MEFlags_To_BMFlags(mvert->flag, BM_VERT);
		v->bweight = (float)mvert->bweight / 255.0f;

		/*Copy Custom Data*/
		CustomData_to_bmesh_block(&me->vdata, &bm->vdata, i, &v->head.data);

		/*set shape key original index*/
		keyi = CustomData_bmesh_get(&bm->vdata, v->head.data, CD_SHAPE_KEYINDEX);
		*keyi = i;
	}

	if (!me->totedge) {
		MEM_freeN(vt);
		return;
	}

	et = MEM_mallocN(sizeof(void**)*me->totedge, "mesh to bmesh etable");

	medge = me->medge;
	for (i=0; i<me->totedge; i++, medge++) {
		e = BM_Make_Edge(bm, vt[medge->v1], vt[medge->v2], NULL, 0);
		et[i] = e;
		
		/*Copy Custom Data*/
		CustomData_to_bmesh_block(&me->edata, &bm->edata, i, &e->head.data);
		
		e->crease = (float)medge->crease / 255.0f;
		e->bweight = (float)medge->bweight / 255.0f;

		/*this is necassary for selection counts to work properly*/
		if (e->head.flag & BM_SELECT) BM_Select(bm, e, 1);

		/*transfer flags*/
		e->head.flag = MEFlags_To_BMFlags(medge->flag, BM_EDGE);
	}
	
	if (!me->totpoly) {
		MEM_freeN(vt);
		MEM_freeN(et);
		return;
	}

	mpoly = me->mpoly;
	li = 0;
	for (i=0; i<me->totpoly; i++, mpoly++) {
		BMVert *v1, *v2;
		BMIter iter;
		BMLoop *l;

		BLI_array_empty(fedges);
		for (j=0; j<mpoly->totloop; j++) {
			ml = &me->mloop[mpoly->loopstart+j];
			v = vt[ml->v];
			e = et[ml->e];

			BLI_array_growone(fedges);

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

		if (!f) {
			printf("Warning! Bad face in mesh"
			       " \"%s\" at index %d!\n", me->id.name+2, i);
			continue;
		}

		/*this is necassary for selection counts to work properly*/
		if (f->head.flag & BM_SELECT) BM_Select(bm, f, 1);

		/*transfer flags*/
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

	BLI_array_free(fedges);

	MEM_freeN(vt);
	MEM_freeN(et);
}


static void loops_to_corners(BMesh *bm, Mesh *me, int findex,
                             BMFace *f, BMLoop *ls[3], int numTex, int numCol) 
{
	BMLoop *l;
	MTFace *texface;
	MTexPoly *texpoly;
	MCol *mcol;
	MLoopCol *mloopcol;
	MLoopUV *mloopuv;
	int i, j;

	for(i=0; i < numTex; i++){
		texface = CustomData_get_n(&me->fdata, CD_MTFACE, findex, i);
		texpoly = CustomData_bmesh_get_n(&bm->pdata, f->head.data, CD_MTEXPOLY, i);
		
		texface->tpage = texpoly->tpage;
		texface->flag = texpoly->flag;
		texface->transp = texpoly->transp;
		texface->mode = texpoly->mode;
		texface->tile = texpoly->tile;
		texface->unwrap = texpoly->unwrap;

		for (j=0; j<3; j++) {
			l = ls[j];
			mloopuv = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPUV, i);
			texface->uv[j][0] = mloopuv->uv[0];
			texface->uv[j][1] = mloopuv->uv[1];
		}
	}

	for(i=0; i < numCol; i++){
		mcol = CustomData_get_n(&me->fdata, CD_MCOL, findex, i);

		for (j=0; j<3; j++) {
			l = ls[j];
			mloopcol = CustomData_bmesh_get_n(&bm->ldata, l->head.data, CD_MLOOPCOL, i);
			mcol[j].r = mloopcol->r;
			mcol[j].g = mloopcol->g;
			mcol[j].b = mloopcol->b;
			mcol[j].a = mloopcol->a;
		}
	}
}

void object_load_bmesh_exec(BMesh *bm, BMOperator *op) {
	Object *ob = BMO_Get_Pnt(op, "object");
	Scene *scene = BMO_Get_Pnt(op, "scene");
	Mesh *me = ob->data;

	BMO_CallOpf(bm, "bmesh_to_mesh mesh=%p object=%p", me, ob);

	/*BMESH_TODO eventually we'll have to handle shapekeys here*/
}

void bmesh_to_mesh_exec(BMesh *bm, BMOperator *op) {
	Mesh *me = BMO_Get_Pnt(op, "mesh");
	Object *ob = BMO_Get_Pnt(op, "object");
	MLoop *mloop;
	MPoly *mpoly;
	MVert *mvert, *oldverts;
	MEdge *medge;
	MFace *mface;
	BMVert *v, *eve;
	BMEdge *e;
	BMLoop *l;
	BMFace *f;
	BMIter iter, liter;
	int i, j, *keyi, ototvert, totloop, totface, numTex, numCol;
	int dotess = !BMO_Get_Int(op, "notesselation");

	numTex = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	numCol = CustomData_number_of_layers(&bm->ldata, CD_MLOOPCOL);

	/* new Vertex block */
	if(bm->totvert==0) mvert= NULL;
	else mvert= MEM_callocN(bm->totvert*sizeof(MVert), "loadeditbMesh vert");

	/* new Edge block */
	if(bm->totedge==0) medge= NULL;
	else medge= MEM_callocN(bm->totedge*sizeof(MEdge), "loadeditbMesh edge");
	
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
	me->totloop= totloop;
	me->totpoly= bm->totface;

	CustomData_copy(&bm->vdata, &me->vdata, CD_MASK_MESH, CD_CALLOC, me->totvert);
	CustomData_copy(&bm->edata, &me->edata, CD_MASK_MESH, CD_CALLOC, me->totedge);
	CustomData_copy(&bm->ldata, &me->ldata, CD_MASK_MESH, CD_CALLOC, me->totloop);
	CustomData_copy(&bm->pdata, &me->pdata, CD_MASK_MESH, CD_CALLOC, me->totpoly);

	CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, mvert, me->totvert);
	CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, medge, me->totedge);
	CustomData_add_layer(&me->ldata, CD_MLOOP, CD_ASSIGN, mloop, me->totloop);
	CustomData_add_layer(&me->pdata, CD_MPOLY, CD_ASSIGN, mpoly, me->totpoly);

	i = 0;
	BM_ITER(v, &iter, bm, BM_VERTS_OF_MESH, NULL) {
		VECCOPY(mvert->co, v->co);

		mvert->no[0] = (short) (v->no[0]*32767.0f);
		mvert->no[1] = (short) (v->no[1]*32767.0f);
		mvert->no[2] = (short) (v->no[2]*32767.0f);
		
		mvert->flag = BMFlags_To_MEFlags(v);

		BMINDEX_SET(v, i);

		/*copy over customdata*/
		CustomData_from_bmesh_block(&bm->vdata, &me->vdata, v->head.data, i);

		i++;
		mvert++;
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

	/*new scanfill tesselation code*/
	if (dotess) {
		/*first counter number of faces we'll need*/
		totface = 0;
		BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
			EditVert *eve, *lasteve = NULL, *firsteve = NULL;
			EditFace *efa;
			
			i = 0;
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
				eve = BLI_addfillvert(l->v->co);
				eve->tmp.p = l;
				
				BMINDEX_SET(l, i);

				if (lasteve) {
					BLI_addfilledge(lasteve, eve);
				}

				lasteve = eve;
				if (!firsteve) firsteve = eve;

				i++;
			}

			BLI_addfilledge(lasteve, firsteve);
			BLI_edgefill(0, 0);

			for (efa=fillfacebase.first; efa; efa=efa->next)
				totface++;

			BLI_end_edgefill();
		}
		
		me->totface = totface;

		/* new tess face block */
		if(totface==0) mface= NULL;
		else mface= MEM_callocN(totface*sizeof(MFace), "loadeditbMesh face");

		CustomData_add_layer(&me->fdata, CD_MFACE, CD_ASSIGN, mface, me->totface);
		CustomData_from_bmeshpoly(&me->fdata, &bm->pdata, &bm->ldata, totface);

		mesh_update_customdata_pointers(me);
		
		i = 0;
		BM_ITER(f, &iter, bm, BM_FACES_OF_MESH, NULL) {
			EditVert *eve, *lasteve = NULL, *firsteve = NULL;
			EditFace *efa;
			BMLoop *ls[3];
			
			BM_ITER(l, &liter, bm, BM_LOOPS_OF_FACE, f) {
				eve = BLI_addfillvert(l->v->co);
				eve->tmp.p = l;

				if (lasteve) {
					BLI_addfilledge(lasteve, eve);
				}

				lasteve = eve;
				if (!firsteve) firsteve = eve;
			}

			BLI_addfilledge(lasteve, firsteve);
			BLI_edgefill(0, 0);

			for (efa=fillfacebase.first; efa; efa=efa->next) {
				ls[0] = efa->v1->tmp.p;
				ls[1] = efa->v2->tmp.p;
				ls[2] = efa->v3->tmp.p;
				
				/*ensure correct winding.  I believe this is
				  analogous to bubble sort on three elements.*/
				if (BMINDEX_GET(ls[0]) > BMINDEX_GET(ls[1])) {
					SWAP(BMLoop*, ls[0], ls[1]);
				}
				if (BMINDEX_GET(ls[1]) > BMINDEX_GET(ls[2])) {
					SWAP(BMLoop*, ls[1], ls[2]);
				}
				if (BMINDEX_GET(ls[0]) > BMINDEX_GET(ls[1])) {
					SWAP(BMLoop*, ls[0], ls[1]);
				}

				mface->mat_nr = f->mat_nr;
				mface->flag = BMFlags_To_MEFlags(f);
				
				mface->v1 = BMINDEX_GET(ls[0]->v);
				mface->v2 = BMINDEX_GET(ls[1]->v);
				mface->v3 = BMINDEX_GET(ls[2]->v);

				test_index_face(mface, &me->fdata, i, 1);
				
				loops_to_corners(bm, me, i, f, ls, numTex, numCol);
				mface++;
				i++;
			}
			BLI_end_edgefill();
		}
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

	/* patch hook indices and vertex parents */
	{
		Object *ob;
		ModifierData *md;
		BMVert **vertMap = NULL;
		int i,j;

		for (ob=G.main->object.first; ob; ob=ob->id.next) {
			if (ob->parent==ob && ELEM(ob->partype, PARVERT1,PARVERT3)) {
				
				/* duplicate code from below, make it function later...? */
				if (!vertMap) {
					vertMap = MEM_callocN(sizeof(*vertMap)*ototvert, "vertMap");
					
					BM_ITER(eve, &iter, bm, BM_VERTS_OF_MESH, NULL) {
						keyi = CustomData_bmesh_get(&bm->vdata, eve->head.data, CD_SHAPE_KEYINDEX);
						if (*keyi != ORIGINDEX_NONE)
							vertMap[*keyi] = eve;
					}
				}
				if(ob->par1 < ototvert) {
					eve = vertMap[ob->par1];
					if(eve) ob->par1= BMINDEX_GET(eve);
				}
				if(ob->par2 < ototvert) {
					eve = vertMap[ob->par2];
					if(eve) ob->par2= BMINDEX_GET(eve);
				}
				if(ob->par3 < ototvert) {
					eve = vertMap[ob->par3];
					if(eve) ob->par3= BMINDEX_GET(eve);
				}
				
			}
			if (ob->data==me) {
				for (md=ob->modifiers.first; md; md=md->next) {
					if (md->type==eModifierType_Hook) {
						HookModifierData *hmd = (HookModifierData*) md;

						if (!vertMap) {
							vertMap = MEM_callocN(sizeof(*vertMap)*ototvert, "vertMap");
							
							BM_ITER(eve, &iter, bm, BM_VERTS_OF_MESH, NULL) {
								keyi = CustomData_bmesh_get(&bm->vdata, eve->head.data, CD_SHAPE_KEYINDEX);
								if (*keyi != ORIGINDEX_NONE)
									vertMap[*keyi] = eve;
							}
						}
						
						for (i=j=0; i<hmd->totindex; i++) {
							if(hmd->indexar[i] < ototvert) {
								eve = vertMap[hmd->indexar[i]];
								
								if (eve) {
									hmd->indexar[j++] = BMINDEX_GET(eve);
								}
							}
							else j++;
						}

						hmd->totindex = j;
					}
				}
			}
		}

		if (vertMap) MEM_freeN(vertMap);
	}

	mesh_update_customdata_pointers(me);

	/* are there keys? */
	if(me->key) {
		float *fp, *newkey, *oldkey;
		KeyBlock *currkey;
		KeyBlock *actkey= BLI_findlink(&me->key->block, bm->shapenr-1);

		/* Lets reorder the key data so that things line up roughly
		 * with the way things were before editmode */
		currkey = me->key->block.first;
		while(currkey) {
			fp= newkey= MEM_callocN(me->key->elemsize*bm->totvert,  "currkey->data");
			oldkey = currkey->data;

			eve= BMIter_New(&iter, bm, BM_VERTS_OF_MESH, NULL);

			i = 0;
			mvert = me->mvert;
			while(eve) {
				keyi = CustomData_bmesh_get(&bm->vdata, eve->head.data, CD_SHAPE_KEYINDEX);
				if (*keyi >= 0 && *keyi < currkey->totelem) { // valid old vertex
					if(currkey == actkey) {
						if(actkey == me->key->refkey) {
							VECCOPY(fp, mvert->co);
						}
						else {
							VECCOPY(fp, mvert->co);
							if(oldverts) {
								VECCOPY(mvert->co, oldverts[*keyi].co);
							}
						}
					}
					else {
						if(oldkey) {
							VECCOPY(fp, oldkey + 3 * *keyi);
						}
					}
				}
				else {
					VECCOPY(fp, mvert->co);
				}
				fp+= 3;
				++i;
				++mvert;
				eve= BMIter_Step(&iter);
			}
			currkey->totelem= bm->totvert;
			if(currkey->data) MEM_freeN(currkey->data);
			currkey->data = newkey;
			
			currkey= currkey->next;
		}
	}

	if(oldverts) MEM_freeN(oldverts);
}