#if 0

/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
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
#include "bmesh.h"
#include "BKE_global.h"
#include "BKE_DerivedMesh.h"
#include "BKE_cdderivedmesh.h"

#include "BLI_blenlib.h"
#include "BLI_editVert.h"
#include "BLI_edgehash.h"
#include "bmesh_private.h"



/*
 * BMESH DERIVED MESH CONVERSION FUNCTIONS
 *
 * The functions in this file provides
 * methods for converting to and from
 * a bmesh. 
 *
*/


/*
 * DMCORNERS TO LOOPS
 *
 * Function to convert derived mesh per-face
 * corner data (uvs, vertex colors), to n-gon
 * per-loop data.
 *
*/

static void DMcorners_to_loops(BMMesh *bm, CustomData *facedata, int index, BMFace *f, int numCol, int numTex)
{
	int i, j;
	BMLoop *l;
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

/*
 * LOOPS TO DMCORNERS
 *
 * Function to convert n-gon per-loop data
 * (uvs, vertex colors, ect)to derived mesh
 * face corner data.
 *
*/

static void loops_to_DMcorners(BMMesh *bm, CustomData *facedata, int index, BMFace *f,int numCol, int numTex)
{
	int i, j;
	BMLoop *l;
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

/*
 * MVERT TO BMESHVERT
 *
 * Converts a MVert to a BMVert
 *
*/
static BMVert *mvert_to_bmeshvert(BMMesh *bm, BMVert **vert_array, int index, MVert *mv, CustomData *data)
{
		BMVert *v = NULL;
		
		v = bmesh_make_vert(bm, mv->co, NULL);
		vert_array[index] = v;
		if(mv->flag & SELECT) bmesh_set_flag(v, BMESH_SELECT);
		v->bweight = mv->bweight/255.0f;
		CustomData_to_bmesh_block(data, &bm->vdata, index, &v->data);
	
		return v;
}

/*
 * MEDGE TO BMESHEDGE
 *
 * Converts a MEdge to a BMEdge
 *
*/

static BMEdge *medge_to_bmeshedge(BMMesh *bm, BMVert **vert_array, int index, MEdge *me, CustomData *data, Edge_Hash *edge_hash)
{
		BMVert *v1, *v2;
		BMEdge *e = NULL;

		v1 = vert_array[me->v1];
		v2 = vert_array[me->v2];
		e = bmesh_make_edge(bm, v1, v2, NULL, 0);
		e->crease = me->crease/255.0f;
		e->bweight = me->bweight/255.0f;
		if(me->flag & 1) bmesh_set_flag(e, BMESH_SELECT);
		if(me->flag & ME_SEAM) bmesh_set_flag(e, BMESH_SEAM);
		BLI_edgehash_insert(edge_hash,me->v1,me->v2,e);
		CustomData_to_bmesh_block(data, &bm->edata, index, &e->data);

		return e;
}

/*
 * MFACE TO BMESHFACE
 *
 * Converts a MFace to a BMFace.
 * Note that this will fail on eekadoodle
 * faces.
 *
*/

static BMFace *mface_to_bmeshface(BMMesh *bm, BMVert **vert_array, int index, MFace *mf, CustomData *data, Edge_Hash *edge_hash)
{
		BMVert *v1, *v2;
		BMEdge *edar[4];
		BMFace *f = NULL;
		int len;

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
		
		f = bmesh_make_ngon(bm, v1, v2, edar, len, 0);
		f->mat_nr = mf->mat_nr;
		if(mf->flag & 1) bmesh_set_flag(f, BMESH_SELECT);
		if(mf->flag & ME_HIDE) bmesh_set_flag(f, BMESH_HIDDEN);
		CustomData_to_bmesh_block(data, &bm->pdata, index, &f->data);

		return f;
}

/*
 * DERIVEDMESH TO BMESH
 *
 * Converts a derived mesh to a bmesh.
 *
*/

BMMesh *derivedmesh_to_bmesh(DerivedMesh *dm)
{
	
	BMMesh *bm;
	BMVert **vert_array;
	BMFace *f=NULL;
	
	MVert *mvert, *mv;
	MEdge *medge, *me;
	MFace *mface, *mf;
	
	int totface,totedge,totvert,i,len, numTex, numCol;
	int allocsize[4] = {512,512,2048,512};
	
	EdgeHash *edge_hash = BLI_edgehash_new();

	/*allocate a new bmesh*/
	bm = bmesh_make_mesh(allocsize);

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
	totface = dm->getNumTessFaces(dm);
	mvert = dm->getVertArray(dm);
	medge = dm->getEdgeArray(dm);
	mface = dm->getTessFaceArray(dm);

	vert_array = MEM_mallocN(sizeof(BMVert *)* totvert,"derivedmesh to bmesh vertex pointer array");

	bmesh_begin_edit(bm);
	/*add verts*/
	for(i=0, mv = mvert; i < totvert; i++, mv++)
		mvert_to_bmeshvert(bm, vert_array, i, mv, &dm->vertData);
	
	/*add edges*/
	for(i=0, me = medge; i < totedge; i++, me++)
		medge_to_bmeshedge(bm, vert_array, i, me, &dm->edgeData, edge_hash);

	/*add faces.*/
	for(i=0, mf = mface; i < totface; i++, mf++){
		f = mface_to_bmeshface(bm, vert_array, mf, &dm->faceData, edge_hash);
		if(f) DMcorners_to_loops(bm, &dm->faceData, i, f, numCol, numTex);
	}
	
	bmesh_end__edit(bm);
	BLI_edgehash_free(edge_hash, NULL);
	MEM_freeN(vert_array);
	return bm;
}

static void bmeshvert_to_mvert(BMMesh *bm, BMVert *v, MVert *mv, int index, CustomData *data)
{
		copy_v3_v3(mv->co, v->co);
		if(bmesh_test_flag(v, BMESH_SELECT)) mv->flag |= 1;
		if(bmesh_test_flag(v, BMESH_HIDDEN)) mv->flag |= ME_HIDE;
		mv->bweight = (char)(255.0*v1->bweight);
		CustomData_from_bmesh_block(&bm->vdata, data, &v1->data, index);
}

static int bmeshedge_to_medge(BMMesh *bm, BMEdge *e, MEdge *me, int index, CustomData *data)
{
	if(e->head.eflag2){
		if(e->v1->head.eflag1 < e->v2->head.eflag1){
			me->v1 = e->v1->head.eflag1;
			me->v2 = e->v2->head.eflag1;
		}
		else{
			me->v1 = e->v2->head.eflag1;
			me->v2 = e->v1->eflag1;
		}
	
		me->crease = (char)(255.0*e->crease);
		me->bweight = (char)(255.0*e->bweight);
		if(bmesh_test_flag(e, BMESH_SELECT)) me->flag |= 1;
		if(bmesh_test_flag(e, BMESH_HIDDEN)) me->flag |= ME_HIDE;
		CustomData_from_bmesh_block(&bm->edata, data, &e->data, index);
		return 1;
	}
	return 0;
}

static int bmeshface_to_mface(BMMesh *bm, BMFace *f, MFace *mf, int index, CustomData *data)
{
	if(f->len==3 || f->len==4){
		mf->v1 = f->loopbase->v->head.eflag1;
		mf->v2 = f->loopbase->next->v->head.eflag1;
		mf->v3 = f->loopbase->next->next->v->head.eflag1;
		if(len == 4){
			mf->v4 = f->loopbase->prev->v->head.eflag1;
		}
		/* test and rotate indexes if necessary so that verts 3 and 4 aren't index 0 */
		if(mf->v3 == 0 || (f->len == 4 && mf->v4 == 0)){
			test_index_face(mf, NULL, index, f->len);
		}
		mf->mat_nr = (unsigned char)f->mat_nr;
		if(bmesh_test_flag(f, BMESH_SELECT)) mf->flag |= 1;
		if(bmesh_test_flag(f, BMESH_HIDDEN)) mf->flag |= ME_HIDE;
		CustomData_from_bmesh_block(&bm->pdata, data, &f->data, index);
		return TRUE;
	}
	return FALSE;
}

/*
 * BMESH TO DERIVEDMESH
 *
 * Converts a bmesh to a derived mesh.
 *
*/

DerivedMesh *bmesh_to_derivedmesh(BMMesh *bm, DerivedMesh *dm)
{
	MFace *mface = NULL, *mf = NULL;
	MEdge *medge = NULL, *me = NULL;
	MVert *mvert = NULL, *mv = NULL;
	DerivedMesh *result = NULL;

	BMVert *v=NULL;
	BMEdge *e=NULL, *oe=NULL;
	BMFace *f=NULL;

	BMIter verts;
	BMIter edges;
	BMIter faces;

	int totface = 0,totedge = 0,totvert = 0,i = 0, numTex, numCol;

	EdgeHash *edge_hash = BLI_edgehash_new();

	/*get element counts*/
	totvert = bmesh_count_element(bm, BMESH_VERT);

	/*store element indices. Note that the abuse of eflag here should NOT be duplicated!*/
	for(i=0, v = bmeshIterator_init(verts, BM_VERTS, bm, 0); v; v = bmeshIterator_step(verts), i++)
		v->head.eflag1 = i;

	/*we cannot have double edges in a derived mesh!*/
	for(e = bmeshIterator_init(edges, BM_EDGES, bm, 0); e; e = bmeshIterator_step(edges)){
		oe = BLI_edgehash_lookup(edge_hash,e->v1->head.eflag1, e->v2->head.eflag1);
		if(!oe){
			totedge++;
			BLI_edgehash_insert(edge_hash,e->v1->head.eflag1,e->v2->head.eflag1,e);
			e->head.eflag2 = 1;
		}
		else{
			e->head.eflag2 = 0;
		}
	}

	/*count quads and tris*/
	for(f = bmeshIterator_init(faces, BM_FACES, bm, 0); f; f = bmeshIterator_step(faces)){
		if(f->len == 3 || f->len == 4) totface++;
	}
	
	/*Allocate derivedmesh and copy custom data*/
	result = CDDM_from_template(dm,totvert,totedge,totface);
	CustomData_merge(&bm->vdata, &result->vertData, CD_MASK_BMESH, CD_CALLOC, totvert);
	CustomData_merge(&bm->edata, &result->edgeData, CD_MASK_BMESH, CD_CALLOC, totedge);
	CustomData_merge(&bm->pdata, &result->faceData, CD_MASK_BMESH, CD_CALLOC, totface);
	CustomData_from_bmeshpoly(&result->faceData, &bm->pdata, &bm->ldata,totface);
	numTex = CustomData_number_of_layers(&bm->pdata, CD_MTEXPOLY);
	numCol = CustomData_number_of_layers(&bm->ldata, CD_MLOOPCOL);

	/*Make Verts*/
	mvert = CDDM_get_verts(result);
	for(i = 0, v = bmeshIterator_init(verts, BM_VERTS, bm, 0); v; v = bmeshIterator_step(verts), i++, mv++){
		bmeshvert_to_mvert(bm,v,mv,i,&result->vertData);
	}

	/*Make Edges*/
	medge = CDDM_get_edges(result);
	i=0;
	for(e = bmeshIterator_init(edges, BM_EDGES, bm, 0); e; e = bmeshIterator_step(edges)){
		me = &medge[i];
		if(bmeshedge_to_medge(bm, e, me, i, &result->edgeData){
			me++;
			i++;
		}
	}
	/*Make Faces*/
	if(totface){
		mface = CDDM_get_faces(result);
		i=0;
		for(f = bmeshIterator_init(faces, BM_FACES, bm, 0); f; f = bmeshIterator_step(faces)){ 
			mf = &mface[i];
			if(bmeshface_to_mface(bm, f, mf, i, &result->faceData)){
				loops_to_DMcorners(bm, &result->faceData, i, f, numCol, numTex);
				i++;
			}
		}
	}
	BLI_edgehash_free(edge_hash, NULL);
	return result;
}

#endif
