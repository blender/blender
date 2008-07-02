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
 * The Original Code is Copyright (C) 2005 Blender Foundation
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

#ifndef WIN32
#include <unistd.h>
#else
#include <io.h>
#endif

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_particle_types.h"
#include "DNA_screen_types.h"
#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_userdef_types.h"
#include "DNA_view3d_types.h"

#include "BKE_armature.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_object.h"
#include "BKE_particle.h"
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
#include "BIF_editmesh.h"
#include "BIF_editparticle.h"

#include "BSE_edit.h"
#include "BSE_view.h"
#include "BDR_drawobject.h"

#include "blendef.h"
#include "transform.h"

/* return codes for select, and drawing flags */

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

/* color codes */

#define MAN_RGB		0
#define MAN_GHOST	1
#define MAN_MOVECOL	2

/* GLOBAL VARIABLE THAT SHOULD MOVED TO SCREEN MEMBER OR SOMETHING  */
extern TransInfo Trans;


static int is_mat4_flipped(float mat[][4])
{
	float vec[3];
	
	Crossf(vec, mat[0], mat[1]);
	if( Inpf(vec, mat[2]) < 0.0 ) return 1;
	return 0;
}	

/* transform widget center calc helper for below */
static void calc_tw_center(float *co)
{
	float *twcent= G.scene->twcent;
	float *min= G.scene->twmin;
	float *max= G.scene->twmax;
	
	DO_MINMAX(co, min, max);
	VecAddf(twcent, twcent, co);
}

static void protectflag_to_drawflags(short protectflag, short *drawflags)
{
	if(protectflag & OB_LOCK_LOCX)
		*drawflags &= ~MAN_TRANS_X;
	if(protectflag & OB_LOCK_LOCY)
		*drawflags &= ~MAN_TRANS_Y;
	if(protectflag & OB_LOCK_LOCZ)
		*drawflags &= ~MAN_TRANS_Z;
	
	if(protectflag & OB_LOCK_ROTX)
		*drawflags &= ~MAN_ROT_X;
	if(protectflag & OB_LOCK_ROTY)
		*drawflags &= ~MAN_ROT_Y;
	if(protectflag & OB_LOCK_ROTZ)
		*drawflags &= ~MAN_ROT_Z;

	if(protectflag & OB_LOCK_SCALEX)
		*drawflags &= ~MAN_SCALE_X;
	if(protectflag & OB_LOCK_SCALEY)
		*drawflags &= ~MAN_SCALE_Y;
	if(protectflag & OB_LOCK_SCALEZ)
		*drawflags &= ~MAN_SCALE_Z;
}

/* for pose mode */
static void stats_pose(View3D *v3d, bPoseChannel *pchan)
{
	Bone *bone= pchan->bone;
	
	if(bone) {
		if (bone->flag & BONE_TRANSFORM) {
			calc_tw_center(pchan->pose_head);
			protectflag_to_drawflags(pchan->protectflag, &v3d->twdrawflag);
		}
	}
}

/* only counts the parent selection, and tags transform flag */
/* bad call... should re-use method from transform_conversion once */
static void count_bone_select(TransInfo *t, bArmature *arm, ListBase *lb, int do_it) 
{
	Bone *bone;
	int do_next;
	
	for(bone= lb->first; bone; bone= bone->next) {
		bone->flag &= ~BONE_TRANSFORM;
		do_next= do_it;
		if(do_it) {
			if(bone->layer & arm->layer) {
				if (bone->flag & BONE_SELECTED) {
					/* We don't let connected children get "grabbed" */
					if ( (t->mode!=TFM_TRANSLATION) || (bone->flag & BONE_CONNECTED)==0 ) {
						bone->flag |= BONE_TRANSFORM;
						t->total++;
						do_next= 0;	// no transform on children if one parent bone is selected
					}
				}
			}
		}
		count_bone_select(t, arm, &bone->childbase, do_next);
	}
}

/* centroid, boundbox, of selection */
/* returns total items selected */
int calc_manipulator_stats(ScrArea *sa)
{
	extern ListBase editNurb;
	TransInfo *t;
	View3D *v3d= sa->spacedata.first;
	Base *base;
	Object *ob= OBACT;
	float normal[3]={0.0, 0.0, 0.0};
	float plane[3]={0.0, 0.0, 0.0};
	int a, totsel=0;

	t = BIF_GetTransInfo();
	
	/* transform widget matrix */
	Mat4One(v3d->twmat);
	
	v3d->twdrawflag= 0xFFFF;
	
	/* transform widget centroid/center */
	G.scene->twcent[0]= G.scene->twcent[1]= G.scene->twcent[2]= 0.0f;
	INIT_MINMAX(G.scene->twmin, G.scene->twmax);
	
	if(G.obedit) {
		ob= G.obedit;
		if((ob->lay & G.vd->lay)==0) return 0;

		if(G.obedit->type==OB_MESH) {
			EditMesh *em = G.editMesh;
			EditVert *eve;
			EditSelection ese;
			float vec[3]= {0,0,0};
			
			/* USE LAST SELECTE WITH ACTIVE */
			if (G.vd->around==V3D_ACTIVE && EM_get_actSelection(&ese)) {
				EM_editselection_center(vec, &ese);
				calc_tw_center(vec);
				totsel= 1;
			} else {
				/* do vertices for center, and if still no normal found, use vertex normals */
				for(eve= em->verts.first; eve; eve= eve->next) {
					if(eve->f & SELECT) {
						totsel++;
						calc_tw_center(eve->co);
					}
				}
			}
		} /* end editmesh */
		else if (G.obedit->type==OB_ARMATURE){
			bArmature *arm= G.obedit->data;
			EditBone *ebo;
			for (ebo=G.edbo.first;ebo;ebo=ebo->next){
				if(ebo->layer & arm->layer) {
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
			Nurb *nu;
			BezTriple *bezt;
			BPoint *bp;
			
			nu= editNurb.first;
			while(nu) {
				if((nu->type & 7)==CU_BEZIER) {
					bezt= nu->bezt;
					a= nu->pntsu;
					while(a--) {
						/* exception */
						if( (bezt->f1 & SELECT) + (bezt->f2 & SELECT) + (bezt->f3 & SELECT) > SELECT ) {
							calc_tw_center(bezt->vec[1]);
							totsel++;
						}
						else {
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
						}
						bezt++;
					}
				}
				else {
					bp= nu->bp;
					a= nu->pntsu*nu->pntsv;
					while(a--) {
						if(bp->f1 & SELECT) {
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
			MetaElem *ml, *ml_sel=NULL;
		
			ml= editelems.first;
			while(ml) {
				if(ml->flag & SELECT) {
					calc_tw_center(&ml->x);
					ml_sel = ml;
					totsel++;
				}
				ml= ml->next;
			}
		}
		else if(G.obedit->type==OB_LATTICE) {
			BPoint *bp;
			bp= editLatt->def;
			
			a= editLatt->pntsu*editLatt->pntsv*editLatt->pntsw;
			while(a--) {
				if(bp->f1 & SELECT) {
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
	else if(ob && (ob->flag & OB_POSEMODE)) {
		bArmature *arm = ob->data;
		bPoseChannel *pchan;
		int mode;
		
		if((ob->lay & G.vd->lay)==0) return 0;
		
		mode = Trans.mode;
		Trans.mode = TFM_ROTATION;	// mislead counting bones... bah
		
		/* count total, we use same method as transform will do */
		Trans.total= 0;
		count_bone_select(&Trans, arm, &arm->bonebase, 1);
		totsel = Trans.total;
		if(totsel) {
			/* use channels to get stats */
			for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
				stats_pose(v3d, pchan);
			}
			
			VecMulf(G.scene->twcent, 1.0f/(float)totsel);	// centroid!
			Mat4MulVecfl(ob->obmat, G.scene->twcent);
			Mat4MulVecfl(ob->obmat, G.scene->twmin);
			Mat4MulVecfl(ob->obmat, G.scene->twmax);
		}
		/* restore, mode can be TFM_INIT */
		Trans.mode = mode;
	}
	else if(G.f & (G_VERTEXPAINT + G_TEXTUREPAINT + G_WEIGHTPAINT + G_SCULPTMODE)) {
		;
	}
	else if(G.f & G_PARTICLEEDIT) {
		ParticleSystem *psys=PE_get_current(OBACT);
		ParticleData *pa = psys->particles;
		ParticleEditKey *ek;
		int k;

		if(psys->edit){
			for(a=0; a<psys->totpart; a++,pa++){
				if(pa->flag & PARS_HIDE) continue;
				for(k=0, ek=psys->edit->keys[a]; k<pa->totkey; k++,ek++){
					if(ek->flag & PEK_SELECT){
						calc_tw_center(ek->world_co);
						totsel++;
					}
				}
			}
			/* selection center */
			if(totsel)
				VecMulf(G.scene->twcent, 1.0f/(float)totsel);	// centroid!
		}
	}
	else {
		
		/* we need the one selected object, if its not active */
		ob= OBACT;
		if(ob && !(ob->flag & SELECT)) ob= NULL;
		
		for(base= G.scene->base.first; base; base= base->next) {
			if TESTBASELIB(base) {
				if(ob==NULL) 
					ob= base->object;
				calc_tw_center(base->object->obmat[3]);
				protectflag_to_drawflags(base->object->protectflag, &v3d->twdrawflag);
				totsel++;
			}
		}
		
		/* selection center */
		if(totsel) {
			VecMulf(G.scene->twcent, 1.0f/(float)totsel);	// centroid!
		}
	}
	
	/* global, local or normal orientation? */
	if(ob && totsel) {
		
		switch(v3d->twmode) {
		case V3D_MANIP_GLOBAL:
			strcpy(t->spacename, "global");
			break;
			
		case V3D_MANIP_NORMAL:
			if(G.obedit || ob->flag & OB_POSEMODE) {
				float mat[3][3];
				int type;
				
				strcpy(t->spacename, "normal");
				
				type = getTransformOrientation(normal, plane, (G.vd->around == V3D_ACTIVE));
				
				switch (type)
				{
					case ORIENTATION_NORMAL:
						if (createSpaceNormalTangent(mat, normal, plane) == 0)
						{
							type = ORIENTATION_NONE;
						}
						break;
					case ORIENTATION_VERT:
						if (createSpaceNormal(mat, normal) == 0)
						{
							type = ORIENTATION_NONE;
						}
						break;
					case ORIENTATION_EDGE:
						if (createSpaceNormalTangent(mat, normal, plane) == 0)
						{
							type = ORIENTATION_NONE;
						}
						break;
					case ORIENTATION_FACE:
						if (createSpaceNormalTangent(mat, normal, plane) == 0)
						{
							type = ORIENTATION_NONE;
						}
						break;
				}
				
				if (type == ORIENTATION_NONE)
				{
					Mat4One(v3d->twmat);
				}
				else
				{
					Mat4CpyMat3(v3d->twmat, mat);
				}
				break;
			}
			/* no break we define 'normal' as 'local' in Object mode */
		case V3D_MANIP_LOCAL:
			strcpy(t->spacename, "local");
			Mat4CpyMat4(v3d->twmat, ob->obmat);
			Mat4Ortho(v3d->twmat);
			break;
			
		case V3D_MANIP_VIEW:
			{
				float mat[3][3];
				strcpy(t->spacename, "view");
				Mat3CpyMat4(mat, v3d->viewinv);
				Mat3Ortho(mat);
				Mat4CpyMat3(v3d->twmat, mat);
			}
			break;
		default: /* V3D_MANIP_CUSTOM */
			applyTransformOrientation();
			break;
		}
		
	}
	   
	return totsel;
}

/* ******************** DRAWING STUFFIES *********** */

static float screen_aligned(float mat[][4])
{
	float vec[3], size;
	
	VECCOPY(vec, mat[0]);
	size= Normalize(vec);
	
	glTranslatef(mat[3][0], mat[3][1], mat[3][2]);
	
	/* sets view screen aligned */
	glRotatef( -360.0f*saacos(G.vd->viewquat[0])/(float)M_PI, G.vd->viewquat[1], G.vd->viewquat[2], G.vd->viewquat[3]);
	
	return size;
}


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
	int i, j, docaps= 1;
	
	if(start==0 && end==nrings) docaps= 0;
	
	ring_delta= 2.0f*(float)M_PI/(float)nrings;
	side_delta= 2.0f*(float)M_PI/(float)nsides;
	
	theta= (float)M_PI+0.5f*ring_delta;
	cos_theta= (float)cos(theta);
	sin_theta= (float)sin(theta);
	
	for(i= nrings - 1; i >= 0; i--) {
		theta1= theta + ring_delta;
		cos_theta1= (float)cos(theta1);
		sin_theta1= (float)sin(theta1);
		
		if(docaps && i==start) {	// cap
			glBegin(GL_POLYGON);
			phi= 0.0;
			for(j= nsides; j >= 0; j--) {
				float cos_phi, sin_phi, dist;
				
				phi += side_delta;
				cos_phi= (float)cos(phi);
				sin_phi= (float)sin(phi);
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
				cos_phi= (float)cos(phi);
				sin_phi= (float)sin(phi);
				dist= radhole + radring * cos_phi;
				
				glVertex3f(cos_theta1 * dist, -sin_theta1 * dist, radring * sin_phi);
				glVertex3f(cos_theta * dist, -sin_theta * dist,  radring * sin_phi);
			}
			glEnd();
		}
		
		if(docaps && i==end) {	// cap
			glBegin(GL_POLYGON);
			phi= 0.0;
			for(j= nsides; j >= 0; j--) {
				float cos_phi, sin_phi, dist;
				
				phi -= side_delta;
				cos_phi= (float)cos(phi);
				sin_phi= (float)sin(phi);
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
   grey for ghosting
   moving: in transform theme color
   else the red/green/blue
*/
static void manipulator_setcolor(char axis, int colcode)
{
	float vec[4];
	char col[4];
	
	vec[3]= 0.7f; // alpha set on 0.5, can be glEnabled or not
	
	if(colcode==MAN_GHOST) {
		glColor4ub(0, 0, 0, 70);
	}
	else if(colcode==MAN_MOVECOL) {
		BIF_GetThemeColor3ubv(TH_TRANSFORM, col);
		glColor4ub(col[0], col[1], col[2], 128);
	}
	else {
		switch(axis) {
		case 'c':
			BIF_GetThemeColor3ubv(TH_TRANSFORM, col);
			if(G.vd->twmode == V3D_MANIP_LOCAL) {
				col[0]= col[0]>200?255:col[0]+55;
				col[1]= col[1]>200?255:col[1]+55;
				col[2]= col[2]>200?255:col[2]+55;
			}
			else if(G.vd->twmode == V3D_MANIP_NORMAL) {
				col[0]= col[0]<55?0:col[0]-55;
				col[1]= col[1]<55?0:col[1]-55;
				col[2]= col[2]<55?0:col[2]-55;
			}
			glColor4ub(col[0], col[1], col[2], 128);
			break;
		case 'x':
			glColor4ub(220, 0, 0, 128);
			break;
		case 'y':
			glColor4ub(0, 220, 0, 128);
			break;
		case 'z':
			glColor4ub(30, 30, 220, 128);
			break;
		}
	}
}

/* viewmatrix should have been set OK, also no shademode! */
static void draw_manipulator_axes(int colcode, int flagx, int flagy, int flagz)
{
	
	/* axes */
	if(flagx) {
		manipulator_setcolor('x', colcode);
		if(flagx & MAN_SCALE_X) glLoadName(MAN_SCALE_X);
		else if(flagx & MAN_TRANS_X) glLoadName(MAN_TRANS_X);
		glBegin(GL_LINES);
		glVertex3f(0.2f, 0.0f, 0.0f);
		glVertex3f(1.0f, 0.0f, 0.0f);
		glEnd();
	}		
	if(flagy) {
		if(flagy & MAN_SCALE_Y) glLoadName(MAN_SCALE_Y);
		else if(flagy & MAN_TRANS_Y) glLoadName(MAN_TRANS_Y);
		manipulator_setcolor('y', colcode);
		glBegin(GL_LINES);
		glVertex3f(0.0f, 0.2f, 0.0f);
		glVertex3f(0.0f, 1.0f, 0.0f);
		glEnd();
	}		
	if(flagz) {
		if(flagz & MAN_SCALE_Z) glLoadName(MAN_SCALE_Z);
		else if(flagz & MAN_TRANS_Z) glLoadName(MAN_TRANS_Z);
		manipulator_setcolor('z', colcode);
		glBegin(GL_LINES);
		glVertex3f(0.0f, 0.0f, 0.2f);
		glVertex3f(0.0f, 0.0f, 1.0f);
		glEnd();
	}
}

/* only called while G.moving */
static void draw_manipulator_rotate_ghost(float mat[][4], int drawflags)
{
	GLUquadricObj *qobj;
	float size, phi, startphi, vec[3], svec[3], matt[4][4], cross[3], tmat[3][3];
	int arcs= (G.rt!=2);
	
	glDisable(GL_DEPTH_TEST);

	qobj= gluNewQuadric(); 
	gluQuadricDrawStyle(qobj, GLU_FILL); 
	
	glColor4ub(0,0,0,64);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);
		
	/* we need both [4][4] transforms, Trans.mat seems to be premul, not post for mat[][4] */
	Mat4CpyMat4(matt, mat); // to copy the parts outside of [3][3]
	Mat4MulMat34(matt, Trans.mat, mat);

	/* Screen aligned view rot circle */
	if(drawflags & MAN_ROT_V) {
		
		/* prepare for screen aligned draw */
		glPushMatrix();
		size= screen_aligned(mat);
	
		vec[0]= (float)(Trans.con.imval[0] - Trans.center2d[0]);
		vec[1]= (float)(Trans.con.imval[1] - Trans.center2d[1]);
		vec[2]= 0.0f;
		Normalize(vec);
		
		startphi= saacos( vec[1] );
		if(vec[0]<0.0) startphi= -startphi;
		
		phi= (float)fmod(180.0*Trans.val/M_PI, 360.0);
		if(phi > 180.0) phi-= 360.0;
		else if(phi<-180.0) phi+= 360.0;
		
		gluPartialDisk(qobj, 0.0, size, 32, 1, 180.0*startphi/M_PI, phi);
		
		glPopMatrix();
	}
	else if(arcs) {
		float imat[3][3], ivmat[3][3];
		/* try to get the start rotation */
		
		svec[0]= (float)(Trans.con.imval[0] - Trans.center2d[0]);
		svec[1]= (float)(Trans.con.imval[1] - Trans.center2d[1]);
		svec[2]= 0.0f;
		
		/* screen aligned vec transform back to manipulator space */
		Mat3CpyMat4(ivmat, G.vd->viewinv);
		Mat3CpyMat4(tmat, mat);
		Mat3Inv(imat, tmat);
		Mat3MulMat3(tmat, imat, ivmat);
		
		Mat3MulVecfl(tmat, svec);	// tmat is used further on
		Normalize(svec);
	}	
	
	mymultmatrix(mat);	// aligns with original widget
	
	/* Z disk */
	if(drawflags & MAN_ROT_Z) {
		if(arcs) {
			/* correct for squeezed arc */
			svec[0]+= tmat[2][0];
			svec[1]+= tmat[2][1];
			Normalize(svec);
			
			startphi= (float)atan2(svec[0], svec[1]);
		}
		else startphi= 0.5f*(float)M_PI;
		
		VECCOPY(vec, mat[0]);	// use x axis to detect rotation
		Normalize(vec);
		Normalize(matt[0]);
		phi= saacos( Inpf(vec, matt[0]) );
		if(phi!=0.0) {
			Crossf(cross, vec, matt[0]);	// results in z vector
			if(Inpf(cross, mat[2]) > 0.0) phi= -phi;
			gluPartialDisk(qobj, 0.0, 1.0, 32, 1, 180.0*startphi/M_PI, 180.0*(phi)/M_PI);
		}
	}
	/* X disk */
	if(drawflags & MAN_ROT_X) {
		if(arcs) {
			/* correct for squeezed arc */
			svec[1]+= tmat[2][1];
			svec[2]+= tmat[2][2];
			Normalize(svec);
			
			startphi= (float)(M_PI + atan2(svec[2], -svec[1]));
		}
		else startphi= 0.0f;
		
		VECCOPY(vec, mat[1]);	// use y axis to detect rotation
		Normalize(vec);
		Normalize(matt[1]);
		phi= saacos( Inpf(vec, matt[1]) );
		if(phi!=0.0) {
			Crossf(cross, vec, matt[1]);	// results in x vector
			if(Inpf(cross, mat[0]) > 0.0) phi= -phi;
			glRotatef(90.0, 0.0, 1.0, 0.0);
			gluPartialDisk(qobj, 0.0, 1.0, 32, 1, 180.0*startphi/M_PI, 180.0*phi/M_PI);
			glRotatef(-90.0, 0.0, 1.0, 0.0);
		}
	}	
	/* Y circle */
	if(drawflags & MAN_ROT_Y) {
		if(arcs) {
			/* correct for squeezed arc */
			svec[0]+= tmat[2][0];
			svec[2]+= tmat[2][2];
			Normalize(svec);
			
			startphi= (float)(M_PI + atan2(-svec[0], svec[2]));
		}
		else startphi= (float)M_PI;
		
		VECCOPY(vec, mat[2]);	// use z axis to detect rotation
		Normalize(vec);
		Normalize(matt[2]);
		phi= saacos( Inpf(vec, matt[2]) );
		if(phi!=0.0) {
			Crossf(cross, vec, matt[2]);	// results in y vector
			if(Inpf(cross, mat[1]) > 0.0) phi= -phi;
			glRotatef(-90.0, 1.0, 0.0, 0.0);
			gluPartialDisk(qobj, 0.0, 1.0, 32, 1, 180.0*startphi/M_PI, 180.0*phi/M_PI);
			glRotatef(90.0, 1.0, 0.0, 0.0);
		}
	}
	
	glDisable(GL_BLEND);
	myloadmatrix(G.vd->viewmat);
}

static void draw_manipulator_rotate(float mat[][4], int moving, int drawflags, int combo)
{
	GLUquadricObj *qobj; 
	double plane[4];
	float size, vec[3], unitmat[4][4];
	float cywid= 0.33f*0.01f*(float)U.tw_handlesize;	
	float cusize= cywid*0.65f;
	int arcs= (G.rt!=2);
	int colcode;
	
	if(moving) colcode= MAN_MOVECOL;
	else colcode= MAN_RGB;
	
	/* when called while moving in mixed mode, do not draw when... */
	if((drawflags & MAN_ROT_C)==0) return;
	
	/* Init stuff */
	glDisable(GL_DEPTH_TEST);
	Mat4One(unitmat);

	qobj= gluNewQuadric();
	gluQuadricDrawStyle(qobj, GLU_FILL); 
	
	/* prepare for screen aligned draw */
	VECCOPY(vec, mat[0]);
	size= Normalize(vec);
	glPushMatrix();
	glTranslatef(mat[3][0], mat[3][1], mat[3][2]);
	
	if(arcs) {
		/* clipplane makes nice handles, calc here because of multmatrix but with translate! */
		VECCOPY(plane, G.vd->viewinv[2]);
		plane[3]= -0.02*size; // clip just a bit more
		glClipPlane(GL_CLIP_PLANE0, plane);
	}
	/* sets view screen aligned */
	glRotatef( -360.0f*saacos(G.vd->viewquat[0])/(float)M_PI, G.vd->viewquat[1], G.vd->viewquat[2], G.vd->viewquat[3]);
	
	/* Screen aligned help circle */
	if(arcs) {
		if((G.f & G_PICKSEL)==0) {
			BIF_ThemeColorShade(TH_BACK, -30);
			drawcircball(GL_LINE_LOOP, unitmat[3], size, unitmat);
		}
	}
	/* Screen aligned view rot circle */
	if(drawflags & MAN_ROT_V) {
		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_V);
		BIF_ThemeColor(TH_TRANSFORM);
		drawcircball(GL_LINE_LOOP, unitmat[3], 1.2f*size, unitmat);
		
		if(moving) {	
			float vec[3];
			vec[0]= (float)(Trans.imval[0] - Trans.center2d[0]);
			vec[1]= (float)(Trans.imval[1] - Trans.center2d[1]);
			vec[2]= 0.0f;
			Normalize(vec);
			VecMulf(vec, 1.2f*size);
			glBegin(GL_LINES);
			glVertex3f(0.0f, 0.0f, 0.0f);
			glVertex3fv(vec);
			glEnd();
		}
	}
	glPopMatrix();
	
	/* apply the transform delta */
	if(moving) {
		float matt[4][4];
		Mat4CpyMat4(matt, mat); // to copy the parts outside of [3][3]
		Mat4MulMat34(matt, Trans.mat, mat);
		mymultmatrix(matt);
		glFrontFace( is_mat4_flipped(matt)?GL_CW:GL_CCW);
	}
	else {
		glFrontFace( is_mat4_flipped(mat)?GL_CW:GL_CCW);
		mymultmatrix(mat);
	}
	
	/* axes */
	if(arcs==0) {
		if(!(G.f & G_PICKSEL)) {
			if( (combo & V3D_MANIP_SCALE)==0) {
				/* axis */
				glBegin(GL_LINES);
				if( (drawflags & MAN_ROT_X) || (moving && (drawflags & MAN_ROT_Z)) ) {
					manipulator_setcolor('x', colcode);
					glVertex3f(0.2f, 0.0f, 0.0f);
					glVertex3f(1.0f, 0.0f, 0.0f);
				}		
				if( (drawflags & MAN_ROT_Y) || (moving && (drawflags & MAN_ROT_X)) ) {
					manipulator_setcolor('y', colcode);
					glVertex3f(0.0f, 0.2f, 0.0f);
					glVertex3f(0.0f, 1.0f, 0.0f);
				}		
				if( (drawflags & MAN_ROT_Z) || (moving && (drawflags & MAN_ROT_Y)) ) {
					manipulator_setcolor('z', colcode);
					glVertex3f(0.0f, 0.0f, 0.2f);
					glVertex3f(0.0f, 0.0f, 1.0f);
				}
				glEnd();
			}
		}
	}
	
	if(arcs==0 && moving) {
		
		/* Z circle */
		if(drawflags & MAN_ROT_Z) {
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Z);
			manipulator_setcolor('z', colcode);
			drawcircball(GL_LINE_LOOP, unitmat[3], 1.0, unitmat);
		}
		/* X circle */
		if(drawflags & MAN_ROT_X) {
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_X);
			glRotatef(90.0, 0.0, 1.0, 0.0);
			manipulator_setcolor('x', colcode);
			drawcircball(GL_LINE_LOOP, unitmat[3], 1.0, unitmat);
			glRotatef(-90.0, 0.0, 1.0, 0.0);
		}	
		/* Y circle */
		if(drawflags & MAN_ROT_Y) {
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Y);
			glRotatef(-90.0, 1.0, 0.0, 0.0);
			manipulator_setcolor('y', colcode);
			drawcircball(GL_LINE_LOOP, unitmat[3], 1.0, unitmat);
			glRotatef(90.0, 1.0, 0.0, 0.0);
		}
		
		if(arcs) glDisable(GL_CLIP_PLANE0);
	}
	// donut arcs
	if(arcs) {
		glEnable(GL_CLIP_PLANE0);
		
		/* Z circle */
		if(drawflags & MAN_ROT_Z) {
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Z);
			manipulator_setcolor('z', colcode);
			partial_donut(cusize/4.0f, 1.0f, 0, 48, 8, 48);
		}
		/* X circle */
		if(drawflags & MAN_ROT_X) {
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_X);
			glRotatef(90.0, 0.0, 1.0, 0.0);
			manipulator_setcolor('x', colcode);
			partial_donut(cusize/4.0f, 1.0f, 0, 48, 8, 48);
			glRotatef(-90.0, 0.0, 1.0, 0.0);
		}	
		/* Y circle */
		if(drawflags & MAN_ROT_Y) {
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Y);
			glRotatef(-90.0, 1.0, 0.0, 0.0);
			manipulator_setcolor('y', colcode);
			partial_donut(cusize/4.0f, 1.0f, 0, 48, 8, 48);
			glRotatef(90.0, 1.0, 0.0, 0.0);
		}
		
		glDisable(GL_CLIP_PLANE0);
	}
	
	if(arcs==0) {
		
		/* Z handle on X axis */
		if(drawflags & MAN_ROT_Z) {
			glPushMatrix();
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Z);
			manipulator_setcolor('z', colcode);

			partial_donut(0.7f*cusize, 1.0f, 31, 33, 8, 64);

			glPopMatrix();
		}	

		/* Y handle on X axis */
		if(drawflags & MAN_ROT_Y) {
			glPushMatrix();
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Y);
			manipulator_setcolor('y', colcode);
			
			glRotatef(90.0, 1.0, 0.0, 0.0);
			glRotatef(90.0, 0.0, 0.0, 1.0);
			partial_donut(0.7f*cusize, 1.0f, 31, 33, 8, 64);
			
			glPopMatrix();
		}
		
		/* X handle on Z axis */
		if(drawflags & MAN_ROT_X) {
			glPushMatrix();
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_X);
			manipulator_setcolor('x', colcode);
			
			glRotatef(-90.0, 0.0, 1.0, 0.0);
			glRotatef(90.0, 0.0, 0.0, 1.0);
			partial_donut(0.7f*cusize, 1.0f, 31, 33, 8, 64);

			glPopMatrix();
		}
		
	}
	
	/* restore */
	myloadmatrix(G.vd->viewmat);
	gluDeleteQuadric(qobj);
	if(G.vd->zbuf) glEnable(GL_DEPTH_TEST);
	
}

static void draw_manipulator_scale(float mat[][4], int moving, int drawflags, int combo, int colcode)
{
	float cywid= 0.25f*0.01f*(float)U.tw_handlesize;	
	float cusize= cywid*0.75f, dz;
	
	/* when called while moving in mixed mode, do not draw when... */
	if((drawflags & MAN_SCALE_C)==0) return;
	
	glDisable(GL_DEPTH_TEST);
	
	/* not in combo mode */
	if( (combo & (V3D_MANIP_TRANSLATE|V3D_MANIP_ROTATE))==0) {
		float size, unitmat[4][4];
		
		/* center circle, do not add to selection when shift is pressed (planar constraint)  */
		if( (G.f & G_PICKSEL) && (G.qual & LR_SHIFTKEY)==0) glLoadName(MAN_SCALE_C);
		
		manipulator_setcolor('c', colcode);
		glPushMatrix();
		size= screen_aligned(mat);
		Mat4One(unitmat);
		drawcircball(GL_LINE_LOOP, unitmat[3], 0.2f*size, unitmat);
		glPopMatrix();
		
		dz= 1.0;
	}
	else dz= 1.0f-4.0f*cusize;
	
	if(moving) {
		float matt[4][4];
		
		Mat4CpyMat4(matt, mat); // to copy the parts outside of [3][3]
		Mat4MulMat34(matt, Trans.mat, mat);
		mymultmatrix(matt);
		glFrontFace( is_mat4_flipped(matt)?GL_CW:GL_CCW);
	}
	else {
		mymultmatrix(mat);
		glFrontFace( is_mat4_flipped(mat)?GL_CW:GL_CCW);
	}
	
	/* axis */
		
	/* in combo mode, this is always drawn as first type */
	draw_manipulator_axes(colcode, drawflags & MAN_SCALE_X, drawflags & MAN_SCALE_Y, drawflags & MAN_SCALE_Z);
	
	/* Z cube */
	glTranslatef(0.0, 0.0, dz);
	if(drawflags & MAN_SCALE_Z) {
		if(G.f & G_PICKSEL) glLoadName(MAN_SCALE_Z);
		manipulator_setcolor('z', colcode);
		drawsolidcube(cusize);
	}	
	/* X cube */
	glTranslatef(dz, 0.0, -dz);
	if(drawflags & MAN_SCALE_X) {
		if(G.f & G_PICKSEL) glLoadName(MAN_SCALE_X);
		manipulator_setcolor('x', colcode);
		drawsolidcube(cusize);
	}	
	/* Y cube */
	glTranslatef(-dz, dz, 0.0);
	if(drawflags & MAN_SCALE_Y) {
		if(G.f & G_PICKSEL) glLoadName(MAN_SCALE_Y);
		manipulator_setcolor('y', colcode);
		drawsolidcube(cusize);
	}
	
	/* if shiftkey, center point as last, for selectbuffer order */
	if(G.f & G_PICKSEL) {
		if(G.qual & LR_SHIFTKEY) {
			glTranslatef(0.0, -dz, 0.0);
			glLoadName(MAN_SCALE_C);
			glBegin(GL_POINTS);
			glVertex3f(0.0, 0.0, 0.0);
			glEnd();
		}
	}
	
	/* restore */
	myloadmatrix(G.vd->viewmat);
	
	if(G.vd->zbuf) glEnable(GL_DEPTH_TEST);	
	glFrontFace(GL_CCW);
}


static void draw_cone(GLUquadricObj *qobj, float len, float width)
{
	glTranslatef(0.0, 0.0, -0.5f*len);
	gluCylinder(qobj, width, 0.0, len, 8, 1);
	gluQuadricOrientation(qobj, GLU_INSIDE);
	gluDisk(qobj, 0.0, width, 8, 1); 
	gluQuadricOrientation(qobj, GLU_OUTSIDE);
	glTranslatef(0.0, 0.0, 0.5f*len);
}

static void draw_cylinder(GLUquadricObj *qobj, float len, float width)
{
	
	width*= 0.8f;	// just for beauty
	
	glTranslatef(0.0, 0.0, -0.5f*len);
	gluCylinder(qobj, width, width, len, 8, 1);
	gluQuadricOrientation(qobj, GLU_INSIDE);
	gluDisk(qobj, 0.0, width, 8, 1); 
	gluQuadricOrientation(qobj, GLU_OUTSIDE);
	glTranslatef(0.0, 0.0, len);
	gluDisk(qobj, 0.0, width, 8, 1); 
	glTranslatef(0.0, 0.0, -0.5f*len);
}


static void draw_manipulator_translate(float mat[][4], int moving, int drawflags, int combo, int colcode)
{
	GLUquadricObj *qobj; 
	float cylen= 0.01f*(float)U.tw_handlesize;
	float cywid= 0.25f*cylen, dz, size;
	float unitmat[4][4];
	
	/* when called while moving in mixed mode, do not draw when... */
	if((drawflags & MAN_TRANS_C)==0) return;
	
	if(moving) glTranslatef(Trans.vec[0], Trans.vec[1], Trans.vec[2]);
	glDisable(GL_DEPTH_TEST);

	qobj= gluNewQuadric();
	gluQuadricDrawStyle(qobj, GLU_FILL); 
	
	/* center circle, do not add to selection when shift is pressed (planar constraint) */
	if( (G.f & G_PICKSEL) && (G.qual & LR_SHIFTKEY)==0) glLoadName(MAN_TRANS_C);
	
	manipulator_setcolor('c', colcode);
	glPushMatrix();
	size= screen_aligned(mat);
	Mat4One(unitmat);
	drawcircball(GL_LINE_LOOP, unitmat[3], 0.2f*size, unitmat);
	glPopMatrix();
	
	/* and now apply matrix, we move to local matrix drawing */
	mymultmatrix(mat);
	
	/* axis */
	glLoadName(-1);
	
	// translate drawn as last, only axis when no combo with scale, or for ghosting
	if((combo & V3D_MANIP_SCALE)==0 || colcode==MAN_GHOST)
		draw_manipulator_axes(colcode, drawflags & MAN_TRANS_X, drawflags & MAN_TRANS_Y, drawflags & MAN_TRANS_Z);

	
	/* offset in combo mode, for rotate a bit more */
	if(combo & (V3D_MANIP_ROTATE)) dz= 1.0f+2.0f*cylen;
	else if(combo & (V3D_MANIP_SCALE)) dz= 1.0f+0.5f*cylen;
	else dz= 1.0f;
	
	/* Z Cone */
	glTranslatef(0.0, 0.0, dz);
	if(drawflags & MAN_TRANS_Z) {
		if(G.f & G_PICKSEL) glLoadName(MAN_TRANS_Z);
		manipulator_setcolor('z', colcode);
		draw_cone(qobj, cylen, cywid);
	}	
	/* X Cone */
	glTranslatef(dz, 0.0, -dz);
	if(drawflags & MAN_TRANS_X) {
		if(G.f & G_PICKSEL) glLoadName(MAN_TRANS_X);
		glRotatef(90.0, 0.0, 1.0, 0.0);
		manipulator_setcolor('x', colcode);
		draw_cone(qobj, cylen, cywid);
		glRotatef(-90.0, 0.0, 1.0, 0.0);
	}	
	/* Y Cone */
	glTranslatef(-dz, dz, 0.0);
	if(drawflags & MAN_TRANS_Y) {
		if(G.f & G_PICKSEL) glLoadName(MAN_TRANS_Y);
		glRotatef(-90.0, 1.0, 0.0, 0.0);
		manipulator_setcolor('y', colcode);
		draw_cone(qobj, cylen, cywid);
	}

	gluDeleteQuadric(qobj);
	myloadmatrix(G.vd->viewmat);
	
	if(G.vd->zbuf) glEnable(GL_DEPTH_TEST);
	
}

static void draw_manipulator_rotate_cyl(float mat[][4], int moving, int drawflags, int combo, int colcode)
{
	GLUquadricObj *qobj;
	float size;
	float cylen= 0.01f*(float)U.tw_handlesize;
	float cywid= 0.25f*cylen;
	
	/* when called while moving in mixed mode, do not draw when... */
	if((drawflags & MAN_ROT_C)==0) return;

	/* prepare for screen aligned draw */
	glPushMatrix();
	size= screen_aligned(mat);
	
	glDisable(GL_DEPTH_TEST);

	qobj= gluNewQuadric(); 
	
	/* Screen aligned view rot circle */
	if(drawflags & MAN_ROT_V) {
		float unitmat[4][4];
		Mat4One(unitmat);
		
		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_V);
		BIF_ThemeColor(TH_TRANSFORM);
		drawcircball(GL_LINE_LOOP, unitmat[3], 1.2f*size, unitmat);
		
		if(moving) {
			float vec[3];
			vec[0]= (float)(Trans.imval[0] - Trans.center2d[0]);
			vec[1]= (float)(Trans.imval[1] - Trans.center2d[1]);
			vec[2]= 0.0f;
			Normalize(vec);
			VecMulf(vec, 1.2f*size);
			glBegin(GL_LINES);
			glVertex3f(0.0, 0.0, 0.0);
			glVertex3fv(vec);
			glEnd();
		}
	}
	glPopMatrix();
	
	/* apply the transform delta */
	if(moving) {
		float matt[4][4];
		Mat4CpyMat4(matt, mat); // to copy the parts outside of [3][3]
		if (Trans.flag & T_USES_MANIPULATOR) {
			Mat4MulMat34(matt, Trans.mat, mat);
		}
		mymultmatrix(matt);
	}
	else {
		mymultmatrix(mat);
	}
	
	glFrontFace( is_mat4_flipped(mat)?GL_CW:GL_CCW);
	
	/* axis */
	if( (G.f & G_PICKSEL)==0 ) {
		
		// only draw axis when combo didn't draw scale axes
		if((combo & V3D_MANIP_SCALE)==0)
			draw_manipulator_axes(colcode, drawflags & MAN_ROT_X, drawflags & MAN_ROT_Y, drawflags & MAN_ROT_Z);
		
		/* only has to be set when not in picking */
		gluQuadricDrawStyle(qobj, GLU_FILL); 
	}
	
	/* Z cyl */
	glTranslatef(0.0, 0.0, 1.0);
	if(drawflags & MAN_ROT_Z) {
		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Z);
		manipulator_setcolor('z', colcode);
		draw_cylinder(qobj, cylen, cywid);
	}	
	/* X cyl */
	glTranslatef(1.0, 0.0, -1.0);
	if(drawflags & MAN_ROT_X) {
		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_X);
		glRotatef(90.0, 0.0, 1.0, 0.0);
		manipulator_setcolor('x', colcode);
		draw_cylinder(qobj, cylen, cywid);
		glRotatef(-90.0, 0.0, 1.0, 0.0);
	}	
	/* Y cylinder */
	glTranslatef(-1.0, 1.0, 0.0);
	if(drawflags & MAN_ROT_Y) {
		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Y);
		glRotatef(-90.0, 1.0, 0.0, 0.0);
		manipulator_setcolor('y', colcode);
		draw_cylinder(qobj, cylen, cywid);
	}
	
	/* restore */
	
	gluDeleteQuadric(qobj);
	myloadmatrix(G.vd->viewmat);
	
	if(G.vd->zbuf) glEnable(GL_DEPTH_TEST);
	
}


/* ********************************************* */

float get_drawsize(View3D *v3d, float *co)
{
	ScrArea *sa = v3d->area;
	float size, vec[3], len1, len2;
	
	/* size calculus, depending ortho/persp settings, like initgrabz() */
	size= v3d->persmat[0][3]*co[0]+ v3d->persmat[1][3]*co[1]+ v3d->persmat[2][3]*co[2]+ v3d->persmat[3][3];
	
	VECCOPY(vec, v3d->persinv[0]);
	len1= Normalize(vec);
	VECCOPY(vec, v3d->persinv[1]);
	len2= Normalize(vec);
	
	size*= 0.01f*(len1>len2?len1:len2);

	/* correct for window size to make widgets appear fixed size */
	if(sa->winx > sa->winy) size*= 1000.0f/(float)sa->winx;
	else size*= 1000.0f/(float)sa->winy;

	return size;
}

static float get_manipulator_drawsize(ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;
	float size = get_drawsize(v3d, v3d->twmat[3]);
	
	size*= (float)U.tw_size;

	return size;
}

/* exported to transform_constraints.c */
/* mat, vec = default orientation and location */
/* type = transform type */
/* axis = x, y, z, c */
/* col: 0 = colored, 1 = moving, 2 = ghost */
void draw_manipulator_ext(ScrArea *sa, int type, char axis, int col, float vec[3], float mat[][3])
{
	int drawflags= 0;
	float mat4[4][4];
	int colcode;
	
	Mat4CpyMat3(mat4, mat);
	VECCOPY(mat4[3], vec);
	
	Mat4MulFloat3((float *)mat4, get_manipulator_drawsize(sa));
	
	glEnable(GL_BLEND);	// let's do it transparent by default
	if(col==0) colcode= MAN_RGB;
	else if(col==1) colcode= MAN_MOVECOL;
	else colcode= MAN_GHOST;
	
	
	if(type==TFM_ROTATION) {
		if(axis=='x') drawflags= MAN_ROT_X;
		else if(axis=='y') drawflags= MAN_ROT_Y;
		else if(axis=='z') drawflags= MAN_ROT_Z;
		else drawflags= MAN_ROT_C;
		
		draw_manipulator_rotate_cyl(mat4, col, drawflags, V3D_MANIP_ROTATE, colcode);
	}	
	else if(type==TFM_RESIZE) {
		if(axis=='x') drawflags= MAN_SCALE_X;
		else if(axis=='y') drawflags= MAN_SCALE_Y;
		else if(axis=='z') drawflags= MAN_SCALE_Z;
		else drawflags= MAN_SCALE_C;

		draw_manipulator_scale(mat4, col, drawflags, V3D_MANIP_SCALE, colcode);
	}	
	else {
		if(axis=='x') drawflags= MAN_TRANS_X;
		else if(axis=='y') drawflags= MAN_TRANS_Y;
		else if(axis=='z') drawflags= MAN_TRANS_Z;
		else drawflags= MAN_TRANS_C;

		draw_manipulator_translate(mat4, 0, drawflags, V3D_MANIP_TRANSLATE, colcode);
	}	
	

	glDisable(GL_BLEND);
}

/* main call, does calc centers & orientation too */
/* uses global G.moving */
static int drawflags= 0xFFFF;		// only for the calls below, belongs in scene...?
void BIF_draw_manipulator(ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;
	int totsel;
	
	if(!(v3d->twflag & V3D_USE_MANIPULATOR)) return;
	if(G.moving && (G.moving & G_TRANSFORM_MANIP)==0) return;
	
	if(G.moving==0) {
		v3d->twflag &= ~V3D_DRAW_MANIPULATOR;
		
		totsel= calc_manipulator_stats(sa);
		if(totsel==0) return;
		drawflags= v3d->twdrawflag;	/* set in calc_manipulator_stats */

		v3d->twflag |= V3D_DRAW_MANIPULATOR;

		/* now we can define center */
		switch(v3d->around) {
		case V3D_CENTER:
		case V3D_ACTIVE:
			v3d->twmat[3][0]= (G.scene->twmin[0] + G.scene->twmax[0])/2.0f;
			v3d->twmat[3][1]= (G.scene->twmin[1] + G.scene->twmax[1])/2.0f;
			v3d->twmat[3][2]= (G.scene->twmin[2] + G.scene->twmax[2])/2.0f;
			if(v3d->around==V3D_ACTIVE && G.obedit==NULL) {
				Object *ob= OBACT;
				if(ob && !(ob->flag & OB_POSEMODE)) 
					VECCOPY(v3d->twmat[3], ob->obmat[3]);
			}
			break;
		case V3D_LOCAL:
		case V3D_CENTROID:
			VECCOPY(v3d->twmat[3], G.scene->twcent);
			break;
		case V3D_CURSOR:
			VECCOPY(v3d->twmat[3], give_cursor());
			break;
		}
		
		Mat4MulFloat3((float *)v3d->twmat, get_manipulator_drawsize(sa));
	}
	
	if(v3d->twflag & V3D_DRAW_MANIPULATOR) {
		
		if(v3d->twtype & V3D_MANIP_ROTATE) {
			
			/* rotate has special ghosting draw, for pie chart */
			if(G.moving) draw_manipulator_rotate_ghost(v3d->twmat, drawflags);
			
			if(G.moving) glEnable(GL_BLEND);
			
			if(G.rt==3) {
				if(G.moving) draw_manipulator_rotate_cyl(v3d->twmat, 1, drawflags, v3d->twtype, MAN_MOVECOL);
				else draw_manipulator_rotate_cyl(v3d->twmat, 0, drawflags, v3d->twtype, MAN_RGB);
			}
			else
				draw_manipulator_rotate(v3d->twmat, G.moving, drawflags, v3d->twtype);
			
			glDisable(GL_BLEND);
		}
		if(v3d->twtype & V3D_MANIP_SCALE) {
			if(G.moving) {
				glEnable(GL_BLEND);
				draw_manipulator_scale(v3d->twmat, 0, drawflags, v3d->twtype, MAN_GHOST);
				draw_manipulator_scale(v3d->twmat, 1, drawflags, v3d->twtype, MAN_MOVECOL);
				glDisable(GL_BLEND);
			}
			else draw_manipulator_scale(v3d->twmat, 0, drawflags, v3d->twtype, MAN_RGB);
		}
		if(v3d->twtype & V3D_MANIP_TRANSLATE) {
			if(G.moving) {
				glEnable(GL_BLEND);
				draw_manipulator_translate(v3d->twmat, 0, drawflags, v3d->twtype, MAN_GHOST);
				draw_manipulator_translate(v3d->twmat, 1, drawflags, v3d->twtype, MAN_MOVECOL);
				glDisable(GL_BLEND);
			}
			else draw_manipulator_translate(v3d->twmat, 0, drawflags, v3d->twtype, MAN_RGB);
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
	
	setwinmatrixview3d(sa->winx, sa->winy, &rect);
	Mat4MulMat4(v3d->persmat, v3d->viewmat, sa->winmat);
	
	glSelectBuffer( 64, buffer);
	glRenderMode(GL_SELECT);
	glInitNames();	/* these two calls whatfor? It doesnt work otherwise */
	glPushName(-2);
	
	/* do the drawing */
	if(v3d->twtype & V3D_MANIP_ROTATE) {
		if(G.rt==3) draw_manipulator_rotate_cyl(v3d->twmat, 0, MAN_ROT_C & v3d->twdrawflag, v3d->twtype, MAN_RGB);
		else draw_manipulator_rotate(v3d->twmat, 0, MAN_ROT_C & v3d->twdrawflag, v3d->twtype);
	}
	if(v3d->twtype & V3D_MANIP_SCALE)
		draw_manipulator_scale(v3d->twmat, 0, MAN_SCALE_C & v3d->twdrawflag, v3d->twtype, MAN_RGB);
	if(v3d->twtype & V3D_MANIP_TRANSLATE)
		draw_manipulator_translate(v3d->twmat, 0, MAN_TRANS_C & v3d->twdrawflag, v3d->twtype, MAN_RGB);
	
	glPopName();
	hits= glRenderMode(GL_RENDER);
	
	G.f &= ~G_PICKSEL;
	setwinmatrixview3d(sa->winx, sa->winy, NULL);
	Mat4MulMat4(v3d->persmat, v3d->viewmat, sa->winmat);
	
	persp(PERSP_WIN);
	
	if(hits==1) return buffer[3];
	else if(hits>1) {
		GLuint val, dep, mindep=0, mindeprot=0, minval=0, minvalrot=0;
		int a;
		
		/* we compare the hits in buffer, but value centers highest */
		/* we also store the rotation hits separate (because of arcs) and return hits on other widgets if there are */

		for(a=0; a<hits; a++) {
			dep= buffer[4*a + 1];
			val= buffer[4*a + 3];
			
			if(val==MAN_TRANS_C) return MAN_TRANS_C;
			else if(val==MAN_SCALE_C) return MAN_SCALE_C;
			else {
				if(val & MAN_ROT_C) {
					if(minvalrot==0 || dep<mindeprot) {
						mindeprot= dep;
						minvalrot= val;
					}
				}
				else {
					if(minval==0 || dep<mindep) {
						mindep= dep;
						minval= val;
					}
				}
			}
		}
		
		if(minval)
			return minval;
		else
			return minvalrot;
	}
	return 0;
}

/* return 0; nothing happened */
int BIF_do_manipulator(ScrArea *sa)
{
	View3D *v3d= sa->spacedata.first;
	int val;
	
	if(!(v3d->twflag & V3D_USE_MANIPULATOR)) return 0;
	if(!(v3d->twflag & V3D_DRAW_MANIPULATOR)) return 0;
	
	// find the hotspots first test narrow hotspot
	val= manipulator_selectbuf(sa, 0.5f*(float)U.tw_hotspot);
	if(val) {
		checkFirstTime(); // TEMPORARY, check this before doing any transform call.
		// drawflags still global, for drawing call above
		drawflags= manipulator_selectbuf(sa, 0.2f*(float)U.tw_hotspot);
		if(drawflags==0) drawflags= val;

		if (drawflags & MAN_TRANS_C) {
			initManipulator(TFM_TRANSLATION);
			switch(drawflags) {
			case MAN_TRANS_C:
				break;
			case MAN_TRANS_X:
				if(G.qual & LR_SHIFTKEY) {
					drawflags= MAN_TRANS_Y|MAN_TRANS_Z;
					BIF_setDualAxisConstraint(v3d->twmat[1], v3d->twmat[2], " Y+Z");
				}
				else
					BIF_setSingleAxisConstraint(v3d->twmat[0], " X");
				break;
			case MAN_TRANS_Y:
				if(G.qual & LR_SHIFTKEY) {
					drawflags= MAN_TRANS_X|MAN_TRANS_Z;
					BIF_setDualAxisConstraint(v3d->twmat[0], v3d->twmat[2], " X+Z");
				}
				else
					BIF_setSingleAxisConstraint(v3d->twmat[1], " Y");
				break;
			case MAN_TRANS_Z:
				if(G.qual & LR_SHIFTKEY) {
					drawflags= MAN_TRANS_X|MAN_TRANS_Y;
					BIF_setDualAxisConstraint(v3d->twmat[0], v3d->twmat[1], " X+Y");
				}
				else
					BIF_setSingleAxisConstraint(v3d->twmat[2], " Z");
				break;
			}
			ManipulatorTransform();
		}
		else if (drawflags & MAN_SCALE_C) {
			initManipulator(TFM_RESIZE);
			switch(drawflags) {
			case MAN_SCALE_X:
				if(G.qual & LR_SHIFTKEY) {
					drawflags= MAN_SCALE_Y|MAN_SCALE_Z;
					BIF_setDualAxisConstraint(v3d->twmat[1], v3d->twmat[2], " Y+Z");
				}
				else
					BIF_setSingleAxisConstraint(v3d->twmat[0], " X");
				break;
			case MAN_SCALE_Y:
				if(G.qual & LR_SHIFTKEY) {
					drawflags= MAN_SCALE_X|MAN_SCALE_Z;
					BIF_setDualAxisConstraint(v3d->twmat[0], v3d->twmat[2], " X+Z");
				}
				else
					BIF_setSingleAxisConstraint(v3d->twmat[1], " Y");
				break;
			case MAN_SCALE_Z:
				if(G.qual & LR_SHIFTKEY) {
					drawflags= MAN_SCALE_X|MAN_SCALE_Y;
					BIF_setDualAxisConstraint(v3d->twmat[0], v3d->twmat[1], " X+Y");
				}
				else
					BIF_setSingleAxisConstraint(v3d->twmat[2], " Z");
				break;
			}
			ManipulatorTransform();
		}
		else if (drawflags == MAN_ROT_T) { /* trackbal need special case, init is different */
			initManipulator(TFM_TRACKBALL);
			ManipulatorTransform();
		}
		else if (drawflags & MAN_ROT_C) {
			initManipulator(TFM_ROTATION);
			switch(drawflags) {
			case MAN_ROT_X:
				BIF_setSingleAxisConstraint(v3d->twmat[0], " X");
				break;
			case MAN_ROT_Y:
				BIF_setSingleAxisConstraint(v3d->twmat[1], " Y");
				break;
			case MAN_ROT_Z:
				BIF_setSingleAxisConstraint(v3d->twmat[2], " Z");
				break;
			}
			ManipulatorTransform();
		}
	}
	/* after transform, restore drawflags */
	drawflags= 0xFFFF;
	
	return val;
}


