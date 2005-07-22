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
 * The Original Code is Copyright (C) 2005 by the Blender Foundation.
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

#include "BMF_Api.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_ID.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_view3d_types.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_editarmature.h"
#include "BIF_gl.h"
#include "BIF_glutil.h"
#include "BIF_graphics.h"
#include "BIF_interface.h"
#include "BIF_poseobject.h"
#include "BIF_mywindow.h"
#include "BIF_resources.h"
#include "BIF_screen.h"

#include "BDR_editobject.h"
#include "BDR_drawobject.h"

#include "BSE_edit.h"
#include "BSE_view.h"
#include "BSE_editaction.h"

#include "mydevice.h"
#include "blendef.h"
#include "nla.h"

/* half the cube, in Y */
static float cube[8][3] = {
{-1.0,  0.0, -1.0},
{-1.0,  0.0,  1.0},
{-1.0,  1.0,  1.0},
{-1.0,  1.0, -1.0},
{ 1.0,  0.0, -1.0},
{ 1.0,  0.0,  1.0},
{ 1.0,  1.0,  1.0},
{ 1.0,  1.0, -1.0},
};


/* *************** Armature drawing, helper calls for parts ******************* */

static void drawsolidcube_size(float xsize, float ysize, float zsize)
{
	float n[3];
	
	glScalef(xsize, ysize, zsize);
	
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
	
}

static void drawcube_size(float xsize, float ysize, float zsize)
{
	
	glScalef(xsize, ysize, zsize);
	
	glBegin(GL_LINE_STRIP);
	glVertex3fv(cube[0]); glVertex3fv(cube[1]);glVertex3fv(cube[2]); glVertex3fv(cube[3]);
	glVertex3fv(cube[0]); glVertex3fv(cube[4]);glVertex3fv(cube[5]); glVertex3fv(cube[6]);
	glVertex3fv(cube[7]); glVertex3fv(cube[4]);
	glEnd();
	
	glBegin(GL_LINE_STRIP);
	glVertex3fv(cube[1]); glVertex3fv(cube[5]);
	glEnd();
	
	glBegin(GL_LINE_STRIP);
	glVertex3fv(cube[2]); glVertex3fv(cube[6]);
	glEnd();
	
	glBegin(GL_LINE_STRIP);
	glVertex3fv(cube[3]); glVertex3fv(cube[7]);
	glEnd();
	
}


static void draw_bonevert(void)
{
	static GLuint displist=0;
	
	if(displist==0) {
		GLUquadricObj	*qobj;
		
		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE_AND_EXECUTE);
				
		glPushMatrix();
		
		qobj	= gluNewQuadric(); 
		gluQuadricDrawStyle(qobj, GLU_SILHOUETTE); 
		gluDisk( qobj, 0.0,  0.05, 16, 1);
		
		glRotatef (90, 0, 1, 0);
		gluDisk( qobj, 0.0,  0.05, 16, 1);
		
		glRotatef (90, 1, 0, 0);
		gluDisk( qobj, 0.0,  0.05, 16, 1);
		
		gluDeleteQuadric(qobj);  
		
		glPopMatrix();
		glEndList();
	}
	else glCallList(displist);
}

static void draw_bonevert_solid(void)
{
	static GLuint displist=0;
	
	if(displist==0) {
		GLUquadricObj	*qobj;
		
		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE_AND_EXECUTE);
		
		qobj	= gluNewQuadric();
		gluQuadricDrawStyle(qobj, GLU_FILL); 
		glShadeModel(GL_SMOOTH);
		gluSphere( qobj, 0.05, 8, 5);
		glShadeModel(GL_FLAT);
		gluDeleteQuadric(qobj);  
		
		glEndList();
	}
	else glCallList(displist);
}

static void draw_bone_octahedral()
{
	static GLuint displist=0;
	
	if(displist==0) {
		float vec[6][3];	
		
		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE_AND_EXECUTE);

		vec[0][0]= vec[0][1]= vec[0][2]= 0.0;
		vec[5][0]= vec[5][2]= 0.0; vec[5][1]= 1.0;
		
		vec[1][0]= 0.1; vec[1][2]= 0.1; vec[1][1]= 0.1;
		vec[2][0]= 0.1; vec[2][2]= -0.1; vec[2][1]= 0.1;
		vec[3][0]= -0.1; vec[3][2]= -0.1; vec[3][1]= 0.1;
		vec[4][0]= -0.1; vec[4][2]= 0.1; vec[4][1]= 0.1;
		
		/*	Section 1, sides */
		glBegin(GL_LINE_LOOP);
		glVertex3fv(vec[0]);
		glVertex3fv(vec[1]);
		glVertex3fv(vec[5]);
		glVertex3fv(vec[3]);
		glVertex3fv(vec[0]);
		glVertex3fv(vec[4]);
		glVertex3fv(vec[5]);
		glVertex3fv(vec[2]);
		glEnd();
		
		/*	Section 1, square */
		glBegin(GL_LINE_LOOP);
		glVertex3fv(vec[1]);
		glVertex3fv(vec[2]);
		glVertex3fv(vec[3]);
		glVertex3fv(vec[4]);
		glEnd();
		
		glEndList();
	}
	else glCallList(displist);
}	

static void draw_bone_solid_octahedral(void)
{
	static GLuint displist=0;
	
	if(displist==0) {
		float vec[6][3], nor[3];	
		
		displist= glGenLists(1);
		glNewList(displist, GL_COMPILE_AND_EXECUTE);
		
		vec[0][0]= vec[0][1]= vec[0][2]= 0.0;
		vec[5][0]= vec[5][2]= 0.0; vec[5][1]= 1.0;
		
		vec[1][0]= 0.1; vec[1][2]= 0.1; vec[1][1]= 0.1;
		vec[2][0]= 0.1; vec[2][2]= -0.1; vec[2][1]= 0.1;
		vec[3][0]= -0.1; vec[3][2]= -0.1; vec[3][1]= 0.1;
		vec[4][0]= -0.1; vec[4][2]= 0.1; vec[4][1]= 0.1;
		
		
		glBegin(GL_TRIANGLES);
		/* bottom */
		CalcNormFloat(vec[2], vec[1], vec[0], nor);
		glNormal3fv(nor);
		glVertex3fv(vec[2]);glVertex3fv(vec[1]);glVertex3fv(vec[0]);
		
		CalcNormFloat(vec[3], vec[2], vec[0], nor);
		glNormal3fv(nor);
		glVertex3fv(vec[3]);glVertex3fv(vec[2]);glVertex3fv(vec[0]);
		
		CalcNormFloat(vec[4], vec[3], vec[0], nor);
		glNormal3fv(nor);
		glVertex3fv(vec[4]);glVertex3fv(vec[3]);glVertex3fv(vec[0]);

		CalcNormFloat(vec[1], vec[4], vec[0], nor);
		glNormal3fv(nor);
		glVertex3fv(vec[1]);glVertex3fv(vec[4]);glVertex3fv(vec[0]);

		/* top */
		CalcNormFloat(vec[5], vec[1], vec[2], nor);
		glNormal3fv(nor);
		glVertex3fv(vec[5]);glVertex3fv(vec[1]);glVertex3fv(vec[2]);
		
		CalcNormFloat(vec[5], vec[2], vec[3], nor);
		glNormal3fv(nor);
		glVertex3fv(vec[5]);glVertex3fv(vec[2]);glVertex3fv(vec[3]);
		
		CalcNormFloat(vec[5], vec[3], vec[4], nor);
		glNormal3fv(nor);
		glVertex3fv(vec[5]);glVertex3fv(vec[3]);glVertex3fv(vec[4]);
		
		CalcNormFloat(vec[5], vec[4], vec[1], nor);
		glNormal3fv(nor);
		glVertex3fv(vec[5]);glVertex3fv(vec[4]);glVertex3fv(vec[1]);
		
		glEnd();
		
		glEndList();
	}
	else glCallList(displist);
}	

/* *************** Armature drawing, bones ******************* */


static void draw_bone_points(int dt, int armflag, unsigned int boneflag, int id)
{
	/*	Draw root point if we have no IK parent */
	if (!(boneflag & BONE_IK_TOPARENT)){
		if (id != -1)
			glLoadName (id | BONESEL_ROOT);
		
		if(dt<=OB_WIRE) {
			if(armflag & ARM_EDITMODE) {
				if (boneflag & BONE_ROOTSEL) BIF_ThemeColor(TH_VERTEX_SELECT);
				else BIF_ThemeColor(TH_VERTEX);
			}
		}
		else 
			BIF_ThemeColor(TH_BONE_SOLID);
		
		if(dt>OB_WIRE) draw_bonevert_solid();
		else draw_bonevert();
	}
	
	/*	Draw tip point */
	if (id != -1)
		glLoadName (id | BONESEL_TIP);
	
	if(dt<=OB_WIRE) {
		if(armflag & ARM_EDITMODE) {
			if (boneflag & BONE_TIPSEL) BIF_ThemeColor(TH_VERTEX_SELECT);
			else BIF_ThemeColor(TH_VERTEX);
		}
	}
	else {
		BIF_ThemeColor(TH_BONE_SOLID);
	}
	
	glTranslatef(0.0, 1.0, 0.0);
	if(dt>OB_WIRE) draw_bonevert_solid();
	else draw_bonevert();
	glTranslatef(0.0, -1.0, 0.0);
	
}

static char bm_dot6[]= {0x0, 0x18, 0x3C, 0x7E, 0x7E, 0x3C, 0x18, 0x0}; 
static char bm_dot8[]= {0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C}; 

static void draw_line_bone(int armflag, int boneflag, int constflag, unsigned int id, bPoseChannel *pchan, EditBone *ebone)
{
	float length;
	
	glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
	
	if(pchan) 
		length= pchan->bone->length;
	else 
		length= ebone->length;
	
	glPushMatrix();
	glScalef(length, length, length);
	
	/* this chunk not in object mode */
	if(armflag & (ARM_EDITMODE|ARM_POSEMODE)) {
		glLineWidth(4.0);
		if(armflag & ARM_POSEMODE) {
			/* outline in black or selection color */
			if (boneflag & BONE_ACTIVE) BIF_ThemeColorShade(TH_BONE_POSE, 40);
			else if (boneflag & BONE_SELECTED) BIF_ThemeColor(TH_BONE_POSE);
			else BIF_ThemeColor(TH_WIRE);
		}
		else if (armflag & ARM_EDITMODE) {
			BIF_ThemeColor(TH_WIRE);
		}
		
		/*	Draw root point if we have no IK parent */
		if (!(boneflag & BONE_IK_TOPARENT)){
			if (id != -1) {	// no bitmap in selection mode, crashes 3d cards...
				glLoadName (id | BONESEL_ROOT);
				glBegin(GL_POINTS);
				glVertex3f(0.0f, 0.0f, 0.0f);
				glEnd();
			}
			else {
				glRasterPos3f(0.0f, 0.0f, 0.0f);
				glBitmap(8, 8, 4, 4, 0, 0, bm_dot8);
			}
		}
		
		if (id != -1)
			glLoadName ((GLuint) id|BONESEL_BONE);
		
		glBegin(GL_LINES);
		glVertex3f(0.0f, 0.0f, 0.0f);
		glVertex3f(0.0f, 1.0f, 0.0f);
		glEnd();
		
		/* tip */
		if (id != -1) {	// no bitmap in selection mode, crashes 3d cards...
			glLoadName (id | BONESEL_TIP);
			glBegin(GL_POINTS);
			glVertex3f(0.0f, 1.0f, 0.0f);
			glEnd();
		}
		else {
			glRasterPos3f(0.0f, 1.0f, 0.0f);
			glBitmap(8, 8, 4, 4, 0, 0, bm_dot8);
		}
		
		/* further we send no names */
		if (id != -1)
			glLoadName (-1);
		
		if(armflag & ARM_POSEMODE) {
			/* inner part in background color or constraint */
			if(constflag) {
				if(constflag & PCHAN_HAS_IK) glColor3ub(255, 255, 0);
				else if(constflag & PCHAN_HAS_CONST) glColor3ub(0, 255, 120);
				else BIF_ThemeColor(TH_BONE_POSE);	// PCHAN_HAS_ACTION 
			}
			else BIF_ThemeColor(TH_BACK);
		}
	}
	
	glLineWidth(2.0);
	
	/*	Draw root point if we have no IK parent */
	if (!(boneflag & BONE_IK_TOPARENT)){
		if (id == -1) {	// no bitmap in selection mode, crashes 3d cards...
			if(armflag & ARM_EDITMODE) {
				if (boneflag & BONE_ROOTSEL) BIF_ThemeColor(TH_VERTEX_SELECT);
				else BIF_ThemeColor(TH_VERTEX);
			}
			glRasterPos3f(0.0f, 0.0f, 0.0f);
			glBitmap(8, 8, 4, 4, 0, 0, bm_dot6);
		}
	}
	
   if(armflag & ARM_EDITMODE) {
	   if (boneflag & BONE_SELECTED) BIF_ThemeColor(TH_EDGE_SELECT);
	   else BIF_ThemeColor(TH_BACK);
   }
	glBegin(GL_LINES);
	glVertex3f(0.0f, 0.0f, 0.0f);
	glVertex3f(0.0f, 1.0f, 0.0f);
	glEnd();
	
	/* tip */
	if (id == -1) {	// no bitmap in selection mode, crashes 3d cards...
		if(armflag & ARM_EDITMODE) {
			if (boneflag & BONE_TIPSEL) BIF_ThemeColor(TH_VERTEX_SELECT);
			else BIF_ThemeColor(TH_VERTEX);
		}
		glRasterPos3f(0.0f, 1.0f, 0.0f);
		glBitmap(8, 8, 4, 4, 0, 0, bm_dot6);
	}
	
	glLineWidth(1.0);
	
	glPopMatrix();
}

static void draw_b_bone_boxes(int dt, bPoseChannel *pchan, float xwidth, float length, float zwidth)
{
	int segments= 0;
	
	if(pchan) segments= pchan->bone->segments;
	
	if(segments>1 && pchan) {
		float dlen= length/(float)segments;
		Mat4 *bbone= b_bone_spline_setup(pchan);
		int a;
		
		for(a=0; a<segments; a++, bbone++) {
			glPushMatrix();
			glMultMatrixf(bbone->mat);
			if(dt==OB_SOLID) drawsolidcube_size(xwidth, dlen, zwidth);
			else drawcube_size(xwidth, dlen, zwidth);
			glPopMatrix();
		}
	}
	else {
		glPushMatrix();
		if(dt==OB_SOLID) drawsolidcube_size(xwidth, length, zwidth);
		else drawcube_size(xwidth, length, zwidth);
		glPopMatrix();
	}
}

static void draw_b_bone(int dt, int armflag, int boneflag, int constflag, unsigned int id, bPoseChannel *pchan, EditBone *ebone)
{
	float xwidth, length, zwidth;
	
	if(pchan) {
		xwidth= pchan->bone->xwidth;
		length= pchan->bone->length;
		zwidth= pchan->bone->zwidth;
	}
	else {
		xwidth= ebone->xwidth;
		length= ebone->length;
		zwidth= ebone->zwidth;
	}
	
	/* draw points only if... */
	if(armflag & ARM_EDITMODE) {
		/* move to unitspace */
		glPushMatrix();
		glScalef(length, length, length);
		draw_bone_points(dt, armflag, boneflag, id);
		glPopMatrix();
		length*= 0.95f;	// make vertices visible
	}

	/* colors for modes */
	if (armflag & ARM_POSEMODE) {
		if(dt==OB_WIRE) {
			if (boneflag & BONE_ACTIVE) BIF_ThemeColorShade(TH_BONE_POSE, 40);
			else if (boneflag & BONE_SELECTED) BIF_ThemeColor(TH_BONE_POSE);
			else BIF_ThemeColor(TH_WIRE);
		}
		else 
			BIF_ThemeColor(TH_BONE_SOLID);
	}
	else if (armflag & ARM_EDITMODE) {
		if(dt==OB_WIRE) {
			if (boneflag & BONE_ACTIVE) BIF_ThemeColor(TH_EDGE_SELECT);
			else if (boneflag & BONE_SELECTED) BIF_ThemeColorShade(TH_EDGE_SELECT, -20);
			else BIF_ThemeColor(TH_WIRE);
		}
		else 
			BIF_ThemeColor(TH_BONE_SOLID);
	}
	
	if (id != -1) {
		glLoadName ((GLuint) id|BONESEL_BONE);
	}
	
	/* set up solid drawing */
	if(dt > OB_WIRE) {
		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_LIGHTING);
		BIF_ThemeColor(TH_BONE_SOLID);
		
		draw_b_bone_boxes(OB_SOLID, pchan, xwidth, length, zwidth);
		
		/* disable solid drawing */
		glDisable(GL_COLOR_MATERIAL);
		glDisable(GL_LIGHTING);
	}
	else {	// wire
		if (armflag & ARM_POSEMODE){
			if(constflag) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glEnable(GL_BLEND);
				
				if(constflag & PCHAN_HAS_IK) glColor4ub(255, 255, 0, 80);
				else if(constflag & PCHAN_HAS_CONST) glColor4ub(0, 255, 120, 80);
				else BIF_ThemeColor4(TH_BONE_POSE);	// PCHAN_HAS_ACTION 
				
				draw_b_bone_boxes(OB_SOLID, pchan, xwidth, length, zwidth);
				
				glDisable(GL_BLEND);
				
				/* restore colors */
				if (boneflag & BONE_ACTIVE) BIF_ThemeColorShade(TH_BONE_POSE, 40);
				else if (boneflag & BONE_SELECTED) BIF_ThemeColor(TH_BONE_POSE);
				else BIF_ThemeColor(TH_WIRE);
			}
		}		
		
		draw_b_bone_boxes(OB_WIRE, pchan, xwidth, length, zwidth);		
	}
}

static void draw_bone(int dt, int armflag, int boneflag, int constflag, unsigned int id)
{
	
	/*	Draw a 3d octahedral bone, we use normalized space based on length,
	    for glDisplayLists */
	
	/* set up solid drawing */
	if(dt > OB_WIRE) {
		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_LIGHTING);
		BIF_ThemeColor(TH_BONE_SOLID);
	}
	
	/* colors for posemode */
	if (armflag & ARM_POSEMODE) {
		if(dt==OB_WIRE) {
			if (boneflag & BONE_ACTIVE) BIF_ThemeColorShade(TH_BONE_POSE, 40);
			else if (boneflag & BONE_SELECTED) BIF_ThemeColor(TH_BONE_POSE);
			else BIF_ThemeColor(TH_WIRE);
		}
		else 
			BIF_ThemeColor(TH_BONE_SOLID);
	}
	
	
	draw_bone_points(dt, armflag, boneflag, id);
	
	/* now draw the bone itself */
	
	if (id != -1) {
		glLoadName ((GLuint) id|BONESEL_BONE);
	}
	
	/* wire? */
	if(dt <= OB_WIRE) {
		/* colors */
		if (armflag & ARM_EDITMODE) {
			if (boneflag & BONE_ACTIVE) BIF_ThemeColor(TH_EDGE_SELECT);
			else if (boneflag & BONE_SELECTED) BIF_ThemeColorShade(TH_EDGE_SELECT, -20);
			else BIF_ThemeColor(TH_WIRE);
		}
		else if (armflag & ARM_POSEMODE){
			if(constflag) {
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glEnable(GL_BLEND);
				
				if(constflag & PCHAN_HAS_IK) glColor4ub(255, 255, 0, 80);
				else if(constflag & PCHAN_HAS_CONST) glColor4ub(0, 255, 120, 80);
				else BIF_ThemeColor4(TH_BONE_POSE);	// PCHAN_HAS_ACTION 
					
				draw_bone_solid_octahedral();
				glDisable(GL_BLEND);

				/* restore colors */
				if (boneflag & BONE_ACTIVE) BIF_ThemeColorShade(TH_BONE_POSE, 40);
				else if (boneflag & BONE_SELECTED) BIF_ThemeColor(TH_BONE_POSE);
				else BIF_ThemeColor(TH_WIRE);
			}
		}		
		draw_bone_octahedral();
	}
	else {	/* solid */

		BIF_ThemeColor(TH_BONE_SOLID);
		draw_bone_solid_octahedral();
	}

	/* disable solid drawing */
	if(dt>OB_WIRE) {
		glDisable(GL_COLOR_MATERIAL);
		glDisable(GL_LIGHTING);
	}
}

/* assumes object is Armature with pose */
static void draw_pose_channels(Object *ob, int dt)
{
	bPoseChannel *pchan;
	Bone *bone;
	bArmature *arm= ob->data;
	GLfloat tmp;
	int index= -1;
	int do_dashed= 1;
	short flag, constflag;
	
	/* little speedup, also make sure transparent only draws once */
	glCullFace(GL_BACK); 
	glEnable(GL_CULL_FACE);
	
	/* hacky... prevent outline select from drawing dashed helplines */
	glGetFloatv(GL_LINE_WIDTH, &tmp);
	if(tmp > 1.1) do_dashed= 0;
		
	/* if solid we draw that first, with selection codes, but without names, axes etc */
	if(dt>OB_WIRE && arm->drawtype!=ARM_LINE) {
		if(arm->flag & ARM_POSEMODE) index= 0;
		
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			bone= pchan->bone;
			if(bone && !(bone->flag & BONE_HIDDEN)) {
				glPushMatrix();
				glMultMatrixf(pchan->pose_mat);
				
				/* catch exception for bone with hidden parent */
				flag= bone->flag;
				if(bone->parent && (bone->parent->flag & BONE_HIDDEN))
					flag &= ~BONE_IK_TOPARENT;
				
				if(arm->drawtype==ARM_B_BONE)
					draw_b_bone(OB_SOLID, arm->flag, flag, 0, index, pchan, NULL);
				else {
					/* scale the matrix to unit bone space */
					glScalef(bone->length, bone->length, bone->length);
					
					draw_bone(OB_SOLID, arm->flag, flag, 0, index);
				}
				glPopMatrix();
			}
			if (index!= -1) index++;
		}
		glLoadName (-1);
		index= -1;
	}
	
	/* wire draw over solid only in posemode */
	if( dt<=OB_WIRE || (arm->flag & ARM_POSEMODE) || arm->drawtype==ARM_LINE) {
	
		/* draw line check first. we do selection indices */
		if (arm->drawtype==ARM_LINE) {
			if(G.f & G_PICKSEL) index= 0;
		}
		/* if solid && posemode, we draw again with polygonoffset */
		else if (dt>OB_WIRE && (arm->flag & ARM_POSEMODE))
			bglPolygonOffset(1.0);
		else
			/* and we use selection indices if not done yet */
			if (arm->flag & ARM_POSEMODE) index= 0;
		
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			bone= pchan->bone;
			if(bone && !(bone->flag & BONE_HIDDEN)) {
					
				//	Draw a line from our root to the parent's tip
				if (do_dashed && bone->parent && !(bone->flag & BONE_IK_TOPARENT) ){
					if (arm->flag & ARM_POSEMODE) {
						glLoadName (-1);
						BIF_ThemeColor(TH_WIRE);
					}
					setlinestyle(3);
					glBegin(GL_LINES);
					glVertex3fv(pchan->pose_head);
					glVertex3fv(pchan->parent->pose_tail);
					glEnd();
					setlinestyle(0);
				}
				
				glPushMatrix();
				glMultMatrixf(pchan->pose_mat);
				
				/* catch exception for bone with hidden parent */
				flag= bone->flag;
				if(bone->parent && (bone->parent->flag & BONE_HIDDEN))
					flag &= ~BONE_IK_TOPARENT;
				
				/* extra draw service for pose mode */
				constflag= pchan->constflag;
				if(pchan->flag & (POSE_ROT|POSE_LOC|POSE_SIZE))
					constflag |= PCHAN_HAS_ACTION;

				if(arm->drawtype==ARM_LINE)
					draw_line_bone(arm->flag, flag, constflag, index, pchan, NULL);
				else if(arm->drawtype==ARM_B_BONE)
					draw_b_bone(OB_WIRE, arm->flag, flag, constflag, index, pchan, NULL);
				else {
					/* scale the matrix to unit bone space */
					glScalef(bone->length, bone->length, bone->length);
					
					draw_bone(OB_WIRE, arm->flag, flag, constflag, index);
				}
				
				glPopMatrix();
			}
			if (index!= -1) index++;
		}
		/* restore things */
		if (arm->drawtype!=ARM_LINE && dt>OB_WIRE && (arm->flag & ARM_POSEMODE))
			bglPolygonOffset(0.0);
		
	}	
	
	/* restore */
	glDisable(GL_CULL_FACE);

	/* finally names and axes */
	if(arm->flag & (ARM_DRAWNAMES|ARM_DRAWAXES)) {
		// patch for several 3d cards (IBM mostly) that crash on glSelect with text drawing
		if((G.f & G_PICKSEL) == 0) {
			float vec[3];
			
			if(G.vd->zbuf) glDisable(GL_DEPTH_TEST);
			
			for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			
				if (arm->flag & (ARM_EDITMODE|ARM_POSEMODE)) {
					bone= pchan->bone;
					if(bone->flag & BONE_SELECTED) BIF_ThemeColor(TH_TEXT_HI);
					else BIF_ThemeColor(TH_TEXT);
				}
				else if(dt > OB_WIRE) BIF_ThemeColor(TH_TEXT);
				
				if (arm->flag & ARM_DRAWNAMES){
					VecMidf(vec, pchan->pose_head, pchan->pose_tail);
					glRasterPos3fv(vec);
					BMF_DrawString(G.font, " ");
					BMF_DrawString(G.font, pchan->name);
				}				
				/*	Draw additional axes */
				if( (arm->flag & ARM_DRAWAXES) && (arm->flag & ARM_POSEMODE) ){
					glPushMatrix();
					glMultMatrixf(pchan->pose_mat);
					glTranslatef(0.0f, pchan->bone->length, 0.0f);
					drawaxes(0.25f);
					glPopMatrix();
				}
			}
			
			if(G.vd->zbuf) glEnable(GL_DEPTH_TEST);
		}
	}
}

/* in editmode, we don't store the bone matrix... */
static void set_matrix_editbone(EditBone *eBone)
{
	float		delta[3],offset[3];
	float		mat[3][3], bmat[4][4];
	
	/*	Compose the parent transforms (i.e. their translations) */
	VECCOPY (offset, eBone->head);
	
	glTranslatef (offset[0],offset[1],offset[2]);
	
	VecSubf(delta, eBone->tail, eBone->head);	
	
	eBone->length = sqrt (delta[0]*delta[0] + delta[1]*delta[1] +delta[2]*delta[2]);
	
	vec_roll_to_mat3(delta, eBone->roll, mat);
	Mat4CpyMat3(bmat, mat);
				
	glMultMatrixf (bmat);
	
}

static void draw_ebones(Object *ob, int dt)
{
	EditBone	*eBone;
	bArmature *arm= ob->data;
	unsigned int	index;
	
	/* if solid we draw it first */
	if(dt>OB_WIRE && arm->drawtype!=ARM_LINE) {
		index= 0;
		for (eBone=G.edbo.first, index=0; eBone; eBone=eBone->next, index++){
			glPushMatrix();
			set_matrix_editbone(eBone);
			
			if(arm->drawtype==ARM_B_BONE)
				draw_b_bone(OB_SOLID, arm->flag, eBone->flag, 0, index, NULL, eBone);
			else {
				/* scale the matrix to unit bone space */
				glScalef(eBone->length, eBone->length, eBone->length);
				draw_bone(OB_SOLID, arm->flag, eBone->flag, 0, index);
			}
			
			glPopMatrix();
		}
	}
	
	/* if wire over solid, set offset */
	index= -1;
	if(arm->drawtype==ARM_LINE) {
		if(G.f & G_PICKSEL)
			index= 0;
	}
	else if (dt>OB_WIRE) 
		bglPolygonOffset(1.0);
	else if(arm->flag & ARM_EDITMODE) 
		index= 0;	// do selection codes
	
	for (eBone=G.edbo.first; eBone; eBone=eBone->next){
		
		glPushMatrix();
		set_matrix_editbone(eBone);
		
		if(arm->drawtype==ARM_LINE) 
			draw_line_bone(arm->flag, eBone->flag, 0, index, NULL, eBone);
		else if(arm->drawtype==ARM_B_BONE)
			draw_b_bone(OB_WIRE, arm->flag, eBone->flag, 0, index, NULL, eBone);
		else {
			/* scale the matrix to unit bone space */
			glScalef(eBone->length, eBone->length, eBone->length);
			draw_bone(OB_WIRE, arm->flag, eBone->flag, 0, index);
		}
		if(arm->flag & ARM_DRAWAXES)
			drawaxes(0.25f);
		
		glPopMatrix();
		
		/* offset to parent */
		if (eBone->parent) {
			BIF_ThemeColor(TH_WIRE);
			glLoadName (-1);
			setlinestyle(3);
			
			glBegin(GL_LINES);
			glVertex3fv(eBone->parent->tail);
			glVertex3fv(eBone->head);
			glEnd();
			
			setlinestyle(0);
		}
		if(index!=-1) index++;
	}
	
	/* restore */
	if (dt>OB_WIRE) bglPolygonOffset(0.0);
	
	/* finally names */
	if(arm->flag & ARM_DRAWNAMES) {
		// patch for several 3d cards (IBM mostly) that crash on glSelect with text drawing
		if((G.f & G_PICKSEL) == 0) {
			float vec[3];
			
			if(G.vd->zbuf) glDisable(GL_DEPTH_TEST);
			
			for (eBone=G.edbo.first, index=0; eBone; eBone=eBone->next, index++){
				
				if(eBone->flag & BONE_SELECTED) BIF_ThemeColor(TH_TEXT_HI);
				else BIF_ThemeColor(TH_TEXT);
				
				VecMidf(vec, eBone->head, eBone->tail);
				glRasterPos3fv(vec);
				BMF_DrawString(G.font, " ");
				BMF_DrawString(G.font, eBone->name);
			}
			
			if(G.vd->zbuf) glEnable(GL_DEPTH_TEST);
		}
	}
	
}

/* called from drawobject.c */
void draw_armature(Object *ob, int dt)
{
	bArmature *arm= ob->data;
	
	/* we use color for solid lighting */
	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
	glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);	// only for lighting...
	
	/* editmode? */
	if(ob==G.obedit || (G.obedit && ob->data==G.obedit->data)) {
		if(ob==G.obedit) arm->flag |= ARM_EDITMODE;
		draw_ebones(ob, dt);
		arm->flag &= ~ARM_EDITMODE;
	}
	else{
		/*	Draw Pose */
		if(ob->pose) {
			if (G.obpose == ob) arm->flag |= ARM_POSEMODE;
			draw_pose_channels(ob, dt);
			arm->flag &= ~ARM_POSEMODE; 
		}
	}
	/* restore */
	glFrontFace(GL_CCW);

}

/* *************** END Armature drawing ******************* */

