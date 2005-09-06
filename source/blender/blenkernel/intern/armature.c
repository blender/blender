/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
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
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <ctype.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <stdio.h>
#include "MEM_guardedalloc.h"

#include "nla.h"

#include "BLI_arithb.h"
#include "BLI_blenlib.h"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_armature_types.h"
#include "DNA_action_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_constraint_types.h"

#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_library.h"
#include "BKE_blender.h"
#include "BKE_armature.h"
#include "BKE_action.h"
#include "BKE_constraint.h"
#include "BKE_object.h"
#include "BKE_object.h"
#include "BKE_deform.h"
#include "BKE_utildefines.h"

#include "BIF_editdeform.h"

#include "IK_solver.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

/*	**************** Generic Functions, data level *************** */

bArmature *get_armature(Object *ob)
{
	if(ob==NULL) return NULL;
	if(ob->type==OB_ARMATURE) return ob->data;
	else return NULL;
}

bArmature *add_armature()
{
	bArmature *arm;
	
	arm= alloc_libblock (&G.main->armature, ID_AR, "Armature");
	arm->deformflag = ARM_DEF_VGROUP|ARM_DEF_ENVELOPE;
	return arm;
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
		/*		unlink_armature(arm);*/
		free_bones(arm);
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
	duplicatelist (&newBone->childbase, &oldBone->childbase);
	
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
	duplicatelist(&newArm->bonebase, &arm->bonebase);
	
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

static char *strcasestr_(register char *s, register char *find)
{
    register char c, sc;
    register size_t len;
	
    if ((c = *find++) != 0) {
		c= tolower(c);
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
				sc= tolower(sc);
			} while (sc != c);
		} while (BLI_strncasecmp(s, find, len) != 0);
		s--;
    }
    return ((char *) s);
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
		index = strcasestr_(prefix, "right");
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
			index = strcasestr_(prefix, "left");
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
Mat4 *b_bone_spline_setup(bPoseChannel *pchan)
{
	static Mat4 bbone_array[MAX_BBONE_SUBDIV];
	bPoseChannel *next, *prev;
	Bone *bone= pchan->bone;
	float h1[3], h2[3], length, hlength1, hlength2, roll;
	float mat3[3][3], imat[4][4];
	float data[MAX_BBONE_SUBDIV+1][4], *fp;
	int a;
	
	length= bone->length;
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
	
	Mat4Invert(imat, pchan->pose_mat);
	
	if(prev) {
		/* transform previous point inside this bone space */
		VECCOPY(h1, prev->pose_head);
		Mat4MulVecfl(imat, h1);
		/* if previous bone is B-bone too, use average handle direction */
		if(prev->bone->segments>1) h1[1]-= length;
		Normalise(h1);
		VecMulf(h1, -hlength1);
	}
	else {
		h1[0]= 0.0f; h1[1]= hlength1; h1[2]= 0.0f;
	}
	if(next) {
		float difmat[4][4], result[3][3], imat3[3][3];
		
		/* transform next point inside this bone space */
		VECCOPY(h2, next->pose_tail);
		Mat4MulVecfl(imat, h2);
		/* if next bone is B-bone too, use average handle direction */
		if(next->bone->segments>1);
		else h2[1]-= length;
		
		/* find the next roll to interpolate as well */
		Mat4MulMat4(difmat, next->pose_mat, imat);
		Mat3CpyMat4(result, difmat);				// the desired rotation at beginning of next bone
		
		vec_roll_to_mat3(h2, 0.0f, mat3);			// the result of vec_roll without roll
		
		Mat3Inv(imat3, mat3);
		Mat3MulMat3(mat3, imat3, result);			// the matrix transforming vec_roll to desired roll
		
		roll= atan2(mat3[2][0], mat3[2][2]);
		
		/* and only now negate handle */
		Normalise(h2);
		VecMulf(h2, -hlength2);
		
	}
	else {
		h2[0]= 0.0f; h2[1]= -hlength2; h2[2]= 0.0f;
		roll= 0.0;
	}
	
	/* make curve */
	if(bone->segments > MAX_BBONE_SUBDIV)
		bone->segments= MAX_BBONE_SUBDIV;
	
	forward_diff_bezier(0.0, h1[0],		h2[0],			0.0,		data[0],	MAX_BBONE_SUBDIV, 4);
	forward_diff_bezier(0.0, h1[1],		length + h2[1],	length,		data[0]+1,	MAX_BBONE_SUBDIV, 4);
	forward_diff_bezier(0.0, h1[2],		h2[2],			0.0,		data[0]+2,	MAX_BBONE_SUBDIV, 4);
	forward_diff_bezier(0.0, 0.390464f*roll, (1.0f-0.390464f)*roll,	roll,	data[0]+3,	MAX_BBONE_SUBDIV, 4);
	
	equalize_bezier(data[0], bone->segments);	// note: does stride 4!
	
	/* make transformation matrices for the segments for drawing */
	for(a=0, fp= data[0]; a<bone->segments; a++, fp+=4) {
		VecSubf(h1, fp+4, fp);
		vec_roll_to_mat3(h1, fp[3], mat3);		// fp[3] is roll
		Mat4CpyMat3(bbone_array[a].mat, mat3);
		VECCOPY(bbone_array[a].mat[3], fp);
	}
	
	return bbone_array;
}

/* ************ Armature Deform ******************* */

static void pchan_b_bone_defmats(bPoseChannel *pchan)
{
	Bone *bone= pchan->bone;
	Mat4 *b_bone= b_bone_spline_setup(pchan);
	Mat4 *b_bone_mats;
	int a;
	
	pchan->b_bone_mats=b_bone_mats= MEM_mallocN((1+bone->segments)*sizeof(Mat4), "BBone defmats");
	
	/* first matrix is the inverse arm_mat, to bring points in local bone space */
	Mat4Invert(b_bone_mats[0].mat, bone->arm_mat);
	
	/* then we multiply the bbone_mats with arm_mat */
	for(a=0; a<bone->segments; a++) {
		Mat4MulMat4(b_bone_mats[a+1].mat, b_bone[a].mat, bone->arm_mat);
	}
}

static void b_bone_deform(bPoseChannel *pchan, Bone *bone, float *defpos)
{
	Mat4 *b_bone= pchan->b_bone_mats;
	float segment;
	int a;
	
	/* need to transform defpos back to bonespace */
	Mat4MulVecfl(b_bone[0].mat, defpos);
	
	/* now calculate which of the b_bones are deforming this */
	segment= bone->length/((float)bone->segments);
	a= (int) (defpos[1]/segment);
	
	/* note; by clamping it extends deform at endpoints, goes best with straight joints in restpos. */
	CLAMP(a, 0, bone->segments-1);

	/* since the bbone mats translate from (0.0.0) on the curve, we subtract */
	defpos[1] -= ((float)a)*segment;
	
	Mat4MulVecfl(b_bone[a+1].mat, defpos);
}

/* using vec with dist to bone b1 - b2 */
float distfactor_to_bone (float vec[3], float b1[3], float b2[3], float rad1, float rad2, float rdist)
{
	float dist=0.0f; 
	float bdelta[3];
	float pdelta[3];
	float hsqr, a, l, rad;
	
	VecSubf (bdelta, b2, b1);
	l = Normalise (bdelta);
	
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
			rad= rad*rad2 + (1.0-rad)*rad1;
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
			a= sqrt(dist)-rad;
			return 1.0-( a*a )/( rdist*rdist );
		}
	}
}

static float dist_bone_deform(bPoseChannel *pchan, float *vec, float *co)
{
	Bone *bone= pchan->bone;
	float	fac;
	float	cop[3];
	float	contrib=0.0;

	if(bone==NULL) return 0.0f;
	
	VECCOPY (cop, co);

	fac= distfactor_to_bone(cop, bone->arm_head, bone->arm_tail, bone->rad_head, bone->rad_tail, bone->dist);
	
	if (fac>0.0){
		
		fac*=bone->weight;
		contrib= fac;
		if(contrib>0.0) {

			VECCOPY (cop, co);
			
			if(bone->segments>1)
				b_bone_deform(pchan, bone, cop);	// applies on cop
			
			Mat4MulVecfl(pchan->chan_mat, cop);
			
			VecSubf (cop, cop, co);	//	Make this a delta from the base position
			cop[0]*=fac; cop[1]*=fac; cop[2]*=fac;
			VecAddf (vec, vec, cop);
		}
	}
	
	return contrib;
}

static void pchan_bone_deform(bPoseChannel *pchan, float weight, float *vec, float *co, float *contrib)
{
	float	cop[3];

	if (!weight)
		return;

	VECCOPY (cop, co);
	
	if(pchan->bone->segments>1)
		b_bone_deform(pchan, pchan->bone, cop);	// applies on cop
	
	Mat4MulVecfl(pchan->chan_mat, cop);
	
	vec[0]+=(cop[0]-co[0])*weight;
	vec[1]+=(cop[1]-co[1])*weight;
	vec[2]+=(cop[2]-co[2])*weight;

	(*contrib)+=weight;
}

void armature_deform_verts(Object *armOb, Object *target, float (*vertexCos)[3], int numVerts) 
{
	bArmature *arm= armOb->data;
	bPoseChannel *pchan, **defnrToPC = NULL;
	MDeformVert *dverts= NULL;
	float obinv[4][4], premat[4][4], postmat[4][4];
	int use_envelope= arm->deformflag & ARM_DEF_ENVELOPE;
	int i;

	Mat4Invert(obinv, target->obmat);
	Mat4CpyMat4(premat, target->obmat);
	Mat4MulMat4(postmat, armOb->obmat, obinv);

	Mat4Invert(premat, postmat);

	/* bone defmats are already in the channels, chan_mat */
	
	/* initialize B_bone matrices */
	for(pchan= armOb->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(!(pchan->bone->flag & BONE_NO_DEFORM))
			if(pchan->bone->segments>1)
				pchan_b_bone_defmats(pchan);
	}

	/* get a vertex-deform-index to posechannel array */
	if(arm->deformflag & ARM_DEF_VGROUP) {
		if (target->type==OB_MESH){
			int numGroups = BLI_countlist(&target->defbase);
			bDeformGroup *dg;
		
			dverts = ((Mesh*)target->data)->dvert;
			if(dverts) {
				defnrToPC = MEM_callocN(sizeof(*defnrToPC)*numGroups, "defnrToBone");
				for (i=0,dg=target->defbase.first; dg; i++,dg=dg->next) {
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

	for(i=0; i<numVerts; i++) {
		MDeformVert *dvert;
		float *co = vertexCos[i];
		float	vec[3];
		float	contrib=0.0;
		int		j;

		vec[0]=vec[1]=vec[2]=0;

		/* Apply the object's matrix */
		Mat4MulVecfl(premat, co);
		
		if(dverts)
			dvert= dverts+i;
		else
			dvert= NULL;
		
		if(dvert && dvert->totweight) {	// use weight groups

			for (j=0; j<dvert->totweight; j++){
				pchan = defnrToPC[dvert->dw[j].def_nr];
				if (pchan) {
					float weight= dvert->dw[j].weight;
					if(pchan->bone->flag & BONE_MULT_VG_ENV) {
						Bone *bone= pchan->bone;
						weight*= distfactor_to_bone(co, bone->arm_head, bone->arm_tail, bone->rad_head, bone->rad_tail, bone->dist);
					}
					pchan_bone_deform(pchan, weight, vec, co, &contrib);
				}
			}
		}
		else if(use_envelope) {
			for(pchan= armOb->pose->chanbase.first; pchan; pchan= pchan->next) {
				if(!(pchan->bone->flag & BONE_NO_DEFORM))
					contrib+= dist_bone_deform(pchan, vec, co);
			}
		}

		if (contrib>0.0){
			vec[0]/=contrib;
			vec[1]/=contrib;
			vec[2]/=contrib;
		}

		VecAddf(co, vec, co);
		Mat4MulVecfl(postmat, co);
	}

	if (defnrToPC) MEM_freeN(defnrToPC);
	
	/* free B_bone matrices */
	for(pchan= armOb->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->b_bone_mats) {
			MEM_freeN(pchan->b_bone_mats);
			pchan->b_bone_mats= NULL;
		}
	}
	
}

/* ************ END Armature Deform ******************* */

void get_objectspace_bone_matrix (struct Bone* bone, float M_accumulatedMatrix[][4], int root, int posed)
{
	Mat4CpyMat4(M_accumulatedMatrix, bone->arm_mat);
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


/*	Calculates the rest matrix of a bone based
	On its vector and a roll around that vector */
void vec_roll_to_mat3(float *vec, float roll, float mat[][3])
{
	float	nor[3], axis[3], target[3]={0,1,0};
	float	theta;
	float	rMatrix[3][3], bMatrix[3][3];
	
	VECCOPY (nor, vec);
	Normalise (nor);
	
	/*	Find Axis & Amount for bone matrix*/
	Crossf (axis,target,nor);
	
	if (Inpf(axis,axis) > 0.0000000000001) {
		/* if nor is *not* a multiple of target ... */
		Normalise (axis);
		theta=(float) acos (Inpf (target,nor));
		
		/*	Make Bone matrix*/
		VecRotToMat3(axis, theta, bMatrix);
	}
	else {
		/* if nor is a multiple of target ... */
		float updown;
		
		/* point same direction, or opposite? */
		updown = ( Inpf (target,nor) > 0 ) ? 1.0 : -1.0;
		
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

/* only after leave editmode, but also for validating older files */
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
	/* sort channels on dependency order, so we can walk the channel list */

	/* and a check for garbage */
	for(pchan= pose->chanbase.first; pchan; pchan= next) {
		next= pchan->next;
		if(pchan->bone==NULL) {
			BLI_freelinkN(&pose->chanbase, pchan);  // constraints?
		}
	}
	//printf("rebuild pose, %d bones\n", counter);
	if(counter<2) return;
	
	update_pose_constraint_flags(ob->pose); // for IK detection for example
	
	/* the sorting */
	DAG_pose_sort(ob);
	
	ob->pose->flag &= ~POSE_RECALC;
}


/* ********************** THE IK SOLVER ******************* */



/* allocates PoseTree, and links that to root bone/channel */
/* note; if we got this working, it can become static too? */
static void initialize_posetree(struct Object *ob, bPoseChannel *pchan_tip)
{
	bPoseChannel *curchan, *pchan_root=NULL, *chanlist[256], **oldchan;
	PoseTree *tree;
	PoseTarget *target;
	bConstraint *con;
	bKinematicConstraint *data;
	int a, segcount= 0, size, newsize, *oldparent, parent;
	
	/* find IK constraint, and validate it */
	for(con= pchan_tip->constraints.first; con; con= con->next) {
		if(con->type==CONSTRAINT_TYPE_KINEMATIC) break;
	}
	if(con==NULL) return;
	if(con->flag & CONSTRAINT_DISABLE) return;  // not sure...
	
	data=(bKinematicConstraint*)con->data;
	if(data->tar==NULL) return;
	if(data->tar->type==OB_ARMATURE && data->subtarget[0]==0) return;
	
	/* exclude tip from chain? */
	if(!(data->flag & CONSTRAINT_IK_TIP))
		pchan_tip= pchan_tip->parent;
	
	/* Find the chain's root & count the segments needed */
	for (curchan = pchan_tip; curchan; curchan=curchan->parent){
		pchan_root = curchan;
		
		curchan->flag |= POSE_CHAIN;	// don't forget to clear this
		chanlist[segcount]=curchan;
		segcount++;
		
		if(segcount==data->rootbone || segcount>255) break; // 255 is weak
	}
	if (!segcount) return;

	/* setup the chain data */
	
	/* we make tree-IK, unless all existing targets are in this chain */
	for(tree= pchan_root->iktree.first; tree; tree= tree->next) {
		for(target= tree->targets.first; target; target= target->next) {
			curchan= tree->pchan[target->tip];
			if(curchan->flag & POSE_CHAIN)
				curchan->flag &= ~POSE_CHAIN;
			else
				break;
		}
		if(target) break;
	}

	/* create a target */
	target= MEM_callocN(sizeof(PoseTarget), "posetarget");
	target->con= con;
	pchan_tip->flag &= ~POSE_CHAIN;

	if(tree==NULL) {
		/* make new tree */
		tree= MEM_callocN(sizeof(PoseTree), "posetree");

		tree->tolerance= data->tolerance;
		tree->iterations= data->iterations;
		tree->totchannel= segcount;
		
		tree->pchan= MEM_callocN(segcount*sizeof(void*), "ik tree pchan");
		tree->parent= MEM_callocN(segcount*sizeof(int), "ik tree parent");
		for(a=0; a<segcount; a++) {
			tree->pchan[a]= chanlist[segcount-a-1];
			tree->parent[a]= a-1;
		}
		target->tip= segcount-1;
		
		/* AND! link the tree to the root */
		BLI_addtail(&pchan_root->iktree, tree);
	}
	else {
		tree->tolerance= MIN2(tree->tolerance, data->tolerance);
		tree->iterations= MAX2(data->iterations, tree->iterations);

		/* skip common pose channels and add remaining*/
		size= MIN2(segcount, tree->totchannel);
		for(a=0; a<size && tree->pchan[a]==chanlist[segcount-a-1]; a++);
		parent= a-1;

		segcount= segcount-a;
		target->tip= tree->totchannel + segcount - 1;

		if (segcount > 0) {
			/* resize array */
			newsize= tree->totchannel + segcount;
			oldchan= tree->pchan;
			oldparent= tree->parent;

			tree->pchan= MEM_callocN(newsize*sizeof(void*), "ik tree pchan");
			tree->parent= MEM_callocN(newsize*sizeof(int), "ik tree parent");
			memcpy(tree->pchan, oldchan, sizeof(void*)*tree->totchannel);
			memcpy(tree->parent, oldparent, sizeof(int)*tree->totchannel);
			MEM_freeN(oldchan);
			MEM_freeN(oldparent);

			/* add new pose channels at the end, in reverse order */
			for(a=0; a<segcount; a++) {
				tree->pchan[tree->totchannel+a]= chanlist[segcount-a-1];
				tree->parent[tree->totchannel+a]= tree->totchannel+a-1;
			}
			tree->parent[tree->totchannel]= parent;
			
			tree->totchannel= newsize;
		}

		/* move tree to end of list, for correct evaluation order */
		BLI_remlink(&pchan_root->iktree, tree);
		BLI_addtail(&pchan_root->iktree, tree);
	}

	/* add target to the tree */
	BLI_addtail(&tree->targets, target);
}

/* called from within the core where_is_pose loop, all animsystems and constraints
were executed & assigned. Now as last we do an IK pass */
static void execute_posetree(Object *ob, PoseTree *tree)
{
	float R_parmat[3][3];
	float iR_parmat[3][3];
	float R_bonemat[3][3];
	float goalrot[3][3], goalpos[3];
	float rootmat[4][4], imat[4][4];
	float goal[4][4], goalinv[4][4];
	float size[3], irest_basis[3][3], full_basis[3][3];
	float length, basis[3][3], rest_basis[3][3], start[3], *ikstretch=NULL;
	int a, flag, hasstretch=0;
	bPoseChannel *pchan;
	IK_Segment *seg, *parent, **iktree, *iktarget;
	IK_Solver *solver;
	PoseTarget *target;
	bKinematicConstraint *data;
	Bone *bone;

	if (tree->totchannel == 0)
		return;

	iktree= MEM_mallocN(sizeof(void*)*tree->totchannel, "ik tree");

	for(a=0; a<tree->totchannel; a++) {
		pchan= tree->pchan[a];
		bone= pchan->bone;

		/* set DoF flag */
		flag= 0;
		if((pchan->ikflag & BONE_IK_NO_XDOF) == 0)
			flag |= IK_XDOF;
		if((pchan->ikflag & BONE_IK_NO_YDOF) == 0)
			flag |= IK_YDOF;
		if((pchan->ikflag & BONE_IK_NO_ZDOF) == 0)
			flag |= IK_ZDOF;

		if(pchan->ikstretch > 0.0) {
			flag |= IK_TRANS_YDOF;
			hasstretch = 1;
		}

		seg= iktree[a]= IK_CreateSegment(flag);

		/* find parent */
		if(a == 0)
			parent= NULL;
		else
			parent= iktree[tree->parent[a]];

		IK_SetParent(seg, parent);
	
		/* get the matrix that transforms from prevbone into this bone */
		Mat3CpyMat4(R_bonemat, pchan->pose_mat);

		/* gather transformations for this IK segment */

		if(a>0 && pchan->parent) {
			Mat3CpyMat4(R_parmat, pchan->parent->pose_mat);
			VECCOPY(start, bone->head); /* bone offset */
		}
		else {
			Mat3One(R_parmat);
			start[0]= start[1]= start[2]= 0.0f;
		}
		
		/* change length based on bone size */
		length= bone->length*VecLength(R_bonemat[1]);

		/* compute rest basis and its inverse */
		Mat3CpyMat3(rest_basis, bone->bone_mat);
		Mat3CpyMat3(irest_basis, bone->bone_mat);
		Mat3Transp(irest_basis);

		/* compute basis with rest_basis removed */
		Mat3Inv(iR_parmat, R_parmat);
		Mat3MulMat3(full_basis, iR_parmat, R_bonemat);
		Mat3MulMat3(basis, irest_basis, full_basis);

		/* basis must be pure rotation */
		Mat3Ortho(basis);

		IK_SetTransform(seg, start, rest_basis, basis, length);

		if (pchan->ikflag & BONE_IK_XLIMIT)
			IK_SetLimit(seg, IK_X, pchan->limitmin[0], pchan->limitmax[0]);
		if (pchan->ikflag & BONE_IK_YLIMIT)
			IK_SetLimit(seg, IK_Y, pchan->limitmin[1], pchan->limitmax[1]);
		if (pchan->ikflag & BONE_IK_ZLIMIT)
			IK_SetLimit(seg, IK_Z, pchan->limitmin[2], pchan->limitmax[2]);

		IK_SetStiffness(seg, IK_X, pchan->stiffness[0]);
		IK_SetStiffness(seg, IK_Y, pchan->stiffness[1]);
		IK_SetStiffness(seg, IK_Z, pchan->stiffness[2]);

		if(pchan->ikstretch > 0.0) {
			float ikstretch = pchan->ikstretch*pchan->ikstretch;
			IK_SetStiffness(seg, IK_TRANS_Y, MIN2(1.0-ikstretch, 0.99));
			IK_SetLimit(seg, IK_TRANS_Y, 0.001, 1e10);
		}
	}

	solver= IK_CreateSolver(iktree[0]);

	/* set solver goals */

	/* first set the goal inverse transform, assuming the root of tree was done ok! */
	pchan= tree->pchan[0];
	Mat4One(rootmat);
	VECCOPY(rootmat[3], pchan->pose_head);
	
	Mat4MulMat4 (imat, rootmat, ob->obmat);
	Mat4Invert (goalinv, imat);
	
	for(target=tree->targets.first; target; target=target->next) {
		data= (bKinematicConstraint*)target->con->data;

		/* 1.0=ctime */
		get_constraint_target_matrix(target->con, TARGET_BONE, NULL, rootmat, size, 1.0);

		/* and set and transform goal */
		Mat4MulMat4(goal, rootmat, goalinv);

		VECCOPY(goalpos, goal[3]);
		Mat3CpyMat4(goalrot, goal);

		/* do we need blending? */
		if(target->con->enforce!=1.0) {
			float vec[3], q1[4], q2[4], q[4];
			float fac= target->con->enforce;
			float mfac= 1.0-fac;
			
			pchan= tree->pchan[target->tip];

			/* blend position */
			VECCOPY(vec, pchan->pose_tail);
			Mat4MulVecfl(ob->obmat, vec); // world space
			Mat4MulVecfl(goalinv, vec);
			goalpos[0]= fac*goalpos[0] + mfac*vec[0];
			goalpos[1]= fac*goalpos[1] + mfac*vec[1];
			goalpos[2]= fac*goalpos[2] + mfac*vec[2];

			/* blend rotation */
			Mat3ToQuat(goalrot, q1);
			Mat3CpyMat4(R_bonemat, pchan->pose_mat);
			Mat3ToQuat(R_bonemat, q2);
			QuatInterpol(q, q1, q2, mfac);
			QuatToMat3(q, goalrot);
		}

		iktarget= iktree[target->tip];

		if(data->weight != 0.0)
			IK_SolverAddGoal(solver, iktarget, goalpos, data->weight);
		if((data->flag & CONSTRAINT_IK_ROT) && (data->orientweight != 0.0))
			IK_SolverAddGoalOrientation(solver, iktarget, goalrot, data->orientweight);
	}

	/* solve */
	IK_Solve(solver, tree->tolerance, tree->iterations);
	IK_FreeSolver(solver);

	/* gather basis changes */
	tree->basis_change= MEM_mallocN(sizeof(float[3][3])*tree->totchannel, "ik basis change");
	if(hasstretch)
		ikstretch= MEM_mallocN(sizeof(float)*tree->totchannel, "ik stretch");

	for(a=0; a<tree->totchannel; a++) {
		IK_GetBasisChange(iktree[a], tree->basis_change[a]);

		if(hasstretch) {
			/* have to compensate for scaling received from parent */
			float parentstretch, stretch;

			pchan= tree->pchan[a];
			parentstretch= (tree->parent[a] >= 0)? ikstretch[tree->parent[a]]: 1.0;

			if(pchan->ikstretch > 0.0) {
				float trans[3], length;

				IK_GetTranslationChange(iktree[a], trans);
				length= pchan->bone->length*VecLength(pchan->pose_mat[1]);

				ikstretch[a]= (length == 0.0)? 1.0: (trans[1]+length)/length;
			}
			else
				ikstretch[a] = 1.0;

			stretch= (parentstretch == 0.0)? 1.0: ikstretch[a]/parentstretch;

			VecMulf(tree->basis_change[a][0], stretch);
			VecMulf(tree->basis_change[a][1], stretch);
			VecMulf(tree->basis_change[a][2], stretch);

		}

		IK_FreeSegment(iktree[a]);
	}
	
	MEM_freeN(iktree);
	if(ikstretch) MEM_freeN(ikstretch);
}

void free_posetree(PoseTree *tree)
{
	BLI_freelistN(&tree->targets);
	if(tree->pchan) MEM_freeN(tree->pchan);
	if(tree->parent) MEM_freeN(tree->parent);
	if(tree->basis_change) MEM_freeN(tree->basis_change);
	MEM_freeN(tree);
}

/* ********************** THE POSE SOLVER ******************* */


/* loc/rot/size to mat4 */
/* used in constraint.c too */
void chan_calc_mat(bPoseChannel *chan)
{
	float smat[3][3];
	float rmat[3][3];
	float tmat[3][3];
	
	SizeToMat3(chan->size, smat);
	
	NormalQuat(chan->quat);
	QuatToMat3(chan->quat, rmat);
	
	Mat3MulMat3(tmat, rmat, smat);
	
	Mat4CpyMat3(chan->chan_mat, tmat);
	
	/* prevent action channels breaking chains */
	/* need to check for bone here, CONSTRAINT_TYPE_ACTION uses this call */
	if (chan->bone==NULL || !(chan->bone->flag & BONE_CONNECTED)) {
		VECCOPY(chan->chan_mat[3], chan->loc);
	}

}

/* transform from bone(b) to bone(b+1), store in chan_mat */
static void make_dmats(bPoseChannel *pchan)
{
	if (pchan->parent) {
		float iR_parmat[4][4];
		Mat4Invert(iR_parmat, pchan->parent->pose_mat);
		Mat4MulMat4(pchan->chan_mat,  pchan->pose_mat, iR_parmat);	// delta mat
	}
	else Mat4CpyMat4(pchan->chan_mat, pchan->pose_mat);
}

/* applies IK matrix to pchan, IK is done separated */
/* formula: pose_mat(b) = pose_mat(b-1) * diffmat(b-1, b) * ik_mat(b) */
/* to make this work, the diffmats have to be precalculated! Stored in chan_mat */
static void where_is_ik_bone(bPoseChannel *pchan, float ik_mat[][3])   // nr = to detect if this is first bone
{
	float vec[3], ikmat[4][4];
	
	Mat4CpyMat3(ikmat, ik_mat);
	
	if (pchan->parent)
		Mat4MulSerie(pchan->pose_mat, pchan->parent->pose_mat, pchan->chan_mat, ikmat, NULL, NULL, NULL, NULL, NULL);
	else 
		Mat4MulMat4(pchan->pose_mat, ikmat, pchan->chan_mat);

	/* calculate head */
	VECCOPY(pchan->pose_head, pchan->pose_mat[3]);
	/* calculate tail */
	VECCOPY(vec, pchan->pose_mat[1]);
	VecMulf(vec, pchan->bone->length);
	VecAddf(pchan->pose_tail, pchan->pose_head, vec);

	pchan->flag |= POSE_DONE;
}

/* The main armature solver, does all constraints excluding IK */
/* pchan is validated, as having bone and parent pointer */
static void where_is_pose_bone(Object *ob, bPoseChannel *pchan)
{
	Bone *bone, *parbone;
	bPoseChannel *parchan;
	float vec[3], ctime= 1.0;   // ctime todo

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
		else 
			Mat4MulSerie(pchan->pose_mat, parchan->pose_mat, offs_bone, pchan->chan_mat, NULL, NULL, NULL, NULL, NULL);
	}
	else 
		Mat4MulMat4(pchan->pose_mat, pchan->chan_mat, bone->arm_mat);
	
	
	/* Do constraints */
	if(pchan->constraints.first) {
		static Object conOb;
		static int initialized= 0;
		
		VECCOPY(vec, pchan->pose_mat[3]);
		
		/* Build a workob to pass the bone to the constraint solver */
		if(initialized==0) {
			memset(&conOb, 0, sizeof(Object));
			initialized= 1;
		}
		conOb.size[0]= conOb.size[1]= conOb.size[2]= 1.0;
		conOb.data = ob->data;
		conOb.type = ob->type;
		conOb.parent = ob;	// ik solver retrieves the armature that way !?!?!?!
		conOb.pose= ob->pose;				// needed for retrieving pchan
		conOb.trackflag = ob->trackflag;
		conOb.upflag = ob->upflag;
		
		/* Collect the constraints from the pose (listbase copy) */
		conOb.constraints = pchan->constraints;
		
		/* conOb.obmat takes bone to worldspace */
		Mat4MulMat4 (conOb.obmat, pchan->pose_mat, ob->obmat);
		
		/* Solve */
		solve_constraints (&conOb, TARGET_BONE, (void*)pchan, ctime);	// ctime doesnt alter objects
		
		/* Take out of worldspace */
		Mat4MulMat4 (pchan->pose_mat, conOb.obmat, ob->imat);
		
		/* prevent constraints breaking a chain */
		if(pchan->bone->flag & BONE_CONNECTED)
			VECCOPY(pchan->pose_mat[3], vec);

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
void where_is_pose (Object *ob)
{
	bArmature *arm;
	Bone *bone;
	bPoseChannel *pchan;
	float imat[4][4];

	arm = get_armature(ob);
	
	if(arm==NULL) return;
	if(ob->pose==NULL || (ob->pose->flag & POSE_RECALC)) 
	   armature_rebuild_pose(ob, arm);
	
	/* In restposition we read the data from the bones */
	if(ob==G.obedit || (arm->flag & ARM_RESTPOS)) {
		
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

		/* 1. construct the PoseTrees, clear flags */
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			pchan->flag &= ~(POSE_DONE|POSE_CHAIN);
			if(pchan->constflag & PCHAN_HAS_IK) // flag is set on editing constraints
				initialize_posetree(ob, pchan);	// will attach it to root!
		}
		
		/* 2. the main loop, channels are already hierarchical sorted from root to children */
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			
			/* 3. if we find an IK root, we handle it separated */
			if(pchan->iktree.first) {
				while(pchan->iktree.first) {
					PoseTree *tree= pchan->iktree.first;
					int a;
					
					/* 4. walk over the tree for regular solving */
					for(a=0; a<tree->totchannel; a++) {
						if(!(tree->pchan[a]->flag & POSE_DONE))	// successive trees can set the flag
							where_is_pose_bone(ob, tree->pchan[a]);
					}
					/* 5. execute the IK solver */
					execute_posetree(ob, tree);
					
					/* 6. apply the differences to the channels, 
						  we need to calculate the original differences first */
					for(a=0; a<tree->totchannel; a++)
						make_dmats(tree->pchan[a]);
					
					for(a=0; a<tree->totchannel; a++)
						/* sets POSE_DONE */
						where_is_ik_bone(tree->pchan[a], tree->basis_change[a]);
					
					/* 7. and free */
					BLI_remlink(&pchan->iktree, tree);
					free_posetree(tree);
				}
			}
			else if(!(pchan->flag & POSE_DONE)) {
				where_is_pose_bone(ob, pchan);
			}
		}
	}
		
	/* calculating deform matrices */
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if(pchan->bone) {
			Mat4Invert(imat, pchan->bone->arm_mat);
			Mat4MulMat4(pchan->chan_mat, imat, pchan->pose_mat);
		}
	}
}

/* ****************** Game Blender functions, called by engine ************** */

/* NOTE: doesn't work at the moment!!! (ton) */

/* ugly Globals */
static float g_premat[4][4];
static float g_postmat[4][4];

void GB_build_mats (float parmat[][4], float obmat[][4], float premat[][4], float postmat[][4])
{
	float obinv[4][4];
	
	Mat4Invert(obinv, obmat);
	Mat4CpyMat4(premat, obmat);
	Mat4MulMat4(postmat, parmat, obinv);
	
	Mat4Invert (premat, postmat);
}

void GB_init_armature_deform(ListBase *defbase, float premat[][4], float postmat[][4])
{
//	g_defbase = defbase;
	Mat4CpyMat4 (g_premat, premat);
	Mat4CpyMat4 (g_postmat, postmat);
	
}

void GB_validate_defgroups (Mesh *mesh, ListBase *defbase)
{
	/* Should only be called when the mesh or armature changes */
//	int j, i;
//	MDeformVert *dvert;
	
//	for (j=0; j<mesh->totvert; j++){
//		dvert = mesh->dvert+j;
//		for (i=0; i<dvert->totweight; i++)
//			dvert->dw[i].data = ((bDeformGroup*)BLI_findlink (defbase, dvert->dw[i].def_nr))->data;
//	}
}

void GB_calc_armature_deform (float *co, MDeformVert *dvert)
{
	float vec[3]={0, 0, 0};
	float contrib = 0;
	int	i;
//	bPoseChannel *pchan;
	
	Mat4MulVecfl(g_premat, co);
	
	for (i=0; i<dvert->totweight; i++){
//		pchan = (bPoseChannel *)dvert->dw[i].data;
//		if (pchan) pchan_bone_deform(pchan, dvert->dw[i].weight, vec, co, &contrib);
	}
	
	if (contrib){
		vec[0]/=contrib;
		vec[1]/=contrib;
		vec[2]/=contrib;
	}
	
	VecAddf (co, vec, co);
	Mat4MulVecfl(g_postmat, co);
}

/* ****************** END Game Blender functions, called by engine ************** */

