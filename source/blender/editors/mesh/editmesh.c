/**
 * $Id$
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, full recode 2002-2008
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "DNA_customdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_object_force.h"
#include "DNA_screen_types.h"
#include "DNA_key_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_material_types.h"
#include "DNA_modifier_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_editVert.h"
#include "BLI_dynstr.h"
#include "BLI_rand.h"

#include "BKE_cloth.h"
#include "BKE_context.h"
#include "BKE_customdata.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_object.h"
#include "BKE_paint.h"
#include "BKE_pointcache.h"
#include "BKE_softbody.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#include "LBM_fluidsim.h"


#include "ED_mesh.h"
#include "ED_object.h"
#include "ED_retopo.h"
#include "ED_screen.h"
#include "ED_util.h"
#include "ED_view3d.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

/* own include */
#include "mesh_intern.h"

/* 
editmesh.c:
	- add/alloc/free data
	- hashtables
	- enter/exit editmode
*/

/* XXX */
static void BIF_undo_push() {}
static void error() {}


/* ***************** HASH ********************* */


#define EDHASHSIZE		(512*512)
#define EDHASH(a, b)	(a % EDHASHSIZE)


/* ************ ADD / REMOVE / FIND ****************** */

static void *calloc_em(EditMesh *em, size_t size, size_t nr)
{
	return calloc(size, nr);
}

/* used to bypass normal calloc with fast one */
static void *(*callocvert)(EditMesh *, size_t, size_t) = calloc_em;
static void *(*callocedge)(EditMesh *, size_t, size_t) = calloc_em;
static void *(*callocface)(EditMesh *, size_t, size_t) = calloc_em;

EditVert *addvertlist(EditMesh *em, float *vec, EditVert *example)
{
	EditVert *eve;
	static int hashnr= 0;

	eve= callocvert(em, sizeof(EditVert), 1);
	BLI_addtail(&em->verts, eve);
	em->totvert++;
	
	if(vec) VECCOPY(eve->co, vec);

	eve->hash= hashnr++;
	if( hashnr>=EDHASHSIZE) hashnr= 0;

	/* new verts get keyindex of -1 since they did not
	 * have a pre-editmode vertex order
	 */
	eve->keyindex = -1;

	if(example) {
		CustomData_em_copy_data(&em->vdata, &em->vdata, example->data, &eve->data);
		eve->bweight = example->bweight;
	}
	else {
		CustomData_em_set_default(&em->vdata, &eve->data);
	}

	return eve;
}

void free_editvert (EditMesh *em, EditVert *eve)
{

	EM_remove_selection(em, eve, EDITVERT);
	CustomData_em_free_block(&em->vdata, &eve->data);
	if(eve->fast==0)
		free(eve);
	
	em->totvert--;
}


EditEdge *findedgelist(EditMesh *em, EditVert *v1, EditVert *v2)
{
	EditVert *v3;
	struct HashEdge *he;

	/* swap ? */
	if( v1 > v2) {
		v3= v2; 
		v2= v1; 
		v1= v3;
	}
	
	if(em->hashedgetab==NULL)
		em->hashedgetab= MEM_callocN(EDHASHSIZE*sizeof(struct HashEdge), "hashedgetab");

	he= em->hashedgetab + EDHASH(v1->hash, v2->hash);
	
	while(he) {
		
		if(he->eed && he->eed->v1==v1 && he->eed->v2==v2) return he->eed;
		
		he= he->next;
	}
	return 0;
}

static void insert_hashedge(EditMesh *em, EditEdge *eed)
{
	/* assuming that eed is not in the list yet, and that a find has been done before */
	
	struct HashEdge *first, *he;

	first= em->hashedgetab + EDHASH(eed->v1->hash, eed->v2->hash);

	if( first->eed==0 ) {
		first->eed= eed;
	}
	else {
		he= &eed->hash; 
		he->eed= eed;
		he->next= first->next;
		first->next= he;
	}
}

static void remove_hashedge(EditMesh *em, EditEdge *eed)
{
	/* assuming eed is in the list */
	
	struct HashEdge *first, *he, *prev=NULL;

	he=first= em->hashedgetab + EDHASH(eed->v1->hash, eed->v2->hash);

	while(he) {
		if(he->eed == eed) {
			/* remove from list */
			if(he==first) {
				if(first->next) {
					he= first->next;
					first->eed= he->eed;
					first->next= he->next;
				}
				else he->eed= 0;
			}
			else {
				prev->next= he->next;
			}
			return;
		}
		prev= he;
		he= he->next;
	}
}

EditEdge *addedgelist(EditMesh *em, EditVert *v1, EditVert *v2, EditEdge *example)
{
	EditVert *v3;
	EditEdge *eed;
	int swap= 0;
	
	if(v1==v2) return NULL;
	if(v1==NULL || v2==NULL) return NULL;

	/* swap ? */
	if(v1>v2) {
		v3= v2; 
		v2= v1; 
		v1= v3;
		swap= 1;
	}
	
	/* find in hashlist */
	eed= findedgelist(em, v1, v2);

	if(eed==NULL) {
	
		eed= (EditEdge *)callocedge(em, sizeof(EditEdge), 1);
		eed->v1= v1;
		eed->v2= v2;
		BLI_addtail(&em->edges, eed);
		eed->dir= swap;
		insert_hashedge(em, eed);
		em->totedge++;
		
		/* copy edge data:
		   rule is to do this with addedgelist call, before addfacelist */
		if(example) {
			eed->crease= example->crease;
			eed->bweight= example->bweight;
			eed->sharp = example->sharp;
			eed->seam = example->seam;
			eed->h |= (example->h & EM_FGON);
		}
	}

	return eed;
}

void remedge(EditMesh *em, EditEdge *eed)
{
	BLI_remlink(&em->edges, eed);
	remove_hashedge(em, eed);
	
	em->totedge--;
}

void free_editedge(EditMesh *em, EditEdge *eed)
{
	EM_remove_selection(em, eed, EDITEDGE);
	if(eed->fast==0){ 
		free(eed);
	}
}

void free_editface(EditMesh *em, EditFace *efa)
{
	EM_remove_selection(em, efa, EDITFACE);
	
	if (em->act_face==efa) {
		EM_set_actFace(em, em->faces.first == efa ? NULL : em->faces.first);
	}
		
	CustomData_em_free_block(&em->fdata, &efa->data);
	if(efa->fast==0)
		free(efa);
	
	em->totface--;
}

void free_vertlist(EditMesh *em, ListBase *edve) 
{
	EditVert *eve, *next;

	if (!edve) return;

	eve= edve->first;
	while(eve) {
		next= eve->next;
		free_editvert(em, eve);
		eve= next;
	}
	edve->first= edve->last= NULL;
	em->totvert= em->totvertsel= 0;
}

void free_edgelist(EditMesh *em, ListBase *lb)
{
	EditEdge *eed, *next;
	
	eed= lb->first;
	while(eed) {
		next= eed->next;
		free_editedge(em, eed);
		eed= next;
	}
	lb->first= lb->last= NULL;
	em->totedge= em->totedgesel= 0;
}

void free_facelist(EditMesh *em, ListBase *lb)
{
	EditFace *efa, *next;
	
	efa= lb->first;
	while(efa) {
		next= efa->next;
		free_editface(em, efa);
		efa= next;
	}
	lb->first= lb->last= NULL;
	em->totface= em->totfacesel= 0;
}

EditFace *addfacelist(EditMesh *em, EditVert *v1, EditVert *v2, EditVert *v3, EditVert *v4, EditFace *example, EditFace *exampleEdges)
{
	EditFace *efa;
	EditEdge *e1, *e2=0, *e3=0, *e4=0;

	/* added sanity check... seems to happen for some tools, or for enter editmode for corrupted meshes */
	if(v1==v4 || v2==v4 || v3==v4) v4= NULL;
	
	/* add face to list and do the edges */
	if(exampleEdges) {
		e1= addedgelist(em, v1, v2, exampleEdges->e1);
		e2= addedgelist(em, v2, v3, exampleEdges->e2);
		if(v4) e3= addedgelist(em, v3, v4, exampleEdges->e3); 
		else e3= addedgelist(em, v3, v1, exampleEdges->e3);
		if(v4) e4= addedgelist(em, v4, v1, exampleEdges->e4);
	}
	else {
		e1= addedgelist(em, v1, v2, NULL);
		e2= addedgelist(em, v2, v3, NULL);
		if(v4) e3= addedgelist(em, v3, v4, NULL); 
		else e3= addedgelist(em, v3, v1, NULL);
		if(v4) e4= addedgelist(em, v4, v1, NULL);
	}
	
	if(v1==v2 || v2==v3 || v1==v3) return NULL;
	if(e2==0) return NULL;

	efa= (EditFace *)callocface(em, sizeof(EditFace), 1);
	efa->v1= v1;
	efa->v2= v2;
	efa->v3= v3;
	efa->v4= v4;

	efa->e1= e1;
	efa->e2= e2;
	efa->e3= e3;
	efa->e4= e4;

	if(example) {
		efa->mat_nr= example->mat_nr;
		efa->flag= example->flag;
		CustomData_em_copy_data(&em->fdata, &em->fdata, example->data, &efa->data);
	}
	else {
		efa->mat_nr= em->mat_nr;

		CustomData_em_set_default(&em->fdata, &efa->data);
	}

	BLI_addtail(&em->faces, efa);
	em->totface++;
	
	if(efa->v4) {
		normal_quad_v3( efa->n,efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co);
		cent_quad_v3(efa->cent, efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co);
	}
	else {
		normal_tri_v3( efa->n,efa->v1->co, efa->v2->co, efa->v3->co);
		cent_tri_v3(efa->cent, efa->v1->co, efa->v2->co, efa->v3->co);
	}

	return efa;
}

/* ************************ end add/new/find ************  */

/* ************************ Edit{Vert,Edge,Face} utilss ***************************** */

/* some nice utility functions */

EditVert *editedge_getOtherVert(EditEdge *eed, EditVert *eve)
{
	if (eve==eed->v1) {
		return eed->v2;
	} else if (eve==eed->v2) {
		return eed->v1;
	} else {
		return NULL;
	}
}

EditVert *editedge_getSharedVert(EditEdge *eed, EditEdge *eed2) 
{
	if (eed->v1==eed2->v1 || eed->v1==eed2->v2) {
		return eed->v1;
	} else if (eed->v2==eed2->v1 || eed->v2==eed2->v2) {
		return eed->v2;
	} else {
		return NULL;
	}
}

int editedge_containsVert(EditEdge *eed, EditVert *eve) 
{
	return (eed->v1==eve || eed->v2==eve);
}

int editface_containsVert(EditFace *efa, EditVert *eve) 
{
	return (efa->v1==eve || efa->v2==eve || efa->v3==eve || (efa->v4 && efa->v4==eve));
}

int editface_containsEdge(EditFace *efa, EditEdge *eed) 
{
	return (efa->e1==eed || efa->e2==eed || efa->e3==eed || (efa->e4 && efa->e4==eed));
}


/* ************************ stuct EditMesh manipulation ***************************** */

/* fake callocs for fastmalloc below */
static void *calloc_fastvert(EditMesh *em, size_t size, size_t nr)
{
	EditVert *eve= em->curvert++;
	eve->fast= 1;
	return eve;
}
static void *calloc_fastedge(EditMesh *em, size_t size, size_t nr)
{
	EditEdge *eed= em->curedge++;
	eed->fast= 1;
	return eed;
}
static void *calloc_fastface(EditMesh *em, size_t size, size_t nr)
{
	EditFace *efa= em->curface++;
	efa->fast= 1;
	return efa;
}

/* allocate 1 chunk for all vertices, edges, faces. These get tagged to
   prevent it from being freed
*/
static void init_editmesh_fastmalloc(EditMesh *em, int totvert, int totedge, int totface)
{
	if(totvert) em->allverts= MEM_callocN(totvert*sizeof(EditVert), "allverts");
	else em->allverts= NULL;
	em->curvert= em->allverts;
	
	if(totedge==0) totedge= 4*totface;	// max possible

	if(totedge) em->alledges= MEM_callocN(totedge*sizeof(EditEdge), "alledges");
	else em->alledges= NULL;
	em->curedge= em->alledges;
	
	if(totface) em->allfaces= MEM_callocN(totface*sizeof(EditFace), "allfaces");
	else em->allfaces= NULL;
	em->curface= em->allfaces;

	callocvert= calloc_fastvert;
	callocedge= calloc_fastedge;
	callocface= calloc_fastface;
}

static void end_editmesh_fastmalloc(void)
{
	callocvert= calloc_em;
	callocedge= calloc_em;
	callocface= calloc_em;
}

/* do not free editmesh itself here */
void free_editMesh(EditMesh *em)
{
	if(em==NULL) return;

	if(em->verts.first) free_vertlist(em, &em->verts);
	if(em->edges.first) free_edgelist(em, &em->edges);
	if(em->faces.first) free_facelist(em, &em->faces);
	if(em->selected.first) BLI_freelistN(&(em->selected));

	CustomData_free(&em->vdata, 0);
	CustomData_free(&em->fdata, 0);

	if(em->derivedFinal) {
		if (em->derivedFinal!=em->derivedCage) {
			em->derivedFinal->needsFree= 1;
			em->derivedFinal->release(em->derivedFinal);
		}
		em->derivedFinal= NULL;
	}
	if(em->derivedCage) {
		em->derivedCage->needsFree= 1;
		em->derivedCage->release(em->derivedCage);
		em->derivedCage= NULL;
	}

	/* DEBUG: hashtabs are slowest part of enter/exit editmode. here a testprint */
#if 0
	if(em->hashedgetab) {
		HashEdge *he, *hen;
		int a, used=0, max=0, nr;
		he= em->hashedgetab;
		for(a=0; a<EDHASHSIZE; a++, he++) {
			if(he->eed) used++;
			hen= he->next;
			nr= 0;
			while(hen) {
				nr++;
				hen= hen->next;
			}
			if(max<nr) max= nr;
		}
		printf("hastab used %d max %d\n", used, max);
	}
#endif
	if(em->hashedgetab) MEM_freeN(em->hashedgetab);
	em->hashedgetab= NULL;
	
	if(em->allverts) MEM_freeN(em->allverts);
	if(em->alledges) MEM_freeN(em->alledges);
	if(em->allfaces) MEM_freeN(em->allfaces);
	
	em->allverts= em->curvert= NULL;
	em->alledges= em->curedge= NULL;
	em->allfaces= em->curface= NULL;
	
	mesh_octree_table(NULL, NULL, NULL, 'e');
	
	em->totvert= em->totedge= em->totface= 0;

// XXX	if(em->retopo_paint_data) retopo_free_paint_data(em->retopo_paint_data);
	em->retopo_paint_data= NULL;
	em->act_face = NULL;
}

static void editMesh_set_hash(EditMesh *em)
{
	EditEdge *eed;

	if(em->hashedgetab) MEM_freeN(em->hashedgetab);
	em->hashedgetab= NULL;
	
	for(eed=em->edges.first; eed; eed= eed->next)  {
		if( findedgelist(em, eed->v1, eed->v2)==NULL )
			insert_hashedge(em, eed);
	}

}


/* ************************ IN & OUT EDITMODE ***************************** */


static void edge_normal_compare(EditEdge *eed, EditFace *efa1)
{
	EditFace *efa2;
	float cent1[3], cent2[3];
	float inp;
	
	efa2 = eed->tmp.f;
	if(efa1==efa2) return;
	
	inp= efa1->n[0]*efa2->n[0] + efa1->n[1]*efa2->n[1] + efa1->n[2]*efa2->n[2];
	if(inp<0.999 && inp >-0.999) eed->f2= 1;
		
	if(efa1->v4) cent_quad_v3(cent1, efa1->v1->co, efa1->v2->co, efa1->v3->co, efa1->v4->co);
	else cent_tri_v3(cent1, efa1->v1->co, efa1->v2->co, efa1->v3->co);
	if(efa2->v4) cent_quad_v3(cent2, efa2->v1->co, efa2->v2->co, efa2->v3->co, efa2->v4->co);
	else cent_tri_v3(cent2, efa2->v1->co, efa2->v2->co, efa2->v3->co);
	
	sub_v3_v3v3(cent1, cent2, cent1);
	normalize_v3(cent1);
	inp= cent1[0]*efa1->n[0] + cent1[1]*efa1->n[1] + cent1[2]*efa1->n[2]; 

	if(inp < -0.001 ) eed->f1= 1;
}

#if 0
typedef struct {
	EditEdge *eed;
	float noLen,no[3];
	int adjCount;
} EdgeDrawFlagInfo;

static int edgeDrawFlagInfo_cmp(const void *av, const void *bv)
{
	const EdgeDrawFlagInfo *a = av;
	const EdgeDrawFlagInfo *b = bv;

	if (a->noLen<b->noLen) return -1;
	else if (a->noLen>b->noLen) return 1;
	else return 0;
}
#endif

static void edge_drawflags(Mesh *me, EditMesh *em)
{
	EditVert *eve;
	EditEdge *eed, *e1, *e2, *e3, *e4;
	EditFace *efa;
	
	/* - count number of times edges are used in faces: 0 en 1 time means draw edge
	 * - edges more than 1 time used: in *tmp.f is pointer to first face
	 * - check all faces, when normal differs to much: draw (flag becomes 1)
	 */

	/* later on: added flags for 'cylinder' and 'sphere' intersection tests in old
	   game engine (2.04)
	 */
	
	recalc_editnormals(em);
	
	/* init */
	eve= em->verts.first;
	while(eve) {
		eve->f1= 1;		/* during test it's set at zero */
		eve= eve->next;
	}
	eed= em->edges.first;
	while(eed) {
		eed->f2= eed->f1= 0;
		eed->tmp.f = 0;
		eed= eed->next;
	}

	efa= em->faces.first;
	while(efa) {
		e1= efa->e1;
		e2= efa->e2;
		e3= efa->e3;
		e4= efa->e4;
		if(e1->f2<4) e1->f2+= 1;
		if(e2->f2<4) e2->f2+= 1;
		if(e3->f2<4) e3->f2+= 1;
		if(e4 && e4->f2<4) e4->f2+= 1;
		
		if(e1->tmp.f == 0) e1->tmp.f = (void *) efa;
		if(e2->tmp.f == 0) e2->tmp.f = (void *) efa;
		if(e3->tmp.f ==0) e3->tmp.f = (void *) efa;
		if(e4 && (e4->tmp.f == 0)) e4->tmp.f = (void *) efa;
		
		efa= efa->next;
	}

	if(me->drawflag & ME_ALLEDGES) {
		efa= em->faces.first;
		while(efa) {
			if(efa->e1->f2>=2) efa->e1->f2= 1;
			if(efa->e2->f2>=2) efa->e2->f2= 1;
			if(efa->e3->f2>=2) efa->e3->f2= 1;
			if(efa->e4 && efa->e4->f2>=2) efa->e4->f2= 1;
			
			efa= efa->next;
		}		
	}	
	else {
		
		/* handle single-edges for 'test cylinder flag' (old engine) */
		
		eed= em->edges.first;
		while(eed) {
			if(eed->f2==1) eed->f1= 1;
			eed= eed->next;
		}

		/* all faces, all edges with flag==2: compare normal */
		efa= em->faces.first;
		while(efa) {
			if(efa->e1->f2==2) edge_normal_compare(efa->e1, efa);
			else efa->e1->f2= 1;
			if(efa->e2->f2==2) edge_normal_compare(efa->e2, efa);
			else efa->e2->f2= 1;
			if(efa->e3->f2==2) edge_normal_compare(efa->e3, efa);
			else efa->e3->f2= 1;
			if(efa->e4) {
				if(efa->e4->f2==2) edge_normal_compare(efa->e4, efa);
				else efa->e4->f2= 1;
			}
			efa= efa->next;
		}
		
		/* sphere collision flag */
		
		eed= em->edges.first;
		while(eed) {
			if(eed->f1!=1) {
				eed->v1->f1= eed->v2->f1= 0;
			}
			eed= eed->next;
		}
		
	}
}

/* turns Mesh into editmesh */
void make_editMesh(Scene *scene, Object *ob)
{
	Mesh *me= ob->data;
	MFace *mface;
	MVert *mvert;
	MSelect *mselect;
	KeyBlock *actkey;
	EditMesh *em;
	EditVert *eve, **evlist, *eve1, *eve2, *eve3, *eve4;
	EditFace *efa, *efa_last_sel= NULL;
	EditEdge *eed;
	EditSelection *ese;
	float *co, (*keyco)[3]= NULL;
	int tot, a, eekadoodle= 0;

	if(me->edit_mesh==NULL)
		me->edit_mesh= MEM_callocN(sizeof(EditMesh), "editmesh");
	else 
		/* because of reload */
		free_editMesh(me->edit_mesh);
	
	em= me->edit_mesh;
	
	em->selectmode= scene->toolsettings->selectmode; // warning needs to be synced
	em->act_face = NULL;
	em->totvert= tot= me->totvert;
	em->totedge= me->totedge;
	em->totface= me->totface;
	
	if(tot==0) {
		return;
	}
	
	if(ob->actcol > 0)
		em->mat_nr= ob->actcol-1;

	/* initialize fastmalloc for editmesh */
	init_editmesh_fastmalloc(em, me->totvert, me->totedge, me->totface);

	actkey = ob_get_keyblock(ob);
	if(actkey) {
		/* undo-ing in past for previous editmode sessions gives corrupt 'keyindex' values */
		undo_editmode_clear();
		keyco= actkey->data;
		em->shapenr= ob->shapenr;
	}

	/* make editverts */
	CustomData_copy(&me->vdata, &em->vdata, CD_MASK_EDITMESH, CD_CALLOC, 0);
	mvert= me->mvert;

	evlist= (EditVert **)MEM_mallocN(tot*sizeof(void *),"evlist");
	for(a=0; a<tot; a++, mvert++) {
		
		co= mvert->co;

		/* edit the shape key coordinate if available */
		if(keyco && a < actkey->totelem)
			co= keyco[a];

		eve= addvertlist(em, co, NULL);
		evlist[a]= eve;
		
		/* face select sets selection in next loop */
		if(!paint_facesel_test(ob))
			eve->f |= (mvert->flag & 1);
		
		if (mvert->flag & ME_HIDE) eve->h= 1;		
		eve->no[0]= mvert->no[0]/32767.0;
		eve->no[1]= mvert->no[1]/32767.0;
		eve->no[2]= mvert->no[2]/32767.0;

		eve->bweight= ((float)mvert->bweight)/255.0f;

		/* lets overwrite the keyindex of the editvert
		 * with the order it used to be in before
		 * editmode
		 */
		eve->keyindex = a;

		CustomData_to_em_block(&me->vdata, &em->vdata, a, &eve->data);
	}

	if(actkey && actkey->totelem!=me->totvert);
	else {
		MEdge *medge= me->medge;
		
		CustomData_copy(&me->edata, &em->edata, CD_MASK_EDITMESH, CD_CALLOC, 0);
		/* make edges */
		for(a=0; a<me->totedge; a++, medge++) {
			eed= addedgelist(em, evlist[medge->v1], evlist[medge->v2], NULL);
			/* eed can be zero when v1 and v2 are identical, dxf import does this... */
			if(eed) {
				eed->crease= ((float)medge->crease)/255.0f;
				eed->bweight= ((float)medge->bweight)/255.0f;
				
				if(medge->flag & ME_SEAM) eed->seam= 1;
				if(medge->flag & ME_SHARP) eed->sharp = 1;
				if(medge->flag & SELECT) eed->f |= SELECT;
				if(medge->flag & ME_FGON) eed->h= EM_FGON;	// 2 different defines!
				if(medge->flag & ME_HIDE) eed->h |= 1;
				if(em->selectmode==SCE_SELECT_EDGE) 
					EM_select_edge(eed, eed->f & SELECT);		// force edge selection to vertices, seems to be needed ...
				CustomData_to_em_block(&me->edata,&em->edata, a, &eed->data);
			}
		}
		
		CustomData_copy(&me->fdata, &em->fdata, CD_MASK_EDITMESH, CD_CALLOC, 0);

		/* make faces */
		mface= me->mface;

		for(a=0; a<me->totface; a++, mface++) {
			eve1= evlist[mface->v1];
			eve2= evlist[mface->v2];
			if(!mface->v3) eekadoodle= 1;
			eve3= evlist[mface->v3];
			if(mface->v4) eve4= evlist[mface->v4]; else eve4= NULL;
			
			efa= addfacelist(em, eve1, eve2, eve3, eve4, NULL, NULL);

			if(efa) {
				CustomData_to_em_block(&me->fdata, &em->fdata, a, &efa->data);

				efa->mat_nr= mface->mat_nr;
				efa->flag= mface->flag & ~ME_HIDE;
				
				/* select and hide face flag */
				if(mface->flag & ME_HIDE) {
					efa->h= 1;
				} else {
					if (a==me->act_face) {
						EM_set_actFace(em, efa);
					}
					
					/* dont allow hidden and selected */
					if(mface->flag & ME_FACE_SEL) {
						efa->f |= SELECT;
						
						if(paint_facesel_test(ob)) {
							EM_select_face(efa, 1); /* flush down */
						}

						efa_last_sel= efa;
					}
				}
			}
		}
	}
	
	if(EM_get_actFace(em, 0) && efa_last_sel) {
		EM_set_actFace(em, efa_last_sel);
	}

	if(eekadoodle)
		error("This Mesh has old style edgecodes, please put it in the bugtracker!");
	
	MEM_freeN(evlist);

	end_editmesh_fastmalloc();	// resets global function pointers
	
	if(me->mselect){
		//restore editselections
		EM_init_index_arrays(em, 1,1,1);
		mselect = me->mselect;
		
		for(a=0; a<me->totselect; a++, mselect++){
			/*check if recorded selection is still valid, if so copy into editmesh*/
			if( (mselect->type == EDITVERT && me->mvert[mselect->index].flag & SELECT) || (mselect->type == EDITEDGE && me->medge[mselect->index].flag & SELECT) || (mselect->type == EDITFACE && me->mface[mselect->index].flag & ME_FACE_SEL) ){
				ese = MEM_callocN(sizeof(EditSelection), "Edit Selection");
				ese->type = mselect->type;	
				if(ese->type == EDITVERT) ese->data = EM_get_vert_for_index(mselect->index); else
				if(ese->type == EDITEDGE) ese->data = EM_get_edge_for_index(mselect->index); else
				if(ese->type == EDITFACE) ese->data = EM_get_face_for_index(mselect->index);
				BLI_addtail(&(em->selected),ese);
			}
		}
		EM_free_index_arrays();
	}
	/* this creates coherent selections. also needed for older files */
	EM_selectmode_set(em);
	/* paranoia check to enforce hide rules */
	EM_hide_reset(em);
	/* sets helper flags which arent saved */
	EM_fgon_flags(em);
	
	if (EM_get_actFace(em, 0)==NULL) {
		EM_set_actFace(em, em->faces.first ); /* will use the first face, this is so we alwats have an active face */
	}
}

/* makes Mesh out of editmesh */
void load_editMesh(Scene *scene, Object *ob)
{
	Mesh *me= ob->data;
	MVert *mvert, *oldverts;
	MEdge *medge;
	MFace *mface;
	MSelect *mselect;
	EditMesh *em= me->edit_mesh;
	EditVert *eve;
	EditFace *efa, *efa_act;
	EditEdge *eed;
	EditSelection *ese;
	float *fp, *newkey, *oldkey, nor[3];
	int i, a, ototvert;
	
	/* this one also tests of edges are not in faces: */
	/* eed->f2==0: not in face, f2==1: draw it */
	/* eed->f1 : flag for dynaface (cylindertest, old engine) */
	/* eve->f1 : flag for dynaface (sphere test, old engine) */
	/* eve->f2 : being used in vertexnormals */
	edge_drawflags(me, em);
	
	EM_stats_update(em);
	
	/* new Vertex block */
	if(em->totvert==0) mvert= NULL;
	else mvert= MEM_callocN(em->totvert*sizeof(MVert), "loadeditMesh vert");

	/* new Edge block */
	if(em->totedge==0) medge= NULL;
	else medge= MEM_callocN(em->totedge*sizeof(MEdge), "loadeditMesh edge");
	
	/* new Face block */
	if(em->totface==0) mface= NULL;
	else mface= MEM_callocN(em->totface*sizeof(MFace), "loadeditMesh face");

	/* lets save the old verts just in case we are actually working on
	 * a key ... we now do processing of the keys at the end */
	oldverts= me->mvert;
	ototvert= me->totvert;

	/* don't free this yet */
	CustomData_set_layer(&me->vdata, CD_MVERT, NULL);

	/* free custom data */
	CustomData_free(&me->vdata, me->totvert);
	CustomData_free(&me->edata, me->totedge);
	CustomData_free(&me->fdata, me->totface);

	/* add new custom data */
	me->totvert= em->totvert;
	me->totedge= em->totedge;
	me->totface= em->totface;

	CustomData_copy(&em->vdata, &me->vdata, CD_MASK_MESH, CD_CALLOC, me->totvert);
	CustomData_copy(&em->edata, &me->edata, CD_MASK_MESH, CD_CALLOC, me->totedge);
	CustomData_copy(&em->fdata, &me->fdata, CD_MASK_MESH, CD_CALLOC, me->totface);

	CustomData_add_layer(&me->vdata, CD_MVERT, CD_ASSIGN, mvert, me->totvert);
	CustomData_add_layer(&me->edata, CD_MEDGE, CD_ASSIGN, medge, me->totedge);
	CustomData_add_layer(&me->fdata, CD_MFACE, CD_ASSIGN, mface, me->totface);
	mesh_update_customdata_pointers(me);

	/* the vertices, use ->tmp.l as counter */
	eve= em->verts.first;
	a= 0;

	while(eve) {
		VECCOPY(mvert->co, eve->co);

		mvert->mat_nr= 32767;  /* what was this for, halos? */
		
		/* vertex normal */
		VECCOPY(nor, eve->no);
		mul_v3_fl(nor, 32767.0);
		VECCOPY(mvert->no, nor);

		/* note: it used to remove me->dvert when it was not in use, cancelled
		   that... annoying when you have a fresh vgroup */
		CustomData_from_em_block(&em->vdata, &me->vdata, eve->data, a);

		eve->tmp.l = a++;  /* counter */
			
		mvert->flag= 0;
		mvert->flag |= (eve->f & SELECT);
		if (eve->h) mvert->flag |= ME_HIDE;
		
		mvert->bweight= (char)(255.0*eve->bweight);

		eve= eve->next;
		mvert++;
	}

	/* the edges */
	a= 0;
	eed= em->edges.first;
	while(eed) {
		medge->v1= (unsigned int) eed->v1->tmp.l;
		medge->v2= (unsigned int) eed->v2->tmp.l;
		
		medge->flag= (eed->f & SELECT) | ME_EDGERENDER;
		if(eed->f2<2) medge->flag |= ME_EDGEDRAW;
		if(eed->f2==0) medge->flag |= ME_LOOSEEDGE;
		if(eed->sharp) medge->flag |= ME_SHARP;
		if(eed->seam) medge->flag |= ME_SEAM;
		if(eed->h & EM_FGON) medge->flag |= ME_FGON;	// different defines yes
		if(eed->h & 1) medge->flag |= ME_HIDE;
		
		medge->crease= (char)(255.0*eed->crease);
		medge->bweight= (char)(255.0*eed->bweight);
		CustomData_from_em_block(&em->edata, &me->edata, eed->data, a);		

		eed->tmp.l = a++;
		
		medge++;
		eed= eed->next;
	}

	/* the faces */
	a = 0;
	efa= em->faces.first;
	efa_act= EM_get_actFace(em, 0);
	i = 0;
	me->act_face = -1;
	while(efa) {
		mface= &((MFace *) me->mface)[i];
		
		mface->v1= (unsigned int) efa->v1->tmp.l;
		mface->v2= (unsigned int) efa->v2->tmp.l;
		mface->v3= (unsigned int) efa->v3->tmp.l;
		if (efa->v4) mface->v4 = (unsigned int) efa->v4->tmp.l;

		mface->mat_nr= efa->mat_nr;
		
		mface->flag= efa->flag;
		/* bit 0 of flag is already taken for smooth... */
		
		if(efa->h) {
			mface->flag |= ME_HIDE;
			mface->flag &= ~ME_FACE_SEL;
		} else {
			if(efa->f & 1) mface->flag |= ME_FACE_SEL;
			else mface->flag &= ~ME_FACE_SEL;
		}
		
		/* mat_nr in vertex */
		if(me->totcol>1) {
			mvert= me->mvert+mface->v1;
			if(mvert->mat_nr == (char)32767) mvert->mat_nr= mface->mat_nr;
			mvert= me->mvert+mface->v2;
			if(mvert->mat_nr == (char)32767) mvert->mat_nr= mface->mat_nr;
			mvert= me->mvert+mface->v3;
			if(mvert->mat_nr == (char)32767) mvert->mat_nr= mface->mat_nr;
			if(mface->v4) {
				mvert= me->mvert+mface->v4;
				if(mvert->mat_nr == (char)32767) mvert->mat_nr= mface->mat_nr;
			}
		}
			
		/* watch: efa->e1->f2==0 means loose edge */ 
			
		if(efa->e1->f2==1) {
			efa->e1->f2= 2;
		}			
		if(efa->e2->f2==1) {
			efa->e2->f2= 2;
		}
		if(efa->e3->f2==1) {
			efa->e3->f2= 2;
		}
		if(efa->e4 && efa->e4->f2==1) {
			efa->e4->f2= 2;
		}

		CustomData_from_em_block(&em->fdata, &me->fdata, efa->data, i);

		/* no index '0' at location 3 or 4 */
		test_index_face(mface, &me->fdata, i, efa->v4?4:3);
		
		if (efa_act == efa)
			me->act_face = a;

		efa->tmp.l = a++;
		i++;
		efa= efa->next;
	}

	/* patch hook indices and vertex parents */
	{
		Object *ob;
		ModifierData *md;
		EditVert **vertMap = NULL;
		int i,j;

		for (ob=G.main->object.first; ob; ob=ob->id.next) {
			if (ob->parent==ob && ELEM(ob->partype, PARVERT1,PARVERT3)) {
				
				/* duplicate code from below, make it function later...? */
				if (!vertMap) {
					vertMap = MEM_callocN(sizeof(*vertMap)*ototvert, "vertMap");
					
					for (eve=em->verts.first; eve; eve=eve->next) {
						if (eve->keyindex!=-1)
							vertMap[eve->keyindex] = eve;
					}
				}
				if(ob->par1 < ototvert) {
					eve = vertMap[ob->par1];
					if(eve) ob->par1= eve->tmp.l;
				}
				if(ob->par2 < ototvert) {
					eve = vertMap[ob->par2];
					if(eve) ob->par2= eve->tmp.l;
				}
				if(ob->par3 < ototvert) {
					eve = vertMap[ob->par3];
					if(eve) ob->par3= eve->tmp.l;
				}
				
			}
			if (ob->data==me) {
				for (md=ob->modifiers.first; md; md=md->next) {
					if (md->type==eModifierType_Hook) {
						HookModifierData *hmd = (HookModifierData*) md;

						if (!vertMap) {
							vertMap = MEM_callocN(sizeof(*vertMap)*ototvert, "vertMap");

							for (eve=em->verts.first; eve; eve=eve->next) {
								if (eve->keyindex!=-1)
									vertMap[eve->keyindex] = eve;
							}
						}
						
						for (i=j=0; i<hmd->totindex; i++) {
							if(hmd->indexar[i] < ototvert) {
								eve = vertMap[hmd->indexar[i]];
								
								if (eve) {
									hmd->indexar[j++] = eve->tmp.l;
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

	/* are there keys? */
	if(me->key) {
		KeyBlock *currkey;
		KeyBlock *actkey= BLI_findlink(&me->key->block, em->shapenr-1);

		/* Lets reorder the key data so that things line up roughly
		 * with the way things were before editmode */
		currkey = me->key->block.first;
		while(currkey) {
			
			fp= newkey= MEM_callocN(me->key->elemsize*em->totvert,  "currkey->data");
			oldkey = currkey->data;

			eve= em->verts.first;

			i = 0;
			mvert = me->mvert;
			while(eve) {
				if (eve->keyindex >= 0 && eve->keyindex < currkey->totelem) { // valid old vertex
					if(currkey == actkey) {
						if(actkey == me->key->refkey) {
							VECCOPY(fp, mvert->co);
						}
						else {
							VECCOPY(fp, mvert->co);
							if(oldverts) {
								VECCOPY(mvert->co, oldverts[eve->keyindex].co);
							}
						}
					}
					else {
						if(oldkey) {
							VECCOPY(fp, oldkey + 3 * eve->keyindex);
						}
					}
				}
				else {
					VECCOPY(fp, mvert->co);
				}
				fp+= 3;
				++i;
				++mvert;
				eve= eve->next;
			}
			currkey->totelem= em->totvert;
			if(currkey->data) MEM_freeN(currkey->data);
			currkey->data = newkey;
			
			currkey= currkey->next;
		}
	}

	if(oldverts) MEM_freeN(oldverts);
	
	i = 0;
	for(ese=em->selected.first; ese; ese=ese->next) i++;
	me->totselect = i;
	if(i==0) mselect= NULL;
	else mselect= MEM_callocN(i*sizeof(MSelect), "loadeditMesh selections");
	
	if(me->mselect) MEM_freeN(me->mselect);
	me->mselect= mselect;
	
	for(ese=em->selected.first; ese; ese=ese->next){
		mselect->type = ese->type;
		if(ese->type == EDITVERT) mselect->index = ((EditVert*)ese->data)->tmp.l;
		else if(ese->type == EDITEDGE) mselect->index = ((EditEdge*)ese->data)->tmp.l;
		else if(ese->type == EDITFACE) mselect->index = ((EditFace*)ese->data)->tmp.l;
		mselect++;
	}
	
	/* to be sure: clear ->tmp.l pointers */
	eve= em->verts.first;
	while(eve) {
		eve->tmp.l = 0;
		eve= eve->next;
	}
	
	eed= em->edges.first;
	while(eed) { 
		eed->tmp.l = 0;
		eed= eed->next;
	}
	
	efa= em->faces.first;
	while(efa) {
		efa->tmp.l = 0;
		efa= efa->next;
	}
	
	/* remake softbody of all users */
	if(me->id.us>1) {
		Base *base;
		for(base= scene->base.first; base; base= base->next)
			if(base->object->data==me)
				base->object->recalc |= OB_RECALC_DATA;
	}

	mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
}

void remake_editMesh(Scene *scene, Object *ob)
{
	make_editMesh(scene, ob);
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	BIF_undo_push("Undo all changes");
}

/* *************** Operator: separate parts *************/

static EnumPropertyItem prop_separate_types[] = {
	{0, "SELECTED", 0, "Selection", ""},
	{1, "MATERIAL", 0, "By Material", ""},
	{2, "LOOSE", 0, "By loose parts", ""},
	{0, NULL, 0, NULL, NULL}
};

/* return 1: success */
static int mesh_separate_selected(Scene *scene, Base *editbase)
{
	EditMesh *em, *emnew;
	EditVert *eve, *v1;
	EditEdge *eed, *e1;
	EditFace *efa, *f1;
	Object *obedit;
	Mesh *me, *menew;
	Base *basenew;
	
	if(editbase==NULL) return 0;
	
	obedit= editbase->object;
	me= obedit->data;
	em= BKE_mesh_get_editmesh(me);
	if(me->key) {
		error("Can't separate with vertex keys");
		BKE_mesh_end_editmesh(me, em);
		return 0;
	}
	
	if(em->selected.first) 
		BLI_freelistN(&(em->selected)); /* clear the selection order */
		
	EM_selectmode_set(em);	// enforce full consistent selection flags 
	
	EM_stats_update(em);
	
	if(em->totvertsel==0) {
		BKE_mesh_end_editmesh(me, em);
		return 0;
	}
	
	/* we are going to work as follows:
	 * 1. add a linked duplicate object: this will be the new one, we remember old pointer
	 * 2. give new object empty mesh and put in editmode
	 * 3: do a split if needed on current editmesh.
	 * 4. copy over: all NOT selected verts, edges, faces
	 * 5. call load_editMesh() on the new object
	 */
	
	/* 1 */
	basenew= ED_object_add_duplicate(scene, editbase, 0);	/* 0 = fully linked */
	ED_base_object_select(basenew, BA_DESELECT);
	
	/* 2 */
	basenew->object->data= menew= add_mesh(me->id.name+2);	/* empty */
	assign_matarar(basenew->object, give_matarar(obedit), *give_totcolp(obedit)); /* new in 2.5 */
	me->id.us--;
	make_editMesh(scene, basenew->object);
	emnew= menew->edit_mesh;
	CustomData_copy(&em->vdata, &emnew->vdata, CD_MASK_EDITMESH, CD_DEFAULT, 0);
	CustomData_copy(&em->edata, &emnew->edata, CD_MASK_EDITMESH, CD_DEFAULT, 0);
	CustomData_copy(&em->fdata, &emnew->fdata, CD_MASK_EDITMESH, CD_DEFAULT, 0);
	
	/* 3 */
	/* SPLIT: first make duplicate */
	adduplicateflag(em, SELECT);
	/* SPLIT: old faces have 3x flag 128 set, delete these ones */
	delfaceflag(em, 128);
	/* since we do tricky things with verts/edges/faces, this makes sure all is selected coherent */
	EM_selectmode_set(em);
	
	/* 4 */
	/* move over: everything that is selected */
	for(eve= em->verts.first; eve; eve= v1) {
		v1= eve->next;
		if(eve->f & SELECT) {
			BLI_remlink(&em->verts, eve);
			BLI_addtail(&emnew->verts, eve);
		}
	}
	
	for(eed= em->edges.first; eed; eed= e1) {
		e1= eed->next;
		if(eed->f & SELECT) {
			BLI_remlink(&em->edges, eed);
			BLI_addtail(&emnew->edges, eed);
		}
	}
	
	for(efa= em->faces.first; efa; efa= f1) {
		f1= efa->next;
		if (efa == em->act_face && (efa->f & SELECT)) {
			EM_set_actFace(em, NULL);
		}

		if(efa->f & SELECT) {
			BLI_remlink(&em->faces, efa);
			BLI_addtail(&emnew->faces, efa);
		}
	}

	/* 5 */
	load_editMesh(scene, basenew->object);
	free_editMesh(emnew);
	MEM_freeN(menew->edit_mesh);
	menew->edit_mesh= NULL;
	
	/* hashedges are invalid now, make new! */
	editMesh_set_hash(em);

	DAG_id_flush_update(&obedit->id, OB_RECALC_DATA);	
	DAG_id_flush_update(&basenew->object->id, OB_RECALC_DATA);	

	BKE_mesh_end_editmesh(me, em);

	return 1;
}

/* return 1: success */
static int mesh_separate_material(Scene *scene, Base *editbase)
{
	Mesh *me= editbase->object->data;
	EditMesh *em= BKE_mesh_get_editmesh(me);
	unsigned char curr_mat;
	
	for (curr_mat = 1; curr_mat < editbase->object->totcol; ++curr_mat) {
		/* clear selection, we're going to use that to select material group */
		EM_clear_flag_all(em, SELECT);
		/* select the material */
		EM_select_by_material(em, curr_mat);
		/* and now separate */
		if(0==mesh_separate_selected(scene, editbase)) {
			BKE_mesh_end_editmesh(me, em);
			return 0;
		}
	}

	BKE_mesh_end_editmesh(me, em);
	return 1;
}

/* return 1: success */
static int mesh_separate_loose(Scene *scene, Base *editbase)
{
	Mesh *me;
	EditMesh *em;
	int doit= 1;
	
	me= editbase->object->data;
	em= BKE_mesh_get_editmesh(me);
	
	if(me->key) {
		error("Can't separate with vertex keys");
		BKE_mesh_end_editmesh(me, em);
		return 0;
	}
	
	EM_clear_flag_all(em, SELECT);
	
	while(doit && em->verts.first) {
		/* Select a random vert to start with */
		EditVert *eve= em->verts.first;
		eve->f |= SELECT;
		
		selectconnected_mesh_all(em);
		
		/* and now separate */
		doit= mesh_separate_selected(scene, editbase);
	}

	BKE_mesh_end_editmesh(me, em);
	return 1;
}


static int mesh_separate_exec(bContext *C, wmOperator *op)
{
	Scene *scene= CTX_data_scene(C);
	Base *base= CTX_data_active_base(C);
	int retval= 0, type= RNA_enum_get(op->ptr, "type");
	
	if(type == 0)
		retval= mesh_separate_selected(scene, base);
	else if(type == 1)
		retval= mesh_separate_material (scene, base);
	else if(type == 2)
		retval= mesh_separate_loose(scene, base);
	   
	if(retval) {
		WM_event_add_notifier(C, NC_GEOM|ND_DATA, base->object->data);
		return OPERATOR_FINISHED;
	}
	return OPERATOR_CANCELLED;
}

void MESH_OT_separate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Separate";
	ot->description= "Separate selected geometry into a new mesh.";
	ot->idname= "MESH_OT_separate";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= mesh_separate_exec;
	ot->poll= ED_operator_editmesh;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	ot->prop= RNA_def_enum(ot->srna, "type", prop_separate_types, 0, "Type", "");
}


/* ******************************************** */

/* *************** UNDO ***************************** */
/* new mesh undo, based on pushing editmesh data itself */
/* reuses same code as for global and curve undo... unify that (ton) */

/* only one 'hack', to save memory it doesn't store the first push, but does a remake editmesh */

/* a compressed version of editmesh data */

typedef struct EditVertC
{
	float no[3];
	float co[3];
	unsigned char f, h;
	short bweight;
	int keyindex;
} EditVertC;

typedef struct EditEdgeC
{
	int v1, v2;
	unsigned char f, h, seam, sharp, pad;
	short crease, bweight, fgoni;
} EditEdgeC;

typedef struct EditFaceC
{
	int v1, v2, v3, v4;
	unsigned char flag, f, h, fgonf, pad1;
	short mat_nr;
} EditFaceC;

typedef struct EditSelectionC{
	short type;
	int index;
}EditSelectionC;

typedef struct UndoMesh {
	EditVertC *verts;
	EditEdgeC *edges;
	EditFaceC *faces;
	EditSelectionC *selected;
	int totvert, totedge, totface, totsel;
	int selectmode, shapenr;
	RetopoPaintData *retopo_paint_data;
	char retopo_mode;
	CustomData vdata, edata, fdata;
} UndoMesh;

/* for callbacks */

static void free_undoMesh(void *umv)
{
	UndoMesh *um= umv;
	
	if(um->verts) MEM_freeN(um->verts);
	if(um->edges) MEM_freeN(um->edges);
	if(um->faces) MEM_freeN(um->faces);
	if(um->selected) MEM_freeN(um->selected);
// XXX	if(um->retopo_paint_data) retopo_free_paint_data(um->retopo_paint_data);
	CustomData_free(&um->vdata, um->totvert);
	CustomData_free(&um->edata, um->totedge);
	CustomData_free(&um->fdata, um->totface);
	MEM_freeN(um);
}

static void *editMesh_to_undoMesh(void *emv)
{
	EditMesh *em= (EditMesh *)emv;
	UndoMesh *um;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	EditSelection *ese;
	EditVertC *evec=NULL;
	EditEdgeC *eedc=NULL;
	EditFaceC *efac=NULL;
	EditSelectionC *esec=NULL;
	int a;

	um= MEM_callocN(sizeof(UndoMesh), "undomesh");
	
	um->selectmode = em->selectmode;
	um->shapenr = em->shapenr;
	
	for(eve=em->verts.first; eve; eve= eve->next) um->totvert++;
	for(eed=em->edges.first; eed; eed= eed->next) um->totedge++;
	for(efa=em->faces.first; efa; efa= efa->next) um->totface++;
	for(ese=em->selected.first; ese; ese=ese->next) um->totsel++; 
	/* malloc blocks */
	
	if(um->totvert) evec= um->verts= MEM_callocN(um->totvert*sizeof(EditVertC), "allvertsC");
	if(um->totedge) eedc= um->edges= MEM_callocN(um->totedge*sizeof(EditEdgeC), "alledgesC");
	if(um->totface) efac= um->faces= MEM_callocN(um->totface*sizeof(EditFaceC), "allfacesC");
	if(um->totsel) esec= um->selected= MEM_callocN(um->totsel*sizeof(EditSelectionC), "allselections");

	if(um->totvert) CustomData_copy(&em->vdata, &um->vdata, CD_MASK_EDITMESH, CD_CALLOC, um->totvert);
	if(um->totedge) CustomData_copy(&em->edata, &um->edata, CD_MASK_EDITMESH, CD_CALLOC, um->totedge);
	if(um->totface) CustomData_copy(&em->fdata, &um->fdata, CD_MASK_EDITMESH, CD_CALLOC, um->totface);
	
	/* now copy vertices */
	a = 0;
	for(eve=em->verts.first; eve; eve= eve->next, evec++, a++) {
		VECCOPY(evec->co, eve->co);
		VECCOPY(evec->no, eve->no);

		evec->f= eve->f;
		evec->h= eve->h;
		evec->keyindex= eve->keyindex;
		eve->tmp.l = a; /*store index*/
		evec->bweight= (short)(eve->bweight*255.0);

		CustomData_from_em_block(&em->vdata, &um->vdata, eve->data, a);
	}
	
	/* copy edges */
	a = 0;
	for(eed=em->edges.first; eed; eed= eed->next, eedc++, a++)  {
		eedc->v1= (int)eed->v1->tmp.l;
		eedc->v2= (int)eed->v2->tmp.l;
		eedc->f= eed->f;
		eedc->h= eed->h;
		eedc->seam= eed->seam;
		eedc->sharp= eed->sharp;
		eedc->crease= (short)(eed->crease*255.0);
		eedc->bweight= (short)(eed->bweight*255.0);
		eedc->fgoni= eed->fgoni;
		eed->tmp.l = a; /*store index*/
		CustomData_from_em_block(&em->edata, &um->edata, eed->data, a);
	
	}
	
	/* copy faces */
	a = 0;
	for(efa=em->faces.first; efa; efa= efa->next, efac++, a++) {
		efac->v1= (int)efa->v1->tmp.l;
		efac->v2= (int)efa->v2->tmp.l;
		efac->v3= (int)efa->v3->tmp.l;
		if(efa->v4) efac->v4= (int)efa->v4->tmp.l;
		else efac->v4= -1;
		
		efac->mat_nr= efa->mat_nr;
		efac->flag= efa->flag;
		efac->f= efa->f;
		efac->h= efa->h;
		efac->fgonf= efa->fgonf;

		efa->tmp.l = a; /*store index*/

		CustomData_from_em_block(&em->fdata, &um->fdata, efa->data, a);
	}
	
	a = 0;
	for(ese=em->selected.first; ese; ese=ese->next, esec++){
		esec->type = ese->type;
		if(ese->type == EDITVERT) a = esec->index = ((EditVert*)ese->data)->tmp.l; 
		else if(ese->type == EDITEDGE) a = esec->index = ((EditEdge*)ese->data)->tmp.l; 
		else if(ese->type == EDITFACE) a = esec->index = ((EditFace*)ese->data)->tmp.l;
	}

// XXX	um->retopo_paint_data= retopo_paint_data_copy(em->retopo_paint_data);
//	um->retopo_mode= scene->toolsettings->retopo_mode;
	
	return um;
}

static void undoMesh_to_editMesh(void *umv, void *emv)
{
	EditMesh *em= (EditMesh *)emv;
	UndoMesh *um= (UndoMesh *)umv;
	EditVert *eve, **evar=NULL;
	EditEdge *eed;
	EditFace *efa;
	EditSelection *ese;
	EditVertC *evec;
	EditEdgeC *eedc;
	EditFaceC *efac;
	EditSelectionC *esec;
	int a=0;

	free_editMesh(em);
	
	/* malloc blocks */
	memset(em, 0, sizeof(EditMesh));
		
	em->selectmode = um->selectmode;
	em->shapenr = um->shapenr;
	
	init_editmesh_fastmalloc(em, um->totvert, um->totedge, um->totface);

	CustomData_free(&em->vdata, 0);
	CustomData_free(&em->edata, 0);
	CustomData_free(&em->fdata, 0);

	CustomData_copy(&um->vdata, &em->vdata, CD_MASK_EDITMESH, CD_CALLOC, 0);
	CustomData_copy(&um->edata, &em->edata, CD_MASK_EDITMESH, CD_CALLOC, 0);
	CustomData_copy(&um->fdata, &em->fdata, CD_MASK_EDITMESH, CD_CALLOC, 0);

	/* now copy vertices */

	if(um->totvert) evar= MEM_mallocN(um->totvert*sizeof(EditVert *), "vertex ar");
	for(a=0, evec= um->verts; a<um->totvert; a++, evec++) {
		eve= addvertlist(em, evec->co, NULL);
		evar[a]= eve;

		VECCOPY(eve->no, evec->no);
		eve->f= evec->f;
		eve->h= evec->h;
		eve->keyindex= evec->keyindex;
		eve->bweight= ((float)evec->bweight)/255.0f;

		CustomData_to_em_block(&um->vdata, &em->vdata, a, &eve->data);
	}

	/* copy edges */
	for(a=0, eedc= um->edges; a<um->totedge; a++, eedc++) {
		eed= addedgelist(em, evar[eedc->v1], evar[eedc->v2], NULL);

		eed->f= eedc->f;
		eed->h= eedc->h;
		eed->seam= eedc->seam;
		eed->sharp= eedc->sharp;
		eed->fgoni= eedc->fgoni;
		eed->crease= ((float)eedc->crease)/255.0f;
		eed->bweight= ((float)eedc->bweight)/255.0f;
		CustomData_to_em_block(&um->edata, &em->edata, a, &eed->data);
	}
	
	/* copy faces */
	for(a=0, efac= um->faces; a<um->totface; a++, efac++) {
		if(efac->v4 != -1)
			efa= addfacelist(em, evar[efac->v1], evar[efac->v2], evar[efac->v3], evar[efac->v4], NULL, NULL);
		else 
			efa= addfacelist(em, evar[efac->v1], evar[efac->v2], evar[efac->v3], NULL, NULL ,NULL);

		efa->mat_nr= efac->mat_nr;
		efa->flag= efac->flag;
		efa->f= efac->f;
		efa->h= efac->h;
		efa->fgonf= efac->fgonf;
		
		CustomData_to_em_block(&um->fdata, &em->fdata, a, &efa->data);
	}
	
	end_editmesh_fastmalloc();
	if(evar) MEM_freeN(evar);
	
	em->totvert = um->totvert;
	em->totedge = um->totedge;
	em->totface = um->totface;
	/*restore stored editselections*/
	if(um->totsel){
		EM_init_index_arrays(em, 1,1,1);
		for(a=0, esec= um->selected; a<um->totsel; a++, esec++){
			ese = MEM_callocN(sizeof(EditSelection), "Edit Selection");
			ese->type = esec->type;
			if(ese->type == EDITVERT) ese->data = EM_get_vert_for_index(esec->index); else
			if(ese->type == EDITEDGE) ese->data = EM_get_edge_for_index(esec->index); else
			if(ese->type == EDITFACE) ese->data = EM_get_face_for_index(esec->index);
			BLI_addtail(&(em->selected),ese);
		}
		EM_free_index_arrays();
	}

	/* restore total selections */
	EM_nvertices_selected(em);
	EM_nedges_selected(em);
	EM_nfaces_selected(em);

// XXX	retopo_free_paint();
//	em->retopo_paint_data= retopo_paint_data_copy(um->retopo_paint_data);
//	scene->toolsettings->retopo_mode= um->retopo_mode;
//	if(scene->toolsettings->retopo_mode) {
// XXX		if(G.vd->depths) G.vd->depths->damaged= 1;
//		retopo_queue_updates(G.vd);
//		retopo_paint_view_update(G.vd);
//	}
	
}

static void *getEditMesh(bContext *C)
{
	Object *obedit= CTX_data_edit_object(C);
	if(obedit && obedit->type==OB_MESH) {
		Mesh *me= obedit->data;
		return me->edit_mesh;
	}
	return NULL;
}

/* and this is all the undo system needs to know */
void undo_push_mesh(bContext *C, char *name)
{
	undo_editmode_push(C, name, getEditMesh, free_undoMesh, undoMesh_to_editMesh, editMesh_to_undoMesh, NULL);
}



/* *************** END UNDO *************/

static EditVert **g_em_vert_array = NULL;
static EditEdge **g_em_edge_array = NULL;
static EditFace **g_em_face_array = NULL;

void EM_init_index_arrays(EditMesh *em, int forVert, int forEdge, int forFace)
{
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	int i;

	if (forVert) {
		em->totvert= BLI_countlist(&em->verts);

		if(em->totvert) {
			g_em_vert_array = MEM_mallocN(sizeof(*g_em_vert_array)*em->totvert, "em_v_arr");

			for (i=0,eve=em->verts.first; eve; i++,eve=eve->next)
				g_em_vert_array[i] = eve;
		}
	}

	if (forEdge) {
		em->totedge= BLI_countlist(&em->edges);

		if(em->totedge) {
			g_em_edge_array = MEM_mallocN(sizeof(*g_em_edge_array)*em->totedge, "em_e_arr");

			for (i=0,eed=em->edges.first; eed; i++,eed=eed->next)
				g_em_edge_array[i] = eed;
		}
	}

	if (forFace) {
		em->totface= BLI_countlist(&em->faces);

		if(em->totface) {
			g_em_face_array = MEM_mallocN(sizeof(*g_em_face_array)*em->totface, "em_f_arr");

			for (i=0,efa=em->faces.first; efa; i++,efa=efa->next)
				g_em_face_array[i] = efa;
		}
	}
}

void EM_free_index_arrays(void)
{
	if (g_em_vert_array) MEM_freeN(g_em_vert_array);
	if (g_em_edge_array) MEM_freeN(g_em_edge_array);
	if (g_em_face_array) MEM_freeN(g_em_face_array);
	g_em_vert_array = NULL;
	g_em_edge_array = NULL;
	g_em_face_array = NULL;
}

EditVert *EM_get_vert_for_index(int index)
{
	return g_em_vert_array?g_em_vert_array[index]:NULL;
}

EditEdge *EM_get_edge_for_index(int index)
{
	return g_em_edge_array?g_em_edge_array[index]:NULL;
}

EditFace *EM_get_face_for_index(int index)
{
	return g_em_face_array?g_em_face_array[index]:NULL;
}

/* can we edit UV's for this mesh?*/
int EM_texFaceCheck(EditMesh *em)
{
	/* some of these checks could be a touch overkill */
	if (	(em) &&
			(em->faces.first) &&
			(CustomData_has_layer(&em->fdata, CD_MTFACE)))
		return 1;
	return 0;
}

/* can we edit colors for this mesh?*/
int EM_vertColorCheck(EditMesh *em)
{
	/* some of these checks could be a touch overkill */
	if (	(em) &&
			(em->faces.first) &&
			(CustomData_has_layer(&em->fdata, CD_MCOL)))
		return 1;
	return 0;
}


void em_setup_viewcontext(bContext *C, ViewContext *vc)
{
	view3d_set_viewcontext(C, vc);
	
	if(vc->obedit) {
		Mesh *me= vc->obedit->data;
		vc->em= me->edit_mesh;
	}
}
