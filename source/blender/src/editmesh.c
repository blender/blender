/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */


#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef WIN32
#include "BLI_winstuff.h"
#endif
#include "MEM_guardedalloc.h"

#include "PIL_time.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_key_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_dynstr.h"

#include "BKE_utildefines.h"
#include "BKE_key.h"
#include "BKE_object.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_texture.h"

#include "BIF_editkey.h"
#include "BIF_editmesh.h"
#include "BIF_editmode_undo.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_space.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "BSE_view.h"
#include "BSE_edit.h"
#include "BSE_trans_types.h"

#include "BDR_drawobject.h"
#include "BDR_editobject.h"
#include "BDR_editface.h"
#include "BDR_vpaint.h"

#include "mydevice.h"
#include "blendef.h"
#include "render.h"

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

EditVert *addvertlist(float *vec)
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
	return eve;
}

void free_editvert (EditVert *eve)
{
	if(eve->dw) MEM_freeN(eve->dw);
	if(eve->fast==0) free(eve);
}


EditEdge *findedgelist(EditVert *v1, EditVert *v2)
{
	EditVert *v3;
	struct HashEdge *he;

	/* swap ? */
	if( (long)v1 > (long)v2) {
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
			eed->seam = example->seam;
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
	if(eed->fast==0) free(eed);
}

void free_editface(EditFace *efa)
{
	if(efa->fast==0) free(efa);
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

EditFace *addfacelist(EditVert *v1, EditVert *v2, EditVert *v3, EditVert *v4, EditFace *example)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	EditEdge *e1, *e2=0, *e3=0, *e4=0;

	/* add face to list and do the edges */
	e1= addedgelist(v1, v2, NULL);
	e2= addedgelist(v2, v3, NULL);
	if(v4) e3= addedgelist(v3, v4, NULL); 
	else e3= addedgelist(v3, v1, NULL);
	if(v4) e4= addedgelist(v4, v1, NULL);

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
		efa->tf= example->tf;
		efa->flag= example->flag;
	}
	else {
		if (G.obedit && G.obedit->actcol)
			efa->mat_nr= G.obedit->actcol-1;
		default_uv(efa->tf.uv, 1.0);

		/* Initialize colors */
		efa->tf.col[0]= efa->tf.col[1]= efa->tf.col[2]= efa->tf.col[3]= vpaint_get_current_col();
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

	return efa;
}

/* ************************ end add/new/find ************  */

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
	if(em==NULL) return;
	
	if(em->verts.first) free_vertlist(&em->verts);
	if(em->edges.first) free_edgelist(&em->edges);
	if(em->faces.first) free_facelist(&em->faces);

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
	
	G.totvert= G.totface= 0;
}

/* on G.editMesh */
static void editMesh_set_hash(void)
{
	EditEdge *eed;

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
	
	efa2= (EditFace *)eed->vn;
	if(efa1==efa2) return;
	
	inp= efa1->n[0]*efa2->n[0] + efa1->n[1]*efa2->n[1] + efa1->n[2]*efa2->n[2];
	if(inp<0.999 && inp >-0.999) eed->f2= 1;
		
	if(efa1->v4) CalcCent4f(cent1, efa1->v1->co, efa1->v2->co, efa1->v3->co, efa1->v4->co);
	else CalcCent3f(cent1, efa1->v1->co, efa1->v2->co, efa1->v3->co);
	if(efa2->v4) CalcCent4f(cent2, efa2->v1->co, efa2->v2->co, efa2->v3->co, efa2->v4->co);
	else CalcCent3f(cent2, efa2->v1->co, efa2->v2->co, efa2->v3->co);
	
	VecSubf(cent1, cent2, cent1);
	Normalise(cent1);
	inp= cent1[0]*efa1->n[0] + cent1[1]*efa1->n[1] + cent1[2]*efa1->n[2]; 

	if(inp < -0.001 ) eed->f1= 1;
}

static void edge_drawflags(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	EditEdge *eed, *e1, *e2, *e3, *e4;
	EditFace *efa;
	
	/* - count number of times edges are used in faces: 0 en 1 time means draw edge
	 * - edges more than 1 time used: in *vn is pointer to first face
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
		eed->vn= 0;
		eed= eed->next;
	}

	efa= em->faces.first;
	while(efa) {
		e1= efa->e1;
		e2= efa->e2;
		e3= efa->e3;
		e4= efa->e4;
		if(e1->f2<3) e1->f2+= 1;
		if(e2->f2<3) e2->f2+= 1;
		if(e3->f2<3) e3->f2+= 1;
		if(e4 && e4->f<3) e4->f2+= 1;
		
		if(e1->vn==0) e1->vn= (EditVert *)efa;
		if(e2->vn==0) e2->vn= (EditVert *)efa;
		if(e3->vn==0) e3->vn= (EditVert *)efa;
		if(e4 && e4->vn==0) e4->vn= (EditVert *)efa;
		
		efa= efa->next;
	}

	if(G.f & G_ALLEDGES) {
		efa= em->faces.first;
		while(efa) {
			if(efa->e1->f2>=2) efa->e1->f= 1;
			if(efa->e2->f2>=2) efa->e2->f= 1;
			if(efa->e3->f2>=2) efa->e3->f= 1;
			if(efa->e4 && efa->e4->f>=2) efa->e4->f= 1;
			
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
			if(efa->e2->f2==2) edge_normal_compare(efa->e2, efa);
			if(efa->e3->f2==2) edge_normal_compare(efa->e3, efa);
			if(efa->e4 && efa->e4->f2==2) edge_normal_compare(efa->e4, efa);
			
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
void make_editMesh()
{
	EditMesh *em = G.editMesh;
	Mesh *me= G.obedit->data;
	MFace *mface;
	TFace *tface;
	MVert *mvert;
	KeyBlock *actkey=0;
	EditVert *eve, **evlist, *eve1, *eve2, *eve3, *eve4;
	EditFace *efa;
	EditEdge *eed;
	int tot, a;

	if(G.obedit==NULL) return;

	/* because of reload */
	free_editMesh(G.editMesh);
	
	G.totvert= tot= me->totvert;

	if(tot==0) {
		countall();
		return;
	}
	
	waitcursor(1);

	/* initialize fastmalloc for editmesh */
	init_editmesh_fastmalloc(G.editMesh, me->totvert, me->totedge, me->totface);

	/* keys? */
	if(me->key) {
		actkey= me->key->block.first;
		while(actkey) {
			if(actkey->flag & SELECT) break;
			actkey= actkey->next;
		}
	}

	if(actkey) {
		key_to_mesh(actkey, me);
		tot= actkey->totelem;
	}

	/* make editverts */
	mvert= me->mvert;

	evlist= (EditVert **)MEM_mallocN(tot*sizeof(void *),"evlist");
	for(a=0; a<tot; a++, mvert++) {
		eve= addvertlist(mvert->co);
		evlist[a]= eve;
		
		// face select sets selection in next loop
		if( (G.f & G_FACESELECT)==0 )
			eve->f |= (mvert->flag & 1);
		
		if (mvert->flag & ME_HIDE) eve->h= 1;		
		eve->no[0]= mvert->no[0]/32767.0;
		eve->no[1]= mvert->no[1]/32767.0;
		eve->no[2]= mvert->no[2]/32767.0;

		/* lets overwrite the keyindex of the editvert
		 * with the order it used to be in before
		 * editmode
		 */
		eve->keyindex = a;

		if (me->dvert){
			eve->totweight = me->dvert[a].totweight;
			if (me->dvert[a].dw){
				eve->dw = MEM_callocN (sizeof(MDeformWeight) * me->dvert[a].totweight, "deformWeight");
				memcpy (eve->dw, me->dvert[a].dw, sizeof(MDeformWeight) * me->dvert[a].totweight);
			}
		}

	}

	if(actkey && actkey->totelem!=me->totvert);
	else {
		unsigned int *mcol;
		
		/* make edges */
		if(me->medge) {
			MEdge *medge= me->medge;
			
			for(a=0; a<me->totedge; a++, medge++) {
				eed= addedgelist(evlist[medge->v1], evlist[medge->v2], NULL);
				eed->crease= ((float)medge->crease)/255.0;
				
				if(medge->flag & ME_SEAM) eed->seam= 1;
				if(medge->flag & SELECT) eed->f |= SELECT;
			}

		}
		
		/* make faces */
		mface= me->mface;
		tface= me->tface;
		mcol= (unsigned int *)me->mcol;
		
		for(a=0; a<me->totface; a++, mface++) {
			eve1= evlist[mface->v1];
			eve2= evlist[mface->v2];
			if(mface->v3) eve3= evlist[mface->v3]; else eve3= NULL;
			if(mface->v4) eve4= evlist[mface->v4]; else eve4= NULL;
			
			efa= addfacelist(eve1, eve2, eve3, eve4, NULL);

			if(efa) {
			
				if(mcol) memcpy(efa->tf.col, mcol, 4*sizeof(int));

				if(me->tface) {
					efa->tf= *tface;

					if( tface->flag & TF_SELECT) {
						if(G.f & G_FACESELECT) {
							EM_select_face(efa, 1);
						}
					}
				}
			
				efa->mat_nr= mface->mat_nr;
				efa->flag= mface->flag;
				
				/* select face flag, if no edges we flush down */
				if(mface->flag & ME_FACE_SEL) {
					efa->f |= SELECT;
					if(me->medge==NULL) EM_select_face(efa, 1);
				}
			}

			if(me->tface) tface++;
			if(mcol) mcol+=4;
		}
	}
	
	/* flush hide flags */
	
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->v1->h || eed->v2->h) eed->h= 1;
		else eed->h= 0;
	}	
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->e1->h || efa->e2->h || efa->e3->h) efa->h= 1;
		else if(efa->e4 && efa->e4->h) efa->h= 1;
	}
	
	MEM_freeN(evlist);

	end_editmesh_fastmalloc();	// resets global function pointers

	if (mesh_uses_displist(me))
		makeDispList(G.obedit);

	/* this creates coherent selections. also needed for older files */
	EM_selectmode_set();

	countall();
	
	waitcursor(0);
}

/** Rotates MFace and UVFace vertices in case the last
  * vertex index is = 0. 
  * This function is a hack and may only be called in the
  * conversion from EditMesh to Mesh data.
  * This function is similar to test_index_mface in
  * blenkernel/intern/mesh.c. 
  * To not clutter the blenkernel code with more bad level
  * calls/structures, this function resides here.
  */

static void fix_faceindices(MFace *mface, EditFace *efa, int nr)
{
	int a;
	float tmpuv[2];
	unsigned int tmpcol;

	/* first test if the face is legal */

	if(mface->v3 && mface->v3==mface->v4) {
		mface->v4= 0;
		nr--;
	}
	if(mface->v2 && mface->v2==mface->v3) {
		mface->v3= mface->v4;
		mface->v4= 0;
		nr--;
	}
	if(mface->v1==mface->v2) {
		mface->v2= mface->v3;
		mface->v3= mface->v4;
		mface->v4= 0;
		nr--;
	}

	/* prevent a zero index value at the wrong location */
	if(nr==2) {
		if(mface->v2==0) SWAP(int, mface->v1, mface->v2);
	}
	else if(nr==3) {
		if(mface->v3==0) {
			SWAP(int, mface->v1, mface->v2);
			SWAP(int, mface->v2, mface->v3);
			/* rotate face UV coordinates, too */
			UVCOPY(tmpuv, efa->tf.uv[0]);
			UVCOPY(efa->tf.uv[0], efa->tf.uv[1]);
			UVCOPY(efa->tf.uv[1], efa->tf.uv[2]);
			UVCOPY(efa->tf.uv[2], tmpuv);
			/* same with vertex colours */
			tmpcol = efa->tf.col[0];
			efa->tf.col[0] = efa->tf.col[1];
			efa->tf.col[1] = efa->tf.col[2];
			efa->tf.col[2] = tmpcol;

			
			a= mface->edcode;
			mface->edcode= 0;
			if(a & ME_V1V2) mface->edcode |= ME_V3V1;
			if(a & ME_V2V3) mface->edcode |= ME_V1V2;
			if(a & ME_V3V1) mface->edcode |= ME_V2V3;
			
			a= mface->puno;
			mface->puno &= ~15;
			if(a & ME_FLIPV1) mface->puno |= ME_FLIPV2;
			if(a & ME_FLIPV2) mface->puno |= ME_FLIPV3;
			if(a & ME_FLIPV3) mface->puno |= ME_FLIPV1;

		}
	}
	else if(nr==4) {
		if(mface->v3==0 || mface->v4==0) {
			SWAP(int, mface->v1, mface->v3);
			SWAP(int, mface->v2, mface->v4);
			/* swap UV coordinates */
			UVCOPY(tmpuv, efa->tf.uv[0]);
			UVCOPY(efa->tf.uv[0], efa->tf.uv[2]);
			UVCOPY(efa->tf.uv[2], tmpuv);
			UVCOPY(tmpuv, efa->tf.uv[1]);
			UVCOPY(efa->tf.uv[1], efa->tf.uv[3]);
			UVCOPY(efa->tf.uv[3], tmpuv);
			/* swap vertex colours */
			tmpcol = efa->tf.col[0];
			efa->tf.col[0] = efa->tf.col[2];
			efa->tf.col[2] = tmpcol;
			tmpcol = efa->tf.col[1];
			efa->tf.col[1] = efa->tf.col[3];
			efa->tf.col[3] = tmpcol;

			a= mface->edcode;
			mface->edcode= 0;
			if(a & ME_V1V2) mface->edcode |= ME_V3V4;
			if(a & ME_V2V3) mface->edcode |= ME_V2V3;
			if(a & ME_V3V4) mface->edcode |= ME_V1V2;
			if(a & ME_V4V1) mface->edcode |= ME_V4V1;

			a= mface->puno;
			mface->puno &= ~15;
			if(a & ME_FLIPV1) mface->puno |= ME_FLIPV3;
			if(a & ME_FLIPV2) mface->puno |= ME_FLIPV4;
			if(a & ME_FLIPV3) mface->puno |= ME_FLIPV1;
			if(a & ME_FLIPV4) mface->puno |= ME_FLIPV2;

		}
	}
}

/* makes Mesh out of editmesh */
void load_editMesh(void)
{
	EditMesh *em = G.editMesh;
	Mesh *me= G.obedit->data;
	MVert *mvert, *oldverts;
	MEdge *medge=NULL;
	MFace *mface;
	MSticky *ms;
	KeyBlock *actkey=NULL, *currkey;
	EditVert *eve;
	EditFace *efa;
	EditEdge *eed;
	float *fp, *newkey, *oldkey, nor[3];
	int i, a, ototvert, totedge=0;
	MDeformVert *dvert;
	int	usedDvert = 0;

	waitcursor(1);
	countall();

	/* this one also tests of edges are not in faces: */
	/* eed->f2==0: not in face, f2==1: draw it */
	/* eed->f1 : flag for dynaface (cylindertest, old engine) */
	/* eve->f1 : flag for dynaface (sphere test, old engine) */
	edge_drawflags();
	
	/* this sets efa->puno, punoflag (for vertex normal & projection) */
	vertexnormals( (me->flag & ME_NOPUNOFLIP)==0 );
		
	eed= em->edges.first;
	while(eed) {
		totedge++;
		if(me->medge==NULL && (eed->f2==0)) G.totface++;
		eed= eed->next;
	}
	
	/* new Vertex block */
	if(G.totvert==0) mvert= NULL;
	else mvert= MEM_callocN(G.totvert*sizeof(MVert), "loadeditMesh vert");

	/* new Edge block */
	if(totedge) {
		if(me->medge==NULL) totedge= 0;	// if edges get added is defined by orig mesh
		else medge= MEM_callocN(totedge*sizeof(MEdge), "loadeditMesh edge");
	}
	
	/* new Face block */
	if(G.totface==0) mface= NULL;
	else mface= MEM_callocN(G.totface*sizeof(MFace), "loadeditMesh face");
	

	if (G.totvert==0) dvert= NULL;
	else dvert = MEM_callocN(G.totvert*sizeof(MDeformVert), "loadeditMesh3");

	if (me->dvert) free_dverts(me->dvert, me->totvert);
	me->dvert=dvert;

	/* lets save the old verts just in case we are actually working on
	 * a key ... we now do processing of the keys at the end */
	oldverts = me->mvert;
	ototvert= me->totvert;

	/* put new data in Mesh */
	me->mvert= mvert;
	me->totvert= G.totvert;

	if(me->medge) MEM_freeN(me->medge);
	me->medge= medge;
	if(medge) me->totedge= totedge; else me->totedge= 0;
	
	if(me->mface) MEM_freeN(me->mface);
	me->mface= mface;
	me->totface= G.totface;
		
	/* the vertices, abuse ->vn as counter */
	eve= em->verts.first;
	a= 0;

	while(eve) {
		VECCOPY(mvert->co, eve->co);
		mvert->mat_nr= 255;  /* what was this for, halos? */
		
		/* vertex normal */
		VECCOPY(nor, eve->no);
		VecMulf(nor, 32767.0);
		VECCOPY(mvert->no, nor);

		/* NEW VERSION */
		if (dvert){
			dvert->totweight=eve->totweight;
			if (eve->dw){
				dvert->dw = MEM_callocN (sizeof(MDeformWeight)*eve->totweight,
										 "deformWeight");
				memcpy (dvert->dw, eve->dw, 
						sizeof(MDeformWeight)*eve->totweight);
				usedDvert++;
			}
		}

		eve->vn= (EditVert *)(long)(a++);  /* counter */
			
		mvert->flag= 0;
		if(eve->f1==1) mvert->flag |= ME_SPHERETEST;
		mvert->flag |= (eve->f & SELECT);
		if (eve->h) mvert->flag |= ME_HIDE;			
			
		eve= eve->next;
		mvert++;
		dvert++;
	}
	
	/* If we didn't actually need the dverts, get rid of them */
	if (!usedDvert){
		free_dverts(me->dvert, G.totvert);
		me->dvert=NULL;
	}

	/* the edges */
	if(medge) {
		eed= em->edges.first;
		while(eed) {
			medge->v1= (unsigned int) eed->v1->vn;
			medge->v2= (unsigned int) eed->v2->vn;
			
			medge->flag= eed->f & SELECT;
			if(eed->f2<2) medge->flag |= ME_EDGEDRAW;
			if(eed->seam) medge->flag |= ME_SEAM;
			
			medge->crease= (char)(255.0*eed->crease);

			medge++;
			eed= eed->next;
		}
	}

	/* the faces */
	efa= em->faces.first;
	i = 0;
	while(efa) {
		mface= &((MFace *) me->mface)[i];
		
		mface->v1= (unsigned int) efa->v1->vn;
		mface->v2= (unsigned int) efa->v2->vn;
		mface->v3= (unsigned int) efa->v3->vn;
		if(efa->v4) mface->v4= (unsigned int) efa->v4->vn;
			
		mface->mat_nr= efa->mat_nr;
		mface->puno= efa->puno;
		mface->flag= efa->flag;
		/* bit 0 of flag is already taken for smooth... */
		if(efa->f & 1) mface->flag |= ME_FACE_SEL;
		else mface->flag &= ~ME_FACE_SEL;
		
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
			mface->edcode |= ME_V1V2; 
			efa->e1->f2= 2;
		}			
		if(efa->e2->f2==1) {
			mface->edcode |= ME_V2V3; 
			efa->e2->f2= 2;
		}
		if(efa->e3->f2==1) {
			if(efa->v4) {
				mface->edcode |= ME_V3V4;
			}
			else {
				mface->edcode |= ME_V3V1;
			}
			efa->e3->f2= 2;
		}
		if(efa->e4 && efa->e4->f2==1) {
			mface->edcode |= ME_V4V1; 
			efa->e4->f2= 2;
		}


		/* no index '0' at location 3 or 4 */
		if(efa->v4) fix_faceindices(mface, efa, 4);
		else fix_faceindices(mface, efa, 3);
			
		i++;
		efa= efa->next;
	}
		
	/* add loose edges as a face */
	if(medge==NULL) {
		eed= em->edges.first;
		while(eed) {
			if( eed->f2==0 ) {
				mface= &((MFace *) me->mface)[i];
				mface->v1= (unsigned int) eed->v1->vn;
				mface->v2= (unsigned int) eed->v2->vn;
				test_index_mface(mface, 2);
				mface->edcode= ME_V1V2;
				i++;
			}
			eed= eed->next;
		}
	}
	
	tex_space_mesh(me);

	/* tface block */
	if( me->tface && me->totface ) {
		TFace *tfn, *tf;
			
		tf=tfn= MEM_callocN(sizeof(TFace)*me->totface, "tface");
		efa= em->faces.first;
		while(efa) {
				
			*tf= efa->tf;
				
			if(G.f & G_FACESELECT) {
				if( efa->f & SELECT)  tf->flag |= TF_SELECT;
				else tf->flag &= ~TF_SELECT;
			}
				
			tf++;
			efa= efa->next;
		}

		if(me->tface) MEM_freeN(me->tface);
		me->tface= tfn;
	}
	else if(me->tface) {
		MEM_freeN(me->tface);
		me->tface= NULL;
	}
		
	/* mcol: same as tface... */
	if(me->mcol && me->totface) {
		unsigned int *mcn, *mc;

		mc=mcn= MEM_mallocN(4*sizeof(int)*me->totface, "mcol");
		efa= em->faces.first;
		while(efa) {
			memcpy(mc, efa->tf.col, 4*sizeof(int));
				
			mc+=4;
			efa= efa->next;
		}
		if(me->mcol) MEM_freeN(me->mcol);
			me->mcol= (MCol *)mcn;
	}
	else if(me->mcol) {
		MEM_freeN(me->mcol);
		me->mcol= 0;
	}


	/* are there keys? */
	if(me->key) {

		/* find the active key */
		actkey= me->key->block.first;
		while(actkey) {
			if(actkey->flag & SELECT) break;
			actkey= actkey->next;
		}

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
				if (eve->keyindex >= 0) { // old vertex
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

	if(actkey) do_spec_key(me->key);
	
	/* te be sure: clear ->vn pointers */
	eve= em->verts.first;
	while(eve) {
		eve->vn= 0;
		eve= eve->next;
	}

	/* displists of all users, including this one */
	freedisplist(&me->disp);
	freedisplist(&G.obedit->disp);
	
	/* sticky */
	if(me->msticky) {
		if (ototvert<me->totvert) {
			ms= MEM_callocN(me->totvert*sizeof(MSticky), "msticky");
			memcpy(ms, me->msticky, ototvert*sizeof(MSticky));
			MEM_freeN(me->msticky);
			me->msticky= ms;
			error("Sticky was too small");
		}
	}
	waitcursor(0);
}

void remake_editMesh(void)
{
	make_editMesh();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
	BIF_undo_push("Undo all changes");
}

/* *************** SEPARATE (partial exit editmode) *************/


void separatemenu(void)
{
	short event;

	event = pupmenu("Separate (No undo!) %t|Selected%x1|All Loose Parts%x2");
	
	if (event==0) return;
	waitcursor(1);
	
	switch (event) {
	case 1: 
		separate_mesh();		    
		break;
	case 2:	    	    	    
		separate_mesh_loose();	    	    
		break;
	}
	waitcursor(0);
}


void separate_mesh(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve, *v1;
	EditEdge *eed, *e1;
	EditFace *efa, *vl1;
	Object *oldob;
	Mesh *me, *men;
	Base *base, *oldbase;
	ListBase edve, eded, edvl;
	float trans[9];
	int ok, flag;
	
	TEST_EDITMESH	

	waitcursor(1);
	
	me= get_mesh(G.obedit);
	if(me->key) {
		error("Can't separate with vertex keys");
		return;
	}
	
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
	
	/* test for split */
	ok= 0;
	eed= em->edges.first;
	while(eed) {
		flag= (eed->v1->f & SELECT)+(eed->v2->f & SELECT);
		if(flag==SELECT) {
			ok= 1;
			break;
		}
		eed= eed->next;
	}
	if(ok) {
		/* SPLIT: first make duplicate */
		adduplicateflag(SELECT);
		/* SPLIT: old faces have 3x flag 128 set, delete these ones */
		delfaceflag(128);
	}
	
	/* set apart: everything that is not selected */
	edve.first= edve.last= eded.first= eded.last= edvl.first= edvl.last= 0;
	eve= em->verts.first;
	while(eve) {
		v1= eve->next;
		if((eve->f & SELECT)==0) {
			BLI_remlink(&em->verts, eve);
			BLI_addtail(&edve, eve);
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
		}
		efa= vl1;
	}
	
	oldob= G.obedit;
	oldbase= BASACT;
	
	trans[0]=trans[1]=trans[2]=trans[3]=trans[4]=trans[5]= 0.0;
	trans[6]=trans[7]=trans[8]= 1.0;
	G.qual |= LR_ALTKEY;	/* patch to make sure we get a linked duplicate */
	adduplicate(trans);
	G.qual &= ~LR_ALTKEY;
	
	G.obedit= BASACT->object;	/* basact was set in adduplicate()  */

	men= copy_mesh(me);
	set_mesh(G.obedit, men);
	/* because new mesh is a copy: reduce user count */
	men->id.us--;
	
	load_editMesh();
	
	BASACT->flag &= ~SELECT;
	
	makeDispList(G.obedit);
	free_editMesh(G.editMesh);
	
	em->verts= edve;
	em->edges= eded;
	em->faces= edvl;
	
	/* hashedges are freed now, make new! */
	editMesh_set_hash();
	
	G.obedit= oldob;
	BASACT= oldbase;
	BASACT->flag |= SELECT;
	
	waitcursor(0);

	countall();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);

}

void separate_mesh_loose(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve, *v1;
	EditEdge *eed, *e1;
	EditFace *efa, *vl1;
	Object *oldob;
	Mesh *me, *men;
	Base *base, *oldbase;
	ListBase edve, eded, edvl;
	float trans[9];
	int ok, vertsep=0, flag;	
	short done=0, check=1;
		
	TEST_EDITMESH
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
		
		me= get_mesh(G.obedit);
		if(me->key) {
			error("Can't separate a mesh with vertex keys");
			return;
		}		
		
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
			/* Test for splitting: Separate selected */
			ok= 0;
			eed= em->edges.first;
			while(eed) {
				flag= (eed->v1->f & SELECT)+(eed->v2->f & SELECT);
				if(flag==SELECT) {
					ok= 1;
					break;
				}
				eed= eed->next;
			}
			if(ok) {
				/* SPLIT: first make duplicate */
				adduplicateflag(SELECT);
				/* SPLIT: old faces have 3x flag 128 set, delete these ones */
				delfaceflag(128);
			}	
			
			EM_select_flush();	// from verts->edges->faces

			/* set apart: everything that is not selected */
			edve.first= edve.last= eded.first= eded.last= edvl.first= edvl.last= 0;
			eve= em->verts.first;
			while(eve) {
				v1= eve->next;
				if((eve->f & SELECT)==0) {
					BLI_remlink(&em->verts, eve);
					BLI_addtail(&edve, eve);
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
				}
				efa= vl1;
			}
			
			oldob= G.obedit;
			oldbase= BASACT;
			
			trans[0]=trans[1]=trans[2]=trans[3]=trans[4]=trans[5]= 0.0;
			trans[6]=trans[7]=trans[8]= 1.0;
			G.qual |= LR_ALTKEY;	/* patch to make sure we get a linked duplicate */
			adduplicate(trans);
			G.qual &= ~LR_ALTKEY;
			
			G.obedit= BASACT->object;	/* basact was set in adduplicate()  */
		
			men= copy_mesh(me);
			set_mesh(G.obedit, men);
			/* because new mesh is a copy: reduce user count */
			men->id.us--;
			
			load_editMesh();
			
			BASACT->flag &= ~SELECT;
			
			makeDispList(G.obedit);
			free_editMesh(G.editMesh);
			
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
	makeDispList(G.obedit);	
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
	short totweight;
	struct MDeformWeight *dw;
	int keyindex; 
} EditVertC;

typedef struct EditEdgeC
{
	int v1, v2;
	unsigned char f, h, seam, pad;
	float crease;
} EditEdgeC;

typedef struct EditFaceC
{
	int v1, v2, v3, v4;
	unsigned char mat_nr, flag, f, h, puno, pad;
	short pad1;
} EditFaceC;

typedef struct UndoMesh {
	EditVertC *verts;
	EditEdgeC *edges;
	EditFaceC *faces;
	TFace *tfaces;
	int totvert, totedge, totface;
} UndoMesh;


/* for callbacks */

static void free_undoMesh(void *umv)
{
	UndoMesh *um= umv;
	EditVertC *evec;
	int a;
	
	for(a=0, evec= um->verts; a<um->totvert; a++, evec++) {
		if(evec->dw) MEM_freeN(evec->dw);
	}
	
	if(um->verts) MEM_freeN(um->verts);
	if(um->edges) MEM_freeN(um->edges);
	if(um->faces) MEM_freeN(um->faces);
	if(um->tfaces) MEM_freeN(um->tfaces);
	MEM_freeN(um);
}

static void *editMesh_to_undoMesh(void)
{
	EditMesh *em= G.editMesh;
	UndoMesh *um;
	Mesh *me= G.obedit->data;
	EditVert *eve;
	EditEdge *eed;
	EditFace *efa;
	EditVertC *evec=NULL;
	EditEdgeC *eedc=NULL;
	EditFaceC *efac=NULL;
	TFace *tface= NULL;
	int a=0;
	
	um= MEM_callocN(sizeof(UndoMesh), "undomesh");

	for(eve=em->verts.first; eve; eve= eve->next) um->totvert++;
	for(eed=em->edges.first; eed; eed= eed->next) um->totedge++;
	for(efa=em->faces.first; efa; efa= efa->next) um->totface++;

	/* malloc blocks */
	
	if(um->totvert) evec= um->verts= MEM_callocN(um->totvert*sizeof(EditVertC), "allvertsC");
	if(um->totedge) eedc= um->edges= MEM_callocN(um->totedge*sizeof(EditEdgeC), "alledgesC");
	if(um->totface) efac= um->faces= MEM_callocN(um->totface*sizeof(EditFaceC), "allfacesC");

	if(me->tface) tface= um->tfaces= MEM_mallocN(um->totface*sizeof(TFace), "all tfacesC");

		//printf("copy editmesh %d\n", um->totvert*sizeof(EditVert) + um->totedge*sizeof(EditEdge) + um->totface*sizeof(EditFace));
		//printf("copy undomesh %d\n", um->totvert*sizeof(EditVertC) + um->totedge*sizeof(EditEdgeC) + um->totface*sizeof(EditFaceC));
	
	/* now copy vertices */
	for(eve=em->verts.first; eve; eve= eve->next, evec++, a++) {
		VECCOPY(evec->co, eve->co);
		VECCOPY(evec->no, eve->no);

		evec->f= eve->f;
		evec->h= eve->h;
		evec->keyindex= eve->keyindex;
		evec->totweight= eve->totweight;
		evec->dw= MEM_dupallocN(eve->dw);
		
		eve->vn= (EditVert *)a;
	}
	
	/* copy edges */
	for(eed=em->edges.first; eed; eed= eed->next, eedc++)  {
		eedc->v1= (int)eed->v1->vn;
		eedc->v2= (int)eed->v2->vn;
		eedc->f= eed->f;
		eedc->h= eed->h;
		eedc->seam= eed->seam;
		eedc->crease= eed->crease;
	}
	
	/* copy faces */
	for(efa=em->faces.first; efa; efa= efa->next, efac++) {
		efac->v1= (int)efa->v1->vn;
		efac->v2= (int)efa->v2->vn;
		efac->v3= (int)efa->v3->vn;
		if(efa->v4) efac->v4= (int)efa->v4->vn;
		else efac->v4= -1;
		
		efac->mat_nr= efa->mat_nr;
		efac->flag= efa->flag;
		efac->f= efa->f;
		efac->h= efa->h;
		efac->puno= efa->puno;
		
		if(tface) {
			*tface= efa->tf;
			tface++;
		}
	}
	
	return um;
}

static void undoMesh_to_editMesh(void *umv)
{
	UndoMesh *um= umv;
	EditMesh *em= G.editMesh;
	EditVert *eve, **evar=NULL;
	EditEdge *eed;
	EditFace *efa;
	EditVertC *evec;
	EditEdgeC *eedc;
	EditFaceC *efac;
	TFace *tface;
	int a=0;
	
	free_editMesh(G.editMesh);
	
	/* malloc blocks */
	memset(em, 0, sizeof(EditMesh));

	init_editmesh_fastmalloc(em, um->totvert, um->totedge, um->totface);

	/* now copy vertices */
	if(um->totvert) evar= MEM_mallocN(um->totvert*sizeof(EditVert *), "vertex ar");
	for(a=0, evec= um->verts; a<um->totvert; a++, evec++) {
		eve= addvertlist(evec->co);
		evar[a]= eve;

		VECCOPY(eve->no, evec->no);
		eve->f= evec->f;
		eve->h= evec->h;
		eve->totweight= evec->totweight;
		eve->keyindex= evec->keyindex;
		eve->dw= MEM_dupallocN(evec->dw);
	}

	/* copy edges */
	for(a=0, eedc= um->edges; a<um->totedge; a++, eedc++) {
		eed= addedgelist(evar[eedc->v1], evar[eedc->v2], NULL);

		eed->f= eedc->f;
		eed->h= eedc->h;
		eed->seam= eedc->seam;
		eed->crease= eedc->crease;
	}
	
	/* copy faces */
	tface= um->tfaces;
	for(a=0, efac= um->faces; a<um->totface; a++, efac++) {
		if(efac->v4 != -1)
			efa= addfacelist(evar[efac->v1], evar[efac->v2], evar[efac->v3], evar[efac->v4], NULL);
		else 
			efa= addfacelist(evar[efac->v1], evar[efac->v2], evar[efac->v3], NULL, NULL);

		efa->mat_nr= efac->mat_nr;
		efa->flag= efac->flag;
		efa->f= efac->f;
		efa->h= efac->h;
		efa->puno= efac->puno;
		
		if(tface) {
			efa->tf= *tface;
			tface++;
		}
	}
	
	end_editmesh_fastmalloc();
	if(evar) MEM_freeN(evar);
}


/* and this is all the undo system needs to know */
void undo_push_mesh(char *name)
{
	undo_editmode_push(name, free_undoMesh, undoMesh_to_editMesh, editMesh_to_undoMesh);
}



/* *************** END UNDO *************/


