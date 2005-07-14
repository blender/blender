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


/* *************** Armature drawing ******************* */

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


static void draw_bone (int dt, int armflag, int boneflag, int constflag, unsigned int id, char *name, float length)
{
	
	/*	Draw a 3d octahedral bone, we use normalized space based on length,
	    for glDisplayLists */
	
	/* set up solid drawing */
	if(dt > OB_WIRE) {
		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_LIGHTING);
		BIF_ThemeColor(TH_BONE_SOLID);
	}
	
	/* change the matrix */
	glScalef(length, length, length);
	
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
	
	/*	Draw root point if we have no IK parent */
	if (!(boneflag & BONE_IK_TOPARENT)){
		if (id != -1)
			glLoadName (id | BONESEL_ROOT);
		if (armflag & ARM_EDITMODE) {
			if(dt<=OB_WIRE) {
				if (boneflag & BONE_ROOTSEL) BIF_ThemeColor(TH_VERTEX_SELECT);
				else BIF_ThemeColor(TH_VERTEX);
			}
			else 
				BIF_ThemeColor(TH_BONE_SOLID);
		}
		if(dt>OB_WIRE) draw_bonevert_solid();
		else draw_bonevert();
	}
	
	/*	Draw tip point */
	if (id != -1)
		glLoadName (id | BONESEL_TIP);
	if (armflag & ARM_EDITMODE) {
		if(dt<=OB_WIRE) {
			if (boneflag & BONE_TIPSEL) BIF_ThemeColor(TH_VERTEX_SELECT);
			else BIF_ThemeColor(TH_VERTEX);
		}
		else 
			BIF_ThemeColor(TH_BONE_SOLID);
	}
	
	glTranslatef(0.0, 1.0, 0.0);
	if(dt>OB_WIRE) draw_bonevert_solid();
	else draw_bonevert();
	
	/*	Draw additional axes */
	if (armflag & ARM_DRAWAXES){
		drawaxes(0.25f);
	}
	
	/* now draw the bone itself */
	glTranslatef(0.0, -1.0, 0.0);
	
	if (id != -1) {
		if (armflag & ARM_POSEMODE)
			glLoadName((GLuint) id);
		else{
			glLoadName ((GLuint) id|BONESEL_BONE);
		}
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
				
				if(constflag & PCHAN_HAS_IK) glColor4ub(255, 255, 0, 100);
				else if(constflag & PCHAN_HAS_CONST) glColor4ub(0, 255, 120, 100);
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

	/*	Draw the bone name */
	if (name && (armflag & ARM_DRAWNAMES)) {
		// patch for several 3d cards (IBM mostly) that crash on glSelect with text drawing
		if((G.f & G_PICKSEL) == 0) {
			if (armflag & (ARM_EDITMODE|ARM_POSEMODE)) {
				if(boneflag & BONE_SELECTED) BIF_ThemeColor(TH_TEXT_HI);
				else BIF_ThemeColor(TH_TEXT);
			}
			else if(dt > OB_WIRE) BIF_ThemeColor(TH_TEXT);
			glRasterPos3f(0,  0.5,  0);
			BMF_DrawString(G.font, " ");
			BMF_DrawString(G.font, name);
		}
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
	if(dt>OB_WIRE) {
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
				
				draw_bone(OB_SOLID, arm->flag, flag, 0, index, bone->name, bone->length);
				
				glPopMatrix();
			}
			if (index!= -1) index++;
		}
		glLoadName (-1);
		index= -1;
	}
	
	/* wire draw over solid only in posemode */
	if(dt<=OB_WIRE || (arm->flag & ARM_POSEMODE)) {
	
		/* if solid && posemode, we draw again with polygonoffset */
		if (dt>OB_WIRE && (arm->flag & ARM_POSEMODE))
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

				draw_bone(OB_WIRE, arm->flag, flag, constflag, index, bone->name, bone->length);
				
				glPopMatrix();
			}
			if (index!= -1) index++;
		}
	}	
	/* restore things */
	if (dt>OB_WIRE && (arm->flag & ARM_POSEMODE))
		bglPolygonOffset(0.0);
	glDisable(GL_CULL_FACE);

}

/* in editmode, we don't store the bone matrix... */
static void set_matrix_editbone(EditBone *eBone)
{
	float		delta[3],offset[3];
	float		mat[3][3], bmat[4][4];
	
	/*	Compose the parent transforms (i.e. their translations) */
	VECCOPY (offset, eBone->head);
	
	glTranslatef (offset[0],offset[1],offset[2]);
	
	delta[0]= eBone->tail[0]-eBone->head[0];	
	delta[1]= eBone->tail[1]-eBone->head[1];	
	delta[2]= eBone->tail[2]-eBone->head[2];
	
	eBone->length = sqrt (delta[0]*delta[0] + delta[1]*delta[1] +delta[2]*delta[2]);
	
	vec_roll_to_mat3(delta, eBone->roll, mat);
	Mat4CpyMat3(bmat, mat);
				
	glMultMatrixf (bmat);
}

/* called from drawobject.c */
void draw_armature(Object *ob, int dt)
{
	bArmature *arm= ob->data;
	
	/* we use color for solid lighting */
	glColorMaterial(GL_FRONT_AND_BACK, GL_DIFFUSE);
	glFrontFace((ob->transflag&OB_NEG_SCALE)?GL_CW:GL_CCW);	// only for lighting...
	
	/* If we're in editmode, draw the Global edit data */
	if(ob==G.obedit || (G.obedit && ob->data==G.obedit->data)) {
		EditBone	*eBone;
		unsigned int	index;
		
		if(ob==G.obedit) arm->flag |= ARM_EDITMODE;
		
		/* if solid we draw it first */
		if(dt>OB_WIRE && (arm->flag & ARM_EDITMODE)) {
			for (eBone=G.edbo.first; eBone; eBone=eBone->next){
				glPushMatrix();
				set_matrix_editbone(eBone);
				draw_bone (OB_SOLID, arm->flag, eBone->flag, 0, -1, eBone->name, eBone->length);
				glPopMatrix();
			}
		}
		
		/* if wire over solid, set offset */
		if (dt>OB_WIRE) bglPolygonOffset(1.0);
		
		for (eBone=G.edbo.first, index=0; eBone; eBone=eBone->next, index++){
			
			glPushMatrix();
			set_matrix_editbone(eBone);
			draw_bone (OB_WIRE, arm->flag, eBone->flag, 0, index, eBone->name, eBone->length);
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
		}
		
		/* restore */
		if (dt>OB_WIRE) bglPolygonOffset(0.0);
		
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

