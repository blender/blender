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

#include "MTC_matrixops.h"

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

/* HASH struct quickly finding of edges */
struct HashEdge {
	struct EditEdge *eed;
	struct HashEdge *next;
};

struct HashEdge *hashedgetab=NULL;


/* ************ ADD / REMOVE / FIND ****************** */

#define EDHASH(a, b)	( (a)*256 + (b) )
#define EDHASHSIZE	65536

EditVert *addvertlist(float *vec)
{
	EditMesh *em = G.editMesh;
	EditVert *eve;
	static unsigned char hashnr= 0;

	eve= calloc(sizeof(EditVert), 1);
	BLI_addtail(&em->verts, eve);
	
	if(vec) VECCOPY(eve->co, vec);

	eve->hash= hashnr++;

	/* new verts get keyindex of -1 since they did not
	 * have a pre-editmode vertex order
	 */
	eve->keyindex = -1;
	return eve;
}

EditEdge *findedgelist(EditVert *v1, EditVert *v2)
{
	EditVert *v3;
	struct HashEdge *he;

	if(hashedgetab==0) {
		hashedgetab= MEM_callocN(EDHASHSIZE*sizeof(struct HashEdge), "hashedgetab");
	}
	
	/* swap ? */
	if( (long)v1 > (long)v2) {
		v3= v2; 
		v2= v1; 
		v1= v3;
	}
	
	he= hashedgetab + EDHASH(v1->hash, v2->hash);
	
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

	first= hashedgetab + EDHASH(eed->v1->hash, eed->v2->hash);

	if( first->eed==0 ) {
		first->eed= eed;
	}
	else {
		he= (struct HashEdge *)malloc(sizeof(struct HashEdge)); 
		he->eed= eed;
		he->next= first->next;
		first->next= he;
	}
}

static void remove_hashedge(EditEdge *eed)
{
	/* assuming eed is in the list */
	
	struct HashEdge *first, *he, *prev=NULL;


	he=first= hashedgetab + EDHASH(eed->v1->hash, eed->v2->hash);

	while(he) {
		if(he->eed == eed) {
			/* remove from list */
			if(he==first) {
				if(first->next) {
					he= first->next;
					first->eed= he->eed;
					first->next= he->next;
					free(he);
				}
				else he->eed= 0;
			}
			else {
				prev->next= he->next;
				free(he);
			}
			return;
		}
		prev= he;
		he= he->next;
	}
}

void free_hashedgetab(void)
{
	struct HashEdge *he, *first, *hen;
	int a;
	
	if(hashedgetab) {
	
		first= hashedgetab;
		for(a=0; a<EDHASHSIZE; a++, first++) {
			he= first->next;
			while(he) {
				hen= he->next;
				free(he);
				he= hen;
			}
		}
		MEM_freeN(hashedgetab);
		hashedgetab= 0;
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

		eed= (EditEdge *)calloc(sizeof(EditEdge), 1);
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

void free_editvert (EditVert *eve)
{
	if (eve->dw) MEM_freeN (eve->dw);
	free (eve);
}

void remedge(EditEdge *eed)
{
	EditMesh *em = G.editMesh;

	BLI_remlink(&em->edges, eed);
	remove_hashedge(eed);
}

void free_editedge(EditEdge *eed)
{
	free(eed);
}

void free_editface(EditFace *efa)
{
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

	efa= (EditFace *)calloc(sizeof(EditFace), 1);
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

	if(efa->v4) CalcNormFloat4(v1->co, v2->co, v3->co, v4->co, efa->n);
	else CalcNormFloat(v1->co, v2->co, v3->co, efa->n);

	return efa;
}

/* ********* end add / new / find */




/* ************************ IN & OUT EDITMODE ***************************** */

static void edge_normal_compare(EditEdge *eed, EditFace *efa1)
{
	EditFace *efa2;
	float cent1[3], cent2[3];
	float inp;
	
	efa2= (EditFace *)eed->vn;
	if(efa1==efa2) return;
	
	inp= efa1->n[0]*efa2->n[0] + efa1->n[1]*efa2->n[1] + efa1->n[2]*efa2->n[2];
	if(inp<0.999 && inp >-0.999) eed->f= 1;
		
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
		eed->f= eed->f1= 0;
		eed->vn= 0;
		eed= eed->next;
	}

	efa= em->faces.first;
	while(efa) {
		e1= efa->e1;
		e2= efa->e2;
		e3= efa->e3;
		e4= efa->e4;
		if(e1->f<3) e1->f+= 1;
		if(e2->f<3) e2->f+= 1;
		if(e3->f<3) e3->f+= 1;
		if(e4 && e4->f<3) e4->f+= 1;
		
		if(e1->vn==0) e1->vn= (EditVert *)efa;
		if(e2->vn==0) e2->vn= (EditVert *)efa;
		if(e3->vn==0) e3->vn= (EditVert *)efa;
		if(e4 && e4->vn==0) e4->vn= (EditVert *)efa;
		
		efa= efa->next;
	}

	if(G.f & G_ALLEDGES) {
		efa= em->faces.first;
		while(efa) {
			if(efa->e1->f>=2) efa->e1->f= 1;
			if(efa->e2->f>=2) efa->e2->f= 1;
			if(efa->e3->f>=2) efa->e3->f= 1;
			if(efa->e4 && efa->e4->f>=2) efa->e4->f= 1;
			
			efa= efa->next;
		}		
	}	
	else {
		
		/* handle single-edges for 'test cylinder flag' (old engine) */
		
		eed= em->edges.first;
		while(eed) {
			if(eed->f==1) eed->f1= 1;
			eed= eed->next;
		}

		/* all faces, all edges with flag==2: compare normal */
		efa= em->faces.first;
		while(efa) {
			if(efa->e1->f==2) edge_normal_compare(efa->e1, efa);
			if(efa->e2->f==2) edge_normal_compare(efa->e2, efa);
			if(efa->e3->f==2) edge_normal_compare(efa->e3, efa);
			if(efa->e4 && efa->e4->f==2) edge_normal_compare(efa->e4, efa);
			
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


void free_editMesh(void)
{
	
	EditMesh *em = G.editMesh;
	if(em->verts.first) free_vertlist(&em->verts);
	if(em->edges.first) free_edgelist(&em->edges);
	if(em->faces.first) free_facelist(&em->faces);
	free_hashedgetab();
	G.totvert= G.totface= 0;
}

void make_editMesh_real(Mesh *me)
{
	EditMesh *em = G.editMesh;
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
	free_editMesh();
	
	G.totvert= tot= me->totvert;

	if(tot==0) {
		countall();
		return;
	}
	
	waitcursor(1);

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
							eve1->f |= 1;
							eve2->f |= 1;
							if(eve3) eve3->f |= 1;
							if(eve4) eve4->f |= 1;
						}
					}
				}
			
				efa->mat_nr= mface->mat_nr;
				efa->flag= mface->flag;
			}

			if(me->tface) tface++;
			if(mcol) mcol+=4;
		}
	}
	
	/* intrr: needed because of hidden vertices imported from Mesh */
	
	eed= em->edges.first;
	while(eed) {
		if(eed->v1->h || eed->v2->h) eed->h= 1;
		else eed->h= 0;
		eed= eed->next;
	}	
	
	MEM_freeN(evlist);
	
	countall();
	
	if (mesh_uses_displist(me))
		makeDispList(G.obedit);
	
	waitcursor(0);
}

void make_editMesh(void)
{
	Mesh *me;	

	me= get_mesh(G.obedit);
	if (me != G.undo_last_data) {
		G.undo_edit_level= -1;
		G.undo_edit_highest= -1;
		if (G.undo_clear) G.undo_clear();
		G.undo_last_data= me;
		G.undo_clear= undo_clear_mesh;
	}
	make_editMesh_real(me);
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


void load_editMesh_real(Mesh *me, int undo)
{
	EditMesh *em = G.editMesh;
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

	/* this one also tests of edges are not in faces: */
	/* eed->f==0: not in face, f==1: draw it */
	/* eed->f1 : flag for dynaface (cylindertest, old engine) */
	/* eve->f1 : flag for dynaface (sphere test, old engine) */
	edge_drawflags();
	
	/* WATCH IT: in efa->f is punoflag (for vertex normal) */
	vertexnormals( (me->flag & ME_NOPUNOFLIP)==0 );
		
	eed= em->edges.first;
	while(eed) {
		totedge++;
		if(me->medge==NULL && (eed->f==0)) G.totface++;
		eed= eed->next;
	}
	
	/* new Vertex block */
	if(G.totvert==0) mvert= NULL;
	else mvert= MEM_callocN(G.totvert*sizeof(MVert), "loadeditMesh vert");

	/* new Edge block */
	if(totedge) {
		Mesh *mesh= G.obedit->data;
		if(mesh->medge==NULL) totedge= 0;	// if edges get added is defined by orig mesh, not undo mesh
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
			
		mvert->flag= 0;
		if(eve->f1==1) mvert->flag |= ME_SPHERETEST;
		mvert->flag |= (eve->f & 1);
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
			if(eed->f<2) medge->flag = ME_EDGEDRAW;
			medge->crease= (char)(255.0*eed->crease);
			if(eed->seam) medge->flag |= ME_SEAM;

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
		mface->puno= efa->f;
		mface->flag= efa->flag;
			
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
			
		/* watch: efa->e1->f==0 means loose edge */ 
			
		if(efa->e1->f==1) {
			mface->edcode |= ME_V1V2; 
			efa->e1->f= 2;
		}			
		if(efa->e2->f==1) {
			mface->edcode |= ME_V2V3; 
			efa->e2->f= 2;
		}
		if(efa->e3->f==1) {
			if(efa->v4) {
				mface->edcode |= ME_V3V4;
			}
			else {
				mface->edcode |= ME_V3V1;
			}
			efa->e3->f= 2;
		}
		if(efa->e4 && efa->e4->f==1) {
			mface->edcode |= ME_V4V1; 
			efa->e4->f= 2;
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
			if( eed->f==0 ) {
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

	/* tface block, always when undo even when it wasnt used, 
	   this because of empty me pointer */
	if( (me->tface || undo) && me->totface ) {
		TFace *tfn, *tf;
			
		tf=tfn= MEM_callocN(sizeof(TFace)*me->totface, "tface");
		efa= em->faces.first;
		while(efa) {
				
			*tf= efa->tf;
				
			if(G.f & G_FACESELECT) {
				if( faceselectedAND(efa, 1) ) tf->flag |= TF_SELECT;
				else tf->flag &= ~TF_SELECT;
			}
				
			tf++;
			efa= efa->next;
		}
		/* if undo, me was empty */
		if(me->tface) MEM_freeN(me->tface);
		me->tface= tfn;
	}
	else if(me->tface) {
		MEM_freeN(me->tface);
		me->tface= NULL;
	}
		
	/* mcol: same as tface... */
	if( (me->mcol || undo) && me->totface) {
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
	undo_push_mesh("Undo all changes");
	make_editMesh();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);
}

/* load from EditMode to Mesh */

void load_editMesh()
{
	Mesh *me;

	waitcursor(1);
	countall();
	me= get_mesh(G.obedit);
       
	load_editMesh_real(me, 0);
}



/* *********************  TOOLS  ********************* */


															


/*********************** EDITMESH UNDO ********************************/
/* Mesh Edit undo by Alexander Ewring,                                */
/* ported by Robert Wenzlaff                                          */
/*                                                                    */
/* Any meshedit function wishing to create an undo step, calls        */
/*     undo_push_mesh("menu_name_of_step");                           */

Mesh *undo_new_mesh(void)
{
	return(MEM_callocN(sizeof(Mesh), "undo_mesh"));
}

void undo_free_mesh(Mesh *me)
{
	if(me->mat) MEM_freeN(me->mat);
	if(me->orco) MEM_freeN(me->orco);
	if(me->mvert) MEM_freeN(me->mvert);
	if(me->medge) MEM_freeN(me->medge);
	if(me->mface) MEM_freeN(me->mface);
	if(me->tface) MEM_freeN(me->tface);
	if(me->dvert) free_dverts(me->dvert, me->totvert);
	if(me->mcol) MEM_freeN(me->mcol);
	if(me->msticky) MEM_freeN(me->msticky);
	if(me->bb) MEM_freeN(me->bb);
	if(me->disp.first) freedisplist(&me->disp);
	MEM_freeN(me);
}


void undo_push_mesh(char *name)
{
	Mesh *me;
	int i;

	countall();

	G.undo_edit_level++;

	if (G.undo_edit_level<0) {
		printf("undo: ERROR: G.undo_edit_level negative\n");
		return;
	}


	if (G.undo_edit[G.undo_edit_level].datablock != 0) {
		undo_free_mesh(G.undo_edit[G.undo_edit_level].datablock);
	}
	if (strcmp(name, "U")!=0) {
		for (i=G.undo_edit_level+1; i<(U.undosteps-1); i++) {
			if (G.undo_edit[i].datablock != 0) {
				undo_free_mesh(G.undo_edit[i].datablock);
				G.undo_edit[i].datablock= 0;
			}
		}
		G.undo_edit_highest= G.undo_edit_level;
	}

	me= undo_new_mesh();

	if (G.undo_edit_level>=U.undosteps) {
		G.undo_edit_level--;
		undo_free_mesh((Mesh*)G.undo_edit[0].datablock);
		G.undo_edit[0].datablock= 0;
		for (i=0; i<(U.undosteps-1); i++) {
			G.undo_edit[i]= G.undo_edit[i+1];
		}
	}

	if (strcmp(name, "U")!=0) strcpy(G.undo_edit[G.undo_edit_level].name, name);
	//printf("undo: saving block: %d [%s]\n", G.undo_edit_level, G.undo_edit[G.undo_edit_level].name);

	G.undo_edit[G.undo_edit_level].datablock= (void*)me;
	load_editMesh_real(me, 1);
}

void undo_pop_mesh(int steps)  /* steps == 1 is one step */
{
	if (G.undo_edit_level > (steps-2)) {
		undo_push_mesh("U");
		G.undo_edit_level-= steps;

		//printf("undo: restoring block: %d [%s]\n", G.undo_edit_level, G.undo_edit[G.undo_edit_level].name);    -
		make_editMesh_real((Mesh*)G.undo_edit[G.undo_edit_level].datablock);
		allqueue(REDRAWVIEW3D, 0);
		makeDispList(G.obedit);
		G.undo_edit_level--;
	} else error("No more steps to undo");
}


void undo_redo_mesh(void)
{
	if ( (G.undo_edit[G.undo_edit_level+2].datablock) &&
		( (G.undo_edit_level+1) <= G.undo_edit_highest ) ) {
		G.undo_edit_level++;

		//printf("redo: restoring block: %d [%s]\n", G.undo_edit_level+1, G.undo_edit[G.undo_edit_level+1].name);-
		make_editMesh_real((Mesh*)G.undo_edit[G.undo_edit_level+1].datablock);
		allqueue(REDRAWVIEW3D, 0);
		makeDispList(G.obedit);
	} else error("No more steps to redo");
}

void undo_clear_mesh(void)
{
	int i;
	Mesh *me;

	for (i=0; i<=UNDO_EDIT_MAX; i++) {
		me= (Mesh*) G.undo_edit[i].datablock;
		if (me) {
			//printf("undo: freeing %d\n", i);
			undo_free_mesh(me);
			G.undo_edit[i].datablock= 0;
		}
	}
}

#ifdef WIN32
	#ifndef snprintf
		#define snprintf  _snprintf
	#endif
#endif

void undo_menu_mesh(void)
{
	short event=66;
	int i;
	char menu[2080], temp[64];

	TEST_EDITMESH

	strcpy(menu, "Undo %t|%l");
	strcat(menu, "|All changes%x1|%l");
	
	for (i=G.undo_edit_level; i>=0; i--) {
		snprintf(temp, 64, "|%s%%x%d", G.undo_edit[i].name, i+2);
		strcat(menu, temp);
	}

	event=pupmenu_col(menu, 20);

	if(event<1) return;

	if (event==1) remake_editMesh();
	else undo_pop_mesh(G.undo_edit_level-event+3);
}

/* *************** END UNDO *************/

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
	 * 4. call loadobeditdata(): this will be the new object
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
	
	/* testen for split */
	ok= 0;
	eed= em->edges.first;
	while(eed) {
		flag= (eed->v1->f & 1)+(eed->v2->f & 1);
		if(flag==1) {
			ok= 1;
			break;
		}
		eed= eed->next;
	}
	if(ok) {
		/* SPLIT: first make duplicate */
		adduplicateflag(1);
		/* SPLIT: old faces have 3x flag 128 set, delete these ones */
		delfaceflag(128);
	}
	
	/* set apart: everything that is not selected */
	edve.first= edve.last= eded.first= eded.last= edvl.first= edvl.last= 0;
	eve= em->verts.first;
	while(eve) {
		v1= eve->next;
		if((eve->f & 1)==0) {
			BLI_remlink(&em->verts, eve);
			BLI_addtail(&edve, eve);
		}
		eve= v1;
	}
	eed= em->edges.first;
	while(eed) {
		e1= eed->next;
		if( (eed->v1->f & 1)==0 || (eed->v2->f & 1)==0 ) {
			BLI_remlink(&em->edges, eed);
			BLI_addtail(&eded, eed);
		}
		eed= e1;
	}
	efa= em->faces.first;
	while(efa) {
		vl1= efa->next;
		if( (efa->v1->f & 1)==0 || (efa->v2->f & 1)==0 || (efa->v3->f & 1)==0 ) {
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
	free_editMesh();
	
	em->verts= edve;
	em->edges= eded;
	em->faces= edvl;
	
	/* hashedges are freed now, make new! */
	eed= em->edges.first;
	while(eed) {
		if( findedgelist(eed->v1, eed->v2)==NULL )
			insert_hashedge(eed);
		eed= eed->next;
	}
	
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
	 * 4. call loadobeditdata(): this will be the new object
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
		//sel= 3;
		/* clear test flags */
		eve= em->verts.first;
		while(eve) {
			eve->f&= ~1;			
			eve= eve->next;
		}
		
		/* Select a random vert to start with */
		eve= em->verts.first;
		eve->f |= 1;
		
		while(check==1) {
			check= 0;			
			eed= em->edges.first;			
			while(eed) {				
				if(eed->h==0) {
					if(eed->v1->f & 1) {
						if( (eed->v2->f & 1)==0 ) {
							eed->v2->f |= 1;
							vertsep++;
							check= 1;
						}
					}
					else if(eed->v2->f & 1) {
						if( (eed->v1->f & 1)==0 ) {
							eed->v1->f |= 1;
							vertsep++;
							check= 1;
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
		if(G.totvert==vertsep)done=1;				
		else{			
			/* Test for splitting: Separate selected */
			ok= 0;
			eed= em->edges.first;
			while(eed) {
				flag= (eed->v1->f & 1)+(eed->v2->f & 1);
				if(flag==1) {
					ok= 1;
					break;
				}
				eed= eed->next;
			}
			if(ok) {
				/* SPLIT: first make duplicate */
				adduplicateflag(1);
				/* SPLIT: old faces have 3x flag 128 set, delete these ones */
				delfaceflag(128);
			}	
			
			
			
			/* set apart: everything that is not selected */
			edve.first= edve.last= eded.first= eded.last= edvl.first= edvl.last= 0;
			eve= em->verts.first;
			while(eve) {
				v1= eve->next;
				if((eve->f & 1)==0) {
					BLI_remlink(&em->verts, eve);
					BLI_addtail(&edve, eve);
				}
				eve= v1;
			}
			eed= em->edges.first;
			while(eed) {
				e1= eed->next;
				if( (eed->v1->f & 1)==0 || (eed->v2->f & 1)==0 ) {
					BLI_remlink(&em->edges, eed);
					BLI_addtail(&eded, eed);
				}
				eed= e1;
			}
			efa= em->faces.first;
			while(efa) {
				vl1= efa->next;
				if( (efa->v1->f & 1)==0 || (efa->v2->f & 1)==0 || (efa->v3->f & 1)==0 ) {
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
			free_editMesh();
			
			em->verts= edve;
			em->edges= eded;
			em->faces= edvl;
			
			/* hashedges are freed now, make new! */
			eed= em->edges.first;
			while(eed) {
				if( findedgelist(eed->v1, eed->v2)==NULL )
					insert_hashedge(eed);
				eed= eed->next;
			}
			
			G.obedit= oldob;
			BASACT= oldbase;
			BASACT->flag |= SELECT;	
					
		}		
	}
	
	/* unselect the vertices that we (ab)used for the separation*/
	eve= em->verts.first;
	while(eve) {
		eve->f&= ~1;			
		eve= eve->next;
	}
	
	waitcursor(0);
	countall();
	allqueue(REDRAWVIEW3D, 0);
	makeDispList(G.obedit);	
}


