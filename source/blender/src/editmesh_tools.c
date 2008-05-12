/**
 * $Id: 
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
 * The Original Code is Copyright (C) 2004 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Johnny Matthews, Geoffrey Bantle.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/*

editmesh_tool.c: UI called tools for editmesh, geometry changes here, otherwise in mods.c

*/

#include <stdlib.h>
#include <string.h>
#include <math.h>


#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "MEM_guardedalloc.h"

#include "BMF_Api.h"
#include "DNA_mesh_types.h"
#include "DNA_material_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"
#include "DNA_key_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"
#include "BLI_heap.h"

#include "BKE_depsgraph.h"
#include "BKE_customdata.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BKE_bmesh.h"

#ifdef WITH_VERSE
#include "BKE_verse.h"
#endif

#include "BIF_cursors.h"
#include "BIF_editmesh.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_resources.h"
#include "BIF_toolbox.h"
#include "BIF_transform.h"
#include "transform.h"

#ifdef WITH_VERSE
#include "BIF_verse.h"
#endif

#include "BDR_drawobject.h"
#include "BDR_editobject.h"

#include "BSE_view.h"
#include "BSE_edit.h"

#include "blendef.h"
#include "multires.h"
#include "mydevice.h"

#include "editmesh.h"

#include "MTC_vectorops.h"

#include "PIL_time.h"

/* local prototypes ---------------*/
void bevel_menu(void);
static void free_tagged_edges_faces(EditEdge *eed, EditFace *efa);

/********* qsort routines *********/


typedef struct xvertsort {
	float x;
	EditVert *v1;
} xvertsort;

static int vergxco(const void *v1, const void *v2)
{
	const xvertsort *x1=v1, *x2=v2;

	if( x1->x > x2->x ) return 1;
	else if( x1->x < x2->x) return -1;
	return 0;
}

struct facesort {
	unsigned long x;
	struct EditFace *efa;
};


static int vergface(const void *v1, const void *v2)
{
	const struct facesort *x1=v1, *x2=v2;
	
	if( x1->x > x2->x ) return 1;
	else if( x1->x < x2->x) return -1;
	return 0;
}


/* *********************************** */

void convert_to_triface(int direction)
{
	EditMesh *em = G.editMesh;
	EditFace *efa, *efan, *next;
	float fac;
	
	if(multires_test()) return;
	
	efa= em->faces.last;
	while(efa) {
		next= efa->prev;
		if(efa->v4) {
			if(efa->f & SELECT) {
				/* choose shortest diagonal for split */
				fac= VecLenf(efa->v1->co, efa->v3->co) - VecLenf(efa->v2->co, efa->v4->co);
				/* this makes sure exact squares get split different in both cases */
				if( (direction==0 && fac<FLT_EPSILON) || (direction && fac>0.0f) ) {
					efan= EM_face_from_faces(efa, NULL, 0, 1, 2, -1);
					if(efa->f & SELECT) EM_select_face(efan, 1);
					efan= EM_face_from_faces(efa, NULL, 0, 2, 3, -1);
					if(efa->f & SELECT) EM_select_face(efan, 1);
				}
				else {
					efan= EM_face_from_faces(efa, NULL, 0, 1, 3, -1);
					if(efa->f & SELECT) EM_select_face(efan, 1);
					efan= EM_face_from_faces(efa, NULL, 1, 2, 3, -1);
					if(efa->f & SELECT) EM_select_face(efan, 1);
				}
				
				BLI_remlink(&em->faces, efa);
				free_editface(efa);
			}
		}
		efa= next;
	}
	
	EM_fgon_flags();	// redo flags and indices for fgons

#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
#endif
	BIF_undo_push("Convert Quads to Triangles");
	
}

int removedoublesflag(short flag, short automerge, float limit)		/* return amount */
{
	/*
		flag -		Test with vert->flags
		automerge -	Alternative operation, merge unselected into selected.
					Used for "Auto Weld" mode. warning.
		limit -		Quick manhattan distance between verts.
	*/
	
	EditMesh *em = G.editMesh;
	/* all verts with (flag & 'flag') are being evaluated */
	EditVert *eve, *v1, *nextve;
	EditEdge *eed, *e1, *nexted;
	EditFace *efa, *nextvl;
	xvertsort *sortblock, *sb, *sb1;
	struct facesort *vlsortblock, *vsb, *vsb1;
	int a, b, test, amount;
	
	if(multires_test()) return 0;

	
	/* flag 128 is cleared, count */

	/* Normal non weld operation */
	eve= em->verts.first;
	amount= 0;
	while(eve) {
		eve->f &= ~128;
		if(eve->h==0 && (automerge || (eve->f & flag))) amount++;
		eve= eve->next;
	}
	if(amount==0) return 0;

	/* allocate memory and qsort */
	sb= sortblock= MEM_mallocN(sizeof(xvertsort)*amount,"sortremovedoub");
	eve= em->verts.first;
	while(eve) {
		if(eve->h==0 && (automerge || (eve->f & flag))) {
			sb->x= eve->co[0]+eve->co[1]+eve->co[2];
			sb->v1= eve;
			sb++;
		}
		eve= eve->next;
	}
	qsort(sortblock, amount, sizeof(xvertsort), vergxco);

	
	/* test for doubles */
	sb= sortblock;	
	if (automerge) {
		for(a=0; a<amount; a++, sb++) {
			eve= sb->v1;
			if( (eve->f & 128)==0 ) {
				sb1= sb+1;
				for(b=a+1; b<amount && (eve->f & 128)==0; b++, sb1++) {
					if(sb1->x - sb->x > limit) break;
					
					/* when automarge, only allow unselected->selected */
					v1= sb1->v1;
					if( (v1->f & 128)==0 ) {
						if ((eve->f & flag)==0 && (v1->f & flag)==1) {
							if(	(float)fabs(v1->co[0]-eve->co[0])<=limit && 
								(float)fabs(v1->co[1]-eve->co[1])<=limit &&
								(float)fabs(v1->co[2]-eve->co[2])<=limit)
							{	/* unique bit */
								eve->f|= 128;
								eve->tmp.v = v1;
							}
						} else if(	(eve->f & flag)==1 && (v1->f & flag)==0 ) {
							if(	(float)fabs(v1->co[0]-eve->co[0])<=limit && 
								(float)fabs(v1->co[1]-eve->co[1])<=limit &&
								(float)fabs(v1->co[2]-eve->co[2])<=limit)
							{	/* unique bit */
								v1->f|= 128;
								v1->tmp.v = eve;
							}
						}
					}
				}
			}
		}
	} else {
		for(a=0; a<amount; a++, sb++) {
			eve= sb->v1;
			if( (eve->f & 128)==0 ) {
				sb1= sb+1;
				for(b=a+1; b<amount; b++, sb1++) {
					/* first test: simpel dist */
					if(sb1->x - sb->x > limit) break;
					v1= sb1->v1;
					
					/* second test: is vertex allowed */
					if( (v1->f & 128)==0 ) {
						if(	(float)fabs(v1->co[0]-eve->co[0])<=limit && 
							(float)fabs(v1->co[1]-eve->co[1])<=limit &&
							(float)fabs(v1->co[2]-eve->co[2])<=limit)
						{
							v1->f|= 128;
							v1->tmp.v = eve;
						}
					}
				}
			}
		}
	}
	MEM_freeN(sortblock);
	
	if (!automerge)
		for(eve = em->verts.first; eve; eve=eve->next)
			if((eve->f & flag) && (eve->f & 128))
				EM_data_interp_from_verts(eve, eve->tmp.v, eve->tmp.v, 0.5f);
	
	/* test edges and insert again */
	eed= em->edges.first;
	while(eed) {
		eed->f2= 0;
		eed= eed->next;
	}
	eed= em->edges.last;
	while(eed) {
		nexted= eed->prev;

		if(eed->f2==0) {
			if( (eed->v1->f & 128) || (eed->v2->f & 128) ) {
				remedge(eed);

				if(eed->v1->f & 128) eed->v1 = eed->v1->tmp.v;
				if(eed->v2->f & 128) eed->v2 = eed->v2->tmp.v;
				e1= addedgelist(eed->v1, eed->v2, eed);

				if(e1) {
					e1->f2= 1;
					if(eed->f & SELECT)
						e1->f |= SELECT;
				}
				if(e1!=eed) free_editedge(eed);
			}
		}
		eed= nexted;
	}

	/* first count amount of test faces */
	efa= (struct EditFace *)em->faces.first;
	amount= 0;
	while(efa) {
		efa->f1= 0;
		if(efa->v1->f & 128) efa->f1= 1;
		else if(efa->v2->f & 128) efa->f1= 1;
		else if(efa->v3->f & 128) efa->f1= 1;
		else if(efa->v4 && (efa->v4->f & 128)) efa->f1= 1;
		
		if(efa->f1==1) amount++;
		efa= efa->next;
	}

	/* test faces for double vertices, and if needed remove them */
	efa= (struct EditFace *)em->faces.first;
	while(efa) {
		nextvl= efa->next;
		if(efa->f1==1) {
			
			if(efa->v1->f & 128) efa->v1= efa->v1->tmp.v;
			if(efa->v2->f & 128) efa->v2= efa->v2->tmp.v;
			if(efa->v3->f & 128) efa->v3= efa->v3->tmp.v;
			if(efa->v4 && (efa->v4->f & 128)) efa->v4= efa->v4->tmp.v;
		
			test= 0;
			if(efa->v1==efa->v2) test+=1;
			if(efa->v2==efa->v3) test+=2;
			if(efa->v3==efa->v1) test+=4;
			if(efa->v4==efa->v1) test+=8;
			if(efa->v3==efa->v4) test+=16;
			if(efa->v2==efa->v4) test+=32;
			
			if(test) {
				if(efa->v4) {
					if(test==1 || test==2) {
						efa->v2= efa->v3;
						efa->v3= efa->v4;
						efa->v4= 0;

						EM_data_interp_from_faces(efa, NULL, efa, 0, 2, 3, 3);

						test= 0;
					}
					else if(test==8 || test==16) {
						efa->v4= 0;
						test= 0;
					}
					else {
						BLI_remlink(&em->faces, efa);
						free_editface(efa);
						amount--;
					}
				}
				else {
					BLI_remlink(&em->faces, efa);
					free_editface(efa);
					amount--;
				}
			}
			
			if(test==0) {
				/* set edge pointers */
				efa->e1= findedgelist(efa->v1, efa->v2);
				efa->e2= findedgelist(efa->v2, efa->v3);
				if(efa->v4==0) {
					efa->e3= findedgelist(efa->v3, efa->v1);
					efa->e4= 0;
				}
				else {
					efa->e3= findedgelist(efa->v3, efa->v4);
					efa->e4= findedgelist(efa->v4, efa->v1);
				}
			}
		}
		efa= nextvl;
	}

	/* double faces: sort block */
	/* count again, now all selected faces */
	amount= 0;
	efa= em->faces.first;
	while(efa) {
		efa->f1= 0;
		if(faceselectedOR(efa, 1)) {
			efa->f1= 1;
			amount++;
		}
		efa= efa->next;
	}

	if(amount) {
		/* double faces: sort block */
		vsb= vlsortblock= MEM_mallocN(sizeof(struct facesort)*amount, "sortremovedoub");
		efa= em->faces.first;
		while(efa) {
			if(efa->f1 & 1) {
				if(efa->v4) vsb->x= (unsigned long) MIN4( (unsigned long)efa->v1, (unsigned long)efa->v2, (unsigned long)efa->v3, (unsigned long)efa->v4);
				else vsb->x= (unsigned long) MIN3( (unsigned long)efa->v1, (unsigned long)efa->v2, (unsigned long)efa->v3);

				vsb->efa= efa;
				vsb++;
			}
			efa= efa->next;
		}
		
		qsort(vlsortblock, amount, sizeof(struct facesort), vergface);
			
		vsb= vlsortblock;
		for(a=0; a<amount; a++) {
			efa= vsb->efa;
			if( (efa->f1 & 128)==0 ) {
				vsb1= vsb+1;

				for(b=a+1; b<amount; b++) {
				
					/* first test: same pointer? */
					if(vsb->x != vsb1->x) break;
					
					/* second test: is test permitted? */
					efa= vsb1->efa;
					if( (efa->f1 & 128)==0 ) {
						if( compareface(efa, vsb->efa)) efa->f1 |= 128;
						
					}
					vsb1++;
				}
			}
			vsb++;
		}
		
		MEM_freeN(vlsortblock);
		
		/* remove double faces */
		efa= (struct EditFace *)em->faces.first;
		while(efa) {
			nextvl= efa->next;
			if(efa->f1 & 128) {
				BLI_remlink(&em->faces, efa);
				free_editface(efa);
			}
			efa= nextvl;
		}
	}
	
	/* remove double vertices */
	a= 0;
	eve= (struct EditVert *)em->verts.first;
	while(eve) {
		nextve= eve->next;
		if(automerge || eve->f & flag) {
			if(eve->f & 128) {
				a++;
				BLI_remlink(&em->verts, eve);
				free_editvert(eve);
			}
		}
		eve= nextve;
	}

#ifdef WITH_VERSE
	if((a>0) && (G.editMesh->vnode)) {
		sync_all_verseverts_with_editverts((VNode*)G.editMesh->vnode);
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
	}
#endif

	return a;	/* amount */
}

/* called from buttons */
static void xsortvert_flag__doSetX(void *userData, EditVert *eve, int x, int y, int index)
{
	xvertsort *sortblock = userData;

	sortblock[index].x = x;
}
void xsortvert_flag(int flag)
{
	EditMesh *em = G.editMesh;
	/* all verts with (flag & 'flag') are sorted */
	EditVert *eve;
	xvertsort *sortblock;
	ListBase tbase;
	int i, amount = BLI_countlist(&em->verts);
	
	if(multires_test()) return;
	
	sortblock = MEM_callocN(sizeof(xvertsort)*amount,"xsort");
	for (i=0,eve=em->verts.first; eve; i++,eve=eve->next)
		if(eve->f & flag)
			sortblock[i].v1 = eve;
	mesh_foreachScreenVert(xsortvert_flag__doSetX, sortblock, 0);
	qsort(sortblock, amount, sizeof(xvertsort), vergxco);
	
		/* make temporal listbase */
	tbase.first= tbase.last= 0;
	for (i=0; i<amount; i++) {
		eve = sortblock[i].v1;

		if (eve) {
			BLI_remlink(&em->verts, eve);
			BLI_addtail(&tbase, eve);
		}
	}
	
	addlisttolist(&em->verts, &tbase);
	
	MEM_freeN(sortblock);

#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
#endif

	BIF_undo_push("Xsort");
	
}

/* called from buttons */
void hashvert_flag(int flag)
{
	/* switch vertex order using hash table */
	EditMesh *em = G.editMesh;
	EditVert *eve;
	struct xvertsort *sortblock, *sb, onth, *newsort;
	ListBase tbase;
	int amount, a, b;
	
	if(multires_test()) return;
	
	/* count */
	eve= em->verts.first;
	amount= 0;
	while(eve) {
		if(eve->f & flag) amount++;
		eve= eve->next;
	}
	if(amount==0) return;
	
	/* allocate memory */
	sb= sortblock= (struct xvertsort *)MEM_mallocN(sizeof(struct xvertsort)*amount,"sortremovedoub");
	eve= em->verts.first;
	while(eve) {
		if(eve->f & flag) {
			sb->v1= eve;
			sb++;
		}
		eve= eve->next;
	}

	BLI_srand(1);
	
	sb= sortblock;
	for(a=0; a<amount; a++, sb++) {
		b= (int)(amount*BLI_drand());
		if(b>=0 && b<amount) {
			newsort= sortblock+b;
			onth= *sb;
			*sb= *newsort;
			*newsort= onth;
		}
	}

	/* make temporal listbase */
	tbase.first= tbase.last= 0;
	sb= sortblock;
	while(amount--) {
		eve= sb->v1;
		BLI_remlink(&em->verts, eve);
		BLI_addtail(&tbase, eve);
		sb++;
	}
	
	addlisttolist(&em->verts, &tbase);
	
	MEM_freeN(sortblock);
#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
#endif
	BIF_undo_push("Hash");

}

/* generic extern called extruder */
void extrude_mesh(void)
{
	float nor[3]= {0.0, 0.0, 0.0};
	short nr, transmode= 0;

	TEST_EDITMESH
	if(multires_test()) return;
	
	if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		if(G.totvertsel==0) nr= 0;
		else if(G.totvertsel==1) nr= 4;
		else if(G.totedgesel==0) nr= 4;
		else if(G.totfacesel==0) 
			nr= pupmenu("Extrude %t|Only Edges%x3|Only Vertices%x4");
		else if(G.totfacesel==1)
			nr= pupmenu("Extrude %t|Region %x1|Only Edges%x3|Only Vertices%x4");
		else 
			nr= pupmenu("Extrude %t|Region %x1||Individual Faces %x2|Only Edges%x3|Only Vertices%x4");
	}
	else if(G.scene->selectmode & SCE_SELECT_EDGE) {
		if (G.totedgesel==0) nr = 0;
		else if (G.totedgesel==1) nr = 3;
		else if(G.totfacesel==0) nr = 3;
		else if(G.totfacesel==1)
			nr= pupmenu("Extrude %t|Region %x1|Only Edges%x3");
		else
			nr= pupmenu("Extrude %t|Region %x1||Individual Faces %x2|Only Edges%x3");
	}
	else {
		if (G.totfacesel == 0) nr = 0;
		else if (G.totfacesel == 1) nr = 1;
		else
			nr= pupmenu("Extrude %t|Region %x1||Individual Faces %x2");
	}
		
	if(nr<1) return;

	if(nr==1)  transmode= extrudeflag(SELECT, nor);
	else if(nr==4) transmode= extrudeflag_verts_indiv(SELECT, nor);
	else if(nr==3) transmode= extrudeflag_edges_indiv(SELECT, nor);
	else transmode= extrudeflag_face_indiv(SELECT, nor);
	
	if(transmode==0) {
		error("No valid selection");
	}
	else {
		EM_fgon_flags();
		countall(); 
		
			/* We need to force immediate calculation here because 
			* transform may use derived objects (which are now stale).
			*
			* This shouldn't be necessary, derived queries should be
			* automatically building this data if invalid. Or something.
			*/
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
		object_handle_update(G.obedit);

		/* individual faces? */
		BIF_TransformSetUndo("Extrude");
		if(nr==2) {
			initTransform(TFM_SHRINKFATTEN, CTX_NO_PET|CTX_NO_MIRROR);
			Transform();
		}
		else {
			initTransform(TFM_TRANSLATION, CTX_NO_PET|CTX_NO_MIRROR);
			if(transmode=='n') {
				Mat4MulVecfl(G.obedit->obmat, nor);
				VecSubf(nor, nor, G.obedit->obmat[3]);
				BIF_setSingleAxisConstraint(nor, "along normal");
			}
			Transform();
		}
	}

}

void split_mesh(void)
{

	TEST_EDITMESH
	if(multires_test()) return;

	if(okee(" Split ")==0) return;

	waitcursor(1);

	/* make duplicate first */
	adduplicateflag(SELECT);
	/* old faces have flag 128 set, delete them */
	delfaceflag(128);
	recalc_editnormals();

	waitcursor(0);

	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
#endif

	BIF_undo_push("Split");

}

void extrude_repeat_mesh(int steps, float offs)
{
	float dvec[3], tmat[3][3], bmat[3][3], nor[3]= {0.0, 0.0, 0.0};
	short a;

	TEST_EDITMESH
	if(multires_test()) return;
	
	/* dvec */
	dvec[0]= G.vd->persinv[2][0];
	dvec[1]= G.vd->persinv[2][1];
	dvec[2]= G.vd->persinv[2][2];
	Normalize(dvec);
	dvec[0]*= offs;
	dvec[1]*= offs;
	dvec[2]*= offs;

	/* base correction */
	Mat3CpyMat4(bmat, G.obedit->obmat);
	Mat3Inv(tmat, bmat);
	Mat3MulVecfl(tmat, dvec);

	for(a=0; a<steps; a++) {
		extrudeflag(SELECT, nor);
		translateflag(SELECT, dvec);
	}
	
	recalc_editnormals();
	
	EM_fgon_flags();
	countall();
	
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

	BIF_undo_push("Extrude Repeat");
}

void spin_mesh(int steps, float degr, float *dvec, int mode)
{
	EditMesh *em = G.editMesh;
	EditVert *eve,*nextve;
	float nor[3]= {0.0, 0.0, 0.0};
	float *curs, si,n[3],q[4],cmat[3][3],imat[3][3], tmat[3][3];
	float cent[3],bmat[3][3];
	float phi;
	short a,ok;

	TEST_EDITMESH
	if(multires_test()) return;
	
	/* imat and center and size */
	Mat3CpyMat4(bmat, G.obedit->obmat);
	Mat3Inv(imat,bmat);

	curs= give_cursor();
	VECCOPY(cent, curs);
	cent[0]-= G.obedit->obmat[3][0];
	cent[1]-= G.obedit->obmat[3][1];
	cent[2]-= G.obedit->obmat[3][2];
	Mat3MulVecfl(imat, cent);

	phi= degr*M_PI/360.0;
	phi/= steps;
	if(G.scene->toolsettings->editbutflag & B_CLOCKWISE) phi= -phi;

	if(dvec) {
		n[0]= G.vd->viewinv[1][0];
		n[1]= G.vd->viewinv[1][1];
		n[2]= G.vd->viewinv[1][2];
	} else {
		n[0]= G.vd->viewinv[2][0];
		n[1]= G.vd->viewinv[2][1];
		n[2]= G.vd->viewinv[2][2];
	}
	Normalize(n);

	q[0]= (float)cos(phi);
	si= (float)sin(phi);
	q[1]= n[0]*si;
	q[2]= n[1]*si;
	q[3]= n[2]*si;
	QuatToMat3(q, cmat);

	Mat3MulMat3(tmat,cmat,bmat);
	Mat3MulMat3(bmat,imat,tmat);

	if(mode==0) if(G.scene->toolsettings->editbutflag & B_KEEPORIG) adduplicateflag(1);
	ok= 1;

	for(a=0;a<steps;a++) {
		if(mode==0) ok= extrudeflag(SELECT, nor);
		else adduplicateflag(SELECT);
		if(ok==0) {
			error("No valid vertices are selected");
			break;
		}
		rotateflag(SELECT, cent, bmat);
		if(dvec) {
			Mat3MulVecfl(bmat,dvec);
			translateflag(SELECT, dvec);
		}
	}

	if(ok==0) {
		/* no vertices or only loose ones selected, remove duplicates */
		eve= em->verts.first;
		while(eve) {
			nextve= eve->next;
			if(eve->f & SELECT) {
				BLI_remlink(&em->verts,eve);
				free_editvert(eve);
			}
			eve= nextve;
		}
	}
	recalc_editnormals();

	EM_fgon_flags();
	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

	
	if(dvec==NULL) BIF_undo_push("Spin");
}

void screw_mesh(int steps, int turns)
{
	EditMesh *em = G.editMesh;
	EditVert *eve,*v1=0,*v2=0;
	EditEdge *eed;
	float dvec[3], nor[3];

	TEST_EDITMESH
	if(multires_test()) return;
	
	/* clear flags */
	eve= em->verts.first;
	while(eve) {
		eve->f1= 0;
		eve= eve->next;
	}
	/* edges set flags in verts */
	eed= em->edges.first;
	while(eed) {
		if(eed->v1->f & SELECT) {
			if(eed->v2->f & SELECT) {
				/* watch: f1 is a byte */
				if(eed->v1->f1<2) eed->v1->f1++;
				if(eed->v2->f1<2) eed->v2->f1++;
			}
		}
		eed= eed->next;
	}
	/* find two vertices with eve->f1==1, more or less is wrong */
	eve= em->verts.first;
	while(eve) {
		if(eve->f1==1) {
			if(v1==0) v1= eve;
			else if(v2==0) v2= eve;
			else {
				v1=0;
				break;
			}
		}
		eve= eve->next;
	}
	if(v1==0 || v2==0) {
		error("You have to select a string of connected vertices too");
		return;
	}

	/* calculate dvec */
	dvec[0]= ( (v1->co[0]- v2->co[0]) )/(steps);
	dvec[1]= ( (v1->co[1]- v2->co[1]) )/(steps);
	dvec[2]= ( (v1->co[2]- v2->co[2]) )/(steps);

	VECCOPY(nor, G.obedit->obmat[2]);

	if(nor[0]*dvec[0]+nor[1]*dvec[1]+nor[2]*dvec[2]>0.000) {
		dvec[0]= -dvec[0];
		dvec[1]= -dvec[1];
		dvec[2]= -dvec[2];
	}

	spin_mesh(turns*steps, turns*360, dvec, 0);

	BIF_undo_push("Spin");
}


static void erase_edges(ListBase *l)
{
	EditEdge *ed, *nexted;
	
	ed = (EditEdge *) l->first;
	while(ed) {
		nexted= ed->next;
		if( (ed->v1->f & SELECT) || (ed->v2->f & SELECT) ) {
			remedge(ed);
			free_editedge(ed);
		}
		ed= nexted;
	}
}

static void erase_faces(ListBase *l)
{
	EditFace *f, *nextf;

	f = (EditFace *) l->first;

	while(f) {
		nextf= f->next;
		if( faceselectedOR(f, SELECT) ) {
			BLI_remlink(l, f);
			free_editface(f);
		}
		f = nextf;
	}
}	

static void erase_vertices(ListBase *l)
{
	EditVert *v, *nextv;

	v = (EditVert *) l->first;
	while(v) {
		nextv= v->next;
		if(v->f & 1) {
			BLI_remlink(l, v);
			free_editvert(v);
		}
		v = nextv;
	}
}

void delete_mesh(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa, *nextvl;
	EditVert *eve,*nextve;
	EditEdge *eed,*nexted;
	short event;
	int count;
	char *str="Erase";

	TEST_EDITMESH
	if(multires_test()) return;
	
	event= pupmenu("Erase %t|Vertices%x10|Edges%x1|Faces%x2|All%x3|Edges & Faces%x4|Only Faces%x5|Edge Loop%x6");
	if(event<1) return;

	if(event==10 ) {
		str= "Erase Vertices";
		erase_edges(&em->edges);
		erase_faces(&em->faces);
		erase_vertices(&em->verts);
	} 
	else if(event==6) {
		if(!EdgeLoopDelete())
			return;

		str= "Erase Edge Loop";
	}
	else if(event==4) {
		str= "Erase Edges & Faces";
		efa= em->faces.first;
		while(efa) {
			nextvl= efa->next;
			/* delete only faces with 1 or more edges selected */
			count= 0;
			if(efa->e1->f & SELECT) count++;
			if(efa->e2->f & SELECT) count++;
			if(efa->e3->f & SELECT) count++;
			if(efa->e4 && (efa->e4->f & SELECT)) count++;
			if(count) {
				BLI_remlink(&em->faces, efa);
				free_editface(efa);
			}
			efa= nextvl;
		}
		eed= em->edges.first;
		while(eed) {
			nexted= eed->next;
			if(eed->f & SELECT) {
				remedge(eed);
				free_editedge(eed);
			}
			eed= nexted;
		}
		efa= em->faces.first;
		while(efa) {
			nextvl= efa->next;
			event=0;
			if( efa->v1->f & SELECT) event++;
			if( efa->v2->f & SELECT) event++;
			if( efa->v3->f & SELECT) event++;
			if(efa->v4 && (efa->v4->f & SELECT)) event++;
			
			if(event>1) {
				BLI_remlink(&em->faces, efa);
				free_editface(efa);
			}
			efa= nextvl;
		}
	} 
	else if(event==1) {
		str= "Erase Edges";
		// faces first
		efa= em->faces.first;
		while(efa) {
			nextvl= efa->next;
			event=0;
			if( efa->e1->f & SELECT) event++;
			if( efa->e2->f & SELECT) event++;
			if( efa->e3->f & SELECT) event++;
			if(efa->e4 && (efa->e4->f & SELECT)) event++;
			
			if(event) {
				BLI_remlink(&em->faces, efa);
				free_editface(efa);
			}
			efa= nextvl;
		}
		eed= em->edges.first;
		while(eed) {
			nexted= eed->next;
			if(eed->f & SELECT) {
				remedge(eed);
				free_editedge(eed);
			}
			eed= nexted;
		}
		/* to remove loose vertices: */
		eed= em->edges.first;
		while(eed) {
			if( eed->v1->f & SELECT) eed->v1->f-=SELECT;
			if( eed->v2->f & SELECT) eed->v2->f-=SELECT;
			eed= eed->next;
		}
		eve= em->verts.first;
		while(eve) {
			nextve= eve->next;
			if(eve->f & SELECT) {
				BLI_remlink(&em->verts,eve);
				free_editvert(eve);
			}
			eve= nextve;
		}

	}
	else if(event==2) {
		str="Erase Faces";
		delfaceflag(SELECT);
	}
	else if(event==3) {
		str= "Erase All";
		if(em->verts.first) free_vertlist(&em->verts);
		if(em->edges.first) free_edgelist(&em->edges);
		if(em->faces.first) free_facelist(&em->faces);
		if(em->selected.first) BLI_freelistN(&(em->selected));
	}
	else if(event==5) {
		str= "Erase Only Faces";
		efa= em->faces.first;
		while(efa) {
			nextvl= efa->next;
			if(efa->f & SELECT) {
				BLI_remlink(&em->faces, efa);
				free_editface(efa);
			}
			efa= nextvl;
		}
	}

	EM_fgon_flags();	// redo flags and indices for fgons

	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	BIF_undo_push(str);
}


/* Got this from scanfill.c. You will need to juggle around the
 * callbacks for the scanfill.c code a bit for this to work. */
void fill_mesh(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve,*v1;
	EditEdge *eed,*e1,*nexted;
	EditFace *efa,*nextvl, *efan;
	short ok;

	if(G.obedit==0 || (G.obedit->type!=OB_MESH)) return;
	if(multires_test()) return;

	waitcursor(1);

	/* copy all selected vertices */
	eve= em->verts.first;
	while(eve) {
		if(eve->f & SELECT) {
			v1= BLI_addfillvert(eve->co);
			eve->tmp.v= v1;
			v1->tmp.v= eve;
			v1->xs= 0;	// used for counting edges
		}
		eve= eve->next;
	}
	/* copy all selected edges */
	eed= em->edges.first;
	while(eed) {
		if( (eed->v1->f & SELECT) && (eed->v2->f & SELECT) ) {
			e1= BLI_addfilledge(eed->v1->tmp.v, eed->v2->tmp.v);
			e1->v1->xs++; 
			e1->v2->xs++;
		}
		eed= eed->next;
	}
	/* from all selected faces: remove vertices and edges to prevent doubles */
	/* all edges add values, faces subtract,
	   then remove edges with vertices ->xs<2 */
	efa= em->faces.first;
	ok= 0;
	while(efa) {
		nextvl= efa->next;
		if( faceselectedAND(efa, 1) ) {
			efa->v1->tmp.v->xs--;
			efa->v2->tmp.v->xs--;
			efa->v3->tmp.v->xs--;
			if(efa->v4) efa->v4->tmp.v->xs--;
			ok= 1;
			
		}
		efa= nextvl;
	}
	if(ok) {	/* there are faces selected */
		eed= filledgebase.first;
		while(eed) {
			nexted= eed->next;
			if(eed->v1->xs<2 || eed->v2->xs<2) {
				BLI_remlink(&filledgebase,eed);
			}
			eed= nexted;
		}
	}

	if(BLI_edgefill(0, (G.obedit && G.obedit->actcol)?(G.obedit->actcol-1):0)) {
		efa= fillfacebase.first;
		while(efa) {
			/* normals default pointing up */
			efan= addfacelist(efa->v3->tmp.v, efa->v2->tmp.v, 
							  efa->v1->tmp.v, 0, NULL, NULL);
			if(efan) EM_select_face(efan, 1);
			efa= efa->next;
		}
	}

	BLI_end_edgefill();

	waitcursor(0);
	EM_select_flush();
	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
#endif

	BIF_undo_push("Fill");
}
/*GB*/
/*-------------------------------------------------------------------------------*/
/*--------------------------- Edge Based Subdivide ------------------------------*/

#define EDGENEW	2
#define FACENEW	2
#define EDGEINNER  4
#define EDGEOLD  8

/*used by faceloop cut to select only edges valid for edge slide*/
#define DOUBLEOPFILL 16

/* calculates offset for co, based on fractal, sphere or smooth settings  */
static void alter_co(float *co, EditEdge *edge, float rad, int beauty, float perc)
{
	float vec1[3], fac;
	
	if(beauty & B_SMOOTH) {
		/* we calculate an offset vector vec1[], to be added to *co */
		float len, fac, nor[3], nor1[3], nor2[3];
		
		VecSubf(nor, edge->v1->co, edge->v2->co);
		len= 0.5f*Normalize(nor);
	
		VECCOPY(nor1, edge->v1->no);
		VECCOPY(nor2, edge->v2->no);
	
		/* cosine angle */
		fac= nor[0]*nor1[0] + nor[1]*nor1[1] + nor[2]*nor1[2] ;
		
		vec1[0]= fac*nor1[0];
		vec1[1]= fac*nor1[1];
		vec1[2]= fac*nor1[2];
	
		/* cosine angle */
		fac= -nor[0]*nor2[0] - nor[1]*nor2[1] - nor[2]*nor2[2] ;
		
		vec1[0]+= fac*nor2[0];
		vec1[1]+= fac*nor2[1];
		vec1[2]+= fac*nor2[2];
		
		vec1[0]*= rad*len;
		vec1[1]*= rad*len;
		vec1[2]*= rad*len;
		
		co[0] += vec1[0];
		co[1] += vec1[1];
		co[2] += vec1[2];
	}
	else {
		if(rad > 0.0) {   /* subdivide sphere */
			Normalize(co);
			co[0]*= rad;
			co[1]*= rad;
			co[2]*= rad;
		}
		else if(rad< 0.0) {  /* fractal subdivide */
			fac= rad* VecLenf(edge->v1->co, edge->v2->co);
			vec1[0]= fac*(float)(0.5-BLI_drand());
			vec1[1]= fac*(float)(0.5-BLI_drand());
			vec1[2]= fac*(float)(0.5-BLI_drand());
			VecAddf(co, co, vec1);
		}

	}
}

/* assumes in the edge is the correct interpolated vertices already */
/* percent defines the interpolation, rad and beauty are for special options */
/* results in new vertex with correct coordinate, vertex normal and weight group info */
static EditVert *subdivide_edge_addvert(EditEdge *edge, float rad, int beauty, float percent)
{
	EditVert *ev;
	float co[3];
	
	co[0] = (edge->v2->co[0]-edge->v1->co[0])*percent + edge->v1->co[0];
	co[1] = (edge->v2->co[1]-edge->v1->co[1])*percent + edge->v1->co[1];
	co[2] = (edge->v2->co[2]-edge->v1->co[2])*percent + edge->v1->co[2];					
	
	/* offset for smooth or sphere or fractal */
	alter_co(co, edge, rad, beauty, percent);
	
	/* clip if needed by mirror modifier */
	if (edge->v1->f2) {
		if ( edge->v1->f2 & edge->v2->f2 & 1) {
			co[0]= 0.0f;
		}
		if ( edge->v1->f2 & edge->v2->f2 & 2) {
			co[1]= 0.0f;
		}
		if ( edge->v1->f2 & edge->v2->f2 & 4) {
			co[2]= 0.0f;
		}
	}
	
	ev = addvertlist(co, NULL);
	
	/* vert data (vgroups, ..) */
	EM_data_interp_from_verts(edge->v1, edge->v2, ev, percent);
	
	/* normal */
	ev->no[0] = (edge->v2->no[0]-edge->v1->no[0])*percent + edge->v1->no[0];
	ev->no[1] = (edge->v2->no[1]-edge->v1->no[1])*percent + edge->v1->no[1];
	ev->no[2] = (edge->v2->no[2]-edge->v1->no[2])*percent + edge->v1->no[2];
	Normalize(ev->no);
	
	return ev;
}

static void flipvertarray(EditVert** arr, short size)
{
	EditVert *hold;
	int i;
	
	for(i=0; i<size/2; i++) {
		hold = arr[i];
		arr[i] = arr[size-i-1];
		arr[size-i-1] = hold;   
	}
}

static void facecopy(EditFace *source, EditFace *target)
{
	EditMesh *em= G.editMesh;
	float *v1 = source->v1->co, *v2 = source->v2->co, *v3 = source->v3->co;
	float *v4 = source->v4? source->v4->co: NULL;
	float w[4][4];

	CustomData_em_copy_data(&em->fdata, &em->fdata, source->data, &target->data);

	target->mat_nr = source->mat_nr;
	target->flag   = source->flag;	
	target->h	   = source->h;
	
	InterpWeightsQ3Dfl(v1, v2, v3, v4, target->v1->co, w[0]);
	InterpWeightsQ3Dfl(v1, v2, v3, v4, target->v2->co, w[1]);
	InterpWeightsQ3Dfl(v1, v2, v3, v4, target->v3->co, w[2]);
	if (target->v4)
		InterpWeightsQ3Dfl(v1, v2, v3, v4, target->v4->co, w[3]);
	
	CustomData_em_interp(&em->fdata, &source->data, NULL, (float*)w, 1, target->data);
}

static void fill_quad_single(EditFace *efa, struct GHash *gh, int numcuts, int seltype)
{
	EditEdge *cedge=NULL;
	EditVert *v[4], **verts;
	EditFace *hold;
	short start=0, end, left, right, vertsize,i;   
							
	v[0] = efa->v1;
	v[1] = efa->v2;
	v[2] = efa->v3;
	v[3] = efa->v4;	 

	if(efa->e1->f & SELECT)	  { cedge = efa->e1; start = 0;}
	else if(efa->e2->f & SELECT) { cedge = efa->e2; start = 1;}	   
	else if(efa->e3->f & SELECT) { cedge = efa->e3; start = 2;}	   
	else if(efa->e4->f & SELECT) { cedge = efa->e4; start = 3;}		 

	// Point verts to the array of new verts for cedge
	verts = BLI_ghash_lookup(gh, cedge);
	//This is the index size of the verts array
	vertsize = numcuts+2;

	// Is the original v1 the same as the first vert on the selected edge?
	// if not, the edge is running the opposite direction in this face so flip
	// the array to the correct direction

	if(verts[0] != v[start]) {flipvertarray(verts,numcuts+2);}
	end	= (start+1)%4;
	left   = (start+2)%4;
	right  = (start+3)%4; 
		   
	/*
	We should have something like this now

			  end		 start				 
			   3   2   1   0   
			   |---*---*---|
			   |		   |
			   |		   |	   
			   |		   |
			   -------------	   
			  left	   right

	where start,end,left, right are indexes of EditFace->v1, etc (stored in v)
	and 0,1,2... are the indexes of the new verts stored in verts

	We will fill this case like this or this depending on even or odd cuts
	 
			   |---*---*---|		  |---*---|
			   |  /	 \  |		  |  / \  |
			   | /	   \ |		  | /   \ |	 
			   |/		 \|		  |/	 \|
			   -------------		  ---------  
	*/

	// Make center face
	if(vertsize % 2 == 0) {
		hold = addfacelist(verts[(vertsize-1)/2],verts[((vertsize-1)/2)+1],v[left],v[right], NULL,NULL);
		hold->e2->f2 |= EDGEINNER;
		hold->e4->f2 |= EDGEINNER;
	}else{
		hold = addfacelist(verts[(vertsize-1)/2],v[left],v[right],NULL, NULL,NULL);  
		hold->e1->f2 |= EDGEINNER;
		hold->e3->f2 |= EDGEINNER;
	}
	facecopy(efa,hold);

	// Make side faces
	for(i=0;i<(vertsize-1)/2;i++) {
		hold = addfacelist(verts[i],verts[i+1],v[right],NULL,NULL,NULL);  
		facecopy(efa,hold);
		if(i+1 != (vertsize-1)/2) {
            if(seltype == SUBDIV_SELECT_INNER) {
	 		   hold->e2->f2 |= EDGEINNER;
            }
		}
		hold = addfacelist(verts[vertsize-2-i],verts[vertsize-1-i],v[left],NULL,NULL,NULL); 
		facecopy(efa,hold);
		if(i+1 != (vertsize-1)/2) {
            if(seltype == SUBDIV_SELECT_INNER) {
		 		hold->e3->f2 |= EDGEINNER;
            }
		}
	}	 
}

static void fill_tri_single(EditFace *efa, struct GHash *gh, int numcuts, int seltype)
{
	EditEdge *cedge=NULL;
	EditVert *v[3], **verts;
	EditFace *hold;
	short start=0, end, op, vertsize,i;   
							
	v[0] = efa->v1;
	v[1] = efa->v2;
	v[2] = efa->v3;	

	if(efa->e1->f & SELECT)	  { cedge = efa->e1; start = 0;}
	else if(efa->e2->f & SELECT) { cedge = efa->e2; start = 1;}	   
	else if(efa->e3->f & SELECT) { cedge = efa->e3; start = 2;}		 

	// Point verts to the array of new verts for cedge
	verts = BLI_ghash_lookup(gh, cedge);
	//This is the index size of the verts array
	vertsize = numcuts+2;

	// Is the original v1 the same as the first vert on the selected edge?
	// if not, the edge is running the opposite direction in this face so flip
	// the array to the correct direction

	if(verts[0] != v[start]) {flipvertarray(verts,numcuts+2);}
	   end	= (start+1)%3;
	   op	 = (start+2)%3;
		   
	/*
	We should have something like this now

			  end		 start				 
			   3   2   1   0   
			   |---*---*---|
			   \		   |
				 \		 |	   
				   \	   |
					 \	 |
					   \   |
						 \ |
						   |op
						   
	where start,end,op are indexes of EditFace->v1, etc (stored in v)
	and 0,1,2... are the indexes of the new verts stored in verts

	We will fill this case like this or this depending on even or odd cuts
	 
			   3   2   1   0   
			   |---*---*---|
			   \	\  \   |
				 \	\ \  |	   
				   \   \ \ |
					 \  \ \|
					   \ \\|
						 \ |
						   |op
	*/

	// Make side faces
	for(i=0;i<(vertsize-1);i++) {
		hold = addfacelist(verts[i],verts[i+1],v[op],NULL,NULL,NULL);  
		if(i+1 != vertsize-1) {
            if(seltype == SUBDIV_SELECT_INNER) {
		 		hold->e2->f2 |= EDGEINNER;
            }
		}
		facecopy(efa,hold);
	}	  
}

static void fill_quad_double_op(EditFace *efa, struct GHash *gh, int numcuts)
{
	EditEdge *cedge[2]={NULL, NULL};
	EditVert *v[4], **verts[2];
	EditFace *hold;
	short start=0, end, left, right, vertsize,i;
							
	v[0] = efa->v1;
	v[1] = efa->v2;
	v[2] = efa->v3;
	v[3] = efa->v4;	 

	if(efa->e1->f & SELECT)	  { cedge[0] = efa->e1;  cedge[1] = efa->e3; start = 0;}
	else if(efa->e2->f & SELECT)	  { cedge[0] = efa->e2;  cedge[1] = efa->e4; start = 1;}

	// Point verts[0] and [1] to the array of new verts for cedge[0] and cedge[1]
	verts[0] = BLI_ghash_lookup(gh, cedge[0]);
	verts[1] = BLI_ghash_lookup(gh, cedge[1]);
	//This is the index size of the verts array
	vertsize = numcuts+2;

	// Is the original v1 the same as the first vert on the selected edge?
	// if not, the edge is running the opposite direction in this face so flip
	// the array to the correct direction

	if(verts[0][0] != v[start]) {flipvertarray(verts[0],numcuts+2);}
	end	= (start+1)%4;
	left   = (start+2)%4;
	right  = (start+3)%4; 
	if(verts[1][0] != v[left]) {flipvertarray(verts[1],numcuts+2);}	
	/*
	We should have something like this now

			  end		 start				 
			   3   2   1   0   
			   |---*---*---|
			   |		   |
			   |		   |	   
			   |		   |
			   |---*---*---|	  
			   0   1   2   3
			  left	   right

	We will fill this case like this or this depending on even or odd cuts
	 
			   |---*---*---|
			   |   |   |   |
			   |   |   |   |	   
			   |   |   |   |
			   |---*---*---| 
	*/
	   
	// Make side faces
	for(i=0;i<vertsize-1;i++) {
		hold = addfacelist(verts[0][i],verts[0][i+1],verts[1][vertsize-2-i],verts[1][vertsize-1-i],NULL,NULL);  
		if(i < vertsize-2) {
			hold->e2->f2 |= EDGEINNER;
			hold->e2->f2 |= DOUBLEOPFILL;
		}
		facecopy(efa,hold);
	}	  
}

static void fill_quad_double_adj_path(EditFace *efa, struct GHash *gh, int numcuts)
{
	EditEdge *cedge[2]={NULL, NULL};
	EditVert *v[4], **verts[2];
	EditFace *hold;
	short start=0, start2=0, vertsize,i;
							
	v[0] = efa->v1;
	v[1] = efa->v2;
	v[2] = efa->v3;
	v[3] = efa->v4;	 

	if(efa->e1->f & SELECT && efa->e2->f & SELECT) {cedge[0] = efa->e1;  cedge[1] = efa->e2; start = 0; start2 = 1;}
	if(efa->e2->f & SELECT && efa->e3->f & SELECT) {cedge[0] = efa->e2;  cedge[1] = efa->e3; start = 1; start2 = 2;}
	if(efa->e3->f & SELECT && efa->e4->f & SELECT) {cedge[0] = efa->e3;  cedge[1] = efa->e4; start = 2; start2 = 3;}
	if(efa->e4->f & SELECT && efa->e1->f & SELECT) {cedge[0] = efa->e4;  cedge[1] = efa->e1; start = 3; start2 = 0;}

	// Point verts[0] and [1] to the array of new verts for cedge[0] and cedge[1]
	verts[0] = BLI_ghash_lookup(gh, cedge[0]);
	verts[1] = BLI_ghash_lookup(gh, cedge[1]);
	//This is the index size of the verts array
	vertsize = numcuts+2;

	// Is the original v1 the same as the first vert on the selected edge?
	// if not, the edge is running the opposite direction in this face so flip
	// the array to the correct direction

	if(verts[0][0] != v[start]) {flipvertarray(verts[0],numcuts+2);}
	if(verts[1][0] != v[start2]) {flipvertarray(verts[1],numcuts+2);}	
	/*
	We should have something like this now

			   end		 start				 
				3   2   1   0   
		start2 0|---*---*---|
				|		   |
			   1*		   |
				|		   |
			   2*		   |	   
				|		   |
		 end2  3|-----------|   

	We will fill this case like this or this depending on even or odd cuts
			   |---*---*---|
			   | /   /   / |
			   *   /   /   |
			   | /   /	 |
			   *   /	   |	   
			   | /		 |
			   |-----------|  
	*/

	// Make outside tris
	hold = addfacelist(verts[0][vertsize-2],verts[0][vertsize-1],verts[1][1],NULL,NULL,NULL);  
	/* when ctrl is depressed, only want verts on the cutline selected */
	if (G.qual  != LR_CTRLKEY)
		hold->e3->f2 |= EDGEINNER;
	facecopy(efa,hold);	   
	hold = addfacelist(verts[0][0],verts[1][vertsize-1],v[(start2+2)%4],NULL,NULL,NULL);
	/* when ctrl is depressed, only want verts on the cutline selected */
	if (G.qual  != LR_CTRLKEY)
		hold->e1->f2 |= EDGEINNER;  
	facecopy(efa,hold);			   
	//if(G.scene->toolsettings->editbutflag & B_AUTOFGON) {
	//	hold->e1->h |= EM_FGON;
	//}	
	// Make side faces

	for(i=0;i<numcuts;i++) {
		hold = addfacelist(verts[0][i],verts[0][i+1],verts[1][vertsize-1-(i+1)],verts[1][vertsize-1-i],NULL,NULL);  
		hold->e2->f2 |= EDGEINNER;
		facecopy(efa,hold);
	}
	//EM_fgon_flags();
		  
}

static void fill_quad_double_adj_fan(EditFace *efa, struct GHash *gh, int numcuts)
{
	EditEdge *cedge[2]={NULL, NULL};
	EditVert *v[4], *op=NULL, **verts[2];
	EditFace *hold;
	short start=0, start2=0, vertsize,i;
							
	v[0] = efa->v1;
	v[1] = efa->v2;
	v[2] = efa->v3;
	v[3] = efa->v4;	 

	if(efa->e1->f & SELECT && efa->e2->f & SELECT) {cedge[0] = efa->e1;  cedge[1] = efa->e2; start = 0; start2 = 1; op = efa->v4;}
	if(efa->e2->f & SELECT && efa->e3->f & SELECT) {cedge[0] = efa->e2;  cedge[1] = efa->e3; start = 1; start2 = 2; op = efa->v1;}
	if(efa->e3->f & SELECT && efa->e4->f & SELECT) {cedge[0] = efa->e3;  cedge[1] = efa->e4; start = 2; start2 = 3; op = efa->v2;}
	if(efa->e4->f & SELECT && efa->e1->f & SELECT) {cedge[0] = efa->e4;  cedge[1] = efa->e1; start = 3; start2 = 0; op = efa->v3;}

	
	// Point verts[0] and [1] to the array of new verts for cedge[0] and cedge[1]
	verts[0] = BLI_ghash_lookup(gh, cedge[0]);
	verts[1] = BLI_ghash_lookup(gh, cedge[1]);
	//This is the index size of the verts array
	vertsize = numcuts+2;

	// Is the original v1 the same as the first vert on the selected edge?
	// if not, the edge is running the opposite direction in this face so flip
	// the array to the correct direction

	if(verts[0][0] != v[start]) {flipvertarray(verts[0],numcuts+2);}
	if(verts[1][0] != v[start2]) {flipvertarray(verts[1],numcuts+2);}	
	/*
	We should have something like this now

			   end		 start				 
				3   2   1   0   
		start2 0|---*---*---|
				|		   |
			   1*		   |
				|		   |
			   2*		   |	   
				|		   |
		 end2  3|-----------|op   

	We will fill this case like this or this (warning horrible ascii art follows)
			   |---*---*---|
			   | \  \   \  |
			   *---\  \  \ |
			   |   \ \ \  \|
			   *---- \ \  \ |	   
			   |    ---  \\\|
			   |-----------|  
	*/

	for(i=0;i<=numcuts;i++) {
		hold = addfacelist(op,verts[1][numcuts-i],verts[1][numcuts-i+1],NULL,NULL,NULL);  
		hold->e1->f2 |= EDGEINNER;
		facecopy(efa,hold);

		hold = addfacelist(op,verts[0][i],verts[0][i+1],NULL,NULL,NULL);  
		hold->e3->f2 |= EDGEINNER;
		facecopy(efa,hold);
	}	  
}

static void fill_quad_double_adj_inner(EditFace *efa, struct GHash *gh, int numcuts)
{
	EditEdge *cedge[2]={NULL, NULL};
	EditVert *v[4], *op=NULL, **verts[2],**inner;
	EditFace *hold;
	short start=0, start2=0, vertsize,i;
	float co[3];
						
	v[0] = efa->v1;
	v[1] = efa->v2;
	v[2] = efa->v3;
	v[3] = efa->v4;	 

	if(efa->e1->f & SELECT && efa->e2->f & SELECT) {cedge[0] = efa->e1;  cedge[1] = efa->e2; start = 0; start2 = 1; op = efa->v4;}
	if(efa->e2->f & SELECT && efa->e3->f & SELECT) {cedge[0] = efa->e2;  cedge[1] = efa->e3; start = 1; start2 = 2; op = efa->v1;}
	if(efa->e3->f & SELECT && efa->e4->f & SELECT) {cedge[0] = efa->e3;  cedge[1] = efa->e4; start = 2; start2 = 3; op = efa->v2;}
	if(efa->e4->f & SELECT && efa->e1->f & SELECT) {cedge[0] = efa->e4;  cedge[1] = efa->e1; start = 3; start2 = 0; op = efa->v3;}

	
	// Point verts[0] and [1] to the array of new verts for cedge[0] and cedge[1]
	verts[0] = BLI_ghash_lookup(gh, cedge[0]);
	verts[1] = BLI_ghash_lookup(gh, cedge[1]);
	//This is the index size of the verts array
	vertsize = numcuts+2;

	// Is the original v1 the same as the first vert on the selected edge?
	// if not, the edge is running the opposite direction in this face so flip
	// the array to the correct direction

	if(verts[0][0] != v[start]) {flipvertarray(verts[0],numcuts+2);}
	if(verts[1][0] != v[start2]) {flipvertarray(verts[1],numcuts+2);}	
	/*
	We should have something like this now

			   end		 start				 
				3   2   1   0   
		start2 0|---*---*---|
				|		   |
			   1*		   |
				|		   |
			   2*		   |	   
				|		   |
		 end2  3|-----------|op   

	We will fill this case like this or this (warning horrible ascii art follows)
			   |---*-----*---|
			   | *     /     |
			   *   \ /       |
			   |    *        |
			   | /	  \	     |
			   *        \    |	   
			   |           \ |
			   |-------------|  
	*/

	// Add Inner Vert(s)
	inner = MEM_mallocN(sizeof(EditVert*)*numcuts,"New inner verts");
	
	for(i=0;i<numcuts;i++) {
		co[0] = (verts[0][numcuts-i]->co[0] + verts[1][i+1]->co[0] ) / 2 ;
		co[1] = (verts[0][numcuts-i]->co[1] + verts[1][i+1]->co[1] ) / 2 ;
		co[2] = (verts[0][numcuts-i]->co[2] + verts[1][i+1]->co[2] ) / 2 ;
		inner[i] = addvertlist(co, NULL);
		inner[i]->f2 |= EDGEINNER;

		EM_data_interp_from_verts(verts[0][numcuts-i], verts[1][i+1], inner[i], 0.5f);
	}
	
	// Add Corner Quad
	hold = addfacelist(verts[0][numcuts+1],verts[1][1],inner[0],verts[0][numcuts],NULL,NULL);  
	hold->e2->f2 |= EDGEINNER;
	hold->e3->f2 |= EDGEINNER;
	facecopy(efa,hold);	
	// Add Bottom Quads
	hold = addfacelist(verts[0][0],verts[0][1],inner[numcuts-1],op,NULL,NULL);  
	hold->e2->f2 |= EDGEINNER;
	facecopy(efa,hold);	

	hold = addfacelist(op,inner[numcuts-1],verts[1][numcuts],verts[1][numcuts+1],NULL,NULL);  
	hold->e2->f2 |= EDGEINNER;
	facecopy(efa,hold);		
	
	//if(G.scene->toolsettings->editbutflag & B_AUTOFGON) {
	//	hold->e1->h |= EM_FGON;
	//}	
	// Add Fill Quads (if # cuts > 1)

	for(i=0;i<numcuts-1;i++) {
		hold = addfacelist(inner[i],verts[1][i+1],verts[1][i+2],inner[i+1],NULL,NULL);  
		hold->e1->f2 |= EDGEINNER;
		hold->e3->f2 |= EDGEINNER;
		facecopy(efa,hold);

		hold = addfacelist(inner[i],inner[i+1],verts[0][numcuts-1-i],verts[0][numcuts-i],NULL,NULL);  
		hold->e2->f2 |= EDGEINNER;
		hold->e4->f2 |= EDGEINNER;
		facecopy(efa,hold);	
		
		//if(G.scene->toolsettings->editbutflag & B_AUTOFGON) {
		//	hold->e1->h |= EM_FGON;
		//}	
	}	
	
	//EM_fgon_flags();
	
	MEM_freeN(inner);  
}

static void fill_tri_double(EditFace *efa, struct GHash *gh, int numcuts)
{
	EditEdge *cedge[2]={NULL, NULL};
	EditVert *v[3], **verts[2];
	EditFace *hold;
	short start=0, start2=0, vertsize,i;
							
	v[0] = efa->v1;
	v[1] = efa->v2;
	v[2] = efa->v3;

	if(efa->e1->f & SELECT && efa->e2->f & SELECT) {cedge[0] = efa->e1;  cedge[1] = efa->e2; start = 0; start2 = 1;}
	if(efa->e2->f & SELECT && efa->e3->f & SELECT) {cedge[0] = efa->e2;  cedge[1] = efa->e3; start = 1; start2 = 2;}
	if(efa->e3->f & SELECT && efa->e1->f & SELECT) {cedge[0] = efa->e3;  cedge[1] = efa->e1; start = 2; start2 = 0;}

	// Point verts[0] and [1] to the array of new verts for cedge[0] and cedge[1]
	verts[0] = BLI_ghash_lookup(gh, cedge[0]);
	verts[1] = BLI_ghash_lookup(gh, cedge[1]);
	//This is the index size of the verts array
	vertsize = numcuts+2;

	// Is the original v1 the same as the first vert on the selected edge?
	// if not, the edge is running the opposite direction in this face so flip
	// the array to the correct direction

	if(verts[0][0] != v[start]) {flipvertarray(verts[0],numcuts+2);}
	if(verts[1][0] != v[start2]) {flipvertarray(verts[1],numcuts+2);}	
	/*
	We should have something like this now

			   end		 start				 
				3   2   1   0   
		start2 0|---*---*---|
				|		 /	 
			   1*	   /		
				|	 /		 
			   2*   /				 
				| /		   
		 end2  3|  

	We will fill this case like this or this depending on even or odd cuts
			   |---*---*---|
			   | /   /   / 
			   *   /   /   
			   | /   /	 
			   *   /			  
			   | /		 
			   |
	*/

	// Make outside tri
	hold = addfacelist(verts[0][vertsize-2],verts[0][vertsize-1],verts[1][1],NULL,NULL,NULL);  
	hold->e3->f2 |= EDGEINNER;
	facecopy(efa,hold);			  
	// Make side faces

	for(i=0;i<numcuts;i++) {
		hold = addfacelist(verts[0][i],verts[0][i+1],verts[1][vertsize-1-(i+1)],verts[1][vertsize-1-i],NULL,NULL);  
		hold->e2->f2 |= EDGEINNER;
		facecopy(efa,hold);
	}	  
}

static void fill_quad_triple(EditFace *efa, struct GHash *gh, int numcuts)
{
	EditEdge *cedge[3]={0};
	EditVert *v[4], **verts[3];
	EditFace *hold;
	short start=0, start2=0, start3=0, vertsize, i, repeats;
	
	v[0] = efa->v1;
	v[1] = efa->v2;
	v[2] = efa->v3;
	v[3] = efa->v4;	 
	   
	if(!(efa->e1->f & SELECT)) {
		cedge[0] = efa->e2;  
		cedge[1] = efa->e3; 
		cedge[2] = efa->e4;
		start = 1;start2 = 2;start3 = 3;   
	}
	if(!(efa->e2->f & SELECT)) {
		cedge[0] = efa->e3;  
		cedge[1] = efa->e4; 
		cedge[2] = efa->e1;
		start = 2;start2 = 3;start3 = 0;   
	}
	if(!(efa->e3->f & SELECT)) {
		cedge[0] = efa->e4;  
		cedge[1] = efa->e1; 
		cedge[2] = efa->e2;
		start = 3;start2 = 0;start3 = 1;   
	}
	if(!(efa->e4->f & SELECT)) {
		cedge[0] = efa->e1;  
		cedge[1] = efa->e2; 
		cedge[2] = efa->e3;
		start = 0;start2 = 1;start3 = 2;   
	}	   
	// Point verts[0] and [1] to the array of new verts for cedge[0] and cedge[1]
	verts[0] = BLI_ghash_lookup(gh, cedge[0]);
	verts[1] = BLI_ghash_lookup(gh, cedge[1]);
	verts[2] = BLI_ghash_lookup(gh, cedge[2]);
	//This is the index size of the verts array
	vertsize = numcuts+2;
	
	// Is the original v1 the same as the first vert on the selected edge?
	// if not, the edge is running the opposite direction in this face so flip
	// the array to the correct direction
	
	if(verts[0][0] != v[start]) {flipvertarray(verts[0],numcuts+2);}
	if(verts[1][0] != v[start2]) {flipvertarray(verts[1],numcuts+2);}  
	if(verts[2][0] != v[start3]) {flipvertarray(verts[2],numcuts+2);}   
	/*
	 We should have something like this now
	 
	 start2				 
	 3   2   1   0   
	 start3 0|---*---*---|3 
	 |		   |
	 1*		   *2
	 |		   |
	 2*		   *1	   
	 |		   |
	 3|-----------|0 start   
	 
	 We will fill this case like this or this depending on even or odd cuts  
	 there are a couple of differences. For odd cuts, there is a tri in the
	 middle as well as 1 quad at the bottom (not including the extra quads
	 for odd cuts > 1		  
	 
	 For even cuts, there is a quad in the middle and 2 quads on the bottom
	 
	 they are numbered here for clarity
	 
	 1 outer tris and bottom quads
	 2 inner tri or quad
	 3 repeating quads
	 
	 |---*---*---*---|
	 |1/   /  \   \ 1|
	 |/ 3 /	\  3 \|
	 *  /	2   \   *
	 | /		  \  |
	 |/			\ | 
	 *---------------*
	 |	  3		|
	 |			   |  
	 *---------------*
	 |			   |
	 |	  1		|				  
	 |			   |
	 |---------------|
	 
	 |---*---*---*---*---|
	 | 1/   /	 \   \ 1|   
	 | /   /	   \   \ |  
	 |/ 3 /		 \ 3 \|
	 *   /		   \   *
	 |  /			 \  |	
	 | /	   2	   \ |   
	 |/				 \|
	 *-------------------*
	 |				   |
	 |		 3		 |
	 |				   | 
	 *-------------------*
	 |				   |
	 |		 1		 |
	 |				   | 
	 *-------------------*
	 |				   |
	 |		1		  |
	 |				   | 
	 |-------------------|
	 
	 */

	// Make outside tris
	hold = addfacelist(verts[0][vertsize-2],verts[0][vertsize-1],verts[1][1],NULL,NULL,NULL);  
	hold->e3->f2 |= EDGEINNER;
	facecopy(efa,hold);	  
	hold = addfacelist(verts[1][vertsize-2],verts[1][vertsize-1],verts[2][1],NULL,NULL,NULL);  
	hold->e3->f2 |= EDGEINNER;
	facecopy(efa,hold);			  
	// Make bottom quad
	hold = addfacelist(verts[0][0],verts[0][1],verts[2][vertsize-2],verts[2][vertsize-1],NULL,NULL);  
	hold->e2->f2 |= EDGEINNER;
	facecopy(efa,hold);		 
	//If it is even cuts, add the 2nd lower quad
	if(numcuts % 2 == 0) {
		hold = addfacelist(verts[0][1],verts[0][2],verts[2][vertsize-3],verts[2][vertsize-2],NULL,NULL);  
		hold->e2->f2 |= EDGEINNER;
		facecopy(efa,hold);		 
		// Also Make inner quad
		hold = addfacelist(verts[1][numcuts/2],verts[1][(numcuts/2)+1],verts[2][numcuts/2],verts[0][(numcuts/2)+1],NULL,NULL);		   
		hold->e3->f2 |= EDGEINNER;
		//if(G.scene->toolsettings->editbutflag & B_AUTOFGON) {
		//	hold->e3->h |= EM_FGON;
		//}
		facecopy(efa,hold);
		repeats = (numcuts / 2) -1;
	} else {
		// Make inner tri	 
		hold = addfacelist(verts[1][(numcuts/2)+1],verts[2][(numcuts/2)+1],verts[0][(numcuts/2)+1],NULL,NULL,NULL);		   
		hold->e2->f2 |= EDGEINNER;
		//if(G.scene->toolsettings->editbutflag & B_AUTOFGON) {
		//	hold->e2->h |= EM_FGON;
		//}
		facecopy(efa,hold);   
		repeats = ((numcuts+1) / 2)-1;
	}
	
	// cuts for 1 and 2 do not have the repeating quads
	if(numcuts < 3) {repeats = 0;}
	for(i=0;i<repeats;i++) {
		//Make side repeating Quads
		hold = addfacelist(verts[1][i+1],verts[1][i+2],verts[0][vertsize-i-3],verts[0][vertsize-i-2],NULL,NULL);  
		hold->e2->f2 |= EDGEINNER;		 
		facecopy(efa,hold);			   
		hold = addfacelist(verts[1][vertsize-i-3],verts[1][vertsize-i-2],verts[2][i+1],verts[2][i+2],NULL,NULL);		   
		hold->e4->f2 |= EDGEINNER;
		facecopy(efa,hold); 
	}
	// Do repeating bottom quads 
	for(i=0;i<repeats;i++) {
		if(numcuts % 2 == 1) {	 
			hold = addfacelist(verts[0][1+i],verts[0][2+i],verts[2][vertsize-3-i],verts[2][vertsize-2-i],NULL,NULL);  
		} else {
			hold = addfacelist(verts[0][2+i],verts[0][3+i],verts[2][vertsize-4-i],verts[2][vertsize-3-i],NULL,NULL);				  
		}
		hold->e2->f2 |= EDGEINNER;
		facecopy(efa,hold);				
	}	
	//EM_fgon_flags();
}

static void fill_quad_quadruple(EditFace *efa, struct GHash *gh, int numcuts, float rad, int beauty)
{
	EditVert **verts[4], ***innerverts;
	EditFace *hold;	
	EditEdge temp;
	short vertsize, i, j;
	
	// Point verts[0] and [1] to the array of new verts for cedge[0] and cedge[1]
	verts[0] = BLI_ghash_lookup(gh, efa->e1);
	verts[1] = BLI_ghash_lookup(gh, efa->e2);
	verts[2] = BLI_ghash_lookup(gh, efa->e3);
	verts[3] = BLI_ghash_lookup(gh, efa->e4);

	//This is the index size of the verts array
	vertsize = numcuts+2;

	// Is the original v1 the same as the first vert on the selected edge?
	// if not, the edge is running the opposite direction in this face so flip
	// the array to the correct direction

	if(verts[0][0] != efa->v1) {flipvertarray(verts[0],numcuts+2);}
	if(verts[1][0] != efa->v2) {flipvertarray(verts[1],numcuts+2);}  
	if(verts[2][0] == efa->v3) {flipvertarray(verts[2],numcuts+2);}
	if(verts[3][0] == efa->v4) {flipvertarray(verts[3],numcuts+2);}	 
	/*
	We should have something like this now
					  1
										  
				3   2   1   0   
			   0|---*---*---|0 
				|           |
			   1*           *1
		     2  |           |   4
			   2*           *2	   
				|           |
			   3|---*---*---|3	
				3   2   1   0

					  3
	// we will fill a 2 dim array of editvert*s to make filling easier
	//  the innervert order is shown

				0   0---1---2---3 
					|   |   |   |
				1   0---1---2---3 
					|   |   |   |
				2   0---1---2---3		
					|   |   |   |
				3   0---1---2---3  
		  
	 */
	innerverts = MEM_mallocN(sizeof(EditVert*)*(numcuts+2),"quad-quad subdiv inner verts outer array"); 
	for(i=0;i<numcuts+2;i++) {
		innerverts[i] = MEM_mallocN(sizeof(EditVert*)*(numcuts+2),"quad-quad subdiv inner verts inner array");
	}  
	
	// first row is e1 last row is e3
	for(i=0;i<numcuts+2;i++) {
		innerverts[0][i]		  = verts[0][(numcuts+1)-i];
		innerverts[numcuts+1][i]  = verts[2][(numcuts+1)-i];
	}
	
	for(i=1;i<=numcuts;i++) {
		/* we create a fake edge for the next loop */
		temp.v2 = innerverts[i][0]			= verts[1][i];
		temp.v1 = innerverts[i][numcuts+1]  = verts[3][i];
		
		for(j=1;j<=numcuts;j++) { 
			float percent= (float)j/(float)(numcuts+1);

			innerverts[i][(numcuts+1)-j]= subdivide_edge_addvert(&temp, rad, beauty, percent);
		}	
	}	
	// Fill with faces
	for(i=0;i<numcuts+1;i++) {
		for(j=0;j<numcuts+1;j++) {
			hold = addfacelist(innerverts[i][j+1],innerverts[i][j],innerverts[i+1][j],innerverts[i+1][j+1],NULL,NULL);	 
			hold->e1->f2 = EDGENEW;	  
			hold->e2->f2 = EDGENEW;  
			hold->e3->f2 = EDGENEW;			
			hold->e4->f2 = EDGENEW;   
			
			if(i != 0) { hold->e1->f2 |= EDGEINNER; }
			if(j != 0) { hold->e2->f2 |= EDGEINNER; }
			if(i != numcuts) { hold->e3->f2 |= EDGEINNER; }
			if(j != numcuts) { hold->e4->f2 |= EDGEINNER; }
			
			facecopy(efa,hold);		
		}		
	}
	// Clean up our dynamic multi-dim array
	for(i=0;i<numcuts+2;i++) {
	   MEM_freeN(innerverts[i]);   
	}	
	MEM_freeN(innerverts);
}

static void fill_tri_triple(EditFace *efa, struct GHash *gh, int numcuts, float rad, int beauty)
{
	EditVert **verts[3], ***innerverts;
	short vertsize, i, j;
	EditFace *hold;  
	EditEdge temp;

	// Point verts[0] and [1] to the array of new verts for cedge[0] and cedge[1]
	verts[0] = BLI_ghash_lookup(gh, efa->e1);
	verts[1] = BLI_ghash_lookup(gh, efa->e2);
	verts[2] = BLI_ghash_lookup(gh, efa->e3);

	//This is the index size of the verts array
	vertsize = numcuts+2;

	// Is the original v1 the same as the first vert on the selected edge?
	// if not, the edge is running the opposite direction in this face so flip
	// the array to the correct direction

	if(verts[0][0] != efa->v1) {flipvertarray(verts[0],numcuts+2);}
	if(verts[1][0] != efa->v2) {flipvertarray(verts[1],numcuts+2);}  
	if(verts[2][0] != efa->v3) {flipvertarray(verts[2],numcuts+2);}   
	/*
	We should have something like this now
					   3
										  
				3   2   1   0   
			   0|---*---*---|3 
				|		  /	
		  1	1*		*2   
				|	  /	  
			   2*	*1	   2		 
				|  /		   
			   3|/ 
				 0

	we will fill a 2 dim array of editvert*s to make filling easier

						3

			 0  0---1---2---3---4
				| / | /  |/  | /	
			 1  0---1----2---3 
	   1		| /  | / | /	
			 2  0----1---2	 2
				|  / |  /	   
				|/   |/   
			 3  0---1 
				|  /
				|/
			 4  0  
	  
	*/
	
	innerverts = MEM_mallocN(sizeof(EditVert*)*(numcuts+2),"tri-tri subdiv inner verts outer array"); 
	for(i=0;i<numcuts+2;i++) {
		  innerverts[i] = MEM_mallocN(sizeof(EditVert*)*((numcuts+2)-i),"tri-tri subdiv inner verts inner array");
	}
	//top row is e3 backwards
	for(i=0;i<numcuts+2;i++) {
		  innerverts[0][i]		  = verts[2][(numcuts+1)-i];
	}   
		   
	for(i=1;i<=numcuts+1;i++) {
		//fake edge, first vert is from e1, last is from e2
		temp.v1= innerverts[i][0]			  = verts[0][i];
		temp.v2= innerverts[i][(numcuts+1)-i]  = verts[1][(numcuts+1)-i];
		
		for(j=1;j<(numcuts+1)-i;j++) {
			float percent= (float)j/(float)((numcuts+1)-i);

			innerverts[i][((numcuts+1)-i)-j]= subdivide_edge_addvert(&temp, rad, beauty, 1-percent);
		}
	}

	// Now fill the verts with happy little tris :)
	for(i=0;i<=numcuts+1;i++) {
		for(j=0;j<(numcuts+1)-i;j++) {   
			//We always do the first tri
			hold = addfacelist(innerverts[i][j+1],innerverts[i][j],innerverts[i+1][j],NULL,NULL,NULL);	
			hold->e1->f2 |= EDGENEW;	  
			hold->e2->f2 |= EDGENEW;  
			hold->e3->f2 |= EDGENEW;  
			if(i != 0) { hold->e1->f2 |= EDGEINNER; }
			if(j != 0) { hold->e2->f2 |= EDGEINNER; }
			if(j+1 != (numcuts+1)-i) {hold->e3->f2 |= EDGEINNER;}
			
			facecopy(efa,hold);		
			//if there are more to come, we do the 2nd	 
			if(j+1 <= numcuts-i) {
				hold = addfacelist(innerverts[i+1][j],innerverts[i+1][j+1],innerverts[i][j+1],NULL,NULL,NULL);		   
				facecopy(efa,hold); 
				hold->e1->f2 |= EDGENEW;	  
				hold->e2->f2 |= EDGENEW;  
				hold->e3->f2 |= EDGENEW;  	
			}
		} 
	}

	// Clean up our dynamic multi-dim array
	for(i=0;i<numcuts+2;i++) {
		MEM_freeN(innerverts[i]);   
	}	
	MEM_freeN(innerverts);
}

//Next two fill types are for knife exact only and are provided to allow for knifing through vertices
//This means there is no multicut!
static void fill_quad_doublevert(EditFace *efa, int v1, int v2){
	EditFace *hold;
	/*
		Depending on which two vertices have been knifed through (v1 and v2), we
		triangulate like the patterns below.
				X-------|	|-------X
				| \  	|	|     / |
				|   \	|	|   /	|
				|	  \	|	| /	    |
				--------X	X--------
	*/
	
	if(v1 == 1 && v2 == 3){
		hold= addfacelist(efa->v1, efa->v2, efa->v3, 0, efa, NULL);
		hold->e1->f2 |= EDGENEW;
		hold->e2->f2 |= EDGENEW;
		hold->e3->f2 |= EDGENEW;
		hold->e3->f2 |= EDGEINNER;
		facecopy(efa, hold);
		
		hold= addfacelist(efa->v1, efa->v3, efa->v4, 0, efa, NULL);
		hold->e1->f2 |= EDGENEW;
		hold->e2->f2 |= EDGENEW;
		hold->e3->f2 |= EDGENEW;
		hold->e1->f2 |= EDGEINNER;
		facecopy(efa, hold);
	}
	else{
		hold= addfacelist(efa->v1, efa->v2, efa->v4, 0, efa, NULL);
		hold->e1->f2 |= EDGENEW;
		hold->e2->f2 |= EDGENEW;
		hold->e3->f2 |= EDGENEW;
		hold->e2->f2 |= EDGEINNER;
		facecopy(efa, hold);
		
		hold= addfacelist(efa->v2, efa->v3, efa->v4, 0, efa, NULL);
		hold->e1->f2 |= EDGENEW;
		hold->e2->f2 |= EDGENEW;
		hold->e3->f2 |= EDGENEW;
		hold->e3->f2 |= EDGEINNER;
		facecopy(efa, hold);
	}
}

static void fill_quad_singlevert(EditFace *efa, struct GHash *gh)
{
	EditEdge *cedge=NULL;
	EditVert *v[4], **verts;
	EditFace *hold;
	short start=0, end, left, right, vertsize;   
							
	v[0] = efa->v1;
	v[1] = efa->v2;
	v[2] = efa->v3;
	v[3] = efa->v4;	 

	if(efa->e1->f & SELECT)	  { cedge = efa->e1; start = 0;}
	else if(efa->e2->f & SELECT) { cedge = efa->e2; start = 1;}	   
	else if(efa->e3->f & SELECT) { cedge = efa->e3; start = 2;}	   
	else if(efa->e4->f & SELECT) { cedge = efa->e4; start = 3;}		 

	// Point verts to the array of new verts for cedge
	verts = BLI_ghash_lookup(gh, cedge);
	//This is the index size of the verts array
	vertsize = 3;

	// Is the original v1 the same as the first vert on the selected edge?
	// if not, the edge is running the opposite direction in this face so flip
	// the array to the correct direction

	if(verts[0] != v[start]) {flipvertarray(verts,3);}
	end	= (start+1)%4;
	left   = (start+2)%4;
	right  = (start+3)%4; 

/*
	We should have something like this now

			  end		 start				 
			   2     1     0   
			   |-----*-----|
			   |		   |
			   |		   |	   
			   |		   |
			   -------------	   
			  left	   right

	where start,end,left, right are indexes of EditFace->v1, etc (stored in v)
	and 0,1,2 are the indexes of the new verts stored in verts. We fill like
	this, depending on whether its vertex 'left' or vertex 'right' thats
	been knifed through...
				
				|---*---|	|---*---|
				|  /	|	|    \  |
				| /		|	|	  \ |
				|/		|	|	   \|
				X--------	--------X
*/

	if(v[left]->f1){
		//triangle is composed of cutvert, end and left
		hold = addfacelist(verts[1],v[end],v[left],NULL, NULL,NULL);
		hold->e1->f2 |= EDGENEW;
		hold->e2->f2 |= EDGENEW;
		hold->e3->f2 |= EDGENEW;
		hold->e3->f2 |= EDGEINNER;
		facecopy(efa, hold);
		
		//quad is composed of cutvert, left, right and start
		hold = addfacelist(verts[1],v[left],v[right],v[start], NULL, NULL);
		hold->e1->f2 |= EDGENEW;
		hold->e2->f2 |= EDGENEW;
		hold->e3->f2 |= EDGENEW;
		hold->e4->f2 |= EDGENEW;
		hold->e1->f2 |= EDGEINNER;
		facecopy(efa, hold);
	}
	else if(v[right]->f1){
		//triangle is composed of cutvert, right and start
		hold = addfacelist(verts[1],v[right],v[start], NULL, NULL, NULL);
		hold->e1->f2 |= EDGENEW;
		hold->e2->f2 |= EDGENEW;
		hold->e3->f2 |= EDGENEW;
		hold->e1->f2 |= EDGEINNER;
		facecopy(efa, hold);
		//quad is composed of cutvert, end, left, right
		hold = addfacelist(verts[1],v[end], v[left], v[right], NULL, NULL);
		hold->e1->f2 |= EDGENEW;
		hold->e2->f2 |= EDGENEW;
		hold->e3->f2 |= EDGENEW;
		hold->e4->f2 |= EDGENEW;
		hold->e4->f2 |= EDGEINNER;
		facecopy(efa, hold);
	}
	
}	

// This function takes an example edge, the current point to create and 
// the total # of points to create, then creates the point and return the
// editvert pointer to it.
static EditVert *subdivideedgenum(EditEdge *edge, int curpoint, int totpoint, float rad, int beauty)
{
	EditVert *ev;
	float percent;
	 
	if (beauty & (B_PERCENTSUBD) && totpoint == 1)
		//percent=(float)(edge->tmp.l)/32768.0f;
		percent= edge->tmp.fp;
	else
		percent= (float)curpoint/(float)(totpoint+1);

	ev= subdivide_edge_addvert(edge, rad, beauty, percent);
	ev->f = edge->v1->f;
	
	return ev;
}

void esubdivideflag(int flag, float rad, int beauty, int numcuts, int seltype)
{
	EditMesh *em = G.editMesh;
	EditFace *ef;
	EditEdge *eed, *cedge, *sort[4];
	EditVert *eve, **templist;
	struct GHash *gh;
	float length[4], v1mat[3], v2mat[3], v3mat[3], v4mat[3];
	int i, j, edgecount, touchcount, facetype,hold;
	ModifierData *md= G.obedit->modifiers.first;
	
	if(multires_test()) return;

	//Set faces f1 to 0 cause we need it later
	for(ef=em->faces.first;ef;ef = ef->next) ef->f1 = 0;
	for(eve=em->verts.first; eve; eve=eve->next) eve->f1 = eve->f2 = 0;

	for (; md; md=md->next) {
		if (md->type==eModifierType_Mirror) {
			MirrorModifierData *mmd = (MirrorModifierData*) md;	
		
			if(mmd->flag & MOD_MIR_CLIPPING) {
				for (eve= em->verts.first; eve; eve= eve->next) {
					eve->f2= 0;
					switch(mmd->axis){
						case 0:
							if (fabs(eve->co[0]) < mmd->tolerance)
								eve->f2 |= 1;
							break;
						case 1:
							if (fabs(eve->co[1]) < mmd->tolerance)
								eve->f2 |= 2;
							break;
						case 2:
							if (fabs(eve->co[2]) < mmd->tolerance)
								eve->f2 |= 4;
							break;
					}
				}
			}
		}
	}
	
	//Flush vertex flags upward to the edges
	for(eed = em->edges.first;eed;eed = eed->next) {
		//if(eed->f & flag && eed->v1->f == eed->v2->f) {
		//	eed->f |= eed->v1->f;   
		// }
		eed->f2 = 0;   
		if(eed->f & flag) {
			eed->f2	|= EDGEOLD;
		}
	}
	
	// We store an array of verts for each edge that is subdivided,
	// we put this array as a value in a ghash which is keyed by the EditEdge*

	// Now for beauty subdivide deselect edges based on length
	if(beauty & B_BEAUTY) { 
		for(ef = em->faces.first;ef;ef = ef->next) {
			if(!ef->v4) {
				continue;
			}
			if(ef->f & SELECT) {
				VECCOPY(v1mat, ef->v1->co);
				VECCOPY(v2mat, ef->v2->co);
				VECCOPY(v3mat, ef->v3->co);
				VECCOPY(v4mat, ef->v4->co);						
				Mat4Mul3Vecfl(G.obedit->obmat, v1mat);
				Mat4Mul3Vecfl(G.obedit->obmat, v2mat);											
				Mat4Mul3Vecfl(G.obedit->obmat, v3mat);
				Mat4Mul3Vecfl(G.obedit->obmat, v4mat);
				
				length[0] = VecLenf(v1mat, v2mat);
				length[1] = VecLenf(v2mat, v3mat);
				length[2] = VecLenf(v3mat, v4mat);
				length[3] = VecLenf(v4mat, v1mat);
				sort[0] = ef->e1;
				sort[1] = ef->e2;
				sort[2] = ef->e3;
				sort[3] = ef->e4;
												  
												
				// Beauty Short Edges
				if(beauty & B_BEAUTY_SHORT) {
					for(j=0;j<2;j++) {
						hold = -1;
						for(i=0;i<4;i++) {
							if(length[i] < 0) {
								continue;							
							} else if(hold == -1) {  
								hold = i; 
							} else {
								if(length[hold] < length[i]) {
									hold = i;   
								}
							}
						}
						sort[hold]->f &= ~SELECT;
						sort[hold]->f2 |= EDGENEW;
						length[hold] = -1;
					}							
				} 
				
				// Beauty Long Edges
				else {
					 for(j=0;j<2;j++) {
						hold = -1;
						for(i=0;i<4;i++) {
							if(length[i] < 0) {
								continue;							
							} else if(hold == -1) {  
								hold = i; 
							} else {
								if(length[hold] > length[i]) {
									hold = i;   
								}
							}
						}
						sort[hold]->f &= ~SELECT;
						sort[hold]->f2 |= EDGENEW;
						length[hold] = -1;
					}							
				}   
			}
		}	
	}

	gh = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp); 

	// If we are knifing, We only need the selected edges that were cut, so deselect if it was not cut
	if(beauty & B_KNIFE) {	
		for(eed= em->edges.first;eed;eed=eed->next) {	
			if( eed->tmp.fp == 0 ) {
				EM_select_edge(eed,0);
			}
		}
	}  
	// So for each edge, if it is selected, we allocate an array of size cuts+2
	// so we can have a place for the v1, the new verts and v2  
	for(eed=em->edges.first;eed;eed = eed->next) {
		if(eed->f & flag) {
			templist = MEM_mallocN(sizeof(EditVert*)*(numcuts+2),"vertlist");
			templist[0] = eed->v1;
			for(i=0;i<numcuts;i++) {
				// This function creates the new vert and returns it back
				// to the array
				templist[i+1] = subdivideedgenum(eed, i+1, numcuts, rad, beauty);
				//while we are here, we can copy edge info from the original edge
				cedge = addedgelist(templist[i],templist[i+1],eed);
				// Also set the edge f2 to EDGENEW so that we can use this info later
				cedge->f2 = EDGENEW;
			}
			templist[i+1] = eed->v2;
			//Do the last edge too
			cedge = addedgelist(templist[i],templist[i+1],eed);
			cedge->f2 = EDGENEW;
			// Now that the edge is subdivided, we can put its verts in the ghash 
			BLI_ghash_insert(gh, eed, templist);			   
		}								  
	}

	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	// Now for each face in the mesh we need to figure out How many edges were cut
	// and which filling method to use for that face
	for(ef = em->faces.first;ef;ef = ef->next) {
		edgecount = 0;
		facetype = 3;
		if(ef->e1->f & flag) {edgecount++;}
		if(ef->e2->f & flag) {edgecount++;}
		if(ef->e3->f & flag) {edgecount++;}
		if(ef->v4) {
			facetype = 4;
			if(ef->e4->f & flag) {edgecount++;}
		}  
		if(facetype == 4) {
			switch(edgecount) {
				case 0:
					if(beauty & B_KNIFE && numcuts == 1){
						/*Test for when knifing through two opposite verts but no edges*/
						touchcount = 0;
						if(ef->v1->f1) touchcount++;
						if(ef->v2->f1) touchcount++;
						if(ef->v3->f1) touchcount++;
						if(ef->v4->f1) touchcount++;
						if(touchcount == 2){
							if(ef->v1->f1 && ef->v3->f1){ 
								ef->f1 = SELECT;
								fill_quad_doublevert(ef, 1, 3); 
							}
							else if(ef->v2->f1 && ef->v4->f1){
								ef->f1 = SELECT;
								fill_quad_doublevert(ef, 2, 4);
							}
						}
					}
					break; 
				
				case 1: 
					if(beauty & B_KNIFE && numcuts == 1){
						/*Test for when knifing through an edge and one vert*/
						touchcount = 0;
						if(ef->v1->f1) touchcount++;
						if(ef->v2->f1) touchcount++;
						if(ef->v3->f1) touchcount++;
						if(ef->v4->f1) touchcount++;
						
						if(touchcount == 1){
							if( (ef->e1->f & flag && ( !ef->e1->v1->f1 && !ef->e1->v2->f1 )) ||
								(ef->e2->f & flag && ( !ef->e2->v1->f1 && !ef->e2->v2->f1 )) ||
								(ef->e3->f & flag && ( !ef->e3->v1->f1 && !ef->e3->v2->f1 )) ||
								(ef->e4->f & flag && ( !ef->e4->v1->f1 && !ef->e4->v2->f1 )) ){
								
								ef->f1 = SELECT; 
								fill_quad_singlevert(ef, gh);
							}
							else{
								ef->f1 = SELECT;
								fill_quad_single(ef, gh, numcuts, seltype);
							}
						}
						else{ 
							ef->f1 = SELECT; 
							fill_quad_single(ef, gh, numcuts, seltype);
						}
					}
					else{ 
						ef->f1 = SELECT;
						fill_quad_single(ef, gh, numcuts, seltype);
					}
					break;   
				case 2: ef->f1 = SELECT;
					// if there are 2, we check if edge 1 and 3 are either both on or off that way
					// we can tell if the selected pair is Adjacent or Opposite of each other
					if((ef->e1->f & flag && ef->e3->f & flag) || 
					   (ef->e2->f & flag && ef->e4->f & flag)) {
						fill_quad_double_op(ef, gh, numcuts);							  
					}else{
						switch(G.scene->toolsettings->cornertype) {
							case 0:	fill_quad_double_adj_path(ef, gh, numcuts); break;
							case 1:	fill_quad_double_adj_inner(ef, gh, numcuts); break;
							case 2:	fill_quad_double_adj_fan(ef, gh, numcuts); break;
						}
												  
					}
						break;	
				case 3: ef->f1 = SELECT;
					fill_quad_triple(ef, gh, numcuts); 
					break;	
				case 4: ef->f1 = SELECT;
					fill_quad_quadruple(ef, gh, numcuts, rad, beauty); 
					break;	
			}
		} else {
			switch(edgecount) {
				case 0: break;
				case 1: ef->f1 = SELECT;
					fill_tri_single(ef, gh, numcuts, seltype);
					break;   
				case 2: ef->f1 = SELECT;
					fill_tri_double(ef, gh, numcuts);
					break;	
				case 3: ef->f1 = SELECT;
					fill_tri_triple(ef, gh, numcuts, rad, beauty);
					break;  
			}	
		}	
	}
	
	// Delete Old Edges and Faces
	for(eed = em->edges.first;eed;eed = eed->next) {
		if(BLI_ghash_haskey(gh,eed)) {
			eed->f1 = SELECT; 
		} else {
			eed->f1 = 0;   
		}
	} 
	free_tagged_edges_faces(em->edges.first, em->faces.first); 
	
	if(seltype == SUBDIV_SELECT_ORIG  && G.qual  != LR_CTRLKEY) {
		for(eed = em->edges.first;eed;eed = eed->next) {
			if(eed->f2 & EDGENEW || eed->f2 & EDGEOLD) {
				eed->f |= flag;
				EM_select_edge(eed,1); 
				
			}else{
				eed->f &= !flag;
				EM_select_edge(eed,0); 
			}
		}   
	} else if ((seltype == SUBDIV_SELECT_INNER || seltype == SUBDIV_SELECT_INNER_SEL)|| G.qual == LR_CTRLKEY) {
		for(eed = em->edges.first;eed;eed = eed->next) {
			if(eed->f2 & EDGEINNER) {
				eed->f |= flag;
				EM_select_edge(eed,1);   
				if(eed->v1->f & EDGEINNER) eed->v1->f |= SELECT;
				if(eed->v2->f & EDGEINNER) eed->v2->f |= SELECT;
			}else{
				eed->f &= !flag;
				EM_select_edge(eed,0); 
			}
		}		  
	} else if(seltype == SUBDIV_SELECT_LOOPCUT){
		for(eed = em->edges.first;eed;eed = eed->next) {
			if(eed->f2 & DOUBLEOPFILL){
				eed->f |= flag;
				EM_select_edge(eed,1);
			}else{
				eed->f &= !flag;
				EM_select_edge(eed,0);
			}
		}
	} 
	 if(G.scene->selectmode & SCE_SELECT_VERTEX) {
		 for(eed = em->edges.first;eed;eed = eed->next) {
			if(eed->f & SELECT) {
				eed->v1->f |= SELECT;
				eed->v2->f |= SELECT;
			}
		}	
	}
	
	//fix hide flags for edges. First pass, hide edges of hidden faces
	for(ef=em->faces.first; ef; ef=ef->next){
		if(ef->h){
			ef->e1->h |= 1;
			ef->e2->h |= 1;
			ef->e3->h |= 1;
			if(ef->e4) ef->e4->h |= 1;
		}
	}
	//second pass: unhide edges of visible faces adjacent to hidden faces
	for(ef=em->faces.first; ef; ef=ef->next){
		if(ef->h == 0){
			ef->e1->h &= ~1;
			ef->e2->h &= ~1;
			ef->e3->h &= ~1;
			if(ef->e4) ef->e4->h &= ~1;
		}
	}
	
	// Free the ghash and call MEM_freeN on all the value entries to return 
	// that memory
	BLI_ghash_free(gh, NULL, (GHashValFreeFP)MEM_freeN);   
	
	EM_selectmode_flush();
	for(ef=em->faces.first;ef;ef = ef->next) {
		if(ef->e4) {
			if(  (ef->e1->f & SELECT && ef->e2->f & SELECT) &&
			 (ef->e3->f & SELECT && ef->e4->f & SELECT) ) {
				ef->f |= SELECT;			 
			}				   
		} else {
			if(  (ef->e1->f & SELECT && ef->e2->f & SELECT) && ef->e3->f & SELECT) {
				ef->f |= SELECT;			 
			}
		}
	}
	
	recalc_editnormals();
	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
}

static int count_selected_edges(EditEdge *ed)
{
	int totedge = 0;
	while(ed) {
		ed->tmp.p = 0;
		if( ed->f & SELECT ) totedge++;
		ed= ed->next;
	}
	return totedge;
}

/* hurms, as if this makes code readable! It's pointerpointer hiding... (ton) */
typedef EditFace *EVPtr;
typedef EVPtr EVPTuple[2];

/** builds EVPTuple array efaa of face tuples (in fact pointers to EditFaces)
	sharing one edge.
	arguments: selected edge list, face list.
	Edges will also be tagged accordingly (see eed->f2)		  */

static int collect_quadedges(EVPTuple *efaa, EditEdge *eed, EditFace *efa)
{
	EditEdge *e1, *e2, *e3;
	EVPtr *evp;
	int i = 0;

	/* run through edges, if selected, set pointer edge-> facearray */
	while(eed) {
		eed->f2= 0;
		eed->f1= 0;
		if( eed->f & SELECT ) {
			eed->tmp.p = (EditVert *) (&efaa[i]);
			i++;
		}
		else eed->tmp.p = NULL;
		
		eed= eed->next;
	}
		
	
	/* find edges pointing to 2 faces by procedure:
	
	- run through faces and their edges, increase
	  face counter e->f1 for each face 
	*/

	while(efa) {
		efa->f1= 0;
		if(efa->v4==0 && (efa->f & SELECT)) {  /* if selected triangle */
			e1= efa->e1;
			e2= efa->e2;
			e3= efa->e3;
			if(e1->f2<3 && e1->tmp.p) {
				if(e1->f2<2) {
					evp= (EVPtr *) e1->tmp.p;
					evp[(int)e1->f2] = efa;
				}
				e1->f2+= 1;
			}
			if(e2->f2<3 && e2->tmp.p) {
				if(e2->f2<2) {
					evp= (EVPtr *) e2->tmp.p;
					evp[(int)e2->f2]= efa;
				}
				e2->f2+= 1;
			}
			if(e3->f2<3 && e3->tmp.p) {
				if(e3->f2<2) {
					evp= (EVPtr *) e3->tmp.p;
					evp[(int)e3->f2]= efa;
				}
				e3->f2+= 1;
			}
		}
		else {
			/* set to 3 to make sure these are not flipped or joined */
			efa->e1->f2= 3;
			efa->e2->f2= 3;
			efa->e3->f2= 3;
			if (efa->e4) efa->e4->f2= 3;
		}

		efa= efa->next;
	}
	return i;
}


/* returns vertices of two adjacent triangles forming a quad 
   - can be righthand or lefthand

			4-----3
			|\	|
			| \ 2 | <- efa1
			|  \  | 
	  efa-> | 1 \ | 
			|	\| 
			1-----2

*/
#define VTEST(face, num, other) \
	(face->v##num != other->v1 && face->v##num != other->v2 && face->v##num != other->v3) 

static void givequadverts(EditFace *efa, EditFace *efa1, EditVert **v1, EditVert **v2, EditVert **v3, EditVert **v4, int *vindex)
{
	if VTEST(efa, 1, efa1) {
		*v1= efa->v1;
		*v2= efa->v2;
		vindex[0]= 0;
		vindex[1]= 1;
	}
	else if VTEST(efa, 2, efa1) {
		*v1= efa->v2;
		*v2= efa->v3;
		vindex[0]= 1;
		vindex[1]= 2;
	}
	else if VTEST(efa, 3, efa1) {
		*v1= efa->v3;
		*v2= efa->v1;
		vindex[0]= 2;
		vindex[1]= 0;
	}
	
	if VTEST(efa1, 1, efa) {
		*v3= efa1->v1;
		*v4= efa1->v2;
		vindex[2]= 0;
		vindex[3]= 1;
	}
	else if VTEST(efa1, 2, efa) {
		*v3= efa1->v2;
		*v4= efa1->v3;
		vindex[2]= 1;
		vindex[3]= 2;
	}
	else if VTEST(efa1, 3, efa) {
		*v3= efa1->v3;
		*v4= efa1->v1;
		vindex[2]= 2;
		vindex[3]= 0;
	}
	else
		*v3= *v4= NULL;
}

/* Helper functions for edge/quad edit features*/
static void untag_edges(EditFace *f)
{
	f->e1->f1 = 0;
	f->e2->f1 = 0;
	f->e3->f1 = 0;
	if (f->e4) f->e4->f1 = 0;
}

/** remove and free list of tagged edges and faces */
static void free_tagged_edges_faces(EditEdge *eed, EditFace *efa)
{
	EditMesh *em= G.editMesh;
	EditEdge *nexted;
	EditFace *nextvl;

	while(efa) {
		nextvl= efa->next;
		if(efa->f1) {
			BLI_remlink(&em->faces, efa);
			free_editface(efa);
		}
		else
			/* avoid deleting edges that are still in use */
			untag_edges(efa);
		efa= nextvl;
	}

	while(eed) {
		nexted= eed->next;
		if(eed->f1) {
			remedge(eed);
			free_editedge(eed);
		}
		eed= nexted;
	}	
}	

/* note; the EM_selectmode_set() calls here illustrate how badly constructed it all is... from before the
   edge/face flags, with very mixed results.... */
void beauty_fill(void)
{
	EditMesh *em = G.editMesh;
	EditVert *v1, *v2, *v3, *v4;
	EditEdge *eed, *nexted;
	EditEdge dia1, dia2;
	EditFace *efa, *w;
	// void **efaar, **efaa;
	EVPTuple *efaar;
	EVPtr *efaa;
	float len1, len2, len3, len4, len5, len6, opp1, opp2, fac1, fac2;
	int totedge, ok, notbeauty=8, onedone, vindex[4];
	
	if(multires_test()) return;

	/* - all selected edges with two faces
		* - find the faces: store them in edges (using datablock)
		* - per edge: - test convex
		*			   - test edge: flip?
		*			   - if true: remedge,  addedge, all edges at the edge get new face pointers
		*/
	
	EM_selectmode_set();	// makes sure in selectmode 'face' the edges of selected faces are selected too 

	totedge = count_selected_edges(em->edges.first);
	if(totedge==0) return;

	/* temp block with face pointers */
	efaar= (EVPTuple *) MEM_callocN(totedge * sizeof(EVPTuple), "beautyfill");

	while (notbeauty) {
		notbeauty--;

		ok = collect_quadedges(efaar, em->edges.first, em->faces.first);

		/* there we go */
		onedone= 0;

		eed= em->edges.first;
		while(eed) {
			nexted= eed->next;
			
			/* f2 is set in collect_quadedges() */
			if(eed->f2==2 && eed->h==0) {

				efaa = (EVPtr *) eed->tmp.p;

				/* none of the faces should be treated before, nor be part of fgon */
				ok= 1;
				efa= efaa[0];
				if(efa->e1->f1 || efa->e2->f1 || efa->e3->f1) ok= 0;
				if(efa->fgonf) ok= 0;
				efa= efaa[1];
				if(efa->e1->f1 || efa->e2->f1 || efa->e3->f1) ok= 0;
				if(efa->fgonf) ok= 0;
				
				if(ok) {
					/* test convex */
					givequadverts(efaa[0], efaa[1], &v1, &v2, &v3, &v4, vindex);
					if(v1 && v2 && v3 && v4) {
						if( convex(v1->co, v2->co, v3->co, v4->co) ) {

							/* test edges */
							if( (v1) > (v3) ) {
								dia1.v1= v3;
								dia1.v2= v1;
							}
							else {
								dia1.v1= v1;
								dia1.v2= v3;
							}

							if( (v2) > (v4) ) {
								dia2.v1= v4;
								dia2.v2= v2;
							}
							else {
								dia2.v1= v2;
								dia2.v2= v4;
							}

							/* testing rule:
							 * the area divided by the total edge lengths
							 */

							len1= VecLenf(v1->co, v2->co);
							len2= VecLenf(v2->co, v3->co);
							len3= VecLenf(v3->co, v4->co);
							len4= VecLenf(v4->co, v1->co);
							len5= VecLenf(v1->co, v3->co);
							len6= VecLenf(v2->co, v4->co);

							opp1= AreaT3Dfl(v1->co, v2->co, v3->co);
							opp2= AreaT3Dfl(v1->co, v3->co, v4->co);

							fac1= opp1/(len1+len2+len5) + opp2/(len3+len4+len5);

							opp1= AreaT3Dfl(v2->co, v3->co, v4->co);
							opp2= AreaT3Dfl(v2->co, v4->co, v1->co);

							fac2= opp1/(len2+len3+len6) + opp2/(len4+len1+len6);

							ok= 0;
							if(fac1 > fac2) {
								if(dia2.v1==eed->v1 && dia2.v2==eed->v2) {
									eed->f1= 1;
									efa= efaa[0];
									efa->f1= 1;
									efa= efaa[1];
									efa->f1= 1;

									w= EM_face_from_faces(efaa[0], efaa[1],
										vindex[0], vindex[1], 4+vindex[2], -1);
									w->f |= SELECT;


									w= EM_face_from_faces(efaa[0], efaa[1],
										vindex[0], 4+vindex[2], 4+vindex[3], -1);
									w->f |= SELECT;

									onedone= 1;
								}
							}
							else if(fac1 < fac2) {
								if(dia1.v1==eed->v1 && dia1.v2==eed->v2) {
									eed->f1= 1;
									efa= efaa[0];
									efa->f1= 1;
									efa= efaa[1];
									efa->f1= 1;


									w= EM_face_from_faces(efaa[0], efaa[1],
										vindex[1], 4+vindex[2], 4+vindex[3], -1);
									w->f |= SELECT;


									w= EM_face_from_faces(efaa[0], efaa[1],
										vindex[0], 4+vindex[1], 4+vindex[3], -1);
									w->f |= SELECT;

									onedone= 1;
								}
							}
						}
					}
				}

			}
			eed= nexted;
		}

		free_tagged_edges_faces(em->edges.first, em->faces.first);

		if(onedone==0) break;
		
		EM_selectmode_set();	// new edges/faces were added
	}

	MEM_freeN(efaar);

	EM_select_flush();
	
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
#endif
	BIF_undo_push("Beauty Fill");
}


/* ******************** BEGIN TRIANGLE TO QUAD ************************************* */
static float measure_facepair(EditVert *v1, EditVert *v2, EditVert *v3, EditVert *v4, float limit){
	
	/*gives a 'weight' to a pair of triangles that join an edge to decide how good a join they would make*/
	/*Note: this is more complicated than it needs to be and should be cleaned up...*/
	float	measure = 0.0, noA1[3], noA2[3], noB1[3], noB2[3], normalADiff, normalBDiff,
			edgeVec1[3], edgeVec2[3], edgeVec3[3], edgeVec4[3], diff,
			minarea, maxarea, areaA, areaB;
	
	/*First Test: Normal difference*/
	CalcNormFloat(v1->co, v2->co, v3->co, noA1);
	CalcNormFloat(v1->co, v3->co, v4->co, noA2);
	
	if(noA1[0] == noA2[0] && noA1[1] == noA2[1] && noA1[2] == noA2[2]) normalADiff = 0.0;
	else normalADiff = VecAngle2(noA1, noA2);
		//if(!normalADiff) normalADiff = 179;
	CalcNormFloat(v2->co, v3->co, v4->co, noB1);
	CalcNormFloat(v4->co, v1->co, v2->co, noB2);
	
	if(noB1[0] == noB2[0] && noB1[1] == noB2[1] && noB1[2] == noB2[2]) normalBDiff = 0.0;
	else normalBDiff = VecAngle2(noB1, noB2);
		//if(!normalBDiff) normalBDiff = 179;
	
	measure += (normalADiff/360) + (normalBDiff/360);
	if(measure > limit) return measure;
	
	/*Second test: Colinearity*/
	VecSubf(edgeVec1, v1->co, v2->co);
	VecSubf(edgeVec2, v2->co, v3->co);
	VecSubf(edgeVec3, v3->co, v4->co);
	VecSubf(edgeVec4, v4->co, v1->co);
	
	diff = 0.0;
	
	diff = (
		fabs(VecAngle2(edgeVec1, edgeVec2) - 90) +
		fabs(VecAngle2(edgeVec2, edgeVec3) - 90) + 
		fabs(VecAngle2(edgeVec3, edgeVec4) - 90) + 
		fabs(VecAngle2(edgeVec4, edgeVec1) - 90)) / 360;
	if(!diff) return 0.0;
	
	measure +=  diff;
	if(measure > limit) return measure;

	/*Third test: Concavity*/
	areaA = AreaT3Dfl(v1->co, v2->co, v3->co) + AreaT3Dfl(v1->co, v3->co, v4->co);
	areaB = AreaT3Dfl(v2->co, v3->co, v4->co) + AreaT3Dfl(v4->co, v1->co, v2->co);
	
	if(areaA <= areaB) minarea = areaA;
	else minarea = areaB;
	
	if(areaA >= areaB) maxarea = areaA;
	else maxarea = areaB;
	
	if(!maxarea) measure += 1;
	else measure += (1 - (minarea / maxarea));

	return measure;
}

#define T2QUV_LIMIT 0.005
#define T2QCOL_LIMIT 3
static int compareFaceAttribs(EditFace *f1, EditFace *f2, EditEdge *eed) 
{
	/*Test to see if the per-face attributes for the joining edge match within limit*/	
	MTFace *tf1, *tf2;
	unsigned int *col1, *col2;
	short i,attrok=0, flag = G.scene->toolsettings->editbutflag, fe1[2], fe2[2];
	
	tf1 = CustomData_em_get(&G.editMesh->fdata, f1->data, CD_MTFACE);
	tf2 = CustomData_em_get(&G.editMesh->fdata, f2->data, CD_MTFACE);

	col1 = CustomData_em_get(&G.editMesh->fdata, f1->data, CD_MCOL);
	col2 = CustomData_em_get(&G.editMesh->fdata, f2->data, CD_MCOL);
	
	/*store indices for faceedges*/
	f1->v1->f1 = 0;
	f1->v2->f1 = 1;
	f1->v3->f1 = 2;
	
	fe1[0] = eed->v1->f1;
	fe1[1] = eed->v2->f1;
	
	f2->v1->f1 = 0;
	f2->v2->f1 = 1;
	f2->v3->f1 = 2;
	
	fe2[0] = eed->v1->f1;
	fe2[1] = eed->v2->f1;
	
	/*compare faceedges for each face attribute. Additional per face attributes can be added later*/
	/*do UVs*/
	if(flag & B_JOINTRIA_UV){
		
		if(tf1 == NULL || tf2 == NULL) attrok |= B_JOINTRIA_UV;
		else if(tf1->tpage != tf2->tpage); /*do nothing*/
		else{
			for(i = 0; i < 2; i++){
				if(tf1->uv[fe1[i]][0] + T2QUV_LIMIT > tf2->uv[fe2[i]][0] && tf1->uv[fe1[i]][0] - T2QUV_LIMIT < tf2->uv[fe2[i]][0] &&
					tf1->uv[fe1[i]][1] + T2QUV_LIMIT > tf2->uv[fe2[i]][1] && tf1->uv[fe1[i]][1] - T2QUV_LIMIT < tf2->uv[fe2[i]][1]) attrok |= B_JOINTRIA_UV;
			}
		}
	}
	
	/*do VCOLs*/
	if(flag & B_JOINTRIA_VCOL){
		if(!col1 || !col2) attrok |= B_JOINTRIA_VCOL;
		else{
			char *f1vcol, *f2vcol;
			for(i = 0; i < 2; i++){
				f1vcol = (char *)&(col1[fe1[i]]);
				f2vcol = (char *)&(col2[fe2[i]]);
		
				/*compare f1vcol with f2vcol*/
				if(	f1vcol[1] + T2QCOL_LIMIT > f2vcol[1] && f1vcol[1] - T2QCOL_LIMIT < f2vcol[1] &&
					f1vcol[2] + T2QCOL_LIMIT > f2vcol[2] && f1vcol[2] - T2QCOL_LIMIT < f2vcol[2] &&
					f1vcol[3] + T2QCOL_LIMIT > f2vcol[3] && f1vcol[3] - T2QCOL_LIMIT < f2vcol[3]) attrok |= B_JOINTRIA_VCOL;
			}
		}
	}
	
	if( ((attrok & B_JOINTRIA_UV) == (flag & B_JOINTRIA_UV)) && ((attrok & B_JOINTRIA_VCOL) == (flag & B_JOINTRIA_VCOL)) ) return 1;
	return 0;
}	
	
static int fplcmp(const void *v1, const void *v2)
{
	const EditEdge *e1= *((EditEdge**)v1), *e2=*((EditEdge**)v2);
	
	if( e1->crease > e2->crease) return 1;
	else if( e1->crease < e2->crease) return -1;
	
	return 0;
}

/*Bitflags for edges.*/
#define T2QDELETE	1
#define T2QCOMPLEX	2
#define T2QJOIN		4
void join_triangles(void)
{
	EditMesh *em=G.editMesh;
	EditVert *v1, *v2, *v3, *v4, *eve;
	EditEdge *eed, **edsortblock = NULL, **edb = NULL;
	EditFace *efa;
	EVPTuple *efaar = NULL;
	EVPtr *efaa = NULL;
	float *creases = NULL;
	float measure; /*Used to set tolerance*/
	float limit = G.scene->toolsettings->jointrilimit;
	int i, ok, totedge=0, totseledge=0, complexedges, vindex[4];
	
	/*test for multi-resolution data*/
	if(multires_test()) return;

	/*if we take a long time on very dense meshes we want waitcursor to display*/
	waitcursor(1);
	
	totseledge = count_selected_edges(em->edges.first);
	if(totseledge==0) return;
	
	/*abusing crease value to store weights for edge pairs. Nasty*/
	for(eed=em->edges.first; eed; eed=eed->next) totedge++;
	if(totedge) creases = MEM_callocN(sizeof(float) * totedge, "Join Triangles Crease Array"); 
	for(eed=em->edges.first, i = 0; eed; eed=eed->next, i++){
		creases[i] = eed->crease; 
		eed->crease = 0.0;
	}
	
	/*clear temp flags*/
	for(eve=em->verts.first; eve; eve=eve->next) eve->f1 = eve->f2 = 0;
	for(eed=em->edges.first; eed; eed=eed->next) eed->f2 = eed->f1 = 0;
	for(efa=em->faces.first; efa; efa=efa->next) efa->f1 = efa->tmp.l = 0;

	/*For every selected 2 manifold edge, create pointers to its two faces.*/
	efaar= (EVPTuple *) MEM_callocN(totseledge * sizeof(EVPTuple), "Tri2Quad");
	ok = collect_quadedges(efaar, em->edges.first, em->faces.first);
	complexedges = 0;
	
	if(ok){
		
		
		/*clear tmp.l flag and store number of faces that are selected and coincident to current face here.*/  
		for(eed=em->edges.first; eed; eed=eed->next){
			/* eed->f2 is 2 only if this edge is part of exactly two
			   triangles, and both are selected, and it has EVPTuple assigned */
			if(eed->f2 == 2){
				efaa= (EVPtr *) eed->tmp.p;
				efaa[0]->tmp.l++;
				efaa[1]->tmp.l++;
			}
		}
		
		for(eed=em->edges.first; eed; eed=eed->next){
			if(eed->f2 == 2){
				efaa= (EVPtr *) eed->tmp.p;
				v1 = v2 = v3 = v4 = NULL;
				givequadverts(efaa[0], efaa[1], &v1, &v2, &v3, &v4, vindex);
				if(v1 && v2 && v3 && v4){
					/*test if simple island first. This mimics 2.42 behaviour and the tests are less restrictive.*/
					if(efaa[0]->tmp.l == 1 && efaa[1]->tmp.l == 1){
						if( convex(v1->co, v2->co, v3->co, v4->co) ){ 
							eed->f1 |= T2QJOIN;
							efaa[0]->f1 = 1; //mark for join
							efaa[1]->f1 = 1; //mark for join
						}
					}
					else{ 
						
						/*	The face pair is part of a 'complex' island, so the rules for dealing with it are more involved.
							Depending on what options the user has chosen, this face pair can be 'thrown out' based upon the following criteria:
							
							1: the two faces do not share the same material
							2: the edge joining the two faces is marked as sharp.
							3: the two faces UV's do not make a good match
							4: the two faces Vertex colors do not make a good match
							
							If the face pair passes all the applicable tests, it is then given a 'weight' with the measure_facepair() function.
							This measures things like concavity, colinearity ect. If this weight is below the threshold set by the user
							the edge joining them is marked as being 'complex' and will be compared against other possible pairs which contain one of the
							same faces in the current pair later.
						
							This technique is based upon an algorithm that Campbell Barton developed for his Tri2Quad script that was previously part of
							the python scripts bundled with Blender releases.
						*/
						
						if(G.scene->toolsettings->editbutflag & B_JOINTRIA_SHARP && eed->sharp); /*do nothing*/
						else if(G.scene->toolsettings->editbutflag & B_JOINTRIA_MAT && efaa[0]->mat_nr != efaa[1]->mat_nr); /*do nothing*/
						else if(((G.scene->toolsettings->editbutflag & B_JOINTRIA_UV) || (G.scene->toolsettings->editbutflag & B_JOINTRIA_VCOL)) &&
								compareFaceAttribs(efaa[0], efaa[1], eed) == 0); /*do nothing*/
						else{	
							measure = measure_facepair(v1, v2, v3, v4, limit);
							if(measure < limit){
								complexedges++;
								eed->f1 |= T2QCOMPLEX;
								eed->crease = measure; /*we dont mark edges for join yet*/
							}
						}
					}
				}
			}
		}
		
		/*Quicksort the complex edges according to their weighting*/
		if(complexedges){
			edsortblock = edb = MEM_callocN(sizeof(EditEdge*) * complexedges, "Face Pairs quicksort Array");
			for(eed = em->edges.first; eed; eed=eed->next){
				if(eed->f1 & T2QCOMPLEX){
					*edb = eed;
					edb++;
				}
			}
			qsort(edsortblock, complexedges, sizeof(EditEdge*), fplcmp);
			/*now go through and mark the edges who get the highest weighting*/
			for(edb=edsortblock, i=0; i < complexedges; edb++, i++){ 
				efaa = (EVPtr *)((*edb)->tmp.p); /*suspect!*/
				if( !efaa[0]->f1 && !efaa[1]->f1){
					efaa[0]->f1 = 1; //mark for join
					efaa[1]->f1 = 1; //mark for join
					(*edb)->f1 |= T2QJOIN;
				}
			}
		}
		
		/*finally go through all edges marked for join (simple and complex) and create new faces*/ 
		for(eed=em->edges.first; eed; eed=eed->next){
			if(eed->f1 & T2QJOIN){
				efaa= (EVPtr *)eed->tmp.p;
				v1 = v2 = v3 = v4 = NULL;
				givequadverts(efaa[0], efaa[1], &v1, &v2, &v3, &v4, vindex);
				if((v1 && v2 && v3 && v4) && (exist_face(v1, v2, v3, v4)==0)){ /*exist_face is very slow! Needs to be adressed.*/
					/*flag for delete*/
					eed->f1 |= T2QDELETE;
					/*create new quad and select*/
					efa = EM_face_from_faces(efaa[0], efaa[1], vindex[0], vindex[1], 4+vindex[2], 4+vindex[3]);
					EM_select_face(efa,1);
				}
				else{
						efaa[0]->f1 = 0;
						efaa[1]->f1 = 0;
				}
			}
		}
	}
	
	/*free data and cleanup*/
	if(creases){
		for(eed=em->edges.first, i = 0; eed; eed=eed->next, i++) eed->crease = creases[i]; 
		MEM_freeN(creases);
	}
	for(eed=em->edges.first; eed; eed=eed->next){
		if(eed->f1 & T2QDELETE) eed->f1 = 1;
		else eed->f1 = 0;
	}
	free_tagged_edges_faces(em->edges.first, em->faces.first);
	if(efaar) MEM_freeN(efaar);
	if(edsortblock) MEM_freeN(edsortblock);
		
	EM_selectmode_flush();
	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
	#endif
	waitcursor(0);
	BIF_undo_push("Convert Triangles to Quads");
}
/* ******************** END TRIANGLE TO QUAD ************************************* */

#define FACE_MARKCLEAR(f) (f->f1 = 1)

/* quick hack, basically a copy of beauty_fill */
void edge_flip(void)
{
	EditMesh *em = G.editMesh;
	EditVert *v1, *v2, *v3, *v4;
	EditEdge *eed, *nexted;
	EditFace *efa, *w;
	//void **efaar, **efaa;
	EVPTuple *efaar;
	EVPtr *efaa;
	int totedge, ok, vindex[4];
	
	/* - all selected edges with two faces
	 * - find the faces: store them in edges (using datablock)
	 * - per edge: - test convex
	 *			   - test edge: flip?
						- if true: remedge,  addedge, all edges at the edge get new face pointers
	 */

	EM_selectmode_flush();	// makes sure in selectmode 'face' the edges of selected faces are selected too 

	totedge = count_selected_edges(em->edges.first);
	if(totedge==0) return;

	/* temporary array for : edge -> face[1], face[2] */
	efaar= (EVPTuple *) MEM_callocN(totedge * sizeof(EVPTuple), "edgeflip");

	ok = collect_quadedges(efaar, em->edges.first, em->faces.first);
	
	eed= em->edges.first;
	while(eed) {
		nexted= eed->next;
		
		if(eed->f2==2) {  /* points to 2 faces */
			
			efaa= (EVPtr *) eed->tmp.p;
			
			/* don't do it if flagged */

			ok= 1;
			efa= efaa[0];
			if(efa->e1->f1 || efa->e2->f1 || efa->e3->f1) ok= 0;
			efa= efaa[1];
			if(efa->e1->f1 || efa->e2->f1 || efa->e3->f1) ok= 0;
			
			if(ok) {
				/* test convex */
				givequadverts(efaa[0], efaa[1], &v1, &v2, &v3, &v4, vindex);

/*
		4-----3		4-----3
		|\	|		|	/|
		| \ 1 |		| 1 / |
		|  \  |  ->	|  /  |	
		| 0 \ |		| / 0 | 
		|	\|		|/	|
		1-----2		1-----2
*/
				/* make new faces */
				if (v1 && v2 && v3) {
					if( convex(v1->co, v2->co, v3->co, v4->co) ) {
						if(exist_face(v1, v2, v3, v4)==0) {
							/* outch this may break seams */ 
							w= EM_face_from_faces(efaa[0], efaa[1], vindex[0],
								vindex[1], 4+vindex[2], -1);

							EM_select_face(w, 1);

							/* outch this may break seams */
							w= EM_face_from_faces(efaa[0], efaa[1], vindex[0],
								4+vindex[2], 4+vindex[3], -1);

							EM_select_face(w, 1);
						}
						/* tag as to-be-removed */
						FACE_MARKCLEAR(efaa[1]);
						FACE_MARKCLEAR(efaa[0]);
						eed->f1 = 1; 
						
					} /* endif test convex */
				}
			}
		}
		eed= nexted;
	}

	/* clear tagged edges and faces: */
	free_tagged_edges_faces(em->edges.first, em->faces.first);
	
	MEM_freeN(efaar);
	
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
#endif
	BIF_undo_push("Flip Triangle Edges");
	
}

static void edge_rotate(EditEdge *eed,int dir)
{
	EditMesh *em = G.editMesh;
	EditVert **verts[2];
	EditFace *face[2], *efa, *newFace[2];
	EditEdge **edges[2], **hiddenedges, *srchedge;
	int facecount, p1, p2, p3, p4, fac1, fac2, i, j;
	int numhidden, numshared, p[2][4];
	
	/* check to make sure that the edge is only part of 2 faces */
	facecount = 0;
	for(efa = em->faces.first;efa;efa = efa->next) {
		if((efa->e1 == eed || efa->e2 == eed) || (efa->e3 == eed || efa->e4 == eed)) {
			if(facecount >= 2) {
				/* more than two faces with this edge */
				return;
			}
			else {
				face[facecount] = efa;
				facecount++;
			}
		}
	}
 
	if(facecount < 2)
		return;

	/* how many edges does each face have */
 	if(face[0]->e4) fac1= 4;
	else fac1= 3;

	if(face[1]->e4) fac2= 4;
	else fac2= 3;
	
	/* make a handy array for verts and edges */
	verts[0]= &face[0]->v1;
	edges[0]= &face[0]->e1;
	verts[1]= &face[1]->v1;
	edges[1]= &face[1]->e1;

	/* we don't want to rotate edges between faces that share more than one edge */
	numshared= 0;
	for(i=0; i<fac1; i++)
		for(j=0; j<fac2; j++)
			if (edges[0][i] == edges[1][j])
				numshared++;

	if(numshared > 1)
		return;
	
	/* coplaner faces only please */
	if(Inpf(face[0]->n,face[1]->n) <= 0.000001)
		return;
	
	/* we want to construct an array of vertex indicis in both faces, starting at
	   the last vertex of the edge being rotated.
	   - first we find the two vertices that lie on the rotating edge
	   - then we make sure they are ordered according to the face vertex order
	   - and then we construct the array */
	p1= p2= p3= p4= 0;

	for(i=0; i<4; i++) {
		if(eed->v1 == verts[0][i]) p1 = i;
		if(eed->v2 == verts[0][i]) p2 = i;
		if(eed->v1 == verts[1][i]) p3 = i;
		if(eed->v2 == verts[1][i]) p4 = i;
	}
	
	if((p1+1)%fac1 == p2)
		SWAP(int, p1, p2);
	if((p3+1)%fac2 == p4)
		SWAP(int, p3, p4);
	
	for (i = 0; i < 4; i++) {
		p[0][i]= (p1 + i)%fac1;
		p[1][i]= (p3 + i)%fac2;
	}

	/* create an Array of the Edges who have h set prior to rotate */
	numhidden = 0;
	for(srchedge = em->edges.first;srchedge;srchedge = srchedge->next)
		if(srchedge->h && ((srchedge->v1->f & SELECT) || (srchedge->v2->f & SELECT)))
			numhidden++;

	hiddenedges = MEM_mallocN(sizeof(EditVert*)*numhidden+1, "RotateEdgeHiddenVerts");
	if(!hiddenedges) {
        error("Malloc Was not happy!");
        return;   
    }

    numhidden = 0;
	for(srchedge=em->edges.first; srchedge; srchedge=srchedge->next)
		if(srchedge->h && (srchedge->v1->f & SELECT || srchedge->v2->f & SELECT))
			hiddenedges[numhidden++] = srchedge;

	/* create the 2 new faces */
	if(fac1 == 3 && fac2 == 3) {
		/* no need of reverse setup */

		newFace[0]= EM_face_from_faces(face[0], face[1], p[0][1], p[0][2], 4+p[1][1], -1);
		newFace[1]= EM_face_from_faces(face[1], face[0], p[1][1], p[1][2], 4+p[0][1], -1);
	}
	else if(fac1 == 4 && fac2 == 3) {
		if(dir == 1) {
			newFace[0]= EM_face_from_faces(face[0], face[1], p[0][1], p[0][2], p[0][3], 4+p[1][1]);
			newFace[1]= EM_face_from_faces(face[1], face[0], p[1][1], p[1][2], 4+p[0][1], -1);
		} else if (dir == 2) {
			newFace[0]= EM_face_from_faces(face[0], face[1], p[0][2], 4+p[1][1], p[0][0], p[0][1]);
			newFace[1]= EM_face_from_faces(face[1], face[0], 4+p[0][2], p[1][0], p[1][1], -1);
			
			verts[0][p[0][2]]->f |= SELECT;
			verts[1][p[1][1]]->f |= SELECT;		
		}
	}
	else if(fac1 == 3 && fac2 == 4) {
		if(dir == 1) {
			newFace[0]= EM_face_from_faces(face[0], face[1], p[0][1], p[0][2], 4+p[1][1], -1);
			newFace[1]= EM_face_from_faces(face[1], face[0], p[1][1], p[1][2], p[1][3], 4+p[0][1]);
		} else if (dir == 2) {
			newFace[0]= EM_face_from_faces(face[0], face[1], p[0][0], p[0][1], 4+p[1][2], -1);
			newFace[1]= EM_face_from_faces(face[1], face[0], p[1][1], p[1][2], 4+p[0][1], 4+p[0][2]);
			
			verts[0][p[0][1]]->f |= SELECT;
			verts[1][p[1][2]]->f |= SELECT;	
		}
	
	}
	else if(fac1 == 4 && fac2 == 4) {
		if(dir == 1) {
			newFace[0]= EM_face_from_faces(face[0], face[1], p[0][1], p[0][2], p[0][3], 4+p[1][1]);
			newFace[1]= EM_face_from_faces(face[1], face[0], p[1][1], p[1][2], p[1][3], 4+p[0][1]);
		} else if (dir == 2) {
			newFace[0]= EM_face_from_faces(face[0], face[1], p[0][2], p[0][3], 4+p[1][1], 4+p[1][2]);
			newFace[1]= EM_face_from_faces(face[1], face[0], p[1][2], p[1][3], 4+p[0][1], 4+p[0][2]);
			
			verts[0][p[0][2]]->f |= SELECT;
			verts[1][p[1][2]]->f |= SELECT;	
		}
	}		
	else
		return; /* This should never happen */

	if(dir == 1 || (fac1 == 3 && fac2 == 3)) {
		verts[0][p[0][1]]->f |= SELECT;
		verts[1][p[1][1]]->f |= SELECT;
	}
	
	/* copy old edge's flags to new center edge*/
	for(srchedge=em->edges.first;srchedge;srchedge=srchedge->next) {
		if((srchedge->v1->f & SELECT) && (srchedge->v2->f & SELECT)) {
			srchedge->f = eed->f;
			srchedge->h = eed->h;
			srchedge->dir = eed->dir;
			srchedge->seam = eed->seam;
			srchedge->crease = eed->crease;
			srchedge->bweight = eed->bweight;
		}
	}
	
	/* resetting hidden flag */
	for(numhidden--; numhidden>=0; numhidden--)
		hiddenedges[numhidden]->h= 1;
	
	/* check for orhphan edges */
	for(srchedge=em->edges.first; srchedge; srchedge=srchedge->next)
		srchedge->f1= -1;   
	
	/* cleanup */
	MEM_freeN(hiddenedges);
	
	/* get rid of the old edge and faces*/
	remedge(eed);
	free_editedge(eed);	
	BLI_remlink(&em->faces, face[0]);
	free_editface(face[0]);	
	BLI_remlink(&em->faces, face[1]);
	free_editface(face[1]);		
}

/* only accepts 1 selected edge, or 2 selected faces */
void edge_rotate_selected(int dir)
{
	EditEdge *eed;
	EditFace *efa;
	short edgeCount = 0;
	
	/*clear new flag for new edges, count selected edges */
	for(eed= G.editMesh->edges.first; eed; eed= eed->next) {
		eed->f1= 0;
		eed->f2 &= ~2;
		if(eed->f & SELECT) edgeCount++;	
	}
	
	if(edgeCount>1) {
		/* more selected edges, check faces */
		for(efa= G.editMesh->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				efa->e1->f1++;
				efa->e2->f1++;
				efa->e3->f1++;
				if(efa->e4) efa->e4->f1++;
			}
		}
		edgeCount= 0;
		for(eed= G.editMesh->edges.first; eed; eed= eed->next) {
			if(eed->f1==2) edgeCount++;
		}
		if(edgeCount==1) {
			for(eed= G.editMesh->edges.first; eed; eed= eed->next) {
				if(eed->f1==2) {
					edge_rotate(eed,dir);
					break;
				}
			}
		}
		else error("Select one edge or two adjacent faces");
	}
	else if(edgeCount==1) {
		for(eed= G.editMesh->edges.first; eed; eed= eed->next) {
			if(eed->f & SELECT) {
				EM_select_edge(eed, 0);
				edge_rotate(eed,dir);
				break;
			}
		}
	}
	else error("Select one edge or two adjacent faces");
	

	/* flush selected vertices (again) to edges/faces */
	EM_select_flush();
	
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);

#ifdef WITH_VERSE
	if(G.editMesh->vnode)
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
#endif

	BIF_undo_push("Rotate Edge");	
}

/******************* BEVEL CODE STARTS HERE ********************/

static void bevel_displace_vec(float *midvec, float *v1, float *v2, float *v3, float d, float no[3])
{
	float a[3], c[3], n_a[3], n_c[3], mid[3], ac, ac2, fac;

	VecSubf(a, v1, v2);
	VecSubf(c, v3, v2);

	Crossf(n_a, a, no);
	Normalize(n_a);
	Crossf(n_c, no, c);
	Normalize(n_c);

	Normalize(a);
	Normalize(c);
	ac = Inpf(a, c);

	if (ac == 1 || ac == -1) {
		midvec[0] = midvec[1] = midvec[2] = 0;
		return;
	}
	ac2 = ac * ac;
	fac = (float)sqrt((ac2 + 2*ac + 1)/(1 - ac2) + 1);
	VecAddf(mid, n_c, n_a);
	Normalize(mid);
	VecMulf(mid, d * fac);
	VecAddf(mid, mid, v2);
	VecCopyf(midvec, mid);
}

/*	Finds the new point using the sinus law to extrapolate a triangle
	Lots of sqrts which would not be good for a real time algo
	Using the mid  point of the extrapolation of both sides 
	Useless for coplanar quads, but that doesn't happen too often */
static void fix_bevel_wrap(float *midvec, float *v1, float *v2, float *v3, float *v4, float d, float no[3]) 
{
	float a[3], b[3], c[3], l_a, l_b, l_c, s_a, s_b, s_c, Pos1[3], Pos2[3], Dir[3];

	VecSubf(a, v3, v2);
	l_a = Normalize(a);
	VecSubf(b, v4, v3);
	Normalize(b);
	VecSubf(c, v1, v2);
	Normalize(c);

	s_b = Inpf(a, c);
	s_b = (float)sqrt(1 - (s_b * s_b));
	s_a = Inpf(b, c);
	s_a = (float)sqrt(1 - (s_a * s_a));
	VecMulf(a, -1);
	s_c = Inpf(a, b);
	s_c = (float)sqrt(1 - (s_c * s_c));

	l_b = s_b * l_a / s_a;
	l_c = s_c * l_a / s_a;

	VecMulf(b, l_b);
	VecMulf(c, l_c);

	VecAddf(Pos1, v2, c);
	VecAddf(Pos2, v3, b);

	VecAddf(Dir, Pos1, Pos2);
	VecMulf(Dir, 0.5);

	bevel_displace_vec(midvec, v3, Dir, v2, d, no);

}


static char detect_wrap(float *o_v1, float *o_v2, float *v1, float *v2, float *no) 
{
	float o_a[3], a[3], o_c[3], c[3];

	VecSubf(o_a, o_v1, o_v2);
	VecSubf(a, v1, v2);

	Crossf(o_c, o_a, no);
	Crossf(c, a, no);

	if (Inpf(c, o_c) <= 0)
		return 1;
	else
		return 0;
}

// Detects and fix a quad wrapping after the resize
// Arguments are the orginal verts followed by the final verts and then the bevel size and the normal
static void fix_bevel_quad_wrap(float *o_v1, float *o_v2, float *o_v3, float *o_v4, float *v1, float *v2, float *v3, float *v4, float d, float *no) 
{
	float vec[3];
	char wrap[4];
	
	// Quads can wrap partially. Watch out
	wrap[0] = detect_wrap(o_v1, o_v2, v1, v2, no); // Edge 1-2
	wrap[1] = detect_wrap(o_v2, o_v3, v2, v3, no); // Edge 2-3
	wrap[2] = detect_wrap(o_v3, o_v4, v3, v4, no); // Edge 3-4
	wrap[3] = detect_wrap(o_v4, o_v1, v4, v1, no); // Edge 4-1

	// Edge 1 inverted
	if (wrap[0] == 1 && wrap[1] == 0 && wrap[2] == 0 && wrap[3] == 0) {
		fix_bevel_wrap(vec, o_v2, o_v3, o_v4, o_v1, d, no);
		VECCOPY(v1, vec);
		VECCOPY(v2, vec);
	}
	// Edge 2 inverted
	else if (wrap[0] == 0 && wrap[1] == 1 && wrap[2] == 0 && wrap[3] == 0) {
		fix_bevel_wrap(vec, o_v3, o_v4, o_v1, o_v2, d, no);
		VECCOPY(v2, vec);
		VECCOPY(v3, vec);
	}
	// Edge 3 inverted
	else if (wrap[0] == 0 && wrap[1] == 0 && wrap[2] == 1 && wrap[3] == 0) {
		fix_bevel_wrap(vec, o_v4, o_v1, o_v2, o_v3, d, no);
		VECCOPY(v3, vec);
		VECCOPY(v4, vec);
	}
	// Edge 4 inverted
	else if (wrap[0] == 0 && wrap[1] == 0 && wrap[2] == 0 && wrap[3] == 1) {
		fix_bevel_wrap(vec, o_v1, o_v2, o_v3, o_v4, d, no);
		VECCOPY(v4, vec);
		VECCOPY(v1, vec);
	}
	// Edge 2 and 4 inverted
	else if (wrap[0] == 0 && wrap[1] == 1 && wrap[2] == 0 && wrap[3] == 1) {
		VecAddf(vec, v2, v3);
		VecMulf(vec, 0.5);
		VECCOPY(v2, vec);
		VECCOPY(v3, vec);
		VecAddf(vec, v1, v4);
		VecMulf(vec, 0.5);
		VECCOPY(v1, vec);
		VECCOPY(v4, vec);
	}
	// Edge 1 and 3 inverted
	else if (wrap[0] == 1 && wrap[1] == 0 && wrap[2] == 1 && wrap[3] == 0) {
		VecAddf(vec, v1, v2);
		VecMulf(vec, 0.5);
		VECCOPY(v1, vec);
		VECCOPY(v2, vec);
		VecAddf(vec, v3, v4);
		VecMulf(vec, 0.5);
		VECCOPY(v3, vec);
		VECCOPY(v4, vec);
	}
	// Totally inverted
	else if (wrap[0] == 1 && wrap[1] == 1 && wrap[2] == 1 && wrap[3] == 1) {
		VecAddf(vec, v1, v2);
		VecAddf(vec, vec, v3);
		VecAddf(vec, vec, v4);
		VecMulf(vec, 0.25);
		VECCOPY(v1, vec);
		VECCOPY(v2, vec);
		VECCOPY(v3, vec);
		VECCOPY(v4, vec);
	}

}

// Detects and fix a tri wrapping after the resize
// Arguments are the orginal verts followed by the final verts and the normal
// Triangles cannot wrap partially (not in this situation
static void fix_bevel_tri_wrap(float *o_v1, float *o_v2, float *o_v3, float *v1, float *v2, float *v3, float *no) 
{
	if (detect_wrap(o_v1, o_v2, v1, v2, no)) {
		float vec[3];
		VecAddf(vec, o_v1, o_v2);
		VecAddf(vec, vec, o_v3);
		VecMulf(vec, 1.0f/3.0f);
		VECCOPY(v1, vec);
		VECCOPY(v2, vec);
		VECCOPY(v3, vec);
	}
}

static void bevel_shrink_faces(float d, int flag)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	float vec[3], no[3], v1[3], v2[3], v3[3], v4[3];

	/* move edges of all faces with efa->f1 & flag closer towards their centers */
	efa= em->faces.first;
	while (efa) {
		if (efa->f1 & flag) {
			VECCOPY(v1, efa->v1->co);
			VECCOPY(v2, efa->v2->co);			
			VECCOPY(v3, efa->v3->co);	
			VECCOPY(no, efa->n);
			if (efa->v4 == NULL) {
				bevel_displace_vec(vec, v1, v2, v3, d, no);
				VECCOPY(efa->v2->co, vec);
				bevel_displace_vec(vec, v2, v3, v1, d, no);
				VECCOPY(efa->v3->co, vec);		
				bevel_displace_vec(vec, v3, v1, v2, d, no);
				VECCOPY(efa->v1->co, vec);

				fix_bevel_tri_wrap(v1, v2, v3, efa->v1->co, efa->v2->co, efa->v3->co, no);
			} else {
				VECCOPY(v4, efa->v4->co);
				bevel_displace_vec(vec, v1, v2, v3, d, no);
				VECCOPY(efa->v2->co, vec);
				bevel_displace_vec(vec, v2, v3, v4, d, no);
				VECCOPY(efa->v3->co, vec);		
				bevel_displace_vec(vec, v3, v4, v1, d, no);
				VECCOPY(efa->v4->co, vec);		
				bevel_displace_vec(vec, v4, v1, v2, d, no);
				VECCOPY(efa->v1->co, vec);

				fix_bevel_quad_wrap(v1, v2, v3, v4, efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co, d, no);
			}
		}
		efa= efa->next;
	}	
}

static void bevel_shrink_draw(float d, int flag)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	float vec[3], no[3], v1[3], v2[3], v3[3], v4[3], fv1[3], fv2[3], fv3[3], fv4[3];

	/* move edges of all faces with efa->f1 & flag closer towards their centers */
	efa= em->faces.first;
	while (efa) {
		VECCOPY(v1, efa->v1->co);
		VECCOPY(v2, efa->v2->co);			
		VECCOPY(v3, efa->v3->co);	
		VECCOPY(no, efa->n);
		if (efa->v4 == NULL) {
			bevel_displace_vec(vec, v1, v2, v3, d, no);
			VECCOPY(fv2, vec);
			bevel_displace_vec(vec, v2, v3, v1, d, no);
			VECCOPY(fv3, vec);		
			bevel_displace_vec(vec, v3, v1, v2, d, no);
			VECCOPY(fv1, vec);

			fix_bevel_tri_wrap(v1, v2, v3, fv1, fv2, fv3, no);

			glBegin(GL_LINES);
			glVertex3fv(fv1);
			glVertex3fv(fv2);
			glEnd();						
			glBegin(GL_LINES);
			glVertex3fv(fv2);
			glVertex3fv(fv3);
			glEnd();						
			glBegin(GL_LINES);
			glVertex3fv(fv1);
			glVertex3fv(fv3);
			glEnd();						
		} else {
			VECCOPY(v4, efa->v4->co);
			bevel_displace_vec(vec, v4, v1, v2, d, no);
			VECCOPY(fv1, vec);
			bevel_displace_vec(vec, v1, v2, v3, d, no);
			VECCOPY(fv2, vec);
			bevel_displace_vec(vec, v2, v3, v4, d, no);
			VECCOPY(fv3, vec);		
			bevel_displace_vec(vec, v3, v4, v1, d, no);
			VECCOPY(fv4, vec);		

			fix_bevel_quad_wrap(v1, v2, v3, v4, fv1, fv2, fv3, fv4, d, no);

			glBegin(GL_LINES);
			glVertex3fv(fv1);
			glVertex3fv(fv2);
			glEnd();						
			glBegin(GL_LINES);
			glVertex3fv(fv2);
			glVertex3fv(fv3);
			glEnd();						
			glBegin(GL_LINES);
			glVertex3fv(fv3);
			glVertex3fv(fv4);
			glEnd();						
			glBegin(GL_LINES);
			glVertex3fv(fv1);
			glVertex3fv(fv4);
			glEnd();						
		}
		efa= efa->next;
	}	
}

static void bevel_mesh(float bsize, int allfaces)
{
	EditMesh *em = G.editMesh;
//#define BEV_DEBUG
/* Enables debug printfs and assigns material indices: */
/* 2 = edge quad									   */
/* 3 = fill polygon (vertex clusters)				  */

	EditFace *efa, *example; //, *nextvl;
	EditEdge *eed, *eed2;
	EditVert *neweve[1024], *eve, *eve2, *eve3, *v1, *v2, *v3, *v4; //, *eve4;
	//short found4,  search;
	//float f1, f2, f3, f4;
	float cent[3], min[3], max[3];
	int a, b, c;
	float limit= 0.001f;
	
	if(multires_test()) return;

	waitcursor(1);

	removedoublesflag(1, 0, limit);

	/* tag all original faces */
	efa= em->faces.first;
	while (efa) {
		efa->f1= 0;
		if (faceselectedAND(efa, 1)||allfaces) {
			efa->f1= 1;
			efa->v1->f |= 128;
			efa->v2->f |= 128;
			efa->v3->f |= 128;
			if (efa->v4) efa->v4->f |= 128;			
		}
		efa->v1->f &= ~64;
		efa->v2->f &= ~64;
		efa->v3->f &= ~64;
		if (efa->v4) efa->v4->f &= ~64;		

		efa= efa->next;
	}

#ifdef BEV_DEBUG
	fprintf(stderr,"bevel_mesh: split\n");
#endif
	
	efa= em->faces.first;
	while (efa) {
		if (efa->f1 & 1) {
			efa->f1-= 1;
			v1= addvertlist(efa->v1->co, efa->v1);
			v1->f= efa->v1->f & ~128;
   			efa->v1->tmp.v = v1;

			v1= addvertlist(efa->v2->co, efa->v2);
			v1->f= efa->v2->f & ~128;
   			efa->v2->tmp.v = v1;

			v1= addvertlist(efa->v3->co, efa->v3);
			v1->f= efa->v3->f & ~128;
   			efa->v3->tmp.v = v1;

			if (efa->v4) {
				v1= addvertlist(efa->v4->co, efa->v4);
				v1->f= efa->v4->f & ~128;
	   			efa->v4->tmp.v = v1;
			}

			/* Needs better adaption of creases? */
   			addedgelist(efa->e1->v1->tmp.v, 
						efa->e1->v2->tmp.v, 
						efa->e1);
   			addedgelist(efa->e2->v1->tmp.v,
						efa->e2->v2->tmp.v, 
						efa->e2);
   			addedgelist(efa->e3->v1->tmp.v,
						efa->e3->v2->tmp.v,
						efa->e3);
   			if (efa->e4) addedgelist(efa->e4->v1->tmp.v,
									 efa->e4->v2->tmp.v,
									 efa->e4);

   			if(efa->v4) {
				v1 = efa->v1->tmp.v;
				v2 = efa->v2->tmp.v;
				v3 = efa->v3->tmp.v;
				v4 = efa->v4->tmp.v;
				addfacelist(v1, v2, v3, v4, efa,NULL);
   			} else {
   				v1= efa->v1->tmp.v;
   				v2= efa->v2->tmp.v;
   				v3= efa->v3->tmp.v;
   				addfacelist(v1, v2, v3, 0, efa,NULL);
   			}

			efa= efa-> next;
		} else {
			efa= efa->next;
		}
	}
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		if( (efa->v1->f & 128) && (efa->v2->f & 128) && (efa->v3->f & 128) ) {
			if(efa->v4==NULL || (efa->v4->f & 128)) efa->f |= 128;
		}
	}

	delfaceflag(128); // works with face flag now

	/* tag all faces for shrink*/
	efa= em->faces.first;
	while (efa) {
		if (faceselectedAND(efa, 1)||allfaces) {
			efa->f1= 2;
		}
		efa= efa->next;
	}

#ifdef BEV_DEBUG
	fprintf(stderr,"bevel_mesh: make edge quads\n");
#endif

	/* find edges that are on each other and make quads between them */

	eed= em->edges.first;
	while(eed) {
		eed->f2= eed->f1= 0;
		if ( ((eed->v1->f & eed->v2->f) & 1) || allfaces) 
			eed->f1 |= 4;	/* original edges */
		eed->tmp.v = 0;
		eed= eed->next;
	}

	eed= em->edges.first;
	while (eed) {
		if ( ((eed->f1 & 2)==0) && (eed->f1 & 4) ) {
			eed2= em->edges.first;
			while (eed2) {
				if ( (eed2 != eed) && ((eed2->f1 & 2)==0) && (eed->f1 & 4) ) {
					if (
					   (eed->v1 != eed2->v1) &&
					   (eed->v1 != eed2->v2) &&					   
					   (eed->v2 != eed2->v1) &&
					   (eed->v2 != eed2->v2) &&	(
					   ( VecCompare(eed->v1->co, eed2->v1->co, limit) &&
						 VecCompare(eed->v2->co, eed2->v2->co, limit) ) ||
					   ( VecCompare(eed->v1->co, eed2->v2->co, limit) &&
						 VecCompare(eed->v2->co, eed2->v1->co, limit) ) ) )
						{
						
#ifdef BEV_DEBUG						
						fprintf(stderr, "bevel_mesh: edge quad\n");
#endif						
						
						eed->f1 |= 2;	/* these edges are finished */
						eed2->f1 |= 2;
						
						example= NULL;
						efa= em->faces.first;	/* search example face (for mat_nr, ME_SMOOTH, ...) */
						while (efa) {
							if ( (efa->e1 == eed) ||
								 (efa->e2 == eed) ||
								 (efa->e3 == eed) ||
								 (efa->e4 && (efa->e4 == eed)) ) {
								example= efa;
								efa= NULL;
							}
							if (efa) efa= efa->next;
						}
						
						neweve[0]= eed->v1; neweve[1]= eed->v2;
						neweve[2]= eed2->v1; neweve[3]= eed2->v2;
						
						if(exist_face(neweve[0], neweve[1], neweve[2], neweve[3])==0) {
							efa= NULL;

							if (VecCompare(eed->v1->co, eed2->v2->co, limit)) {
								efa= addfacelist(neweve[0], neweve[1], neweve[2], neweve[3], example,NULL);
							} else {
								efa= addfacelist(neweve[0], neweve[2], neweve[3], neweve[1], example,NULL);
							}
						
							if(efa) {
								float inp;	
								CalcNormFloat(efa->v1->co, efa->v2->co, efa->v3->co, efa->n);
								inp= efa->n[0]*G.vd->viewmat[0][2] + efa->n[1]*G.vd->viewmat[1][2] + efa->n[2]*G.vd->viewmat[2][2];
								if(inp < 0.0) flipface(efa);
#ifdef BEV_DEBUG
								efa->mat_nr= 1;
#endif
							} else fprintf(stderr,"bevel_mesh: error creating face\n");
						}
						eed2= NULL;
					}
				}
				if (eed2) eed2= eed2->next;
			}
		}
		eed= eed->next;
	}

	eed= em->edges.first;
	while(eed) {
		eed->f2= eed->f1= 0;
		eed->f1= 0;
		eed->v1->f1 &= ~1;
		eed->v2->f1 &= ~1;		
		eed->tmp.v = 0;
		eed= eed->next;
	}

#ifdef BEV_DEBUG
	fprintf(stderr,"bevel_mesh: find clusters\n");
#endif	

	/* Look for vertex clusters */

	eve= em->verts.first;
	while (eve) {
		eve->f &= ~(64|128);
		eve->tmp.v = NULL;
		eve= eve->next;
	}
	
	/* eve->f: 128: first vertex in a list (->tmp.v) */
	/*		  64: vertex is in a list */
	
	eve= em->verts.first;
	while (eve) {
		eve2= em->verts.first;
		eve3= NULL;
		while (eve2) {
			if ((eve2 != eve) && ((eve2->f & (64|128))==0)) {
				if (VecCompare(eve->co, eve2->co, limit)) {
					if ((eve->f & (128|64)) == 0) {
						/* fprintf(stderr,"Found vertex cluster:\n  *\n  *\n"); */
						eve->f |= 128;
						eve->tmp.v = eve2;
						eve3= eve2;
					} else if ((eve->f & 64) == 0) {
						/* fprintf(stderr,"  *\n"); */
						if (eve3) eve3->tmp.v = eve2;
						eve2->f |= 64;
						eve3= eve2;
					}
				}
			}
			eve2= eve2->next;
			if (!eve2) {
				if (eve3) eve3->tmp.v = NULL;
			}
		}
		eve= eve->next;
	}

#ifdef BEV_DEBUG
	fprintf(stderr,"bevel_mesh: shrink faces\n");
#endif	

	bevel_shrink_faces(bsize, 2);

#ifdef BEV_DEBUG
	fprintf(stderr,"bevel_mesh: fill clusters\n");
#endif
	
	/* Make former vertex clusters faces */

	eve= em->verts.first;
	while (eve) {
		eve->f &= ~64;
		eve= eve->next;
	}

	eve= em->verts.first;
	while (eve) {
		if (eve->f & 128) {
			eve->f &= ~128;
			a= 0;
			neweve[a]= eve;
			eve2 = eve->tmp.v;
			while (eve2) {
				a++;
				neweve[a]= eve2;
				eve2 = eve2->tmp.v;
			}
			a++;
			efa= NULL;
			if (a>=3) {
				example= NULL;
				efa= em->faces.first;	/* search example face */
				while (efa) {
					if ( (efa->v1 == neweve[0]) ||
						 (efa->v2 == neweve[0]) ||
						 (efa->v3 == neweve[0]) ||
						 (efa->v4 && (efa->v4 == neweve[0])) ) {
						example= efa;
						efa= NULL;
					}
					if (efa) efa= efa->next;
				}
#ifdef BEV_DEBUG				
				fprintf(stderr,"bevel_mesh: Making %d-gon\n", a);
#endif				
				if (a>4) {
					cent[0]= cent[1]= cent[2]= 0.0;				
					INIT_MINMAX(min, max);				
					for (b=0; b<a; b++) {
						VecAddf(cent, cent, neweve[b]->co);
						DO_MINMAX(neweve[b]->co, min, max);
					}
					cent[0]= (min[0]+max[0])/2;
					cent[1]= (min[1]+max[1])/2;
					cent[2]= (min[2]+max[2])/2;
					eve2= addvertlist(cent, NULL);
					eve2->f |= 1;
					eed= em->edges.first;
					while (eed) {
						c= 0;
						for (b=0; b<a; b++) 
							if ((neweve[b]==eed->v1) || (neweve[b]==eed->v2)) c++;
						if (c==2) {
							if(exist_face(eed->v1, eed->v2, eve2, 0)==0) {
								efa= addfacelist(eed->v1, eed->v2, eve2, 0, example,NULL);
#ifdef BEV_DEBUG
								efa->mat_nr= 2;
#endif								
							}
						}
						eed= eed->next;
					}
				} else if (a==4) {
					if(exist_face(neweve[0], neweve[1], neweve[2], neweve[3])==0) {
						/* the order of vertices can be anything, three cases to check */
						if( convex(neweve[0]->co, neweve[1]->co, neweve[2]->co, neweve[3]->co) ) {
							efa= addfacelist(neweve[0], neweve[1], neweve[2], neweve[3], NULL, NULL);
						}
						else if( convex(neweve[0]->co, neweve[2]->co, neweve[3]->co, neweve[1]->co) ) {
							efa= addfacelist(neweve[0], neweve[2], neweve[3], neweve[1], NULL, NULL);
						}
						else if( convex(neweve[0]->co, neweve[2]->co, neweve[1]->co, neweve[3]->co) ) {
							efa= addfacelist(neweve[0], neweve[2], neweve[1], neweve[3], NULL, NULL);
						}
					}				
				}
				else if (a==3) {
					if(exist_face(neweve[0], neweve[1], neweve[2], 0)==0)
						efa= addfacelist(neweve[0], neweve[1], neweve[2], 0, example, NULL);
				}
				if(efa) {
					float inp;	
					CalcNormFloat(neweve[0]->co, neweve[1]->co, neweve[2]->co, efa->n);
					inp= efa->n[0]*G.vd->viewmat[0][2] + efa->n[1]*G.vd->viewmat[1][2] + efa->n[2]*G.vd->viewmat[2][2];
					if(inp < 0.0) flipface(efa);
#ifdef BEV_DEBUG
						efa->mat_nr= 2;
#endif												
				}				
			}
		}
		eve= eve->next;
	}

	eve= em->verts.first;
	while (eve) {
		eve->f1= 0;
		eve->f &= ~(128|64);
		eve->tmp.v= NULL;
		eve= eve->next;
	}
	
	recalc_editnormals();
	waitcursor(0);
	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	
	removedoublesflag(1, 0, limit);

	/* flush selected vertices to edges/faces */
	EM_select_flush();

#undef BEV_DEBUG
}

static void bevel_mesh_recurs(float bsize, short recurs, int allfaces) 
{
	float d;
	short nr;

	d= bsize;
	for (nr=0; nr<recurs; nr++) {
		bevel_mesh(d, allfaces);
		if (nr==0) d /= 3; else d /= 2;
	}
}

void bevel_menu() {
	BME_Mesh *bm;
	BME_TransData_Head *td;
	TransInfo *t;
	int options, res, gbm_free = 0;

	t = BIF_GetTransInfo();
	if (!G.editBMesh) {
		G.editBMesh = MEM_callocN(sizeof(*(G.editBMesh)),"bevel_menu() G.editBMesh");
		gbm_free = 1;
	}

	G.editBMesh->options = BME_BEVEL_RUNNING | BME_BEVEL_SELECT;
	G.editBMesh->res = 1;

	while(G.editBMesh->options & BME_BEVEL_RUNNING) {
		options = G.editBMesh->options;
		res = G.editBMesh->res;
		bm = BME_make_mesh();
		bm = BME_editmesh_to_bmesh(G.editMesh, bm);
		BIF_undo_push("Pre-Bevel");
		free_editMesh(G.editMesh);
		BME_bevel(bm,0.1f,res,options,0,0,&td);
		BME_bmesh_to_editmesh(bm, td);
		EM_selectmode_flush();
		G.editBMesh->bm = bm;
		G.editBMesh->td = td;
		initTransform(TFM_BEVEL,CTX_BMESH);
		Transform();
		BME_free_transdata(td);
		BME_free_mesh(bm);
		if (t->state != TRANS_CONFIRM) {
			BIF_undo();
		}
		if (options == G.editBMesh->options) {
			G.editBMesh->options &= ~BME_BEVEL_RUNNING;
		}
	}

	if (gbm_free) {
		MEM_freeN(G.editBMesh);
		G.editBMesh = NULL;
	}
}


void bevel_menu_old()
{
	char Finished = 0, Canceled = 0, str[100], Recalc = 0;
	short mval[2], oval[2], curval[2], event = 0, recurs = 1, nr;
	float vec[3], d, drawd=0.0, center[3], fac = 1;

	getmouseco_areawin(mval);
	oval[0] = mval[0]; oval[1] = mval[1];

	// Silly hackish code to initialise the variable (warning if not done)
	// while still drawing in the first iteration (and without using another variable)
	curval[0] = mval[0] + 1; curval[1] = mval[1] + 1;

	// Init grabz for window to vec conversions
	initgrabz(-G.vd->ofs[0], -G.vd->ofs[1], -G.vd->ofs[2]);
	window_to_3d(center, mval[0], mval[1]);

	if(button(&recurs, 1, 4, "Recursion:")==0) return;

	for (nr=0; nr<recurs-1; nr++) {
		if (nr==0) fac += 1.0f/3.0f; else fac += 1.0f/(3 * nr * 2.0f);
	}
	
	EM_set_flag_all(SELECT);
		
	SetBlenderCursor(SYSCURSOR);

	while (Finished == 0)
	{
		getmouseco_areawin(mval);
		if (mval[0] != curval[0] || mval[1] != curval[1] || (Recalc == 1))
		{
			Recalc = 0;
			curval[0] = mval[0];
			curval[1] = mval[1];
			
			window_to_3d(vec, mval[0]-oval[0], mval[1]-oval[1]);
			d = Normalize(vec) / 10;


			drawd = d * fac;
			if (G.qual & LR_CTRLKEY)
				drawd = (float) floor(drawd * 10.0f)/10.0f;
			if (G.qual & LR_SHIFTKEY)
				drawd /= 10;

			/*------------- Preview lines--------------- */
			
			/* uses callback mechanism to draw it all in current area */
			scrarea_do_windraw(curarea);			
			
			/* set window matrix to perspective, default an area returns with buttons transform */
			persp(PERSP_VIEW);
			/* make a copy, for safety */
			glPushMatrix();
			/* multiply with the object transformation */
			mymultmatrix(G.obedit->obmat);
			
			glColor3ub(255, 255, 0);

			// PREVIEW CODE GOES HERE
			bevel_shrink_draw(drawd, 2);

			/* restore matrix transform */
			glPopMatrix();
			
			sprintf(str, "Bevel Size: %.4f		LMB to confirm, RMB to cancel, SPACE to input directly.", drawd);
			headerprint(str);

			/* this also verifies other area/windows for clean swap */
			screen_swapbuffers();

			persp(PERSP_WIN);
			
			glDrawBuffer(GL_FRONT);
			
			BIF_ThemeColor(TH_WIRE);

			setlinestyle(3);
			glBegin(GL_LINE_STRIP); 
				glVertex2sv(mval); 
				glVertex2sv(oval); 
			glEnd();
			setlinestyle(0);
			
			persp(PERSP_VIEW);
			bglFlush(); // flush display for frontbuffer
			glDrawBuffer(GL_BACK);
		}
		while(qtest()) {
			short val=0;			
			event= extern_qread(&val);	// extern_qread stores important events for the mainloop to handle 

			/* val==0 on key-release event */
			if(val && (event==ESCKEY || event==RIGHTMOUSE || event==LEFTMOUSE || event==RETKEY || event==ESCKEY)) {
				if (event==RIGHTMOUSE || event==ESCKEY)
					Canceled = 1;
				Finished = 1;
			}
			else if (val && event==SPACEKEY) {
				if (fbutton(&d, 0.000, 10.000, 10, 0, "Width:")!=0) {
					drawd = d * fac;
					Finished = 1;
				}
			}
			else if (val) {
				/* On any other keyboard event, recalc */
				Recalc = 1;
			}

		}	
	}
	if (Canceled==0) {
		SetBlenderCursor(BC_WAITCURSOR);
		bevel_mesh_recurs(drawd/fac, recurs, 1);
		righthandfaces(1);
		SetBlenderCursor(SYSCURSOR);
		BIF_undo_push("Bevel");
	}
}

/* *********** END BEVEL *********/

typedef struct SlideVert {
	EditEdge *up,*down;
	EditVert origvert;
} SlideVert;

int EdgeLoopDelete(void) {
	if(!EdgeSlide(1, 1)) {
		return 0;
	}
	EM_select_more();
	removedoublesflag(1,0, 0.001);
	EM_select_flush();
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	return 1;
}

int EdgeSlide(short immediate, float imperc)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	EditEdge *eed,*first=NULL,*last=NULL, *temp = NULL;
	EditVert *ev, *nearest;
	LinkNode *edgelist = NULL, *vertlist=NULL, *look;
	GHash *vertgh;
	SlideVert *tempsv;
	float perc = 0, percp = 0,vertdist, projectMat[4][4], viewMat[4][4];
	float shiftlabda= 0.0f,len = 0.0f;
	int i = 0,j, numsel, numadded=0, timesthrough = 0, vertsel=0, prop=1, cancel = 0,flip=0;
	int wasshift = 0;
	short event, draw=1;
	short mval[2], mvalo[2];
	char str[128]; 
	float labda = 0.0f;
	
	view3d_get_object_project_mat(curarea, G.obedit, projectMat, viewMat);
	
	mvalo[0] = -1; mvalo[1] = -1; 
	numsel =0;  
	
	// Get number of selected edges and clear some flags
	for(eed=em->edges.first;eed;eed=eed->next) {
		eed->f1 = 0;
		eed->f2 = 0;   
		if(eed->f & SELECT) numsel++;
	}
	
	for(ev=em->verts.first;ev;ev=ev->next) {
		ev->f1 = 0;   
	} 
	
	//Make sure each edge only has 2 faces
	// make sure loop doesn't cross face
	for(efa=em->faces.first;efa;efa=efa->next) {
		int ct = 0;
		if(efa->e1->f & SELECT) {
			ct++;
			efa->e1->f1++;
			if(efa->e1->f1 > 2) {
				error("3+ face edge");
				return 0;				 
			}
		}
		if(efa->e2->f & SELECT) {
			ct++;
			efa->e2->f1++;
			if(efa->e2->f1 > 2) {
				error("3+ face edge");
				return 0;				 
			}
		}
		if(efa->e3->f & SELECT) {
			ct++;
			efa->e3->f1++;
			if(efa->e3->f1 > 2) {
				error("3+ face edge");
				return 0;				 
			}
		}
		if(efa->e4 && efa->e4->f & SELECT) {
			ct++;
			efa->e4->f1++;
			if(efa->e4->f1 > 2) {
				error("3+ face edge");
				return 0;				 
			}
		}	
		// Make sure loop is not 2 edges of same face	
		if(ct > 1) {
		   error("loop crosses itself");
		   return 0;   
		}
	}	   
	// Get # of selected verts
	for(ev=em->verts.first;ev;ev=ev->next) { 
		if(ev->f & SELECT) vertsel++;
	}	
	   
	// Test for multiple segments
	if(vertsel > numsel+1) {
		error("Was not a single edge loop");
		return 0;		   
	}  
	
	// Get the edgeloop in order - mark f1 with SELECT once added
	for(eed=em->edges.first;eed;eed=eed->next) {
		if((eed->f & SELECT) && !(eed->f1 & SELECT)) {
			// If this is the first edge added, just put it in
			if(!edgelist) {
				BLI_linklist_prepend(&edgelist,eed);
				numadded++;
				first = eed;
				last  = eed; 
				eed->f1 = SELECT;
			} else {  
				if(editedge_getSharedVert(eed, last)) {
					BLI_linklist_append(&edgelist,eed);
					eed->f1 = SELECT;
					numadded++;
					last = eed;					  
				}  else if(editedge_getSharedVert(eed, first)) {
					BLI_linklist_prepend(&edgelist,eed);
					eed->f1 = SELECT;
					numadded++;
					first = eed;					  
				}   
			}
		}   
		if(eed->next == NULL && numadded != numsel) {
			eed=em->edges.first;	
			timesthrough++;
		}
		
		// It looks like there was an unexpected case - Hopefully should not happen
		if(timesthrough >= numsel*2) {
			BLI_linklist_free(edgelist,NULL); 
			error("could not order loop");
			return 0;   
		}
	}
	
	// Put the verts in order in a linklist
	look = edgelist;
	while(look) {
		eed = look->link;
		if(!vertlist) {
			if(look->next) {
				temp = look->next->link;

				//This is the first entry takes care of extra vert
				if(eed->v1 != temp->v1 && eed->v1 != temp->v2) {
					BLI_linklist_append(&vertlist,eed->v1); 
					eed->v1->f1 = 1; 
				} else {
					BLI_linklist_append(&vertlist,eed->v2);  
					eed->v2->f1 = 1; 
				}			 
			} else {
				//This is the case that we only have 1 edge
				BLI_linklist_append(&vertlist,eed->v1); 
				eed->v1->f1 = 1;					
			}
		}		
		// for all the entries
		if(eed->v1->f1 != 1) {
			BLI_linklist_append(&vertlist,eed->v1); 
			eed->v1->f1 = 1;		   
		} else  if(eed->v2->f1 != 1) {
			BLI_linklist_append(&vertlist,eed->v2); 
			eed->v2->f1 = 1;					
		} 
		look = look->next;   
	}		 
	
	// populate the SlideVerts
	
	vertgh = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp); 
	look = vertlist;	  
	while(look) {
		i=0;
		j=0;
		ev = look->link;
		tempsv = (struct SlideVert*)MEM_mallocN(sizeof(struct SlideVert),"SlideVert");
		tempsv->up = NULL;
		tempsv->down = NULL;
		tempsv->origvert.co[0] = ev->co[0];
		tempsv->origvert.co[1] = ev->co[1];
		tempsv->origvert.co[2] = ev->co[2];
		tempsv->origvert.no[0] = ev->no[0];
		tempsv->origvert.no[1] = ev->no[1];
		tempsv->origvert.no[2] = ev->no[2];
		// i is total edges that vert is on
		// j is total selected edges that vert is on
		
		for(eed=em->edges.first;eed;eed=eed->next) {
			if(eed->v1 == ev || eed->v2 == ev) {
				i++;	
				if(eed->f & SELECT) {
					 j++;   
				}
			}		
		}
		// If the vert is in the middle of an edge loop, it touches 2 selected edges and 2 unselected edges
		if(i == 4 && j == 2) {
			for(eed=em->edges.first;eed;eed=eed->next) {
				if(editedge_containsVert(eed, ev)) {
					if(!(eed->f & SELECT)) {
						 if(!tempsv->up) {
							 tempsv->up = eed;
						 } else if (!(tempsv->down)) {
							 tempsv->down = eed;  
						 }
					}
				}		
			}			
		}
		// If it is on the end of the loop, it touches 1 selected and as least 2 more unselected
		if(i >= 3 && j == 1) {
			for(eed=em->edges.first;eed;eed=eed->next) {
				if(editedge_containsVert(eed, ev) && eed->f & SELECT) {
					for(efa = em->faces.first;efa;efa=efa->next) {
						if(editface_containsEdge(efa, eed)) {
							if(editedge_containsVert(efa->e1, ev) && efa->e1 != eed) {
								 if(!tempsv->up) {
									 tempsv->up = efa->e1;
								 } else if (!(tempsv->down)) {
									 tempsv->down = efa->e1;  
								 }								   
							}
							if(editedge_containsVert(efa->e2, ev) && efa->e2 != eed) {
								 if(!tempsv->up) {
									 tempsv->up = efa->e2;
								 } else if (!(tempsv->down)) {
									 tempsv->down = efa->e2;  
								 }								   
							}							
							if(editedge_containsVert(efa->e3, ev) && efa->e3 != eed) {
								 if(!tempsv->up) {
									 tempsv->up = efa->e3;
								 } else if (!(tempsv->down)) {
									 tempsv->down = efa->e3;  
								 }								   
							}  
							if(efa->e4) {
								if(editedge_containsVert(efa->e4, ev) && efa->e4 != eed) {
									 if(!tempsv->up) {
										 tempsv->up = efa->e4;
									 } else if (!(tempsv->down)) {
										 tempsv->down = efa->e4;  
									 }								   
								}
							}														  
							
						}
					}
				}		
			}			
		}		
		if(i > 4 && j == 2) {
			BLI_ghash_free(vertgh, NULL, (GHashValFreeFP)MEM_freeN);
			BLI_linklist_free(vertlist,NULL); 
			BLI_linklist_free(edgelist,NULL); 
			return 0;   
		}
		BLI_ghash_insert(vertgh,ev,tempsv);
		
		look = look->next;   
	}		  
   
	// make sure the UPs nad DOWNs are 'faceloops'
	// Also find the nearest slidevert to the cursor
	getmouseco_areawin(mval);
	look = vertlist;	
	nearest = NULL;
	vertdist = -1;  
	while(look) {	
		tempsv  = BLI_ghash_lookup(vertgh,(EditVert*)look->link);
		
		if(!tempsv->up || !tempsv->down) {
			error("Missing rails");
			BLI_ghash_free(vertgh, NULL, (GHashValFreeFP)MEM_freeN);
			BLI_linklist_free(vertlist,NULL); 
			BLI_linklist_free(edgelist,NULL); 
			return 0;
		}

		if(G.f & G_DRAW_EDGELEN) {
			if(!(tempsv->up->f & SELECT)) {
				tempsv->up->f |= SELECT;
				tempsv->up->f2 |= 16;
			} else {
				tempsv->up->f2 |= ~16;
			}
			if(!(tempsv->down->f & SELECT)) {
				tempsv->down->f |= SELECT;
				tempsv->down->f2 |= 16;
			} else {
				tempsv->down->f2 |= ~16;
			}
		}

		if(look->next != NULL) {
			SlideVert *sv;

			sv = BLI_ghash_lookup(vertgh,(EditVert*)look->next->link);

			if(sv) {
				float tempdist, co[2];

				if(!sharesFace(tempsv->up,sv->up)) {
					EditEdge *swap;
					swap = sv->up;
					sv->up = sv->down;
					sv->down = swap; 
				}

				view3d_project_float(curarea, tempsv->origvert.co, co, projectMat);
				
				tempdist = sqrt(pow(co[0] - mval[0],2)+pow(co[1]  - mval[1],2));

				if(vertdist < 0) {
					vertdist = tempdist;
					nearest  = (EditVert*)look->link;   
				} else if ( tempdist < vertdist ) {
					vertdist = tempdist;
					nearest  = (EditVert*)look->link;	
				}		
			}
		}   		
		
		
		
		look = look->next;   
	}	   
	// we should have enough info now to slide

	len = 0.0f; 
	
	percp = -1;
	while(draw) {
		 /* For the % calculation */   
		short mval[2];   
		float rc[2];
		float v2[2], v3[2];
		EditVert *centerVert, *upVert, *downVert;
		
		

		getmouseco_areawin(mval);  
		
		if (!immediate && (mval[0] == mvalo[0] && mval[1] ==  mvalo[1])) {
			PIL_sleep_ms(10);
		} else {

			mvalo[0] = mval[0];
			mvalo[1] = mval[1];
			
			//Adjust Edgeloop
			if(immediate) {
				perc = imperc;   
			}
			percp = perc;
			if(prop) {
				look = vertlist;	  
				while(look) { 
					EditVert *tempev;
					ev = look->link;
					tempsv = BLI_ghash_lookup(vertgh,ev);
					
					tempev = editedge_getOtherVert((perc>=0)?tempsv->up:tempsv->down, ev);
					VecLerpf(ev->co, tempsv->origvert.co, tempev->co, fabs(perc));
									
					look = look->next;	 
				}
			}
			else {
				//Non prop code  
				look = vertlist;	  
				while(look) { 
					float newlen;
					ev = look->link;
					tempsv = BLI_ghash_lookup(vertgh,ev);
					newlen = (len / VecLenf(editedge_getOtherVert(tempsv->up,ev)->co,editedge_getOtherVert(tempsv->down,ev)->co));
					if(newlen > 1.0) {newlen = 1.0;}
					if(newlen < 0.0) {newlen = 0.0;}
					if(flip == 0) {
						VecLerpf(ev->co, editedge_getOtherVert(tempsv->down,ev)->co, editedge_getOtherVert(tempsv->up,ev)->co, fabs(newlen));									
					} else{
						VecLerpf(ev->co, editedge_getOtherVert(tempsv->up,ev)->co, editedge_getOtherVert(tempsv->down,ev)->co, fabs(newlen));				
					}
					look = look->next;	 
				}

			}
			
			tempsv = BLI_ghash_lookup(vertgh,nearest);

			centerVert = editedge_getSharedVert(tempsv->up, tempsv->down);
			upVert = editedge_getOtherVert(tempsv->up, centerVert);
			downVert = editedge_getOtherVert(tempsv->down, centerVert);
			 // Highlight the Control Edges
	
			scrarea_do_windraw(curarea);   
			persp(PERSP_VIEW);   
			glPushMatrix();   
			mymultmatrix(G.obedit->obmat);

			glColor3ub(0, 255, 0);   
			glBegin(GL_LINES);
			glVertex3fv(upVert->co);
			glVertex3fv(downVert->co);
			glEnd(); 
			
			if(prop == 0) {
				// draw start edge for non-prop
				glPointSize(5);
				glBegin(GL_POINTS);
				glColor3ub(255,0,255);
				if(flip) {
					glVertex3fv(upVert->co);
				} else {
					glVertex3fv(downVert->co);					
				}
				glEnd();	
			}
			
			
			glPopMatrix();		 

			view3d_project_float(curarea, upVert->co, v2, projectMat);
			view3d_project_float(curarea, downVert->co, v3, projectMat);

			/* Determine the % on which the loop should be cut */   

			rc[0]= v3[0]-v2[0];   
			rc[1]= v3[1]-v2[1];   
			len= rc[0]*rc[0]+ rc[1]*rc[1];
			if (len==0) {len = 0.0001;}

			if ((G.qual & LR_SHIFTKEY)==0) {
				wasshift = 0;
				labda= ( rc[0]*((mval[0]-v2[0])) + rc[1]*((mval[1]-v2[1])) )/len;   
			}
			else {
				if (wasshift==0) {
					wasshift = 1;
					shiftlabda = labda;
				}							
				labda= ( rc[0]*((mval[0]-v2[0])) + rc[1]*((mval[1]-v2[1])) )/len / 10.0 + shiftlabda;   			
			}
			

			if(labda<=0.0) labda=0.0;   
			else if(labda>=1.0)labda=1.0;   

			perc=((1-labda)*2)-1;		  
			
			if(G.qual == 0) {
				perc *= 100;
				perc = floor(perc);
				perc /= 100;
			} else if (G.qual == LR_CTRLKEY) {
				perc *= 10;
				perc = floor(perc);
				perc /= 10;				   
			}			
			if(prop) {
				sprintf(str, "(P)ercentage: %f", perc);
			} else {
				len = VecLenf(upVert->co,downVert->co)*((perc+1)/2);
				if(flip == 1) {
					len = VecLenf(upVert->co,downVert->co) - len;
				} 
				sprintf(str, "Non (P)rop Length: %f, Press (F) to flip control side", len);
			}

			
			
			headerprint(str);
			screen_swapbuffers();			
		}
		if(!immediate) {
			while(qtest()) {
				short val=0;		   	
				event= extern_qread(&val);	// extern_qread stores important events for the mainloop to handle 
					
				/* val==0 on key-release event */
				if (val) {
					if(ELEM(event, ESCKEY, RIGHTMOUSE)) {
							prop = 1; // Go back to prop mode
							imperc = 0; // This is the % that gets set for immediate
							immediate = 1; //Run through eval code 1 more time
							cancel = 1;   // Return -1
							mvalo[0] = -1;
					} else if(ELEM3(event, PADENTER, LEFTMOUSE, RETKEY)) {
							draw = 0; // End looping now
					} else if(event==MIDDLEMOUSE) {
							perc = 0;  
							immediate = 1;
					} else if(event==PKEY) {
							(prop == 1) ? (prop = 0):(prop = 1);
							mvalo[0] = -1;  
					} else if(event==FKEY) {
							(flip == 1) ? (flip = 0):(flip = 1); 
							mvalo[0] = -1; 
					} else if(ELEM(event, RIGHTARROWKEY, WHEELUPMOUSE)) { // Scroll through Control Edges
						look = vertlist;	
				 		while(look) {	
							if(nearest == (EditVert*)look->link) {
								if(look->next == NULL) {
									nearest =  (EditVert*)vertlist->link;  
								} else {
									nearest = (EditVert*)look->next->link;
								}	 
								mvalo[0] = -1;
								break;				
							}
							look = look->next;   
						}	  
					} else if(ELEM(event, LEFTARROWKEY, WHEELDOWNMOUSE)) { // Scroll through Control Edges
						look = vertlist;	
				 		while(look) {	
							if(look->next) {
								if(look->next->link == nearest) {
									nearest = (EditVert*)look->link;
									mvalo[0] = -1;
									break;
								}	  
							} else {
								if((EditVert*)vertlist->link == nearest) {
									nearest = look->link;
									mvalo[0] = -1;
									break;							 
								}	   
							}   
							look = look->next;   
						}	  
					}
				}
			} 
		} else {
			draw = 0;
		}
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	 
	}
	
	
	if(G.f & G_DRAW_EDGELEN) {
		look = vertlist;
		while(look) {	
			tempsv  = BLI_ghash_lookup(vertgh,(EditVert*)look->link);
			if(tempsv != NULL) {
				tempsv->up->f &= !SELECT;
				tempsv->down->f &= !SELECT;
			}
			look = look->next;
		}
	}
	
	force_draw(0);
	
	if(!immediate)
		EM_automerge(0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	scrarea_queue_winredraw(curarea);		 
	
	//BLI_ghash_free(edgesgh, freeGHash, NULL); 
	BLI_ghash_free(vertgh, NULL, (GHashValFreeFP)MEM_freeN);
	BLI_linklist_free(vertlist,NULL); 
	BLI_linklist_free(edgelist,NULL); 

	if(cancel == 1) {
		return -1;
	}
	else {
#ifdef WITH_VERSE
	if(G.editMesh->vnode) {
		sync_all_verseverts_with_editverts((VNode*)G.editMesh->vnode);
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
	}
#endif
	}
	return 1;
}

/* -------------------- More tools ------------------ */

void mesh_set_face_flags(short mode)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	MTFace *tface;
	short m_tex=0, m_tiles=0, m_shared=0, m_light=0, m_invis=0, m_collision=0, m_twoside=0, m_obcolor=0; 
	short flag = 0, change = 0;
	
	if (!EM_texFaceCheck()) {
		error("not a mesh with uv/image layers");
		return;
	}
	
	add_numbut(0, TOG|SHO, "Texture", 0, 0, &m_tex, NULL);
	add_numbut(1, TOG|SHO, "Tiles", 0, 0, &m_tiles, NULL);
	add_numbut(2, TOG|SHO, "Shared", 0, 0, &m_shared, NULL);
	add_numbut(3, TOG|SHO, "Light", 0, 0, &m_light, NULL);
	add_numbut(4, TOG|SHO, "Invisible", 0, 0, &m_invis, NULL);
	add_numbut(5, TOG|SHO, "Collision", 0, 0, &m_collision, NULL);
	add_numbut(6, TOG|SHO, "Twoside", 0, 0, &m_twoside, NULL);
	add_numbut(7, TOG|SHO, "ObColor", 0, 0, &m_obcolor, NULL);
	
	if (!do_clever_numbuts((mode ? "Set Flags" : "Clear Flags"), 8, REDRAW))
 		return;
	
	if (m_tex)			flag |= TF_TEX;
	if (m_tiles)		flag |= TF_TILES;
	if (m_shared)		flag |= TF_SHAREDCOL;
	if (m_light)		flag |= TF_LIGHT;
	if (m_invis)		flag |= TF_INVISIBLE;
	if (m_collision)	flag |= TF_DYNAMIC;
	if (m_twoside)		flag |= TF_TWOSIDE;
	if (m_obcolor)		flag |= TF_OBCOL;
	
	efa= em->faces.first;
	while(efa) {
		if(efa->f & SELECT) {
			tface= CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (mode)	tface->mode |= flag;
			else		tface->mode &= ~flag;
		}
		efa= efa->next;
	}
	
	if (change) {
		BIF_undo_push((mode ? "Set Flags" : "Clear Flags"));
		
		allqueue(REDRAWIMAGE, 0);
		allqueue(REDRAWVIEW3D, 0);
	}
}

void mesh_set_smooth_faces(short event)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;

	if(G.obedit==0) return;
	
	if(G.obedit->type != OB_MESH) return;
	
	efa= em->faces.first;
	while(efa) {
		if(efa->f & SELECT) {
			if(event==1) efa->flag |= ME_SMOOTH;
			else if(event==0) efa->flag &= ~ME_SMOOTH;
		}
		efa= efa->next;
	}
	
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
	
	if(event==1) BIF_undo_push("Set Smooth");
	else if(event==0) BIF_undo_push("Set Solid");
}

/* helper to find edge for edge_rip */
static float mesh_rip_edgedist(float mat[][4], float *co1, float *co2, short *mval)
{
	float vec1[3], vec2[3], mvalf[2];
	
	view3d_project_float(curarea, co1, vec1, mat);
	view3d_project_float(curarea, co2, vec2, mat);
	mvalf[0]= (float)mval[0];
	mvalf[1]= (float)mval[1];
	
	return PdistVL2Dfl(mvalf, vec1, vec2);
}

/* helper for below */
static void mesh_rip_setface(EditFace *sefa)
{
	/* put new vertices & edges in best face */
	if(sefa->v1->tmp.v) sefa->v1= sefa->v1->tmp.v;
	if(sefa->v2->tmp.v) sefa->v2= sefa->v2->tmp.v;
	if(sefa->v3->tmp.v) sefa->v3= sefa->v3->tmp.v;
	if(sefa->v4 && sefa->v4->tmp.v) sefa->v4= sefa->v4->tmp.v;
	
	sefa->e1= addedgelist(sefa->v1, sefa->v2, sefa->e1);
	sefa->e2= addedgelist(sefa->v2, sefa->v3, sefa->e2);
	if(sefa->v4) {
		sefa->e3= addedgelist(sefa->v3, sefa->v4, sefa->e3);
		sefa->e4= addedgelist(sefa->v4, sefa->v1, sefa->e4);
	}
	else 
		sefa->e3= addedgelist(sefa->v3, sefa->v1, sefa->e3);
	
}

/* based on mouse cursor position, it defines how is being ripped */
void mesh_rip(void)
{
	extern void faceloop_select(EditEdge *startedge, int select);
	EditMesh *em = G.editMesh;
	EditVert *eve, *nextve;
	EditEdge *eed, *seed= NULL;
	EditFace *efa, *sefa= NULL;
	float projectMat[4][4], viewMat[4][4], vec[3], dist, mindist;
	short doit= 1, mval[2],propmode,prop;
	
	propmode = G.scene->prop_mode;
	G.scene->prop_mode = 0;
	prop = G.scene->proportional;
	G.scene->proportional = 0;
	
	/* select flush... vertices are important */
	EM_selectmode_set();
	
	getmouseco_areawin(mval);
	view3d_get_object_project_mat(curarea, G.obedit, projectMat, viewMat);

	/* find best face, exclude triangles and break on face select or faces with 2 edges select */
	mindist= 1000000.0f;
	for(efa= em->faces.first; efa; efa=efa->next) {
		if( efa->f & 1) 
			break;
		if(efa->v4 && faceselectedOR(efa, SELECT) ) {
			int totsel=0;
			
			if(efa->e1->f & SELECT) totsel++;
			if(efa->e2->f & SELECT) totsel++;
			if(efa->e3->f & SELECT) totsel++;
			if(efa->e4->f & SELECT) totsel++;
			
			if(totsel>1)
				break;
			view3d_project_float(curarea, efa->cent, vec, projectMat);
			dist= sqrt( (vec[0]-mval[0])*(vec[0]-mval[0]) + (vec[1]-mval[1])*(vec[1]-mval[1]) );
			if(dist<mindist) {
				mindist= dist;
				sefa= efa;
			}
		}
	}
	
	if(efa) {
		error("Can't perform ripping with faces selected this way");
		return;
	}
	if(sefa==NULL) {
		error("No proper selection or faces included");
		return;
	}
	

	/* duplicate vertices, new vertices get selected */
	for(eve = em->verts.last; eve; eve= eve->prev) {
		eve->tmp.v = NULL;
		if(eve->f & SELECT) {
			eve->tmp.v = addvertlist(eve->co, eve);
			eve->f &= ~SELECT;
			eve->tmp.v->f |= SELECT;
		}
	}
	
	/* find the best candidate edge */
	/* or one of sefa edges is selected... */
	if(sefa->e1->f & SELECT) seed= sefa->e2;
	if(sefa->e2->f & SELECT) seed= sefa->e1;
	if(sefa->e3->f & SELECT) seed= sefa->e2;
	if(sefa->e4 && sefa->e4->f & SELECT) seed= sefa->e3;
	
	/* or we do the distance trick */
	if(seed==NULL) {
		mindist= 1000000.0f;
		if(sefa->e1->v1->tmp.v || sefa->e1->v2->tmp.v) {
			dist = mesh_rip_edgedist(projectMat, 
									 sefa->e1->v1->co, 
									 sefa->e1->v2->co, mval);
			if(dist<mindist) {
				seed= sefa->e1;
				mindist= dist;
			}
		}
		if(sefa->e2->v1->tmp.v || sefa->e2->v2->tmp.v) {
			dist = mesh_rip_edgedist(projectMat,
									 sefa->e2->v1->co, 
									 sefa->e2->v2->co, mval);
			if(dist<mindist) {
				seed= sefa->e2;
				mindist= dist;
			}
		}
		if(sefa->e3->v1->tmp.v || sefa->e3->v2->tmp.v) {
			dist= mesh_rip_edgedist(projectMat, 
									sefa->e3->v1->co, 
									sefa->e3->v2->co, mval);
			if(dist<mindist) {
				seed= sefa->e3;
				mindist= dist;
			}
		}
		if(sefa->e4 && (sefa->e4->v1->tmp.v || sefa->e4->v2->tmp.v)) {
			dist= mesh_rip_edgedist(projectMat, 
									sefa->e4->v1->co, 
									sefa->e4->v2->co, mval);
			if(dist<mindist) {
				seed= sefa->e4;
				mindist= dist;
			}
		}
	}
	
	if(seed==NULL) {	// never happens?
		error("No proper edge found to start");
		return;
	}
	
	faceloop_select(seed, 2);	// tmp abuse for finding all edges that need duplicated, returns OK faces with f1

	/* duplicate edges in the loop, with at least 1 vertex selected, needed for selection flip */
	for(eed = em->edges.last; eed; eed= eed->prev) {
		eed->tmp.v = NULL;
		if((eed->v1->tmp.v) || (eed->v2->tmp.v)) {
			EditEdge *newed;
			
			newed= addedgelist(eed->v1->tmp.v?eed->v1->tmp.v:eed->v1, 
							   eed->v2->tmp.v?eed->v2->tmp.v:eed->v2, eed);
			if(eed->f & SELECT) {
				eed->f &= ~SELECT;
				newed->f |= SELECT;
			}
			eed->tmp.v = (EditVert *)newed;
		}
	}

	/* first clear edges to help finding neighbours */
	for(eed = em->edges.last; eed; eed= eed->prev) eed->f1= 0;

	/* put new vertices & edges && flag in best face */
	mesh_rip_setface(sefa);
	
	/* starting with neighbours of best face, we loop over the seam */
	sefa->f1= 2;
	doit= 1;
	while(doit) {
		doit= 0;
		
		for(efa= em->faces.first; efa; efa=efa->next) {
			/* new vert in face */
			if (efa->v1->tmp.v || efa->v2->tmp.v || 
				efa->v3->tmp.v || (efa->v4 && efa->v4->tmp.v)) {
				/* face is tagged with loop */
				if(efa->f1==1) {
					mesh_rip_setface(efa);
					efa->f1= 2;
					doit= 1;
				}
			}
		}		
	}
	
	/* remove loose edges, that were part of a ripped face */
	for(eve = em->verts.first; eve; eve= eve->next) eve->f1= 0;
	for(eed = em->edges.last; eed; eed= eed->prev) eed->f1= 0;
	for(efa= em->faces.first; efa; efa=efa->next) {
		efa->e1->f1= 1;
		efa->e2->f1= 1;
		efa->e3->f1= 1;
		if(efa->e4) efa->e4->f1= 1;
	}
	
	for(eed = em->edges.last; eed; eed= seed) {
		seed= eed->prev;
		if(eed->f1==0) {
			if(eed->v1->tmp.v || eed->v2->tmp.v || 
			   (eed->v1->f & SELECT) || (eed->v2->f & SELECT)) {
				remedge(eed);
				free_editedge(eed);
				eed= NULL;
			}
		}
		if(eed) {
			eed->v1->f1= 1;
			eed->v2->f1= 1;
		}
	}
	
	/* and remove loose selected vertices, that got duplicated accidentally */
	for(eve = em->verts.first; eve; eve= nextve) {
		nextve= eve->next;
		if(eve->f1==0 && (eve->tmp.v || (eve->f & SELECT))) {
			BLI_remlink(&em->verts,eve);
			free_editvert(eve);
		}
	}
	
	countall();	// apparently always needed when adding stuff, derived mesh

#ifdef WITH_VERSE
	if(G.editMesh->vnode) {
		sync_all_verseverts_with_editverts((VNode*)G.editMesh->vnode);
		sync_all_versefaces_with_editfaces((VNode*)G.editMesh->vnode);
	}
#endif

	BIF_TransformSetUndo("Rip");
	initTransform(TFM_TRANSLATION, 0);
	Transform();
	
	G.scene->prop_mode = propmode;
	G.scene->proportional = prop;
}

void shape_propagate(){
	EditMesh *em = G.editMesh;
	EditVert *ev = NULL;
	Mesh* me = (Mesh*)G.obedit->data;
	Key*  ky = NULL;
	KeyBlock* kb = NULL;
	Base* base=NULL;
	
	
	if(me->key){
		ky = me->key;
	} else {
		error("Object Has No Key");	
		return;
	}	

	if(ky->block.first){
		for(ev = em->verts.first; ev ; ev = ev->next){
			if(ev->f & SELECT){
				for(kb=ky->block.first;kb;kb = kb->next){
					float *data;		
					data = kb->data;			
					VECCOPY(data+(ev->keyindex*3),ev->co);				
				}
			}		
		}						
	} else {
		error("Object Has No Blendshapes");	
		return;			
	}
	
	//TAG Mesh Objects that share this data
	for(base = G.scene->base.first; base; base = base->next){
		if(base->object && base->object->data == me){
			base->object->recalc = OB_RECALC_DATA;
		}
	}		

	BIF_undo_push("Propagate Blendshape Verts");
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
	return;	
}

void shape_copy_from_lerp(KeyBlock* thisBlock, KeyBlock* fromBlock)
{
	EditMesh *em = G.editMesh;
	EditVert *ev = NULL;
	short mval[2], curval[2], event = 0, finished = 0, canceled = 0, fullcopy=0 ;
	float perc = 0;
	char str[64];
	float *data, *odata;
				
	data  = fromBlock->data;
	odata = thisBlock->data;
	
	getmouseco_areawin(mval);
	curval[0] = mval[0] + 1; curval[1] = mval[1] + 1;

	while (finished == 0)
	{
		getmouseco_areawin(mval);
		if (mval[0] != curval[0] || mval[1] != curval[1])
		{
			
			if(mval[0] > curval[0])
				perc += 0.1;
			else if(mval[0] < curval[0])
				perc -= 0.1;
				
			if(perc < 0) perc = 0;
			if(perc > 1) perc = 1;
			
			curval[0] = mval[0];
			curval[1] = mval[1];

			if(fullcopy == 1){
				perc = 1;	
			}

			for(ev = em->verts.first; ev ; ev = ev->next){
				if(ev->f & SELECT){
					VecLerpf(ev->co,odata+(ev->keyindex*3),data+(ev->keyindex*3),perc);
				}		
			}	
			sprintf(str,"Blending at %d%c  MMB to Copy at 100%c",(int)(perc*100),'%','%');
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
			headerprint(str);
			force_draw(0);			

			if(fullcopy == 1){
				break;	
			}

		} else {
			PIL_sleep_ms(10);	
		}

		while(qtest()) {
			short val=0;			
			event= extern_qread(&val);	
			if(val){
				if(ELEM3(event, PADENTER, LEFTMOUSE, RETKEY)){
					finished = 1;
				}
				else if (event == MIDDLEMOUSE){
					fullcopy = 1;	
				}
				else if (ELEM3(event,ESCKEY,RIGHTMOUSE,RIGHTMOUSE)){
					canceled = 1;
					finished = 1;
				}
			} 
		}
	}
	if(!canceled)						
		BIF_undo_push("Copy Blendshape Verts");
	else
		for(ev = em->verts.first; ev ; ev = ev->next){
			if(ev->f & SELECT){
				VECCOPY(ev->co, odata+(ev->keyindex*3));
			}		
		}
	return;
}



void shape_copy_select_from()
{
	Mesh* me = (Mesh*)G.obedit->data;
	EditMesh *em = G.editMesh;
	EditVert *ev = NULL;
	int totverts = 0,curshape = G.obedit->shapenr;
	
	Key*  ky = NULL;
	KeyBlock *kb = NULL,*thisBlock = NULL;
	int maxlen=32, nr=0, a=0;
	char *menu;
	
	if(me->key){
		ky = me->key;
	} else {
		error("Object Has No Key");	
		return;
	}
	
	if(ky->block.first){
		for(kb=ky->block.first;kb;kb = kb->next){
			maxlen += 40; // Size of a block name
			if(a == curshape-1){
					thisBlock = kb;		
			}
			
			a++;
		}
		a=0;
		menu = MEM_callocN(maxlen, "Copy Shape Menu Text");
		strcpy(menu, "Copy Vert Positions from Shape %t|");
		for(kb=ky->block.first;kb;kb = kb->next){
			if(a != curshape-1){ 
				sprintf(menu,"%s %s %cx%d|",menu,kb->name,'%',a);
			}
			a++;
		}
		nr = pupmenu_col(menu, 20);
		MEM_freeN(menu);		
	} else {
		error("Object Has No Blendshapes");	
		return;			
	}
	
	a = 0;
	
	for(kb=ky->block.first;kb;kb = kb->next){
		if(a == nr){
			
			for(ev = em->verts.first;ev;ev = ev->next){
				totverts++;
			}
			
			if(me->totvert != totverts){
				error("Shape Has had Verts Added/Removed, please cycle editmode before copying");
				return;	
			}
			shape_copy_from_lerp(thisBlock,kb);		
					
			return;
		}
		a++;
	}		
	return;
}

/* Collection Routines|Currently used by the improved merge code*/
/* buildEdge_collection() creates a list of lists*/
/* these lists are filled with edges that are topologically connected.*/
/* This whole tool needs to be redone, its rather poorly implemented...*/

typedef struct Collection{
	struct Collection *next, *prev;
	int index;
	ListBase collectionbase;
} Collection;

typedef struct CollectedEdge{
	struct CollectedEdge *next, *prev;
	EditEdge *eed;
} CollectedEdge;

#define MERGELIMIT 0.000001

static void build_edgecollection(ListBase *allcollections)
{
	EditEdge *eed;
	Collection *edgecollection, *newcollection;
	CollectedEdge *newedge;
	
	int currtag = 1;
	short ebalanced = 0;
	short collectionfound = 0;
	
	for (eed=G.editMesh->edges.first; eed; eed = eed->next){	
		eed->tmp.l = 0;
		eed->v1->tmp.l = 0;
		eed->v2->tmp.l = 0;
	}
	
	/*1st pass*/
	for(eed=G.editMesh->edges.first; eed; eed=eed->next){
			if(eed->f&SELECT){
				eed->v1->tmp.l = currtag;
				eed->v2->tmp.l = currtag;
				currtag +=1;
			}
	}
			
	/*2nd pass - Brute force. Loop through selected faces until there are no 'unbalanced' edges left (those with both vertices 'tmp.l' tag matching */
	while(ebalanced == 0){
		ebalanced = 1;
		for(eed=G.editMesh->edges.first; eed; eed = eed->next){
			if(eed->f&SELECT){
				if(eed->v1->tmp.l != eed->v2->tmp.l) /*unbalanced*/{
					if(eed->v1->tmp.l > eed->v2->tmp.l && eed->v2->tmp.l !=0) eed->v1->tmp.l = eed->v2->tmp.l; 
					else if(eed->v1 != 0) eed->v2->tmp.l = eed->v1->tmp.l; 
					ebalanced = 0;
				}
			}
		}
	}
	
	/*3rd pass, set all the edge flags (unnessecary?)*/
	for(eed=G.editMesh->edges.first; eed; eed = eed->next){
		if(eed->f&SELECT) eed->tmp.l = eed->v1->tmp.l;
	}
	
	for(eed=G.editMesh->edges.first; eed; eed=eed->next){
		if(eed->f&SELECT){
			if(allcollections->first){
				for(edgecollection = allcollections->first; edgecollection; edgecollection=edgecollection->next){
					if(edgecollection->index == eed->tmp.l){
						newedge = MEM_mallocN(sizeof(CollectedEdge), "collected edge");
						newedge->eed = eed;
						BLI_addtail(&(edgecollection->collectionbase), newedge);
						collectionfound = 1;
						break;
					}
					else collectionfound = 0;
				}
			}
			if(allcollections->first == NULL || collectionfound == 0){
				newcollection = MEM_mallocN(sizeof(Collection), "element collection");
				newcollection->index = eed->tmp.l;
				newcollection->collectionbase.first = 0;
				newcollection->collectionbase.last = 0;
				
				newedge = MEM_mallocN(sizeof(CollectedEdge), "collected edge");
				newedge->eed = eed;
					
				BLI_addtail(&(newcollection->collectionbase), newedge);
				BLI_addtail(allcollections, newcollection);
			}
		}
		
	}
}

static void freecollections(ListBase *allcollections)
{
	struct Collection *curcollection;
	
	for(curcollection = allcollections->first; curcollection; curcollection = curcollection->next)
		BLI_freelistN(&(curcollection->collectionbase));
	BLI_freelistN(allcollections);
}

/*Begin UV Edge Collapse Code 
	Like Edge subdivide, Edge Collapse should handle UV's intelligently, but since UV's are a per-face attribute, normal edge collapse will fail
	in areas such as the boundries of 'UV islands'. So for each edge collection we need to build a set of 'welded' UV vertices and edges for it.
	The welded UV edges can then be sorted and collapsed.
*/
typedef struct wUV{
	struct wUV *next, *prev;
	ListBase nodes;
	float u, v; /*cached copy of UV coordinates pointed to by nodes*/
	EditVert *eve;
	int f;
} wUV;

typedef struct wUVNode{
	struct wUVNode *next, *prev;
	float *u; /*pointer to original tface data*/
	float *v; /*pointer to original tface data*/
} wUVNode;

typedef struct wUVEdge{
	struct wUVEdge *next, *prev;
	float v1uv[2], v2uv[2]; /*nasty.*/
	struct wUV *v1, *v2; /*oriented same as editedge*/
	EditEdge *eed;
	int f;
} wUVEdge;

typedef struct wUVEdgeCollect{ /*used for grouping*/
	struct wUVEdgeCollect *next, *prev;
	wUVEdge *uved;
	int id; 
} wUVEdgeCollect;

static void append_weldedUV(EditFace *efa, EditVert *eve, int tfindex, ListBase *uvverts)
{
	wUV *curwvert, *newwvert;
	wUVNode *newnode;
	int found;
	MTFace *tf = CustomData_em_get(&G.editMesh->fdata, efa->data, CD_MTFACE);
	
	found = 0;
	
	for(curwvert=uvverts->first; curwvert; curwvert=curwvert->next){
		if(curwvert->eve == eve && curwvert->u == tf->uv[tfindex][0] && curwvert->v == tf->uv[tfindex][1]){
			newnode = MEM_callocN(sizeof(wUVNode), "Welded UV Vert Node");
			newnode->u = &(tf->uv[tfindex][0]);
			newnode->v = &(tf->uv[tfindex][1]);
			BLI_addtail(&(curwvert->nodes), newnode);
			found = 1;
			break;
		}
	}
	
	if(!found){
		newnode = MEM_callocN(sizeof(wUVNode), "Welded UV Vert Node");
		newnode->u = &(tf->uv[tfindex][0]);
		newnode->v = &(tf->uv[tfindex][1]);
		
		newwvert = MEM_callocN(sizeof(wUV), "Welded UV Vert");
		newwvert->u = *(newnode->u);
		newwvert->v = *(newnode->v);
		newwvert->eve = eve;
		
		BLI_addtail(&(newwvert->nodes), newnode);
		BLI_addtail(uvverts, newwvert);
		
	}
}

static void build_weldedUVs(ListBase *uvverts)
{
	EditFace *efa;
	for(efa=G.editMesh->faces.first; efa; efa=efa->next){
		if(efa->v1->f1) append_weldedUV(efa, efa->v1, 0, uvverts);
		if(efa->v2->f1) append_weldedUV(efa, efa->v2, 1, uvverts);
		if(efa->v3->f1) append_weldedUV(efa, efa->v3, 2, uvverts);
		if(efa->v4 && efa->v4->f1) append_weldedUV(efa, efa->v4, 3, uvverts);
	}
}

static void append_weldedUVEdge(EditFace *efa, EditEdge *eed, ListBase *uvedges)
{
	wUVEdge *curwedge, *newwedge;
	int v1tfindex, v2tfindex, found;
	MTFace *tf = CustomData_em_get(&G.editMesh->fdata, efa->data, CD_MTFACE);
	
	found = 0;
	
	if(eed->v1 == efa->v1) v1tfindex = 0;
	else if(eed->v1 == efa->v2) v1tfindex = 1;
	else if(eed->v1 == efa->v3) v1tfindex = 2;
	else /* if(eed->v1 == efa->v4) */ v1tfindex = 3;
			
	if(eed->v2 == efa->v1) v2tfindex = 0;
	else if(eed->v2 == efa->v2) v2tfindex = 1;
	else if(eed->v2 == efa->v3) v2tfindex = 2;
	else /* if(eed->v2 == efa->v4) */ v2tfindex = 3;

	for(curwedge=uvedges->first; curwedge; curwedge=curwedge->next){
			if(curwedge->eed == eed && curwedge->v1uv[0] == tf->uv[v1tfindex][0] && curwedge->v1uv[1] == tf->uv[v1tfindex][1] && curwedge->v2uv[0] == tf->uv[v2tfindex][0] && curwedge->v2uv[1] == tf->uv[v2tfindex][1]){
				found = 1;
				break; //do nothing, we don't need another welded uv edge
			}
	}
	
	if(!found){
		newwedge = MEM_callocN(sizeof(wUVEdge), "Welded UV Edge");
		newwedge->v1uv[0] = tf->uv[v1tfindex][0];
		newwedge->v1uv[1] = tf->uv[v1tfindex][1];
		newwedge->v2uv[0] = tf->uv[v2tfindex][0];
		newwedge->v2uv[1] = tf->uv[v2tfindex][1];
		newwedge->eed = eed;
		
		BLI_addtail(uvedges, newwedge);
	}
}

static void build_weldedUVEdges(ListBase *uvedges, ListBase *uvverts)
{
	wUV *curwvert;
	wUVEdge *curwedge;
	EditFace *efa;
	
	for(efa=G.editMesh->faces.first; efa; efa=efa->next){
		if(efa->e1->f1) append_weldedUVEdge(efa, efa->e1, uvedges);
		if(efa->e2->f1) append_weldedUVEdge(efa, efa->e2, uvedges);
		if(efa->e3->f1) append_weldedUVEdge(efa, efa->e3, uvedges);
		if(efa->e4 && efa->e4->f1) append_weldedUVEdge(efa, efa->e4, uvedges);
	}
	
	
	//link vertices: for each uvedge, search uvverts to populate v1 and v2 pointers
	for(curwedge=uvedges->first; curwedge; curwedge=curwedge->next){
		for(curwvert=uvverts->first; curwvert; curwvert=curwvert->next){
			if(curwedge->eed->v1 == curwvert->eve && curwedge->v1uv[0] == curwvert->u && curwedge->v1uv[1] == curwvert->v){
				curwedge->v1 = curwvert;
				break;
			}
		}
		for(curwvert=uvverts->first; curwvert; curwvert=curwvert->next){
			if(curwedge->eed->v2 == curwvert->eve && curwedge->v2uv[0] == curwvert->u && curwedge->v2uv[1] == curwvert->v){
				curwedge->v2 = curwvert;
				break;
			}
		}
	}
}

static void free_weldedUVs(ListBase *uvverts)
{
	wUV *curwvert;
	for(curwvert = uvverts->first; curwvert; curwvert=curwvert->next) BLI_freelistN(&(curwvert->nodes));
	BLI_freelistN(uvverts);
}

static void collapse_edgeuvs(void)
{
	ListBase uvedges, uvverts, allcollections;
	wUVEdge *curwedge;
	wUVNode *curwnode;
	wUVEdgeCollect *collectedwuve, *newcollectedwuve;
	Collection *wuvecollection, *newcollection;
	int curtag, balanced, collectionfound= 0, vcount;
	float avg[2];

	if (!EM_texFaceCheck())
		return;
	
	uvverts.first = uvverts.last = uvedges.first = uvedges.last = allcollections.first = allcollections.last = NULL;
	
	build_weldedUVs(&uvverts);
	build_weldedUVEdges(&uvedges, &uvverts);
	
	curtag = 0;
	
	for(curwedge=uvedges.first; curwedge; curwedge=curwedge->next){
		curwedge->v1->f = curtag;
		curwedge->v2->f = curtag;
		curtag +=1;
	}
	
	balanced = 0;
	while(!balanced){
		balanced = 1;
		for(curwedge=uvedges.first; curwedge; curwedge=curwedge->next){
			if(curwedge->v1->f != curwedge->v2->f){
				if(curwedge->v1->f > curwedge->v2->f) curwedge->v1->f = curwedge->v2->f;
				else curwedge->v2->f = curwedge->v1->f;
				balanced = 0;
			}
		}
	}
	
	for(curwedge=uvedges.first; curwedge; curwedge=curwedge->next) curwedge->f = curwedge->v1->f;
	
	
	for(curwedge=uvedges.first; curwedge; curwedge=curwedge->next){
		if(allcollections.first){
			for(wuvecollection = allcollections.first; wuvecollection; wuvecollection=wuvecollection->next){
				if(wuvecollection->index == curwedge->f){
					newcollectedwuve = MEM_callocN(sizeof(wUVEdgeCollect), "Collected Welded UV Edge");
					newcollectedwuve->uved = curwedge;
					BLI_addtail(&(wuvecollection->collectionbase), newcollectedwuve);
					collectionfound = 1;
					break;
				}
				
				else collectionfound = 0;
			}
		}
		if(allcollections.first == NULL || collectionfound == 0){
			newcollection = MEM_callocN(sizeof(Collection), "element collection");
			newcollection->index = curwedge->f;
			newcollection->collectionbase.first = 0;
			newcollection->collectionbase.last = 0;
				
			newcollectedwuve = MEM_callocN(sizeof(wUVEdgeCollect), "Collected Welded UV Edge");
			newcollectedwuve->uved = curwedge;
					
			BLI_addtail(&(newcollection->collectionbase), newcollectedwuve);
			BLI_addtail(&allcollections, newcollection);
		}
	}
		
	for(wuvecollection=allcollections.first; wuvecollection; wuvecollection=wuvecollection->next){
		
		vcount = avg[0] = avg[1] = 0;
		
		for(collectedwuve= wuvecollection->collectionbase.first; collectedwuve; collectedwuve = collectedwuve->next){
			avg[0] += collectedwuve->uved->v1uv[0];
			avg[1] += collectedwuve->uved->v1uv[1];
			
			avg[0] += collectedwuve->uved->v2uv[0];
			avg[1] += collectedwuve->uved->v2uv[1];
			
			vcount +=2;
		
		}
		
		avg[0] /= vcount; avg[1] /= vcount;
		
		for(collectedwuve= wuvecollection->collectionbase.first; collectedwuve; collectedwuve = collectedwuve->next){
			for(curwnode=collectedwuve->uved->v1->nodes.first; curwnode; curwnode=curwnode->next){
				*(curwnode->u) = avg[0];
				*(curwnode->v) = avg[1];
			}
			for(curwnode=collectedwuve->uved->v2->nodes.first; curwnode; curwnode=curwnode->next){
				*(curwnode->u) = avg[0];
				*(curwnode->v) = avg[1];
			}
		}
	}
	
	free_weldedUVs(&uvverts);
	BLI_freelistN(&uvedges);
	freecollections(&allcollections);
}

/*End UV Edge collapse code*/

static void collapseuvs(EditVert *mergevert)
{
	EditFace *efa;
	MTFace *tf;
	int uvcount;
	float uvav[2];

	if (!EM_texFaceCheck())
		return;
	
	uvcount = 0;
	uvav[0] = 0;
	uvav[1] = 0;
	
	for(efa = G.editMesh->faces.first; efa; efa=efa->next){
		tf = CustomData_em_get(&G.editMesh->fdata, efa->data, CD_MTFACE);

		if(efa->v1->f1 && ELEM(mergevert, NULL, efa->v1)) {
			uvav[0] += tf->uv[0][0];
			uvav[1] += tf->uv[0][1];
			uvcount += 1;
		}
		if(efa->v2->f1 && ELEM(mergevert, NULL, efa->v2)){
			uvav[0] += tf->uv[1][0];		
			uvav[1] += tf->uv[1][1];
			uvcount += 1;
		}
		if(efa->v3->f1 && ELEM(mergevert, NULL, efa->v3)){
			uvav[0] += tf->uv[2][0];
			uvav[1] += tf->uv[2][1];
			uvcount += 1;
		}
		if(efa->v4 && efa->v4->f1 && ELEM(mergevert, NULL, efa->v4)){
			uvav[0] += tf->uv[3][0];
			uvav[1] += tf->uv[3][1];
			uvcount += 1;
		}
	}
	
	if(uvcount > 0) {
		uvav[0] /= uvcount; 
		uvav[1] /= uvcount;
	
		for(efa = G.editMesh->faces.first; efa; efa=efa->next){
			tf = CustomData_em_get(&G.editMesh->fdata, efa->data, CD_MTFACE);

			if(efa->v1->f1){
				tf->uv[0][0] = uvav[0];
				tf->uv[0][1] = uvav[1];
			}
			if(efa->v2->f1){
				tf->uv[1][0] = uvav[0];		
				tf->uv[1][1] = uvav[1];
			}
			if(efa->v3->f1){
				tf->uv[2][0] = uvav[0];
				tf->uv[2][1] = uvav[1];
			}
			if(efa->v4 && efa->v4->f1){
				tf->uv[3][0] = uvav[0];
				tf->uv[3][1] = uvav[1];
			}
		}
	}
}

int collapseEdges(void)
{
	EditVert *eve;
	EditEdge *eed;
	
	ListBase allcollections;
	CollectedEdge *curredge;
	Collection *edgecollection;
	
	int totedges, groupcount, mergecount,vcount;
	float avgcount[3];
	
	allcollections.first = 0;
	allcollections.last = 0;
	
	mergecount = 0;
	
	if(multires_test()) return 0;
	
	build_edgecollection(&allcollections);
	groupcount = BLI_countlist(&allcollections);
	
	
	for(edgecollection = allcollections.first; edgecollection; edgecollection = edgecollection->next){
		totedges = BLI_countlist(&(edgecollection->collectionbase));
		mergecount += totedges;
		avgcount[0] = 0; avgcount[1] = 0; avgcount[2] = 0;
		
		vcount = 0;
		
		for(curredge = edgecollection->collectionbase.first; curredge; curredge = curredge->next){
			avgcount[0] += ((EditEdge*)curredge->eed)->v1->co[0];
			avgcount[1] += ((EditEdge*)curredge->eed)->v1->co[1];
			avgcount[2] += ((EditEdge*)curredge->eed)->v1->co[2];
			
			avgcount[0] += ((EditEdge*)curredge->eed)->v2->co[0];
			avgcount[1] += ((EditEdge*)curredge->eed)->v2->co[1];
			avgcount[2] += ((EditEdge*)curredge->eed)->v2->co[2];
			
			vcount +=2;
		}
		
		avgcount[0] /= vcount; avgcount[1] /=vcount; avgcount[2] /= vcount;
		
		for(curredge = edgecollection->collectionbase.first; curredge; curredge = curredge->next){
			VECCOPY(((EditEdge*)curredge->eed)->v1->co,avgcount);
			VECCOPY(((EditEdge*)curredge->eed)->v2->co,avgcount);
		}
		
		if (EM_texFaceCheck()) {
			/*uv collapse*/
			for(eve=G.editMesh->verts.first; eve; eve=eve->next) eve->f1 = 0;
			for(eed=G.editMesh->edges.first; eed; eed=eed->next) eed->f1 = 0;
			for(curredge = edgecollection->collectionbase.first; curredge; curredge = curredge->next){
				curredge->eed->v1->f1 = 1;
				curredge->eed->v2->f1 = 1;
				curredge->eed->f1 = 1;
			}
			collapse_edgeuvs();
		}
		
	}
	freecollections(&allcollections);
	removedoublesflag(1, 0, MERGELIMIT);
	/*get rid of this!*/
	countall();
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	return mergecount;
}

int merge_firstlast(int first, int uvmerge)
{
	EditVert *eve,*mergevert;
	EditSelection *ese;
	
	if(multires_test()) return 0;
	
	/* do sanity check in mergemenu in edit.c ?*/
	if(first == 0){ 
		ese = G.editMesh->selected.last;
		mergevert= (EditVert*)ese->data;
	}
	else{ 
		ese = G.editMesh->selected.first;
		mergevert = (EditVert*)ese->data;
	}
	
	if(mergevert->f&SELECT){
		for (eve=G.editMesh->verts.first; eve; eve=eve->next){
			if (eve->f&SELECT)
			VECCOPY(eve->co,mergevert->co);
		}
	}
	
	if(uvmerge && CustomData_has_layer(&G.editMesh->fdata, CD_MTFACE)){
		
		for(eve=G.editMesh->verts.first; eve; eve=eve->next) eve->f1 = 0;
		for(eve=G.editMesh->verts.first; eve; eve=eve->next){
			if(eve->f&SELECT) eve->f1 = 1;
		}
		collapseuvs(mergevert);
	}
	
	countall();
	return removedoublesflag(1, 0, MERGELIMIT);
}

int merge_target(int target, int uvmerge)
{
	EditVert *eve;
	
	if(multires_test()) return 0;
	
	if(target) snap_sel_to_curs();
	else snap_to_center();
	
	if(uvmerge && CustomData_has_layer(&G.editMesh->fdata, CD_MTFACE)){
		for(eve=G.editMesh->verts.first; eve; eve=eve->next) eve->f1 = 0;
		for(eve=G.editMesh->verts.first; eve; eve=eve->next){
				if(eve->f&SELECT) eve->f1 = 1;
		}
		collapseuvs(NULL);
	}
	
	countall();
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
	return removedoublesflag(1, 0, MERGELIMIT);
	
}
#undef MERGELIMIT

typedef struct PathNode{
	int u;
	int visited;
	ListBase edges;
} PathNode;

typedef struct PathEdge{
	struct PathEdge *next, *prev;
	int v;
	float w;
} PathEdge;

void pathselect(void)
{
	EditVert *eve, *s, *t;
	EditEdge *eed;
	EditSelection *ese;
	PathEdge *newpe, *currpe;
	PathNode *currpn;
	PathNode *Q;
	int v, *previous, pathvert, pnindex; /*pnindex redundant?*/
 	int unbalanced, totnodes;
	short physical;
	float *cost;
	Heap *heap; /*binary heap for sorting pointers to PathNodes based upon a 'cost'*/
	
	s = t = NULL;
	
	countall(); /*paranoid?*/
	
	ese = ((EditSelection*)G.editMesh->selected.last);
	if(ese && ese->type == EDITVERT && ese->prev && ese->prev->type == EDITVERT){
		physical= pupmenu("Distance Method? %t|Edge Length%x1|Topological%x0");
		
		t = (EditVert*)ese->data;
		s = (EditVert*)ese->prev->data;
		
		/*need to find out if t is actually reachable by s....*/
		for(eve=G.editMesh->verts.first; eve; eve=eve->next){ 
			eve->f1 = 0;
		}
		
		s->f1 = 1;
		
		unbalanced = 1;
		totnodes = 1;
		while(unbalanced){
			unbalanced = 0;
			for(eed=G.editMesh->edges.first; eed; eed=eed->next){
				if(!eed->h){
					if(eed->v1->f1 && !eed->v2->f1){ 
							eed->v2->f1 = 1;
							totnodes++;
							unbalanced = 1;
					}
					else if(eed->v2->f1 && !eed->v1->f1){
							eed->v1->f1 = 1;
							totnodes++;
							unbalanced = 1;
					}
				}
			}
		}
		
		
		
		if(s->f1 && t->f1){ /*t can be reached by s*/
			Q = MEM_callocN(sizeof(PathNode)*totnodes, "Path Select Nodes");
			totnodes = 0;
			for(eve=G.editMesh->verts.first; eve; eve=eve->next){
				if(eve->f1){
					Q[totnodes].u = totnodes;
					Q[totnodes].edges.first = 0;
					Q[totnodes].edges.last = 0;
					Q[totnodes].visited = 0;
					eve->tmp.p = &(Q[totnodes]);
					totnodes++;
				}
				else eve->tmp.p = NULL;
			}
			
			for(eed=G.editMesh->edges.first; eed; eed=eed->next){
				if(!eed->h){
					if(eed->v1->f1){
						currpn = ((PathNode*)eed->v1->tmp.p);
						
						newpe = MEM_mallocN(sizeof(PathEdge), "Path Edge");
						newpe->v = ((PathNode*)eed->v2->tmp.p)->u;
						if(physical){
								newpe->w = VecLenf(eed->v1->co, eed->v2->co);
						}
						else newpe->w = 1;
						newpe->next = 0;
						newpe->prev = 0;
						BLI_addtail(&(currpn->edges), newpe);
					} 
					if(eed->v2->f1){
						currpn = ((PathNode*)eed->v2->tmp.p); 
						newpe = MEM_mallocN(sizeof(PathEdge), "Path Edge");
						newpe->v = ((PathNode*)eed->v1->tmp.p)->u;
						if(physical){
								newpe->w = VecLenf(eed->v1->co, eed->v2->co);
						}
						else newpe->w = 1;
						newpe->next = 0;
						newpe->prev = 0;
						BLI_addtail(&(currpn->edges), newpe);
					}
				}
			}
			
			heap = BLI_heap_new();
			cost = MEM_callocN(sizeof(float)*totnodes, "Path Select Costs");
			previous = MEM_callocN(sizeof(int)*totnodes, "PathNode indices");
			
			for(v=0; v < totnodes; v++){
				cost[v] = 1000000;
				previous[v] = -1; /*array of indices*/
			}
			
			pnindex = ((PathNode*)s->tmp.p)->u;
			cost[pnindex] = 0;
			BLI_heap_insert(heap,  0.0f, SET_INT_IN_POINTER(pnindex));
						
			while( !BLI_heap_empty(heap) ){
				
				pnindex = GET_INT_FROM_POINTER(BLI_heap_popmin(heap));
				currpn = &(Q[pnindex]);
				
				if(currpn == (PathNode*)t->tmp.p) /*target has been reached....*/
					break;
				
				for(currpe=currpn->edges.first; currpe; currpe=currpe->next){
					if(!Q[currpe->v].visited){
						if( cost[currpe->v] > (cost[currpn->u ] + currpe->w) ){
							cost[currpe->v] = cost[currpn->u] + currpe->w;
							previous[currpe->v] = currpn->u;
							Q[currpe->v].visited = 1;
							BLI_heap_insert(heap, cost[currpe->v], SET_INT_IN_POINTER(currpe->v));
						}
					}
				}
			}
			
			pathvert = ((PathNode*)t->tmp.p)->u;
			while(pathvert != -1){
				for(eve=G.editMesh->verts.first; eve; eve=eve->next){
					if(eve->f1){
						if( ((PathNode*)eve->tmp.p)->u == pathvert) eve->f |= SELECT;
					}
				}
				pathvert = previous[pathvert];
			}
			
			for(v=0; v < totnodes; v++) BLI_freelistN(&(Q[v].edges));
			MEM_freeN(Q);
			MEM_freeN(cost);
			MEM_freeN(previous);
			BLI_heap_free(heap, NULL);
			EM_select_flush();
			countall();
			DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
			allqueue(REDRAWVIEW3D, 0);
			if (EM_texFaceCheck())
				allqueue(REDRAWIMAGE, 0);
		}
	}
	else{
		error("Path Selection requires that exactly two vertices be selected");
		return;
	}
}

void region_to_loop(void)
{
	EditEdge *eed;
	EditFace *efa;
	
	if(G.totfacesel){
		for(eed=G.editMesh->edges.first; eed; eed=eed->next) eed->f1 = 0;
		
		for(efa=G.editMesh->faces.first; efa; efa=efa->next){
			if(efa->f&SELECT){
				efa->e1->f1++;
				efa->e2->f1++;
				efa->e3->f1++;
				if(efa->e4)
					efa->e4->f1++;
			}
		}
		
		EM_clear_flag_all(SELECT);
		
		for(eed=G.editMesh->edges.first; eed; eed=eed->next){
			if(eed->f1 == 1) EM_select_edge(eed, 1);
		}
		
		G.scene->selectmode = SCE_SELECT_EDGE;
		EM_selectmode_set();
		countall();
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		if (EM_texFaceCheck())
			allqueue(REDRAWIMAGE, 0);
		BIF_undo_push("Face Region to Edge Loop");

	}
}

static int validate_loop(Collection *edgecollection)
{
	EditEdge *eed;
	EditFace *efa;
	CollectedEdge *curredge;
	
	/*1st test*/
	for(curredge = (CollectedEdge*)edgecollection->collectionbase.first; curredge; curredge=curredge->next){
		curredge->eed->v1->f1 = 0;
		curredge->eed->v2->f1 = 0;
	}
	for(curredge = (CollectedEdge*)edgecollection->collectionbase.first; curredge; curredge=curredge->next){
		curredge->eed->v1->f1++;
		curredge->eed->v2->f1++;
	}
	for(curredge = (CollectedEdge*)edgecollection->collectionbase.first; curredge; curredge=curredge->next){
		if(curredge->eed->v1->f1 > 2) return(0); else
		if(curredge->eed->v2->f1 > 2) return(0);
	}
	
	/*2nd test*/
	for(eed = G.editMesh->edges.first; eed; eed=eed->next) eed->f1 = 0;
	for(efa=G.editMesh->faces.first; efa; efa=efa->next){
		efa->e1->f1++;
		efa->e2->f1++;
		efa->e3->f1++;
		if(efa->e4) efa->e4->f1++;
	}
	for(curredge = (CollectedEdge*)edgecollection->collectionbase.first; curredge; curredge=curredge->next){
		if(curredge->eed->f1 > 2) return(0);
	}
	return(1);
}

static int loop_bisect(Collection *edgecollection){
	
	EditFace *efa, *sf1, *sf2;
	EditEdge *eed, *sed;
	CollectedEdge *curredge;
	int totsf1, totsf2, unbalanced,balancededges;
	
	for(eed=G.editMesh->edges.first; eed; eed=eed->next) eed->f1 = eed->f2 = 0;
	for(efa=G.editMesh->faces.first; efa; efa=efa->next) efa->f1 = 0;	
	
	for(curredge = (CollectedEdge*)edgecollection->collectionbase.first; curredge; curredge=curredge->next) curredge->eed->f1 = 1;
	
	sf1 = sf2 = NULL;
	sed = ((CollectedEdge*)edgecollection->collectionbase.first)->eed;
	
	for(efa=G.editMesh->faces.first; efa; efa=efa->next){
		if(sf2) break;
		else if(sf1){
			if(efa->e1 == sed || efa->e2 == sed || efa->e3 == sed || ( (efa->e4) ? efa->e4 == sed : 0) ) sf2 = efa;
		}
		else{
			if(efa->e1 == sed || efa->e2 == sed || efa->e3 == sed || ( (efa->e4) ? efa->e4 == sed : 0) ) sf1 = efa;
		}
	}
	
	if(sf1==NULL || sf2==NULL)
		return(-1);
	
	if(!(sf1->e1->f1)) sf1->e1->f2 = 1;
	if(!(sf1->e2->f1)) sf1->e2->f2 = 1;
	if(!(sf1->e3->f1)) sf1->e3->f2 = 1;
	if(sf1->e4 && !(sf1->e4->f1)) sf1->e4->f2 = 1;
	sf1->f1 = 1;
	totsf1 = 1;
	
	if(!(sf2->e1->f1)) sf2->e1->f2 = 2;
	if(!(sf2->e2->f1)) sf2->e2->f2 = 2;
	if(!(sf2->e3->f1)) sf2->e3->f2 = 2;
	if(sf2->e4 && !(sf2->e4->f1)) sf2->e4->f2 = 2;
	sf2->f1 = 2;
	totsf2 = 1;
	
	/*do sf1*/
	unbalanced = 1;
	while(unbalanced){
		unbalanced = 0;
		for(efa=G.editMesh->faces.first; efa; efa=efa->next){
			balancededges = 0;
			if(efa->f1 == 0){
				if(efa->e1->f2 == 1 || efa->e2->f2 == 1 || efa->e3->f2 == 1 || ( (efa->e4) ? efa->e4->f2 == 1 : 0) ){
					balancededges += efa->e1->f2 = (efa->e1->f1) ? 0 : 1;
					balancededges += efa->e2->f2 = (efa->e2->f1) ? 0 : 1;
					balancededges += efa->e3->f2 = (efa->e3->f1) ? 0 : 1;
					if(efa->e4) balancededges += efa->e4->f2 = (efa->e4->f1) ? 0 : 1;
					if(balancededges){
						unbalanced = 1;
						efa->f1 = 1;
						totsf1++;
					}
				}
			}
		}
	}
	
	/*do sf2*/
	unbalanced = 1;
	while(unbalanced){
		unbalanced = 0;
		for(efa=G.editMesh->faces.first; efa; efa=efa->next){
			balancededges = 0;
			if(efa->f1 == 0){
				if(efa->e1->f2 == 2 || efa->e2->f2 == 2 || efa->e3->f2 == 2 || ( (efa->e4) ? efa->e4->f2 == 2 : 0) ){
					balancededges += efa->e1->f2 = (efa->e1->f1) ? 0 : 2;
					balancededges += efa->e2->f2 = (efa->e2->f1) ? 0 : 2;
					balancededges += efa->e3->f2 = (efa->e3->f1) ? 0 : 2;
					if(efa->e4) balancededges += efa->e4->f2 = (efa->e4->f1) ? 0 : 2;
					if(balancededges){
						unbalanced = 1;
						efa->f1 = 2;
						totsf2++;
					}
				}
			}
		}
	}
		
	if(totsf1 < totsf2) return(1);
	else return(2);
}

void loop_to_region(void)
{
	EditFace *efa;
	ListBase allcollections={NULL,NULL};
	Collection *edgecollection;
	int testflag;
	
	build_edgecollection(&allcollections);
	
	for(edgecollection = (Collection *)allcollections.first; edgecollection; edgecollection=edgecollection->next){
		if(validate_loop(edgecollection)){
			testflag = loop_bisect(edgecollection);
			for(efa=G.editMesh->faces.first; efa; efa=efa->next){
				if(efa->f1 == testflag){
					if(efa->f&SELECT) EM_select_face(efa, 0);
					else EM_select_face(efa,1);
				}
			}
		}
	}
	
	for(efa=G.editMesh->faces.first; efa; efa=efa->next){ /*fix this*/
		if(efa->f&SELECT) EM_select_face(efa,1);
	}
	
	countall();
	freecollections(&allcollections);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	allqueue(REDRAWVIEW3D, 0);
	if (EM_texFaceCheck())
		allqueue(REDRAWIMAGE, 0);
	BIF_undo_push("Edge Loop to Face Region");
}


/* texface and vertex color editmode tools for the face menu */

void mesh_rotate_uvs(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	short change = 0, ccw;
	MTFace *tf;
	float u1, v1;
	
	if (!EM_texFaceCheck()) {
		error("mesh has no uv/image layers");
		return;
	}
	
	ccw = (G.qual == LR_SHIFTKEY);
	
	for(efa=em->faces.first; efa; efa=efa->next) {
		if (efa->f & SELECT) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			u1= tf->uv[0][0];
			v1= tf->uv[0][1];
			
			if (ccw) {
				if(efa->v4) {
					tf->uv[0][0]= tf->uv[3][0];
					tf->uv[0][1]= tf->uv[3][1];
					
					tf->uv[3][0]= tf->uv[2][0];
					tf->uv[3][1]= tf->uv[2][1];
				} else {
					tf->uv[0][0]= tf->uv[2][0];
					tf->uv[0][1]= tf->uv[2][1];
				}
				
				tf->uv[2][0]= tf->uv[1][0];
				tf->uv[2][1]= tf->uv[1][1];
				
				tf->uv[1][0]= u1;
				tf->uv[1][1]= v1;
			} else {	
				tf->uv[0][0]= tf->uv[1][0];
				tf->uv[0][1]= tf->uv[1][1];
	
				tf->uv[1][0]= tf->uv[2][0];
				tf->uv[1][1]= tf->uv[2][1];
				
				if(efa->v4) {
					tf->uv[2][0]= tf->uv[3][0];
					tf->uv[2][1]= tf->uv[3][1];
				
					tf->uv[3][0]= u1;
					tf->uv[3][1]= v1;
				}
				else {
					tf->uv[2][0]= u1;
					tf->uv[2][1]= v1;
				}
			}
			change = 1;
		}
	}
	
	if (change) {
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Rotate UV face");
	}
}

void mesh_mirror_uvs(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	short change = 0, altaxis;
	MTFace *tf;
	float u1, v1;
	
	if (!EM_texFaceCheck()) {
		error("mesh has no uv/image layers");
		return;
	}
	
	altaxis = (G.qual == LR_SHIFTKEY);
	
	for(efa=em->faces.first; efa; efa=efa->next) {
		if (efa->f & SELECT) {
			tf = CustomData_em_get(&em->fdata, efa->data, CD_MTFACE);
			if (altaxis) {
				u1= tf->uv[1][0];
				v1= tf->uv[1][1];
				if(efa->v4) {
				
					tf->uv[1][0]= tf->uv[2][0];
					tf->uv[1][1]= tf->uv[2][1];
			
					tf->uv[2][0]= u1;
					tf->uv[2][1]= v1;

					u1= tf->uv[3][0];
					v1= tf->uv[3][1];

					tf->uv[3][0]= tf->uv[0][0];
					tf->uv[3][1]= tf->uv[0][1];
			
					tf->uv[0][0]= u1;
					tf->uv[0][1]= v1;
				}
				else {
					tf->uv[1][0]= tf->uv[2][0];
					tf->uv[1][1]= tf->uv[2][1];
					tf->uv[2][0]= u1;
					tf->uv[2][1]= v1;
				}
				
			} else {
				u1= tf->uv[0][0];
				v1= tf->uv[0][1];
				if(efa->v4) {
				
					tf->uv[0][0]= tf->uv[1][0];
					tf->uv[0][1]= tf->uv[1][1];
			
					tf->uv[1][0]= u1;
					tf->uv[1][1]= v1;

					u1= tf->uv[3][0];
					v1= tf->uv[3][1];

					tf->uv[3][0]= tf->uv[2][0];
					tf->uv[3][1]= tf->uv[2][1];
			
					tf->uv[2][0]= u1;
					tf->uv[2][1]= v1;
				}
				else {
					tf->uv[0][0]= tf->uv[1][0];
					tf->uv[0][1]= tf->uv[1][1];
					tf->uv[1][0]= u1;
					tf->uv[1][1]= v1;
				}
			}
			change = 1;
		}
	}
	
	if (change) {
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Mirror UV face");
	}
}

void mesh_rotate_colors(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	short change = 0, ccw;
	MCol tmpcol, *mcol;
	if (!EM_vertColorCheck()) {
		error("mesh has no color layers");
		return;
	}
	
	ccw = (G.qual == LR_SHIFTKEY);
	
	for(efa=em->faces.first; efa; efa=efa->next) {
		if (efa->f & SELECT) {
			mcol = CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
			tmpcol= mcol[0];
			
			if (ccw) {
				if(efa->v4) {
					mcol[0]= mcol[3];
					mcol[3]= mcol[2];
				} else {
					mcol[0]= mcol[2];
				}
				mcol[2]= mcol[1];
				mcol[1]= tmpcol;
			} else {
				mcol[0]= mcol[1];
				mcol[1]= mcol[2];
	
				if(efa->v4) {
					mcol[2]= mcol[3];
					mcol[3]= tmpcol;
				}
				else
					mcol[2]= tmpcol;
			}
			change = 1;
		}
	}
	
	if (change) {
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Rotate Color face");
	}	
}

void mesh_mirror_colors(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	short change = 0, altaxis;
	MCol tmpcol, *mcol;
	if (!EM_vertColorCheck()) {
		error("mesh has no color layers");
		return;
	}
	
	altaxis = (G.qual == LR_SHIFTKEY);
	
	for(efa=em->faces.first; efa; efa=efa->next) {
		if (efa->f & SELECT) {
			mcol = CustomData_em_get(&em->fdata, efa->data, CD_MCOL);
			if (altaxis) {
				tmpcol= mcol[1];
				mcol[1]= mcol[2];
				mcol[2]= tmpcol;
				
				if(efa->v4) {
					tmpcol= mcol[0];
					mcol[0]= mcol[3];
					mcol[3]= tmpcol;
				}
			} else {
				tmpcol= mcol[0];
				mcol[0]= mcol[1];
				mcol[1]= tmpcol;
				
				if(efa->v4) {
					tmpcol= mcol[2];
					mcol[2]= mcol[3];
					mcol[3]= tmpcol;
				}
			}
			change = 1;
		}
	}
	
	if (change) {
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
		allqueue(REDRAWVIEW3D, 0);
		BIF_undo_push("Mirror Color face");
	}
}
