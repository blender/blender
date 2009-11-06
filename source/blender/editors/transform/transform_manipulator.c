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

#include "RNA_access.h"

#include "BKE_armature.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_lattice.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_particle.h"
#include "BKE_pointcache.h"
#include "BKE_utildefines.h"

#include "BLI_arithb.h"
#include "BLI_editVert.h"

#include "BIF_gl.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_mesh.h"
#include "ED_particle.h"
#include "ED_space_api.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "UI_resources.h"

/* local module include */
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


static int is_mat4_flipped(float mat[][4])
{
	float vec[3];

	Crossf(vec, mat[0], mat[1]);
	if( Inpf(vec, mat[2]) < 0.0 ) return 1;
	return 0;
}

/* transform widget center calc helper for below */
static void calc_tw_center(Scene *scene, float *co)
{
	float *twcent= scene->twcent;
	float *min= scene->twmin;
	float *max= scene->twmax;

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
static void stats_pose(Scene *scene, View3D *v3d, bPoseChannel *pchan)
{
	Bone *bone= pchan->bone;

	if(bone) {
		if (bone->flag & BONE_TRANSFORM) {
			calc_tw_center(scene, pchan->pose_head);
			protectflag_to_drawflags(pchan->protectflag, &v3d->twdrawflag);
		}
	}
}

/* for editmode*/
static void stats_editbone(View3D *v3d, EditBone *ebo)
{
	if (ebo->flag & BONE_EDITMODE_LOCKED)
		protectflag_to_drawflags(OB_LOCK_LOC|OB_LOCK_ROT|OB_LOCK_SCALE, &v3d->twdrawflag);
}


static int test_rotmode_euler(short rotmode)
{
	return (ELEM(rotmode, ROT_MODE_AXISANGLE, ROT_MODE_QUAT)) ? 0:1;
}

void gimbal_axis(Object *ob, float gmat[][3])
{
	if(ob->mode & OB_MODE_POSE)
	{
		bPoseChannel *pchan= NULL;

		/* use channels to get stats */
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if (pchan->bone && pchan->bone->flag & BONE_ACTIVE) {
				if(test_rotmode_euler(pchan->rotmode)) {
					break;
				}
			}
		}

		if(pchan) {
			float mat[3][3], tmat[3][3], obmat[3][3];

			EulToGimbalAxis(mat, pchan->eul, pchan->rotmode);

			/* apply bone transformation */
			Mat3MulMat3(tmat, pchan->bone->bone_mat, mat);
			
			if (pchan->parent)
			{
				float parent_mat[3][3];

				Mat3CpyMat4(parent_mat, pchan->parent->pose_mat);
				Mat3MulMat3(mat, parent_mat, tmat);

				/* needed if object transformation isn't identity */
				Mat3CpyMat4(obmat, ob->obmat);
				Mat3MulMat3(gmat, obmat, mat);
			}
			else
			{
				/* needed if object transformation isn't identity */
				Mat3CpyMat4(obmat, ob->obmat);
				Mat3MulMat3(gmat, obmat, tmat);
			}

			Mat3Ortho(gmat);
		}
	}
	else {
		if(test_rotmode_euler(ob->rotmode)) {
			
			
			if (ob->parent)
			{
				float parent_mat[3][3], amat[3][3];
				
				EulToGimbalAxis(amat, ob->rot, ob->rotmode);
				Mat3CpyMat4(parent_mat, ob->parent->obmat);
				Mat3Ortho(parent_mat);
				Mat3MulMat3(gmat, parent_mat, amat);
			}
			else
			{
				EulToGimbalAxis(gmat, ob->rot, ob->rotmode);
			}
		}
	}
}


/* centroid, boundbox, of selection */
/* returns total items selected */
int calc_manipulator_stats(const bContext *C)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	Scene *scene= CTX_data_scene(C);
	Object *obedit= CTX_data_edit_object(C);
	View3D *v3d= sa->spacedata.first;
	RegionView3D *rv3d= ar->regiondata;
	Base *base;
	Object *ob= OBACT;
	int a, totsel= 0;

	/* transform widget matrix */
	Mat4One(rv3d->twmat);

	v3d->twdrawflag= 0xFFFF;

	/* transform widget centroid/center */
	scene->twcent[0]= scene->twcent[1]= scene->twcent[2]= 0.0f;
	INIT_MINMAX(scene->twmin, scene->twmax);

	if(obedit) {
		ob= obedit;
		if((ob->lay & v3d->lay)==0) return 0;

		if(obedit->type==OB_MESH) {
			EditMesh *em = BKE_mesh_get_editmesh(obedit->data);
			EditVert *eve;
			EditSelection ese;
			float vec[3]= {0,0,0};

			/* USE LAST SELECTE WITH ACTIVE */
			if (v3d->around==V3D_ACTIVE && EM_get_actSelection(em, &ese)) {
				EM_editselection_center(vec, &ese);
				calc_tw_center(scene, vec);
				totsel= 1;
			} else {
				/* do vertices for center, and if still no normal found, use vertex normals */
				for(eve= em->verts.first; eve; eve= eve->next) {
					if(eve->f & SELECT) {
						totsel++;
						calc_tw_center(scene, eve->co);
					}
				}
			}
		} /* end editmesh */
		else if (obedit->type==OB_ARMATURE){
			bArmature *arm= obedit->data;
			EditBone *ebo;
			for (ebo= arm->edbo->first; ebo; ebo=ebo->next){
				if(ebo->layer & arm->layer) {
					if (ebo->flag & BONE_TIPSEL) {
						calc_tw_center(scene, ebo->tail);
						totsel++;
					}
					if (ebo->flag & BONE_ROOTSEL) {
						calc_tw_center(scene, ebo->head);
						totsel++;
					}
					if (ebo->flag & BONE_SELECTED) {
						stats_editbone(v3d, ebo);
					}
				}
			}
		}
		else if ELEM(obedit->type, OB_CURVE, OB_SURF) {
			Curve *cu= obedit->data;
			Nurb *nu;
			BezTriple *bezt;
			BPoint *bp;

			nu= cu->editnurb->first;
			while(nu) {
				if(nu->type == CU_BEZIER) {
					bezt= nu->bezt;
					a= nu->pntsu;
					while(a--) {
						/* exceptions
						 * if handles are hidden then only check the center points.
						 * If 2 or more are selected then only use the center point too.
						 */
						if (cu->drawflag & CU_HIDE_HANDLES) {
							if (bezt->f2 & SELECT) {
								calc_tw_center(scene, bezt->vec[1]);
								totsel++;
							}
						}
						else if ( (bezt->f1 & SELECT) + (bezt->f2 & SELECT) + (bezt->f3 & SELECT) > SELECT ) {
							calc_tw_center(scene, bezt->vec[1]);
							totsel++;
						}
						else {
							if(bezt->f1) {
								calc_tw_center(scene, bezt->vec[0]);
								totsel++;
							}
							if(bezt->f2) {
								calc_tw_center(scene, bezt->vec[1]);
								totsel++;
							}
							if(bezt->f3) {
								calc_tw_center(scene, bezt->vec[2]);
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
							calc_tw_center(scene, bp->vec);
							totsel++;
						}
						bp++;
					}
				}
				nu= nu->next;
			}
		}
		else if(obedit->type==OB_MBALL) {
			MetaBall *mb = (MetaBall*)obedit->data;
			MetaElem *ml, *ml_sel=NULL;

			ml= mb->editelems->first;
			while(ml) {
				if(ml->flag & SELECT) {
					calc_tw_center(scene, &ml->x);
					ml_sel = ml;
					totsel++;
				}
				ml= ml->next;
			}
		}
		else if(obedit->type==OB_LATTICE) {
			BPoint *bp;
			Lattice *lt= obedit->data;

			bp= lt->editlatt->def;

			a= lt->editlatt->pntsu*lt->editlatt->pntsv*lt->editlatt->pntsw;
			while(a--) {
				if(bp->f1 & SELECT) {
					calc_tw_center(scene, bp->vec);
					totsel++;
				}
				bp++;
			}
		}

		/* selection center */
		if(totsel) {
			VecMulf(scene->twcent, 1.0f/(float)totsel);	// centroid!
			Mat4MulVecfl(obedit->obmat, scene->twcent);
			Mat4MulVecfl(obedit->obmat, scene->twmin);
			Mat4MulVecfl(obedit->obmat, scene->twmax);
		}
	}
	else if(ob && (ob->mode & OB_MODE_POSE)) {
		bPoseChannel *pchan;
		int mode = TFM_ROTATION; // mislead counting bones... bah. We don't know the manipulator mode, could be mixed

		if((ob->lay & v3d->lay)==0) return 0;

		totsel = count_set_pose_transflags(&mode, 0, ob);

		if(totsel) {
			/* use channels to get stats */
			for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
				stats_pose(scene, v3d, pchan);
			}

			VecMulf(scene->twcent, 1.0f/(float)totsel);	// centroid!
			Mat4MulVecfl(ob->obmat, scene->twcent);
			Mat4MulVecfl(ob->obmat, scene->twmin);
			Mat4MulVecfl(ob->obmat, scene->twmax);
		}
	}
	else if(ob && (ob->mode & (OB_MODE_SCULPT|OB_MODE_VERTEX_PAINT|OB_MODE_WEIGHT_PAINT|OB_MODE_TEXTURE_PAINT))) {
		;
	}
	else if(ob && ob->mode & OB_MODE_PARTICLE_EDIT) {
		PTCacheEdit *edit= PE_get_current(scene, ob);
		PTCacheEditPoint *point;
		PTCacheEditKey *ek;
		int k;

		if(edit) {
			point = edit->points;
			for(a=0; a<edit->totpoint; a++,point++) {
				if(point->flag & PEP_HIDE) continue;

				for(k=0, ek=point->keys; k<point->totkey; k++, ek++) {
					if(ek->flag & PEK_SELECT) {
						calc_tw_center(scene, ek->flag & PEK_USE_WCO ? ek->world_co : ek->co);
						totsel++;
					}
				}
			}

			/* selection center */
			if(totsel)
				VecMulf(scene->twcent, 1.0f/(float)totsel);	// centroid!
		}
	}
	else {

		/* we need the one selected object, if its not active */
		ob= OBACT;
		if(ob && !(ob->flag & SELECT)) ob= NULL;

		for(base= scene->base.first; base; base= base->next) {
			if TESTBASELIB(scene, base) {
				if(ob==NULL)
					ob= base->object;
				calc_tw_center(scene, base->object->obmat[3]);
				protectflag_to_drawflags(base->object->protectflag, &v3d->twdrawflag);
				totsel++;
			}
		}

		/* selection center */
		if(totsel) {
			VecMulf(scene->twcent, 1.0f/(float)totsel);	// centroid!
		}
	}

	/* global, local or normal orientation? */
	if(ob && totsel) {

		switch(v3d->twmode) {
		
		case V3D_MANIP_GLOBAL:
			break; /* nothing to do */

		case V3D_MANIP_GIMBAL:
		{
			float mat[3][3];
			Mat3One(mat);
			gimbal_axis(ob, mat);
			Mat4CpyMat3(rv3d->twmat, mat);
			break;
		}
		case V3D_MANIP_NORMAL:
			if(obedit || ob->mode & OB_MODE_POSE) {
				getTransformOrientationMatrix(C, rv3d->twmat, (v3d->around == V3D_ACTIVE));
				break;
			}
			/* no break we define 'normal' as 'local' in Object mode */
		case V3D_MANIP_LOCAL:
			Mat4CpyMat4(rv3d->twmat, ob->obmat);
			Mat4Ortho(rv3d->twmat);
			break;

		case V3D_MANIP_VIEW:
			{
				float mat[3][3];
				Mat3CpyMat4(mat, rv3d->viewinv);
				Mat3Ortho(mat);
				Mat4CpyMat3(rv3d->twmat, mat);
			}
			break;
		default: /* V3D_MANIP_CUSTOM */
			{
				float mat[3][3];
				applyTransformOrientation(C, mat, NULL);
				Mat4CpyMat3(rv3d->twmat, mat);
				break;
			}
		}

	}

	return totsel;
}

/* ******************** DRAWING STUFFIES *********** */

static float screen_aligned(RegionView3D *rv3d, float mat[][4])
{
	float vec[3], size;

	VECCOPY(vec, mat[0]);
	size= Normalize(vec);

	glTranslatef(mat[3][0], mat[3][1], mat[3][2]);

	/* sets view screen aligned */
	glRotatef( -360.0f*saacos(rv3d->viewquat[0])/(float)M_PI, rv3d->viewquat[1], rv3d->viewquat[2], rv3d->viewquat[3]);

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
static void manipulator_setcolor(View3D *v3d, char axis, int colcode)
{
	float vec[4];
	char col[4];

	vec[3]= 0.7f; // alpha set on 0.5, can be glEnabled or not

	if(colcode==MAN_GHOST) {
		glColor4ub(0, 0, 0, 70);
	}
	else if(colcode==MAN_MOVECOL) {
		UI_GetThemeColor3ubv(TH_TRANSFORM, col);
		glColor4ub(col[0], col[1], col[2], 128);
	}
	else {
		switch(axis) {
		case 'c':
			UI_GetThemeColor3ubv(TH_TRANSFORM, col);
			if(v3d->twmode == V3D_MANIP_LOCAL) {
				col[0]= col[0]>200?255:col[0]+55;
				col[1]= col[1]>200?255:col[1]+55;
				col[2]= col[2]>200?255:col[2]+55;
			}
			else if(v3d->twmode == V3D_MANIP_NORMAL) {
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
static void draw_manipulator_axes(View3D *v3d, int colcode, int flagx, int flagy, int flagz)
{

	/* axes */
	if(flagx) {
		manipulator_setcolor(v3d, 'x', colcode);
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
		manipulator_setcolor(v3d, 'y', colcode);
		glBegin(GL_LINES);
		glVertex3f(0.0f, 0.2f, 0.0f);
		glVertex3f(0.0f, 1.0f, 0.0f);
		glEnd();
	}
	if(flagz) {
		if(flagz & MAN_SCALE_Z) glLoadName(MAN_SCALE_Z);
		else if(flagz & MAN_TRANS_Z) glLoadName(MAN_TRANS_Z);
		manipulator_setcolor(v3d, 'z', colcode);
		glBegin(GL_LINES);
		glVertex3f(0.0f, 0.0f, 0.2f);
		glVertex3f(0.0f, 0.0f, 1.0f);
		glEnd();
	}
}

static void preOrtho(int ortho, float twmat[][4], int axis)
{
	if (ortho == 0) {
		float omat[4][4];
		Mat4CpyMat4(omat, twmat);
		Mat4Orthogonal(omat, axis);
		glPushMatrix();
		wmMultMatrix(omat);
	}
}

static void preOrthoFront(int ortho, float twmat[][4], int axis)
{
	if (ortho == 0) {
		float omat[4][4];
		Mat4CpyMat4(omat, twmat);
		Mat4Orthogonal(omat, axis);
		glPushMatrix();
		wmMultMatrix(omat);
		glFrontFace( is_mat4_flipped(omat)?GL_CW:GL_CCW);
	}
}

static void postOrtho(int ortho)
{
	if (ortho == 0) {
		glPopMatrix();
	}
}

/* only called while G.moving */
static void draw_manipulator_rotate_ghost(View3D *v3d, RegionView3D *rv3d, int drawflags)
{
	GLUquadricObj *qobj;
	float size, phi, startphi, vec[3], svec[3], matt[4][4], cross[3], tmat[3][3];
	int arcs= (G.rt!=2);
	int ortho;

	glDisable(GL_DEPTH_TEST);

	qobj= gluNewQuadric();
	gluQuadricDrawStyle(qobj, GLU_FILL);

	glColor4ub(0,0,0,64);
	glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	/* we need both [4][4] transforms, t->mat seems to be premul, not post for mat[][4] */
	Mat4CpyMat4(matt, rv3d->twmat); // to copy the parts outside of [3][3]
// XXX	Mat4MulMat34(matt, t->mat, rv3d->twmat);

	/* Screen aligned view rot circle */
	if(drawflags & MAN_ROT_V) {

		/* prepare for screen aligned draw */
		glPushMatrix();
		size= screen_aligned(rv3d, rv3d->twmat);

		vec[0]= 0; // XXX (float)(t->con.imval[0] - t->center2d[0]);
		vec[1]= 0; // XXX (float)(t->con.imval[1] - t->center2d[1]);
		vec[2]= 0.0f;
		Normalize(vec);

		startphi= saacos( vec[1] );
		if(vec[0]<0.0) startphi= -startphi;

		phi= 0; // XXX (float)fmod(180.0*t->val/M_PI, 360.0);
		if(phi > 180.0) phi-= 360.0;
		else if(phi<-180.0) phi+= 360.0;

		gluPartialDisk(qobj, 0.0, size, 32, 1, 180.0*startphi/M_PI, phi);

		glPopMatrix();
	}
	else if(arcs) {
		float imat[3][3], ivmat[3][3];
		/* try to get the start rotation */

		svec[0]= 0; // XXX (float)(t->con.imval[0] - t->center2d[0]);
		svec[1]= 0; // XXX (float)(t->con.imval[1] - t->center2d[1]);
		svec[2]= 0.0f;

		/* screen aligned vec transform back to manipulator space */
		Mat3CpyMat4(ivmat, rv3d->viewinv);
		Mat3CpyMat4(tmat, rv3d->twmat);
		Mat3Inv(imat, tmat);
		Mat3MulMat3(tmat, imat, ivmat);

		Mat3MulVecfl(tmat, svec);	// tmat is used further on
		Normalize(svec);
	}
	
	ortho = IsMat4Orthogonal(rv3d->twmat);

	if (ortho) {
		wmMultMatrix(rv3d->twmat);	// aligns with original widget
	}
	

	/* Z disk */
	if(drawflags & MAN_ROT_Z) {
		preOrtho(ortho, rv3d->twmat, 2);
		
		if(arcs) {
			/* correct for squeezed arc */
			svec[0]+= tmat[2][0];
			svec[1]+= tmat[2][1];
			Normalize(svec);

			startphi= (float)atan2(svec[0], svec[1]);
		}
		else startphi= 0.5f*(float)M_PI;

		VECCOPY(vec, rv3d->twmat[0]);	// use x axis to detect rotation
		Normalize(vec);
		Normalize(matt[0]);
		phi= saacos( Inpf(vec, matt[0]) );
		if(phi!=0.0) {
			Crossf(cross, vec, matt[0]);	// results in z vector
			if(Inpf(cross, rv3d->twmat[2]) > 0.0) phi= -phi;
			gluPartialDisk(qobj, 0.0, 1.0, 32, 1, 180.0*startphi/M_PI, 180.0*(phi)/M_PI);
		}

		postOrtho(ortho);
	}
	/* X disk */
	if(drawflags & MAN_ROT_X) {
		preOrtho(ortho, rv3d->twmat, 0);
		
		if(arcs) {
			/* correct for squeezed arc */
			svec[1]+= tmat[2][1];
			svec[2]+= tmat[2][2];
			Normalize(svec);

			startphi= (float)(M_PI + atan2(svec[2], -svec[1]));
		}
		else startphi= 0.0f;

		VECCOPY(vec, rv3d->twmat[1]);	// use y axis to detect rotation
		Normalize(vec);
		Normalize(matt[1]);
		phi= saacos( Inpf(vec, matt[1]) );
		if(phi!=0.0) {
			Crossf(cross, vec, matt[1]);	// results in x vector
			if(Inpf(cross, rv3d->twmat[0]) > 0.0) phi= -phi;
			glRotatef(90.0, 0.0, 1.0, 0.0);
			gluPartialDisk(qobj, 0.0, 1.0, 32, 1, 180.0*startphi/M_PI, 180.0*phi/M_PI);
			glRotatef(-90.0, 0.0, 1.0, 0.0);
		}

		postOrtho(ortho);
	}
	/* Y circle */
	if(drawflags & MAN_ROT_Y) {
		preOrtho(ortho, rv3d->twmat, 1);
		
		if(arcs) {
			/* correct for squeezed arc */
			svec[0]+= tmat[2][0];
			svec[2]+= tmat[2][2];
			Normalize(svec);

			startphi= (float)(M_PI + atan2(-svec[0], svec[2]));
		}
		else startphi= (float)M_PI;

		VECCOPY(vec, rv3d->twmat[2]);	// use z axis to detect rotation
		Normalize(vec);
		Normalize(matt[2]);
		phi= saacos( Inpf(vec, matt[2]) );
		if(phi!=0.0) {
			Crossf(cross, vec, matt[2]);	// results in y vector
			if(Inpf(cross, rv3d->twmat[1]) > 0.0) phi= -phi;
			glRotatef(-90.0, 1.0, 0.0, 0.0);
			gluPartialDisk(qobj, 0.0, 1.0, 32, 1, 180.0*startphi/M_PI, 180.0*phi/M_PI);
			glRotatef(90.0, 1.0, 0.0, 0.0);
		}

		postOrtho(ortho);
	}

	glDisable(GL_BLEND);
	wmLoadMatrix(rv3d->viewmat);
}

static void draw_manipulator_rotate(View3D *v3d, RegionView3D *rv3d, int moving, int drawflags, int combo)
{
	GLUquadricObj *qobj;
	double plane[4];
	float matt[4][4];
	float size, vec[3], unitmat[4][4];
	float cywid= 0.33f*0.01f*(float)U.tw_handlesize;
	float cusize= cywid*0.65f;
	int arcs= (G.rt!=2);
	int colcode;
	int ortho;

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
	VECCOPY(vec, rv3d->twmat[0]);
	size= Normalize(vec);
	glPushMatrix();
	glTranslatef(rv3d->twmat[3][0], rv3d->twmat[3][1], rv3d->twmat[3][2]);

	if(arcs) {
		/* clipplane makes nice handles, calc here because of multmatrix but with translate! */
		VECCOPY(plane, rv3d->viewinv[2]);
		plane[3]= -0.02*size; // clip just a bit more
		glClipPlane(GL_CLIP_PLANE0, plane);
	}
	/* sets view screen aligned */
	glRotatef( -360.0f*saacos(rv3d->viewquat[0])/(float)M_PI, rv3d->viewquat[1], rv3d->viewquat[2], rv3d->viewquat[3]);

	/* Screen aligned help circle */
	if(arcs) {
		if((G.f & G_PICKSEL)==0) {
			UI_ThemeColorShade(TH_BACK, -30);
			drawcircball(GL_LINE_LOOP, unitmat[3], size, unitmat);
		}
	}

	/* Screen aligned trackball rot circle */
	if(drawflags & MAN_ROT_T) {
		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_T);

		UI_ThemeColor(TH_TRANSFORM);
		drawcircball(GL_LINE_LOOP, unitmat[3], 0.2f*size, unitmat);
	}

	/* Screen aligned view rot circle */
	if(drawflags & MAN_ROT_V) {
		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_V);
		UI_ThemeColor(TH_TRANSFORM);
		drawcircball(GL_LINE_LOOP, unitmat[3], 1.2f*size, unitmat);

		if(moving) {
			float vec[3];
			vec[0]= 0; // XXX (float)(t->imval[0] - t->center2d[0]);
			vec[1]= 0; // XXX (float)(t->imval[1] - t->center2d[1]);
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


	ortho = IsMat4Orthogonal(rv3d->twmat);
	
	/* apply the transform delta */
	if(moving) {
		Mat4CpyMat4(matt, rv3d->twmat); // to copy the parts outside of [3][3]
		// XXX Mat4MulMat34(matt, t->mat, rv3d->twmat);
		if (ortho) {
			wmMultMatrix(matt);
			glFrontFace( is_mat4_flipped(matt)?GL_CW:GL_CCW);
		}
	}
	else {
		if (ortho) {
			glFrontFace( is_mat4_flipped(rv3d->twmat)?GL_CW:GL_CCW);
			wmMultMatrix(rv3d->twmat);
		}
	}

	/* axes */
	if(arcs==0) {
		if(!(G.f & G_PICKSEL)) {
			if( (combo & V3D_MANIP_SCALE)==0) {
				/* axis */
				if( (drawflags & MAN_ROT_X) || (moving && (drawflags & MAN_ROT_Z)) ) {
					preOrthoFront(ortho, rv3d->twmat, 2);
					manipulator_setcolor(v3d, 'x', colcode);
					glBegin(GL_LINES);
					glVertex3f(0.2f, 0.0f, 0.0f);
					glVertex3f(1.0f, 0.0f, 0.0f);
					glEnd();
					postOrtho(ortho);
				}
				if( (drawflags & MAN_ROT_Y) || (moving && (drawflags & MAN_ROT_X)) ) {
					preOrthoFront(ortho, rv3d->twmat, 0);
					manipulator_setcolor(v3d, 'y', colcode);
					glBegin(GL_LINES);
					glVertex3f(0.0f, 0.2f, 0.0f);
					glVertex3f(0.0f, 1.0f, 0.0f);
					glEnd();
					postOrtho(ortho);
				}
				if( (drawflags & MAN_ROT_Z) || (moving && (drawflags & MAN_ROT_Y)) ) {
					preOrthoFront(ortho, rv3d->twmat, 1);
					manipulator_setcolor(v3d, 'z', colcode);
					glBegin(GL_LINES);
					glVertex3f(0.0f, 0.0f, 0.2f);
					glVertex3f(0.0f, 0.0f, 1.0f);
					glEnd();
					postOrtho(ortho);
				}
			}
		}
	}

	if(arcs==0 && moving) {

		/* Z circle */
		if(drawflags & MAN_ROT_Z) {
			preOrthoFront(ortho, matt, 2);
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Z);
			manipulator_setcolor(v3d, 'z', colcode);
			drawcircball(GL_LINE_LOOP, unitmat[3], 1.0, unitmat);
			postOrtho(ortho);
		}
		/* X circle */
		if(drawflags & MAN_ROT_X) {
			preOrthoFront(ortho, matt, 0);
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_X);
			glRotatef(90.0, 0.0, 1.0, 0.0);
			manipulator_setcolor(v3d, 'x', colcode);
			drawcircball(GL_LINE_LOOP, unitmat[3], 1.0, unitmat);
			glRotatef(-90.0, 0.0, 1.0, 0.0);
			postOrtho(ortho);
		}
		/* Y circle */
		if(drawflags & MAN_ROT_Y) {
			preOrthoFront(ortho, matt, 1);
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Y);
			glRotatef(-90.0, 1.0, 0.0, 0.0);
			manipulator_setcolor(v3d, 'y', colcode);
			drawcircball(GL_LINE_LOOP, unitmat[3], 1.0, unitmat);
			glRotatef(90.0, 1.0, 0.0, 0.0);
			postOrtho(ortho);
		}

		if(arcs) glDisable(GL_CLIP_PLANE0);
	}
	// donut arcs
	if(arcs) {
		glEnable(GL_CLIP_PLANE0);

		/* Z circle */
		if(drawflags & MAN_ROT_Z) {
			preOrthoFront(ortho, rv3d->twmat, 2);
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Z);
			manipulator_setcolor(v3d, 'z', colcode);
			partial_donut(cusize/4.0f, 1.0f, 0, 48, 8, 48);
			postOrtho(ortho);
		}
		/* X circle */
		if(drawflags & MAN_ROT_X) {
			preOrthoFront(ortho, rv3d->twmat, 0);
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_X);
			glRotatef(90.0, 0.0, 1.0, 0.0);
			manipulator_setcolor(v3d, 'x', colcode);
			partial_donut(cusize/4.0f, 1.0f, 0, 48, 8, 48);
			glRotatef(-90.0, 0.0, 1.0, 0.0);
			postOrtho(ortho);
		}
		/* Y circle */
		if(drawflags & MAN_ROT_Y) {
			preOrthoFront(ortho, rv3d->twmat, 1);
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Y);
			glRotatef(-90.0, 1.0, 0.0, 0.0);
			manipulator_setcolor(v3d, 'y', colcode);
			partial_donut(cusize/4.0f, 1.0f, 0, 48, 8, 48);
			glRotatef(90.0, 1.0, 0.0, 0.0);
			postOrtho(ortho);
		}

		glDisable(GL_CLIP_PLANE0);
	}

	if(arcs==0) {

		/* Z handle on X axis */
		if(drawflags & MAN_ROT_Z) {
			preOrthoFront(ortho, rv3d->twmat, 2);
			glPushMatrix();
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Z);
			manipulator_setcolor(v3d, 'z', colcode);

			partial_donut(0.7f*cusize, 1.0f, 31, 33, 8, 64);

			glPopMatrix();
			postOrtho(ortho);
		}

		/* Y handle on X axis */
		if(drawflags & MAN_ROT_Y) {
			preOrthoFront(ortho, rv3d->twmat, 1);
			glPushMatrix();
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Y);
			manipulator_setcolor(v3d, 'y', colcode);

			glRotatef(90.0, 1.0, 0.0, 0.0);
			glRotatef(90.0, 0.0, 0.0, 1.0);
			partial_donut(0.7f*cusize, 1.0f, 31, 33, 8, 64);

			glPopMatrix();
			postOrtho(ortho);
		}

		/* X handle on Z axis */
		if(drawflags & MAN_ROT_X) {
			preOrthoFront(ortho, rv3d->twmat, 0);
			glPushMatrix();
			if(G.f & G_PICKSEL) glLoadName(MAN_ROT_X);
			manipulator_setcolor(v3d, 'x', colcode);

			glRotatef(-90.0, 0.0, 1.0, 0.0);
			glRotatef(90.0, 0.0, 0.0, 1.0);
			partial_donut(0.7f*cusize, 1.0f, 31, 33, 8, 64);

			glPopMatrix();
			postOrtho(ortho);
		}

	}

	/* restore */
	wmLoadMatrix(rv3d->viewmat);
	gluDeleteQuadric(qobj);
	if(v3d->zbuf) glEnable(GL_DEPTH_TEST);

}

static void drawsolidcube(float size)
{
	static float cube[8][3] = {
	{-1.0, -1.0, -1.0},
	{-1.0, -1.0,  1.0},
	{-1.0,  1.0,  1.0},
	{-1.0,  1.0, -1.0},
	{ 1.0, -1.0, -1.0},
	{ 1.0, -1.0,  1.0},
	{ 1.0,  1.0,  1.0},
	{ 1.0,  1.0, -1.0},	};
	float n[3];

	glPushMatrix();
	glScalef(size, size, size);

	n[0]=0; n[1]=0; n[2]=0;
	glBegin(GL_QUADS);
	n[0]= -1.0;
	glNormal3fv(n);
	glVertex3fv(cube[0]); glVertex3fv(cube[1]); glVertex3fv(cube[2]); glVertex3fv(cube[3]);
	n[0]=0;
	glEnd();

	glBegin(GL_QUADS);
	n[1]= -1.0;
	glNormal3fv(n);
	glVertex3fv(cube[0]); glVertex3fv(cube[4]); glVertex3fv(cube[5]); glVertex3fv(cube[1]);
	n[1]=0;
	glEnd();

	glBegin(GL_QUADS);
	n[0]= 1.0;
	glNormal3fv(n);
	glVertex3fv(cube[4]); glVertex3fv(cube[7]); glVertex3fv(cube[6]); glVertex3fv(cube[5]);
	n[0]=0;
	glEnd();

	glBegin(GL_QUADS);
	n[1]= 1.0;
	glNormal3fv(n);
	glVertex3fv(cube[7]); glVertex3fv(cube[3]); glVertex3fv(cube[2]); glVertex3fv(cube[6]);
	n[1]=0;
	glEnd();

	glBegin(GL_QUADS);
	n[2]= 1.0;
	glNormal3fv(n);
	glVertex3fv(cube[1]); glVertex3fv(cube[5]); glVertex3fv(cube[6]); glVertex3fv(cube[2]);
	n[2]=0;
	glEnd();

	glBegin(GL_QUADS);
	n[2]= -1.0;
	glNormal3fv(n);
	glVertex3fv(cube[7]); glVertex3fv(cube[4]); glVertex3fv(cube[0]); glVertex3fv(cube[3]);
	glEnd();

	glPopMatrix();
}


static void draw_manipulator_scale(View3D *v3d, RegionView3D *rv3d, int moving, int drawflags, int combo, int colcode)
{
	float cywid= 0.25f*0.01f*(float)U.tw_handlesize;
	float cusize= cywid*0.75f, dz;

	/* when called while moving in mixed mode, do not draw when... */
	if((drawflags & MAN_SCALE_C)==0) return;

	glDisable(GL_DEPTH_TEST);

	/* not in combo mode */
	if( (combo & (V3D_MANIP_TRANSLATE|V3D_MANIP_ROTATE))==0) {
		float size, unitmat[4][4];
		int shift= 0; // XXX

		/* center circle, do not add to selection when shift is pressed (planar constraint)  */
		if( (G.f & G_PICKSEL) && shift==0) glLoadName(MAN_SCALE_C);

		manipulator_setcolor(v3d, 'c', colcode);
		glPushMatrix();
		size= screen_aligned(rv3d, rv3d->twmat);
		Mat4One(unitmat);
		drawcircball(GL_LINE_LOOP, unitmat[3], 0.2f*size, unitmat);
		glPopMatrix();

		dz= 1.0;
	}
	else dz= 1.0f-4.0f*cusize;

	if(moving) {
		float matt[4][4];

		Mat4CpyMat4(matt, rv3d->twmat); // to copy the parts outside of [3][3]
		// XXX Mat4MulMat34(matt, t->mat, rv3d->twmat);
		wmMultMatrix(matt);
		glFrontFace( is_mat4_flipped(matt)?GL_CW:GL_CCW);
	}
	else {
		wmMultMatrix(rv3d->twmat);
		glFrontFace( is_mat4_flipped(rv3d->twmat)?GL_CW:GL_CCW);
	}

	/* axis */

	/* in combo mode, this is always drawn as first type */
	draw_manipulator_axes(v3d, colcode, drawflags & MAN_SCALE_X, drawflags & MAN_SCALE_Y, drawflags & MAN_SCALE_Z);

	/* Z cube */
	glTranslatef(0.0, 0.0, dz);
	if(drawflags & MAN_SCALE_Z) {
		if(G.f & G_PICKSEL) glLoadName(MAN_SCALE_Z);
		manipulator_setcolor(v3d, 'z', colcode);
		drawsolidcube(cusize);
	}
	/* X cube */
	glTranslatef(dz, 0.0, -dz);
	if(drawflags & MAN_SCALE_X) {
		if(G.f & G_PICKSEL) glLoadName(MAN_SCALE_X);
		manipulator_setcolor(v3d, 'x', colcode);
		drawsolidcube(cusize);
	}
	/* Y cube */
	glTranslatef(-dz, dz, 0.0);
	if(drawflags & MAN_SCALE_Y) {
		if(G.f & G_PICKSEL) glLoadName(MAN_SCALE_Y);
		manipulator_setcolor(v3d, 'y', colcode);
		drawsolidcube(cusize);
	}

	/* if shiftkey, center point as last, for selectbuffer order */
	if(G.f & G_PICKSEL) {
		int shift= 0; // XXX

		if(shift) {
			glTranslatef(0.0, -dz, 0.0);
			glLoadName(MAN_SCALE_C);
			glBegin(GL_POINTS);
			glVertex3f(0.0, 0.0, 0.0);
			glEnd();
		}
	}

	/* restore */
	wmLoadMatrix(rv3d->viewmat);

	if(v3d->zbuf) glEnable(GL_DEPTH_TEST);
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


static void draw_manipulator_translate(View3D *v3d, RegionView3D *rv3d, int moving, int drawflags, int combo, int colcode)
{
	GLUquadricObj *qobj;
	float cylen= 0.01f*(float)U.tw_handlesize;
	float cywid= 0.25f*cylen, dz, size;
	float unitmat[4][4];
	int shift= 0; // XXX

	/* when called while moving in mixed mode, do not draw when... */
	if((drawflags & MAN_TRANS_C)==0) return;

	// XXX if(moving) glTranslatef(t->vec[0], t->vec[1], t->vec[2]);
	glDisable(GL_DEPTH_TEST);

	qobj= gluNewQuadric();
	gluQuadricDrawStyle(qobj, GLU_FILL);

	/* center circle, do not add to selection when shift is pressed (planar constraint) */
	if( (G.f & G_PICKSEL) && shift==0) glLoadName(MAN_TRANS_C);

	manipulator_setcolor(v3d, 'c', colcode);
	glPushMatrix();
	size= screen_aligned(rv3d, rv3d->twmat);
	Mat4One(unitmat);
	drawcircball(GL_LINE_LOOP, unitmat[3], 0.2f*size, unitmat);
	glPopMatrix();

	/* and now apply matrix, we move to local matrix drawing */
	wmMultMatrix(rv3d->twmat);

	/* axis */
	glLoadName(-1);

	// translate drawn as last, only axis when no combo with scale, or for ghosting
	if((combo & V3D_MANIP_SCALE)==0 || colcode==MAN_GHOST)
		draw_manipulator_axes(v3d, colcode, drawflags & MAN_TRANS_X, drawflags & MAN_TRANS_Y, drawflags & MAN_TRANS_Z);


	/* offset in combo mode, for rotate a bit more */
	if(combo & (V3D_MANIP_ROTATE)) dz= 1.0f+2.0f*cylen;
	else if(combo & (V3D_MANIP_SCALE)) dz= 1.0f+0.5f*cylen;
	else dz= 1.0f;

	/* Z Cone */
	glTranslatef(0.0, 0.0, dz);
	if(drawflags & MAN_TRANS_Z) {
		if(G.f & G_PICKSEL) glLoadName(MAN_TRANS_Z);
		manipulator_setcolor(v3d, 'z', colcode);
		draw_cone(qobj, cylen, cywid);
	}
	/* X Cone */
	glTranslatef(dz, 0.0, -dz);
	if(drawflags & MAN_TRANS_X) {
		if(G.f & G_PICKSEL) glLoadName(MAN_TRANS_X);
		glRotatef(90.0, 0.0, 1.0, 0.0);
		manipulator_setcolor(v3d, 'x', colcode);
		draw_cone(qobj, cylen, cywid);
		glRotatef(-90.0, 0.0, 1.0, 0.0);
	}
	/* Y Cone */
	glTranslatef(-dz, dz, 0.0);
	if(drawflags & MAN_TRANS_Y) {
		if(G.f & G_PICKSEL) glLoadName(MAN_TRANS_Y);
		glRotatef(-90.0, 1.0, 0.0, 0.0);
		manipulator_setcolor(v3d, 'y', colcode);
		draw_cone(qobj, cylen, cywid);
	}

	gluDeleteQuadric(qobj);
	wmLoadMatrix(rv3d->viewmat);

	if(v3d->zbuf) glEnable(GL_DEPTH_TEST);

}

static void draw_manipulator_rotate_cyl(View3D *v3d, RegionView3D *rv3d, int moving, int drawflags, int combo, int colcode)
{
	GLUquadricObj *qobj;
	float size;
	float cylen= 0.01f*(float)U.tw_handlesize;
	float cywid= 0.25f*cylen;

	/* when called while moving in mixed mode, do not draw when... */
	if((drawflags & MAN_ROT_C)==0) return;

	/* prepare for screen aligned draw */
	glPushMatrix();
	size= screen_aligned(rv3d, rv3d->twmat);

	glDisable(GL_DEPTH_TEST);

	qobj= gluNewQuadric();

	/* Screen aligned view rot circle */
	if(drawflags & MAN_ROT_V) {
		float unitmat[4][4];
		Mat4One(unitmat);

		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_V);
		UI_ThemeColor(TH_TRANSFORM);
		drawcircball(GL_LINE_LOOP, unitmat[3], 1.2f*size, unitmat);

		if(moving) {
			float vec[3];
			vec[0]= 0; // XXX (float)(t->imval[0] - t->center2d[0]);
			vec[1]= 0; // XXX (float)(t->imval[1] - t->center2d[1]);
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
		Mat4CpyMat4(matt, rv3d->twmat); // to copy the parts outside of [3][3]
		// XXX 		if (t->flag & T_USES_MANIPULATOR) {
		// XXX 			Mat4MulMat34(matt, t->mat, rv3d->twmat);
		// XXX }
		wmMultMatrix(matt);
	}
	else {
		wmMultMatrix(rv3d->twmat);
	}

	glFrontFace( is_mat4_flipped(rv3d->twmat)?GL_CW:GL_CCW);

	/* axis */
	if( (G.f & G_PICKSEL)==0 ) {

		// only draw axis when combo didn't draw scale axes
		if((combo & V3D_MANIP_SCALE)==0)
			draw_manipulator_axes(v3d, colcode, drawflags & MAN_ROT_X, drawflags & MAN_ROT_Y, drawflags & MAN_ROT_Z);

		/* only has to be set when not in picking */
		gluQuadricDrawStyle(qobj, GLU_FILL);
	}

	/* Z cyl */
	glTranslatef(0.0, 0.0, 1.0);
	if(drawflags & MAN_ROT_Z) {
		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Z);
		manipulator_setcolor(v3d, 'z', colcode);
		draw_cylinder(qobj, cylen, cywid);
	}
	/* X cyl */
	glTranslatef(1.0, 0.0, -1.0);
	if(drawflags & MAN_ROT_X) {
		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_X);
		glRotatef(90.0, 0.0, 1.0, 0.0);
		manipulator_setcolor(v3d, 'x', colcode);
		draw_cylinder(qobj, cylen, cywid);
		glRotatef(-90.0, 0.0, 1.0, 0.0);
	}
	/* Y cylinder */
	glTranslatef(-1.0, 1.0, 0.0);
	if(drawflags & MAN_ROT_Y) {
		if(G.f & G_PICKSEL) glLoadName(MAN_ROT_Y);
		glRotatef(-90.0, 1.0, 0.0, 0.0);
		manipulator_setcolor(v3d, 'y', colcode);
		draw_cylinder(qobj, cylen, cywid);
	}

	/* restore */

	gluDeleteQuadric(qobj);
	wmLoadMatrix(rv3d->viewmat);

	if(v3d->zbuf) glEnable(GL_DEPTH_TEST);

}


/* ********************************************* */

static float get_manipulator_drawsize(ARegion *ar)
{
	RegionView3D *rv3d= ar->regiondata;
	float size = get_drawsize(ar, rv3d->twmat[3]);

	size*= (float)U.tw_size;

	return size;
}


/* main call, does calc centers & orientation too */
/* uses global G.moving */
static int drawflags= 0xFFFF;		// only for the calls below, belongs in scene...?

void BIF_draw_manipulator(const bContext *C)
{
	ScrArea *sa= CTX_wm_area(C);
	ARegion *ar= CTX_wm_region(C);
	Scene *scene= CTX_data_scene(C);
	View3D *v3d= sa->spacedata.first;
	RegionView3D *rv3d= ar->regiondata;
	int totsel;

	if(!(v3d->twflag & V3D_USE_MANIPULATOR)) return;
//	if(G.moving && (G.moving & G_TRANSFORM_MANIP)==0) return;

//	if(G.moving==0) {
	{
		v3d->twflag &= ~V3D_DRAW_MANIPULATOR;

		totsel= calc_manipulator_stats(C);
		if(totsel==0) return;
		drawflags= v3d->twdrawflag;	/* set in calc_manipulator_stats */

		v3d->twflag |= V3D_DRAW_MANIPULATOR;

		/* now we can define center */
		switch(v3d->around) {
		case V3D_CENTER:
		case V3D_ACTIVE:
			rv3d->twmat[3][0]= (scene->twmin[0] + scene->twmax[0])/2.0f;
			rv3d->twmat[3][1]= (scene->twmin[1] + scene->twmax[1])/2.0f;
			rv3d->twmat[3][2]= (scene->twmin[2] + scene->twmax[2])/2.0f;
			if(v3d->around==V3D_ACTIVE && scene->obedit==NULL) {
				Object *ob= OBACT;
				if(ob && !(ob->mode & OB_MODE_POSE))
					VECCOPY(rv3d->twmat[3], ob->obmat[3]);
			}
			break;
		case V3D_LOCAL:
		case V3D_CENTROID:
			VECCOPY(rv3d->twmat[3], scene->twcent);
			break;
		case V3D_CURSOR:
			VECCOPY(rv3d->twmat[3], give_cursor(scene, v3d));
			break;
		}

		Mat4MulFloat3((float *)rv3d->twmat, get_manipulator_drawsize(ar));
	}

	if(v3d->twflag & V3D_DRAW_MANIPULATOR) {

		if(v3d->twtype & V3D_MANIP_ROTATE) {

			/* rotate has special ghosting draw, for pie chart */
			if(G.moving) draw_manipulator_rotate_ghost(v3d, rv3d, drawflags);

			if(G.moving) glEnable(GL_BLEND);

			if(G.rt==3) {
				if(G.moving) draw_manipulator_rotate_cyl(v3d, rv3d, 1, drawflags, v3d->twtype, MAN_MOVECOL);
				else draw_manipulator_rotate_cyl(v3d, rv3d, 0, drawflags, v3d->twtype, MAN_RGB);
			}
			else
				draw_manipulator_rotate(v3d, rv3d, 0 /* G.moving*/, drawflags, v3d->twtype);

			glDisable(GL_BLEND);
		}
		if(v3d->twtype & V3D_MANIP_SCALE) {
			if(G.moving) {
				glEnable(GL_BLEND);
				draw_manipulator_scale(v3d, rv3d, 0, drawflags, v3d->twtype, MAN_GHOST);
				draw_manipulator_scale(v3d, rv3d, 1, drawflags, v3d->twtype, MAN_MOVECOL);
				glDisable(GL_BLEND);
			}
			else draw_manipulator_scale(v3d, rv3d, 0, drawflags, v3d->twtype, MAN_RGB);
		}
		if(v3d->twtype & V3D_MANIP_TRANSLATE) {
			if(G.moving) {
				glEnable(GL_BLEND);
				draw_manipulator_translate(v3d, rv3d, 0, drawflags, v3d->twtype, MAN_GHOST);
				draw_manipulator_translate(v3d, rv3d, 1, drawflags, v3d->twtype, MAN_MOVECOL);
				glDisable(GL_BLEND);
			}
			else draw_manipulator_translate(v3d, rv3d, 0, drawflags, v3d->twtype, MAN_RGB);
		}
	}
}

static int manipulator_selectbuf(ScrArea *sa, ARegion *ar, short *mval, float hotspot)
{
	View3D *v3d= sa->spacedata.first;
	RegionView3D *rv3d= ar->regiondata;
	rctf rect;
	GLuint buffer[64];		// max 4 items per select, so large enuf
	short hits;
	extern void setwinmatrixview3d(ARegion *ar, View3D *v3d, rctf *rect); // XXX check a bit later on this... (ton)

	G.f |= G_PICKSEL;

	rect.xmin= mval[0]-hotspot;
	rect.xmax= mval[0]+hotspot;
	rect.ymin= mval[1]-hotspot;
	rect.ymax= mval[1]+hotspot;

	setwinmatrixview3d(ar, v3d, &rect);
	Mat4MulMat4(rv3d->persmat, rv3d->viewmat, rv3d->winmat);

	glSelectBuffer( 64, buffer);
	glRenderMode(GL_SELECT);
	glInitNames();	/* these two calls whatfor? It doesnt work otherwise */
	glPushName(-2);

	/* do the drawing */
	if(v3d->twtype & V3D_MANIP_ROTATE) {
		if(G.rt==3) draw_manipulator_rotate_cyl(v3d, rv3d, 0, MAN_ROT_C & v3d->twdrawflag, v3d->twtype, MAN_RGB);
		else draw_manipulator_rotate(v3d, rv3d, 0, MAN_ROT_C & v3d->twdrawflag, v3d->twtype);
	}
	if(v3d->twtype & V3D_MANIP_SCALE)
		draw_manipulator_scale(v3d, rv3d, 0, MAN_SCALE_C & v3d->twdrawflag, v3d->twtype, MAN_RGB);
	if(v3d->twtype & V3D_MANIP_TRANSLATE)
		draw_manipulator_translate(v3d, rv3d, 0, MAN_TRANS_C & v3d->twdrawflag, v3d->twtype, MAN_RGB);

	glPopName();
	hits= glRenderMode(GL_RENDER);

	G.f &= ~G_PICKSEL;
	setwinmatrixview3d(ar, v3d, NULL);
	Mat4MulMat4(rv3d->persmat, rv3d->viewmat, rv3d->winmat);

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
int BIF_do_manipulator(bContext *C, struct wmEvent *event, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	View3D *v3d= sa->spacedata.first;
	ARegion *ar= CTX_wm_region(C);
	int constraint_axis[3] = {0, 0, 0};
	int val;
	int shift = event->shift;

	if(!(v3d->twflag & V3D_USE_MANIPULATOR)) return 0;
	if(!(v3d->twflag & V3D_DRAW_MANIPULATOR)) return 0;

	// find the hotspots first test narrow hotspot
	val= manipulator_selectbuf(sa, ar, event->mval, 0.5f*(float)U.tw_hotspot);
	if(val) {

		// drawflags still global, for drawing call above
		drawflags= manipulator_selectbuf(sa, ar, event->mval, 0.2f*(float)U.tw_hotspot);
		if(drawflags==0) drawflags= val;

		if (drawflags & MAN_TRANS_C) {
			switch(drawflags) {
			case MAN_TRANS_C:
				break;
			case MAN_TRANS_X:
				if(shift) {
					constraint_axis[1] = 1;
					constraint_axis[2] = 1;
				}
				else
					constraint_axis[0] = 1;
				break;
			case MAN_TRANS_Y:
				if(shift) {
					constraint_axis[0] = 1;
					constraint_axis[2] = 1;
				}
				else
					constraint_axis[1] = 1;
				break;
			case MAN_TRANS_Z:
				if(shift) {
					constraint_axis[0] = 1;
					constraint_axis[1] = 1;
				}
				else
					constraint_axis[2] = 1;
				break;
			}
			RNA_boolean_set_array(op->ptr, "constraint_axis", constraint_axis);
			WM_operator_name_call(C, "TFM_OT_translate", WM_OP_INVOKE_DEFAULT, op->ptr);
		}
		else if (drawflags & MAN_SCALE_C) {
			switch(drawflags) {
			case MAN_SCALE_X:
				if(shift) {
					constraint_axis[1] = 1;
					constraint_axis[2] = 1;
				}
				else
					constraint_axis[0] = 1;
				break;
			case MAN_SCALE_Y:
				if(shift) {
					constraint_axis[0] = 1;
					constraint_axis[2] = 1;
				}
				else
					constraint_axis[1] = 1;
				break;
			case MAN_SCALE_Z:
				if(shift) {
					constraint_axis[0] = 1;
					constraint_axis[1] = 1;
				}
				else
					constraint_axis[2] = 1;
				break;
			}
			RNA_boolean_set_array(op->ptr, "constraint_axis", constraint_axis);
			WM_operator_name_call(C, "TFM_OT_resize", WM_OP_INVOKE_DEFAULT, op->ptr);
		}
		else if (drawflags == MAN_ROT_T) { /* trackball need special case, init is different */
			WM_operator_name_call(C, "TFM_OT_trackball", WM_OP_INVOKE_DEFAULT, op->ptr);
		}
		else if (drawflags & MAN_ROT_C) {
			switch(drawflags) {
			case MAN_ROT_X:
				constraint_axis[0] = 1;
				break;
			case MAN_ROT_Y:
				constraint_axis[1] = 1;
				break;
			case MAN_ROT_Z:
				constraint_axis[2] = 1;
				break;
			}
			RNA_boolean_set_array(op->ptr, "constraint_axis", constraint_axis);
			WM_operator_name_call(C, "TFM_OT_rotate", WM_OP_INVOKE_DEFAULT, op->ptr);
		}
	}
	/* after transform, restore drawflags */
	drawflags= 0xFFFF;

	return val;
}

