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
 * The Original Code is Copyright (C) 2005 Blender Foundation
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

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_armature.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BIF_editarmature.h"
#include "BIF_gl.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_transform.h"

#include "BSE_edit.h"
#include "BSE_view.h"
#include "BDR_drawobject.h"

#include "blendef.h"
#include "transform.h"
#include "transform_generics.h"

/* return codes for select */

#define MAN_TRANS_X		1
#define MAN_TRANS_Y		2
#define MAN_TRANS_Z		4
#define MAN_TRANS_C		7

#define MAN_ROT_X		8
#define MAN_ROT_Y		16
#define MAN_ROT_Z		32
#define MAN_ROT_V		64
#define MAN_ROT_T		128
#define MAN_ROT_C		248

#define MAN_SCALE_X		256
#define MAN_SCALE_Y		512
#define MAN_SCALE_Z		1024
#define MAN_SCALE_C		1792


/* GLOBAL VARIABLE THAT SHOULD MOVED TO SCREEN MEMBER OR SOMETHING  */
extern TransInfo Trans;


/* transform widget center calc helper for below */
static void calc_tw_center(float *co)
{
	float *twcent= G.scene->twcent;
	float *min= G.scene->twmin;
	float *max= G.scene->twmax;
	
	DO_MINMAX(co, min, max);
	VecAddf(twcent, twcent, co);
}

/* callback */
static void stats_pose(ListBase *lb)
{
	Bone *bone;
	float vec[3];
	
	for(bone= lb->first; bone; bone= bone->next) {
		if (bone->flag & BONE_SELECTED) {
			/* We don't let IK children get "grabbed" */
			/* ALERT! abusive global Trans here */
			if ( (Trans.mode!=TFM_TRANSLATION) || (bone->flag & BONE_IK_TOPARENT)==0 ) {
				
				get_bone_root_pos (bone, vec, 1);
				
				calc_tw_center(vec);
				return;	// see above function
			}
		}
		stats_pose(&bone->childbase);
	}
}


/* centroid, boundbox, of selection */
/* returns total items selected */
static int calc_manipulator(ScrArea *sa)
{
	extern ListBase editNurb;
	View3D *v3d= sa->spacedata.first;
	Base *base;
	Object *ob=NULL;
	Nurb *nu;
	BezTriple *bezt;
	BPoint *bp;
	MetaElem *ml;
	EditVert *eve;
//	EditFace *efa;
	EditBone *ebo;
	int a, totsel=0;
	
	/* transform widget matrix */
	Mat4One(v3d->twmat);
	
	/* transform widget centroid/center */
	G.scene->twcent[0]= G.scene->twcent[1]= G.scene->twcent[2]= 0.0f;
	INIT_MINMAX(G.scene->twmin, G.scene->twmax);
	
	if(G.obedit) {
		ob= G.obedit;
		
		if(G.obedit->type==OB_MESH) {
			EditMesh *em = G.editMesh;
			eve= em->verts.first;
			while(eve) {
				if(eve->f & SELECT) {
					totsel++;
					calc_tw_center(eve->co);
				}
				eve= eve->next;
			}
		}
		else if (G.obedit->type==OB_ARMATURE){
			for (ebo=G.edbo.first;ebo;ebo=ebo->next){
				
				//	If this is an IK child and it's parent is being moved, don't count as selected
				if ((ebo->flag & BONE_IK_TOPARENT)&& (ebo->flag & BONE_ROOTSEL) && ebo->parent && (ebo->parent->flag & BONE_TIPSEL));
				else {
					if (ebo->flag & BONE_TIPSEL) {
						calc_tw_center(ebo->tail);
						totsel++;
					}
					if (ebo->flag & BONE_ROOTSEL) {
						calc_tw_center(ebo->head);
						totsel++;
					}
				}
			}
		}
		else if ELEM3(G.obedit->type, OB_CURVE, OB_SURF, OB_FONT) {
			nu= editNurb.first;
			while(nu) {
				if((nu->type & 7)==CU_BEZIER) {
					bezt= nu->bezt;
					a= nu->pntsu;
					while(a--) {
						if(bezt->f1) {
							calc_tw_center(bezt->vec[0]);
							totsel++;
						}
						if(bezt->f2) {
							calc_tw_center(bezt->vec[1]);
							totsel++;
						}
						if(bezt->f3) {
							calc_tw_center(bezt->vec[2]);
							totsel++;
						}
						bezt++;
					}
				}
				else {
					bp= nu->bp;
					a= nu->pntsu*nu->pntsv;
					while(a--) {
						if(bp->f1 & 1) {
							calc_tw_center(bp->vec);
							totsel++;
						}
						bp++;
					}
				}
				nu= nu->next;
			}
		}
		else if(G.obedit->type==OB_MBALL) {
			/* editmball.c */
			extern ListBase editelems;  /* go away ! */
			
			ml= editelems.first;
			while(ml) {
				if(ml->flag & SELECT) {
					calc_tw_center(&ml->x);
					totsel++;
				}
				ml= ml->next;
			}
		}
		else if(G.obedit->type==OB_LATTICE) {
			bp= editLatt->def;
			
			a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
			while(a--) {
				if(bp->f1 & 1) {
					calc_tw_center(bp->vec);
					totsel++;
				}
				bp++;
			}
		}
		
		/* selection center */
		if(totsel) {
			VecMulf(G.scene->twcent, 1.0f/(float)totsel);	// centroid!
			Mat4MulVecfl(G.obedit->obmat, G.scene->twcent);
			Mat4MulVecfl(G.obedit->obmat, G.scene->twmin);
			Mat4MulVecfl(G.obedit->obmat, G.scene->twmax);
		}
	}
	else if(G.obpose) {
		bArmature *arm= G.obpose->data;
		
		/* count total */
		count_bone_select(&arm->bonebase, &totsel);
		if(totsel) {
			/* recursive get stats */
			stats_pose(&arm->bonebase);
			
			VecMulf(G.scene->twcent, 1.0f/(float)totsel);	// centroid!
			Mat4MulVecfl(G.obpose->obmat, G.scene->twcent);
			Mat4MulVecfl(G.obpose->obmat, G.scene->twmin);
			Mat4MulVecfl(G.obpose->obmat, G.scene->twmax);
		}
	}
	else if(G.f & (G_FACESELECT + G_VERTEXPAINT + G_TEXTUREPAINT +G_WEIGHTPAINT)) {
		;
	}
	else {
		
		/* we need the one selected object */
		ob= OBACT;
		if(ob && !(ob->flag & SELECT)) ob= NULL;
		
		base= (G.scene->base.first);
		while(base) {
			if(v3d->lay & base->lay) {
				
				if(base->flag & SELECT) {
					if(ob==NULL) ob= base->object;
					calc_tw_center(base->object->obmat[3]);
					totsel++;
				}
			}
			base= base->next;
		}
		
		/* selection center */
		if(totsel) {
			VecMulf(G.scene->twcent, 1.0f/(float)totsel);	// centroid!
		}
	}
	
	/* global, local or normal orientation? */
	if(ob) {
		// local....
		if(totsel==1 || v3d->around==V3D_LOCAL || G.obedit || G.obpose) {
			Mat4CpyMat4(v3d->twmat, ob->obmat);
			Mat4Ortho(v3d->twmat);
		}		
	}
	   
	return totsel;
}

/* ******************** DRAWING STUFFIES *********** */

/* radring = radius of donut rings
   radhole = radius hole
   start = starting segment (based on nrings)
   end   = end segment
   nsides = amount of points in ring
   nrigns = amount of rings
*/
static void partial_donut(float radring, float radhole, int start, int end, int nsides, int nrings)
{
	float theta, phi, theta1;
	float cos_theta, sin_theta;
	float cos_theta1, sin_theta1;
	float ring_delta, side_delta;
	int i, j;
	
	ring_delta= 2.0*M_PI/(float)nrings;
	side_delta= 2.0*M_PI/(float)nsides;
	
	theta= M_PI+0.5*ring_delta;
	cos_theta= cos(theta);
	sin_theta= sin(theta);
	
	for(i= nrings - 1; i >= 0; i--) {
		theta1= theta + ring_delta;
		cos_theta1= cos(theta1);
		sin_theta1= sin(theta1);
		
		if(i==start) {	// cap
			glBegin(GL_POLYGON);
			glNormal3f(-sin_theta1, -cos_theta1, 0.0);
			phi= 0.0;
			for(j= nsides; j >= 0; j--) {
				float cos_phi, sin_phi, dist;
				
				phi += side_delta;
				cos_phi= cos(phi);
				sin_phi= sin(phi);
				dist= radhole + radring * cos_phi;
				
				glVertex3f(cos_theta1 * dist, -sin_theta1 * dist,  radring * sin_phi);
			}
			glEnd();
		}
		if(i>=start && i<=end) {
			glBegin(GL_QUAD_STRIP);
			phi= 0.0;
			for(j= nsides; j >= 0; j--) {
				float cos_phi, sin_phi, dist;
				
				phi += side_delta;
				cos_phi= cos(phi);
				sin_phi= sin(phi);
				dist= radhole + radring * cos_phi;
				
				glNormal3f(cos_theta1 * cos_phi, -sin_theta1 * cos_phi, sin_phi);
				glVertex3f(cos_theta1 * dist, -sin_theta1 * dist, radring * sin_phi);
				glNormal3f(cos_theta * cos_phi, -sin_theta * cos_phi, sin_phi);
				glVertex3f(cos_theta * dist, -sin_theta * dist,  radring * sin_phi);
			}
			glEnd();
		}
		
		if(i==end) {	// cap
			glBegin(GL_POLYGON);
			glNormal3f(sin_theta, cos_theta, 0.0);
			phi= 0.0;
			for(j= nsides; j >= 0; j--) {
				float cos_phi, sin_phi, dist;
				
				phi -= side_delta;
				cos_phi= cos(phi);
				sin_phi= sin(phi);
				dist= radhole + radring * cos_phi;
				
				glVertex3f(cos_theta * dist, -sin_theta * dist,  radring * sin_phi);
			}
			glEnd();
		}
		
		
		theta= theta1;
		cos_theta= cos_theta1;
		sin_theta= sin_theta1;
	}
}

/* three colors can be set;
   GL_BLEND: grey for ghosting
   G.moving: in transform theme color
   else the red/green/blue
*/
static void manipulator_setcolor(char mode)
{
	float vec[4];
	
	vec[3]= 1.0;
	
	if(glIsEnabled(GL_BLEND)) {
		if(mode > 'Z') glColor4ub(0, 0, 0, 70);
		else {
			vec[0]= vec[1]= vec[2]= 1.0; vec[3]= 0.3;
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, vec);
		}
	}
	else if(G.moving) {
		if(mode > 'Z') BIF_ThemeColor(TH_TRANSFORM);
		else {
			BIF_GetThemeColor3fv(TH_TRANSFORM, vec);
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, vec);
		}
	}
	else {
		switch(mode) {
		case 'x':
			glColor3ub(255, 0, 100);
			break;
		case 'y':
			glColor3ub(100, 255, 100);
			break;
		case 'z':
			glColor3ub(50, 50, 225);
			break;
		case 'X':
			vec[2]= vec[1]= 0.0; vec[0]= 1.0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, vec);
			break;
		case 'Y':
			vec[0]= vec[2]= 0.0; vec[1]= 1.0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, vec);
			break;
		case 'Z':
			vec[0]= vec[1]= 0.0; vec[2]= 1.0;
			glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, vec);
			break;
		}
	}
}

static int Gval= 0xFFFF;	// defines drawmodus while moving...

/* viewmatrix should have been set OK, also no shademode! */
static void draw_manipulator_axes(int flagx, int flagy, int flagz)
{
	
	/* axes */
	if(Gval & flagx) {
		manipulator_setcolor('x');
		glBegin(GL_LINES);
		glVertex3f(0.0, 0.0, 0.0);
		glVertex3f(1.0, 0.0, 0.0);
		glEnd();
	}		
	if(Gval & flagy) {
		manipulator_setcolor('y');
		glBegin(GL_LINES);
		glVertex3f(0.0, 0.0, 0.0);
		glVertex3f(0.0, 1.0, 0.0);
		glEnd();
	}		
	if(Gval & flagz) {
		manipulator_setcolor('z');
		glBegin(GL_LINES);
		glVertex3f(0.0, 0.0, 0.0);
		glVertex3f(0.0, 0.0, 1.0);
		glEnd();
	}
}


/* only called while G.moving */
static void draw_manipulator_rotate_ghost(float mat[][4])
{
	GLUquadricObj *qobj= gluNewQuadric(); 
	float phi, vec[3], matt[4][4], cross[3];
	
	glDisable(GL_DEPTH_TEST);
	gluQuadricDrawStyle(qobj, GLU_FILL); 
	
	/* we need both [4][4] transforms, Trans.mat seems to be premul, not post for mat[][4] */
	Mat4CpyMat4(matt, mat); // to copy the parts outside of [3][3]
	Mat4MulMat34(matt, Trans.mat, mat);
	
	mymultmatrix(mat);	// aligns with original widget
	
	glColor4ub(0,0,0,64);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	/* Z disk */
	if(Gval & MAN_ROT_Z) {
		VECCOPY(vec, mat[0]);	// use x axis to detect rotation
		Normalise(vec);
		Normalise(matt[0]);
		phi= saacos( Inpf(vec, matt[0]) );
		if(phi!=0.0) {
			Crossf(cross, vec, matt[0]);	// results in z vector
			if(Inpf(cross, mat[2]) > 0.0) phi= -phi;
			gluPartialDisk(qobj, 0.0, 1.0, 32, 1, 90.0, 180.0*phi/M_PI);
		}
	}
	/* X disk */
	if(Gval & MAN_ROT_X) {
		VECCOPY(vec, mat[1]);	// use y axis to detect rotation
		Normalise(vec);
		Normalise(matt[1]);
		phi= saacos( Inpf(vec, matt[1]) );
		if(phi!=0.0) {
			Crossf(cross, vec, matt[1]);	// results in x vector
			if(Inpf(cross, mat[0]) > 0.0) phi= -phi;
			glRotatef(90.0, 0.0, 1.0, 0.0);
			gluPartialDisk(qobj, 0.0, 1.0, 32, 1, 0.0, 180.0*phi/M_PI);
			glRotatef(-90.0, 0.0, 1.0, 0.0);
		}
	}	
	/* Y circle */
	if(Gval & MAN_ROT_Y) {
		VECCOPY(vec, mat[2]);	// use z axis to detect rotation
		Normalise(vec);
		Normalise(matt[2]);
		phi= saacos( Inpf(vec, matt[2]) );
		if(phi!=0.0) {
			Crossf(cross, vec, matt[2]);	// results in y vector
			if(Inpf(cross, mat[1]) > 0.0) phi= -phi;
			glRotatef(-90.0, 1.0, 0.0, 0.0);
			gluPartialDisk(qobj, 0.0, 1.0, 32, 1, 180.0, 180.0*phi/M_PI);
			glRotatef(90.0, 1.0, 0.0, 0.0);
		}
	}
	
	glDisable(GL_BLEND);
	myloadmatrix(G.vd->viewmat);
}

static void draw_manipulator_rotate(float mat[][4])
{
	GLUquadricObj *qobj= gluNewQuadric(); 
	double plane[4];
	float size, vec[3], unitmat[4][4];
	float cywid= 0.33f*0.01f*(float)U.tw_handlesize;	
	float cusize= cywid*0.75;
	int arcs= (G.rt==2);
	
	if(G.rt==3) cusize= cywid*0.25;
	
	/* when called while moving in mixed mode, do not draw when... */
	if((Gval & MAN_ROT_C)==0) return;
	
	/* Init stuff */
	glDisable(GL_DEPTH_TEST);
	Mat4One(unitmat);
	gluQuadricDrawStyle(qobj, GLU_FILL); 
	gluQuadricNormals(qobj, GLU_SMOOTH);
	glEnable(GL_CULL_FACE);		// backface removal
	glShadeModel(GL_SMOOTH);
	
	
	/* prepare for screen aligned draw */
	VECCOPY(vec, mat[0]);
	size= Normalise(vec);
	glPushMatrix();
	glTranslatef(mat[3][0], mat[3][1], mat[3][2]);
	
	if(arcs) {
		/* clipplane makes nice handles, calc here because of multmatrix but with translate! */
		VECCOPY(plane, G.vd->viewinv[2]);
		plane[3]= -0.1; // clip more
		glClipPlane(GL_CLIP_PLANE0, plane);
	}
	/* sets view screen aligned */
	glRotatef( -360.0*saacos(G.vd->viewquat[0])/M_PI, G.vd->viewquat[1], G.vd->viewquat[2], G.vd->viewquat[3]);
	
	/* Screen aligned help circle */
	if(arcs) {
		if((G.f & G_PICKSEL)==0) {
			BIF_ThemeColorShade(TH_BACK, -30);
			drawcircball(unitmat[3], size, unitmat);
		}
	}
	/* Screen aligned view rot circle */
	if(Gval & MAN_ROT_V) {
		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_V);
		BIF_ThemeColor(TH_TRANSFORM);
		drawcircball(unitmat[3], 1.2*size, unitmat);
		
		if(G.moving) {	
			float vec[3];
			vec[0]= Trans.imval[0] - Trans.center2d[0];
			vec[1]= Trans.imval[1] - Trans.center2d[1];
			vec[2]= 0.0;
			Normalise(vec);
			VecMulf(vec, 1.2*size);
			glBegin(GL_LINES);
			glVertex3f(0.0, 0.0, 0.0);
			glVertex3fv(vec);
			glEnd();
		}
	}
	glPopMatrix();
	
	/* apply the transform delta */
	if(G.moving) {
		float matt[4][4];
		Mat4CpyMat4(matt, mat); // to copy the parts outside of [3][3]
		Mat4MulMat34(matt, Trans.mat, mat);
		mymultmatrix(matt);
	}
	else mymultmatrix(mat);
	
	/* axes */
	if(arcs==0) {
		if(!(G.f & G_PICKSEL)) {
			/* axis */
			glBegin(GL_LINES);
			if( (Gval & MAN_ROT_X) || (G.moving && (Gval & MAN_ROT_Z)) ) {
				manipulator_setcolor('x');
				glVertex3f(0.0, 0.0, 0.0);
				glVertex3f(1.0, 0.0, 0.0);
			}		
			if( (Gval & MAN_ROT_Y) || (G.moving && (Gval & MAN_ROT_X)) ) {
				manipulator_setcolor('y');
				glVertex3f(0.0, 0.0, 0.0);
				glVertex3f(0.0, 1.0, 0.0);
			}		
			if( (Gval & MAN_ROT_Z) || (G.moving && (Gval & MAN_ROT_Y)) ) {
				manipulator_setcolor('z');
				glVertex3f(0.0, 0.0, 0.0);
				glVertex3f(0.0, 0.0, 1.0);
			}
			glEnd();
		}
	}
	
	/* Trackball center */
	if(Gval & MAN_ROT_T) {
		float smat[3][3], imat[3][3];
		float offset[3];
		
		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_T);
		
		Mat3CpyMat4(smat, mat);
		Mat3Inv(imat, smat);

		getViewVector(mat[3], offset);
		Mat3MulVecfl(imat, offset);
		Normalise(offset);	// matrix space is such that 1.0 = size of sphere
		
		BIF_ThemeColor(TH_TRANSFORM);
		glBegin(GL_LINES);
		glVertex3f(0.0, 0.0, 0.0);
		glVertex3fv(offset);
		glEnd();
		
		glEnable(GL_LIGHTING);
		BIF_GetThemeColor3fv(TH_TRANSFORM, vec);
		glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, vec);
		
		VECCOPY(vec, offset);
		glTranslatef(vec[0], vec[1], vec[2]);
		gluSphere(qobj, cywid, 8, 6);
		
		/* restore */
		glTranslatef(-vec[0], -vec[1], -vec[2]);
		glDisable(GL_LIGHTING);
	}
	
	if(arcs==0 && G.moving) {
		
		if(arcs) glEnable(GL_CLIP_PLANE0);

		/* Z circle */
		if(Gval & MAN_ROT_Z) {
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Z);
			manipulator_setcolor('z');
			drawcircball(unitmat[3], 1.0, unitmat);
		}
		/* X circle */
		if(Gval & MAN_ROT_X) {
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_X);
			glRotatef(90.0, 0.0, 1.0, 0.0);
			manipulator_setcolor('x');
			drawcircball(unitmat[3], 1.0, unitmat);
			glRotatef(-90.0, 0.0, 1.0, 0.0);
		}	
		/* Y circle */
		if(Gval & MAN_ROT_Y) {
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Y);
			glRotatef(-90.0, 1.0, 0.0, 0.0);
			manipulator_setcolor('y');
			drawcircball(unitmat[3], 1.0, unitmat);
			glRotatef(90.0, 1.0, 0.0, 0.0);
		}
		
		if(arcs) glDisable(GL_CLIP_PLANE0);
	}
	// donut arcs
	if(arcs) {
		glEnable(GL_LIGHTING);
		glEnable(GL_CLIP_PLANE0);
		
		/* Z circle */
		if(Gval & MAN_ROT_Z) {
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Z);
			manipulator_setcolor('Z');
			partial_donut(cusize/3.0, 1.0, 0, 48, 8, 48);
		}
		/* X circle */
		if(Gval & MAN_ROT_X) {
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_X);
			glRotatef(90.0, 0.0, 1.0, 0.0);
			manipulator_setcolor('X');
			partial_donut(cusize/3.0, 1.0, 0, 48, 8, 48);
			glRotatef(-90.0, 0.0, 1.0, 0.0);
		}	
		/* Y circle */
		if(Gval & MAN_ROT_Y) {
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Y);
			glRotatef(-90.0, 1.0, 0.0, 0.0);
			manipulator_setcolor('Y');
			partial_donut(cusize/3.0, 1.0, 0, 48, 8, 48);
			glRotatef(90.0, 1.0, 0.0, 0.0);
		}
		
		glDisable(GL_CLIP_PLANE0);
	}
	
	if(arcs==0) {
		glEnable(GL_LIGHTING);
		
		/* Z handle on X axis */
		if(Gval & MAN_ROT_Z) {
			glPushMatrix();
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Z);
			manipulator_setcolor('Z');

			if(G.rt==3) {
				partial_donut(cusize, 1.0, 21, 27, 8, 48);

				/* Z handle on Y axis */
				glRotatef(90.0, 0.0, 0.0, 1.0);
				partial_donut(cusize, 1.0, 21, 27, 8, 48);
			}
			else {
				partial_donut(cusize, 1.0, 23, 25, 8, 48);
			}
			glPopMatrix();
		}	

		/* Y handle on X axis */
		if(Gval & MAN_ROT_Y) {
			glPushMatrix();
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Y);
			manipulator_setcolor('Y');
			
			if(G.rt==3) {
				glRotatef(90.0, 1.0, 0.0, 0.0);
				partial_donut(cusize, 1.0, 21, 27, 8, 48);
				
				/* Y handle on Z axis */
				glRotatef(90.0, 0.0, 0.0, 1.0);
				partial_donut(cusize, 1.0, 21, 27, 8, 48);
			}
			else {
				glRotatef(90.0, 1.0, 0.0, 0.0);
				glRotatef(90.0, 0.0, 0.0, 1.0);
				partial_donut(cusize, 1.0, 23, 25, 8, 48);
			}
			
			glPopMatrix();
		}
		
		/* X handle on Z axis */
		if(Gval & MAN_ROT_X) {
			glPushMatrix();
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_X);
			manipulator_setcolor('X');
			
			if(G.rt==3) {
				glRotatef(-90.0, 0.0, 1.0, 0.0);
				partial_donut(cusize, 1.0, 21, 27, 8, 48);
				
				/* X handle on Y axis */
				glRotatef(90.0, 0.0, 0.0, 1.0);
				partial_donut(cusize, 1.0, 21, 27, 8, 48);
			}
			else {
				glRotatef(-90.0, 0.0, 1.0, 0.0);
				glRotatef(90.0, 0.0, 0.0, 1.0);
				partial_donut(cusize, 1.0, 23, 25, 8, 48);
			}
			glPopMatrix();
		}
		
	}
	
	/* restore */
	myloadmatrix(G.vd->viewmat);
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	gluDeleteQuadric(qobj);
	if(G.zbuf) glEnable(GL_DEPTH_TEST);		// shouldn't be global, tsk!
	
}

/* only called while G.moving */
static void draw_manipulator_scale_ghost(float mat[][4])
{
	float cywid= 0.33f*0.01f*(float)U.tw_handlesize;	
	float cusize= cywid*0.75;
	float vec[4];
	
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_CULL_FACE);		// backface removal
	glEnable(GL_LIGHTING);
	glShadeModel(GL_SMOOTH);
	
	mymultmatrix(mat);	// aligns with original widget
	
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	draw_manipulator_axes(MAN_SCALE_X, MAN_SCALE_Y, MAN_SCALE_Z);
	
	vec[0]= vec[1]= vec[2]= 1.0; vec[3]= 0.3;
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, vec);

	/* Z cube */
	glTranslatef(0.0, 0.0, 1.0+cusize/2);
	if(Gval & MAN_SCALE_Z) {
		drawsolidcube(cusize);
	}	
	/* X cube */
	glTranslatef(1.0+cusize/2, 0.0, -(1.0+cusize/2));
	if(Gval & MAN_SCALE_X) {
		drawsolidcube(cusize);
	}	
	/* Y cube */
	glTranslatef(-(1.0+cusize/2), 1.0+cusize/2, 0.0);
	if(Gval & MAN_SCALE_Y) {
		drawsolidcube(cusize);
	}
	
	/* restore */
	glDisable(GL_BLEND);
	myloadmatrix(G.vd->viewmat);
	if(G.zbuf) glEnable(GL_DEPTH_TEST);		// shouldn't be global, tsk!
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
}	

static void draw_manipulator_scale(float mat[][4])
{
	float cywid= 0.33f*0.01f*(float)U.tw_handlesize;	
	float cusize= cywid*0.75;
	float vec[3];
	
	/* when called while moving in mixed mode, do not draw when... */
	if((Gval & MAN_SCALE_C)==0) return;
	
	if(G.moving) {
		float matt[4][4];
		
		Mat4CpyMat4(matt, mat); // to copy the parts outside of [3][3]
		Mat4MulMat34(matt, Trans.mat, mat);
		mymultmatrix(matt);
	}
	else mymultmatrix(mat);

	/* axis */
	if( (G.f & G_PICKSEL)==0 ) {
		
		glDisable(GL_DEPTH_TEST);
		
		draw_manipulator_axes(MAN_SCALE_X, MAN_SCALE_Y, MAN_SCALE_Z);

		/* only has to be set when not in picking */
		glEnable(GL_CULL_FACE);		// backface removal
		glEnable(GL_LIGHTING);
		glShadeModel(GL_SMOOTH);
	}
	
	/* center cube, do not add to selection when shift is pressed (planar constraint)  */
	if( (G.f & G_PICKSEL) && (G.qual & LR_SHIFTKEY)==0) glLoadName(MAN_SCALE_C);
	BIF_GetThemeColor3fv(TH_TRANSFORM, vec);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, vec);
	
	drawsolidcube(cusize);
	
	/* Z cube */
	glTranslatef(0.0, 0.0, 1.0+cusize/2);
	if(Gval & MAN_SCALE_Z) {
		if(G.f & G_PICKSEL) glLoadName(MAN_SCALE_Z);
		manipulator_setcolor('Z');
		drawsolidcube(cusize);
	}	
	/* X cube */
	glTranslatef(1.0+cusize/2, 0.0, -(1.0+cusize/2));
	if(Gval & MAN_SCALE_X) {
		if(G.f & G_PICKSEL) glLoadName(MAN_SCALE_X);
		manipulator_setcolor('X');
		drawsolidcube(cusize);
	}	
	/* Y cube */
	glTranslatef(-(1.0+cusize/2), 1.0+cusize/2, 0.0);
	if(Gval & MAN_SCALE_Y) {
		if(G.f & G_PICKSEL) glLoadName(MAN_SCALE_Y);
		manipulator_setcolor('Y');
		drawsolidcube(cusize);
	}
	
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	
	/* if shiftkey, center point as last, for selectbuffer order */
	if(G.f & G_PICKSEL) {
		if(G.qual & LR_SHIFTKEY) {
			glTranslatef(0.0, -(1.0+cusize/2), 0.0);
			glLoadName(MAN_SCALE_C);
			glBegin(GL_POINTS);
			glVertex3f(0.0, 0.0, 0.0);
			glEnd();
		}
	}
	
	myloadmatrix(G.vd->viewmat);
	
	if(G.zbuf) glEnable(GL_DEPTH_TEST);		// shouldn't be global, tsk!
}

static void draw_cone(GLUquadricObj *qobj, float len, float width)
{
	gluCylinder(qobj, width, 0.0, len, 8, 1);
	gluQuadricOrientation(qobj, GLU_INSIDE);
	gluDisk(qobj, 0.0, width, 8, 1); 
	gluQuadricOrientation(qobj, GLU_OUTSIDE);
}

/* only called while G.moving */
static void draw_manipulator_translate_ghost(float mat[][4])
{
	GLUquadricObj *qobj = gluNewQuadric(); 
	float vec[4];
	float cylen= 0.01f*(float)U.tw_handlesize;
	float cywid= 0.33f*cylen;
	
	glDisable(GL_DEPTH_TEST);
	
	mymultmatrix(mat);	// aligns with original widget
	
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
	
	draw_manipulator_axes(MAN_TRANS_X, MAN_TRANS_Y, MAN_TRANS_Z);
	
	glEnable(GL_CULL_FACE);		// backface removal
	glEnable(GL_LIGHTING);
	glShadeModel(GL_SMOOTH);
	vec[0]= vec[1]= vec[2]= 1.0; vec[3]= 0.3;
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, vec);

	/* center sphere */
	gluSphere(qobj, cywid, 8, 6); 
	
	/* Z Cone */
	glTranslatef(0.0, 0.0, 1.0 - cylen);
	if(Gval & MAN_TRANS_Z) {
		draw_cone(qobj, cylen, cywid);
	}	
	/* X Cone */
	glTranslatef(1.0 - cylen, 0.0, -(1.0 - cylen));
	if(Gval & MAN_TRANS_X) {
		glRotatef(90.0, 0.0, 1.0, 0.0);
		draw_cone(qobj, cylen, cywid);
		glRotatef(-90.0, 0.0, 1.0, 0.0);
	}	
	/* Y Cone */
	glTranslatef(-(1.0 - cylen), 1.0 - cylen, 0.0);
	if(Gval & MAN_TRANS_Y) {
		glRotatef(-90.0, 1.0, 0.0, 0.0);
		draw_cone(qobj, cylen, cywid);
	}
	
	/* restore */
	glDisable(GL_BLEND);
	myloadmatrix(G.vd->viewmat);
	if(G.zbuf) glEnable(GL_DEPTH_TEST);		// shouldn't be global, tsk!
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
}	



static void draw_manipulator_translate(float mat[][4])
{
	GLUquadricObj *qobj = gluNewQuadric(); 
	float vec[3];
	float cylen= 0.01f*(float)U.tw_handlesize;
	float cywid= 0.33f*cylen;
	
	/* when called while moving in mixed mode, do not draw when... */
	if((Gval & MAN_TRANS_C)==0) return;
	
	if(G.moving) glTranslatef(Trans.vec[0], Trans.vec[1], Trans.vec[2]);
	
	mymultmatrix(mat);

	glDisable(GL_DEPTH_TEST);
	
	/* axis */
	if( (G.f & G_PICKSEL)==0 ) {
		draw_manipulator_axes(MAN_TRANS_X, MAN_TRANS_Y, MAN_TRANS_Z);
	
		/* only has to be set when not in picking */
		gluQuadricDrawStyle(qobj, GLU_FILL); 
		gluQuadricNormals(qobj, GLU_SMOOTH);
		glEnable(GL_CULL_FACE);		// backface removal
		glEnable(GL_LIGHTING);
		glShadeModel(GL_SMOOTH);
	}
	
	/* center sphere, do not add to selection when shift is pressed (planar constraint) */
	if( (G.f & G_PICKSEL) && (G.qual & LR_SHIFTKEY)==0) glLoadName(MAN_TRANS_C);
	BIF_GetThemeColor3fv(TH_TRANSFORM, vec);
	glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, vec);

	gluSphere(qobj, cywid, 8, 6); 
	
	/* Z Cone */
	glTranslatef(0.0, 0.0, 1.0 - cylen);
	if(Gval & MAN_TRANS_Z) {
		if(G.f & G_PICKSEL) glLoadName(MAN_TRANS_Z);
		manipulator_setcolor('Z');
		draw_cone(qobj, cylen, cywid);
	}	
	/* X Cone */
	glTranslatef(1.0 - cylen, 0.0, -(1.0 - cylen));
	if(Gval & MAN_TRANS_X) {
		if(G.f & G_PICKSEL) glLoadName(MAN_TRANS_X);
		glRotatef(90.0, 0.0, 1.0, 0.0);
		manipulator_setcolor('X');
		draw_cone(qobj, cylen, cywid);
		glRotatef(-90.0, 0.0, 1.0, 0.0);
	}	
	/* Y Cone */
	glTranslatef(-(1.0 - cylen), 1.0 - cylen, 0.0);
	if(Gval & MAN_TRANS_Y) {
		if(G.f & G_PICKSEL) glLoadName(MAN_TRANS_Y);
		glRotatef(-90.0, 1.0, 0.0, 0.0);
		manipulator_setcolor('Y');
		draw_cone(qobj, cylen, cywid);
	}
	
	glDisable(GL_CULL_FACE);
	glDisable(GL_LIGHTING);
	
	/* if shiftkey, center point as last, for selectbuffer */
	if(G.f & G_PICKSEL) {
		if(G.qual & LR_SHIFTKEY) {
			glRotatef(90.0, 1.0, 0.0, 0.0);
			glTranslatef(0.0, -(1.0 - cylen), 0.0);
			//glLoadName(MAN_TRANS_C);
			//glBegin(GL_POINTS);
			//glVertex3f(0.0, 0.0, 0.0);
			//glEnd();
		}
	}
	
	gluDeleteQuadric(qobj);
	myloadmatrix(G.vd->viewmat);
	
	if(G.zbuf) glEnable(GL_DEPTH_TEST);		// shouldn't be global, tsk!
	
}

/* main call, does calc centers & orientation too */
void BIF_draw_manipulator(ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;
	float len1, len2, size, vec[3];
	int totsel;
	
	if(!(v3d->twflag & V3D_USE_MANIPULATOR)) return;
	if(G.moving && (G.moving & G_TRANSFORM_MANIP)==0) return;
	
	if(G.moving==0) {
		v3d->twflag &= ~V3D_DRAW_MANIPULATOR;
		
		totsel= calc_manipulator(sa);
		if(totsel==0) return;
		
		v3d->twflag |= V3D_DRAW_MANIPULATOR;

		/* now we can define centre */
		switch(v3d->around) {
		case V3D_CENTRE:
		case V3D_LOCAL:
			v3d->twmat[3][0]= (G.scene->twmin[0] + G.scene->twmax[0])/2.0;
			v3d->twmat[3][1]= (G.scene->twmin[1] + G.scene->twmax[1])/2.0;
			v3d->twmat[3][2]= (G.scene->twmin[2] + G.scene->twmax[2])/2.0;
			break;
		case V3D_CENTROID:
			VECCOPY(v3d->twmat[3], G.scene->twcent);
			break;
		case V3D_CURSOR:
			VECCOPY(v3d->twmat[3], G.scene->cursor);
			break;
		}
		
		/* size calculus, depending ortho/persp settings, like initgrabz() */
		size= G.vd->persmat[0][3]*v3d->twmat[3][0]+ G.vd->persmat[1][3]*v3d->twmat[3][1]+ G.vd->persmat[2][3]*v3d->twmat[3][2]+ G.vd->persmat[3][3];
		
		VECCOPY(vec, G.vd->persinv[0]);
		len1= Normalise(vec);
		VECCOPY(vec, G.vd->persinv[1]);
		len2= Normalise(vec);
		
		size*= (0.01f*(float)U.tw_size)*(len1>len2?len1:len2);
		if(U.tw_flag & U_TW_ABSOLUTE) {
			/* correct for relative window size */
			if(sa->winx > sa->winy) size*= 1000.0f/(float)sa->winx;
			else size*= 1000.0f/(float)sa->winy;
		}
		Mat4MulFloat3((float *)v3d->twmat, size);
	}
	
	if(v3d->twflag & V3D_DRAW_MANIPULATOR) {
		
		if(v3d->twtype & V3D_MANIPULATOR_ROTATE) {
			if(G.moving) draw_manipulator_rotate_ghost(v3d->twmat);
			draw_manipulator_rotate(v3d->twmat);
		}
		if(v3d->twtype & V3D_MANIPULATOR_SCALE) {
			if(G.moving) draw_manipulator_scale_ghost(v3d->twmat);
			draw_manipulator_scale(v3d->twmat);
		}
		if(v3d->twtype & V3D_MANIPULATOR_TRANSLATE) {
			if(G.moving) draw_manipulator_translate_ghost(v3d->twmat);
			draw_manipulator_translate(v3d->twmat);
		}
	}
}

static int manipulator_selectbuf(ScrArea *sa, float hotspot)
{
	View3D *v3d= sa->spacedata.first;
	rctf rect;
	GLuint buffer[64];		// max 4 items per select, so large enuf
	short hits, mval[2];
	
	G.f |= G_PICKSEL;
	
	getmouseco_areawin(mval);
	rect.xmin= mval[0]-hotspot;
	rect.xmax= mval[0]+hotspot;
	rect.ymin= mval[1]-hotspot;
	rect.ymax= mval[1]+hotspot;
	
	/* get rid of overlay button matrix */
	persp(PERSP_VIEW);
	
	setwinmatrixview3d(&rect);
	Mat4MulMat4(G.vd->persmat, G.vd->viewmat, sa->winmat);
	
	glSelectBuffer( 64, buffer);
	glRenderMode(GL_SELECT);
	glInitNames();	/* these two calls whatfor? It doesnt work otherwise */
	glPushName(-2);
	
	/* do the drawing */
	if(v3d->twtype & V3D_MANIPULATOR_ROTATE)
		draw_manipulator_rotate(v3d->twmat);
	if(v3d->twtype & V3D_MANIPULATOR_SCALE)
		draw_manipulator_scale(v3d->twmat);
	if(v3d->twtype & V3D_MANIPULATOR_TRANSLATE)
		draw_manipulator_translate(v3d->twmat);
	
	glPopName();
	hits= glRenderMode(GL_RENDER);
	
	G.f &= ~G_PICKSEL;
	setwinmatrixview3d(0);
	Mat4MulMat4(G.vd->persmat, G.vd->viewmat, sa->winmat);
	
	persp(PERSP_WIN);
	
	if(hits==1) return buffer[3];
	else if(hits>1) {
		/* we compare the two first in buffer, but exclude centers */
		
		if(buffer[3]==MAN_TRANS_C || buffer[3]==MAN_SCALE_C);
		else if(buffer[4+3]==MAN_TRANS_C || buffer[4+3]==MAN_SCALE_C);
		else {
			if(buffer[4+1] < buffer[1]) return buffer[4+3];
		}
		return buffer[3];
	}
	return 0;
}

int BIF_do_manipulator(ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;
	int val;
	
	if(!(v3d->twflag & V3D_USE_MANIPULATOR)) return 0;
	if(!(v3d->twflag & V3D_DRAW_MANIPULATOR)) return 0;
	
	// find the hotspots (Gval is for draw). first test narrow hotspot
	// warning, Gval is ugly global defining how it draws, don't set it before doing select calls!
	val= manipulator_selectbuf(sa, 0.5*(float)U.tw_hotspot);
	if(val) {
		Gval= manipulator_selectbuf(sa, 0.2*(float)U.tw_hotspot);
		if(Gval) val= Gval;
		else Gval= val;
	}

	switch(val) {
		case MAN_TRANS_C:
			ManipulatorTransform(TFM_TRANSLATION);
			break;
		case MAN_TRANS_X:
			if(G.qual & LR_SHIFTKEY) {
				Gval= MAN_TRANS_Y|MAN_TRANS_Z;
				BIF_setDualAxisConstraint(v3d->twmat[1], v3d->twmat[2]);
			}
			else
				BIF_setSingleAxisConstraint(v3d->twmat[0]);
			ManipulatorTransform(TFM_TRANSLATION);
			break;
		case MAN_TRANS_Y:
			if(G.qual & LR_SHIFTKEY) {
				Gval= MAN_TRANS_X|MAN_TRANS_Z;
				BIF_setDualAxisConstraint(v3d->twmat[0], v3d->twmat[2]);
			}
			else
				BIF_setSingleAxisConstraint(v3d->twmat[1]);
			ManipulatorTransform(TFM_TRANSLATION);
			break;
		case MAN_TRANS_Z:
			if(G.qual & LR_SHIFTKEY) {
				Gval= MAN_TRANS_X|MAN_TRANS_Y;
				BIF_setDualAxisConstraint(v3d->twmat[0], v3d->twmat[1]);
			}
			else
				BIF_setSingleAxisConstraint(v3d->twmat[2]);
			ManipulatorTransform(TFM_TRANSLATION);
			break;
			
		case MAN_SCALE_C:
			ManipulatorTransform(TFM_RESIZE);
			break;
		case MAN_SCALE_X:
			if(G.qual & LR_SHIFTKEY) {
				Gval= MAN_SCALE_Y|MAN_SCALE_Z;
				BIF_setDualAxisConstraint(v3d->twmat[1], v3d->twmat[2]);
			}
			else
				BIF_setSingleAxisConstraint(v3d->twmat[0]);
			ManipulatorTransform(TFM_RESIZE);
			break;
		case MAN_SCALE_Y:
			if(G.qual & LR_SHIFTKEY) {
				Gval= MAN_SCALE_X|MAN_SCALE_Z;
				BIF_setDualAxisConstraint(v3d->twmat[0], v3d->twmat[2]);
			}
			else
				BIF_setSingleAxisConstraint(v3d->twmat[1]);
			ManipulatorTransform(TFM_RESIZE);
			break;
		case MAN_SCALE_Z:
			if(G.qual & LR_SHIFTKEY) {
				Gval= MAN_SCALE_X|MAN_SCALE_Y;
				BIF_setDualAxisConstraint(v3d->twmat[0], v3d->twmat[1]);
			}
			else
				BIF_setSingleAxisConstraint(v3d->twmat[2]);
			ManipulatorTransform(TFM_RESIZE);
			break;
		
		case MAN_ROT_X:
			BIF_setSingleAxisConstraint(v3d->twmat[0]);
			ManipulatorTransform(TFM_ROTATION);
			break;
		case MAN_ROT_Y:
			BIF_setSingleAxisConstraint(v3d->twmat[1]);
			ManipulatorTransform(TFM_ROTATION);
			break;
		case MAN_ROT_Z:
			BIF_setSingleAxisConstraint(v3d->twmat[2]);
			ManipulatorTransform(TFM_ROTATION);
			break;
		case MAN_ROT_T:
			ManipulatorTransform(TFM_TRACKBALL);
			break;			
		case MAN_ROT_V:
			ManipulatorTransform(TFM_ROTATION);
			break;
	}
	
	Gval= 0xFFFF;
	
	return val;
}


