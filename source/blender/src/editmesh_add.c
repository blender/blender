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

void addvert_mesh(void)
{
	EditMesh *em = G.editMesh;
	EditVert *eve,*v1=0;
	float *curs, mat[3][3],imat[3][3];

	// hurms, yah...
	if(G.scene->selectmode==SCE_SELECT_FACE) return;

	TEST_EDITMESH

	Mat3CpyMat4(mat, G.obedit->obmat);
	Mat3Inv(imat, mat);

	v1= em->verts.first;
	while(v1) {
		if(v1->f & SELECT) break;
		v1= v1->next;
	}
	eve= v1;

	/* prevent there are more selected */
	EM_clear_flag_all(SELECT);
	
	eve= addvertlist(0);
	
	curs= give_cursor();
	VECCOPY(eve->co, curs);
	VecSubf(eve->co, eve->co, G.obedit->obmat[3]);

	Mat3MulVecfl(imat, eve->co);
	eve->f= SELECT;

	if(v1) {
		addedgelist(v1, eve, NULL);
		v1->f= 0;
	}
	countall();

	BIF_undo_push("Add vertex");
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
		if(exist_face(neweve[0], neweve[1], neweve[2], 0)==0) {
			
			efa= addfacelist(neweve[0], neweve[1], neweve[2], 0, NULL, NULL);
			EM_select_face(efa, 1);
		}
		else error("The selected vertices already form a face");
	}
	else if(amount==4) {
		if(exist_face(neweve[0], neweve[1], neweve[2], neweve[3])==0) {
			int tria= 0;
			
			/* remove trias if they exist, 4 cases.... */
			if(exist_face(neweve[0], neweve[1], neweve[2], NULL)) tria++;
			if(exist_face(neweve[0], neweve[1], neweve[3], NULL)) tria++;
			if(exist_face(neweve[0], neweve[2], neweve[3], NULL)) tria++;
			if(exist_face(neweve[1], neweve[2], neweve[3], NULL)) tria++;
		
			if(tria==2) join_triangles();
			else {
				
				/* if 4 edges exist, we just create the face, convex or not */
				efa= addface_from_edges();
				if(efa==NULL) {
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
					else error("The selected vertices form a concave quad");
				}
			}

		}
		else error("The selected vertices already form a face");
	}
	
	if(efa) {	// now we're calculating direction of normal
		float inp;	
		/* dot product view mat with normal, should give info! */
	
		EM_select_face(efa, 1);
		CalcNormFloat(efa->v1->co, efa->v2->co, efa->v3->co, efa->n);

		inp= efa->n[0]*G.vd->viewmat[0][2] + efa->n[1]*G.vd->viewmat[1][2] + efa->n[2]*G.vd->viewmat[2][2];

		if(inp < 0.0) flipface(efa);
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



void add_primitiveMesh(int type)
{
	EditMesh *em = G.editMesh;
	Mesh *me;
	EditVert *eve, *v1=NULL, *v2, *v3, *v4=NULL, *vtop, *vdown;
	float *curs, d, dia, phi, phid, cent[3], vec[3], imat[3][3], mat[3][3];
	float q[4], cmat[3][3], nor[3]= {0.0, 0.0, 0.0};
	static short tot=32, seg=32, subdiv=2;
	short a, b, ext=0, fill=0, totoud, newob=0;
	char *undostr="Add Primitive";
	
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
	me= G.obedit->data;
	
	/* deselectall */
	EM_clear_flag_all(SELECT);

	totoud= tot; /* store, and restore when cube/plane */
	
	/* imat and centre and size */
	Mat3CpyMat4(mat, G.obedit->obmat);

	curs= give_cursor();
	VECCOPY(cent, curs);
	cent[0]-= G.obedit->obmat[3][0];
	cent[1]-= G.obedit->obmat[3][1];
	cent[2]-= G.obedit->obmat[3][2];

	if(type!= 31) {
		Mat3CpyMat4(imat, G.vd->viewmat);
		Mat3MulVecfl(imat, cent);
		Mat3MulMat3(cmat, imat, mat);
		Mat3Inv(imat,cmat);
	} else {
		Mat3Inv(imat, mat);
	}
	
	/* ext==extrudeflag, tot==amount of vertices in basis */

	switch(type) {
	case 0:		/* plane */
		tot= 4;
		ext= 0;
		fill= 1;
		if(newob) rename_id((ID *)G.obedit, "Plane");
		if(newob) rename_id((ID *)me, "Plane");
		undostr="Add Plane";
		break;
	case 1:		/* cube  */
		tot= 4;
		ext= 1;
		fill= 1;
		if(newob) rename_id((ID *)G.obedit, "Cube");
		if(newob) rename_id((ID *)me, "Cube");
		undostr="Add Cube";
		break;
	case 4:		/* circle  */
		if(button(&tot,3,100,"Vertices:")==0) return;
		ext= 0;
		fill= 0;
		if(newob) rename_id((ID *)G.obedit, "Circle");
		if(newob) rename_id((ID *)me, "Circle");
		undostr="Add Circle";
		break;
	case 5:		/* cylinder  */
		if(button(&tot,3,100,"Vertices:")==0) return;
		ext= 1;
		fill= 1;
		if(newob) rename_id((ID *)G.obedit, "Cylinder");
		if(newob) rename_id((ID *)me, "Cylinder");
		undostr="Add Cylinder";
		break;
	case 6:		/* tube  */
		if(button(&tot,3,100,"Vertices:")==0) return;
		ext= 1;
		fill= 0;
		if(newob) rename_id((ID *)G.obedit, "Tube");
		if(newob) rename_id((ID *)me, "Tube");
		undostr="Add Tube";
		break;
	case 7:		/* cone  */
		if(button(&tot,3,100,"Vertices:")==0) return;
		ext= 0;
		fill= 1;
		if(newob) rename_id((ID *)G.obedit, "Cone");
		if(newob) rename_id((ID *)me, "Cone");
		undostr="Add Cone";
		break;
	case 10:	/* grid */
		if(button(&tot,2,100,"X res:")==0) return;
		if(button(&seg,2,100,"Y res:")==0) return;
		if(newob) rename_id((ID *)G.obedit, "Grid");
		if(newob) rename_id((ID *)me, "Grid");
		undostr="Add Grid";
		break;
	case 11:	/* UVsphere */
		if(button(&seg,3,100,"Segments:")==0) return;
		if(button(&tot,3,100,"Rings:")==0) return;
		if(newob) rename_id((ID *)G.obedit, "Sphere");
		if(newob) rename_id((ID *)me, "Sphere");
		undostr="Add UV Sphere";
		break;
	case 12:	/* Icosphere */
		if(button(&subdiv,1,5,"Subdivision:")==0) return;
		if(newob) rename_id((ID *)G.obedit, "Sphere");
		if(newob) rename_id((ID *)me, "Sphere");
		undostr="Add Ico Sphere";
		break;
	case 13:	/* Monkey */
		if(newob) rename_id((ID *)G.obedit, "Suzanne");
		if(newob) rename_id((ID *)me, "Suzanne");
		undostr="Add Monkey";
		break;
	}

	dia= sqrt(2.0)*G.vd->grid;
	d= -G.vd->grid;
	phid= 2*M_PI/tot;
	phi= .25*M_PI;


	if(type<10) {	/* all types except grid, sphere... */
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
		
		if(type<2) tot= totoud;
		
	}
	else if(type==10) {	/*  grid */
		/* clear flags */
		eve= em->verts.first;
		while(eve) {
			eve->f= 0;
			eve= eve->next;
		}
		dia= G.vd->grid;
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
	}
	else if(type==11) {	/*  UVsphere */
		
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
	}
	else if(type==12) {	/* Icosphere */
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
		
		
	} else if (type==13) {	/* Monkey */
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

