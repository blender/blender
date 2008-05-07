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
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

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
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_dynstr.h"
#include "BLI_rand.h"

#include "BKE_DerivedMesh.h"
#include "BKE_depsgraph.h"
#include "BKE_cloth.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_key.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_modifier.h"
#include "BKE_multires.h"
#include "BKE_object.h"
#include "BKE_pointcache.h"
#include "BKE_softbody.h"
#include "BKE_texture.h"
#include "BKE_utildefines.h"

#ifdef WITH_VERSE
#include "BKE_verse.h"
#endif

#include "BIF_editkey.h"
#include "BIF_editmesh.h"
#include "BIF_editmode_undo.h"
#include "BIF_interface.h"
#include "BIF_meshtools.h"
#include "BIF_mywindow.h"
#include "BIF_retopo.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#ifdef WITH_VERSE
#include "BIF_verse.h"
#endif

#include "BSE_view.h"
#include "BSE_edit.h"
#include "BSE_trans_types.h"

#include "BDR_drawobject.h"
#include "BDR_editobject.h"
#include "BDR_editface.h"
#include "BDR_vpaint.h"

#include "LBM_fluidsim.h"

#include "multires.h"
#include "mydevice.h"
#include "blendef.h"

/* own include */
#include "editmesh.h"

/* 

editmesh.c:
- add/alloc/free data
- hashtables
- enter/exit editmode

*/


/* ***************** HASH ********************* */


#define EDHASHSIZE		(512*512)
#define EDHASH(a, b)	(a % EDHASHSIZE)


/* ************ ADD / REMOVE / FIND ****************** */

/* used to bypass normal calloc with fast one */
static void *(*callocvert)(size_t, size_t) = calloc;
static void *(*callocedge)(size_t, size_t) = calloc;
static void *(*callocface)(size_t, size_t) = calloc;

EditVert *addvertlist(float *vec, EditVert *example)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	static int hashnr= 0;

	eve= callocvert(sizeof(EditVert), 1);
	BLI_addtail(&em->verts, eve);
	
	if(vec) VECCOPY(eve->co, vec);

	eve->hash= hashnr++;
	if( hashnr>=EDHASHSIZE) hashnr= 0;

	/* new verts get keyindex of -1 since they did not
	 * have a pre-editmode vertex order
	 */
	eve->keyindex = -1;
#ifdef WITH_VERSE
	createVerseVert(eve);
#endif

	if(example) {
		CustomData_em_copy_data(&em->vdata, &em->vdata, example->data, &eve->data);
		eve->bweight = example->bweight;
	}
	else {
		CustomData_em_set_default(&em->vdata, &eve->data);
	}

	return eve;
}

void free_editvert (EditVert *eve)
{
#ifdef WITH_VERSE
	if(eve->vvert) {
		/* it prevents from removing all verse vertexes
		 * during entering edit mode ... messy solution */
		if(G.editMesh->vnode)
			b_verse_send_vertex_delete(eve);
		else
			((VerseVert*)eve->vvert)->vertex = NULL;
	}
#endif

	EM_remove_selection(eve, EDITVERT);
	CustomData_em_free_block(&G.editMesh->vdata, &eve->data);
	if(eve->fast==0)
		free(eve);
}


EditEdge *findedgelist(EditVert *v1, EditVert *v2)
{
	EditVert *v3;
	struct HashEdge *he;

	/* swap ? */
	if( v1 > v2) {
		v3= v2; 
		v2= v1; 
		v1= v3;
	}
	
	if(G.editMesh->hashedgetab==NULL)
		G.editMesh->hashedgetab= MEM_callocN(EDHASHSIZE*sizeof(struct HashEdge), "hashedgetab");

	he= G.editMesh->hashedgetab + EDHASH(v1->hash, v2->hash);
	
	while(he) {
		
		if(he->eed && he->eed->v1==v1 && he->eed->v2==v2) return he->eed;
		
		he= he->next;
	}
	return 0;
}

static void insert_hashedge(EditEdge *eed)
{
	/* assuming that eed is not in the list yet, and that a find has been done before */
	
	struct HashEdge *first, *he;

	first= G.editMesh->hashedgetab + EDHASH(eed->v1->hash, eed->v2->hash);

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

static void remove_hashedge(EditEdge *eed)
{
	/* assuming eed is in the list */
	
	struct HashEdge *first, *he, *prev=NULL;

	he=first= G.editMesh->hashedgetab + EDHASH(eed->v1->hash, eed->v2->hash);

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

EditEdge *addedgelist(EditVert *v1, EditVert *v2, EditEdge *example)
{
	EditMesh *em = G.editMesh;
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
	eed= findedgelist(v1, v2);

	if(eed==NULL) {
	
		eed= (EditEdge *)callocedge(sizeof(EditEdge), 1);
		eed->v1= v1;
		eed->v2= v2;
		BLI_addtail(&em->edges, eed);
		eed->dir= swap;
		insert_hashedge(eed);
		
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

void remedge(EditEdge *eed)
{
	EditMesh *em = G.editMesh;

	BLI_remlink(&em->edges, eed);
	remove_hashedge(eed);
}

void free_editedge(EditEdge *eed)
{
	EM_remove_selection(eed, EDITEDGE);
	if(eed->fast==0){ 
		free(eed);
	}
}

void free_editface(EditFace *efa)
{
#ifdef WITH_VERSE
	if(efa->vface) {
		/* it prevents from removing all verse faces
		 * during entering edit mode ... messy solution */
		if(G.editMesh->vnode)
			b_verse_send_face_delete(efa);
		else
			((VerseFace*)efa->vface)->face = NULL;
	}
#endif
	EM_remove_selection(efa, EDITFACE);
	
	if (G.editMesh->act_face==efa)
		EM_set_actFace(NULL);
		
	CustomData_em_free_block(&G.editMesh->fdata, &efa->data);
	if(efa->fast==0)
		free(efa);
}

void free_vertlist(ListBase *edve) 
{
	EditVert *eve, *next;

	if (!edve) return;

	eve= edve->first;
	while(eve) {
		next= eve->next;
		free_editvert(eve);
		eve= next;
	}
	edve->first= edve->last= NULL;
}

void free_edgelist(ListBase *lb)
{
	EditEdge *eed, *next;
	
	eed= lb->first;
	while(eed) {
		next= eed->next;
		free_editedge(eed);
		eed= next;
	}
	lb->first= lb->last= NULL;
}

void free_facelist(ListBase *lb)
{
	EditFace *efa, *next;
	
	efa= lb->first;
	while(efa) {
		next= efa->next;
		free_editface(efa);
		efa= next;
	}
	lb->first= lb->last= NULL;
}

EditFace *addfacelist(EditVert *v1, EditVert *v2, EditVert *v3, EditVert *v4, EditFace *example, EditFace *exampleEdges)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	EditEdge *e1, *e2=0, *e3=0, *e4=0;

	/* added sanity check... seems to happen for some tools, or for enter editmode for corrupted meshes */
	if(v1==v4 || v2==v4 || v3==v4) v4= NULL;
	
	/* add face to list and do the edges */
	if(exampleEdges) {
		e1= addedgelist(v1, v2, exampleEdges->e1);
		e2= addedgelist(v2, v3, exampleEdges->e2);
		if(v4) e3= addedgelist(v3, v4, exampleEdges->e3); 
		else e3= addedgelist(v3, v1, exampleEdges->e3);
		if(v4) e4= addedgelist(v4, v1, exampleEdges->e4);
	}
	else {
		e1= addedgelist(v1, v2, NULL);
		e2= addedgelist(v2, v3, NULL);
		if(v4) e3= addedgelist(v3, v4, NULL); 
		else e3= addedgelist(v3, v1, NULL);
		if(v4) e4= addedgelist(v4, v1, NULL);
	}
	
	if(v1==v2 || v2==v3 || v1==v3) return NULL;
	if(e2==0) return NULL;

	efa= (EditFace *)callocface(sizeof(EditFace), 1);
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
		if (G.obedit && G.obedit->actcol)
			efa->mat_nr= G.obedit->actcol-1;

		CustomData_em_set_default(&em->fdata, &efa->data);
	}

	BLI_addtail(&em->faces, efa);

	if(efa->v4) {
		CalcNormFloat4(efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co, efa->n);
		CalcCent4f(efa->cent, efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co);
	}
	else {
		CalcNormFloat(efa->v1->co, efa->v2->co, efa->v3->co, efa->n);
		CalcCent3f(efa->cent, efa->v1->co, efa->v2->co, efa->v3->co);
	}

#ifdef WITH_VERSE
	createVerseFace(efa);
#endif

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
static void *calloc_fastvert(size_t size, size_t nr)
{
	EditVert *eve= G.editMesh->curvert++;
	eve->fast= 1;
	return eve;
}
static void *calloc_fastedge(size_t size, size_t nr)
{
	EditEdge *eed= G.editMesh->curedge++;
	eed->fast= 1;
	return eed;
}
static void *calloc_fastface(size_t size, size_t nr)
{
	EditFace *efa= G.editMesh->curface++;
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
	callocvert= calloc;
	callocedge= calloc;
	callocface= calloc;
}

void free_editMesh(EditMesh *em)
{
#ifdef WITH_VERSE
	struct VNode *vnode=NULL;
#endif
	if(em==NULL) return;

#ifdef WITH_VERSE
	if(em->vnode) {
		vnode = (VNode*)em->vnode;
		em->vnode = NULL;
	}
#endif

	if(em->verts.first) free_vertlist(&em->verts);
	if(em->edges.first) free_edgelist(&em->edges);
	if(em->faces.first) free_facelist(&em->faces);
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

#ifdef WITH_VERSE
	if(vnode) {
		em->vnode = (void*)vnode;
	}
#endif

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
	
	mesh_octree_table(NULL, NULL, 'e');
	
	G.totvert= G.totface= 0;

	if(em->retopo_paint_data) retopo_free_paint_data(em->retopo_paint_data);
	em->retopo_paint_data= NULL;
}

/* on G.editMesh */
static void editMesh_set_hash(void)
{
	EditEdge *eed;

	G.editMesh->hashedgetab= NULL;
	
	for(eed=G.editMesh->edges.first; eed; eed= eed->next)  {
		if( findedgelist(eed->v1, eed->v2)==NULL )
			insert_hashedge(eed);
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
		
	if(efa1->v4) CalcCent4f(cent1, efa1->v1->co, efa1->v2->co, efa1->v3->co, efa1->v4->co);
	else CalcCent3f(cent1, efa1->v1->co, efa1->v2->co, efa1->v3->co);
	if(efa2->v4) CalcCent4f(cent2, efa2->v1->co, efa2->v2->co, efa2->v3->co, efa2->v4->co);
	else CalcCent3f(cent2, efa2->v1->co, efa2->v2->co, efa2->v3->co);
	
	VecSubf(cent1, cent2, cent1);
	Normalize(cent1);
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

static void edge_drawflags(void)
{
	EditMesh *em = G.editMesh;
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
	
	recalc_editnormals();
	
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

	if(G.f & G_ALLEDGES) {
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

static int editmesh_pointcache_edit(Object *ob, int totvert, PTCacheID *pid_p, float mat[][4], int load)
{
	Cloth *cloth;
	SoftBody *sb;
	ClothModifierData *clmd;
	PTCacheID pid, tmpid;
	int cfra= (int)G.scene->r.cfra, found= 0;

	pid.cache= NULL;

	/* check for cloth */
	if(modifiers_isClothEnabled(ob)) {
		clmd= (ClothModifierData*)modifiers_findByType(ob, eModifierType_Cloth);
		cloth= clmd->clothObject;
		
		BKE_ptcache_id_from_cloth(&tmpid, ob, clmd);

		/* verify vertex count and baked status */
		if(cloth && (totvert == cloth->numverts)) {
			if((tmpid.cache->flag & PTCACHE_BAKED) && (tmpid.cache->flag & PTCACHE_BAKE_EDIT)) {
				pid= tmpid;

				if(load && (pid.cache->flag & PTCACHE_BAKE_EDIT_ACTIVE))
					found= 1;
			}
		}
	}

	/* check for softbody */
	if(!found && ob->soft) {
		sb= ob->soft;

		BKE_ptcache_id_from_softbody(&tmpid, ob, sb);

		/* verify vertex count and baked status */
		if(sb->bpoint && (totvert == sb->totpoint)) {
			if((tmpid.cache->flag & PTCACHE_BAKED) && (tmpid.cache->flag & PTCACHE_BAKE_EDIT)) {
				pid= tmpid;

				if(load && (pid.cache->flag & PTCACHE_BAKE_EDIT_ACTIVE))
					found= 1;
			}
		}
	}

	/* if not making editmesh verify editing was active for this point cache */
	if(load) {
		if(found)
			pid.cache->flag &= ~PTCACHE_BAKE_EDIT_ACTIVE;
		else
			return 0;
	}

	/* check if we have cache for this frame */
	if(pid.cache && BKE_ptcache_id_exist(&pid, cfra)) {
		*pid_p = pid;
		
		if(load) {
			Mat4CpyMat4(mat, ob->obmat);
		}
		else {
			pid.cache->editframe= cfra;
			pid.cache->flag |= PTCACHE_BAKE_EDIT_ACTIVE;
			Mat4Invert(mat, ob->obmat); /* ob->imat is not up to date */
		}

		return 1;
	}

	return 0;
}

/* turns Mesh into editmesh */
void make_editMesh()
{
	Mesh *me= G.obedit->data;
	EditMesh *em= G.editMesh;
	MFace *mface;
	MVert *mvert;
	MSelect *mselect;
	KeyBlock *actkey;
	EditVert *eve, **evlist, *eve1, *eve2, *eve3, *eve4;
	EditFace *efa;
	EditEdge *eed;
	EditSelection *ese;
	PTCacheID pid;
	Cloth *cloth;
	SoftBody *sb;
	float cacheco[3], cachemat[4][4], *co;
	int tot, a, cacheedit= 0, eekadoodle= 0;

#ifdef WITH_VERSE
	if(me->vnode){
		create_edit_mesh_from_geom_node(me->vnode);
		return;
	}
#endif

	/* because of reload */
	free_editMesh(em);
	
	em->act_face = NULL;
	G.totvert= tot= me->totvert;
	G.totedge= me->totedge;
	G.totface= me->totface;
	
	if(tot==0) {
		countall();
		return;
	}
	
	/* initialize fastmalloc for editmesh */
	init_editmesh_fastmalloc(em, me->totvert, me->totedge, me->totface);

	actkey = ob_get_keyblock(G.obedit);
	if(actkey) {
		strcpy(G.editModeTitleExtra, "(Key) ");
		key_to_mesh(actkey, me);
		tot= actkey->totelem;
	}

	/* make editverts */
	CustomData_copy(&me->vdata, &em->vdata, CD_MASK_EDITMESH, CD_CALLOC, 0);
	mvert= me->mvert;

	cacheedit= editmesh_pointcache_edit(G.obedit, tot, &pid, cachemat, 0);

	evlist= (EditVert **)MEM_mallocN(tot*sizeof(void *),"evlist");
	for(a=0; a<tot; a++, mvert++) {
		
		if(cacheedit) {
			if(pid.type == PTCACHE_TYPE_CLOTH) {
				cloth= ((ClothModifierData*)pid.data)->clothObject;
				VECCOPY(cacheco, cloth->verts[a].x)
			}
			else if(pid.type == PTCACHE_TYPE_SOFTBODY) {
				sb= (SoftBody*)pid.data;
				VECCOPY(cacheco, sb->bpoint[a].pos)
			}

			Mat4MulVecfl(cachemat, cacheco);
			co= cacheco;
		}
		else
			co= mvert->co;

		eve= addvertlist(co, NULL);
		evlist[a]= eve;
		
		// face select sets selection in next loop
		if( (FACESEL_PAINT_TEST)==0 )
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
			eed= addedgelist(evlist[medge->v1], evlist[medge->v2], NULL);
			/* eed can be zero when v1 and v2 are identical, dxf import does this... */
			if(eed) {
				eed->crease= ((float)medge->crease)/255.0f;
				eed->bweight= ((float)medge->bweight)/255.0f;
				
				if(medge->flag & ME_SEAM) eed->seam= 1;
				if(medge->flag & ME_SHARP) eed->sharp = 1;
				if(medge->flag & SELECT) eed->f |= SELECT;
				if(medge->flag & ME_FGON) eed->h= EM_FGON;	// 2 different defines!
				if(medge->flag & ME_HIDE) eed->h |= 1;
				if(G.scene->selectmode==SCE_SELECT_EDGE) 
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
			
			efa= addfacelist(eve1, eve2, eve3, eve4, NULL, NULL);

			if(efa) {
				CustomData_to_em_block(&me->fdata, &em->fdata, a, &efa->data);

				efa->mat_nr= mface->mat_nr;
				efa->flag= mface->flag & ~ME_HIDE;
				
				/* select and hide face flag */
				if(mface->flag & ME_HIDE) {
					efa->h= 1;
				} else {
					if (a==me->act_face) {
						EM_set_actFace(efa);
					}
					
					/* dont allow hidden and selected */
					if(mface->flag & ME_FACE_SEL) {
						efa->f |= SELECT;
						
						if(FACESEL_PAINT_TEST) {
							EM_select_face(efa, 1); /* flush down */
						}
					}
				}
			}
		}
	}
	
	if(eekadoodle)
		error("This Mesh has old style edgecodes, please put it in the bugtracker!");
	
	MEM_freeN(evlist);

	end_editmesh_fastmalloc();	// resets global function pointers
	
	if(me->mselect){
		//restore editselections
		EM_init_index_arrays(1,1,1);
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
	EM_selectmode_set();
	/* paranoia check to enforce hide rules */
	EM_hide_reset();
	/* sets helper flags which arent saved */
	EM_fgon_flags();

	/* vertex coordinates change with cache edit, need to recalc */
	if(cacheedit)
		recalc_editnormals();
	
	countall();
}

/* makes Mesh out of editmesh */
void load_editMesh(void)
{
	EditMesh *em = G.editMesh;
	Mesh *me= G.obedit->data;
	MVert *mvert, *oldverts;
	MEdge *medge;
	MFace *mface;
	MSelect *mselect;
	EditVert *eve;
	EditFace *efa, *efa_act;
	EditEdge *eed;
	EditSelection *ese;
	SoftBody *sb;
	Cloth *cloth;
	ClothModifierData *clmd;
	PTCacheID pid;
	float *fp, *newkey, *oldkey, nor[3], cacheco[3], cachemat[4][4];
	int i, a, ototvert, totedge=0, cacheedit= 0;
	
#ifdef WITH_VERSE
	if(em->vnode) {
		struct VNode *vnode = (VNode*)em->vnode;
		((VGeomData*)vnode->data)->editmesh = NULL;
		em->vnode = NULL;
	}
#endif

	countall();

	/* this one also tests of edges are not in faces: */
	/* eed->f2==0: not in face, f2==1: draw it */
	/* eed->f1 : flag for dynaface (cylindertest, old engine) */
	/* eve->f1 : flag for dynaface (sphere test, old engine) */
	/* eve->f2 : being used in vertexnormals */
	edge_drawflags();
	
	eed= em->edges.first;
	while(eed) {
		totedge++;
		eed= eed->next;
	}
	
	/* new Vertex block */
	if(G.totvert==0) mvert= NULL;
	else mvert= MEM_callocN(G.totvert*sizeof(MVert), "loadeditMesh vert");

	/* new Edge block */
	if(totedge==0) medge= NULL;
	else medge= MEM_callocN(totedge*sizeof(MEdge), "loadeditMesh edge");
	
	/* new Face block */
	if(G.totface==0) mface= NULL;
	else mface= MEM_callocN(G.totface*sizeof(MFace), "loadeditMesh face");

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
	me->totvert= G.totvert;
	me->totedge= totedge;
	me->totface= G.totface;

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

	/* check for point cache editing */
	cacheedit= editmesh_pointcache_edit(G.obedit, G.totvert, &pid, cachemat, 1);

	while(eve) {
		if(cacheedit) {
			if(pid.type == PTCACHE_TYPE_CLOTH) {
				clmd= (ClothModifierData*)pid.data;
				cloth= clmd->clothObject;

				/* assign position */
				VECCOPY(cacheco, cloth->verts[a].x)
				VECCOPY(cloth->verts[a].x, eve->co);
				Mat4MulVecfl(cachemat, cloth->verts[a].x);

				/* find plausible velocity, not physical correct but gives
				 * nicer results when commented */
				VECSUB(cacheco, cloth->verts[a].x, cacheco);
				VecMulf(cacheco, clmd->sim_parms->stepsPerFrame*10.0f);
				VECADD(cloth->verts[a].v, cloth->verts[a].v, cacheco);
			}
			else if(pid.type == PTCACHE_TYPE_SOFTBODY) {
				sb= (SoftBody*)pid.data;

				/* assign position */
				VECCOPY(cacheco, sb->bpoint[a].pos)
				VECCOPY(sb->bpoint[a].pos, eve->co);
				Mat4MulVecfl(cachemat, sb->bpoint[a].pos);

				/* changing velocity for softbody doesn't seem to give
				 * good results? */
#if 0
				VECSUB(cacheco, sb->bpoint[a].pos, cacheco);
				VecMulf(cacheco, sb->minloops*10.0f);
				VECADD(sb->bpoint[a].vec, sb->bpoint[a].pos, cacheco);
#endif
			}

			if(oldverts)
				VECCOPY(mvert->co, oldverts[a].co)
		}
		else
			VECCOPY(mvert->co, eve->co);

		mvert->mat_nr= 255;  /* what was this for, halos? */
		
		/* vertex normal */
		VECCOPY(nor, eve->no);
		VecMulf(nor, 32767.0);
		VECCOPY(mvert->no, nor);

		/* note: it used to remove me->dvert when it was not in use, cancelled
		   that... annoying when you have a fresh vgroup */
		CustomData_from_em_block(&em->vdata, &me->vdata, eve->data, a);

		eve->tmp.l = a++;  /* counter */
			
		mvert->flag= 0;
		if(eve->f1==1) mvert->flag |= ME_SPHERETEST;
		mvert->flag |= (eve->f & SELECT);
		if (eve->h) mvert->flag |= ME_HIDE;
		mvert->bweight= (char)(255.0*eve->bweight);

#ifdef WITH_VERSE
		if(eve->vvert) {
			((VerseVert*)eve->vvert)->vertex = NULL;
			eve->vvert = NULL;
		}
#endif			
		eve= eve->next;
		mvert++;
	}
	
	/* write changes to cache */
	if(cacheedit) {
		if(pid.type == PTCACHE_TYPE_CLOTH)
			cloth_write_cache(G.obedit, pid.data, pid.cache->editframe);
		else if(pid.type == PTCACHE_TYPE_SOFTBODY)
			sbWriteCache(G.obedit, pid.cache->editframe);
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
	efa_act= EM_get_actFace(0);
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
			if(mvert->mat_nr == (char)255) mvert->mat_nr= mface->mat_nr;
			mvert= me->mvert+mface->v2;
			if(mvert->mat_nr == (char)255) mvert->mat_nr= mface->mat_nr;
			mvert= me->mvert+mface->v3;
			if(mvert->mat_nr == (char)255) mvert->mat_nr= mface->mat_nr;
			if(mface->v4) {
				mvert= me->mvert+mface->v4;
				if(mvert->mat_nr == (char)255) mvert->mat_nr= mface->mat_nr;
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

#ifdef WITH_VERSE
		if(efa->vface) {
			((VerseFace*)efa->vface)->face = NULL;
			efa->vface = NULL;
		}
#endif		
		efa->tmp.l = a++;
		i++;
		efa= efa->next;
	}

	/* patch hook indices and vertex parents */
	{
		Object *ob;
		ModifierData *md;
		EditVert *eve, **vertMap = NULL;
		int i,j;

		for (ob=G.main->object.first; ob; ob=ob->id.next) {
			if (ob->parent==G.obedit && ELEM(ob->partype, PARVERT1,PARVERT3)) {
				
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
		KeyBlock *currkey, *actkey = ob_get_keyblock(G.obedit);

		/* Lets reorder the key data so that things line up roughly
		 * with the way things were before editmode */
		currkey = me->key->block.first;
		while(currkey) {
			
			fp= newkey= MEM_callocN(me->key->elemsize*G.totvert,  "currkey->data");
			oldkey = currkey->data;

			eve= em->verts.first;

			i = 0;
			mvert = me->mvert;
			while(eve) {
				if (eve->keyindex >= 0 && eve->keyindex < currkey->totelem) { // valid old vertex
					if(currkey == actkey) {
						if (actkey == me->key->refkey) {
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
			currkey->totelem= G.totvert;
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
		for(base= G.scene->base.first; base; base= base->next)
			if(base->object->data==me)
				base->object->recalc |= OB_RECALC_DATA;
	}

	mesh_calc_normals(me->mvert, me->totvert, me->mface, me->totface, NULL);
}

void remake_editMesh(void)
{
	make_editMesh();
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0); /* needed to have nice cloth panels */
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	BIF_undo_push("Undo all changes");
}

/* ***************		(partial exit editmode) *************/


void separatemenu(void)
{
	short event;

	if(G.editMesh->verts.first==NULL) return;
	   
	event = pupmenu("Separate %t|Selected%x1|All Loose Parts%x2|By Material%x3");
	
	if (event==0) return;
	waitcursor(1);
	
	switch (event) {
	case 1: 
		separate_mesh();		    
		break;
	case 2:	    	    	    
		separate_mesh_loose();	    	    
		break;
	case 3:
		separate_material();
		break;
	}
	waitcursor(0);
}

void separate_material(void)
{
	EditMesh *em = G.editMesh;
	unsigned char curr_mat;
	Mesh *me;
	
	if(multires_test()) return;
	
	me= get_mesh(G.obedit);
	if(me->key) {
		error("Can't separate with vertex keys");
		return;
	}

	if(G.obedit && em) {
		if(G.obedit->type == OB_MESH) {
			for (curr_mat = 1; curr_mat < G.obedit->totcol; ++curr_mat) {
				/* clear selection, we're going to use that to select material group */
				EM_clear_flag_all(SELECT);
				/* select the material */
				editmesh_select_by_material(curr_mat);
				/* and now separate */
				separate_mesh();
			}
		}
	}
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

}

void separate_mesh(void)
{
	EditMesh *em = G.editMesh;
	EditMesh emcopy;
	EditVert *eve, *v1;
	EditEdge *eed, *e1;
	EditFace *efa, *vl1;
	Object *oldob;
	Mesh *me, *men;
	Base *base, *oldbase;
	ListBase edve, eded, edvl;
#ifdef WITH_VERSE
	struct VNode *vnode = NULL;
#endif
	
	TEST_EDITMESH
	if(multires_test()) return;

	waitcursor(1);
	
	me= get_mesh(G.obedit);
	if(me->key) {
		error("Can't separate with vertex keys");
		return;
	}
	
	/* blender crashes in derivedmesh drawing if I don't do this... but why? 
		Anyhoo, this function is horrible anyway (ton) 
		the fluidsimFlag also has to be reset btw. (n_t) */
	if(G.obedit->fluidsimSettings) {
		fluidsimSettingsFree(G.obedit->fluidsimSettings);
		G.obedit->fluidsimSettings = NULL;
		G.obedit->fluidsimFlag = 0;
	}
	
	if(em->selected.first) BLI_freelistN(&(em->selected)); /* clear the selection order */
		
	EM_selectmode_set();	// enforce full consistant selection flags 
	
	/* we are going to abuse the system as follows:
	 * 1. add a duplicate object: this will be the new one, we remember old pointer
	 * 2: then do a split if needed.
	 * 3. put apart: all NOT selected verts, edges, faces
	 * 4. call load_editMesh(): this will be the new object
	 * 5. freelist and get back old verts, edges, facs
	 */
	
	/* make only obedit selected */
	base= FIRSTBASE;
	while(base) {
		if(base->lay & G.vd->lay) {
			if(base->object==G.obedit) base->flag |= SELECT;
			else base->flag &= ~SELECT;
		}
		base= base->next;
	}
	
#ifdef WITH_VERSE
	if(G.editMesh->vnode) {
		vnode = G.editMesh->vnode;
		G.editMesh->vnode = NULL;
	}
#endif
	/* no test for split, split doesn't split when a loose part is selected */
	/* SPLIT: first make duplicate */
	adduplicateflag(SELECT);

#ifdef WITH_VERSE
	if(vnode) {
		G.editMesh->vnode = vnode;
	}
#endif
	/* SPLIT: old faces have 3x flag 128 set, delete these ones */
	delfaceflag(128);
	
	/* since we do tricky things with verts/edges/faces, this makes sure all is selected coherent */
	EM_selectmode_set();
	
	/* set apart: everything that is not selected */
	edve.first= edve.last= eded.first= eded.last= edvl.first= edvl.last= 0;
	eve= em->verts.first;
	while(eve) {
		v1= eve->next;
		if((eve->f & SELECT)==0) {
			BLI_remlink(&em->verts, eve);
			BLI_addtail(&edve, eve);
#ifdef WITH_VERSE
			if(eve->vvert) {
				((VerseVert*)eve->vvert)->vertex = NULL;
				eve->vvert = NULL;
			}
#endif
		}
		
		eve= v1;
	}
	eed= em->edges.first;
	while(eed) {
		e1= eed->next;
		if((eed->f & SELECT)==0) {
			BLI_remlink(&em->edges, eed);
			BLI_addtail(&eded, eed);
		}
		eed= e1;
	}
	efa= em->faces.first;
	while(efa) {
		vl1= efa->next;
		if((efa->f & SELECT)==0) {
			BLI_remlink(&em->faces, efa);
			BLI_addtail(&edvl, efa);
#ifdef WITH_VERSE
			if(efa->vface) {
				((VerseFace*)efa->vface)->face = NULL;
				efa->vface = NULL;
			}
#endif
		}
		efa= vl1;
	}
	
	oldob= G.obedit;
	oldbase= BASACT;
	
#ifdef WITH_VERSE
	if(G.obedit->vnode) {
		vnode = G.obedit->vnode;
		G.obedit->vnode = NULL;
	}
#endif
	adduplicate(1, 0); /* notrans and a linked duplicate */
	
#ifdef WITH_VERSE
	if(vnode) {
		G.obedit->vnode = vnode;
	}
#endif
	
	G.obedit= BASACT->object;	/* basact was set in adduplicate()  */
	
	men= copy_mesh(me);
	set_mesh(G.obedit, men);
	/* because new mesh is a copy: reduce user count */
	men->id.us--;
	
	load_editMesh();
	
	BASACT->flag &= ~SELECT;
	
	/* we cannot free the original buffer... */
	emcopy= *G.editMesh;
	emcopy.allverts= NULL;
	emcopy.alledges= NULL;
	emcopy.allfaces= NULL;
	emcopy.derivedFinal= emcopy.derivedCage= NULL;
	memset(&emcopy.vdata, 0, sizeof(emcopy.vdata));
	memset(&emcopy.fdata, 0, sizeof(emcopy.fdata));
	free_editMesh(&emcopy);
	
	em->verts= edve;
	em->edges= eded;
	em->faces= edvl;
	
	/* hashedges are freed now, make new! */
	editMesh_set_hash();

	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
	G.obedit= oldob;
	BASACT= oldbase;
	BASACT->flag |= SELECT;
	
	waitcursor(0);

	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	

}

void separate_mesh_loose(void)
{
	EditMesh *em = G.editMesh;
	EditMesh emcopy;
	EditVert *eve, *v1;
	EditEdge *eed, *e1;
	EditFace *efa, *vl1;
	Object *oldob=NULL;
	Mesh *me, *men;
	Base *base, *oldbase;
	ListBase edve, eded, edvl;
	int vertsep=0;	
	short done=0, check=1;
#ifdef WITH_VERSE
	struct VNode *vnode = NULL;
#endif
			
	me= get_mesh(G.obedit);
#ifdef WITH_VERSE
	if(me->vnode) {
		error("Can't separate a mesh shared at verse server");
		return;
	}
#endif
	if(me->key) {
		error("Can't separate a mesh with vertex keys");
		return;
	}
	
	/* same problem as in separate_mesh above (n_t) */
	if(G.obedit->fluidsimSettings) {
		fluidsimSettingsFree(G.obedit->fluidsimSettings);
		G.obedit->fluidsimSettings = NULL;
		G.obedit->fluidsimFlag = 0;
	}

	TEST_EDITMESH
	if(multires_test()) return;
	waitcursor(1);	
	
	/* we are going to abuse the system as follows:
	 * 1. add a duplicate object: this will be the new one, we remember old pointer
	 * 2: then do a split if needed.
	 * 3. put apart: all NOT selected verts, edges, faces
	 * 4. call load_editMesh(): this will be the new object
	 * 5. freelist and get back old verts, edges, facs
	 */
			
	while(!done){		
		vertsep=check=1;
		
		countall();
		
		/* make only obedit selected */
		base= FIRSTBASE;
		while(base) {
			if(base->lay & G.vd->lay) {
				if(base->object==G.obedit) base->flag |= SELECT;
				else base->flag &= ~SELECT;
			}
			base= base->next;
		}		
		
		/*--------- Select connected-----------*/		
		
		EM_clear_flag_all(SELECT);

		/* Select a random vert to start with */
		eve= em->verts.first;
		eve->f |= SELECT;
		
		while(check==1) {
			check= 0;			
			eed= em->edges.first;			
			while(eed) {				
				if(eed->h==0) {
					if(eed->v1->f & SELECT) {
						if( (eed->v2->f & SELECT)==0 ) {
							eed->v2->f |= SELECT;
							vertsep++;
							check= 1;
						}
					}
					else if(eed->v2->f & SELECT) {
						if( (eed->v1->f & SELECT)==0 ) {
							eed->v1->f |= SELECT;
							vertsep++;
							check= SELECT;
						}
					}
				}
				eed= eed->next;				
			}
		}		
		/*----------End of select connected--------*/
		
		
		/* If the amount of vertices that is about to be split == the total amount 
		   of verts in the mesh, it means that there is only 1 unconnected object, so we don't have to separate
		*/
		if(G.totvert==vertsep) done=1;				
		else{			
			/* No splitting: select connected goes fine */
			
			EM_select_flush();	// from verts->edges->faces

			/* set apart: everything that is not selected */
			edve.first= edve.last= eded.first= eded.last= edvl.first= edvl.last= 0;
			eve= em->verts.first;
			while(eve) {
				v1= eve->next;
				if((eve->f & SELECT)==0) {
					BLI_remlink(&em->verts, eve);
					BLI_addtail(&edve, eve);
#ifdef WITH_VERSE
					if(eve->vvert) {
						b_verse_send_vertex_delete(eve);
					}
#endif
				}
				eve= v1;
			}
			eed= em->edges.first;
			while(eed) {
				e1= eed->next;
				if( (eed->f & SELECT)==0 ) {
					BLI_remlink(&em->edges, eed);
					BLI_addtail(&eded, eed);
				}
				eed= e1;
			}
			efa= em->faces.first;
			while(efa) {
				vl1= efa->next;
				if( (efa->f & SELECT)==0 ) {
					BLI_remlink(&em->faces, efa);
					BLI_addtail(&edvl, efa);
#ifdef WITH_VERSE
					if(efa->vface) {
						b_verse_send_face_delete(efa);
					}
#endif
				}
				efa= vl1;
			}
			
			oldob= G.obedit;
			oldbase= BASACT;
			
#ifdef WITH_VERSE
			if(G.obedit->vnode) {
				vnode = G.obedit->vnode;
				G.obedit->vnode = NULL;
			}
#endif
			adduplicate(1, 0); /* notrans and a linked duplicate*/
#ifdef WITH_VERSE
			if(vnode) {
				G.obedit->vnode = vnode;
			}
#endif
			
			G.obedit= BASACT->object;	/* basact was set in adduplicate()  */

			men= copy_mesh(me);
			set_mesh(G.obedit, men);
			/* because new mesh is a copy: reduce user count */
			men->id.us--;
			
			load_editMesh();
			
			BASACT->flag &= ~SELECT;
			
			/* we cannot free the original buffer... */
			emcopy= *G.editMesh;
			emcopy.allverts= NULL;
			emcopy.alledges= NULL;
			emcopy.allfaces= NULL;
			emcopy.derivedFinal= emcopy.derivedCage= NULL;
			memset(&emcopy.vdata, 0, sizeof(emcopy.vdata));
			memset(&emcopy.fdata, 0, sizeof(emcopy.fdata));
			free_editMesh(&emcopy);
			
			em->verts= edve;
			em->edges= eded;
			em->faces= edvl;
			
			/* hashedges are freed now, make new! */
			editMesh_set_hash();
			
			G.obedit= oldob;
			BASACT= oldbase;
			BASACT->flag |= SELECT;	
					
		}		
	}
	
	/* unselect the vertices that we (ab)used for the separation*/
	EM_clear_flag_all(SELECT);
		
	waitcursor(0);
	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
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
	unsigned char mat_nr, flag, f, h, fgonf;
	short pad1;
} EditFaceC;

typedef struct EditSelectionC{
	short type;
	int index;
}EditSelectionC;

typedef struct EM_MultiresUndo {
	int users;
	Multires *mr;
} EM_MultiresUndo;

typedef struct UndoMesh {
	EditVertC *verts;
	EditEdgeC *edges;
	EditFaceC *faces;
	EditSelectionC *selected;
	int totvert, totedge, totface, totsel;
	short selectmode;
	RetopoPaintData *retopo_paint_data;
	char retopo_mode;
	CustomData vdata, edata, fdata;
	EM_MultiresUndo *mru;
} UndoMesh;

/* for callbacks */

static void free_undoMesh(void *umv)
{
	UndoMesh *um= umv;
	
	if(um->verts) MEM_freeN(um->verts);
	if(um->edges) MEM_freeN(um->edges);
	if(um->faces) MEM_freeN(um->faces);
	if(um->selected) MEM_freeN(um->selected);
	if(um->retopo_paint_data) retopo_free_paint_data(um->retopo_paint_data);
	CustomData_free(&um->vdata, um->totvert);
	CustomData_free(&um->edata, um->totedge);
	CustomData_free(&um->fdata, um->totface);
	if(um->mru) {
		--um->mru->users;
		if(um->mru->users==0) {
			multires_free(um->mru->mr);
			um->mru->mr= NULL;
			MEM_freeN(um->mru);
		}
	}
	MEM_freeN(um);
}

static void *editMesh_to_undoMesh(void)
{
	EditMesh *em= G.editMesh;
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
	
	um->selectmode = G.scene->selectmode;
	
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

	um->retopo_paint_data= retopo_paint_data_copy(em->retopo_paint_data);
	um->retopo_mode= G.scene->toolsettings->retopo_mode;
	
	{
		Multires *mr= get_mesh(G.obedit)->mr;
		UndoMesh *prev= undo_editmode_get_prev(G.obedit);
		
		um->mru= NULL;
		
		if(mr) {
			if(prev && prev->mru && prev->mru->mr && prev->mru->mr->current == mr->current) {
				um->mru= prev->mru;
				++um->mru->users;
			}
			else {
				um->mru= MEM_callocN(sizeof(EM_MultiresUndo), "EM_MultiresUndo");
				um->mru->users= 1;
				um->mru->mr= multires_copy(mr);
			}
		}
	}
	
	return um;
}

static void undoMesh_to_editMesh(void *umv)
{
	UndoMesh *um= (UndoMesh*)umv;
	EditMesh *em= G.editMesh;
	EditVert *eve, **evar=NULL;
	EditEdge *eed;
	EditFace *efa;
	EditSelection *ese;
	EditVertC *evec;
	EditEdgeC *eedc;
	EditFaceC *efac;
	EditSelectionC *esec;
	int a=0;

#ifdef WITH_VERSE
	struct VNode *vnode = G.editMesh->vnode;
	if(vnode) {
		/* send delete command to all verse vertexes and verse face ...
		 * verse mesh will be recreated from new edit mesh */
		destroy_versemesh(vnode);
	}
#endif	
	G.scene->selectmode = um->selectmode;
	
	free_editMesh(G.editMesh);
	
	/* malloc blocks */
	memset(em, 0, sizeof(EditMesh));
		
	init_editmesh_fastmalloc(em, um->totvert, um->totedge, um->totface);

#ifdef WITH_VERSE
	G.editMesh->vnode = vnode;
#endif

	CustomData_free(&em->vdata, 0);
	CustomData_free(&em->edata, 0);
	CustomData_free(&em->fdata, 0);

	CustomData_copy(&um->vdata, &em->vdata, CD_MASK_EDITMESH, CD_CALLOC, 0);
	CustomData_copy(&um->edata, &em->edata, CD_MASK_EDITMESH, CD_CALLOC, 0);
	CustomData_copy(&um->fdata, &em->fdata, CD_MASK_EDITMESH, CD_CALLOC, 0);

	/* now copy vertices */

	if(um->totvert) evar= MEM_mallocN(um->totvert*sizeof(EditVert *), "vertex ar");
	for(a=0, evec= um->verts; a<um->totvert; a++, evec++) {
		eve= addvertlist(evec->co, NULL);
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
		eed= addedgelist(evar[eedc->v1], evar[eedc->v2], NULL);

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
			efa= addfacelist(evar[efac->v1], evar[efac->v2], evar[efac->v3], evar[efac->v4], NULL, NULL);
		else 
			efa= addfacelist(evar[efac->v1], evar[efac->v2], evar[efac->v3], NULL, NULL ,NULL);

		efa->mat_nr= efac->mat_nr;
		efa->flag= efac->flag;
		efa->f= efac->f;
		efa->h= efac->h;
		efa->fgonf= efac->fgonf;
		
		CustomData_to_em_block(&um->fdata, &em->fdata, a, &efa->data);
	}
	
	end_editmesh_fastmalloc();
	if(evar) MEM_freeN(evar);
	
	G.totvert = um->totvert;
	G.totedge = um->totedge;
	G.totface = um->totface;
	/*restore stored editselections*/
	if(um->totsel){
		EM_init_index_arrays(1,1,1);
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

	retopo_free_paint();
	em->retopo_paint_data= retopo_paint_data_copy(um->retopo_paint_data);
	G.scene->toolsettings->retopo_mode= um->retopo_mode;
	if(G.scene->toolsettings->retopo_mode) {
		if(G.vd->depths) G.vd->depths->damaged= 1;
		retopo_queue_updates(G.vd);
		retopo_paint_view_update(G.vd);
	}
	
	{
		Mesh *me= get_mesh(G.obedit);
		multires_free(me->mr);
		me->mr= NULL;
		if(um->mru && um->mru->mr) me->mr= multires_copy(um->mru->mr);
	}
}


/* and this is all the undo system needs to know */
void undo_push_mesh(char *name)
{
	undo_editmode_push(name, free_undoMesh, undoMesh_to_editMesh, editMesh_to_undoMesh, NULL);
}



/* *************** END UNDO *************/

static EditVert **g_em_vert_array = NULL;
static EditEdge **g_em_edge_array = NULL;
static EditFace **g_em_face_array = NULL;

void EM_init_index_arrays(int forVert, int forEdge, int forFace)
{
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	int i;

	if (forVert) {
		g_em_vert_array = MEM_mallocN(sizeof(*g_em_vert_array)*G.totvert, "em_v_arr");

		for (i=0,eve=G.editMesh->verts.first; eve; i++,eve=eve->next)
			g_em_vert_array[i] = eve;
	}

	if (forEdge) {
		g_em_edge_array = MEM_mallocN(sizeof(*g_em_edge_array)*G.totedge, "em_e_arr");

		for (i=0,eed=G.editMesh->edges.first; eed; i++,eed=eed->next)
			g_em_edge_array[i] = eed;
	}

	if (forFace) {
		g_em_face_array = MEM_mallocN(sizeof(*g_em_face_array)*G.totface, "em_f_arr");

		for (i=0,efa=G.editMesh->faces.first; efa; i++,efa=efa->next)
			g_em_face_array[i] = efa;
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
int EM_texFaceCheck(void)
{
	/* some of these checks could be a touch overkill */
	if (	(G.obedit) &&
			(G.obedit->type == OB_MESH) &&
			(G.editMesh) &&
			(G.editMesh->faces.first) &&
			(CustomData_has_layer(&G.editMesh->fdata, CD_MTFACE)))
		return 1;
	return 0;
}

/* can we edit colors for this mesh?*/
int EM_vertColorCheck(void)
{
	/* some of these checks could be a touch overkill */
	if (	(G.obedit) &&
			(G.obedit->type == OB_MESH) &&
			(G.editMesh) &&
			(G.editMesh->faces.first) &&
			(CustomData_has_layer(&G.editMesh->fdata, CD_MCOL)))
		return 1;
	return 0;
}

