#include "MEM_guardedalloc.h"

#include "BSE_edit.h"

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
#include "DNA_view3d_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"

#include "BLI_PointerArray.h"
#include "BLI_memarena.h"
#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_resources.h"
#include "BIF_language.h"
#include "BIF_interface.h"

#include "BDR_editobject.h"
#include "BDR_drawobject.h"

#include "BSE_drawview.h"

#include "mydevice.h" //event codes
#include "editbmesh.h"
/*
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
*/
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
	
	
	BME_model_begin(bmesh);
	bmesh->selectmode = G.scene->selectmode;
	
	if (me->totface && !me->totpoly) {
		printf("ERROR: paranoia mesh conversion function was called!\n");
		return BME_fromOldMesh(me);
	}

	CustomData_copy(&me->vdata, &bmesh->vdata, CD_MASK_EDITMESH, CD_CALLOC, 0);
	for (i=0, mvert=me->mvert; i<me->totvert; i++, mvert++) {
		vert_table[i] = BME_MV(bmesh, mvert->co);
		vert_table[i]->no[0] = (float)mvert->no[0] / 32767.0f;
		vert_table[i]->no[1] = (float)mvert->no[1] / 32767.0f;
		vert_table[i]->no[2] = (float)mvert->no[2] / 32767.0f;

		vert_table[i]->flag = mvert->flag;
		CustomData_to_em_block(&me->vdata, &bmesh->vdata, i, &vert_table[i]->data);
	}

	for (i=0, medge=me->medge; i<me->totedge; i++, medge++) {
		edge_table[i] = BME_ME(bmesh, vert_table[medge->v1], vert_table[medge->v2]);
		edge_table[i]->flag = medge->flag;
	}
	
	CustomData_copy(&me->pdata, &bmesh->pdata, CD_MASK_EDITMESH, CD_CALLOC, 0);
	for (i=0, mpoly=me->mpoly; i<me->totpoly; i++, mpoly++) {
		mloop = &me->mloop[mpoly->firstloop];
		for (j=0; j<mpoly->totloop; j++) {
			/*hrm. . . a simple scratch pointer array of, oh 6000 length might be better here.*/
			PA_AddToArray(&edgearr, edge_table[mloop->edge], j);
			mloop++;
		}		
		mloop = &me->mloop[mpoly->firstloop];
		poly = BME_MF(bmesh, vert_table[mloop->v],vert_table[(mloop+1)->v],(BME_Edge**)edgearr.array, j);
		poly->flag = mpoly->flag;
		if (!poly) {
			printf("EVIL POLY NOT CREATED!! EVVVIILL!!\n");
			PA_FreeArray(&edgearr); /*basically sets array length to NULL*/
			return bmesh;
		}
		PA_FreeArray(&edgearr); /*basically sets array length to NULL*/
		CustomData_to_em_block(&me->pdata, &bmesh->pdata, i, &poly->data);
	}
	
	/*remember to restore loop data here, including custom data!*/
	
	if (vert_table) MEM_freeN(vert_table);
	if (edge_table) MEM_freeN(edge_table);
	
	BME_model_end(bmesh);
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
	if (G.editMesh) BME_free_mesh(G.editMesh);
	G.editMesh = BME_FromMesh(G.obedit->data);
}

void EditBME_loadEditMesh(Mesh *mesh)
{
	Mesh_FromBMesh(G.editMesh, mesh);
}

void EditBME_FlushSelUpward(BME_Mesh *mesh) /*remove this, its not used...*/
{
	BME_Edge *eed;
	BME_Loop *loop;
	BME_Poly *efa;

	for (eed=mesh->edges.first; eed; eed=eed->next) {
		if ((eed->v1->flag & SELECT) && (eed->v2->flag & SELECT)) eed->flag |= SELECT;
		else eed->flag &= ~SELECT;
	}

	for (efa=mesh->polys.first; efa; efa=efa->next) {
		loop = efa->loopbase;
		efa->flag |= SELECT;
		do {
			if ((loop->v->flag & SELECT)==0) efa->flag &= ~SELECT;
			loop=loop->next;
		} while (loop != efa->loopbase);
	}
}


/* best distance based on screen coords. 
   use g.scene->selectmode to define how to use 
   selected vertices and edges get disadvantage
   return 1 if found one
*/
int unified_findnearest(BME_Vert **eve, BME_Edge **eed, BME_Poly **efa) 
{
	int dist= 75;
	
	*eve= NULL;
	*eed= NULL;
	*efa= NULL;
	
	if(G.scene->selectmode & SCE_SELECT_VERTEX)
		*eve= EditBME_FindNearestVert(&dist, 1, 0);
	if(G.scene->selectmode & SCE_SELECT_FACE)
		*efa= EditBME_FindNearestPoly(&dist);

	dist-= 20;	/* since edges select lines, we give dots advantage of 20 pix */
	if(G.scene->selectmode & SCE_SELECT_EDGE)
		*eed=EditBME_FindNearestEdge(&dist);

	/* return only one of 3 pointers, for frontbuffer redraws */
	if(*eed) {
		*efa= NULL; *eve= NULL;
	}
	else if(*efa) {
		*eve= NULL;
	}
	
	return (*eve || *eed || *efa);
}

/* ***************** MAIN MOUSE SELECTION ************** */

/* just to have the functions nice together */
static void mouse_mesh_loop(void)
{
	BME_Edge *eed;
	int select= 1;
	int dist= 50;
	
	eed= EditBME_FindNearestEdge(&dist);
	if(eed) {
		if((G.qual & LR_SHIFTKEY)==0) BME_clear_flag_all(G.editMesh,SELECT);
		
		if((BME_SELECTED(eed))==0) select=1;
		else if(G.qual & LR_SHIFTKEY) select=0;

		if(G.qual == (LR_CTRLKEY | LR_ALTKEY) || G.qual == (LR_CTRLKEY | LR_ALTKEY |LR_SHIFTKEY)){
		//if(G.qual & LR_ALTKEY){
			BME_Edge *e;
			BME_clear_flag_all(G.editMesh,BME_VISITED);
			BME_MeshRing_walk(G.editMesh, eed, BME_edgering_nextedge, NULL, 0);
			for(e=BME_first(G.editMesh,BME_EDGE);e;e=BME_next(G.editMesh,BME_EDGE,e)){
				if(BME_ISVISITED(e)) BME_select_edge(G.editMesh,e,select);
			}
			BME_selectmode_flush(G.editMesh); 
		}
		else if(G.qual & LR_ALTKEY){
			BME_Vert *v;
			BME_Edge *e;
			
			BME_clear_flag_all(G.editMesh,BME_VISITED);
			
			if(!eed->loop){
				
				BME_MeshWalk(G.editMesh, eed->v1, NULL, NULL, BME_RESTRICTWIRE); 
				for(v=BME_first(G.editMesh,BME_VERT);v;v=BME_next(G.editMesh,BME_VERT,v)){
					if(BME_ISVISITED(v)) BME_select_vert(G.editMesh,v,select);
				}
			}
			else{
				int radlen = BME_cycle_length(&(eed->loop->radial));
				if(radlen == 1) BME_MeshLoop_walk(G.editMesh, eed, BME_edgeshell_nextedge,NULL,NULL);
				else BME_MeshLoop_walk(G.editMesh, eed, BME_edgeloop_nextedge, NULL, NULL);
				for(e=BME_first(G.editMesh,BME_EDGE);e;e=BME_next(G.editMesh,BME_EDGE,e)){
					if(BME_ISVISITED(e)) BME_select_edge(G.editMesh,e,select);
				}
			}
			BME_selectmode_flush(G.editMesh); //nasty
		}
		/* frontbuffer draw of last selected only */
		/*unified_select_draw(NULL, eed, NULL);*/
		BME_selectmode_flush(G.editMesh);
		makeDerivedMesh(G.obedit,CD_MASK_EDITMESH);
		countall();
		allqueue(REDRAWVIEW3D, 0);
	}
}



void mouse_bmesh(void) /*rewrite me like the old mouse_mesh from editmesh....*/
{
	BME_Mesh *bm = G.editMesh;
	BME_Vert *v,*vsel=NULL;
	BME_Edge *e,*esel=NULL;
	BME_Poly *f,*fsel=NULL;

	if(G.qual & LR_ALTKEY) mouse_mesh_loop();
	else if(unified_findnearest(&vsel, &esel, &fsel)) {
		if((G.qual & LR_SHIFTKEY)==0){
			//clear selection flags.
			for(v=BME_first(bm,BME_VERT);v;v=BME_next(bm,BME_VERT,v)) BME_select_vert(bm,v,0);
			for(e=BME_first(bm,BME_EDGE);e;e=BME_next(bm,BME_VERT,e)) BME_select_edge(bm,e,0);
			for(f=BME_first(bm,BME_POLY);f;f=BME_next(bm,BME_POLY,f)) BME_select_poly(bm,f,0);
		}
		if(fsel){
			if(BME_SELECTED(fsel)) BME_select_poly(bm,fsel,0);
			else BME_select_poly(bm,fsel,1);
		}
		else if(esel){
			if(BME_SELECTED(esel)) BME_select_edge(bm,esel,0);
			else BME_select_edge(bm,esel,1);
		}
		else if(vsel){
			if(BME_SELECTED(vsel)) BME_select_vert(bm,vsel,0);
			else BME_select_vert(bm,vsel,1);
		}
	}
	
	BME_selectmode_flush(bm);
	countall();
	makeDerivedMesh(G.obedit,CD_MASK_EDITMESH);
	allqueue(REDRAWVIEW3D,0);
	rightmouse_transform();
}

static unsigned int findnearestvert__backbufIndextest(unsigned int index){
	BME_Vert *eve = BLI_findlink(&G.editMesh->verts, index-1);
	if(eve && (eve->flag & SELECT)) return 0;
	return 1; 
}

static void findnearestvert__doClosest(void *userData, BME_Vert *eve, int x, int y, int index)
{
	struct { short mval[2], pass, select, strict; int dist, lastIndex, closestIndex; BME_Vert *closest; } *data = userData;

	if (data->pass==0) {
		if (index<=data->lastIndex)
			return;
	} else {
		if (index>data->lastIndex)
			return;
	}

	if (data->dist>3) {
		int temp = abs(data->mval[0] - x) + abs(data->mval[1]- y);
		if ((eve->flag&SELECT) == data->select) {
			if (data->strict == 1)
				return;
			else
				temp += 5;
		}

		if (temp<data->dist) {
			data->dist = temp;
			data->closest = eve;
			data->closestIndex = index;
		}
	}
}

BME_Vert *EditBME_FindNearestVert(int *dist, short sel, short strict)
{
	short mval[2];
	getmouseco_areawin(mval);

	if(G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT)) {
	} else {
		struct { short mval[2], pass, select, strict; int dist, lastIndex, closestIndex; BME_Vert *closest; } data;
		static int lastSelectedIndex=0;
		static BME_Vert *lastSelected=NULL;
		if (lastSelected && BLI_findlink(&G.editMesh->verts, lastSelectedIndex)!=lastSelected) {
			lastSelectedIndex = 0;
			lastSelected = NULL;
		}

		data.lastIndex = lastSelectedIndex;
		data.mval[0] = mval[0];
		data.mval[1] = mval[1];
		data.select = sel;
		data.dist = *dist;
		data.strict = strict;
		data.closest = NULL;
		data.closestIndex = 0;

		data.pass = 0;
		mesh_foreachScreenVert(findnearestvert__doClosest, &data, 1);

		if (data.dist>3) {
			data.pass = 1;
			mesh_foreachScreenVert(findnearestvert__doClosest, &data, 1);
		}

		*dist = data.dist;
		lastSelected = data.closest;
		lastSelectedIndex = data.closestIndex;

		return data.closest;
	}
	return NULL;
}

/* taken from editmesh_mods.c, and why isn't the arithb function used anymore?
  returns labda for closest distance v1 to line-piece v2-v3 */
static float labda_PdistVL2Dfl( float *v1, float *v2, float *v3) 
{
	float rc[2], len;
	
	rc[0]= v3[0]-v2[0];
	rc[1]= v3[1]-v2[1];
	len= rc[0]*rc[0]+ rc[1]*rc[1];
	if(len==0.0f)
		return 0.0f;
	
	return ( rc[0]*(v1[0]-v2[0]) + rc[1]*(v1[1]-v2[1]) )/len;
}

/* note; uses G.vd, so needs active 3d window */
static void BME_findnearestedge__doClosest(void *userData, BME_Edge *eed, int x0, int y0, int x1, int y1, int index)
{
	struct { float mval[2]; int dist; BME_Edge *closest; } *data = userData;
	float v1[2], v2[2];
	int distance;
		
	v1[0] = x0;
	v1[1] = y0;
	v2[0] = x1;
	v2[1] = y1;
		
	distance= PdistVL2Dfl(data->mval, v1, v2);
		
	if(eed->flag & SELECT) distance+=5;
	if(distance < data->dist) {
		if(G.vd->flag & V3D_CLIPPING) {
			float labda= labda_PdistVL2Dfl(data->mval, v1, v2);
			float vec[3];

			vec[0]= eed->v1->co[0] + labda*(eed->v2->co[0] - eed->v1->co[0]);
			vec[1]= eed->v1->co[1] + labda*(eed->v2->co[1] - eed->v1->co[1]);
			vec[2]= eed->v1->co[2] + labda*(eed->v2->co[2] - eed->v1->co[2]);
			Mat4MulVecfl(G.obedit->obmat, vec);

			if(view3d_test_clipping(G.vd, vec)==0) {
				data->dist = distance;
				data->closest = eed;
			}
		}
		else {
			data->dist = distance;
			data->closest = eed;
		}
	}
}
BME_Edge *EditBME_FindNearestEdge(int *dist)
{
	short mval[2];
		
	getmouseco_areawin(mval);

	if(G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT)) {
		/*int distance;
		unsigned int index = sample_backbuf_rect(mval, 50, em_solidoffs, em_wireoffs, &distance,0, NULL);
		BME_Edge *eed = BLI_findlink(&G.editMesh->edges, index-1);

		if (eed && distance<*dist) {
			*dist = distance;
			return eed;
		} else {
			return NULL;
		}*/
	}
	else {
		struct { float mval[2]; int dist; BME_Edge *closest; } data;

		data.mval[0] = mval[0];
		data.mval[1] = mval[1];
		data.dist = *dist;
		data.closest = NULL;

		mesh_foreachScreenEdge(BME_findnearestedge__doClosest, &data, 2);

		*dist = data.dist;
		return data.closest;
	}
	return NULL;
}



static void findnearestface__getDistance(void *userData, BME_Poly *f, int x, int y, int index)
{
	struct { short mval[2]; int dist; BME_Poly *toFace; } *data = userData;

	if (f==data->toFace) {
		int temp = abs(data->mval[0]-x) + abs(data->mval[1]-y);

		if (temp<data->dist)
			data->dist = temp;
	}
}
static void findnearestface__doClosest(void *userData, BME_Poly *f, int x, int y, int index)
{
	struct { short mval[2], pass; int dist, lastIndex, closestIndex; BME_Poly *closest; } *data = userData;

	if (data->pass==0) {
		if (index<=data->lastIndex)
			return;
	} else {
		if (index>data->lastIndex)
			return;
	}

	if (data->dist>3) {
		int temp = abs(data->mval[0]-x) + abs(data->mval[1]-y);

		if (temp<data->dist) {
			data->dist = temp;
			data->closest = f;
			data->closestIndex = index;
		//temp
		}
	}
}
BME_Poly *EditBME_FindNearestPoly(int *dist)
{
	short mval[2];

	getmouseco_areawin(mval);

	if(G.vd->drawtype>OB_WIRE && (G.vd->flag & V3D_ZBUF_SELECT)); //{
		//unsigned int index = sample_backbuf(mval[0], mval[1]);
		//EditFace *efa = BLI_findlink(&G.editMesh->faces, index-1);

		//if (efa) {
		//	struct { short mval[2]; int dist; EditFace *toFace; } data;

		//	data.mval[0] = mval[0];
		//	data.mval[1] = mval[1];
		//	data.dist = 0x7FFF;		/* largest short */
		//	data.toFace = efa;

		//	mesh_foreachScreenFace(findnearestface__getDistance, &data);

		//	if(G.scene->selectmode == SCE_SELECT_FACE || data.dist<*dist) {	/* only faces, no dist check */
		//		*dist= data.dist;
		//		return efa;
		//	}
		//}
		
		//return NULL;
	//}
	else {
		struct { short mval[2], pass; int dist, lastIndex, closestIndex; BME_Poly *closest; } data;
		static int lastSelectedIndex=0;
		static BME_Poly *lastSelected=NULL;

		if (lastSelected && BLI_findlink(&G.editMesh->polys, lastSelectedIndex)!=lastSelected) {
			lastSelectedIndex = 0;
			lastSelected = NULL;
		}

		data.lastIndex = lastSelectedIndex;
		data.mval[0] = mval[0];
		data.mval[1] = mval[1];
		data.dist = *dist;
		data.closest = NULL;
		data.closestIndex = 0;

		data.pass = 0;
		mesh_foreachScreenFace(findnearestface__doClosest, &data);

		if (data.dist>3) {
			data.pass = 1;
			mesh_foreachScreenFace(findnearestface__doClosest, &data);
		}

		*dist = data.dist;
		lastSelected = data.closest;
		lastSelectedIndex = data.closestIndex;

		return data.closest;
	}
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

void EM_selectmode_menu(void){
	int val;
	
	if(G.scene->selectmode & SCE_SELECT_VERTEX) pupmenu_set_active(1);
	else if(G.scene->selectmode & SCE_SELECT_EDGE) pupmenu_set_active(2);
	else if(G.scene->selectmode & SCE_SELECT_FACE) pupmenu_set_active(3);
		
	val= pupmenu("Select Mode%t|Vertices|Edges|Faces");
	
	if(val>0){
		if(val==1){
			G.scene->selectmode = G.editMesh->selectmode = SCE_SELECT_VERTEX;
			BME_selectmode_set(G.editMesh);
			countall();
			BIF_undo_push("Selectmode Set: Vertex");
		}
		else if(val==2){
			G.scene->selectmode = G.editMesh->selectmode = SCE_SELECT_EDGE;
			BME_selectmode_set(G.editMesh);
			countall();
			BIF_undo_push("Selectmode Set: Edge");
		}
		else if(val==3){
			G.scene->selectmode = G.editMesh->selectmode = SCE_SELECT_FACE;
			BME_selectmode_set(G.editMesh);
			countall();
			BIF_undo_push("Selectmode Set: Face");
		}
	}
	allqueue(REDRAWVIEW3D,1);
}
