/**
 * $Id: 
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
 * The Original Code is Copyright (C) 2004 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Johnny Matthews.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
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
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"
#include "BLI_rand.h"
#include "BLI_ghash.h"
#include "BLI_linklist.h"

#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_cursors.h"
#include "BIF_editmesh.h"
#include "BIF_gl.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_resources.h"
#include "BIF_toolbox.h"
#include "BIF_transform.h"

#include "BDR_drawobject.h"
#include "BDR_editobject.h"

#include "BSE_view.h"
#include "BSE_edit.h"

#include "mydevice.h"
#include "blendef.h"

#include "editmesh.h"

#include "MTC_vectorops.h"

#include "PIL_time.h"

/* local prototypes ---------------*/
void bevel_menu(void);
static void free_tagged_edgelist(EditEdge *eed);
static void free_tagged_facelist(EditFace *efa);

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
	
	efa= em->faces.last;
	while(efa) {
		next= efa->prev;
		if(efa->v4) {
			if(efa->f & SELECT) {
				/* choose shortest diagonal for split */
				fac= VecLenf(efa->v1->co, efa->v3->co) - VecLenf(efa->v2->co, efa->v4->co);
				/* this makes sure exact squares get split different in both cases */
				if( (direction==0 && fac<FLT_EPSILON) || (direction && fac>0.0f) ) {
					
					efan= addfacelist(efa->v1, efa->v2, efa->v3, 0, efa, NULL);
					if(efa->f & SELECT) EM_select_face(efan, 1);
					efan= addfacelist(efa->v1, efa->v3, efa->v4, 0, efa, NULL);
					if(efa->f & SELECT) EM_select_face(efan, 1);

					efan->tf.uv[1][0]= efan->tf.uv[2][0];
					efan->tf.uv[1][1]= efan->tf.uv[2][1];
					efan->tf.uv[2][0]= efan->tf.uv[3][0];
					efan->tf.uv[2][1]= efan->tf.uv[3][1];
					
					efan->tf.col[1]= efan->tf.col[2];
					efan->tf.col[2]= efan->tf.col[3];
				}
				else {
					efan= addfacelist(efa->v1, efa->v2, efa->v4, 0, efa, NULL);
					if(efa->f & SELECT) EM_select_face(efan, 1);
					
					efan->tf.uv[2][0]= efan->tf.uv[3][0];
					efan->tf.uv[2][1]= efan->tf.uv[3][1];
					efan->tf.col[2]= efan->tf.col[3];
					
					efan= addfacelist(efa->v2, efa->v3, efa->v4, 0, efa, NULL);
					if(efa->f & SELECT) EM_select_face(efan, 1);
					
					efan->tf.uv[0][0]= efan->tf.uv[1][0];
					efan->tf.uv[0][1]= efan->tf.uv[1][1];
					efan->tf.uv[1][0]= efan->tf.uv[2][0];
					efan->tf.uv[1][1]= efan->tf.uv[2][1];
					efan->tf.uv[2][0]= efan->tf.uv[3][0];
					efan->tf.uv[2][1]= efan->tf.uv[3][1];
					
					efan->tf.col[0]= efan->tf.col[1];
					efan->tf.col[1]= efan->tf.col[2];
					efan->tf.col[2]= efan->tf.col[3];
				}
				
				BLI_remlink(&em->faces, efa);
				free_editface(efa);
			}
		}
		efa= next;
	}
	
	EM_fgon_flags();	// redo flags and indices for fgons
	BIF_undo_push("Convert Quads to Triangles");
	
}


int removedoublesflag(short flag, float limit)		/* return amount */
{
	EditMesh *em = G.editMesh;
	/* all verts with (flag & 'flag') are being evaluated */
	EditVert *eve, *v1, *nextve;
	EditEdge *eed, *e1, *nexted;
	EditFace *efa, *nextvl;
	xvertsort *sortblock, *sb, *sb1;
	struct facesort *vlsortblock, *vsb, *vsb1;
	float dist;
	int a, b, test, amount;

	/* flag 128 is cleared, count */
	eve= em->verts.first;
	amount= 0;
	while(eve) {
		eve->f &= ~128;
		if(eve->h==0 && (eve->f & flag)) amount++;
		eve= eve->next;
	}
	if(amount==0) return 0;

	/* allocate memory and qsort */
	sb= sortblock= MEM_mallocN(sizeof(xvertsort)*amount,"sortremovedoub");
	eve= em->verts.first;
	while(eve) {
		if(eve->h==0 && (eve->f & flag)) {
			sb->x= eve->co[0]+eve->co[1]+eve->co[2];
			sb->v1= eve;
			sb++;
		}
		eve= eve->next;
	}
	qsort(sortblock, amount, sizeof(xvertsort), vergxco);

	/* test for doubles */
	sb= sortblock;
	for(a=0; a<amount; a++) {
		eve= sb->v1;
		if( (eve->f & 128)==0 ) {
			sb1= sb+1;
			for(b=a+1; b<amount; b++) {
				/* first test: simpel dist */
				dist= sb1->x - sb->x;
				if(dist > limit) break;
				
				/* second test: is vertex allowed */
				v1= sb1->v1;
				if( (v1->f & 128)==0 ) {
					
					dist= (float)fabs(v1->co[0]-eve->co[0]);
					if(dist<=limit) {
						dist= (float)fabs(v1->co[1]-eve->co[1]);
						if(dist<=limit) {
							dist= (float)fabs(v1->co[2]-eve->co[2]);
							if(dist<=limit) {
								v1->f|= 128;
								v1->vn= eve;
							}
						}
					}
				}
				sb1++;
			}
		}
		sb++;
	}
	MEM_freeN(sortblock);

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

				if(eed->v1->f & 128) eed->v1= eed->v1->vn;
				if(eed->v2->f & 128) eed->v2= eed->v2->vn;
				e1= addedgelist(eed->v1, eed->v2, eed);
				
				if(e1) e1->f2= 1;
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
			
			if(efa->v1->f & 128) efa->v1= efa->v1->vn;
			if(efa->v2->f & 128) efa->v2= efa->v2->vn;
			if(efa->v3->f & 128) efa->v3= efa->v3->vn;
			if(efa->v4 && (efa->v4->f & 128)) efa->v4= efa->v4->vn;
		
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
		if(faceselectedAND(efa, 1)) {
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
		if(eve->f & flag) {
			if(eve->f & 128) {
				a++;
				BLI_remlink(&em->verts, eve);
				free_editvert(eve);
			}
		}
		eve= nextve;
	}

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
	BIF_undo_push("Hash");

}

/* generic extern called extruder */
void extrude_mesh(void)
{
	float nor[3]= {0.0, 0.0, 0.0};
	short nr, transmode= 0;

	TEST_EDITMESH
	
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
			initTransform(TFM_SHRINKFATTEN, CTX_NO_PET);
			Transform();
		}
		else {
			initTransform(TFM_TRANSLATION, CTX_NO_PET);
			if(transmode=='n') {
				Mat4MulVecfl(G.obedit->obmat, nor);
				VecSubf(nor, nor, G.obedit->obmat[3]);
				BIF_setSingleAxisConstraint(nor, NULL);
			}
			Transform();
		}
	}

}

void split_mesh(void)
{

	TEST_EDITMESH

	if(okee(" Split ")==0) return;

	waitcursor(1);

	/* make duplicate first */
	adduplicateflag(SELECT);
	/* old faces have flag 128 set, delete them */
	delfaceflag(128);

	waitcursor(0);

	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	BIF_undo_push("Split");

}

void extrude_repeat_mesh(int steps, float offs)
{
	float dvec[3], tmat[3][3], bmat[3][3], nor[3]= {0.0, 0.0, 0.0};
	short a;

	TEST_EDITMESH
	
	/* dvec */
	dvec[0]= G.vd->persinv[2][0];
	dvec[1]= G.vd->persinv[2][1];
	dvec[2]= G.vd->persinv[2][2];
	Normalise(dvec);
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

void spin_mesh(int steps,int degr,float *dvec, int mode)
{
	EditMesh *em = G.editMesh;
	EditVert *eve,*nextve;
	float nor[3]= {0.0, 0.0, 0.0};
	float *curs, si,n[3],q[4],cmat[3][3],imat[3][3], tmat[3][3];
	float cent[3],bmat[3][3];
	float phi;
	short a,ok;

	TEST_EDITMESH
	
	/* imat and centre and size */
	Mat3CpyMat4(bmat, G.obedit->obmat);
	Mat3Inv(imat,bmat);

	curs= give_cursor();
	VECCOPY(cent, curs);
	cent[0]-= G.obedit->obmat[3][0];
	cent[1]-= G.obedit->obmat[3][1];
	cent[2]-= G.obedit->obmat[3][2];
	Mat3MulVecfl(imat, cent);

	phi= (float)(degr*M_PI/360.0);
	phi/= steps;
	if(G.scene->toolsettings->editbutflag & B_CLOCKWISE) phi= -phi;

	if(dvec) {
		n[0]=n[1]= 0.0;
		n[2]= 1.0;
	} else {
		n[0]= G.vd->viewinv[2][0];
		n[1]= G.vd->viewinv[2][1];
		n[2]= G.vd->viewinv[2][2];
		Normalise(n);
	}

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

void screw_mesh(int steps,int turns)
{
	EditMesh *em = G.editMesh;
	EditVert *eve,*v1=0,*v2=0;
	EditEdge *eed;
	float dvec[3], nor[3];

	TEST_EDITMESH

	/* first condition: we need frontview! */
	if(G.vd->view!=1) {
		error("Must be in Front View");
		return;
	}
	
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
		error("No curve is selected");
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
	
	event= pupmenu("Erase %t|Vertices%x10|Edges%x1|Faces%x2|All%x3|Edges & Faces%x4|Only Faces%x5");
	if(event<1) return;

	if(event==10 ) {
		str= "Erase Vertices";
		erase_edges(&em->edges);
		erase_faces(&em->faces);
		erase_vertices(&em->verts);
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

	waitcursor(1);

	/* copy all selected vertices */
	eve= em->verts.first;
	while(eve) {
		if(eve->f & SELECT) {
			v1= BLI_addfillvert(eve->co);
			eve->vn= v1;
			v1->vn= eve;
			v1->xs= 0;	// used for counting edges
		}
		eve= eve->next;
	}
	/* copy all selected edges */
	eed= em->edges.first;
	while(eed) {
		if( (eed->v1->f & SELECT) && (eed->v2->f & SELECT) ) {
			e1= BLI_addfilledge(eed->v1->vn, eed->v2->vn);
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
			efa->v1->vn->xs--;
			efa->v2->vn->xs--;
			efa->v3->vn->xs--;
			if(efa->v4) efa->v4->vn->xs--;
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
			efan= addfacelist(efa->v3->vn, efa->v2->vn, efa->v1->vn, 0, NULL, NULL); // normals default pointing up
			EM_select_face(efan, 1);
			efa= efa->next;
		}
	}

	BLI_end_edgefill();

	waitcursor(0);
	EM_select_flush();
	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	BIF_undo_push("Fill");
}

/*--------------Edge Based Subdivide------------------*/
#define EDGENEW	2
#define FACENEW	2
#define EDGEINNER  4

static void alter_co(float* co,EditEdge *edge,float rad,int beauty,float perc)
{
	float  vec1[3],fac;
	
	if(rad > 0.0) {   /* subdivide sphere */
		Normalise(co);
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

	if(beauty & B_SMOOTH) {		 
		float len, fac, nor[3], nor1[3], nor2[3];
		
		VecSubf(nor, edge->v1->co, edge->v2->co);
		len= 0.5f*Normalise(nor);
	
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
	
		if(perc > .5){
			perc = 1-perc;	
		}
		perc /= 2;
		
		vec1[0]*= perc*len;
		vec1[1]*= perc*len;
		vec1[2]*= perc*len;
		
		co[0] += vec1[0];
		co[1] += vec1[1];
		co[2] += vec1[2];
	}	
}

static void flipvertarray(EditVert** arr, short size)
{
	EditVert *hold;
	int i;
	
	for(i=0;i<size/2;i++){
		hold = arr[i];
		arr[i] = arr[size-i-1];
		arr[size-i-1] = hold;   
	}
}

static int VecEqual(float *a, float *b){
	if( a[0] == b[0] && 
	   (a[1] == b[1] && 
		a[2] == b[2])){
		return 1;
	}
	else{
		return 0;
	}
}

static void set_uv_vcol(EditFace *efa, float *co, float *uv, char *col)
{ 
	EditVert *v1,*v2,*v3,*v4;
	float xn, yn, zn;
	float t00, t01, t10, t11;
	float detsh, u, v, l;
	int fac;
	short i, j;
	char *cp0, *cp1, *cp2;
	char *hold;
	
	//First Check for exact match between co and efa verts
	if(VecEqual(co,efa->v1->co)){
		uv[0] = efa->tf.uv[0][0];
		uv[1] = efa->tf.uv[0][1];
		
		hold = (char*)&efa->tf.col[0];
		col[0]= hold[0];
		col[1]= hold[1];
		col[2]= hold[2];
		col[3]= hold[3];
		return;	  
	} else if(VecEqual(co,efa->v2->co)){
		uv[0] = efa->tf.uv[1][0];
		uv[1] = efa->tf.uv[1][1];
		
		hold = (char*)&efa->tf.col[1];
		col[0]= hold[0];
		col[1]= hold[1];
		col[2]= hold[2];
		col[3]= hold[3]; 
		return;	   
	} else if(VecEqual(co,efa->v3->co)){
		uv[0] = efa->tf.uv[2][0];
		uv[1] = efa->tf.uv[2][1];
		
		hold = (char*)&efa->tf.col[2];
		col[0]= hold[0];
		col[1]= hold[1];
		col[2]= hold[2];
		col[3]= hold[3];	
		return;   
	} else if(efa->v4 && VecEqual(co,efa->v4->co)){
		uv[0] = efa->tf.uv[3][0];
		uv[1] = efa->tf.uv[3][1];
		
		hold = (char*)&efa->tf.col[3];
		col[0]= hold[0];
		col[1]= hold[1];
		col[2]= hold[2];
		col[3]= hold[3];   
		return;	 
	}	
	
	/* define best projection of face XY, XZ or YZ */
	xn= fabs(efa->n[0]);
	yn= fabs(efa->n[1]);
	zn= fabs(efa->n[2]);
	if(zn>=xn && zn>=yn) {i= 0; j= 1;}
	else if(yn>=xn && yn>=zn) {i= 0; j= 2;}
	else {i= 1; j= 2;} 
	
	/* calculate u and v */
	v1= efa->v1;
	v2= efa->v2;
	v3= efa->v3;
		
	t00= v3->co[i]-v1->co[i]; t01= v3->co[j]-v1->co[j];
	t10= v3->co[i]-v2->co[i]; t11= v3->co[j]-v2->co[j];
		
	detsh= 1.0/(t00*t11-t10*t01);	/* potential danger */
	t00*= detsh; t01*=detsh;
	t10*=detsh; t11*=detsh;
		
	u= (co[i]-v3->co[i])*t11-(co[j]-v3->co[j])*t10;
	v= (co[j]-v3->co[j])*t00-(co[i]-v3->co[i])*t01; 
	
	/* btw; u and v range from -1 to 0 */
		
	/* interpolate */
	l= 1.0+u+v;
		/* outside triangle? */
	// printf("l: %f\n",l);
	if(efa->v4 && l >= 0.5) {
	//	printf("outside\n");
		/* do it all over, but now with vertex 2 replaced with 4 */
		
		/* calculate u and v */
		v1= efa->v1;
		v4= efa->v4;
		v3= efa->v3;
				
		t00= v3->co[i]-v1->co[i]; t01= v3->co[j]-v1->co[j];
		t10= v3->co[i]-v4->co[i]; t11= v3->co[j]-v4->co[j];
				
		detsh= 1.0/(t00*t11-t10*t01);	/* potential danger */
		t00*= detsh; t01*=detsh;
		t10*=detsh; t11*=detsh;
				
		u= (co[i]-v3->co[i])*t11-(co[j]-v3->co[j])*t10;
		v= (co[j]-v3->co[j])*t00-(co[i]-v3->co[i])*t01; 
		
		/* btw; u and v range from -1 to 0 */
				
		/* interpolate */
		l= 1.0+u+v;
		uv[0] = (l*efa->tf.uv[2][0] - u*efa->tf.uv[0][0] - v*efa->tf.uv[3][0]);
		uv[1] = (l*efa->tf.uv[2][1] - u*efa->tf.uv[0][1] - v*efa->tf.uv[3][1]);

		cp0= (char*)&(efa->tf.col[0]);
		cp1= (char*)&(efa->tf.col[3]);
		cp2= (char*)&(efa->tf.col[2]);
		
		
		for(i=0; i<4; i++) {
				fac= (int)(l*cp2[i] - u*cp0[i] - v*cp1[i]);
				col[i]= CLAMPIS(fac, 0, 255);
		}			  
	} else {	 
	//	printf("inside\n");	 
		//new = l*vertex3_val - u*vertex1_val - v*vertex2_val;
		uv[0] = (l*efa->tf.uv[2][0] - u*efa->tf.uv[0][0] - v*efa->tf.uv[1][0]);
		uv[1] = (l*efa->tf.uv[2][1] - u*efa->tf.uv[0][1] - v*efa->tf.uv[1][1]);

		cp0= (char*)&(efa->tf.col[0]);
		cp1= (char*)&(efa->tf.col[1]);
		cp2= (char*)&(efa->tf.col[2]);
		
		for(i=0; i<4; i++) {
				fac= (int)(l*cp2[i] - u*cp0[i] - v*cp1[i]);
				col[i]= CLAMPIS(fac, 0, 255);
		}					   
	}
} 

static void facecopy(EditFace *source,EditFace *target)
{

	set_uv_vcol(source,target->v1->co,target->tf.uv[0],(char*)&target->tf.col[0]);
	set_uv_vcol(source,target->v2->co,target->tf.uv[1],(char*)&target->tf.col[1]);
	set_uv_vcol(source,target->v3->co,target->tf.uv[2],(char*)&target->tf.col[2]);
	if(target->v4){
		set_uv_vcol(source,target->v4->co,target->tf.uv[3],(char*)&target->tf.col[3]);
	}

	target->mat_nr	 = source->mat_nr;
	target->tf.flag	= source->tf.flag&~TF_ACTIVE;
	target->tf.transp  = source->tf.transp;
	target->tf.mode	= source->tf.mode;
	target->tf.tile	= source->tf.tile;
	target->tf.unwrap  = source->tf.unwrap;
	target->tf.tpage   = source->tf.tpage;
	target->flag	   = source->flag;	
	
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

	if(verts[0] != v[start]){flipvertarray(verts,numcuts+2);}
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
	if(vertsize % 2 == 0){
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
	for(i=0;i<(vertsize-1)/2;i++){
		hold = addfacelist(verts[i],verts[i+1],v[right],NULL,NULL,NULL);  
		facecopy(efa,hold);
		if(i+1 != (vertsize-1)/2){
            if(seltype == SUBDIV_SELECT_INNER){
	 		   hold->e2->f2 |= EDGEINNER;
            }
		}
		hold = addfacelist(verts[vertsize-2-i],verts[vertsize-1-i],v[left],NULL,NULL,NULL); 
		facecopy(efa,hold);
		if(i+1 != (vertsize-1)/2){
            if(seltype == SUBDIV_SELECT_INNER){
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

	if(verts[0] != v[start]){flipvertarray(verts,numcuts+2);}
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
	for(i=0;i<(vertsize-1);i++){
		hold = addfacelist(verts[i],verts[i+1],v[op],NULL,NULL,NULL);  
		if(i+1 != vertsize-1){
            if(seltype == SUBDIV_SELECT_INNER){
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

	if(verts[0][0] != v[start]){flipvertarray(verts[0],numcuts+2);}
	end	= (start+1)%4;
	left   = (start+2)%4;
	right  = (start+3)%4; 
	if(verts[1][0] != v[left]){flipvertarray(verts[1],numcuts+2);}	
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
	for(i=0;i<vertsize-1;i++){
		hold = addfacelist(verts[0][i],verts[0][i+1],verts[1][vertsize-2-i],verts[1][vertsize-1-i],NULL,NULL);  
		if(i < vertsize-2){
			hold->e2->f2 |= EDGEINNER;
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

	if(verts[0][0] != v[start]){flipvertarray(verts[0],numcuts+2);}
	if(verts[1][0] != v[start2]){flipvertarray(verts[1],numcuts+2);}	
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
	hold->e3->f2 |= EDGEINNER;
	facecopy(efa,hold);	   
	hold = addfacelist(verts[0][0],verts[1][vertsize-1],v[(start2+2)%4],NULL,NULL,NULL);
	hold->e1->f2 |= EDGEINNER;  
	facecopy(efa,hold);			   
	if(G.scene->toolsettings->editbutflag & B_AUTOFGON){
		hold->e1->h |= EM_FGON;
	}	
	// Make side faces

	for(i=0;i<numcuts;i++){
		hold = addfacelist(verts[0][i],verts[0][i+1],verts[1][vertsize-1-(i+1)],verts[1][vertsize-1-i],NULL,NULL);  
		hold->e2->f2 |= EDGEINNER;
		facecopy(efa,hold);
	}
	EM_fgon_flags();
		  
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

	if(verts[0][0] != v[start]){flipvertarray(verts[0],numcuts+2);}
	if(verts[1][0] != v[start2]){flipvertarray(verts[1],numcuts+2);}	
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

	for(i=0;i<=numcuts;i++){
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

	if(verts[0][0] != v[start]){flipvertarray(verts[0],numcuts+2);}
	if(verts[1][0] != v[start2]){flipvertarray(verts[1],numcuts+2);}	
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
	
	for(i=0;i<numcuts;i++){
			co[0] = (verts[0][numcuts-i]->co[0] + verts[1][i+1]->co[0] ) / 2 ;
			co[1] = (verts[0][numcuts-i]->co[1] + verts[1][i+1]->co[1] ) / 2 ;
			co[2] = (verts[0][numcuts-i]->co[2] + verts[1][i+1]->co[2] ) / 2 ;
			inner[i] = addvertlist(co);
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
	
	if(G.scene->toolsettings->editbutflag & B_AUTOFGON){
		hold->e1->h |= EM_FGON;
	}	
	// Add Fill Quads (if # cuts > 1)

	for(i=0;i<numcuts-1;i++){
		hold = addfacelist(inner[i],verts[1][i+1],verts[1][i+2],inner[i+1],NULL,NULL);  
		hold->e1->f2 |= EDGEINNER;
		hold->e3->f2 |= EDGEINNER;
		facecopy(efa,hold);

		hold = addfacelist(inner[i],inner[i+1],verts[0][numcuts-1-i],verts[0][numcuts-i],NULL,NULL);  
		hold->e2->f2 |= EDGEINNER;
		hold->e4->f2 |= EDGEINNER;
		facecopy(efa,hold);	
		
		if(G.scene->toolsettings->editbutflag & B_AUTOFGON){
			hold->e1->h |= EM_FGON;
		}	
	}	
	
	EM_fgon_flags();
	
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

	if(verts[0][0] != v[start]){flipvertarray(verts[0],numcuts+2);}
	if(verts[1][0] != v[start2]){flipvertarray(verts[1],numcuts+2);}	
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

	for(i=0;i<numcuts;i++){
		hold = addfacelist(verts[0][i],verts[0][i+1],verts[1][vertsize-1-(i+1)],verts[1][vertsize-1-i],NULL,NULL);  
		hold->e2->f2 |= EDGEINNER;
		facecopy(efa,hold);
	}	  
}

static void fill_quad_triple(EditFace *efa, struct GHash *gh, int numcuts)
{
	EditEdge *cedge[3];
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
	if(verts[1][0] != v[start2]){flipvertarray(verts[1],numcuts+2);}  
	if(verts[2][0] != v[start3]){flipvertarray(verts[2],numcuts+2);}   
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
	if(numcuts % 2 == 0){
		hold = addfacelist(verts[0][1],verts[0][2],verts[2][vertsize-3],verts[2][vertsize-2],NULL,NULL);  
		hold->e2->f2 |= EDGEINNER;
		facecopy(efa,hold);		 
		// Also Make inner quad
		hold = addfacelist(verts[1][numcuts/2],verts[1][(numcuts/2)+1],verts[2][numcuts/2],verts[0][(numcuts/2)+1],NULL,NULL);		   
		hold->e3->f2 |= EDGEINNER;
		if(G.scene->toolsettings->editbutflag & B_AUTOFGON){
			hold->e3->h |= EM_FGON;
		}
		facecopy(efa,hold);
		repeats = (numcuts / 2) -1;
	} else {
		// Make inner tri	 
		hold = addfacelist(verts[1][(numcuts/2)+1],verts[2][(numcuts/2)+1],verts[0][(numcuts/2)+1],NULL,NULL,NULL);		   
		hold->e2->f2 |= EDGEINNER;
		if(G.scene->toolsettings->editbutflag & B_AUTOFGON){
			hold->e2->h |= EM_FGON;
		}
		facecopy(efa,hold);   
		repeats = ((numcuts+1) / 2)-1;
	}
	
	// cuts for 1 and 2 do not have the repeating quads
	if(numcuts < 3){repeats = 0;}
	for(i=0;i<repeats;i++){
		//Make side repeating Quads
		hold = addfacelist(verts[1][i+1],verts[1][i+2],verts[0][vertsize-i-3],verts[0][vertsize-i-2],NULL,NULL);  
		hold->e2->f2 |= EDGEINNER;		 
		facecopy(efa,hold);			   
		hold = addfacelist(verts[1][vertsize-i-3],verts[1][vertsize-i-2],verts[2][i+1],verts[2][i+2],NULL,NULL);		   
		hold->e4->f2 |= EDGEINNER;
		facecopy(efa,hold); 
	}
	// Do repeating bottom quads 
	for(i=0;i<repeats;i++){
		if(numcuts % 2 == 1){	 
			hold = addfacelist(verts[0][1+i],verts[0][2+i],verts[2][vertsize-3-i],verts[2][vertsize-2-i],NULL,NULL);  
		} else {
			hold = addfacelist(verts[0][2+i],verts[0][3+i],verts[2][vertsize-4-i],verts[2][vertsize-3-i],NULL,NULL);				  
		}
		hold->e2->f2 |= EDGEINNER;
		facecopy(efa,hold);				
	}	
	EM_fgon_flags();
}

static void fill_quad_quadruple(EditFace *efa, struct GHash *gh, int numcuts,float rad,int beauty)
{
	EditVert **verts[4], ***innerverts;
	short vertsize, i, j;
	float co[3];				
	EditFace *hold;	
	EditEdge temp;
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
				|		   |
			   1*		   *1
		 2	  |		   |	  4
			   2*		   *2	   
				|		   |
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
	for(i=0;i<numcuts+2;i++){
		innerverts[i] = MEM_mallocN(sizeof(EditVert*)*(numcuts+2),"quad-quad subdiv inner verts inner array");
	}   
	// first row is e1 last row is e3
	for(i=0;i<numcuts+2;i++){
		innerverts[0][i]		  = verts[0][(numcuts+1)-i];
		innerverts[numcuts+1][i]  = verts[2][(numcuts+1)-i];
	}		   
	for(i=1;i<=numcuts;i++){
		innerverts[i][0]		  = verts[1][i];
		innerverts[i][numcuts+1]  = verts[3][i];
		for(j=1;j<=numcuts;j++){ 
			co[0] = ((verts[1][i]->co[0] - verts[3][i]->co[0]) * (j /(float)(numcuts+1))) + verts[3][i]->co[0];
			co[1] = ((verts[1][i]->co[1] - verts[3][i]->co[1]) * (j /(float)(numcuts+1))) + verts[3][i]->co[1];
			co[2] = ((verts[1][i]->co[2] - verts[3][i]->co[2]) * (j /(float)(numcuts+1))) + verts[3][i]->co[2];		  

			temp.v1 = innerverts[i][0];
			temp.v2 = innerverts[i][numcuts+1];

			// Call alter co for things like fractal and smooth
			alter_co(co,&temp,rad,beauty,j/(float)(numcuts+1));

			innerverts[i][(numcuts+1)-j] = addvertlist(co); 


		}	
	}	
	// Fill with faces
	for(i=0;i<numcuts+1;i++){
		for(j=0;j<numcuts+1;j++){
			hold = addfacelist(innerverts[i][j+1],innerverts[i][j],innerverts[i+1][j],innerverts[i+1][j+1],NULL,NULL);	 
			hold->e1->f2 = EDGENEW;	  
			hold->e2->f2 = EDGENEW;  
			hold->e3->f2 = EDGENEW;			
			hold->e4->f2 = EDGENEW;   
			
			if(i != 0){ hold->e1->f2 |= EDGEINNER; }
			if(j != 0){ hold->e2->f2 |= EDGEINNER; }
			if(i != numcuts){ hold->e3->f2 |= EDGEINNER; }
			if(j != numcuts){ hold->e4->f2 |= EDGEINNER; }
			
			facecopy(efa,hold);		
		}		
	}
	// Clean up our dynamic multi-dim array
	for(i=0;i<numcuts+2;i++){
	   MEM_freeN(innerverts[i]);   
	}	
	MEM_freeN(innerverts);
}

static void fill_tri_triple(EditFace *efa, struct GHash *gh, int numcuts,float rad,int beauty)
{
	EditVert **verts[3], ***innerverts;
	short vertsize, i, j;
	float co[3];				
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
	for(i=0;i<numcuts+2;i++){
		  innerverts[i] = MEM_mallocN(sizeof(EditVert*)*((numcuts+2)-i),"tri-tri subdiv inner verts inner array");
	}
	//top row is e3 backwards
	for(i=0;i<numcuts+2;i++){
		  innerverts[0][i]		  = verts[2][(numcuts+1)-i];
	}   
		   
	for(i=1;i<=numcuts+1;i++){
		//first vert is from e1, last is from e2
		innerverts[i][0]			  = verts[0][i];
		innerverts[i][(numcuts+1)-i]  = verts[1][(numcuts+1)-i];		
		for(j=1;j<(numcuts+1)-i;j++){
			co[0] = ((verts[0][i]->co[0] - verts[1][(numcuts+1)-i]->co[0]) * (j/(float)((numcuts+1)-i))) + verts[1][(numcuts+1)-i]->co[0];
			co[1] = ((verts[0][i]->co[1] - verts[1][(numcuts+1)-i]->co[1]) * (j/(float)((numcuts+1)-i))) + verts[1][(numcuts+1)-i]->co[1];
			co[2] = ((verts[0][i]->co[2] - verts[1][(numcuts+1)-i]->co[2]) * (j/(float)((numcuts+1)-i))) + verts[1][(numcuts+1)-i]->co[2];						  

			temp.v1 = innerverts[i][0];
			temp.v2 = innerverts[i][(numcuts+1)-i];

			alter_co(co,&temp,rad,beauty,j/(float)((numcuts+1)-i));

			innerverts[i][((numcuts+1)-i)-j] = addvertlist(co);	
		}
	}


	// Now fill the verts with happy little tris :)
	for(i=0;i<=numcuts+1;i++){
		for(j=0;j<(numcuts+1)-i;j++){   
			//We always do the first tri
			hold = addfacelist(innerverts[i][j+1],innerverts[i][j],innerverts[i+1][j],NULL,NULL,NULL);	
			hold->e1->f2 |= EDGENEW;	  
			hold->e2->f2 |= EDGENEW;  
			hold->e3->f2 |= EDGENEW;  
			if(i != 0){ hold->e1->f2 |= EDGEINNER; }
			if(j != 0){ hold->e2->f2 |= EDGEINNER; }
			if(j+1 != (numcuts+1)-i){hold->e3->f2 |= EDGEINNER;}
			
			facecopy(efa,hold);		
			//if there are more to come, we do the 2nd	 
			if(j+1 <= numcuts-i){
				hold = addfacelist(innerverts[i+1][j],innerverts[i+1][j+1],innerverts[i][j+1],NULL,NULL,NULL);		   
				facecopy(efa,hold); 
				hold->e1->f2 |= EDGENEW;	  
				hold->e2->f2 |= EDGENEW;  
				hold->e3->f2 |= EDGENEW;  	
			}
		} 
	}

	// Clean up our dynamic multi-dim array
	for(i=0;i<numcuts+2;i++){
		MEM_freeN(innerverts[i]);   
	}	
	MEM_freeN(innerverts);
}

// This function takes an example edge, the current point to create and 
// the total # of points to create, then creates the point and return the
// editvert pointer to it.
static EditVert *subdivideedgenum(EditEdge *edge,int curpoint,int totpoint,float rad,int beauty)
{
	float co[3];
	float percent;
	EditVert *ev;
	 
	if (beauty & (B_PERCENTSUBD) && totpoint == 1){
		percent=(float)(edge->f1)/32768.0f;
		co[0] = (edge->v2->co[0]-edge->v1->co[0])*percent+edge->v1->co[0];
		co[1] = (edge->v2->co[1]-edge->v1->co[1])*percent+edge->v1->co[1];
		co[2] = (edge->v2->co[2]-edge->v1->co[2])*percent+edge->v1->co[2];					
	} else {				 
		co[0] = (edge->v2->co[0]-edge->v1->co[0])*(curpoint/(float)(totpoint+1))+edge->v1->co[0];
		co[1] = (edge->v2->co[1]-edge->v1->co[1])*(curpoint/(float)(totpoint+1))+edge->v1->co[1];
		co[2] = (edge->v2->co[2]-edge->v1->co[2])*(curpoint/(float)(totpoint+1))+edge->v1->co[2];
	}
			
	alter_co(co,edge,rad,beauty,curpoint/(float)(totpoint+1));
	ev = addvertlist(co);
	ev->f = edge->v1->f;
	
	return ev;
}

void esubdivideflag(int flag, float rad, int beauty, int numcuts, int seltype)
{
	EditMesh *em = G.editMesh;
	EditFace *ef;
	EditEdge *eed, *cedge, *sort[4];
	EditVert **templist;
	struct GHash *gh;
	int i,j,edgecount,facetype,hold;
	float length[4];
	
	//Set faces f1 to 0 cause we need it later
			
	for(ef=em->faces.first;ef;ef = ef->next){
		ef->f1 = 0;
	}
	for(eed = em->edges.first;eed;eed = eed->next){
		//Flush vertext flags upward to the edges
		//if(eed->f & flag && eed->v1->f == eed->v2->f){
		//	eed->f |= eed->v1->f;   
		// }
		eed->f2 = 0;   
	}   
	// We store an array of verts for each edge that is subdivided,
	// we put this array as a value in a ghash which is keyed by the EditEdge*

	// Now for beauty subdivide deselect edges based on length
	if(beauty & B_BEAUTY){ 
		for(ef = em->faces.first;ef;ef = ef->next){
			if(!ef->v4){
				continue;
			}
			if(ef->f & SELECT){
				length[0] = VecLenf(ef->e1->v1->co,ef->e1->v2->co);
				length[1] = VecLenf(ef->e2->v1->co,ef->e2->v2->co);
				length[2] = VecLenf(ef->e3->v1->co,ef->e3->v2->co);
				length[3] = VecLenf(ef->e4->v1->co,ef->e4->v2->co);
				sort[0] = ef->e1;
				sort[1] = ef->e2;
				sort[2] = ef->e3;
				sort[3] = ef->e4;
												  
												
				// Beauty Short Edges
				if(beauty & B_BEAUTY_SHORT){
					for(j=0;j<2;j++){
						hold = -1;
						for(i=0;i<4;i++){
							if(length[i] < 0){
								continue;							
							} else if(hold == -1){  
								hold = i; 
							} else {
								if(length[hold] < length[i]){
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
					 for(j=0;j<2;j++){
						hold = -1;
						for(i=0;i<4;i++){
							if(length[i] < 0){
								continue;							
							} else if(hold == -1){  
								hold = i; 
							} else {
								if(length[hold] > length[i]){
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
		for(eed= em->edges.first;eed;eed=eed->next){	
			if( eed->f1 == 0 ){
				EM_select_edge(eed,0);   
			}
		}
	}  
	// So for each edge, if it is selected, we allocate an array of size cuts+2
	// so we can have a place for the v1, the new verts and v2  
	for(eed=em->edges.first;eed;eed = eed->next){
		if(eed->f & flag){
			templist = MEM_mallocN(sizeof(EditVert*)*(numcuts+2),"vertlist");
			templist[0] = eed->v1;
			for(i=0;i<numcuts;i++){
				// This function creates the new vert and returns it back
				// to the array
				templist[i+1] = subdivideedgenum(eed,i+1,numcuts,rad,beauty);
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
	for(ef = em->faces.first;ef;ef = ef->next){
		edgecount = 0;
		facetype = 3;
		if(ef->e1->f & flag) {edgecount++;}
		if(ef->e2->f & flag) {edgecount++;}
		if(ef->e3->f & flag) {edgecount++;}
		if(ef->v4){
			facetype = 4;
			if(ef->e4->f & flag){edgecount++;}
		}  
		if(facetype == 4){
			switch(edgecount){
				case 0: break;
				case 1: ef->f1 = SELECT;
					fill_quad_single(ef, gh, numcuts, seltype);
					break;   
				case 2: ef->f1 = SELECT;
					// if there are 2, we check if edge 1 and 3 are either both on or off that way
					// we can tell if the selected pair is Adjacent or Opposite of each other
					if((ef->e1->f & flag && ef->e3->f & flag) || 
					   (ef->e2->f & flag && ef->e4->f & flag)){
						fill_quad_double_op(ef, gh, numcuts);							  
					}else{
						switch(G.scene->toolsettings->cornertype){
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
					fill_quad_quadruple(ef, gh, numcuts,rad,beauty); 
					break;	
			}
		} else {
			switch(edgecount){
				case 0: break;
				case 1: ef->f1 = SELECT;
					fill_tri_single(ef, gh, numcuts, seltype);
					break;   
				case 2: ef->f1 = SELECT;
					fill_tri_double(ef, gh, numcuts);
					break;	
				case 3: ef->f1 = SELECT;
					fill_tri_triple(ef, gh, numcuts,rad,beauty);
					break;  
			}	
		}	
	}
	
	// Delete Old Faces
	free_tagged_facelist(em->faces.first);	 
	//Delete Old Edges
	for(eed = em->edges.first;eed;eed = eed->next){
		if(BLI_ghash_haskey(gh,eed)){
			eed->f1 = SELECT; 
		} else {
			eed->f1 = 0;   
		}
	} 
	free_tagged_edgelist(em->edges.first); 
	
	if(seltype == SUBDIV_SELECT_ORIG  && G.qual != LR_CTRLKEY){
		for(eed = em->edges.first;eed;eed = eed->next){
			if(eed->f2 & EDGENEW){
				eed->f |= flag;
				EM_select_edge(eed,1); 
				
			}else{
				eed->f &= !flag;
				EM_select_edge(eed,0); 
			}
		}   
	} else if ((seltype == SUBDIV_SELECT_INNER || seltype == SUBDIV_SELECT_INNER_SEL)|| G.qual == LR_CTRLKEY){
		for(eed = em->edges.first;eed;eed = eed->next){
			if(eed->f2 & EDGEINNER){
				eed->f |= flag;
				EM_select_edge(eed,1);   
			}else{
				eed->f &= !flag;
				EM_select_edge(eed,0); 
			}
		}		  
	} 
	 if(G.scene->selectmode & SCE_SELECT_VERTEX){
		 for(eed = em->edges.first;eed;eed = eed->next){
			if(eed->f & SELECT){
				eed->v1->f |= SELECT;
				eed->v2->f |= SELECT;
			}
		}	
	}
	// Free the ghash and call MEM_freeN on all the value entries to return 
	// that memory
	BLI_ghash_free(gh, NULL, (GHashValFreeFP)MEM_freeN);   
	
	EM_selectmode_flush();
	for(ef=em->faces.first;ef;ef = ef->next){
		if(ef->e4){
			if(  (ef->e1->f & SELECT && ef->e2->f & SELECT) &&
			 (ef->e3->f & SELECT && ef->e4->f & SELECT) ){
				ef->f |= SELECT;			 
			}				   
		} else {
			if(  (ef->e1->f & SELECT && ef->e2->f & SELECT) && ef->e3->f & SELECT){
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
		ed->vn= 0;
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
			eed->vn= (EditVert *) (&efaa[i]);
			i++;
		}
		else eed->vn= NULL;
		
		eed= eed->next;
	}
		
	
	/* find edges pointing to 2 faces by procedure:
	
	- run through faces and their edges, increase
	  face counter e->f1 for each face 
	*/

	while(efa) {
		efa->f1= 0;
		if(efa->v4==0) {  /* if triangle */
			if(efa->f & SELECT) {
				
				e1= efa->e1;
				e2= efa->e2;
				e3= efa->e3;
				if(e1->f2<3 && e1->vn) {
					if(e1->f2<2) {
						evp= (EVPtr *) e1->vn;
						evp[(int)e1->f2]= efa;
					}
					e1->f2+= 1;
				}
				if(e2->f2<3 && e2->vn) {
					if(e2->f2<2) {
						evp= (EVPtr *) e2->vn;
						evp[(int)e2->f2]= efa;
					}
					e2->f2+= 1;
				}
				if(e3->f2<3 && e3->vn) {
					if(e3->f2<2) {
						evp= (EVPtr *) e3->vn;
						evp[(int)e3->f2]= efa;
					}
					e3->f2+= 1;
				}
			}
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

static void givequadverts(EditFace *efa, EditFace *efa1, EditVert **v1, EditVert **v2, EditVert **v3, EditVert **v4, float **uv, unsigned int *col)
{
	if VTEST(efa, 1, efa1) {
	//if(efa->v1!=efa1->v1 && efa->v1!=efa1->v2 && efa->v1!=efa1->v3) {
		*v1= efa->v1;
		*v2= efa->v2;
		uv[0] = efa->tf.uv[0];
		uv[1] = efa->tf.uv[1];
		col[0] = efa->tf.col[0];
		col[1] = efa->tf.col[1];
	}
	else if VTEST(efa, 2, efa1) {
	//else if(efa->v2!=efa1->v1 && efa->v2!=efa1->v2 && efa->v2!=efa1->v3) {
		*v1= efa->v2;
		*v2= efa->v3;
		uv[0] = efa->tf.uv[1];
		uv[1] = efa->tf.uv[2];
		col[0] = efa->tf.col[1];
		col[1] = efa->tf.col[2];
	}
	else if VTEST(efa, 3, efa1) {
	// else if(efa->v3!=efa1->v1 && efa->v3!=efa1->v2 && efa->v3!=efa1->v3) {
		*v1= efa->v3;
		*v2= efa->v1;
		uv[0] = efa->tf.uv[2];
		uv[1] = efa->tf.uv[0];
		col[0] = efa->tf.col[2];
		col[1] = efa->tf.col[0];
	}
	
	if VTEST(efa1, 1, efa) {
	// if(efa1->v1!=efa->v1 && efa1->v1!=efa->v2 && efa1->v1!=efa->v3) {
		*v3= efa1->v1;
		uv[2] = efa1->tf.uv[0];
		col[2] = efa1->tf.col[0];

		*v4= efa1->v2;
		uv[3] = efa1->tf.uv[1];
		col[3] = efa1->tf.col[1];
/*
if(efa1->v2== *v2) {
			*v4= efa1->v3;
			uv[3] = efa1->tf.uv[2];
		} else {
			*v4= efa1->v2;
			uv[3] = efa1->tf.uv[1];
		}	
		*/
	}
	else if VTEST(efa1, 2, efa) {
	// else if(efa1->v2!=efa->v1 && efa1->v2!=efa->v2 && efa1->v2!=efa->v3) {
		*v3= efa1->v2;
		uv[2] = efa1->tf.uv[1];
		col[2] = efa1->tf.col[1];

		*v4= efa1->v3;
		uv[3] = efa1->tf.uv[2];
		col[3] = efa1->tf.col[2];
/*
if(efa1->v3== *v2) {
			*v4= efa1->v1;
			uv[3] = efa1->tf.uv[0];
		} else {	
			*v4= efa1->v3;
			uv[3] = efa1->tf.uv[2];
		}	
		*/
	}
	else if VTEST(efa1, 3, efa) {
	// else if(efa1->v3!=efa->v1 && efa1->v3!=efa->v2 && efa1->v3!=efa->v3) {
		*v3= efa1->v3;
		uv[2] = efa1->tf.uv[2];
		col[2] = efa1->tf.col[2];

		*v4= efa1->v1;
		uv[3] = efa1->tf.uv[0];
		col[3] = efa1->tf.col[0];
/*
if(efa1->v1== *v2) {
			*v4= efa1->v2;
			uv[3] = efa1->tf.uv[3];
		} else {	
			*v4= efa1->v1;
			uv[3] = efa1->tf.uv[0];
		}	
		*/
	}
	else {
		*v3= *v4= NULL;
		
		return;
	}
	
}

/* Helper functions for edge/quad edit features*/

static void untag_edges(EditFace *f)
{
	f->e1->f2 = 0;
	f->e2->f2 = 0;
	if (f->e3) f->e3->f2 = 0;
	if (f->e4) f->e4->f2 = 0;
}

/** remove and free list of tagged edges */
static void free_tagged_edgelist(EditEdge *eed)
{
	EditEdge *nexted;

	while(eed) {
		nexted= eed->next;
		if(eed->f1) {
			remedge(eed);
			free_editedge(eed);
		}
		eed= nexted;
	}	
}	
/** remove and free list of tagged faces */

static void free_tagged_facelist(EditFace *efa)
{	
	EditMesh *em = G.editMesh;
	EditFace *nextvl;

	while(efa) {
		nextvl= efa->next;
		if(efa->f1) {
			BLI_remlink(&em->faces, efa);
			free_editface(efa);
		}
		efa= nextvl;
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
	float *uv[4];
	unsigned int col[4];
	float len1, len2, len3, len4, len5, len6, opp1, opp2, fac1, fac2;
	int totedge, ok, notbeauty=8, onedone;

	/* - all selected edges with two faces
		* - find the faces: store them in edges (using datablock)
		* - per edge: - test convex
		*			   - test edge: flip?
		*			   - if true: remedge,  addedge, all edges at the edge get new face pointers
		*/
	
	EM_selectmode_set();	// makes sure in selectmode 'face' the edges of selected faces are selected too 

	totedge = count_selected_edges(em->edges.first);
	if(totedge==0) return;

	if(okee("Beautify fill")==0) return;
	
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

				efaa = (EVPtr *) eed->vn;

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
					givequadverts(efaa[0], efaa[1], &v1, &v2, &v3, &v4, uv, col);
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

									w= addfacelist(v1, v2, v3, 0, efa, NULL);
									w->f |= SELECT;
									
									UVCOPY(w->tf.uv[0], uv[0]);
									UVCOPY(w->tf.uv[1], uv[1]);
									UVCOPY(w->tf.uv[2], uv[2]);

									w->tf.col[0] = col[0]; w->tf.col[1] = col[1]; w->tf.col[2] = col[2];
									w= addfacelist(v1, v3, v4, 0, efa, NULL);
									w->f |= SELECT;

									UVCOPY(w->tf.uv[0], uv[0]);
									UVCOPY(w->tf.uv[1], uv[2]);
									UVCOPY(w->tf.uv[2], uv[3]);

									w->tf.col[0] = col[0]; w->tf.col[1] = col[2]; w->tf.col[2] = col[3];

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

									w= addfacelist(v2, v3, v4, 0, efa, NULL);
									w->f |= SELECT;

									UVCOPY(w->tf.uv[0], uv[1]);
									UVCOPY(w->tf.uv[1], uv[3]);
									UVCOPY(w->tf.uv[2], uv[4]);

									w= addfacelist(v1, v2, v4, 0, efa, NULL);
									w->f |= SELECT;

									UVCOPY(w->tf.uv[0], uv[0]);
									UVCOPY(w->tf.uv[1], uv[1]);
									UVCOPY(w->tf.uv[2], uv[3]);

									onedone= 1;
								}
							}
						}
					}
				}

			}
			eed= nexted;
		}

		free_tagged_edgelist(em->edges.first);
		free_tagged_facelist(em->faces.first);

		if(onedone==0) break;
		
		EM_selectmode_set();	// new edges/faces were added
	}

	MEM_freeN(efaar);

	EM_select_flush();
	
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	BIF_undo_push("Beauty Fill");
}


/* ******************** FLIP EDGE ************************************* */


#define FACE_MARKCLEAR(f) (f->f1 = 1)

void join_triangles(void)
{
	EditMesh *em = G.editMesh;
	EditVert *v1, *v2, *v3, *v4;
	EditFace *efa, *w;
	EVPTuple *efaar;
	EVPtr *efaa;
	EditEdge *eed, *nexted;
	int totedge, ok;
	float *uv[4];
	unsigned int col[4];

	EM_selectmode_flush();	// makes sure in selectmode 'face' the edges of selected faces are selected too 
	
	totedge = count_selected_edges(em->edges.first);
	if(totedge==0) return;

	efaar= (EVPTuple *) MEM_callocN(totedge * sizeof(EVPTuple), "jointris");

	ok = collect_quadedges(efaar, em->edges.first, em->faces.first);
	if (G.f & G_DEBUG) {
		printf("Edges selected: %d\n", ok);
	}	

	eed= em->edges.first;
	while(eed) {
		nexted= eed->next;
		
		if(eed->f2==2) {  /* points to 2 faces */
			
			efaa= (EVPtr *) eed->vn;
			
			/* don't do it if flagged */

			ok= 1;
			efa= efaa[0];
			if(efa->e1->f1 || efa->e2->f1 || efa->e3->f1) ok= 0;
			efa= efaa[1];
			if(efa->e1->f1 || efa->e2->f1 || efa->e3->f1) ok= 0;
			
			if(ok) {
				/* test convex */
				givequadverts(efaa[0], efaa[1], &v1, &v2, &v3, &v4, uv, col);

/*
		4-----3		4-----3
		|\	|		|	 |
		| \ 1 |		|	 |
		|  \  |  ->	|	 |	
		| 0 \ |		|	 | 
		|	\|		|	 |
		1-----2		1-----2
*/
				/* make new faces */
				if(v1 && v2 && v3 && v4) {
					if( convex(v1->co, v2->co, v3->co, v4->co) ) {
						if(exist_face(v1, v2, v3, v4)==0) {
							w = addfacelist(v1, v2, v3, v4, efaa[0], NULL); /* seam edge may get broken */
							w->f= efaa[0]->f;	/* copy selection flag */
							untag_edges(w);

							UVCOPY(w->tf.uv[0], uv[0]);
							UVCOPY(w->tf.uv[1], uv[1]);
							UVCOPY(w->tf.uv[2], uv[2]);
							UVCOPY(w->tf.uv[3], uv[3]);

							memcpy(w->tf.col, col, sizeof(w->tf.col));
						}
						/* tag as to-be-removed */
						FACE_MARKCLEAR(efaa[0]);
						FACE_MARKCLEAR(efaa[1]);
						eed->f1 = 1; 
					} /* endif test convex */
				}
			}
		}
		eed= nexted;
	}
	free_tagged_edgelist(em->edges.first);
	free_tagged_facelist(em->faces.first);

	MEM_freeN(efaar);
	
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	BIF_undo_push("Convert Triangles to Quads");
}

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

	float *uv[4];
	unsigned int col[4];

	int totedge, ok;
	
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
			
			efaa= (EVPtr *) eed->vn;
			
			/* don't do it if flagged */

			ok= 1;
			efa= efaa[0];
			if(efa->e1->f1 || efa->e2->f1 || efa->e3->f1) ok= 0;
			efa= efaa[1];
			if(efa->e1->f1 || efa->e2->f1 || efa->e3->f1) ok= 0;
			
			if(ok) {
				/* test convex */
				givequadverts(efaa[0], efaa[1], &v1, &v2, &v3, &v4, uv, col);

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
				if (v1 && v2 && v3){
					if( convex(v1->co, v2->co, v3->co, v4->co) ) {
						if(exist_face(v1, v2, v3, v4)==0) {
							w = addfacelist(v1, v2, v3, 0, efaa[1], NULL); /* outch this may break seams */ 
							EM_select_face(w, 1);
							untag_edges(w);

							UVCOPY(w->tf.uv[0], uv[0]);
							UVCOPY(w->tf.uv[1], uv[1]);
							UVCOPY(w->tf.uv[2], uv[2]);

							w->tf.col[0] = col[0]; w->tf.col[1] = col[1]; w->tf.col[2] = col[2]; 
							
							w = addfacelist(v1, v3, v4, 0, efaa[1], NULL); /* outch this may break seams */
							EM_select_face(w, 1);
							untag_edges(w);

							UVCOPY(w->tf.uv[0], uv[0]);
							UVCOPY(w->tf.uv[1], uv[2]);
							UVCOPY(w->tf.uv[2], uv[3]);

							w->tf.col[0] = col[0]; w->tf.col[1] = col[2]; w->tf.col[2] = col[3]; 
							
							/* erase old faces and edge */
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
	free_tagged_edgelist(em->edges.first);
	free_tagged_facelist(em->faces.first);
	
	MEM_freeN(efaar);
	
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	BIF_undo_push("Flip Triangle Edges");
	
}

static void edge_rotate(EditEdge *eed,int dir)
{
	EditMesh *em = G.editMesh;
	EditFace *face[2], *efa, *newFace[2];
	EditVert *faces[2][4],*v1,*v2,*v3,*v4,*vtemp;
	EditEdge *srchedge = NULL;
	short facecount=0, p1=0,p2=0,p3=0,p4=0,fac1=4,fac2=4,i,j,numhidden;
	EditEdge **hiddenedges;
	
	/* check to make sure that the edge is only part of 2 faces */
	for(efa = em->faces.first;efa;efa = efa->next){
		if((efa->e1 == eed || efa->e2 == eed) || (efa->e3 == eed || efa->e4 == eed)){
			if(facecount == 2){
				return;
			}
			if(facecount < 2)
				face[facecount] = efa;
			facecount++;
		}
	}
 
	if(facecount < 2){
		return;
	}

	/* how many edges does each face have */
 	if(face[0]->e4 == NULL)
		fac1=3;
	else
		fac1=4;
	if(face[1]->e4 == NULL)
		fac2=3;
	else
		fac2=4;
	
	/*store the face info in a handy array */			
	faces[0][0] =  face[0]->v1;
	faces[0][1] =  face[0]->v2;
	faces[0][2] =  face[0]->v3;
	if(face[0]->e4 != NULL)
		faces[0][3] =  face[0]->v4;
	else
		faces[0][3] = NULL;
			
	faces[1][0] =  face[1]->v1;
	faces[1][1] =  face[1]->v2;
	faces[1][2] =  face[1]->v3;
	if(face[1]->e4 != NULL)
		faces[1][3] =  face[1]->v4;
	else
		faces[1][3] = NULL;
	

	/* we don't want to rotate edges between faces that share more than one edge */
	
	j=0;
	if(face[0]->e1 == face[1]->e1 ||
	   face[0]->e1 == face[1]->e2 ||
	   face[0]->e1 == face[1]->e3 ||
	   ((face[1]->e4) && face[0]->e1 == face[1]->e4) )
	   j++;
	   
	if(face[0]->e2 == face[1]->e1 ||
	   face[0]->e2 == face[1]->e2 ||
	   face[0]->e2 == face[1]->e3 ||
	   ((face[1]->e4) && face[0]->e2 == face[1]->e4) )
	   j++;
	   
	if(face[0]->e3 == face[1]->e1 ||
	   face[0]->e3 == face[1]->e2 ||
	   face[0]->e3 == face[1]->e3 ||
	   ((face[1]->e4) && face[0]->e3 == face[1]->e4) )
	   j++;	   

	if(face[0]->e4){
		if(face[0]->e4 == face[1]->e1 ||
		   face[0]->e4 == face[1]->e2 ||
		   face[0]->e4 == face[1]->e3 ||
		   ((face[1]->e4) && face[0]->e4 == face[1]->e4) )
			   j++;	
	 }	   	   
	if(j > 1){
		return;
	}
	
	/* Coplaner Faces Only Please */
	if(Inpf(face[0]->n,face[1]->n) <= 0.000001){	
		return;
	}
	
	/*get the edges verts */
	v1 = eed->v1;
	v2 = eed->v2;
	v3 = eed->v1;
	v4 = eed->v2;

	/*figure out where the edges verts lie one the 2 faces */
	for(i=0;i<4;i++){
		if(v1 == faces[0][i])
			p1 = i;
		if(v2 == faces[0][i])
			p2 = i;
		if(v1 == faces[1][i])
			p3 = i;
		if(v2 == faces[1][i])
			p4 = i;
	}
	
	/*make sure the verts are in the correct order */
	if((p1+1)%fac1 == p2){
		vtemp = v2;
		v2 = v1;
		v1 = vtemp;
		
		i = p1;
		p1 = p2;
		p2 = i;
	}
	if((p3+1)%fac2 == p4){
		vtemp = v4;
		v4 = v3;
		v3 = vtemp;
		
		i = p3;
		p3 = p4;
		p4 = i;	
	}	


	/* Create an Array of the Edges who have h set prior to rotate */
	numhidden = 0;
	for(srchedge = em->edges.first;srchedge;srchedge = srchedge->next){
		if(srchedge->h && (srchedge->v1->f & SELECT || srchedge->v2->f & SELECT)){
			numhidden++;
		}
	}
	hiddenedges = MEM_mallocN(sizeof(EditVert*)*numhidden+1,"Hidden Vert Scratch Array for Rotate Edges");
	if(!hiddenedges){
        error("Malloc Was not happy!");
        return;   
    }
    numhidden = 0;
	for(srchedge = em->edges.first;srchedge;srchedge = srchedge->next){
		if(srchedge->h && (srchedge->v1->f & SELECT || srchedge->v2->f & SELECT)){
			hiddenedges[numhidden] = srchedge;
			numhidden++;
		}
	}	
							
	/* create the 2 new faces */   								
	if(fac1 == 3 && fac2 == 3){
		/*No need of reverse setup*/
		newFace[0] = addfacelist(faces[0][(p1+1 )%3],faces[0][(p1+2 )%3],faces[1][(p3+1 )%3],NULL,NULL,NULL);
		newFace[1] = addfacelist(faces[1][(p3+1 )%3],faces[1][(p3+2 )%3],faces[0][(p1+1 )%3],NULL,NULL,NULL);
	
		newFace[0]->tf.col[0] = face[0]->tf.col[(p1+1 )%3];
		newFace[0]->tf.col[1] = face[0]->tf.col[(p1+2 )%3];
		newFace[0]->tf.col[2] = face[1]->tf.col[(p3+1 )%3];
		newFace[1]->tf.col[0] = face[1]->tf.col[(p3+1 )%3];
		newFace[1]->tf.col[1] = face[1]->tf.col[(p3+2 )%3];
		newFace[1]->tf.col[2] =	face[0]->tf.col[(p1+1 )%3];
	
		UVCOPY(newFace[0]->tf.uv[0],face[0]->tf.uv[(p1+1 )%3]);
		UVCOPY(newFace[0]->tf.uv[1],face[0]->tf.uv[(p1+2 )%3]);	
		UVCOPY(newFace[0]->tf.uv[2],face[1]->tf.uv[(p3+1 )%3]);
		UVCOPY(newFace[1]->tf.uv[0],face[1]->tf.uv[(p3+1 )%3]);
		UVCOPY(newFace[1]->tf.uv[1],face[1]->tf.uv[(p3+2 )%3]);
		UVCOPY(newFace[1]->tf.uv[2],face[0]->tf.uv[(p1+1 )%3]);
	}
	else if(fac1 == 4 && fac2 == 3){
		if(dir == 1){
			newFace[0] = addfacelist(faces[0][(p1+1 )%4],faces[0][(p1+2 )%4],faces[0][(p1+3 )%4],faces[1][(p3+1 )%3],NULL,NULL);
			newFace[1] = addfacelist(faces[1][(p3+1 )%3],faces[1][(p3+2 )%3],faces[0][(p1+1 )%4],NULL,NULL,NULL);
	
			newFace[0]->tf.col[0] = face[0]->tf.col[(p1+1 )%4];
			newFace[0]->tf.col[1] = face[0]->tf.col[(p1+2 )%4];
			newFace[0]->tf.col[2] = face[0]->tf.col[(p1+3 )%4];
			newFace[0]->tf.col[3] = face[1]->tf.col[(p3+1 )%3];
			newFace[1]->tf.col[0] = face[1]->tf.col[(p3+1 )%3];
			newFace[1]->tf.col[1] = face[1]->tf.col[(p3+2 )%3];
			newFace[1]->tf.col[2] =	face[0]->tf.col[(p1+1 )%4];
	
			UVCOPY(newFace[0]->tf.uv[0],face[0]->tf.uv[(p1+1 )%4]);
			UVCOPY(newFace[0]->tf.uv[1],face[0]->tf.uv[(p1+2 )%4]);	
			UVCOPY(newFace[0]->tf.uv[2],face[0]->tf.uv[(p1+3 )%4]);		
			UVCOPY(newFace[0]->tf.uv[3],face[1]->tf.uv[(p3+1 )%3]);
			UVCOPY(newFace[1]->tf.uv[0],face[1]->tf.uv[(p3+1 )%3]);
			UVCOPY(newFace[1]->tf.uv[1],face[1]->tf.uv[(p3+2 )%3]);
			UVCOPY(newFace[1]->tf.uv[2],face[0]->tf.uv[(p1+1 )%4]);	
		} else if (dir == 2){
			newFace[0] = addfacelist(faces[0][(p1+2 )%4],faces[1][(p3+1)%3],faces[0][(p1)%4],faces[0][(p1+1 )%4],NULL,NULL);
			newFace[1] = addfacelist(faces[0][(p1+2 )%4],faces[1][(p3)%3],faces[1][(p3+1 )%3],NULL,NULL,NULL);

			newFace[0]->tf.col[0] = face[0]->tf.col[(p1+2)%4];
			newFace[0]->tf.col[1] = face[1]->tf.col[(p3+1)%3];
			newFace[0]->tf.col[2] = face[0]->tf.col[(p1  )%4];
			newFace[0]->tf.col[3] = face[0]->tf.col[(p1+1)%4];
			newFace[1]->tf.col[0] = face[0]->tf.col[(p1+2)%4];
			newFace[1]->tf.col[1] = face[1]->tf.col[(p3  )%3];
			newFace[1]->tf.col[2] =	face[1]->tf.col[(p3+1)%3];
	
			UVCOPY(newFace[0]->tf.uv[0],face[0]->tf.uv[(p1+2)%4]);
			UVCOPY(newFace[0]->tf.uv[1],face[1]->tf.uv[(p3+1)%3]);	
			UVCOPY(newFace[0]->tf.uv[2],face[0]->tf.uv[(p1  )%4]);		
			UVCOPY(newFace[0]->tf.uv[3],face[0]->tf.uv[(p1+1)%4]);
			UVCOPY(newFace[1]->tf.uv[0],face[0]->tf.uv[(p1+2)%4]);
			UVCOPY(newFace[1]->tf.uv[1],face[1]->tf.uv[(p3  )%3]);
			UVCOPY(newFace[1]->tf.uv[2],face[1]->tf.uv[(p3+1)%3]);	
			
			faces[0][(p1+2)%fac1]->f |= SELECT;
			faces[1][(p3+1)%fac2]->f |= SELECT;		
		}
	}

	else if(fac1 == 3 && fac2 == 4){
		if(dir == 1){
			newFace[0] = addfacelist(faces[0][(p1+1 )%3],faces[0][(p1+2 )%3],faces[1][(p3+1 )%4],NULL,NULL,NULL);
			newFace[1] = addfacelist(faces[1][(p3+1 )%4],faces[1][(p3+2 )%4],faces[1][(p3+3 )%4],faces[0][(p1+1 )%3],NULL,NULL);
	
			newFace[0]->tf.col[0] = face[0]->tf.col[(p1+1 )%3];
			newFace[0]->tf.col[1] = face[0]->tf.col[(p1+2 )%3];
			newFace[0]->tf.col[2] = face[1]->tf.col[(p3+1 )%4];
			newFace[1]->tf.col[0] = face[1]->tf.col[(p3+1 )%4];
			newFace[1]->tf.col[1] = face[1]->tf.col[(p3+2 )%4];
			newFace[1]->tf.col[2] = face[1]->tf.col[(p3+3 )%4];		
			newFace[1]->tf.col[3] =	face[0]->tf.col[(p1+1 )%3];
	
			UVCOPY(newFace[0]->tf.uv[0],face[0]->tf.uv[(p1+1 )%3]);
			UVCOPY(newFace[0]->tf.uv[1],face[0]->tf.uv[(p1+2 )%3]);	
			UVCOPY(newFace[0]->tf.uv[2],face[1]->tf.uv[(p3+1 )%4]);
			UVCOPY(newFace[1]->tf.uv[0],face[1]->tf.uv[(p3+1 )%4]);
			UVCOPY(newFace[1]->tf.uv[1],face[1]->tf.uv[(p3+2 )%4]);
			UVCOPY(newFace[1]->tf.uv[2],face[1]->tf.uv[(p3+3 )%4]);
			UVCOPY(newFace[1]->tf.uv[3],face[0]->tf.uv[(p1+1 )%3]);
		} else if (dir == 2){
			newFace[0] = addfacelist(faces[0][(p1)%3],faces[0][(p1+1 )%3],faces[1][(p3+2 )%4],NULL,NULL,NULL);
			newFace[1] = addfacelist(faces[1][(p3+1 )%4],faces[1][(p3+2 )%4],faces[0][(p1+1 )%3],faces[0][(p1+2 )%3],NULL,NULL);
	
			newFace[0]->tf.col[0] = face[0]->tf.col[(p1 )%3];
			newFace[0]->tf.col[1] = face[0]->tf.col[(p1+1 )%3];
			newFace[0]->tf.col[2] = face[1]->tf.col[(p3+2 )%4];
			newFace[1]->tf.col[0] = face[1]->tf.col[(p3+1 )%4];
			newFace[1]->tf.col[1] = face[1]->tf.col[(p3+2 )%4];
			newFace[1]->tf.col[2] = face[0]->tf.col[(p1+1 )%3];		
			newFace[1]->tf.col[3] =	face[0]->tf.col[(p1+2 )%3];
	
			UVCOPY(newFace[0]->tf.uv[0],face[0]->tf.uv[(p1 )%3]);
			UVCOPY(newFace[0]->tf.uv[1],face[0]->tf.uv[(p1+1 )%3]);	
			UVCOPY(newFace[0]->tf.uv[2],face[1]->tf.uv[(p3+2 )%4]);
			UVCOPY(newFace[1]->tf.uv[0],face[1]->tf.uv[(p3+1 )%4]);
			UVCOPY(newFace[1]->tf.uv[1],face[1]->tf.uv[(p3+2 )%4]);
			UVCOPY(newFace[1]->tf.uv[2],face[0]->tf.uv[(p1+1 )%3]);
			UVCOPY(newFace[1]->tf.uv[3],face[0]->tf.uv[(p1+2 )%3]);	
			
			faces[0][(p1+1)%fac1]->f |= SELECT;
			faces[1][(p3+2)%fac2]->f |= SELECT;	
		}
	
	}
	
	else if(fac1 == 4 && fac2 == 4){
		if(dir == 1){
			newFace[0] = addfacelist(faces[0][(p1+1 )%4],faces[0][(p1+2 )%4],faces[0][(p1+3 )%4],faces[1][(p3+1 )%4],NULL,NULL);
			newFace[1] = addfacelist(faces[1][(p3+1 )%4],faces[1][(p3+2 )%4],faces[1][(p3+3 )%4],faces[0][(p1+1 )%4],NULL,NULL);
	
			newFace[0]->tf.col[0] = face[0]->tf.col[(p1+1 )%4];
			newFace[0]->tf.col[1] = face[0]->tf.col[(p1+2 )%4];
			newFace[0]->tf.col[2] = face[0]->tf.col[(p1+3 )%4];
			newFace[0]->tf.col[3] = face[1]->tf.col[(p3+1 )%4];
			newFace[1]->tf.col[0] = face[1]->tf.col[(p3+1 )%4];
			newFace[1]->tf.col[1] = face[1]->tf.col[(p3+2 )%4];
			newFace[1]->tf.col[2] = face[1]->tf.col[(p3+3 )%4];		
			newFace[1]->tf.col[3] =	face[0]->tf.col[(p1+1 )%4];
									
			UVCOPY(newFace[0]->tf.uv[0],face[0]->tf.uv[(p1+1 )%4]);
			UVCOPY(newFace[0]->tf.uv[1],face[0]->tf.uv[(p1+2 )%4]);	
			UVCOPY(newFace[0]->tf.uv[2],face[0]->tf.uv[(p1+3 )%4]);		
			UVCOPY(newFace[0]->tf.uv[3],face[1]->tf.uv[(p3+1 )%4]);
			UVCOPY(newFace[1]->tf.uv[0],face[1]->tf.uv[(p3+1 )%4]);
			UVCOPY(newFace[1]->tf.uv[1],face[1]->tf.uv[(p3+2 )%4]);
			UVCOPY(newFace[1]->tf.uv[2],face[1]->tf.uv[(p3+3 )%4]);
			UVCOPY(newFace[1]->tf.uv[3],face[0]->tf.uv[(p1+1 )%4]);		
		} else if (dir == 2){
			newFace[0] = addfacelist(faces[0][(p1+2 )%4],faces[0][(p1+3 )%4],faces[1][(p3+1 )%4],faces[1][(p3+2 )%4],NULL,NULL);
			newFace[1] = addfacelist(faces[1][(p3+2 )%4],faces[1][(p3+3 )%4],faces[0][(p1+1 )%4],faces[0][(p1+2 )%4],NULL,NULL);
	
			newFace[0]->tf.col[0] = face[0]->tf.col[(p1+2 )%4];
			newFace[0]->tf.col[1] = face[0]->tf.col[(p1+3 )%4];
			newFace[0]->tf.col[2] = face[1]->tf.col[(p3+1 )%4];
			newFace[0]->tf.col[3] = face[1]->tf.col[(p3+2 )%4];
			newFace[1]->tf.col[0] = face[1]->tf.col[(p3+2 )%4];
			newFace[1]->tf.col[1] = face[1]->tf.col[(p3+3 )%4];
			newFace[1]->tf.col[2] = face[0]->tf.col[(p1+1 )%4];		
			newFace[1]->tf.col[3] =	face[0]->tf.col[(p1+2 )%4];
									
			UVCOPY(newFace[0]->tf.uv[0],face[0]->tf.uv[(p1+2 )%4]);
			UVCOPY(newFace[0]->tf.uv[1],face[0]->tf.uv[(p1+3 )%4]);	
			UVCOPY(newFace[0]->tf.uv[2],face[1]->tf.uv[(p3+1 )%4]);		
			UVCOPY(newFace[0]->tf.uv[3],face[1]->tf.uv[(p3+2 )%4]);
			UVCOPY(newFace[1]->tf.uv[0],face[1]->tf.uv[(p3+2 )%4]);
			UVCOPY(newFace[1]->tf.uv[1],face[1]->tf.uv[(p3+3 )%4]);
			UVCOPY(newFace[1]->tf.uv[2],face[0]->tf.uv[(p1+1 )%4]);
			UVCOPY(newFace[1]->tf.uv[3],face[0]->tf.uv[(p1+2 )%4]);		
			
			faces[0][(p1+2)%fac1]->f |= SELECT;
			faces[1][(p3+2)%fac2]->f |= SELECT;	
		}
		

	}		
	else{
		/*This should never happen*/
		return;
	}

	if(dir == 1){
		faces[0][(p1+1)%fac1]->f |= SELECT;
		faces[1][(p3+1)%fac2]->f |= SELECT;
	}
	
	/*Copy old edge's flags to new center edge*/
	for(srchedge=em->edges.first;srchedge;srchedge=srchedge->next){
		if(srchedge->v1->f & SELECT &&srchedge->v2->f & SELECT  ){
			srchedge->f = eed->f;
			srchedge->h = eed->h;
			srchedge->dir = eed->dir;
			srchedge->seam = eed->seam;
			srchedge->crease = eed->crease;
		}
	}
	
	
	/* copy flags and material */
	
	newFace[0]->mat_nr	 = face[0]->mat_nr;
	newFace[0]->tf.flag	= face[0]->tf.flag;
	newFace[0]->tf.transp  = face[0]->tf.transp;
	newFace[0]->tf.mode	= face[0]->tf.mode;
	newFace[0]->tf.tile	= face[0]->tf.tile;
	newFace[0]->tf.unwrap  = face[0]->tf.unwrap;
	newFace[0]->tf.tpage   = face[0]->tf.tpage;
	newFace[0]->flag	   = face[0]->flag;

	newFace[1]->mat_nr	 = face[1]->mat_nr;
	newFace[1]->tf.flag	= face[1]->tf.flag;
	newFace[1]->tf.transp  = face[1]->tf.transp;
	newFace[1]->tf.mode	= face[1]->tf.mode;
	newFace[1]->tf.tile	= face[1]->tf.tile;
	newFace[1]->tf.unwrap  = face[1]->tf.unwrap;
	newFace[1]->tf.tpage   = face[1]->tf.tpage;
	newFace[1]->flag	   = face[1]->flag;
	
	/* Resetting Hidden Flag */
	for(numhidden--;numhidden>=0;numhidden--){
		hiddenedges[numhidden]->h = 1;
		   
	}
	
	/* check for orhphan edges */
	for(srchedge=em->edges.first;srchedge;srchedge = srchedge->next){
		srchedge->f1 = -1;   
	}
	
	/*for(efa = em->faces.first;efa;efa = efa->next){
		if(efa->h == 0){
			efa->e1->f1 = 1;   
			efa->e2->f1 = 1;   
			efa->e3->f1 = 1;   
			if(efa->e4){
				efa->e4->f1 = 1;   
			}
		}
		if(efa->h == 1){
			if(efa->e1->f1 == -1){
				efa->e1->f1 = 0; 
			}  
			if(efa->e2->f1 == -1){
				efa->e2->f1 = 0; 
			} 
						if(efa->e1->f1 == -1){
				efa->e1->f1 = 0; 
			}	
			if(efa->e4){
				efa->e4->f1 = 1;   
			}
		}		
	}
	 A Little Cleanup */
	MEM_freeN(hiddenedges);
	
	/* get rid of the old edge and faces*/
	remedge(eed);
	free_editedge(eed);	
	BLI_remlink(&em->faces, face[0]);
	free_editface(face[0]);	
	BLI_remlink(&em->faces, face[1]);
	free_editface(face[1]);		
	return;																																																														
}						

/* only accepts 1 selected edge, or 2 selected faces */
void edge_rotate_selected(int dir)
{
	EditEdge *eed;
	EditFace *efa;
	short edgeCount = 0;
	
	/*clear new flag for new edges, count selected edges */
	for(eed= G.editMesh->edges.first; eed; eed= eed->next){
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
	
	BIF_undo_push("Rotate Edge");	
}

/******************* BEVEL CODE STARTS HERE ********************/

static void bevel_displace_vec(float *midvec, float *v1, float *v2, float *v3, float d, float no[3])
{
	float a[3], c[3], n_a[3], n_c[3], mid[3], ac, ac2, fac;

	VecSubf(a, v1, v2);
	VecSubf(c, v3, v2);

	Crossf(n_a, a, no);
	Normalise(n_a);
	Crossf(n_c, no, c);
	Normalise(n_c);

	Normalise(a);
	Normalise(c);
	ac = Inpf(a, c);

	if (ac == 1 || ac == -1) {
		midvec[0] = midvec[1] = midvec[2] = 0;
		return;
	}
	ac2 = ac * ac;
	fac = (float)sqrt((ac2 + 2*ac + 1)/(1 - ac2) + 1);
	VecAddf(mid, n_c, n_a);
	Normalise(mid);
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
	l_a = Normalise(a);
	VecSubf(b, v4, v3);
	Normalise(b);
	VecSubf(c, v1, v2);
	Normalise(c);

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

	/* move edges of all faces with efa->f1 & flag closer towards their centres */
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

	/* move edges of all faces with efa->f1 & flag closer towards their centres */
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

	waitcursor(1);

	removedoublesflag(1, limit);

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
			v1= addvertlist(efa->v1->co);
			v1->f= efa->v1->f & ~128;
   			efa->v1->vn= v1;
#ifdef __NLA
   			v1->totweight = efa->v1->totweight;
   			if (efa->v1->totweight){
   				v1->dw = MEM_mallocN (efa->v1->totweight * sizeof(MDeformWeight), "deformWeight");
   				memcpy (v1->dw, efa->v1->dw, efa->v1->totweight * sizeof(MDeformWeight));
   			}
   			else
   				v1->dw=NULL;
#endif
			v1= addvertlist(efa->v2->co);
			v1->f= efa->v2->f & ~128;
   			efa->v2->vn= v1;
#ifdef __NLA
   			v1->totweight = efa->v2->totweight;
   			if (efa->v2->totweight){
   				v1->dw = MEM_mallocN (efa->v2->totweight * sizeof(MDeformWeight), "deformWeight");
   				memcpy (v1->dw, efa->v2->dw, efa->v2->totweight * sizeof(MDeformWeight));
   			}
   			else
   				v1->dw=NULL;
#endif
			v1= addvertlist(efa->v3->co);
			v1->f= efa->v3->f & ~128;
   			efa->v3->vn= v1;
#ifdef __NLA
   			v1->totweight = efa->v3->totweight;
   			if (efa->v3->totweight){
   				v1->dw = MEM_mallocN (efa->v3->totweight * sizeof(MDeformWeight), "deformWeight");
   				memcpy (v1->dw, efa->v3->dw, efa->v3->totweight * sizeof(MDeformWeight));
   			}
   			else
   				v1->dw=NULL;
#endif
			if (efa->v4) {
				v1= addvertlist(efa->v4->co);
				v1->f= efa->v4->f & ~128;
	   			efa->v4->vn= v1;
#ifdef __NLA
	   			v1->totweight = efa->v4->totweight;
	   			if (efa->v4->totweight){
	   				v1->dw = MEM_mallocN (efa->v4->totweight * sizeof(MDeformWeight), "deformWeight");
	   				memcpy (v1->dw, efa->v4->dw, efa->v4->totweight * sizeof(MDeformWeight));
	   			}
	   			else
	   				v1->dw=NULL;
#endif
			}

			/* Needs better adaption of creases? */
   			addedgelist(efa->e1->v1->vn, efa->e1->v2->vn, efa->e1);
   			addedgelist(efa->e2->v1->vn,efa->e2->v2->vn, efa->e2);   			
   			addedgelist(efa->e3->v1->vn,efa->e3->v2->vn, efa->e3);   			
   			if (efa->e4) addedgelist(efa->e4->v1->vn,efa->e4->v2->vn, efa->e4);  

   			if(efa->v4) {
				v1= efa->v1->vn;
				v2= efa->v2->vn;
				v3= efa->v3->vn;
				v4= efa->v4->vn;
				addfacelist(v1, v2, v3, v4, efa,NULL);
   			} else {
   				v1= efa->v1->vn;
   				v2= efa->v2->vn;
   				v3= efa->v3->vn;
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
		if ( ((eed->v1->f & eed->v2->f) & 1) || allfaces) eed->f1 |= 4;	/* original edges */
		eed->vn= 0;
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
		eed->vn= 0;
		eed= eed->next;
	}

#ifdef BEV_DEBUG
	fprintf(stderr,"bevel_mesh: find clusters\n");
#endif	

	/* Look for vertex clusters */

	eve= em->verts.first;
	while (eve) {
		eve->f &= ~(64|128);
		eve->vn= NULL;
		eve= eve->next;
	}
	
	/* eve->f: 128: first vertex in a list (->vn) */
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
						eve->vn= eve2;
						eve3= eve2;
					} else if ((eve->f & 64) == 0) {
						/* fprintf(stderr,"  *\n"); */
						if (eve3) eve3->vn= eve2;
						eve2->f |= 64;
						eve3= eve2;
					}
				}
			}
			eve2= eve2->next;
			if (!eve2) {
				if (eve3) eve3->vn= NULL;
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
			eve2= eve->vn;
			while (eve2) {
				a++;
				neweve[a]= eve2;
				eve2= eve2->vn;
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
					eve2= addvertlist(cent);
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
		eve->vn= NULL;
		eve= eve->next;
	}
	
	recalc_editnormals();
	waitcursor(0);
	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	
	removedoublesflag(1, limit);

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

void bevel_menu()
{
	char Finished = 0, Canceled = 0, str[100], Recalc = 0;
	short mval[2], oval[2], curval[2], event = 0, recurs = 1, nr;
	float vec[3], d, drawd=0.0, centre[3], fac = 1;

	getmouseco_areawin(mval);
	oval[0] = mval[0]; oval[1] = mval[1];

	// Silly hackish code to initialise the variable (warning if not done)
	// while still drawing in the first iteration (and without using another variable)
	curval[0] = mval[0] + 1; curval[1] = mval[1] + 1;

	window_to_3d(centre, mval[0], mval[1]);

	if(button(&recurs, 1, 4, "Recursion:")==0) return;

	for (nr=0; nr<recurs-1; nr++) {
		if (nr==0) fac += 1.0f/3.0f; else fac += 1.0f/(3 * nr * 2.0f);
	}

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
			d = Normalise(vec) / 10;


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
			glFlush(); // flush display for frontbuffer
			glDrawBuffer(GL_BACK);
		}
		while(qtest()) {
			unsigned short val=0;			
			event= extern_qread(&val);	// extern_qread stores important events for the mainloop to handle 

			/* val==0 on key-release event */
			if(val && (event==ESCKEY || event==RIGHTMOUSE || event==LEFTMOUSE || event==RETKEY || event==ESCKEY)){
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

void EdgeLoopDelete(void) {
	EdgeSlide(1, 1);
	select_more();
	removedoublesflag(1,0.001);
	EM_select_flush();
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
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
	float perc = 0, percp = 0,vertdist, projectMat[4][4], viewMat[4][4], len;
	int i = 0,j, numsel, numadded=0, timesthrough = 0, vertsel=0, prop=1, cancel = 0,flip=0;
	short event, draw=1;
	short mval[2], mvalo[2];
	char str[128]; 
	
	view3d_get_object_project_mat(curarea, G.obedit, projectMat, viewMat);
	
	mvalo[0] = -1; mvalo[1] = -1; 
	numsel =0;  
	
	// Get number of selected edges and clear some flags
	for(eed=em->edges.first;eed;eed=eed->next){
		eed->f1 = 0;
		eed->f2 = 0;   
		if(eed->f & SELECT) numsel++;
	}
	
	for(ev=em->verts.first;ev;ev=ev->next){
		ev->f1 = 0;   
	} 
	
	//Make sure each edge only has 2 faces
	// make sure loop doesn't cross face
	for(efa=em->faces.first;efa;efa=efa->next){
		int ct = 0;
		if(efa->e1->f & SELECT){
			ct++;
			efa->e1->f1++;
			if(efa->e1->f1 > 2){
				error("3+ face edge");
				return 0;				 
			}
		}
		if(efa->e2->f & SELECT){
			ct++;
			efa->e2->f1++;
			if(efa->e2->f1 > 2){
				error("3+ face edge");
				return 0;				 
			}
		}
		if(efa->e3->f & SELECT){
			ct++;
			efa->e3->f1++;
			if(efa->e3->f1 > 2){
				error("3+ face edge");
				return 0;				 
			}
		}
		if(efa->e4 && efa->e4->f & SELECT){
			ct++;
			efa->e4->f1++;
			if(efa->e4->f1 > 2){
				error("3+ face edge");
				return 0;				 
			}
		}	
		// Make sure loop is not 2 edges of same face	
		if(ct > 1){
		   error("loop crosses itself");
		   return 0;   
		}
	}	   
	// Get # of selected verts
	for(ev=em->verts.first;ev;ev=ev->next){ 
		if(ev->f & SELECT) vertsel++;
	}	
	   
	// Test for multiple segments
	if(vertsel > numsel+1){
		error("Was not a single edge loop");
		return 0;		   
	}  
	
	// Get the edgeloop in order - mark f1 with SELECT once added
	for(eed=em->edges.first;eed;eed=eed->next){
		if((eed->f & SELECT) && !(eed->f1 & SELECT)){
			// If this is the first edge added, just put it in
			if(!edgelist){
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
		if(eed->next == NULL && numadded != numsel){
			eed=em->edges.first;	
			timesthrough++;
		}
		
		// It looks like there was an unexpected case - Hopefully should not happen
		if(timesthrough >= numsel*2){
			BLI_linklist_free(edgelist,NULL); 
			error("could not order loop");
			return 0;   
		}
	}
	
	// Put the verts in order in a linklist
	look = edgelist;
	while(look){
		eed = look->link;
		if(!vertlist){
			if(look->next){
				temp = look->next->link;

				//This is the first entry takes care of extra vert
				if(eed->v1 != temp->v1 && eed->v1 != temp->v2){
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
		if(eed->v1->f1 != 1){
			BLI_linklist_append(&vertlist,eed->v1); 
			eed->v1->f1 = 1;		   
		} else  if(eed->v2->f1 != 1){
			BLI_linklist_append(&vertlist,eed->v2); 
			eed->v2->f1 = 1;					
		} 
		look = look->next;   
	}		 
	
	// populate the SlideVerts
	
	vertgh = BLI_ghash_new(BLI_ghashutil_ptrhash, BLI_ghashutil_ptrcmp); 
	look = vertlist;	  
	while(look){
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
		
		for(eed=em->edges.first;eed;eed=eed->next){
			if(eed->v1 == ev || eed->v2 == ev){
				i++;	
				if(eed->f & SELECT){
					 j++;   
				}
			}		
		}
		// If the vert is in the middle of an edge loop, it touches 2 selected edges and 2 unselected edges
		if(i == 4 && j == 2){
			for(eed=em->edges.first;eed;eed=eed->next){
				if(editedge_containsVert(eed, ev)){
					if(!(eed->f & SELECT)){
						 if(!tempsv->up){
							 tempsv->up = eed;
						 } else if (!(tempsv->down)){
							 tempsv->down = eed;  
						 }
					}
				}		
			}			
		}
		// If it is on the end of the loop, it touches 1 selected and as least 2 more unselected
		if(i >= 3 && j == 1){
			for(eed=em->edges.first;eed;eed=eed->next){
				if(editedge_containsVert(eed, ev) && eed->f & SELECT){
					for(efa = em->faces.first;efa;efa=efa->next){
						if(editface_containsEdge(efa, eed)){
							if(editedge_containsVert(efa->e1, ev) && efa->e1 != eed){
								 if(!tempsv->up){
									 tempsv->up = efa->e1;
								 } else if (!(tempsv->down)){
									 tempsv->down = efa->e1;  
								 }								   
							}
							if(editedge_containsVert(efa->e2, ev) && efa->e2 != eed){
								 if(!tempsv->up){
									 tempsv->up = efa->e2;
								 } else if (!(tempsv->down)){
									 tempsv->down = efa->e2;  
								 }								   
							}							
							if(editedge_containsVert(efa->e3, ev) && efa->e3 != eed){
								 if(!tempsv->up){
									 tempsv->up = efa->e3;
								 } else if (!(tempsv->down)){
									 tempsv->down = efa->e3;  
								 }								   
							}  
							if(efa->e4){
								if(editedge_containsVert(efa->e4, ev) && efa->e4 != eed){
									 if(!tempsv->up){
										 tempsv->up = efa->e4;
									 } else if (!(tempsv->down)){
										 tempsv->down = efa->e4;  
									 }								   
								}
							}														  
							
						}
					}
				}		
			}			
		}		
		if(i > 4 && j == 2){
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
	while(look){	
		if(look->next != NULL){
			SlideVert *sv;

			tempsv  = BLI_ghash_lookup(vertgh,(EditVert*)look->link);
			sv		= BLI_ghash_lookup(vertgh,(EditVert*)look->next->link);
			
			if(!tempsv->up || !tempsv->down){
				error("Missing rails");
				BLI_ghash_free(vertgh, NULL, (GHashValFreeFP)MEM_freeN);
				BLI_linklist_free(vertlist,NULL); 
				BLI_linklist_free(edgelist,NULL); 
				return 0;
			}

			if(G.f & G_DRAW_EDGELEN){
				if(!(tempsv->up->f & SELECT)){
					tempsv->up->f |= SELECT;
					tempsv->up->f2 |= 16;
				} else {
					tempsv->up->f2 |= ~16;
				}
				if(!(tempsv->down->f & SELECT)){
					tempsv->down->f |= SELECT;
					tempsv->down->f2 |= 16;
				} else {
					tempsv->down->f2 |= ~16;
				}
			}

			if(sv) {
				float tempdist, co[2];

				if(!sharesFace(tempsv->up,sv->up)){
					EditEdge *swap;
					swap = sv->up;
					sv->up = sv->down;
					sv->down = swap; 
				}

				view3d_project_float(curarea, tempsv->origvert.co, co, projectMat);
				
				tempdist = sqrt(pow(co[0] - mval[0],2)+pow(co[1]  - mval[1],2));

				if(vertdist < 0){
					vertdist = tempdist;
					nearest  = (EditVert*)look->link;   
				} else if ( tempdist < vertdist ){
					vertdist = tempdist;
					nearest  = (EditVert*)look->link;	
				}		
			}
		}   		
		
		
		
		look = look->next;   
	}	   
	// we should have enough info now to slide
	//persp(PERSP_WIN);
	//glDrawBuffer(GL_FRONT); 
	percp = -1;
	while(draw){
		 /* For the % calculation */   
		short mval[2];   
		float labda, rc[2], len;   
		float v2[2], v3[2];
		EditVert *centerVert, *upVert, *downVert;

		getmouseco_areawin(mval);  
		
		if (!immediate && (mval[0] == mvalo[0] && mval[1] ==  mvalo[1])) {
			PIL_sleep_ms(10);
		} else {
			mvalo[0] = mval[0];
			mvalo[1] = mval[1];
			//Adjust Edgeloop
			if(immediate){
				perc = imperc;   
			}
			percp = perc;
			if(prop){
				look = vertlist;	  
				while(look){ 
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
				while(look){ 
					float newlen;
					EditVert *tempev;
					ev = look->link;
					tempsv = BLI_ghash_lookup(vertgh,ev);
					newlen = (len / VecLenf(editedge_getOtherVert(tempsv->up,ev)->co,editedge_getOtherVert(tempsv->down,ev)->co));
					if(newlen > 1.0){newlen = 1.0;}
					if(newlen < 0.0){newlen = 0.0;}
					if(flip == 0){
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
			
			if(prop == 0){
				// draw start edge for non-prop
				glPointSize(5);
				glBegin(GL_POINTS);
				glColor3ub(255,0,255);
				if(flip){
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
			labda= ( rc[0]*(mval[0]-v2[0]) + rc[1]*(mval[1]-v2[1]) )/len;   

			if(labda<=0.0) labda=0.0;   
			else if(labda>=1.0)labda=1.0;   

			perc=((1-labda)*2)-1;		  
			
			if(G.qual == 0){
				perc *= 100;
				perc = floor(perc);
				perc /= 100;
			} else if (G.qual == LR_CTRLKEY){
				perc *= 10;
				perc = floor(perc);
				perc /= 10;				   
			}			
			if(prop){
				sprintf(str, "(P)ercentage: %f", perc);
			} else {
				len = VecLenf(upVert->co,downVert->co)*((perc+1)/2);
				if(flip == 1){
					len = VecLenf(upVert->co,downVert->co) - len;
				} 
				sprintf(str, "Non (P)rop Length: %f, Press (F) to flip control side", len);
			}

			
			
			headerprint(str);
			screen_swapbuffers();			
		}
		if(!immediate){
			while(qtest()) {
				unsigned short val=0;		   	
				event= extern_qread(&val);	// extern_qread stores important events for the mainloop to handle 
					
				/* val==0 on key-release event */
				if (val) {
					if(ELEM(event, ESCKEY, RIGHTMOUSE)) {
							perc = 0; // Set back to begining %
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
					} else if(event==FKEY) {
							(flip == 1) ? (flip = 0):(flip = 1);  
					} else if(ELEM(event, RIGHTARROWKEY, WHEELUPMOUSE)) { // Scroll through Control Edges
						look = vertlist;	
				 		while(look){	
							if(nearest == (EditVert*)look->link){
								if(look->next == NULL){
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
				 		while(look){	
							if(look->next){
								if(look->next->link == nearest){
									nearest = (EditVert*)look->link;
									mvalo[0] = -1;
									break;
								}	  
							} else {
								if((EditVert*)vertlist->link == nearest){
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
	
	
	if(G.f & G_DRAW_EDGELEN){
		look = vertlist;
		while(look){	
			tempsv  = BLI_ghash_lookup(vertgh,(EditVert*)look->link);
			if(tempsv != NULL){
				tempsv->up->f &= !SELECT;
				tempsv->down->f &= !SELECT;
			}
			look = look->next;
		}
	}
	
	force_draw(0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
	scrarea_queue_winredraw(curarea);		 
	
	//BLI_ghash_free(edgesgh, freeGHash, NULL); 
	BLI_ghash_free(vertgh, NULL, (GHashValFreeFP)MEM_freeN);
	BLI_linklist_free(vertlist,NULL); 
	BLI_linklist_free(edgelist,NULL); 
	
	if(cancel == 1){
		return -1;
	}
	return 1;
}

//----------------------------------------------  OLD SUBDIVIDE -----------------------------------------------------

/* ******************** SUBDIVIDE ********************************** */


static void merge_weights(EditVert * vt, EditVert *vs )
{
	MDeformWeight *newdw;
	int i,j,done;
	for (j=0; j<vs->totweight; j++){
		done=0;
		/* Is vertex memeber of group */
		/* If so: Change its weight */
		for (i=0; i<vt->totweight; i++){
			if (vt->dw[i].def_nr == vs->dw[j].def_nr)
			{   /* taking the maximum makes it independant from order of occurance */
				if (vt->dw[i].weight < vs->dw[j].weight) vt->dw[i].weight = vs->dw[j].weight;
				done=1;
				break;
			}
		}
		/* If not: Add the group and set its weight */
		if (!done){
			newdw = MEM_callocN (sizeof(MDeformWeight)*(vt->totweight+1), "deformWeight");
			if (vt->dw){
				memcpy (newdw, vt->dw, sizeof(MDeformWeight)*vt->totweight);
				MEM_freeN (vt->dw);
			}
			vt->dw=newdw;
			vt->dw[vt->totweight].weight=vs->dw[j].weight;
			vt->dw[vt->totweight].def_nr=vs->dw[j].def_nr;
			vt->totweight++;
		}
	}
}


static void set_weights(EditVert * vt, EditVert *vs1,EditVert *vs2,EditVert *vs3,EditVert *vs4 )
{
/*
vt is a new generated vertex with empty deform group information
vs1..v4 are egde neighbours holding group information
so let the information ooze into the new one
*/
	if (vs1) merge_weights(vt,vs1);
	if (vs2) merge_weights(vt,vs2);
	if (vs3) merge_weights(vt,vs3);
	if (vs4) merge_weights(vt,vs4);
}




static unsigned int cpack_fact(unsigned int col1, unsigned int col2, float fact)
{
	char *cp1, *cp2, *cp;
	unsigned int col=0;
	float facti;
	
	facti=1-fact; /*result is (1-fact) * col1 and fact * col2 */
		
	cp1= (char *)&col1;
	cp2= (char *)&col2;
	cp=  (char *)&col;
	
	cp[0]= (char)(facti*cp1[0]+fact*cp2[0]);
	cp[1]= (char)(facti*cp1[1]+fact*cp2[1]);
	cp[2]= (char)(facti*cp1[2]+fact*cp2[2]);
	cp[3]= (char)(facti*cp1[3]+fact*cp2[3]);
	
	return col;
}


static void uv_half(float *uv, float *uv1, float *uv2)
{
	uv[0]= (uv1[0]+uv2[0])/2.0f;
	uv[1]= (uv1[1]+uv2[1])/2.0f;
	
}

static void uv_fact(float *uv, float *uv1, float *uv2, float fact)
{
	float facti = 1.0f - fact;
	uv[0] = facti * uv1[0] + fact * uv2[0];
	uv[1] = facti * uv1[1] + fact * uv2[1];
}

static void uv_quart(float *uv, float *uv1)
{
	uv[0]= (uv1[0]+uv1[2]+uv1[4]+uv1[6])/4.0f;
	uv[1]= (uv1[1]+uv1[3]+uv1[5]+uv1[7])/4.0f;
}

static void face_pin_vertex(EditFace *efa, EditVert *vertex)
{
	if(efa->v1 == vertex) efa->tf.unwrap |= TF_PIN1;
	else if(efa->v2 == vertex) efa->tf.unwrap |= TF_PIN2;
	else if(efa->v3 == vertex) efa->tf.unwrap |= TF_PIN3;
	else if(efa->v4 && vertex && efa->v4 == vertex) efa->tf.unwrap |= TF_PIN4;
}

static int vert_offset(EditFace *efa, EditVert *eve)
{
	if (efa->v1 == eve)
		return 0;
	if (efa->v2 == eve)
		return 1;
	if (efa->v3 == eve)
		return 2;
	if (efa->v4)
		if (efa->v4 == eve)
			return 3;
	return -1;
}

static void set_wuv(int tot, EditFace *efa, int v1, int v2, int v3, int v4, EditFace *efapin)
{
	/* this weird function only to be used for subdivide, the 'w' in the name has no meaning! */
	float *uv, uvo[4][2];
	unsigned int *col, colo[4], col1, col2;
	int a, v;

	/* recover pinning */
	if(efapin){
		efa->tf.unwrap= 0;
		if(efapin->tf.unwrap & TF_PIN1) face_pin_vertex(efa, efapin->v1);
		if(efapin->tf.unwrap & TF_PIN2) face_pin_vertex(efa, efapin->v2);
		if(efapin->tf.unwrap & TF_PIN3) face_pin_vertex(efa, efapin->v3);
		if(efapin->tf.unwrap & TF_PIN4) face_pin_vertex(efa, efapin->v4);
	}

	memcpy(uvo, efa->tf.uv, sizeof(uvo));	
	uv= efa->tf.uv[0];
	memcpy(colo, efa->tf.col, sizeof(colo));
	col= efa->tf.col;

	/* 
		Quads and Triangles reuse the same cases numbers, so we migh as well do both in the
		same loop. Especially now that the offsets are calculated and not hardcoded, it's
		much easier to reduce the code size (and make it less buggy).
	*/

	/* ******************************************** */
	/*											  */
	/* Numbers corespond to verts (corner points), 	*/
	/* edge->vn's (center edges), the Center 		*/
	/* And the quincunx points of a face 			*/
	/*											  */
	/* ******************************************** */

	/* ******************************************** */
	/* as shown here for quads:					 */
	/*											  */
	/*		   2 ------- 5 -------- 1			 */
	/*		   | \	/  |  \	/  |			 */
	/*		   |   10	|	13	|			 */
	/*		   | /	\  |  /	\  |			 */
	/*		   6 ------- 9 -------- 8			 */
	/*		   | \	/  |  \	/  |			 */
	/*		   |   11	|	12	|			 */
	/*		   | /	\  |  /	\  |			 */
	/*		   3 ------- 7 -------- 4			 */
	/*											  */
	/* ******************************************** */

	/* ******************************************** */
	/* and for triangles:						   */
	/*					 1						*/
	/*				   /   \					  */
	/*				 /	   \					*/
	/*			   5		   7				  */
	/*			 /			  \				 */
	/*		   /				  \			   */
	/*		 2 --------- 6 -------- 3			 */
	/*											  */
	/* ******************************************** */

	/* ******************************************** */
	/*											  */
	/* My talents in ascii arts are minimal so the  */
	/* drawings don't show all possible subdivision */
	/* just draw them on paper if you need to.	  */
	/*											  */
	/* ******************************************** */

	for(a=0; a<tot; a++, uv+=2, col++) {
		/* edges that are subdivided, if any */
		EditEdge *e1 = NULL, *e2 = NULL;

		if(a==0) v= v1;
		else if(a==1) v= v2;
		else if(a==2) v= v3;
		else v= v4;
		
		if(a==3 && v4==0) break;

		switch (v) {
		/* Face corners, direct copy of the UVs and VCol */
		case 1:
		case 2:
		case 3:
		case 4:
			uv[0]= uvo[v-1][0];
			uv[1]= uvo[v-1][1];
			*col= colo[v-1];
			break;
		/* Face sides (cutting an edge) */
		/*
			set the edge pointer accordingly, it's used latter to do the
			actual calculations of the new UV and VCol 
		*/
		case 5:
			e1 = efapin->e1;
			break;
		case 6:
			e1 = efapin->e2;
			break;
		case 7:
			e1 = efapin->e3;
			break;
		case 8:
			e1 = efapin->e4;
			break;

		/* The following applies to Quads only */

		/* Quad middle, just used when subdividing a quad as a whole */
		/* (not knife nor loop cut) */
		/* UVs and VCol is just the average of the four corners */
		case 9:
			uv_quart(uv, uvo[0]);
			col1= cpack_fact(colo[1], colo[0], 0.5f);
			col2= cpack_fact(colo[2], colo[3], 0.5f);
			*col= cpack_fact(col1, col2, 0.5f);
			break;
		/* Quad corner cuts */
		/* only when two adjacent edges are subdivided (and no others) */
		/* Set both edge pointers accordingly, used later for calculations */
		case 10: // case test==3 in subdivideflag() 	
			e1 = efapin->e1;
			e2 = efapin->e2;
			break;
		case 11: // case of test==6
			e1 = efapin->e2;
			e2 = efapin->e3;
			break;
		case 12: // case of test==12
			e1 = efapin->e3;
			e2 = efapin->e4;
			break;
		case 13: // case of test==9
			e1 = efapin->e4;
			e2 = efapin->e1;
			break;
		}
		/* if splitting at least an edge */
		if (e1) {
			float percent;
			int off1, off2;
			/* if splitting two edges */
			if (e2) {
				float uv1[2], uv2[2];
				/*
					UV and VCol is obtained by using the middle ground of the weighted
					average for both edges (weighted with Percent cut flag).
					In a nutshell, the average of the cuts on both edges.
				*/
				/* first cut */
				off1 = vert_offset(efapin, e1->v1);
				off2 = vert_offset(efapin, e1->v2);
				percent = e1->f1 / 32768.0f;
				uv_fact(uv1, uvo[off1], uvo[off2], percent);
				col1= cpack_fact(colo[off1], colo[off2], percent);

				/* second cut */
				off1 = vert_offset(efapin, e2->v1);
				off2 = vert_offset(efapin, e2->v2);
				percent = e2->f1 / 32768.0f;
				uv_fact(uv2, uvo[off1], uvo[off2], percent);
				col2= cpack_fact(colo[off1], colo[off1], percent);

				/* average the two */
				uv_half(uv, uv1, uv2);
				*col= cpack_fact(col1, col2, 0.5f);
			}
			/* or only one */
			else {
				/*
					UV and VCol is obtained by using the weighted average 
					of both vertice (weighted with Percent cut flag).
				*/
				off1 = vert_offset(efapin, e1->v1);
				off2 = vert_offset(efapin, e1->v2);
				percent = e1->f1 / 32768.0f;
				uv_fact(uv, uvo[off1], uvo[off2], percent);
				*col= cpack_fact(colo[off1], colo[off2], percent);
			}
		}
	}
}

static EditVert *vert_from_number(EditFace *efa, int nr)
{
	switch(nr) {
	case 0:
		return 0;
	case 1:
		return efa->v1;
	case 2:
		return efa->v2;
	case 3:
		return efa->v3;
	case 4:
		return efa->v4;
	case 5:
		return efa->e1->vn;
	case 6:
		return efa->e2->vn;
	case 7:
		return efa->e3->vn;
	case 8:
		return efa->e4->vn;
	}
	
	return NULL;
}

static void addface_subdiv(EditFace *efa, int val1, int val2, int val3, int val4, EditVert *eve, EditFace *efapin)
{
	EditFace *w;
	EditVert *v1, *v2, *v3, *v4;
	
	if(val1>=9) v1= eve;
	else v1= vert_from_number(efa, val1);
	
	if(val2>=9) v2= eve;
	else v2= vert_from_number(efa, val2);

	if(val3>=9) v3= eve;
	else v3= vert_from_number(efa, val3);

	if(val4>=9) v4= eve;
	else v4= vert_from_number(efa, val4);
	
	w= addfacelist(v1, v2, v3, v4, efa, NULL);
	if(w) {
		if(efa->v4) set_wuv(4, w, val1, val2, val3, val4, efapin);
		else set_wuv(3, w, val1, val2, val3, val4, efapin);
	}
}

static float smoothperc= 0.0;

static void smooth_subdiv_vec(float *v1, float *v2, float *n1, float *n2, float *vec)
{
	float len, fac, nor[3], nor1[3], nor2[3];
	
	VecSubf(nor, v1, v2);
	len= 0.5f*Normalise(nor);

	VECCOPY(nor1, n1);
	VECCOPY(nor2, n2);

	/* cosine angle */
	fac= nor[0]*nor1[0] + nor[1]*nor1[1] + nor[2]*nor1[2] ;
	
	vec[0]= fac*nor1[0];
	vec[1]= fac*nor1[1];
	vec[2]= fac*nor1[2];

	/* cosine angle */
	fac= -nor[0]*nor2[0] - nor[1]*nor2[1] - nor[2]*nor2[2] ;
	
	vec[0]+= fac*nor2[0];
	vec[1]+= fac*nor2[1];
	vec[2]+= fac*nor2[2];

	vec[0]*= smoothperc*len;
	vec[1]*= smoothperc*len;
	vec[2]*= smoothperc*len;
}

static void smooth_subdiv_quad(EditFace *efa, float *vec)
{
	
	float nor1[3], nor2[3];
	float vec1[3], vec2[3];
	float cent[3];
	
	/* vlr->e1->vn is new vertex inbetween v1 / v2 */
	
	VecMidf(nor1, efa->v1->no, efa->v2->no);
	Normalise(nor1);
	VecMidf(nor2, efa->v3->no, efa->v4->no);
	Normalise(nor2);

	smooth_subdiv_vec( efa->e1->vn->co, efa->e3->vn->co, nor1, nor2, vec1);

	VecMidf(nor1, efa->v2->no, efa->v3->no);
	Normalise(nor1);
	VecMidf(nor2, efa->v4->no, efa->v1->no);
	Normalise(nor2);

	smooth_subdiv_vec( efa->e2->vn->co, efa->e4->vn->co, nor1, nor2, vec2);

	VecAddf(vec1, vec1, vec2);

	CalcCent4f(cent, efa->v1->co,  efa->v2->co,  efa->v3->co,  efa->v4->co);
	VecAddf(vec, cent, vec1);
}

void subdivideflag(int flag, float rad, int beauty)
{
	EditMesh *em = G.editMesh;
	/* subdivide all with (vertflag & flag) */
	/* if rad>0.0 it's a 'sphere' subdivide */
	/* if rad<0.0 it's a fractal subdivide */
	EditVert *eve;
	EditEdge *eed, *e1, *e2, *e3, *e4, *nexted;
	EditFace *efa, efapin;
	float fac, vec[3], vec1[3], len1, len2, len3, percent;
	short test;

	if(beauty & B_SMOOTH) {
		short perc= 100;

		if(button(&perc, 10, 500, "Percentage:")==0) return;
		
		smoothperc= 0.292f*perc/100.0f;
	}

	/* edgeflags */
	if((beauty & B_KNIFE)==0) {		// knife option sets own flags
		eed= em->edges.first;
		while(eed) {	
			if( (eed->v1->f & flag) && (eed->v2->f & flag) ) eed->f2= flag;
			else eed->f2= 0;	
			eed= eed->next;
		}
	}
	
	/* if beauty: test for area and clear edge flags of 'ugly' edges */
	if(beauty & B_BEAUTY) {
		efa= em->faces.first;
		while(efa) {
			if( faceselectedAND(efa, flag) ) {
				if(efa->v4) {
				
					/* area */
					len1= AreaQ3Dfl(efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co);
					if(len1 <= G.scene->toolsettings->doublimit) {
						efa->e1->f2 = 0;
						efa->e2->f2 = 0;
						efa->e3->f2 = 0;
						efa->e4->f2 = 0;
					}
					else {
						len1= VecLenf(efa->v1->co, efa->v2->co) + VecLenf(efa->v3->co, efa->v4->co);
						len2= VecLenf(efa->v2->co, efa->v3->co) + VecLenf(efa->v1->co, efa->v4->co);
						
						if(len1 < len2) {
							efa->e1->f2 = 0;
							efa->e3->f2 = 0;
						}
						else if(len1 > len2) {
							efa->e2->f2 = 0;
							efa->e4->f2 = 0;
						}
					}
				}
				else {
					/* area */
					len1= AreaT3Dfl(efa->v1->co, efa->v2->co, efa->v3->co);
					if(len1 <= G.scene->toolsettings->doublimit) {
						efa->e1->f2 = 0;
						efa->e2->f2 = 0;
						efa->e3->f2 = 0;
					}
					else {
						len1= VecLenf(efa->v1->co, efa->v2->co) ;
						len2= VecLenf(efa->v2->co, efa->v3->co) ;
						len3= VecLenf(efa->v3->co, efa->v1->co) ;
						
						if(len1<len2 && len1<len3) {
							efa->e1->f2 = 0;
						}
						else if(len2<len3 && len2<len1) {
							efa->e2->f2 = 0;
						}
						else if(len3<len2 && len3<len1) {
							efa->e3->f2 = 0;
						}
					}
				}
			}
			efa= efa->next;
		}
	}

	/* make new normal and put in edge, clear flag! needed for face creation part below */
	eed= em->edges.first;
	while(eed) {
		if(eed->f2 & flag) {
			/* for now */
			eed->h &= ~EM_FGON;
		
			/* Subdivide percentage is stored in 1/32768ths in eed->f1 */
			if (beauty & B_PERCENTSUBD) percent=(float)(eed->f1)/32768.0f;
			else {
				eed->f1 = 32768 / 2;
				percent=0.5f;
			}
			
			vec[0]= (1-percent)*eed->v1->co[0] + percent*eed->v2->co[0];
			vec[1]= (1-percent)*eed->v1->co[1] + percent*eed->v2->co[1];
			vec[2]= (1-percent)*eed->v1->co[2] + percent*eed->v2->co[2];

			if(rad > 0.0) {   /* subdivide sphere */
				Normalise(vec);
				vec[0]*= rad;
				vec[1]*= rad;
				vec[2]*= rad;
			}
			else if(rad< 0.0) {  /* fractal subdivide */
				fac= rad* VecLenf(eed->v1->co, eed->v2->co);
				vec1[0]= fac*(float)(0.5-BLI_drand());
				vec1[1]= fac*(float)(0.5-BLI_drand());
				vec1[2]= fac*(float)(0.5-BLI_drand());
				VecAddf(vec, vec, vec1);
			}
			
			if(beauty & B_SMOOTH) {
				smooth_subdiv_vec(eed->v1->co, eed->v2->co, eed->v1->no, eed->v2->no, vec1);
				VecAddf(vec, vec, vec1);
			}
			
			eed->vn= addvertlist(vec);
			eed->vn->f= eed->v1->f;

		}
		else eed->vn= 0;
		
		eed->f2= 0; /* needed! */
		
		eed= eed->next;
	}

	/* test all faces for subdivide edges, there are 8 or 16 cases (ugh)! */

	efa= em->faces.last;
	while(efa) {

		efapin= *efa; /* make a copy of efa to recover uv pinning later */

		if( faceselectedOR(efa, flag) ) {
			/* for now */
			efa->fgonf= 0;
			
			e1= efa->e1;
			e2= efa->e2;
			e3= efa->e3;
			e4= efa->e4;

			test= 0;
			if(e1 && e1->vn) { 
				test+= 1;
				e1->f2= 1;
				/* add edges here, to copy correct edge data */
				eed= addedgelist(e1->v1, e1->vn, e1);
				eed= addedgelist(e1->vn, e1->v2, e1);
				set_weights(e1->vn, e1->v1,e1->v2,NULL,NULL);
			}
			if(e2 && e2->vn) {
				test+= 2;
				e2->f2= 1;
				/* add edges here, to copy correct edge data */
				eed= addedgelist(e2->v1, e2->vn, e2);
				eed= addedgelist(e2->vn, e2->v2, e2);
				set_weights(e2->vn, e2->v1,e2->v2,NULL,NULL);
			}
			if(e3 && e3->vn) {
				test+= 4;
				e3->f2= 1;
				/* add edges here, to copy correct edge data */
				eed= addedgelist(e3->v1, e3->vn, e3);
				eed= addedgelist(e3->vn, e3->v2, e3);
				set_weights(e3->vn, e3->v1,e3->v2,NULL,NULL);
			}
			if(e4 && e4->vn) {
				test+= 8;
				e4->f2= 1;
				/* add edges here, to copy correct edge data */
				eed= addedgelist(e4->v1, e4->vn, e4);
				eed= addedgelist(e4->vn, e4->v2, e4);
				set_weights(e4->vn, e4->v1,e4->v2,NULL,NULL);
			}
			if(test) {
				if(efa->v4==0) {  /* All the permutations of 3 edges*/
					if((test & 3)==3) addface_subdiv(efa, 2, 2+4, 1+4, 0, 0, &efapin);
					if((test & 6)==6) addface_subdiv(efa, 3, 3+4, 2+4, 0, 0, &efapin);
					if((test & 5)==5) addface_subdiv(efa, 1, 1+4, 3+4, 0, 0, &efapin);

					if(test==7) {  /* four new faces, old face renews */
						efa->v1= e1->vn;
						efa->v2= e2->vn;
						efa->v3= e3->vn;
						set_wuv(3, efa, 1+4, 2+4, 3+4, 0, &efapin);
					}
					else if(test==3) {
						addface_subdiv(efa, 1+4, 2+4, 3, 0, 0, &efapin);
						efa->v2= e1->vn;
						set_wuv(3, efa, 1, 1+4, 3, 0, &efapin);
					}
					else if(test==6) {
						addface_subdiv(efa, 2+4, 3+4, 1, 0, 0, &efapin);
						efa->v3= e2->vn;
						set_wuv(3, efa, 1, 2, 2+4, 0, &efapin);
					}
					else if(test==5) {
						addface_subdiv(efa, 3+4, 1+4, 2, 0, 0, &efapin);
						efa->v1= e3->vn;
						set_wuv(3, efa, 3+4, 2, 3, 0, &efapin);
					}
					else if(test==1) {
						addface_subdiv(efa, 1+4, 2, 3, 0, 0, &efapin);
						efa->v2= e1->vn;
						set_wuv(3, efa, 1, 1+4, 3, 0, &efapin);
					}
					else if(test==2) {
						addface_subdiv(efa, 2+4, 3, 1, 0, 0, &efapin);
						efa->v3= e2->vn;
						set_wuv(3, efa, 1, 2, 2+4, 0, &efapin);
					}
					else if(test==4) {
						addface_subdiv(efa, 3+4, 1, 2, 0, 0, &efapin);
						efa->v1= e3->vn;
						set_wuv(3, efa, 3+4, 2, 3, 0, &efapin);
					}
					efa->e1= addedgelist(efa->v1, efa->v2, NULL);
					efa->e2= addedgelist(efa->v2, efa->v3, NULL);
					efa->e3= addedgelist(efa->v3, efa->v1, NULL);

				}
				else {  /* All the permutations of 4 faces */
					if(test==15) {
						/* add a new point in center */
						CalcCent4f(vec, efa->v1->co, efa->v2->co, efa->v3->co, efa->v4->co);
						
						if(beauty & B_SMOOTH) {
							smooth_subdiv_quad(efa, vec);	/* adds */
						}
						eve= addvertlist(vec);
						set_weights(eve, efa->v1,efa->v2,efa->v3,efa->v4);
						eve->f |= flag;

						addface_subdiv(efa, 2, 2+4, 9, 1+4, eve, &efapin);
						addface_subdiv(efa, 3, 3+4, 9, 2+4, eve, &efapin);
						addface_subdiv(efa, 4, 4+4, 9, 3+4, eve, &efapin);

						efa->v2= e1->vn;
						efa->v3= eve;
						efa->v4= e4->vn;
						set_wuv(4, efa, 1, 1+4, 9, 4+4, &efapin);
					}
					else {
						if(((test & 3)==3)&&(test!=3)) addface_subdiv(efa, 1+4, 2, 2+4, 0, 0, &efapin);
						if(((test & 6)==6)&&(test!=6)) addface_subdiv(efa, 2+4, 3, 3+4, 0, 0, &efapin);
						if(((test & 12)==12)&&(test!=12)) addface_subdiv(efa, 3+4, 4, 4+4, 0, 0, &efapin);
						if(((test & 9)==9)&&(test!=9)) addface_subdiv(efa, 4+4, 1, 1+4, 0, 0, &efapin);

						if(test==1) { /* Edge 1 has new vert */
							addface_subdiv(efa, 1+4, 2, 3, 0, 0, &efapin);
							addface_subdiv(efa, 1+4, 3, 4, 0, 0, &efapin);
							efa->v2= e1->vn;
							efa->v3= efa->v4;
							efa->v4= 0;
							set_wuv(4, efa, 1, 1+4, 4, 0, &efapin);
						}
						else if(test==2) { /* Edge 2 has new vert */
							addface_subdiv(efa, 2+4, 3, 4, 0, 0, &efapin);
							addface_subdiv(efa, 2+4, 4, 1, 0, 0, &efapin);
							efa->v3= e2->vn;
							efa->v4= 0;
							set_wuv(4, efa, 1, 2, 2+4, 0, &efapin);
						}
						else if(test==4) { /* Edge 3 has new vert */
							addface_subdiv(efa, 3+4, 4, 1, 0, 0, &efapin);
							addface_subdiv(efa, 3+4, 1, 2, 0, 0, &efapin);
							efa->v1= efa->v2;
							efa->v2= efa->v3;
							efa->v3= e3->vn;
							efa->v4= 0;
							set_wuv(4, efa, 2, 3, 3+4, 0, &efapin);
						}
						else if(test==8) { /* Edge 4 has new vert */
							addface_subdiv(efa, 4+4, 1, 2, 0, 0, &efapin);
							addface_subdiv(efa, 4+4, 2, 3, 0, 0, &efapin);
							efa->v1= efa->v3;
							efa->v2= efa->v4;
							efa->v3= e4->vn;
							efa->v4= 0;
							set_wuv(4, efa, 3, 4, 4+4, 0, &efapin);
						}
						else if(test==3) { /*edge 1&2 */
							/* make new vert in center of new edge */
							vec[0]=(e1->vn->co[0]+e2->vn->co[0])/2;
							vec[1]=(e1->vn->co[1]+e2->vn->co[1])/2;
							vec[2]=(e1->vn->co[2]+e2->vn->co[2])/2;
							eve= addvertlist(vec);
							set_weights(eve, e1->vn,e2->vn,NULL,NULL);
							eve->f |= flag;
							/* Add new faces */
							addface_subdiv(efa, 4, 10, 2+4, 3, eve, &efapin);
							addface_subdiv(efa, 4, 1, 1+4, 10, eve, &efapin);
							/* orig face becomes small corner */
							efa->v1=e1->vn;
							//efa->v2=efa->v2;
							efa->v3=e2->vn;
							efa->v4=eve;

							set_wuv(4, efa, 1+4, 2, 2+4, 10, &efapin);
						}
						else if(test==6) { /* 2&3 */
							/* make new vert in center of new edge */
							vec[0]=(e2->vn->co[0]+e3->vn->co[0])/2;
							vec[1]=(e2->vn->co[1]+e3->vn->co[1])/2;
							vec[2]=(e2->vn->co[2]+e3->vn->co[2])/2;
							eve= addvertlist(vec);
							set_weights(eve, e2->vn,e3->vn,NULL,NULL);
							eve->f |= flag;
							/*New faces*/
							addface_subdiv(efa, 1, 11, 3+4, 4, eve, &efapin);
							addface_subdiv(efa, 1, 2, 2+4, 11, eve, &efapin);
							/* orig face becomes small corner */
							efa->v1=e2->vn;
							efa->v2=efa->v3;
							efa->v3=e3->vn;
							efa->v4=eve;

							set_wuv(4, efa, 2+4, 3, 3+4, 11, &efapin);
						}
						else if(test==12) { /* 3&4 */
							/* make new vert in center of new edge */
							vec[0]=(e3->vn->co[0]+e4->vn->co[0])/2;
							vec[1]=(e3->vn->co[1]+e4->vn->co[1])/2;
							vec[2]=(e3->vn->co[2]+e4->vn->co[2])/2;
							eve= addvertlist(vec);
							set_weights(eve, e3->vn,e4->vn,NULL,NULL);
							eve->f |= flag;
							/*New Faces*/
							addface_subdiv(efa, 2, 12, 4+4, 1, eve, &efapin);
							addface_subdiv(efa, 2, 3, 3+4, 12, eve, &efapin);
							/* orig face becomes small corner */
							efa->v1=e3->vn;
							efa->v2=efa->v4;
							efa->v3=e4->vn;
							efa->v4=eve;

							set_wuv(4, efa, 3+4, 4, 4+4, 12, &efapin);
						}
						else if(test==9) { /* 4&1 */
							/* make new vert in center of new edge */
							vec[0]=(e1->vn->co[0]+e4->vn->co[0])/2;
							vec[1]=(e1->vn->co[1]+e4->vn->co[1])/2;
							vec[2]=(e1->vn->co[2]+e4->vn->co[2])/2;
							eve= addvertlist(vec);
							set_weights(eve, e1->vn,e4->vn,NULL,NULL);
							eve->f |= flag;
							/*New Faces*/
							addface_subdiv(efa, 3, 13, 1+4, 2, eve, &efapin);
							addface_subdiv(efa, 3, 4,  4+4,13, eve, &efapin);
							/* orig face becomes small corner */
							efa->v2=efa->v1;
							efa->v1=e4->vn;
							efa->v3=e1->vn;
							efa->v4=eve;

							set_wuv(4, efa, 4+4, 1, 1+4, 13, &efapin);
						}
						else if(test==5) { /* 1&3 */
							addface_subdiv(efa, 1+4, 2, 3, 3+4, 0, &efapin);
							efa->v2= e1->vn;
							efa->v3= e3->vn;
							set_wuv(4, efa, 1, 1+4, 3+4, 4, &efapin);
						}
						else if(test==10) { /* 2&4 */
							addface_subdiv(efa, 2+4, 3, 4, 4+4, 0, &efapin);
							efa->v3= e2->vn;
							efa->v4= e4->vn;
							set_wuv(4, efa, 1, 2, 2+4, 4+4, &efapin);
						}/* Unfortunately, there is no way to avoid tris on 1 or 3 edges*/
						else if(test==7) { /*1,2&3 */
							addface_subdiv(efa, 1+4, 2+4, 3+4, 0, 0, &efapin);
							efa->v2= e1->vn;
							efa->v3= e3->vn;
							set_wuv(4, efa, 1, 1+4, 3+4, 4, &efapin);
						}
						
						else if(test==14) { /* 2,3&4 */
							addface_subdiv(efa, 2+4, 3+4, 4+4, 0, 0, &efapin);
							efa->v3= e2->vn;
							efa->v4= e4->vn;
							set_wuv(4, efa, 1, 2, 2+4, 4+4, &efapin);
						}
						else if(test==13) {/* 1,3&4 */
							addface_subdiv(efa, 3+4, 4+4, 1+4, 0, 0, &efapin);
							efa->v4= e3->vn;
							efa->v1= e1->vn;
							set_wuv(4, efa, 1+4, 2, 3, 3+4, &efapin);
						}
						else if(test==11) { /* 1,2,&4 */
							addface_subdiv(efa, 4+4, 1+4, 2+4, 0, 0, &efapin);
							efa->v1= e4->vn;
							efa->v2= e2->vn;
							set_wuv(4, efa, 4+4, 2+4, 3, 4, &efapin);
						}
					}
					efa->e1= addedgelist(efa->v1, efa->v2, NULL);
					efa->e2= addedgelist(efa->v2, efa->v3, NULL);
					if(efa->v4) efa->e3= addedgelist(efa->v3, efa->v4, NULL);
					else efa->e3= addedgelist(efa->v3, efa->v1, NULL);
					if(efa->v4) efa->e4= addedgelist(efa->v4, efa->v1, NULL);
					else efa->e4= NULL;
				}
			}
		}
		efa= efa->prev;
	}

	/* remove all old edges, if needed make new ones */
	eed= em->edges.first;
	while(eed) {
		nexted= eed->next;
		if( eed->vn ) {
			eed->vn->f |= 16;			
			if(eed->f2==0) {  /* not used in face */				
				addedgelist(eed->v1, eed->vn, eed);
				addedgelist(eed->vn, eed->v2, eed);
			}						
			remedge(eed);
			free_editedge(eed);
		}
		eed= nexted;
	}
	
	/* since this is all on vertex level, flush vertex selection */
	EM_select_flush();
	recalc_editnormals();
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);
}
