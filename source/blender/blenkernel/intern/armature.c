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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Full recode, Ton Roosendaal, Crete 2005
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include <float.h>

#include "MEM_guardedalloc.h"

//XXX #include "nla.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "DNA_armature_types.h"
#include "DNA_action_types.h"
#include "DNA_constraint_types.h"
#include "DNA_mesh_types.h"
#include "DNA_lattice_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"

#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_blender.h"
#include "BKE_constraint.h"
#include "BKE_curve.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_DerivedMesh.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_lattice.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BIK_api.h"
#include "BKE_sketch.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*	**************** Generic Functions, data level *************** */

bArmature *add_armature(char *name)
{
	bArmature *arm;
	
	arm= alloc_libblock (&G.main->armature, ID_AR, name);
	arm->deformflag = ARM_DEF_VGROUP|ARM_DEF_ENVELOPE;
	arm->flag = ARM_COL_CUSTOM; /* custom bone-group colors */
	arm->layer= 1;
	return arm;
}

bArmature *get_armature(Object *ob)
{
	if(ob->type==OB_ARMATURE)
		return (bArmature *)ob->data;
	return NULL;
}

void free_boneChildren(Bone *bone)
{ 
	Bone *child;
	
	if (bone) {
		
		child=bone->childbase.first;
		if (child){
			while (child){
				free_boneChildren (child);
				child=child->next;
			}
			BLI_freelistN (&bone->childbase);
		}
	}
}

void free_bones (bArmature *arm)
{
	Bone *bone;
	/*	Free children (if any)	*/
	bone= arm->bonebase.first;
	if (bone) {
		while (bone){
			free_boneChildren (bone);
			bone=bone->next;
		}
	}
	
	
	BLI_freelistN(&arm->bonebase);
}

void free_armature(bArmature *arm)
{
	if (arm) {
		free_bones(arm);
		
		/* free editmode data */
		if (arm->edbo) {
			BLI_freelistN(arm->edbo);
			
			MEM_freeN(arm->edbo);
			arm->edbo= NULL;
		}

		/* free sketch */
		if (arm->sketch) {
			freeSketch(arm->sketch);
			arm->sketch = NULL;
		}
	}
}

void make_local_armature(bArmature *arm)
{
	int local=0, lib=0;
	Object *ob;
	bArmature *newArm;
	
	if (arm->id.lib==0)
		return;
	if (arm->id.us==1) {
		arm->id.lib= 0;
		arm->id.flag= LIB_LOCAL;
		new_id(0, (ID*)arm, 0);
		return;
	}
	
	if(local && lib==0) {
		arm->id.lib= 0;
		arm->id.flag= LIB_LOCAL;
		new_id(0, (ID *)arm, 0);
	}
	else if(local && lib) {
		newArm= copy_armature(arm);
		newArm->id.us= 0;
		
		ob= G.main->object.first;
		while(ob) {
			if(ob->data==arm) {
				
				if(ob->id.lib==0) {
					ob->data= newArm;
					newArm->id.us++;
					arm->id.us--;
				}
			}
			ob= ob->id.next;
		}
	}
}

static void	copy_bonechildren (Bone* newBone, Bone* oldBone)
{
	Bone	*curBone, *newChildBone;
	
	/*	Copy this bone's list*/
	BLI_duplicatelist(&newBone->childbase, &oldBone->childbase);
	
	/*	For each child in the list, update it's children*/
	newChildBone=newBone->childbase.first;
	for (curBone=oldBone->childbase.first;curBone;curBone=curBone->next){
		newChildBone->parent=newBone;
		copy_bonechildren(newChildBone,curBone);
		newChildBone=newChildBone->next;
	}
}

bArmature *copy_armature(bArmature *arm)
{
	bArmature *newArm;
	Bone		*oldBone, *newBone;
	
	newArm= copy_libblock (arm);
	BLI_duplicatelist(&newArm->bonebase, &arm->bonebase);
	
	/*	Duplicate the childrens' lists*/
	newBone=newArm->bonebase.first;
	for (oldBone=arm->bonebase.first;oldBone;oldBone=oldBone->next){
		newBone->parent=NULL;
		copy_bonechildren (newBone, oldBone);
		newBone=newBone->next;
	};
	
	return newArm;
}

static Bone *get_named_bone_bonechildren (Bone *bone, const char *name)
{
	Bone *curBone, *rbone;
	
	if (!strcmp (bone->name, name))
		return bone;
	
	for (curBone=bone->childbase.first; curBone; curBone=curBone->next){
		rbone=get_named_bone_bonechildren (curBone, name);
		if (rbone)
			return rbone;
	}
	
	return NULL;
}


Bone *get_named_bone (bArmature *arm, const char *name)
/*
	Walk the list until the bone is found
 */
{
	Bone *bone=NULL, *curBone;
	
	if (!arm) return NULL;
	
	for (curBone=arm->bonebase.first; curBone; curBone=curBone->next){
		bone = get_named_bone_bonechildren (curBone, name);
		if (bone)
			return bone;
	}
	
	return bone;
}


#define IS_SEPARATOR(a)	(a=='.' || a==' ' || a=='-' || a=='_')

/* finds the best possible flipped name. For renaming; check for unique names afterwards */
/* if strip_number: removes number extensions */
void bone_flip_name (char *name, int strip_number)
{
	int		len;
	char	prefix[128]={""};	/* The part before the facing */
	char	suffix[128]={""};	/* The part after the facing */
	char	replace[128]={""};	/* The replacement string */
	char	number[128]={""};	/* The number extension string */
	char	*index=NULL;

	len= strlen(name);
	if(len<3) return;	// we don't do names like .R or .L

	/* We first check the case with a .### extension, let's find the last period */
	if(isdigit(name[len-1])) {
		index= strrchr(name, '.');	// last occurrance
		if (index && isdigit(index[1]) ) {		// doesnt handle case bone.1abc2 correct..., whatever!
			if(strip_number==0) 
				strcpy(number, index);
			*index= 0;
			len= strlen(name);
		}
	}

	strcpy (prefix, name);

	/* first case; separator . - _ with extensions r R l L  */
	if( IS_SEPARATOR(name[len-2]) ) {
		switch(name[len-1]) {
			case 'l':
				prefix[len-1]= 0;
				strcpy(replace, "r");
				break;
			case 'r':
				prefix[len-1]= 0;
				strcpy(replace, "l");
				break;
			case 'L':
				prefix[len-1]= 0;
				strcpy(replace, "R");
				break;
			case 'R':
				prefix[len-1]= 0;
				strcpy(replace, "L");
				break;
		}
	}
	/* case; beginning with r R l L , with separator after it */
	else if( IS_SEPARATOR(name[1]) ) {
		switch(name[0]) {
			case 'l':
				strcpy(replace, "r");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'r':
				strcpy(replace, "l");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'L':
				strcpy(replace, "R");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
			case 'R':
				strcpy(replace, "L");
				strcpy(suffix, name+1);
				prefix[0]= 0;
				break;
		}
	}
	else if(len > 5) {
		/* hrms, why test for a separator? lets do the rule 'ultimate left or right' */
		index = BLI_strcasestr(prefix, "right");
		if (index==prefix || index==prefix+len-5) {
			if(index[0]=='r') 
				strcpy (replace, "left");
			else {
				if(index[1]=='I') 
					strcpy (replace, "LEFT");
				else
					strcpy (replace, "Left");
			}
			*index= 0;
			strcpy (suffix, index+5);
		}
		else {
			index = BLI_strcasestr(prefix, "left");
			if (index==prefix || index==prefix+len-4) {
				if(index[0]=='l') 
					strcpy (replace, "right");
				else {
					if(index[1]=='E') 
						strcpy (replace, "RIGHT");
					else
						strcpy (replace, "Right");
				}
				*index= 0;
				strcpy (suffix, index+4);
			}
		}		
	}

	sprintf (name, "%s%s%s%s", prefix, replace, suffix, number);
}

/* Finds the best possible extension to the name on a particular axis. (For renaming, check for unique names afterwards)
 * This assumes that bone names are at most 32 chars long!
 * 	strip_number: removes number extensions  (TODO: not used)
 *	axis: the axis to name on
 *	head/tail: the head/tail co-ordinate of the bone on the specified axis
 */
void bone_autoside_name (char *name, int strip_number, short axis, float head, float tail)
{
	unsigned int len;
	char	basename[32]={""};
	char 	extension[5]={""};

	len= strlen(name);
	if (len == 0) return;
	strcpy(basename, name);
	
	/* Figure out extension to append: 
	 *	- The extension to append is based upon the axis that we are working on.
	 *	- If head happens to be on 0, then we must consider the tail position as well to decide
	 *	  which side the bone is on
	 *		-> If tail is 0, then it's bone is considered to be on axis, so no extension should be added
	 *		-> Otherwise, extension is added from perspective of object based on which side tail goes to
	 *	- If head is non-zero, extension is added from perspective of object based on side head is on
	 */
	if (axis == 2) {
		/* z-axis - vertical (top/bottom) */
		if (IS_EQ(head, 0)) {
			if (tail < 0)
				strcpy(extension, "Bot");
			else if (tail > 0)
				strcpy(extension, "Top");
		}
		else {
			if (head < 0)
				strcpy(extension, "Bot");
			else
				strcpy(extension, "Top");
		}
	}
	else if (axis == 1) {
		/* y-axis - depth (front/back) */
		if (IS_EQ(head, 0)) {
			if (tail < 0)
				strcpy(extension, "Fr");
			else if (tail > 0)
				strcpy(extension, "Bk");
		}
		else {
			if (head < 0)
				strcpy(extension, "Fr");
			else
				strcpy(extension, "Bk");
		}
	}
	else {
		/* x-axis - horizontal (left/right) */
		if (IS_EQ(head, 0)) {
			if (tail < 0)
				strcpy(extension, "R");
			else if (tail > 0)
				strcpy(extension, "L");
		}
		else {
			if (head < 0)
				strcpy(extension, "R");
			else if (head > 0)
				strcpy(extension, "L");
		}
	}

	/* Simple name truncation 
	 *	- truncate if there is an extension and it wouldn't be able to fit
	 *	- otherwise, just append to end
	 */
	if (extension[0]) {
		int change = 1;
		
		while (change) { /* remove extensions */
			change = 0;
			if (len > 2 && basename[len-2]=='.') {
				if (basename[len-1]=='L' || basename[len-1] == 'R' ) { /* L R */
					basename[len-2] = '\0';
					len-=2;
					change= 1;
				}
			} else if (len > 3 && basename[len-3]=='.') {
				if (	(basename[len-2]=='F' && basename[len-1] == 'r') ||	/* Fr */
						(basename[len-2]=='B' && basename[len-1] == 'k')	/* Bk */
				) {
					basename[len-3] = '\0';
					len-=3;
					change= 1;
				}
			} else if (len > 4 && basename[len-4]=='.') {
				if (	(basename[len-3]=='T' && basename[len-2]=='o' && basename[len-1] == 'p') ||	/* Top */
						(basename[len-3]=='B' && basename[len-2]=='o' && basename[len-1] == 't')	/* Bot */
				) {
					basename[len-4] = '\0';
					len-=4;
					change= 1;
				}
			}
		}
		
		if ((32 - len) < strlen(extension) + 1) { /* add 1 for the '.' */
			strncpy(name, basename, len-strlen(extension));
		}
	}

	sprintf(name, "%s.%s", basename, extension);
}

/* ************* B-Bone support ******************* */

#define MAX_BBONE_SUBDIV	32

/* data has MAX_BBONE_SUBDIV+1 interpolated points, will become desired amount with equal distances */
static void equalize_bezier(float *data, int desired)
{
	float *fp, totdist, ddist, dist, fac1, fac2;
	float pdist[MAX_BBONE_SUBDIV+1];
	float temp[MAX_BBONE_SUBDIV+1][4];
	int a, nr;
	
	pdist[0]= 0.0f;
	for(a=0, fp= data; a<MAX_BBONE_SUBDIV; a++, fp+=4) {
		QUATCOPY(temp[a], fp);
		pdist[a+1]= pdist[a]+VecLenf(fp, fp+4);
	}
	/* do last point */
	QUATCOPY(temp[a], fp);
	totdist= pdist[a];
	
	/* go over distances and calculate new points */
	ddist= totdist/((float)desired);
	nr= 1;
	for(a=1, fp= data+4; a<desired; a++, fp+=4) {
		
		dist= ((float)a)*ddist;
		
		/* we're looking for location (distance) 'dist' in the array */
		while((dist>= pdist[nr]) && nr<MAX_BBONE_SUBDIV) {
			nr++;
		}
		
		fac1= pdist[nr]- pdist[nr-1];
		fac2= pdist[nr]-dist;
		fac1= fac2/fac1;
		fac2= 1.0f-fac1;
		
		fp[0]= fac1*temp[nr-1][0]+ fac2*temp[nr][0];
		fp[1]= fac1*temp[nr-1][1]+ fac2*temp[nr][1];
		fp[2]= fac1*temp[nr-1][2]+ fac2*temp[nr][2];
		fp[3]= fac1*temp[nr-1][3]+ fac2*temp[nr][3];
	}
	/* set last point, needed for orientation calculus */
	QUATCOPY(fp, temp[MAX_BBONE_SUBDIV]);
}

/* returns pointer to static array, filled with desired amount of bone->segments elements */
/* this calculation is done  within unit bone space */
Mat4 *b_bone_spline_setup(bPoseChannel *pchan, int rest)
{
	static Mat4 bbone_array[MAX_BBONE_SUBDIV];
	static Mat4 bbone_rest_array[MAX_BBONE_SUBDIV];
	Mat4 *result_array= (rest)? bbone_rest_array: bbone_array;
	bPoseChannel *next, *prev;
	Bone *bone= pchan->bone;
	float h1[3], h2[3], scale[3], length, hlength1, hlength2, roll1=0.0f, roll2;
	float mat3[3][3], imat[4][4], posemat[4][4], scalemat[4][4], iscalemat[4][4];
	float data[MAX_BBONE_SUBDIV+1][4], *fp;
	int a, doscale= 0;

	length= bone->length;

	if(!rest) {
		/* check if we need to take non-uniform bone scaling into account */
		scale[0]= VecLength(pchan->pose_mat[0]);
		scale[1]= VecLength(pchan->pose_mat[1]);
		scale[2]= VecLength(pchan->pose_mat[2]);

		if(fabs(scale[0] - scale[1]) > 1e-6f || fabs(scale[1] - scale[2]) > 1e-6f) {
			Mat4One(scalemat);
			scalemat[0][0]= scale[0];
			scalemat[1][1]= scale[1];
			scalemat[2][2]= scale[2];
			Mat4Invert(iscalemat, scalemat);

			length *= scale[1];
			doscale = 1;
		}
	}
	
	hlength1= bone->ease1*length*0.390464f;		// 0.5*sqrt(2)*kappa, the handle length for near-perfect circles
	hlength2= bone->ease2*length*0.390464f;
	
	/* evaluate next and prev bones */
	if(bone->flag & BONE_CONNECTED)
		prev= pchan->parent;
	else
		prev= NULL;
	
	next= pchan->child;
	
	/* find the handle points, since this is inside bone space, the 
		first point = (0,0,0)
		last point =  (0, length, 0) */
	
	if(rest) {
		Mat4Invert(imat, pchan->bone->arm_mat);
	}
	else if(doscale) {
		Mat4CpyMat4(posemat, pchan->pose_mat);
		Mat4Ortho(posemat);
		Mat4Invert(imat, posemat);
	}
	else
		Mat4Invert(imat, pchan->pose_mat);
	
	if(prev) {
		float difmat[4][4], result[3][3], imat3[3][3];

		/* transform previous point inside this bone space */
		if(rest)
			VECCOPY(h1, prev->bone->arm_head)
		else
			VECCOPY(h1, prev->pose_head)
		Mat4MulVecfl(imat, h1);

		if(prev->bone->segments>1) {
			/* if previous bone is B-bone too, use average handle direction */
			h1[1]-= length;
			roll1= 0.0f;
		}

		Normalize(h1);
		VecMulf(h1, -hlength1);

		if(prev->bone->segments==1) {
			/* find the previous roll to interpolate */
			if(rest)
				Mat4MulMat4(difmat, prev->bone->arm_mat, imat);
			else
				Mat4MulMat4(difmat, prev->pose_mat, imat);
			Mat3CpyMat4(result, difmat);				// the desired rotation at beginning of next bone
			
			vec_roll_to_mat3(h1, 0.0f, mat3);			// the result of vec_roll without roll
			
			Mat3Inv(imat3, mat3);
			Mat3MulMat3(mat3, result, imat3);			// the matrix transforming vec_roll to desired roll
			
			roll1= (float)atan2(mat3[2][0], mat3[2][2]);
		}
	}
	else {
		h1[0]= 0.0f; h1[1]= hlength1; h1[2]= 0.0f;
		roll1= 0.0f;
	}
	if(next) {
		float difmat[4][4], result[3][3], imat3[3][3];
		
		/* transform next point inside this bone space */
		if(rest)
			VECCOPY(h2, next->bone->arm_tail)
		else
			VECCOPY(h2, next->pose_tail)
		Mat4MulVecfl(imat, h2);
		/* if next bone is B-bone too, use average handle direction */
		if(next->bone->segments>1);
		else h2[1]-= length;
		Normalize(h2);
		
		/* find the next roll to interpolate as well */
		if(rest)
			Mat4MulMat4(difmat, next->bone->arm_mat, imat);
		else
			Mat4MulMat4(difmat, next->pose_mat, imat);
		Mat3CpyMat4(result, difmat);				// the desired rotation at beginning of next bone
		
		vec_roll_to_mat3(h2, 0.0f, mat3);			// the result of vec_roll without roll
		
		Mat3Inv(imat3, mat3);
		Mat3MulMat3(mat3, imat3, result);			// the matrix transforming vec_roll to desired roll
		
		roll2= (float)atan2(mat3[2][0], mat3[2][2]);
		
		/* and only now negate handle */
		VecMulf(h2, -hlength2);
	}
	else {
		h2[0]= 0.0f; h2[1]= -hlength2; h2[2]= 0.0f;
		roll2= 0.0;
	}

	/* make curve */
	if(bone->segments > MAX_BBONE_SUBDIV)
		bone->segments= MAX_BBONE_SUBDIV;
	
	forward_diff_bezier(0.0, h1[0],		h2[0],			0.0,		data[0],	MAX_BBONE_SUBDIV, 4*sizeof(float));
	forward_diff_bezier(0.0, h1[1],		length + h2[1],	length,		data[0]+1,	MAX_BBONE_SUBDIV, 4*sizeof(float));
	forward_diff_bezier(0.0, h1[2],		h2[2],			0.0,		data[0]+2,	MAX_BBONE_SUBDIV, 4*sizeof(float));
	forward_diff_bezier(roll1, roll1 + 0.390464f*(roll2-roll1), roll2 - 0.390464f*(roll2-roll1),	roll2,	data[0]+3,	MAX_BBONE_SUBDIV, 4*sizeof(float));
	
	equalize_bezier(data[0], bone->segments);	// note: does stride 4!
	
	/* make transformation matrices for the segments for drawing */
	for(a=0, fp= data[0]; a<bone->segments; a++, fp+=4) {
		VecSubf(h1, fp+4, fp);
		vec_roll_to_mat3(h1, fp[3], mat3);		// fp[3] is roll

		Mat4CpyMat3(result_array[a].mat, mat3);
		VECCOPY(result_array[a].mat[3], fp);

		if(doscale) {
			/* correct for scaling when this matrix is used in scaled space */
			Mat4MulSerie(result_array[a].mat, iscalemat, result_array[a].mat,
				scalemat, NULL, NULL, NULL, NULL, NULL);
		}
	}
	
	return result_array;
}

/* ************ Armature Deform ******************* */

static void pchan_b_bone_defmats(bPoseChannel *pchan, int use_quaternion, int rest_def)
{
	Bone *bone= pchan->bone;
	Mat4 *b_bone= b_bone_spline_setup(pchan, 0);
	Mat4 *b_bone_rest= (rest_def)? NULL: b_bone_spline_setup(pchan, 1);
	Mat4 *b_bone_mats;
	DualQuat *b_bone_dual_quats= NULL;
	float tmat[4][4];
	int a;
	
	/* allocate b_bone matrices and dual quats */
	b_bone_mats= MEM_mallocN((1+bone->segments)*sizeof(Mat4), "BBone defmats");
	pchan->b_bone_mats= b_bone_mats;

	if(use_quaternion) {
		b_bone_dual_quats= MEM_mallocN((bone->segments)*sizeof(DualQuat), "BBone dqs");
		pchan->b_bone_dual_quats= b_bone_dual_quats;
	}
	
	/* first matrix is the inverse arm_mat, to bring points in local bone space
	   for finding out which segment it belongs to */
	Mat4Invert(b_bone_mats[0].mat, bone->arm_mat);

	/* then we make the b_bone_mats:
	    - first transform to local bone space
		- translate over the curve to the bbone mat space
		- transform with b_bone matrix
		- transform back into global space */
	Mat4One(tmat);

	for(a=0; a<bone->segments; a++) {
		if(b_bone_rest)
			Mat4Invert(tmat, b_bone_rest[a].mat);
		else
			tmat[3][1] = -a*(bone->length/(float)bone->segments);

		Mat4MulSerie(b_bone_mats[a+1].mat, pchan->chan_mat, bone->arm_mat,
			b_bone[a].mat, tmat, b_bone_mats[0].mat, NULL, NULL, NULL);

		if(use_quaternion)
			Mat4ToDQuat(bone->arm_mat, b_bone_mats[a+1].mat, &b_bone_dual_quats[a]);
	}
}

static void b_bone_deform(bPoseChannel *pchan, Bone *bone, float *co, DualQuat *dq, float defmat[][3])
{
	Mat4 *b_bone= pchan->b_bone_mats;
	float (*mat)[4]= b_bone[0].mat;
	float segment, y;
	int a;
	
	/* need to transform co back to bonespace, only need y */
	y= mat[0][1]*co[0] + mat[1][1]*co[1] + mat[2][1]*co[2] + mat[3][1];
	
	/* now calculate which of the b_bones are deforming this */
	segment= bone->length/((float)bone->segments);
	a= (int)(y/segment);
	
	/* note; by clamping it extends deform at endpoints, goes best with
	   straight joints in restpos. */
	CLAMP(a, 0, bone->segments-1);

	if(dq) {
		DQuatCpyDQuat(dq, &((DualQuat*)pchan->b_bone_dual_quats)[a]);
	}
	else {
		Mat4MulVecfl(b_bone[a+1].mat, co);

		if(defmat)
			Mat3CpyMat4(defmat, b_bone[a+1].mat);
	}
}

/* using vec with dist to bone b1 - b2 */
float distfactor_to_bone (float vec[3], float b1[3], float b2[3], float rad1, float rad2, float rdist)
{
	float dist=0.0f; 
	float bdelta[3];
	float pdelta[3];
	float hsqr, a, l, rad;
	
	VecSubf (bdelta, b2, b1);
	l = Normalize (bdelta);
	
	VecSubf (pdelta, vec, b1);
	
	a = bdelta[0]*pdelta[0] + bdelta[1]*pdelta[1] + bdelta[2]*pdelta[2];
	hsqr = ((pdelta[0]*pdelta[0]) + (pdelta[1]*pdelta[1]) + (pdelta[2]*pdelta[2]));
	
	if (a < 0.0F){
		/* If we're past the end of the bone, do a spherical field attenuation thing */
		dist= ((b1[0]-vec[0])*(b1[0]-vec[0]) +(b1[1]-vec[1])*(b1[1]-vec[1]) +(b1[2]-vec[2])*(b1[2]-vec[2])) ;
		rad= rad1;
	}
	else if (a > l){
		/* If we're past the end of the bone, do a spherical field attenuation thing */
		dist= ((b2[0]-vec[0])*(b2[0]-vec[0]) +(b2[1]-vec[1])*(b2[1]-vec[1]) +(b2[2]-vec[2])*(b2[2]-vec[2])) ;
		rad= rad2;
	}
	else {
		dist= (hsqr - (a*a));
		
		if(l!=0.0f) {
			rad= a/l;
			rad= rad*rad2 + (1.0f-rad)*rad1;
		}
		else rad= rad1;
	}
	
	a= rad*rad;
	if(dist < a) 
		return 1.0f;
	else {
		l= rad+rdist;
		l*= l;
		if(rdist==0.0f || dist >= l) 
			return 0.0f;
		else {
			a= (float)sqrt(dist)-rad;
			return 1.0f-( a*a )/( rdist*rdist );
		}
	}
}

static void pchan_deform_mat_add(bPoseChannel *pchan, float weight, float bbonemat[][3], float mat[][3])
{
	float wmat[3][3];

	if(pchan->bone->segments>1)
		Mat3CpyMat3(wmat, bbonemat);
	else
		Mat3CpyMat4(wmat, pchan->chan_mat);

	Mat3MulFloat((float*)wmat, weight);
	Mat3AddMat3(mat, mat, wmat);
}

static float dist_bone_deform(bPoseChannel *pchan, float *vec, DualQuat *dq, float mat[][3], float *co)
{
	Bone *bone= pchan->bone;
	float fac, contrib=0.0;
	float cop[3], bbonemat[3][3];
	DualQuat bbonedq;

	if(bone==NULL) return 0.0f;
	
	VECCOPY (cop, co);

	fac= distfactor_to_bone(cop, bone->arm_head, bone->arm_tail, bone->rad_head, bone->rad_tail, bone->dist);
	
	if (fac>0.0) {
		
		fac*=bone->weight;
		contrib= fac;
		if(contrib>0.0) {
			if(vec) {
				if(bone->segments>1)
					// applies on cop and bbonemat
					b_bone_deform(pchan, bone, cop, NULL, (mat)?bbonemat:NULL);
				else
					Mat4MulVecfl(pchan->chan_mat, cop);

				//	Make this a delta from the base position
				VecSubf (cop, cop, co);
				cop[0]*=fac; cop[1]*=fac; cop[2]*=fac;
				VecAddf (vec, vec, cop);

				if(mat)
					pchan_deform_mat_add(pchan, fac, bbonemat, mat);
			}
			else {
				if(bone->segments>1) {
					b_bone_deform(pchan, bone, cop, &bbonedq, NULL);
					DQuatAddWeighted(dq, &bbonedq, fac);
				}
				else
					DQuatAddWeighted(dq, pchan->dual_quat, fac);
			}
		}
	}
	
	return contrib;
}

static void pchan_bone_deform(bPoseChannel *pchan, float weight, float *vec, DualQuat *dq, float mat[][3], float *co, float *contrib)
{
	float cop[3], bbonemat[3][3];
	DualQuat bbonedq;

	if (!weight)
		return;

	VECCOPY(cop, co);

	if(vec) {
		if(pchan->bone->segments>1)
			// applies on cop and bbonemat
			b_bone_deform(pchan, pchan->bone, cop, NULL, (mat)?bbonemat:NULL);
		else
			Mat4MulVecfl(pchan->chan_mat, cop);
		
		vec[0]+=(cop[0]-co[0])*weight;
		vec[1]+=(cop[1]-co[1])*weight;
		vec[2]+=(cop[2]-co[2])*weight;

		if(mat)
			pchan_deform_mat_add(pchan, weight, bbonemat, mat);
	}
	else {
		if(pchan->bone->segments>1) {
			b_bone_deform(pchan, pchan->bone, cop, &bbonedq, NULL);
			DQuatAddWeighted(dq, &bbonedq, weight);
		}
		else
			DQuatAddWeighted(dq, pchan->dual_quat, weight);
	}

	(*contrib)+=weight;
}

void armature_deform_verts(Object *armOb, Object *target, DerivedMesh *dm,
                           float (*vertexCos)[3], float (*defMats)[3][3],
						   int numVerts, int deformflag, 
						   float (*prevCos)[3], const char *defgrp_name)
{
	bArmature *arm= armOb->data;
	bPoseChannel *pchan, **defnrToPC = NULL;
	MDeformVert *dverts = NULL;
	bDeformGroup *dg;
	DualQuat *dualquats= NULL;
	float obinv[4][4], premat[4][4], postmat[4][4];
	int use_envelope = deformflag & ARM_DEF_ENVELOPE;
	int use_quaternion = deformflag & ARM_DEF_QUATERNION;
	int bbone_rest_def = deformflag & ARM_DEF_B_BONE_REST;
	int invert_vgroup= deformflag & ARM_DEF_INVERT_VGROUP;
	int numGroups = 0;		/* safety for vertexgroup index overflow */
	int i, target_totvert = 0;	/* safety for vertexgroup overflow */
	int use_dverts = 0;
	int armature_def_nr = -1;
	int totchan;

	if(arm->edbo) return;
	
	Mat4Invert(obinv, target->obmat);
	Mat4CpyMat4(premat, target->obmat);
	Mat4MulMat4(postmat, armOb->obmat, obinv);
	Mat4Invert(premat, postmat);

	/* bone defmats are already in the channels, chan_mat */
	
	/* initialize B_bone matrices and dual quaternions */
	if(use_quaternion) {
		totchan= BLI_countlist(&armOb->pose->chanbase);
		dualquats= MEM_callocN(sizeof(DualQuat)*totchan, "dualquats");
	}

	totchan= 0;
	for(pchan = armOb->pose->chanbase.first; pchan; pchan = pchan->next) {
		if(!(pchan->bone->flag & BONE_NO_DEFORM)) {
			if(pchan->bone->segments > 1)
				pchan_b_bone_defmats(pchan, use_quaternion, bbone_rest_def);

			if(use_quaternion) {
				pchan->dual_quat= &dualquats[totchan++];
				Mat4ToDQuat(pchan->bone->arm_mat, pchan->chan_mat, pchan->dual_quat);
			}
		}
	}

	/* get the def_nr for the overall armature vertex group if present */
	for(i = 0, dg = target->defbase.first; dg; i++, dg = dg->next)
		if(defgrp_name && strcmp(defgrp_name, dg->name) == 0)
			armature_def_nr = i;

	/* get a vertex-deform-index to posechannel array */
	if(deformflag & ARM_DEF_VGROUP) {
		if(ELEM(target->type, OB_MESH, OB_LATTICE)) {
			numGroups = BLI_countlist(&target->defbase);
			
			if(target->type==OB_MESH) {
				Mesh *me= target->data;
				dverts = me->dvert;
				target_totvert = me->totvert;
			}
			else {
				Lattice *lt= target->data;
				dverts = lt->dvert;
				if(dverts)
					target_totvert = lt->pntsu*lt->pntsv*lt->pntsw;
			}
			/* if we have a DerivedMesh, only use dverts if it has them */
			if(dm)
				if(dm->getVertData(dm, 0, CD_MDEFORMVERT))
					use_dverts = 1;
				else use_dverts = 0;
			else if(dverts) use_dverts = 1;

			if(use_dverts) {
				defnrToPC = MEM_callocN(sizeof(*defnrToPC) * numGroups,
				                        "defnrToBone");
				for(i = 0, dg = target->defbase.first; dg;
				    i++, dg = dg->next) {
					defnrToPC[i] = get_pose_channel(armOb->pose, dg->name);
					/* exclude non-deforming bones */
					if(defnrToPC[i]) {
						if(defnrToPC[i]->bone->flag & BONE_NO_DEFORM)
							defnrToPC[i]= NULL;
					}
				}
			}
		}
	}

	for(i = 0; i < numVerts; i++) {
		MDeformVert *dvert;
		DualQuat sumdq, *dq = NULL;
		float *co, dco[3];
		float sumvec[3], summat[3][3];
		float *vec = NULL, (*smat)[3] = NULL;
		float contrib = 0.0f;
		float armature_weight = 1.0f;	/* default to 1 if no overall def group */
		float prevco_weight = 1.0f;		/* weight for optional cached vertexcos */
		int	  j;

		if(use_quaternion) {
			memset(&sumdq, 0, sizeof(DualQuat));
			dq= &sumdq;
		}
		else {
			sumvec[0] = sumvec[1] = sumvec[2] = 0.0f;
			vec= sumvec;

			if(defMats) {
				Mat3Clr((float*)summat);
				smat = summat;
			}
		}

		if(use_dverts || armature_def_nr >= 0) {
			if(dm) dvert = dm->getVertData(dm, i, CD_MDEFORMVERT);
			else if(dverts && i < target_totvert) dvert = dverts + i;
			else dvert = NULL;
		} else
			dvert = NULL;

		if(armature_def_nr >= 0 && dvert) {
			armature_weight = 0.0f; /* a def group was given, so default to 0 */
			for(j = 0; j < dvert->totweight; j++) {
				if(dvert->dw[j].def_nr == armature_def_nr) {
					armature_weight = dvert->dw[j].weight;
					break;
				}
			}
			/* hackish: the blending factor can be used for blending with prevCos too */
			if(prevCos) {
				if(invert_vgroup)
					prevco_weight= 1.0f-armature_weight;
				else
					prevco_weight= armature_weight;
				armature_weight= 1.0f;
			}
		}

		/* check if there's any  point in calculating for this vert */
		if(armature_weight == 0.0f) continue;
		
		/* get the coord we work on */
		co= prevCos?prevCos[i]:vertexCos[i];
		
		/* Apply the object's matrix */
		Mat4MulVecfl(premat, co);
		
		if(use_dverts && dvert && dvert->totweight) { // use weight groups ?
			int deformed = 0;
			
			for(j = 0; j < dvert->totweight; j++){
				int index = dvert->dw[j].def_nr;
				pchan = index < numGroups?defnrToPC[index]:NULL;
				if(pchan) {
					float weight = dvert->dw[j].weight;
					Bone *bone = pchan->bone;

					deformed = 1;
					
					if(bone && bone->flag & BONE_MULT_VG_ENV) {
						weight *= distfactor_to_bone(co, bone->arm_head,
						                             bone->arm_tail,
						                             bone->rad_head,
						                             bone->rad_tail,
						                             bone->dist);
					}
					pchan_bone_deform(pchan, weight, vec, dq, smat, co, &contrib);
				}
			}
			/* if there are vertexgroups but not groups with bones
			 * (like for softbody groups)
			 */
			if(deformed == 0 && use_envelope) {
				for(pchan = armOb->pose->chanbase.first; pchan;
				    pchan = pchan->next) {
					if(!(pchan->bone->flag & BONE_NO_DEFORM))
						contrib += dist_bone_deform(pchan, vec, dq, smat, co);
				}
			}
		}
		else if(use_envelope) {
			for(pchan = armOb->pose->chanbase.first; pchan;
			    pchan = pchan->next) {
				if(!(pchan->bone->flag & BONE_NO_DEFORM))
					contrib += dist_bone_deform(pchan, vec, dq, smat, co);
			}
		}

		/* actually should be EPSILON? weight values and contrib can be like 10e-39 small */
		if(contrib > 0.0001f) {
			if(use_quaternion) {
				DQuatNormalize(dq, contrib);

				if(armature_weight != 1.0f) {
					VECCOPY(dco, co);
					DQuatMulVecfl(dq, dco, (defMats)? summat: NULL);
					VecSubf(dco, dco, co);
					VecMulf(dco, armature_weight);
					VecAddf(co, co, dco);
				}
				else
					DQuatMulVecfl(dq, co, (defMats)? summat: NULL);

				smat = summat;
			}
			else {
				VecMulf(vec, armature_weight/contrib);
				VecAddf(co, vec, co);
			}

			if(defMats) {
				float pre[3][3], post[3][3], tmpmat[3][3];

				Mat3CpyMat4(pre, premat);
				Mat3CpyMat4(post, postmat);
				Mat3CpyMat3(tmpmat, defMats[i]);

				if(!use_quaternion) /* quaternion already is scale corrected */
					Mat3MulFloat((float*)smat, armature_weight/contrib);

				Mat3MulSerie(defMats[i], tmpmat, pre, smat, post,
					NULL, NULL, NULL, NULL);
			}
		}
		
		/* always, check above code */
		Mat4MulVecfl(postmat, co);
		
		
		/* interpolate with previous modifier position using weight group */
		if(prevCos) {
			float mw= 1.0f - prevco_weight;
			vertexCos[i][0]= prevco_weight*vertexCos[i][0] + mw*co[0];
			vertexCos[i][1]= prevco_weight*vertexCos[i][1] + mw*co[1];
			vertexCos[i][2]= prevco_weight*vertexCos[i][2] + mw*co[2];
		}
	}

	if(dualquats) MEM_freeN(dualquats);
	if(defnrToPC) MEM_freeN(defnrToPC);
	
	/* free B_bone matrices */
	for(pchan = armOb->pose->chanbase.first; pchan; pchan = pchan->next) {
		if(pchan->b_bone_mats) {
			MEM_freeN(pchan->b_bone_mats);
			pchan->b_bone_mats = NULL;
		}
		if(pchan->b_bone_dual_quats) {
			MEM_freeN(pchan->b_bone_dual_quats);
			pchan->b_bone_dual_quats = NULL;
		}

		pchan->dual_quat = NULL;
	}
}

/* ************ END Armature Deform ******************* */

void get_objectspace_bone_matrix (struct Bone* bone, float M_accumulatedMatrix[][4], int root, int posed)
{
	Mat4CpyMat4(M_accumulatedMatrix, bone->arm_mat);
}

/* **************** Space to Space API ****************** */

/* Convert World-Space Matrix to Pose-Space Matrix */
void armature_mat_world_to_pose(Object *ob, float inmat[][4], float outmat[][4]) 
{
	float obmat[4][4];
	
	/* prevent crashes */
	if (ob==NULL) return;
	
	/* get inverse of (armature) object's matrix  */
	Mat4Invert(obmat, ob->obmat);
	
	/* multiply given matrix by object's-inverse to find pose-space matrix */
	Mat4MulMat4(outmat, obmat, inmat);
}

/* Convert Wolrd-Space Location to Pose-Space Location
 * NOTE: this cannot be used to convert to pose-space location of the supplied
 * 		pose-channel into its local space (i.e. 'visual'-keyframing) 
 */
void armature_loc_world_to_pose(Object *ob, float *inloc, float *outloc) 
{
	float xLocMat[4][4];
	float nLocMat[4][4];
	
	/* build matrix for location */
	Mat4One(xLocMat);
	VECCOPY(xLocMat[3], inloc);

	/* get bone-space cursor matrix and extract location */
	armature_mat_world_to_pose(ob, xLocMat, nLocMat);
	VECCOPY(outloc, nLocMat[3]);
}

/* Convert Pose-Space Matrix to Bone-Space Matrix 
 * NOTE: this cannot be used to convert to pose-space transforms of the supplied
 * 		pose-channel into its local space (i.e. 'visual'-keyframing)
 */
void armature_mat_pose_to_bone(bPoseChannel *pchan, float inmat[][4], float outmat[][4])
{
	float pc_trans[4][4], inv_trans[4][4];
	float pc_posemat[4][4], inv_posemat[4][4];
	
	/* paranoia: prevent crashes with no pose-channel supplied */
	if (pchan==NULL) return;
	
	/* get the inverse matrix of the pchan's transforms */
	if (pchan->rotmode)
		LocEulSizeToMat4(pc_trans, pchan->loc, pchan->eul, pchan->size);
	else
		LocQuatSizeToMat4(pc_trans, pchan->loc, pchan->quat, pchan->size);
	Mat4Invert(inv_trans, pc_trans);
	
	/* Remove the pchan's transforms from it's pose_mat.
	 * This should leave behind the effects of restpose + 
	 * parenting + constraints
	 */
	Mat4MulMat4(pc_posemat, inv_trans, pchan->pose_mat);
	
	/* get the inverse of the leftovers so that we can remove 
	 * that component from the supplied matrix
	 */
	Mat4Invert(inv_posemat, pc_posemat);
	
	/* get the new matrix */
	Mat4MulMat4(outmat, inmat, inv_posemat);
}

/* Convert Pose-Space Location to Bone-Space Location
 * NOTE: this cannot be used to convert to pose-space location of the supplied
 * 		pose-channel into its local space (i.e. 'visual'-keyframing) 
 */
void armature_loc_pose_to_bone(bPoseChannel *pchan, float *inloc, float *outloc) 
{
	float xLocMat[4][4];
	float nLocMat[4][4];
	
	/* build matrix for location */
	Mat4One(xLocMat);
	VECCOPY(xLocMat[3], inloc);

	/* get bone-space cursor matrix and extract location */
	armature_mat_pose_to_bone(pchan, xLocMat, nLocMat);
	VECCOPY(outloc, nLocMat[3]);
}

/* Remove rest-position effects from pose-transform for obtaining
 * 'visual' transformation of pose-channel. 
 * (used by the Visual-Keyframing stuff)
 */
void armature_mat_pose_to_delta(float delta_mat[][4], float pose_mat[][4], float arm_mat[][4])
{
 	float imat[4][4];
 
 	Mat4Invert(imat, arm_mat);
 	Mat4MulMat4(delta_mat, pose_mat, imat);
}

/* **************** Rotation Mode Conversions ****************************** */
/* Used for Objects and Pose Channels, since both can have multiple rotation representations */

/* Called from RNA when rotation mode changes 
 *	- the result should be that the rotations given in the provided pointers have had conversions 
 *	  applied (as appropriate), such that the rotation of the element hasn't 'visually' changed 
 *
 *	- as in SDNA data, quat is used to store quaternions AND axis-angle rotations...
 */
void BKE_rotMode_change_values (float quat[4], float eul[3], short oldMode, short newMode)
{
	/* check if any change - if so, need to convert data */
	if (newMode > 0) { /* to euler */
		if (oldMode == ROT_MODE_AXISANGLE) {
			/* axis-angle to euler */
			AxisAngleToEulO(&quat[1], quat[0], eul, newMode);
		}
		else if (oldMode == ROT_MODE_QUAT) {
			/* quat to euler */
			QuatToEulO(quat, eul, newMode);
		}
		/* else { no conversion needed } */
	}
	else if (newMode == ROT_MODE_QUAT) { /* to quat */
		if (oldMode == ROT_MODE_AXISANGLE) {
			/* axis angle to quat */
			float q[4];
			
			/* copy to temp var first, since quats and axis-angle are stored in same place */
			QuatCopy(q, quat);
			AxisAngleToQuat(q, &quat[1], quat[0]);
		}
		else if (oldMode > 0) {
			/* euler to quat */
			EulOToQuat(eul, oldMode, quat);
		}
		/* else { no conversion needed } */
	}
	else { /* to axis-angle */
		if (oldMode > 0) {
			/* euler to axis angle */
			EulOToAxisAngle(eul, oldMode, &quat[1], &quat[0]);
		}
		else if (oldMode == ROT_MODE_QUAT) {
			/* quat to axis angle */
			float q[4];
			
			/* copy to temp var first, since quats and axis-angle are stored in same place */
			QuatCopy(q, quat);
			QuatToAxisAngle(q, &quat[1], &quat[0]);
		}
		
		/* when converting to axis-angle, we need a special exception for the case when there is no axis */
		if (IS_EQ(quat[1], quat[2]) && IS_EQ(quat[2], quat[3])) {
			/* for now, rotate around y-axis then (so that it simply becomes the roll) */
			quat[2]= 1.0f;
		}
	}
}

/* **************** The new & simple (but OK!) armature evaluation ********* */ 

/*  ****************** And how it works! ****************************************

  This is the bone transformation trick; they're hierarchical so each bone(b)
  is in the coord system of bone(b-1):

  arm_mat(b)= arm_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) 
  
  -> yoffs is just the y axis translation in parent's coord system
  -> d_root is the translation of the bone root, also in parent's coord system

  pose_mat(b)= pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) * chan_mat(b)

  we then - in init deform - store the deform in chan_mat, such that:

  pose_mat(b)= arm_mat(b) * chan_mat(b)
  
  *************************************************************************** */
/*  Computes vector and roll based on a rotation. "mat" must
     contain only a rotation, and no scaling. */ 
void mat3_to_vec_roll(float mat[][3], float *vec, float *roll) 
{
    if (vec)
        VecCopyf(vec, mat[1]);

    if (roll) {
        float vecmat[3][3], vecmatinv[3][3], rollmat[3][3];

        vec_roll_to_mat3(mat[1], 0.0f, vecmat);
        Mat3Inv(vecmatinv, vecmat);
        Mat3MulMat3(rollmat, vecmatinv, mat);

        *roll= (float)atan2(rollmat[2][0], rollmat[2][2]);
    }
}

/*	Calculates the rest matrix of a bone based
	On its vector and a roll around that vector */
void vec_roll_to_mat3(float *vec, float roll, float mat[][3])
{
	float	nor[3], axis[3], target[3]={0,1,0};
	float	theta;
	float	rMatrix[3][3], bMatrix[3][3];
	
	VECCOPY (nor, vec);
	Normalize (nor);
	
	/*	Find Axis & Amount for bone matrix*/
	Crossf (axis,target,nor);

	if (Inpf(axis,axis) > 0.0000000000001) {
		/* if nor is *not* a multiple of target ... */
		Normalize (axis);
		
		theta= NormalizedVecAngle2(target, nor);
		
		/*	Make Bone matrix*/
		VecRotToMat3(axis, theta, bMatrix);
	}
	else {
		/* if nor is a multiple of target ... */
		float updown;
		
		/* point same direction, or opposite? */
		updown = ( Inpf (target,nor) > 0 ) ? 1.0f : -1.0f;
		
		/* I think this should work ... */
		bMatrix[0][0]=updown; bMatrix[0][1]=0.0;    bMatrix[0][2]=0.0;
		bMatrix[1][0]=0.0;    bMatrix[1][1]=updown; bMatrix[1][2]=0.0;
		bMatrix[2][0]=0.0;    bMatrix[2][1]=0.0;    bMatrix[2][2]=1.0;
	}
	
	/*	Make Roll matrix*/
	VecRotToMat3(nor, roll, rMatrix);
	
	/*	Combine and output result*/
	Mat3MulMat3 (mat, rMatrix, bMatrix);
}


/* recursive part, calculates restposition of entire tree of children */
/* used by exiting editmode too */
void where_is_armature_bone(Bone *bone, Bone *prevbone)
{
	float vec[3];
	
	/* Bone Space */
	VecSubf (vec, bone->tail, bone->head);
	vec_roll_to_mat3(vec, bone->roll, bone->bone_mat);

	bone->length= VecLenf(bone->head, bone->tail);
	
	/* this is called on old file reading too... */
	if(bone->xwidth==0.0) {
		bone->xwidth= 0.1f;
		bone->zwidth= 0.1f;
		bone->segments= 1;
	}
	
	if(prevbone) {
		float offs_bone[4][4];  // yoffs(b-1) + root(b) + bonemat(b)
		
		/* bone transform itself */
		Mat4CpyMat3(offs_bone, bone->bone_mat);
				
		/* The bone's root offset (is in the parent's coordinate system) */
		VECCOPY(offs_bone[3], bone->head);

		/* Get the length translation of parent (length along y axis) */
		offs_bone[3][1]+= prevbone->length;
		
		/* Compose the matrix for this bone  */
		Mat4MulMat4(bone->arm_mat, offs_bone, prevbone->arm_mat);
	}
	else {
		Mat4CpyMat3(bone->arm_mat, bone->bone_mat);
		VECCOPY(bone->arm_mat[3], bone->head);
	}
	
	/* head */
	VECCOPY(bone->arm_head, bone->arm_mat[3]);
	/* tail is in current local coord system */
	VECCOPY(vec, bone->arm_mat[1]);
	VecMulf(vec, bone->length);
	VecAddf(bone->arm_tail, bone->arm_head, vec);
	
	/* and the kiddies */
	prevbone= bone;
	for(bone= bone->childbase.first; bone; bone= bone->next) {
		where_is_armature_bone(bone, prevbone);
	}
}

/* updates vectors and matrices on rest-position level, only needed 
   after editing armature itself, now only on reading file */
void where_is_armature (bArmature *arm)
{
	Bone *bone;
	
	/* hierarchical from root to children */
	for(bone= arm->bonebase.first; bone; bone= bone->next) {
		where_is_armature_bone(bone, NULL);
	}
}

/* if bone layer is protected, copy the data from from->pose */
static void pose_proxy_synchronize(Object *ob, Object *from, int layer_protected)
{
	bPose *pose= ob->pose, *frompose= from->pose;
	bPoseChannel *pchan, *pchanp, pchanw;
	bConstraint *con;
	
	if (frompose==NULL) return;
	
	/* exception, armature local layer should be proxied too */
	if (pose->proxy_layer)
		((bArmature *)ob->data)->layer= pose->proxy_layer;
	
	/* clear all transformation values from library */
	rest_pose(frompose);
	
	/* copy over all of the proxy's bone groups */
		/* TODO for later - implement 'local' bone groups as for constraints
		 *	Note: this isn't trivial, as bones reference groups by index not by pointer, 
		 *		 so syncing things correctly needs careful attention
		 */
	BLI_freelistN(&pose->agroups);
	BLI_duplicatelist(&pose->agroups, &frompose->agroups);
	pose->active_group= frompose->active_group;
	
	for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
		if (pchan->bone->layer & layer_protected) {
			ListBase proxylocal_constraints = {NULL, NULL};
			pchanp= get_pose_channel(frompose, pchan->name);
			
			/* copy posechannel to temp, but restore important pointers */
			pchanw= *pchanp;
			pchanw.prev= pchan->prev;
			pchanw.next= pchan->next;
			pchanw.parent= pchan->parent;
			pchanw.child= pchan->child;
			pchanw.path= NULL;
			
			/* constraints - proxy constraints are flushed... local ones are added after 
			 *	1. extract constraints not from proxy (CONSTRAINT_PROXY_LOCAL) from pchan's constraints
			 *	2. copy proxy-pchan's constraints on-to new
			 *	3. add extracted local constraints back on top 
			 */
			extract_proxylocal_constraints(&proxylocal_constraints, &pchan->constraints);
			copy_constraints(&pchanw.constraints, &pchanp->constraints);
			addlisttolist(&pchanw.constraints, &proxylocal_constraints);
			
			/* constraints - set target ob pointer to own object */
			for (con= pchanw.constraints.first; con; con= con->next) {
				bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;
				
				if (cti && cti->get_constraint_targets) {
					cti->get_constraint_targets(con, &targets);
					
					for (ct= targets.first; ct; ct= ct->next) {
						if (ct->tar == from)
							ct->tar = ob;
					}
					
					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(con, &targets, 0);
				}
			}
			
			/* free stuff from current channel */
			if (pchan->path) MEM_freeN(pchan->path);
			free_constraints(&pchan->constraints);
			
			/* the final copy */
			*pchan= pchanw;
		}
	}
}

static int rebuild_pose_bone(bPose *pose, Bone *bone, bPoseChannel *parchan, int counter)
{
	bPoseChannel *pchan = verify_pose_channel (pose, bone->name);   // verify checks and/or adds

	pchan->bone= bone;
	pchan->parent= parchan;
	
	counter++;
	
	for(bone= bone->childbase.first; bone; bone= bone->next) {
		counter= rebuild_pose_bone(pose, bone, pchan, counter);
		/* for quick detecting of next bone in chain, only b-bone uses it now */
		if(bone->flag & BONE_CONNECTED)
			pchan->child= get_pose_channel(pose, bone->name);
	}
	
	return counter;
}

/* only after leave editmode, duplicating, validating older files, library syncing */
/* NOTE: pose->flag is set for it */
void armature_rebuild_pose(Object *ob, bArmature *arm)
{
	Bone *bone;
	bPose *pose;
	bPoseChannel *pchan, *next;
	int counter=0;
		
	/* only done here */
	if(ob->pose==NULL) ob->pose= MEM_callocN(sizeof(bPose), "new pose");
	pose= ob->pose;
	
	/* clear */
	for(pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
		pchan->bone= NULL;
		pchan->child= NULL;
	}
	
	/* first step, check if all channels are there */
	for(bone= arm->bonebase.first; bone; bone= bone->next) {
		counter= rebuild_pose_bone(pose, bone, NULL, counter);
	}

	/* and a check for garbage */
	for(pchan= pose->chanbase.first; pchan; pchan= next) {
		next= pchan->next;
		if(pchan->bone==NULL) {
			if(pchan->path)
				MEM_freeN(pchan->path);
			free_constraints(&pchan->constraints);
			BLI_freelinkN(&pose->chanbase, pchan);
		}
	}
	// printf("rebuild pose %s, %d bones\n", ob->id.name, counter);
	
	/* synchronize protected layers with proxy */
	if(ob->proxy)
		pose_proxy_synchronize(ob, ob->proxy, arm->layer_protected);
	
	update_pose_constraint_flags(ob->pose); // for IK detection for example
	
	/* the sorting */
	if(counter>1)
		DAG_pose_sort(ob);
	
	ob->pose->flag &= ~POSE_RECALC;
	ob->pose->flag |= POSE_WAS_REBUILT;
}


/* ********************** THE POSE SOLVER ******************* */


/* loc/rot/size to mat4 */
/* used in constraint.c too */
void chan_calc_mat(bPoseChannel *chan)
{
	float smat[3][3];
	float rmat[3][3];
	float tmat[3][3];
	
	/* get scaling matrix */
	SizeToMat3(chan->size, smat);
	
	/* rotations may either be quats, eulers (with various rotation orders), or axis-angle */
	if (chan->rotmode > 0) {
		/* euler rotations (will cause gimble lock, but this can be alleviated a bit with rotation orders) */
		EulOToMat3(chan->eul, chan->rotmode, rmat);
	}
	else if (chan->rotmode == ROT_MODE_AXISANGLE) {
		/* axis-angle - stored in quaternion data, but not really that great for 3D-changing orientations */
		AxisAngleToMat3(&chan->quat[1], chan->quat[0], rmat);
	}
	else {
		/* quats are normalised before use to eliminate scaling issues */
		NormalQuat(chan->quat);
		QuatToMat3(chan->quat, rmat);
	}
	
	/* calculate matrix of bone (as 3x3 matrix, but then copy the 4x4) */
	Mat3MulMat3(tmat, rmat, smat);
	Mat4CpyMat3(chan->chan_mat, tmat);
	
	/* prevent action channels breaking chains */
	/* need to check for bone here, CONSTRAINT_TYPE_ACTION uses this call */
	if ((chan->bone==NULL) || !(chan->bone->flag & BONE_CONNECTED)) {
		VECCOPY(chan->chan_mat[3], chan->loc);
	}
}

/* NLA strip modifiers */
static void do_strip_modifiers(Scene *scene, Object *armob, Bone *bone, bPoseChannel *pchan)
{
	bActionModifier *amod;
	bActionStrip *strip, *strip2;
	float scene_cfra= (float)scene->r.cfra;
	int do_modif;

	for (strip=armob->nlastrips.first; strip; strip=strip->next) {
		do_modif=0;
		
		if (scene_cfra>=strip->start && scene_cfra<=strip->end)
			do_modif=1;
		
		if ((scene_cfra > strip->end) && (strip->flag & ACTSTRIP_HOLDLASTFRAME)) {
			do_modif=1;
			
			/* if there are any other strips active, ignore modifiers for this strip - 
			 * 'hold' option should only hold action modifiers if there are 
			 * no other active strips */
			for (strip2=strip->next; strip2; strip2=strip2->next) {
				if (strip2 == strip) continue;
				
				if (scene_cfra>=strip2->start && scene_cfra<=strip2->end) {
					if (!(strip2->flag & ACTSTRIP_MUTE))
						do_modif=0;
				}
			}
			
			/* if there are any later, activated, strips with 'hold' set, they take precedence, 
			 * so ignore modifiers for this strip */
			for (strip2=strip->next; strip2; strip2=strip2->next) {
				if (scene_cfra < strip2->start) continue;
				if ((strip2->flag & ACTSTRIP_HOLDLASTFRAME) && !(strip2->flag & ACTSTRIP_MUTE)) {
					do_modif=0;
				}
			}
		}
		
		if (do_modif) {
			/* temporal solution to prevent 2 strips accumulating */
			if(scene_cfra==strip->end && strip->next && strip->next->start==scene_cfra)
				continue;
			
			for(amod= strip->modifiers.first; amod; amod= amod->next) {
				switch (amod->type) {
				case ACTSTRIP_MOD_DEFORM:
				{
					/* validate first */
					if(amod->ob && amod->ob->type==OB_CURVE && amod->channel[0]) {
						
						if( strcmp(pchan->name, amod->channel)==0 ) {
							float mat4[4][4], mat3[3][3];
							
							curve_deform_vector(scene, amod->ob, armob, bone->arm_mat[3], pchan->pose_mat[3], mat3, amod->no_rot_axis);
							Mat4CpyMat4(mat4, pchan->pose_mat);
							Mat4MulMat34(pchan->pose_mat, mat3, mat4);
							
						}
					}
				}
					break;
				case ACTSTRIP_MOD_NOISE:	
				{
					if( strcmp(pchan->name, amod->channel)==0 ) {
						float nor[3], loc[3], ofs;
						float eul[3], size[3], eulo[3], sizeo[3];
						
						/* calculate turbulance */
						ofs = amod->turbul / 200.0f;
						
						/* make a copy of starting conditions */
						VECCOPY(loc, pchan->pose_mat[3]);
						Mat4ToEul(pchan->pose_mat, eul);
						Mat4ToSize(pchan->pose_mat, size);
						VECCOPY(eulo, eul);
						VECCOPY(sizeo, size);
						
						/* apply noise to each set of channels */
						if (amod->channels & 4) {
							/* for scaling */
							nor[0] = BLI_gNoise(amod->noisesize, size[0]+ofs, size[1], size[2], 0, 0) - ofs;
							nor[1] = BLI_gNoise(amod->noisesize, size[0], size[1]+ofs, size[2], 0, 0) - ofs;	
							nor[2] = BLI_gNoise(amod->noisesize, size[0], size[1], size[2]+ofs, 0, 0) - ofs;
							VecAddf(size, size, nor);
							
							if (sizeo[0] != 0)
								VecMulf(pchan->pose_mat[0], size[0] / sizeo[0]);
							if (sizeo[1] != 0)
								VecMulf(pchan->pose_mat[1], size[1] / sizeo[1]);
							if (sizeo[2] != 0)
								VecMulf(pchan->pose_mat[2], size[2] / sizeo[2]);
						}
						if (amod->channels & 2) {
							/* for rotation */
							nor[0] = BLI_gNoise(amod->noisesize, eul[0]+ofs, eul[1], eul[2], 0, 0) - ofs;
							nor[1] = BLI_gNoise(amod->noisesize, eul[0], eul[1]+ofs, eul[2], 0, 0) - ofs;	
							nor[2] = BLI_gNoise(amod->noisesize, eul[0], eul[1], eul[2]+ofs, 0, 0) - ofs;
							
							compatible_eul(nor, eulo);
							VecAddf(eul, eul, nor);
							compatible_eul(eul, eulo);
							
							LocEulSizeToMat4(pchan->pose_mat, loc, eul, size);
						}
						if (amod->channels & 1) {
							/* for location */
							nor[0] = BLI_gNoise(amod->noisesize, loc[0]+ofs, loc[1], loc[2], 0, 0) - ofs;
							nor[1] = BLI_gNoise(amod->noisesize, loc[0], loc[1]+ofs, loc[2], 0, 0) - ofs;	
							nor[2] = BLI_gNoise(amod->noisesize, loc[0], loc[1], loc[2]+ofs, 0, 0) - ofs;
							
							VecAddf(pchan->pose_mat[3], loc, nor);
						}
					}
				}
					break;
				}
			}
		}
	}
}


/* The main armature solver, does all constraints excluding IK */
/* pchan is validated, as having bone and parent pointer */
void where_is_pose_bone(Scene *scene, Object *ob, bPoseChannel *pchan, float ctime)
{
	Bone *bone, *parbone;
	bPoseChannel *parchan;
	float vec[3];
	
	/* set up variables for quicker access below */
	bone= pchan->bone;
	parbone= bone->parent;
	parchan= pchan->parent;
	
	/* this gives a chan_mat with actions (ipos) results */
	chan_calc_mat(pchan);
	
	/* construct the posemat based on PoseChannels, that we do before applying constraints */
	/* pose_mat(b)= pose_mat(b-1) * yoffs(b-1) * d_root(b) * bone_mat(b) * chan_mat(b) */
	
	if(parchan) {
		float offs_bone[4][4];  // yoffs(b-1) + root(b) + bonemat(b)
		
		/* bone transform itself */
		Mat4CpyMat3(offs_bone, bone->bone_mat);
		
		/* The bone's root offset (is in the parent's coordinate system) */
		VECCOPY(offs_bone[3], bone->head);
		
		/* Get the length translation of parent (length along y axis) */
		offs_bone[3][1]+= parbone->length;
		
		/* Compose the matrix for this bone  */
		if(bone->flag & BONE_HINGE) {	// uses restposition rotation, but actual position
			float tmat[4][4];
			
			/* the rotation of the parent restposition */
			Mat4CpyMat4(tmat, parbone->arm_mat);
			
			/* the location of actual parent transform */
			VECCOPY(tmat[3], offs_bone[3]);
			offs_bone[3][0]= offs_bone[3][1]= offs_bone[3][2]= 0.0f;
			Mat4MulVecfl(parchan->pose_mat, tmat[3]);
			
			Mat4MulSerie(pchan->pose_mat, tmat, offs_bone, pchan->chan_mat, NULL, NULL, NULL, NULL, NULL);
		}
		else if(bone->flag & BONE_NO_SCALE) {
			float orthmat[4][4];
			
			/* get the official transform, but we only use the vector from it (optimize...) */
			Mat4MulSerie(pchan->pose_mat, parchan->pose_mat, offs_bone, pchan->chan_mat, NULL, NULL, NULL, NULL, NULL);
			VECCOPY(vec, pchan->pose_mat[3]);
			
			/* do this again, but with an ortho-parent matrix */
			Mat4CpyMat4(orthmat, parchan->pose_mat);
			Mat4Ortho(orthmat);
			Mat4MulSerie(pchan->pose_mat, orthmat, offs_bone, pchan->chan_mat, NULL, NULL, NULL, NULL, NULL);
			
			/* copy correct transform */
			VECCOPY(pchan->pose_mat[3], vec);
		}
		else 
			Mat4MulSerie(pchan->pose_mat, parchan->pose_mat, offs_bone, pchan->chan_mat, NULL, NULL, NULL, NULL, NULL);
	}
	else {
		Mat4MulMat4(pchan->pose_mat, pchan->chan_mat, bone->arm_mat);
		
		/* only rootbones get the cyclic offset (unless user doesn't want that) */
		if ((bone->flag & BONE_NO_CYCLICOFFSET) == 0)
			VecAddf(pchan->pose_mat[3], pchan->pose_mat[3], ob->pose->cyclic_offset);
	}
	
	/* do NLA strip modifiers - i.e. curve follow */
	do_strip_modifiers(scene, ob, bone, pchan);
	
	/* Do constraints */
	if (pchan->constraints.first) {
		bConstraintOb *cob;
		
		/* make a copy of location of PoseChannel for later */
		VECCOPY(vec, pchan->pose_mat[3]);
		
		/* prepare PoseChannel for Constraint solving 
		 * - makes a copy of matrix, and creates temporary struct to use 
		 */
		cob= constraints_make_evalob(scene, ob, pchan, CONSTRAINT_OBTYPE_BONE);
		
		/* Solve PoseChannel's Constraints */
		solve_constraints(&pchan->constraints, cob, ctime);	// ctime doesnt alter objects
		
		/* cleanup after Constraint Solving 
		 * - applies matrix back to pchan, and frees temporary struct used
		 */
		constraints_clear_evalob(cob);
		
		/* prevent constraints breaking a chain */
		if(pchan->bone->flag & BONE_CONNECTED) {
			VECCOPY(pchan->pose_mat[3], vec);
		}
	}
	
	/* calculate head */
	VECCOPY(pchan->pose_head, pchan->pose_mat[3]);
	/* calculate tail */
	VECCOPY(vec, pchan->pose_mat[1]);
	VecMulf(vec, bone->length);
	VecAddf(pchan->pose_tail, pchan->pose_head, vec);
}

/* This only reads anim data from channels, and writes to channels */
/* This is the only function adding poses */
void where_is_pose (Scene *scene, Object *ob)
{
	bArmature *arm;
	Bone *bone;
	bPoseChannel *pchan;
	float imat[4][4];
	float ctime;
	
	if(ob->type!=OB_ARMATURE) return;
	arm = ob->data;
	
	if(ELEM(NULL, arm, scene)) return;
	if((ob->pose==NULL) || (ob->pose->flag & POSE_RECALC)) 
	   armature_rebuild_pose(ob, arm);
	   
	ctime= bsystem_time(scene, ob, (float)scene->r.cfra, 0.0);	/* not accurate... */
	
	/* In editmode or restposition we read the data from the bones */
	if(arm->edbo || (arm->flag & ARM_RESTPOS)) {
		
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			bone= pchan->bone;
			if(bone) {
				Mat4CpyMat4(pchan->pose_mat, bone->arm_mat);
				VECCOPY(pchan->pose_head, bone->arm_head);
				VECCOPY(pchan->pose_tail, bone->arm_tail);
			}
		}
	}
	else {
		Mat4Invert(ob->imat, ob->obmat);	// imat is needed 

		/* 1. clear flags */
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			pchan->flag &= ~(POSE_DONE|POSE_CHAIN|POSE_IKTREE);
		}
		/* 2. construct the IK tree */
		BIK_initialize_tree(scene, ob, ctime);

		/* 3. the main loop, channels are already hierarchical sorted from root to children */
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			
			/* 4. if we find an IK root, we handle it separated */
			if(pchan->flag & POSE_IKTREE) {
				BIK_execute_tree(scene, ob, pchan, ctime);
			}
			/* 5. otherwise just call the normal solver */
			else if(!(pchan->flag & POSE_DONE)) {
				where_is_pose_bone(scene, ob, pchan, ctime);
			}
		}
		/* 6. release the IK tree */
		BIK_release_tree(scene, ob, ctime);
	}
		
	/* calculating deform matrices */
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone) {
			Mat4Invert(imat, pchan->bone->arm_mat);
			Mat4MulMat4(pchan->chan_mat, imat, pchan->pose_mat);
		}
	}
}
