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

#include "MEM_guardedalloc.h"


#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_editmesh.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_mywindow.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"
#include "BIF_transform.h"

#include "BDR_editobject.h" 

#include "BSE_view.h"
#include "BSE_edit.h"

#include "mydevice.h"
#include "blendef.h"

#include "editmesh.h"

static float icovert[12][3] = {
	{0,0,-200}, 
	{144.72, -105.144,-89.443},
	{-55.277, -170.128,-89.443}, 
	{-178.885,0,-89.443},
	{-55.277,170.128,-89.443}, 
	{144.72,105.144,-89.443},
	{55.277,-170.128,89.443},
	{-144.72,-105.144,89.443},
	{-144.72,105.144,89.443},
	{55.277,170.128,89.443},
	{178.885,0,89.443},
	{0,0,200}
};
static short icoface[20][3] = {
	{1,0,2},
	{1,0,5},
	{2,0,3},
	{3,0,4},
	{4,0,5},
	{1,5,10},
	{2,1,6},
	{3,2,7},
	{4,3,8},
	{5,4,9},
	{10,1,6},
	{6,2,7},
	{7,3,8},
	{8,4,9},
	{9,5,10},
	{6,10,11},
	{7,6,11},
	{8,7,11},
	{9,8,11},
	{10,9,11}
};

static void get_view_aligned_coordinate(float *fp)
{
	float dvec[3];
	short mx, my, mval[2];
	
	getmouseco_areawin(mval);
	mx= mval[0];
	my= mval[1];
	
	project_short_noclip(fp, mval);
	
	initgrabz(fp[0], fp[1], fp[2]);
	
	if(mval[0]!=IS_CLIPPED) {
		window_to_3d(dvec, mval[0]-mx, mval[1]-my);
		VecSubf(fp, fp, dvec);
	}
}

void add_click_mesh(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve, *v1;
	float min[3], max[3];
	int done= 0;
	
	TEST_EDITMESH
	
	INIT_MINMAX(min, max);
	
	for(v1= em->verts.first;v1; v1=v1->next) {
		if(v1->f & SELECT) {
			DO_MINMAX(v1->co, min, max);
			done= 1;
		}
	}

	/* call extrude? */
	if(done) {
		EditEdge *eed;
		float vec[3], cent[3], mat[3][3];
		float nor[3]= {0.0, 0.0, 0.0};
		
		/* check for edges that are half selected, use for rotation */
		done= 0;
		for(eed= em->edges.first; eed; eed= eed->next) {
			if( (eed->v1->f & SELECT)+(eed->v2->f & SELECT) == SELECT ) {
				if(eed->v1->f & SELECT) VecSubf(vec, eed->v1->co, eed->v2->co);
				else VecSubf(vec, eed->v2->co, eed->v1->co);
				VecAddf(nor, nor, vec);
				done= 1;
			}
		}
		if(done) Normalise(nor);
		
		/* centre */
		VecAddf(cent, min, max);
		VecMulf(cent, 0.5f);
		VECCOPY(min, cent);
		
		Mat4MulVecfl(G.obedit->obmat, min);	// view space
		get_view_aligned_coordinate(min);
		Mat4Invert(G.obedit->imat, G.obedit->obmat); 
		Mat4MulVecfl(G.obedit->imat, min); // back in object space
		
		VecSubf(min, min, cent);
		
		/* calculate rotation */
		Mat3One(mat);
		if(done) {
			float dot;
			
			VECCOPY(vec, min);
			Normalise(vec);
			dot= INPR(vec, nor);

			if( fabs(dot)<0.999) {
				float cross[3], si, q1[4];
				
				Crossf(cross, nor, vec);
				Normalise(cross);
				dot= 0.5f*saacos(dot);
				si= (float)sin(dot);
				q1[0]= (float)cos(dot);
				q1[1]= cross[0]*si;
				q1[2]= cross[1]*si;
				q1[3]= cross[2]*si;
				
				QuatToMat3(q1, mat);
			}
		}
		
		extrudeflag(SELECT, nor);
		rotateflag(SELECT, cent, mat);
		translateflag(SELECT, min);
		
		recalc_editnormals();
	}
	else {
		float mat[3][3],imat[3][3];
		float *curs= give_cursor();
		
		eve= addvertlist(0);

		Mat3CpyMat4(mat, G.obedit->obmat);
		Mat3Inv(imat, mat);
		
		VECCOPY(eve->co, curs);
		VecSubf(eve->co, eve->co, G.obedit->obmat[3]);

		Mat3MulVecfl(imat, eve->co);
		
		eve->f= SELECT;
	}
	
	countall();

	BIF_undo_push("Add vertex/edge/face");
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
	
	while(get_mbut()&R_MOUSE);

}

/* selected faces get hidden edges */
static void make_fgon(void)
{
	EditMesh *em = G.editMesh;
	EditFace *efa;
	EditEdge *eed;
	EditVert *eve;
	float *nor=NULL;	// reference
	int done=0, ret;
	
	ret= pupmenu("FGon %t|Make|Clear");
	if(ret<1) return;
	
	if(ret==2) {
		for(efa= em->faces.first; efa; efa= efa->next) {
			if(efa->f & SELECT) {
				efa->fgonf= 0;
				efa->e1->h &= ~EM_FGON;
				efa->e2->h &= ~EM_FGON;
				efa->e3->h &= ~EM_FGON;
				if(efa->e4) efa->e4->h &= ~EM_FGON;
			}
		}
		allqueue(REDRAWVIEW3D, 0);
		EM_fgon_flags();	// redo flags and indices for fgons
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
		BIF_undo_push("Clear FGon");
		return;
	}

	/* tagging edges. rule is:
	   - edge used by exactly 2 selected faces
	   - no vertices allowed with only tagged edges (return)
	   - face normals are allowed to difffer
	 
	*/
	for(eed= em->edges.first; eed; eed= eed->next) {
		eed->f1= 0;	// amount of selected
		eed->f2= 0; // amount of unselected
	}
	
	for(efa= em->faces.first; efa; efa= efa->next) {
		if(efa->f & SELECT) {
			if(nor==NULL) nor= efa->n;
			if(efa->e1->f1 < 3) efa->e1->f1++;
			if(efa->e2->f1 < 3) efa->e2->f1++;
			if(efa->e3->f1 < 3) efa->e3->f1++;
			if(efa->e4 && efa->e4->f1 < 3) efa->e4->f1++;
		}
		else {
			if(efa->e1->f2 < 3) efa->e1->f2++;
			if(efa->e2->f2 < 3) efa->e2->f2++;
			if(efa->e3->f2 < 3) efa->e3->f2++;
			if(efa->e4 && efa->e4->f2 < 3) efa->e4->f2++;
		}
	}
	// now eed->f1 becomes tagged edge
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->f1==2 && eed->f2==0) eed->f1= 1;
		else eed->f1= 0;
	}
	
	// no vertices allowed with only tagged edges
	for(eve= em->verts.first; eve; eve= eve->next) eve->f1= 0;
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->f1) {
			eed->v1->f1 |= 1;
			eed->v2->f1 |= 1;
		}
		else {
			eed->v1->f1 |= 2;
			eed->v2->f1 |= 2;
		}
	}
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f1==1) break;
	}
	if(eve) {
		error("Cannot make polygon with interior vertices");
		return;
	}
	
	// check for faces
	if(nor==NULL) {
		error("No faces selected to make FGon");
		return;
	}

	// and there we go
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->f1) {
			eed->h |= EM_FGON;
			done= 1;
		}
	}
	
	if(done==0) {
		error("Didn't find FGon to create");
	}
	else {
		EM_fgon_flags();	// redo flags and indices for fgons

		allqueue(REDRAWVIEW3D, 0);
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
		BIF_undo_push("Make FGon");
	}
}

/* precondition; 4 vertices selected, check for 4 edges and create face */
static EditFace *addface_from_edges(void)
{
	EditMesh *em = G.editMesh;
	EditEdge *eed, *eedar[4]={NULL, NULL, NULL, NULL};
	EditVert *v1=NULL, *v2=NULL, *v3=NULL, *v4=NULL;
	int a;
	
	/* find the 4 edges */
	for(eed= em->edges.first; eed; eed= eed->next) {
		if(eed->f & SELECT) {
			if(eedar[0]==NULL) eedar[0]= eed;
			else if(eedar[1]==NULL) eedar[1]= eed;
			else if(eedar[2]==NULL) eedar[2]= eed;
			else eedar[3]= eed;
		}
	}
	if(eedar[3]) {
		/* first 2 points */
		v1= eedar[0]->v1;
		v2= eedar[0]->v2;
		
		/* find the 2 edges connected to first edge */
		for(a=1; a<4; a++) {
			if( eedar[a]->v1 == v2) v3= eedar[a]->v2;
			else if(eedar[a]->v2 == v2) v3= eedar[a]->v1;
			else if( eedar[a]->v1 == v1) v4= eedar[a]->v2;
			else if(eedar[a]->v2 == v1) v4= eedar[a]->v1;
		}
		
		/* verify if last edge exists */
		if(v3 && v4) {
			for(a=1; a<4; a++) {
				if( eedar[a]->v1==v3 && eedar[a]->v2==v4) break;
				if( eedar[a]->v2==v3 && eedar[a]->v1==v4) break;
			}
			if(a!=4) {
				return addfacelist(v1, v2, v3, v4, NULL, NULL);
			}
		}
	}
	return NULL;
}

/* this also allows to prevent triangles being made in quads */
static int compareface_overlaps(EditFace *vl1, EditFace *vl2)
{
	EditVert *v1, *v2, *v3, *v4;
	int equal= 0;
	
	v1= vl2->v1;
	v2= vl2->v2;
	v3= vl2->v3;
	v4= vl2->v4;
	
	if(vl1==vl2) return 0;
	
	if(v4==NULL && vl1->v4==NULL) {
		if(vl1->v1==v1 || vl1->v2==v1 || vl1->v3==v1) equal++;
		if(vl1->v1==v2 || vl1->v2==v2 || vl1->v3==v2) equal++;
		if(vl1->v1==v3 || vl1->v2==v3 || vl1->v3==v3) equal++;
	}
	else {
		if(vl1->v1==v1 || vl1->v2==v1 || vl1->v3==v1 || vl1->v4==v1) equal++;
		if(vl1->v1==v2 || vl1->v2==v2 || vl1->v3==v2 || vl1->v4==v2) equal++;
		if(vl1->v1==v3 || vl1->v2==v3 || vl1->v3==v3 || vl1->v4==v3) equal++;
		if(vl1->v1==v4 || vl1->v2==v4 || vl1->v3==v4 || vl1->v4==v4) equal++;
	}

	if(v4 && vl1->v4) {
		if(equal==4) return 1;
	}
	else 
		if(equal>=3) return 1;
	
	return 0;
}

/* checks for existance, and for tria overlapping inside quad */
static EditFace *exist_face_overlaps(EditVert *v1, EditVert *v2, EditVert *v3, EditVert *v4)
{
	EditMesh *em = G.editMesh;
	EditFace *efa, efatest;
	
	efatest.v1= v1;
	efatest.v2= v2;
	efatest.v3= v3;
	efatest.v4= v4;
	
	efa= em->faces.first;
	while(efa) {
		if(compareface_overlaps(&efatest, efa)) return efa;
		efa= efa->next;
	}
	return NULL;
}

/* will be new face smooth or solid? depends on smoothness of face neighbours
 * of new face, if function return 1, then new face will be smooth, when functio
 * will return zero, then new face will be solid */
static void fix_new_face(EditFace *eface)
{
	struct EditMesh *em = G.editMesh;
	struct EditFace *efa;
	struct EditEdge *eed=NULL;
	struct EditVert *v1 = eface->v1, *v2 = eface->v2, *v3 = eface->v3, *v4 = eface->v4;
	struct EditVert *ev1=NULL, *ev2=NULL;
	short smooth=0; /* "total smoothnes" of faces in neighbourhood */
	short coef;	/* "weight" of smoothness */
	short count=0;	/* number of edges with same direction as eface */
	short vi00=0, vi01=0, vi10=0, vi11=0; /* vertex indexes */

	efa = em->faces.first;

	while(efa) {

		if(efa==eface) {
			efa = efa->next;
			continue;
		}

		coef = 0;
		ev1 = ev2 = NULL;
		eed = NULL;

		if(efa->v1==v1 || efa->v2==v1 || efa->v3==v1 || efa->v4==v1) {
			ev1 = v1;
			coef++;
		}
		if(efa->v1==v2 || efa->v2==v2 || efa->v3==v2 || efa->v4==v2) {
			if(ev1) ev2 = v2;
			else ev1 = v2;
			coef++;
		}
		if(efa->v1==v3 || efa->v2==v3 || efa->v3==v3 || efa->v4==v3) {
			if(coef<2) {
				if(ev1) ev2 = v3;
				else ev1 = v3;
			}
			coef++;
		}
		if((v4) && (efa->v1==v4 || efa->v2==v4 || efa->v3==v4 || efa->v4==v4)) {
			if(ev1 && coef<2) ev2 = v4;
			coef++;
		}

		/* "democracy" of smoothness */
		if(efa->flag & ME_SMOOTH)
			smooth += coef;
		else
			smooth -= coef;

		/* try to find edge using vertexes ev1 and ev2 */
		if((ev1) && (ev2) && (ev1!=ev2)) eed = findedgelist(ev1, ev2);

		/* has bordering edge of efa same direction as edge of eface ? */
		if(eed) {
			if(eed->v1==v1) vi00 = 1;
			else if(eed->v1==v2) vi00 = 2;
			else if(eed->v1==v3) vi00 = 3;
			else if(v4 && eed->v1==v4) vi00 = 4;

			if(eed->v2==v1) vi01 = 1;
			else if(eed->v2==v2) vi01 = 2;
			else if(eed->v2==v3) vi01 = 3;
			else if(v4 && eed->v2==v4) vi01 = 4;

			if(v4) {
				if(vi01==1 && vi00==4) vi00 = 0;
				if(vi01==4 && vi00==1) vi01 = 0;
			}
			else {
				if(vi01==1 && vi00==3) vi00 = 0;
				if(vi01==3 && vi00==1) vi01 = 0;
			}

			if(eed->v1==efa->v1) vi10 = 1;
			else if(eed->v1==efa->v2) vi10 = 2;
			else if(eed->v1==efa->v3) vi10 = 3;
			else if(efa->v4 && eed->v1==efa->v4) vi10 = 4;

			if(eed->v2==efa->v1) vi11 = 1;
			else if(eed->v2==efa->v2) vi11 = 2;
			else if(eed->v2==efa->v3) vi11 = 3;
			else if(efa->v4 && eed->v2==efa->v4) vi11 = 4;

			if(efa->v4) {
				if(vi11==1 && vi10==4) vi10 = 0;
				if(vi11==4 && vi10==1) vi11 = 0;
			}
			else {
				if(vi11==1 && vi10==3) vi10 = 0;
				if(vi11==3 && vi10==1) vi11 = 0;
			}

			if(((vi00>vi01) && (vi10>vi11)) ||
				((vi00<vi01) && (vi10<vi11)))
				count++;
			else
				count--;
		}

		efa = efa->next;
	}

	/* set up smoothness according voting of face in neighbourhood */
	if(smooth >= 0)
		eface->flag |= ME_SMOOTH;
	else
		eface->flag &= ~ME_SMOOTH;

	/* flip face, when too much "face normals" in neighbourhood is different */
	if(count > 0) flipface(eface);
}

void addedgeface_mesh(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve, *neweve[4];
	EditEdge *eed;
	EditFace *efa;
	short amount=0;

	if( (G.vd->lay & G.obedit->lay)==0 ) return;

	/* how many selected ? */
	if(G.scene->selectmode & SCE_SELECT_EDGE) {
		/* in edge mode finding selected vertices means flushing down edge codes... */
		/* can't make face with only edge selection info... */
		EM_selectmode_set();
	}
	
	for(eve= em->verts.first; eve; eve= eve->next) {
		if(eve->f & SELECT) {
			amount++;
			if(amount>4) break;			
			neweve[amount-1]= eve;
		}
	}

	if(amount==2) {
		eed= addedgelist(neweve[0], neweve[1], NULL);
		EM_select_edge(eed, 1);
		BIF_undo_push("Add edge");
		allqueue(REDRAWVIEW3D, 0);
		countall();
		DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
		return;
	}
	else if(amount > 4) {
		make_fgon();
		return;
	}
	else if(amount<2) {
		error("Incorrect number of vertices to make edge/face");
		return;
	}

	efa= NULL; // check later

	if(amount==3) {
		
		if(exist_face_overlaps(neweve[0], neweve[1], neweve[2], NULL)==0) {
			efa= addfacelist(neweve[0], neweve[1], neweve[2], 0, NULL, NULL);
			EM_select_face(efa, 1);
		}
		else error("The selected vertices already form a face");
	}
	else if(amount==4) {
		/* this test survives when theres 2 triangles */
		if(exist_face(neweve[0], neweve[1], neweve[2], neweve[3])==0) {
			int tria= 0;
			
			/* remove trias if they exist, 4 cases.... */
			if(exist_face(neweve[0], neweve[1], neweve[2], NULL)) tria++;
			if(exist_face(neweve[0], neweve[1], neweve[3], NULL)) tria++;
			if(exist_face(neweve[0], neweve[2], neweve[3], NULL)) tria++;
			if(exist_face(neweve[1], neweve[2], neweve[3], NULL)) tria++;
		
			if(tria==2) join_triangles();
			else if(exist_face_overlaps(neweve[0], neweve[1], neweve[2], neweve[3])==0) {
				/* if 4 edges exist, we just create the face, convex or not */
				efa= addface_from_edges();
				if(efa==NULL) {
					/* the order of vertices can be anything, 6 cases to check */
					if( convex(neweve[0]->co, neweve[1]->co, neweve[2]->co, neweve[3]->co) ) {
						efa= addfacelist(neweve[0], neweve[1], neweve[2], neweve[3], NULL, NULL);
					}
					else if( convex(neweve[0]->co, neweve[2]->co, neweve[3]->co, neweve[1]->co) ) {
						efa= addfacelist(neweve[0], neweve[2], neweve[3], neweve[1], NULL, NULL);
					}
					else if( convex(neweve[0]->co, neweve[2]->co, neweve[1]->co, neweve[3]->co) ) {
						efa= addfacelist(neweve[0], neweve[2], neweve[1], neweve[3], NULL, NULL);
					}
					
					else if( convex(neweve[1]->co, neweve[2]->co, neweve[3]->co, neweve[0]->co) ) {
						efa= addfacelist(neweve[1], neweve[2], neweve[3], neweve[0], NULL, NULL);
					}
					else if( convex(neweve[1]->co, neweve[3]->co, neweve[0]->co, neweve[2]->co) ) {
						efa= addfacelist(neweve[1], neweve[3], neweve[0], neweve[2], NULL, NULL);
					}
					else if( convex(neweve[1]->co, neweve[3]->co, neweve[2]->co, neweve[0]->co) ) {
						efa= addfacelist(neweve[1], neweve[3], neweve[2], neweve[0], NULL, NULL);
					}
					else error("The selected vertices form a concave quad");
				}
			}
			else error("The selected vertices already form a face");
		}
		else error("The selected vertices already form a face");
	}
	
	if(efa) {
		EM_select_face(efa, 1);

		fix_new_face(efa);

		BIF_undo_push("Add face");
	}
	
	countall();
	allqueue(REDRAWVIEW3D, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
}


void adduplicate_mesh(void)
{

	TEST_EDITMESH

	waitcursor(1);

	adduplicateflag(SELECT);

	waitcursor(0);
	countall(); 

		/* We need to force immediate calculation here because 
		* transform may use derived objects (which are now stale).
		*
		* This shouldn't be necessary, derived queries should be
		* automatically building this data if invalid. Or something.
		*/
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	
	object_handle_update(G.obedit);

	BIF_TransformSetUndo("Add Duplicate");
	initTransform(TFM_TRANSLATION, CTX_NO_PET);
	Transform();
}

/* check whether an object to add mesh to exists, if not, create one
* returns 1 if new object created, else 0 */
int confirm_objectExists( Mesh **me, float mat[][3] )
{
	int newob = 0;
	
	/* deselectall */
	EM_clear_flag_all(SELECT);
	
	/* if no obedit: new object and enter editmode */
	if(G.obedit==NULL) {
		/* add_object actually returns an object ! :-)
		But it also stores the added object struct in
		G.scene->basact->object (BASACT->object) */

		add_object_draw(OB_MESH);

		G.obedit= BASACT->object;
		
		where_is_object(G.obedit);
		
		make_editMesh();
		setcursor_space(SPACE_VIEW3D, CURSOR_EDIT);
		newob= 1;
	}
	*me = G.obedit->data;
	
	/* imat and centre and size */
	Mat3CpyMat4(mat, G.obedit->obmat);
	
	return newob;
}

void make_prim(int type, float imat[3][3], short tot, short seg,
		short subdiv, float dia, float d, short ext, short fill,
        float cent[3])
{
	EditMesh *em = G.editMesh;
	EditVert *eve, *v1=NULL, *v2, *v3, *v4=NULL, *vtop, *vdown;
	float phi, phid, vec[3];
	float q[4], cmat[3][3], nor[3]= {0.0, 0.0, 0.0};
	short a, b;

	phid= 2*M_PI/tot;
	phi= .25*M_PI;

	switch(type) {
	case 10: /*  grid */
		/* clear flags */
		eve= em->verts.first;
		while(eve) {
			eve->f= 0;
			eve= eve->next;
		}
		/* one segment first: the X axis */
		phi= 1.0; 
		phid= 2.0/((float)tot-1);
		for(a=0;a<tot;a++) {
			vec[0]= cent[0]+dia*phi;
			vec[1]= cent[1]- dia;
			vec[2]= cent[2];
			Mat3MulVecfl(imat,vec);
			eve= addvertlist(vec);
			eve->f= 1+2+4;
			if (a) {
				addedgelist(eve->prev, eve, NULL);
			}
			phi-=phid;
		}
		/* extrude and translate */
		vec[0]= vec[2]= 0.0;
		vec[1]= dia*phid;
		Mat3MulVecfl(imat, vec);
		for(a=0;a<seg-1;a++) {
			extrudeflag_vert(2, nor);	// nor unused
			translateflag(2, vec);
		}
		break;
	case 11: /*  UVsphere */
		
		/* clear all flags */
		eve= em->verts.first;
		while(eve) {
			eve->f= 0;
			eve= eve->next;
		}
		
		/* one segment first */
		phi= 0; 
		phid/=2;
		for(a=0; a<=tot; a++) {
			vec[0]= dia*sin(phi);
			vec[1]= 0.0;
			vec[2]= dia*cos(phi);
			eve= addvertlist(vec);
			eve->f= 1+2+4;
			if(a==0) v1= eve;
			else addedgelist(eve->prev, eve, NULL);
			phi+= phid;
		}
		
		/* extrude and rotate */
		phi= M_PI/seg;
		q[0]= cos(phi);
		q[3]= sin(phi);
		q[1]=q[2]= 0;
		QuatToMat3(q, cmat);
		
		for(a=0; a<seg; a++) {
			extrudeflag_vert(2, nor); // nor unused
			rotateflag(2, v1->co, cmat);
		}

		removedoublesflag(4, 0.0001);

		/* and now do imat */
		eve= em->verts.first;
		while(eve) {
			if(eve->f & SELECT) {
				VecAddf(eve->co,eve->co,cent);
				Mat3MulVecfl(imat,eve->co);
			}
			eve= eve->next;
		}
		break;
	case 12: /* Icosphere */
		{
			EditVert *eva[12];
			EditEdge *eed;
			
			/* clear all flags */
			eve= em->verts.first;
			while(eve) {
				eve->f= 0;
				eve= eve->next;
			}
			dia/=200;
			for(a=0;a<12;a++) {
				vec[0]= dia*icovert[a][0];
				vec[1]= dia*icovert[a][1];
				vec[2]= dia*icovert[a][2];
				eva[a]= addvertlist(vec);
				eva[a]->f= 1+2;
			}
			for(a=0;a<20;a++) {
				EditFace *evtemp;
				v1= eva[ icoface[a][0] ];
				v2= eva[ icoface[a][1] ];
				v3= eva[ icoface[a][2] ];
				evtemp = addfacelist(v1, v2, v3, 0, NULL, NULL);
				evtemp->e1->f = 1+2;
				evtemp->e2->f = 1+2;
				evtemp->e3->f = 1+2;
			}

			dia*=200;
			for(a=1; a<subdiv; a++) esubdivideflag(2, dia, 0,1,0);
			/* and now do imat */
			eve= em->verts.first;
			while(eve) {
				if(eve->f & 2) {
					VecAddf(eve->co,eve->co,cent);
					Mat3MulVecfl(imat,eve->co);
				}
				eve= eve->next;
			}
			
			// Clear the flag 2 from the edges
			for(eed=em->edges.first;eed;eed=eed->next){
				if(eed->f & 2){
					   eed->f &= !2;
				}   
			}
		}
		break;
	case 13: /* Monkey */
		{
			extern int monkeyo, monkeynv, monkeynf;
			extern signed char monkeyf[][4];
			extern signed char monkeyv[][3];
			EditVert **tv= MEM_mallocN(sizeof(*tv)*monkeynv*2, "tv");
			EditFace *efa;
			int i;

			for (i=0; i<monkeynv; i++) {
				float v[3];
				v[0]= (monkeyv[i][0]+127)/128.0, v[1]= monkeyv[i][1]/128.0, v[2]= monkeyv[i][2]/128.0;
				tv[i]= addvertlist(v);
				tv[i]->f |= SELECT;
				tv[monkeynv+i]= (fabs(v[0]= -v[0])<0.001)?tv[i]:addvertlist(v);
				tv[monkeynv+i]->f |= SELECT;
			}
			for (i=0; i<monkeynf; i++) {
				efa= addfacelist(tv[monkeyf[i][0]+i-monkeyo], tv[monkeyf[i][1]+i-monkeyo], tv[monkeyf[i][2]+i-monkeyo], (monkeyf[i][3]!=monkeyf[i][2])?tv[monkeyf[i][3]+i-monkeyo]:NULL, NULL, NULL);
				efa= addfacelist(tv[monkeynv+monkeyf[i][2]+i-monkeyo], tv[monkeynv+monkeyf[i][1]+i-monkeyo], tv[monkeynv+monkeyf[i][0]+i-monkeyo], (monkeyf[i][3]!=monkeyf[i][2])?tv[monkeynv+monkeyf[i][3]+i-monkeyo]:NULL, NULL, NULL);
			}

			MEM_freeN(tv);

			/* and now do imat */
			for(eve= em->verts.first; eve; eve= eve->next) {
				if(eve->f & SELECT) {
					VecAddf(eve->co,eve->co,cent);
					Mat3MulVecfl(imat,eve->co);
				}
			}
			recalc_editnormals();
		}
		break;
	default: /* all types except grid, sphere... */
		if(ext==0 && type!=7) d= 0;
	
		/* vertices */
		vtop= vdown= v1= v2= 0;
		for(b=0; b<=ext; b++) {
			for(a=0; a<tot; a++) {
				
				vec[0]= cent[0]+dia*sin(phi);
				vec[1]= cent[1]+dia*cos(phi);
				vec[2]= cent[2]+d;
				
				Mat3MulVecfl(imat, vec);
				eve= addvertlist(vec);
				eve->f= SELECT;
				if(a==0) {
					if(b==0) v1= eve;
					else v2= eve;
				}
				phi+=phid;
			}
			d= -d;
		}
		/* centre vertices */
		if(fill && type>1) {
			VECCOPY(vec,cent);
			vec[2]-= -d;
			Mat3MulVecfl(imat,vec);
			vdown= addvertlist(vec);
			if(ext || type==7) {
				VECCOPY(vec,cent);
				vec[2]-= d;
				Mat3MulVecfl(imat,vec);
				vtop= addvertlist(vec);
			}
		} else {
			vdown= v1;
			vtop= v2;
		}
		if(vtop) vtop->f= SELECT;
		if(vdown) vdown->f= SELECT;
	
		/* top and bottom face */
		if(fill) {
			if(tot==4 && (type==0 || type==1)) {
				v3= v1->next->next;
				if(ext) v4= v2->next->next;
				
				addfacelist(v3, v1->next, v1, v3->next, NULL, NULL);
				if(ext) addfacelist(v2, v2->next, v4, v4->next, NULL, NULL);
				
			}
			else {
				v3= v1;
				v4= v2;
				for(a=1; a<tot; a++) {
					addfacelist(vdown, v3, v3->next, 0, NULL, NULL);
					v3= v3->next;
					if(ext) {
						addfacelist(vtop, v4, v4->next, 0, NULL, NULL);
						v4= v4->next;
					}
				}
				if(type>1) {
					addfacelist(vdown, v3, v1, 0, NULL, NULL);
					if(ext) addfacelist(vtop, v4, v2, 0, NULL, NULL);
				}
			}
		}
		else if(type==4) {  /* we need edges for a circle */
			v3= v1;
			for(a=1;a<tot;a++) {
				addedgelist(v3, v3->next, NULL);
				v3= v3->next;
			}
			addedgelist(v3, v1, NULL);
		}
		/* side faces */
		if(ext) {
			v3= v1;
			v4= v2;
			for(a=1; a<tot; a++) {
				addfacelist(v3, v3->next, v4->next, v4, NULL, NULL);
				v3= v3->next;
				v4= v4->next;
			}
			addfacelist(v3, v1, v2, v4, NULL, NULL);
		}
		else if(type==7) { /* cone */
			v3= v1;
			for(a=1; a<tot; a++) {
				addfacelist(vtop, v3->next, v3, 0, NULL, NULL);
				v3= v3->next;
			}
			addfacelist(vtop, v1, v3, 0, NULL, NULL);
		}
	}
	/* simple selection flush OK, based on fact it's a single model */
	EM_select_flush(); /* flushes vertex -> edge -> face selection */
	
	if(type!=0 && type!=13)
		righthandfaces(1);	/* otherwise monkey has eyes in wrong direction */
}

void add_primitiveMesh(int type)
{
	Mesh *me;
	float *curs, d, dia, phi, phid, cent[3], imat[3][3], mat[3][3];
	float cmat[3][3];
	static short tot=32, seg=32, subdiv=2;
	short ext=0, fill=0, totoud, newob=0;
	char *undostr="Add Primitive";
	char *name=NULL;
	
	if(G.scene->id.lib) return;

	/* this function also comes from an info window */
	if ELEM(curarea->spacetype, SPACE_VIEW3D, SPACE_INFO); else return;
	if(G.vd==0) return;

	/* if editmode exists for other type, it exits */
	check_editmode(OB_MESH);
	
	if(G.f & (G_VERTEXPAINT+G_FACESELECT+G_TEXTUREPAINT)) {
		G.f &= ~(G_VERTEXPAINT+G_FACESELECT+G_TEXTUREPAINT);
		setcursor_space(SPACE_VIEW3D, CURSOR_EDIT);
	}

	totoud= tot; /* store, and restore when cube/plane */
	
	/* ext==extrudeflag, tot==amount of vertices in basis */

	switch(type) {
	case 0:		/* plane */
		tot= 4;
		ext= 0;
		fill= 1;
		newob = confirm_objectExists( &me, mat );
		if(newob) name = "Plane";
		undostr="Add Plane";
		break;
	case 1:		/* cube  */
		tot= 4;
		ext= 1;
		fill= 1;
		newob = confirm_objectExists( &me, mat );
		if(newob) name = "Cube";
		undostr="Add Cube";
		break;
	case 4:		/* circle  */
		if(button(&tot,3,100,"Vertices:")==0) return;
		ext= 0;
		fill= 0;
		newob = confirm_objectExists( &me, mat );
		if(newob) name = "Circle";
		undostr="Add Circle";
		break;
	case 5:		/* cylinder  */
		if(button(&tot,3,100,"Vertices:")==0) return;
		ext= 1;
		fill= 1;
		newob = confirm_objectExists( &me, mat );
		if(newob) name = "Cylinder";
		undostr="Add Cylinder";
		break;
	case 6:		/* tube  */
		if(button(&tot,3,100,"Vertices:")==0) return;
		ext= 1;
		fill= 0;
		newob = confirm_objectExists( &me, mat );
		if(newob) name = "Tube";
		undostr="Add Tube";
		break;
	case 7:		/* cone  */
		if(button(&tot,3,100,"Vertices:")==0) return;
		ext= 0;
		fill= 1;
		newob = confirm_objectExists( &me, mat );
		if(newob) name = "Cone";
		undostr="Add Cone";
		break;
	case 10:	/* grid */
		if(button(&tot,2,100,"X res:")==0) return;
		if(button(&seg,2,100,"Y res:")==0) return;
		newob = confirm_objectExists( &me, mat );
		if(newob) name = "Grid";
		undostr="Add Grid";
		break;
	case 11:	/* UVsphere */
		if(button(&seg,3,100,"Segments:")==0) return;
		if(button(&tot,3,100,"Rings:")==0) return;
		newob = confirm_objectExists( &me, mat );
		if(newob) name = "Sphere";
		undostr="Add UV Sphere";
		break;
	case 12:	/* Icosphere */
		if(button(&subdiv,1,5,"Subdivision:")==0) return;
		newob = confirm_objectExists( &me, mat );
		if(newob) name = "Sphere";
		undostr="Add Ico Sphere";
		break;
	case 13:	/* Monkey */
		newob = confirm_objectExists( &me, mat );
		if(newob) name = "Suzanne";
		undostr="Add Monkey";
		break;
	default:
		newob = confirm_objectExists( &me, mat );
		break;
	}

	if( name!=NULL ) {
		rename_id((ID *)G.obedit, name );
		rename_id((ID *)me, name );
	}

	curs= give_cursor();
	VECCOPY(cent, curs);
	cent[0]-= G.obedit->obmat[3][0];
	cent[1]-= G.obedit->obmat[3][1];
	cent[2]-= G.obedit->obmat[3][2];

	Mat3CpyMat4(imat, G.vd->viewmat);
	Mat3MulVecfl(imat, cent);
	Mat3MulMat3(cmat, imat, mat);
	Mat3Inv(imat,cmat);

	dia= G.vd->grid;
	if(type == 0 || type == 1) /* plane, cube (diameter of 1.41 makes it unit size) */
		dia *= sqrt(2.0);

	d= -G.vd->grid;
	phid= 2*M_PI/tot;
	phi= .25*M_PI;

	make_prim(type, imat, tot, seg, subdiv, dia, d,
        ext, fill, cent);

	if(type<2) tot = totoud;

	// simple selection flush OK, based on fact it's a single model
	EM_select_flush(); // flushes vertex -> edge -> face selection
	
	if(type!=0 && type!=13) righthandfaces(1);	// otherwise monkey has eyes in wrong direction...
	countall();

	allqueue(REDRAWINFO, 1); 	/* 1, because header->win==0! */	
	allqueue(REDRAWALL, 0);
	DAG_object_flush_update(G.scene, G.obedit, OB_RECALC_DATA);	

	/* if a new object was created, it stores it in Mesh, for reload original data and undo */
	if(newob) load_editMesh();	
	BIF_undo_push(undostr);
}

