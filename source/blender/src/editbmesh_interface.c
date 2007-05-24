#include "MEM_guardedalloc.h"

#include "BKE_bmesh.h"
#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_customdata.h"
#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_utildefines.h"

#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_object_types.h"
#include "DNA_customdata_types.h"

#include "BLI_PointerArray.h"
#include "BLI_memarena.h"
#include "BLI_blenlib.h"

#include "BIF_space.h"

#include "mydevice.h" //event codes
#include "editbmesh.h"

typedef struct BME_Undo_Mesh {
} BME_Undo_Mesh;

typedef struct BME_Undo_Poly {
} BME_Undo_Poly;

typedef struct BME_Undo_Loop {
} BME_Undo_Loop;

typedef struct BME_Undo_Edge {
} BME_Undo_Edge;

typedef struct BME_Undo_Vert {
} BME_Undo_Vert;

BME_Undo_Mesh *EditBME_makeUndoMesh(BME_Mesh *mesh)
{
	return NULL;
}

typedef struct _edgeref {
	struct _edgeref *next, *prev;
	BME_Edge *edge;
} _edgeref;

//after this, iref should point to the right BME_Edge _edgeref.
#define GET_EDGE_FOR_VERTS(_iref, _v1, _v2, _vlist_edges) \
	for (_iref=_vlist_edges[_v1->tflag1].first; _iref; _iref=_iref->next) {\
		if ((_iref->edge->v1==_v1 && _iref->edge->v2==_v2) ||\
		    (_iref->edge->v1==_v2 && _iref->edge->v2==_v1)) break;}

/*okkkaaay. . .why did I write this function, conversion is better in readfile.c*/
BME_Mesh *BME_fromOldMesh(Mesh *mesh)
{
	BME_Mesh *bmesh = BME_make_mesh();
	MFace *mface;
	MEdge *medge;
	MVert *mvert;
	BME_Vert **vert_table;
	BME_Edge **edge_table;
	BME_Edge *edges[4];
	ListBase *vlist_edges;
	_edgeref *ref;
	MemArena *edgearena = BLI_memarena_new(BLI_MEMARENA_STD_BUFSIZE);
	int i, j;
	
	if (!mesh->totvert) return bmesh;
	vert_table = MEM_callocN(sizeof(void*)*mesh->totvert, "vert table");
	edge_table = MEM_callocN(sizeof(void*)*mesh->totedge, "edge table");
	
	vlist_edges = MEM_callocN(sizeof(ListBase)*mesh->totvert, "vlist_e");
	
	for (i=0, mvert=mesh->mvert; i<mesh->totvert; i++, mvert++) {
		vert_table[i] = BME_MV(bmesh, mvert->co);
		vert_table[i]->flag = mvert->flag;
		vert_table[i]->tflag1 = i;
	}
	
	for (i=0, medge=mesh->medge; i<mesh->totedge; i++, medge++) {
		edge_table[i] = BME_ME(bmesh, vert_table[medge->v1], vert_table[medge->v2]);
		ref = BLI_memarena_alloc(edgearena, sizeof(_edgeref)*2);
		ref->edge = edge_table[i];
		BLI_addtail(&vlist_edges[medge->v1], ref);
		ref++;
		ref->edge = edge_table[i];
		BLI_addtail(&vlist_edges[medge->v2], ref);
	}
	
	for (i=0, mface=mesh->mface; i<mesh->totface; i++, mface++) {
		j = 0;
		GET_EDGE_FOR_VERTS(ref, vert_table[mface->v1], vert_table[mface->v2], vlist_edges);
		edges[0] = ref->edge;
		GET_EDGE_FOR_VERTS(ref, vert_table[mface->v2], vert_table[mface->v3], vlist_edges);
		edges[1] = ref->edge;
		if (mface->v4) {
			GET_EDGE_FOR_VERTS(ref, vert_table[mface->v3], vert_table[mface->v4], vlist_edges);
			edges[2] = ref->edge;
			GET_EDGE_FOR_VERTS(ref, vert_table[mface->v4], vert_table[mface->v1], vlist_edges);
			edges[3] = ref->edge;
		} else {
			GET_EDGE_FOR_VERTS(ref, vert_table[mface->v3], vert_table[mface->v1], vlist_edges);
			edges[3] = ref->edge;
		}
		
		BME_MF(bmesh, edges[0]->v1, edges[0]->v2, edges, mface->v4?4:3);
	}
	
	if (vert_table) MEM_freeN(vert_table);
	if (edge_table) MEM_freeN(edge_table);
	if (vlist_edges) MEM_freeN(vlist_edges);
	BLI_memarena_free(edgearena);
	
	return bmesh;	
}

BME_Mesh *BME_FromMesh(Mesh *me)
{
	BME_Mesh *bmesh = BME_make_mesh();
	MPoly *mpoly;
	MLoop *mloop;
	MEdge *medge;
	MVert *mvert;
	BME_Vert **vert_table = MEM_callocN(sizeof(void*)*me->totvert, "vert table");
	BME_Edge **edge_table = MEM_callocN(sizeof(void*)*me->totedge, "edge table");
	BME_Poly *poly;
	PointerArray edgearr = {0};
	int i, j;
	
	if (me->totface && !me->totpoly) {
		printf("ERROR: paranoia mesh conversion function was called!\n");
		return BME_fromOldMesh(me);
	}

	CustomData_copy(&me->vdata, &bmesh->vdata, CD_MASK_EDITMESH, CD_CALLOC, 0);
	for (i=0, mvert=me->mvert; i<me->totvert; i++, mvert++) {
		vert_table[i] = BME_MV(bmesh, mvert->co);
		vert_table[i]->no[0] = mvert->no[0] / 32767.0f;
		vert_table[i]->no[1] = mvert->no[1] / 32767.0f;
		vert_table[i]->no[2] = mvert->no[2] / 32767.0f;

		vert_table[i]->flag = mvert->flag;
		CustomData_to_em_block(&me->vdata, &bmesh->vdata, i, &vert_table[i]->data);

		printf("vert_table->no: %f %f %f\n", vert_table[i]->no[0], vert_table[i]->no[1], vert_table[i]->no[2]);
		printf("mvert->no: %d %d %d\n", (int)mvert->no[0], (int)mvert->no[1], (int)mvert->no[2]);
	}

	for (i=0, medge=me->medge; i<me->totedge; i++, medge++) {
		edge_table[i] = BME_ME(bmesh, vert_table[medge->v1], vert_table[medge->v2]);
	}
	
	CustomData_copy(&me->pdata, &bmesh->pdata, CD_MASK_EDITMESH, CD_CALLOC, 0);
	for (i=0, mpoly=me->mpoly; i<me->totpoly; i++, mpoly++) {
		mloop = &me->mloop[mpoly->firstloop];
		printf("mpoly->firstloop: %d, me->mloop: %p\n", mpoly->firstloop, me->mloop);
		for (j=0; j<mpoly->totloop; j++) {
			/*hrm. . . a simple scratch pointer array of, oh 6000 length might be better here.*/
			PA_AddToArray(&edgearr, edge_table[mloop->edge], j);
			mloop++;
		}		
		poly = BME_MF(bmesh, ((BME_Edge*)edgearr.array[0])->v1, ((BME_Edge*)edgearr.array[0])->v2, (BME_Edge**)edgearr.array, j);
		poly->flag = mpoly->flag;
		if (!poly) printf("EVIL POLY NOT CREATED!! EVVVIILL!!\n");
		PA_FreeArray(&edgearr); /*basically sets array length to NULL*/
		CustomData_to_em_block(&me->pdata, &bmesh->pdata, i, &poly->data);
	}
	
	/*remember to restore loop data here, including custom data!*/
	
	if (vert_table) MEM_freeN(vert_table);
	if (edge_table) MEM_freeN(edge_table);
	
	return bmesh;
}

/*Remember to use custom data stuff to allocate everything, including mpolys and mloops!*/
void Mesh_FromBMesh(BME_Mesh *bmesh, Mesh *me)
{
	MPoly *mpoly;
	MLoop *mloop;
	MEdge *medge;
	MVert *mvert;
	BME_Vert *bve;
	BME_Edge *bed;
	BME_Loop *blo;
	BME_Poly *bply;
	int i, j, curloop=0;
	short no[3];

	printf("v: %d e: %d l: %d p: %d\n", bmesh->totvert, bmesh->totedge, bmesh->totloop, bmesh->totpoly);
	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);
	CustomData_free(&me->ldata, me->totloop);
	CustomData_free(&me->pdata, me->totpoly);
	
	/*mvert/edge/loop/poly are all used progressively, from this initial assignment.*/
	mvert = me->mvert = MEM_callocN(sizeof(MVert)*bmesh->totvert, "mvert");
	medge = me->medge = MEM_callocN(sizeof(MEdge)*bmesh->totedge, "medge");
	mloop = me->mloop = MEM_callocN(sizeof(MLoop)*bmesh->totloop, "mloop");
	mpoly = me->mpoly = MEM_callocN(sizeof(MPoly)*bmesh->totpoly, "mpoly");
	
	CustomData_copy(&bmesh->vdata, &me->vdata, CD_MASK_MESH, CD_CALLOC, me->totvert);
	CustomData_copy(&bmesh->ldata, &me->ldata, CD_MASK_MESH, CD_CALLOC, me->totloop);
	CustomData_copy(&bmesh->pdata, &me->pdata, CD_MASK_MESH, CD_CALLOC, me->totpoly);

	CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, mvert, me->totvert);
	CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, medge, me->totedge);
	CustomData_add_layer(&me->ldata, CD_MLOOP, CD_ASSIGN, mloop, me->totloop);
	CustomData_add_layer(&me->pdata, CD_MPOLY, CD_ASSIGN, mpoly, me->totpoly);
	
	me->totface = 0; me->mface = NULL; /*set mface to NULL*/

	me->totvert = bmesh->totvert;
	me->totedge = bmesh->totedge;
	me->totloop = bmesh->totloop;
	me->totpoly = bmesh->totpoly;
	
	mesh_update_customdata_pointers(me);

	for (bve=bmesh->verts.first, i=0; bve; i++, bve=bve->next, mvert++) {
		bve->tflag1 = i;
		VECCOPY(mvert->co, bve->co);

		no[0] = (short)(bve->no[0]*32767.0f);
		no[1] = (short)(bve->no[1]*32767.0f);
		no[2] = (short)(bve->no[2]*32767.0f);
		VECCOPY(mvert->no, no);
		CustomData_from_em_block(&bmesh->vdata, &me->vdata, bve->data, i);
		mvert->flag = bve->flag;

		printf("bve->no: %f %f %f\n", bve->no[0], bve->no[1], bve->no[2]);
		printf("bve->no: %d %d %d\n", (int)mvert->no[0], (int)mvert->no[1], (int)mvert->no[2]);

		//printf("mesh->co: %f %f %f\n", mvert->co[0], mvert->co[1], mvert->co[2]);
	}
	
	/* the edges */
	for (bed=bmesh->edges.first, i=0; bed; i++, bed=bed->next, medge++) {
		bed->tflag1 = i;
		medge->v1= (unsigned int) bed->v1->tflag1;
		medge->v2= (unsigned int) bed->v2->tflag1;
		
		medge->flag= (bed->flag & SELECT) | ME_EDGERENDER;
		//if(eed->f2<2) medge->flag |= ME_EDGEDRAW;
		//f(eed->f2==0) medge->flag |= ME_LOOSEEDGE;
		//if (eed->sharp) medge->flag |= ME_SHARP;
		//if (eed->seam) medge->flag |= ME_SEAM;
		//if (eed->h & EM_FGON) medge->flag |= ME_FGON;	// different defines yes
		//if (eed->h & 1) medge->flag |= ME_HIDE;
		
		medge->crease= (char)(255.0*bed->crease);
	}
	
	for (bply=bmesh->polys.first, i=0; bply; i++, mpoly++, bply=bply->next) {
		CustomData_from_em_block(&bmesh->pdata, &me->pdata, bply->data, i);
		mpoly->firstloop = curloop;
		mpoly->flag = bply->flag;
		mpoly->mat_nr = bply->mat_nr;
		blo=bply->loopbase;
		j = 0;
		do {
			mloop->v = blo->v->tflag1;
			mloop->poly = i;
			mloop->edge = blo->e->tflag1;
			mloop++;
			curloop++;
			j++;
			blo=blo->next;
		} while (blo != bply->loopbase);
		mpoly->totloop = j;
	}
}


void hide_mesh(int i)
{
}

void reveal_mesh(void)
{
}

void deselectall_mesh(void)
{
}

/*eventual replacement for EM_check_backbuf, if I decide to do it that way.*/
void BME_check_backbuf(int offset)
{
}

void add_primitiveMesh(int type)
{
}

void EditBME_remakeEditMesh(void)
{
	EditBME_makeEditMesh();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	BIF_undo_push("Undo all changes");
}

void EditBME_makeEditMesh(void)
{
	G.editMesh = BME_FromMesh(G.obedit->data);
}

void EditBME_loadEditMesh(Mesh *mesh)
{
	Mesh_FromBMesh(G.editMesh, mesh);
}

void mouse_bmesh()
{
}

BME_Vert *EditBME_FindNearestVert(int *dis)
{
	return NULL;
}

BME_Edge *EditBME_FindNearestEdge(int *dis)
{
	return NULL;
}

BME_Poly *EditBME_FindNearestPoly(int *dis)
{
	return NULL;
}

void undo_push_mesh(char *str)
{
}

void BME_data_interp_from_verts(BME_Vert *v1, BME_Vert *v2, BME_Vert *eve, float fac)
{
	BME_Mesh *em= G.editMesh;
	void *src[2];
	float w[2];

	if (v1->data && v2->data) {
		src[0]= v1->data;
		src[1]= v2->data;
		w[0] = 1.0f-fac;
		w[1] = fac;

		CustomData_em_interp(&em->vdata, src, w, NULL, 2, eve->data);
	}
}

static void update_data_blocks(CustomData *olddata, CustomData *data)
{
	BME_Mesh *em= G.editMesh;
	BME_Poly *efa;
	BME_Vert *eve;
	void *block;

	if (data == &G.editMesh->vdata) {
		for(eve= em->verts.first; eve; eve= eve->next) {
			block = NULL;
			CustomData_em_set_default(data, &block);
			CustomData_em_copy_data(olddata, data, eve->data, &block);
			CustomData_em_free_block(olddata, &eve->data);
			eve->data= block;
		}
	}
	else if (data == &G.editMesh->pdata) {
		for(efa= em->polys.first; efa; efa= efa->next) {
			block = NULL;
			CustomData_em_set_default(data, &block);
			CustomData_em_copy_data(olddata, data, efa->data, &block);
			CustomData_em_free_block(olddata, &efa->data);
			efa->data= block;
		}
	}
	else if (data == &G.editMesh->edata) {
	}
	else if (data == &G.editMesh->ldata) {
	}
}

void BME_add_data_layer(CustomData *data, int type)
{
	CustomData olddata;

	olddata= *data;
	olddata.layers= (olddata.layers)? MEM_dupallocN(olddata.layers): NULL;
	CustomData_add_layer(data, type, CD_CALLOC, NULL, 0);

	update_data_blocks(&olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

void BME_free_data_layer(CustomData *data, int type)
{
	CustomData olddata;

	olddata= *data;
	olddata.layers= (olddata.layers)? MEM_dupallocN(olddata.layers): NULL;
	CustomData_free_layer_active(data, type, 0);

	update_data_blocks(&olddata, data);
	if (olddata.layers) MEM_freeN(olddata.layers);
}

/*Various customdata stuff still in need of conversion :/*/
#if 0
/* paranoia check, actually only for entering editmode. rule:
- vertex hidden, always means edge is hidden too
- edge hidden, always means face is hidden too
- face hidden, dont change anything
*/
void EM_hide_reset(void)
{
	EditMesh *em = G.editMesh;
	EditEdge *eed;
	EditFace *efa;
	
	for(eed= em->edges.first; eed; eed= eed->next) 
		if(eed->v1->h || eed->v2->h) eed->h |= 1;
		
	for(efa= em->faces.first; efa; efa= efa->next) 
		if((efa->e1->h & 1) || (efa->e2->h & 1) || (efa->e3->h & 1) || (efa->e4 && (efa->e4->h & 1)))
			efa->h= 1;
		
}

void EM_data_interp_from_faces(EditFace *efa1, EditFace *efa2, EditFace *efan, int i1, int i2, int i3, int i4)
{
	EditMesh *em= G.editMesh;
	float w[2][4][4];
	void *src[2];
	int count = (efa2)? 2: 1;

	if (efa1->data) {
		/* set weights for copying from corners directly to other corners */
		memset(w, 0, sizeof(w));

		w[i1/4][0][i1%4]= 1.0f;
		w[i2/4][1][i2%4]= 1.0f;
		w[i3/4][2][i3%4]= 1.0f;
		if (i4 != -1)
			w[i4/4][3][i4%4]= 1.0f;

		src[0]= efa1->data;
		src[1]= (efa2)? efa2->data: NULL;

		CustomData_em_interp(&em->fdata, src, NULL, (float*)w, count, efan->data);
	}
}

EditFace *EM_face_from_faces(EditFace *efa1, EditFace *efa2, int i1, int i2, int i3, int i4)
{
	EditFace *efan;
	EditVert **v[2];
	
	v[0]= &efa1->v1;
	v[1]= (efa2)? &efa2->v1: NULL;

	efan= addfacelist(v[i1/4][i1%4], v[i2/4][i2%4], v[i3/4][i3%4],
		(i4 == -1)? 0: v[i4/4][i4%4], efa1, NULL);

	EM_data_interp_from_faces(efa1, efa2, efan, i1, i2, i3, i4);
	
	return efan;
}
#endif
