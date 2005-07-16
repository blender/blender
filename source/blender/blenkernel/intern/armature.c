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

/* ugly Globals */
static float g_premat[4][4];
static float g_postmat[4][4];
static MDeformVert *g_dverts;
static ListBase		*g_defbase;
static Object *g_deform;

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

/* ************* B-Bone support ******************* */

#define MAX_BBONE_SUBDIV	32

/* returns pointer to static array, filled with desired amount of bone->segments elements */
/* this calculation is done within pchan pose_mat space */
Mat4 *b_bone_spline_setup(bPoseChannel *pchan)
{
	static Mat4 bbone_array[MAX_BBONE_SUBDIV];
	bPoseChannel *next, *prev;
	Bone *bone= pchan->bone;
	float h1[3], h2[3], length, hlength, roll;
	float mat3[3][3], imat[4][4];
	float data[MAX_BBONE_SUBDIV+1][4], *fp;
	int a;
	
	length= bone->length;
	hlength= length*0.390464f;		// 0.5*sqrt(2)*kappa, the handle length for near-perfect circles
	
	/* evaluate next and prev bones */
	if(bone->flag & BONE_IK_TOPARENT)
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
		VecMulf(h1, -hlength);
	}
	else {
		h1[0]= 0.0f; h1[1]= hlength; h1[2]= 0.0f;
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
		VecMulf(h2, -hlength);
		
	}
	else {
		h2[0]= 0.0f; h2[1]= -hlength; h2[2]= 0.0f;
		roll= 0.0;
	}
	
	//	VecMulf(h1, pchan->bone->ease1);
	//	VecMulf(h2, pchan->bone->ease2);
	
	/* make curve */
	if(bone->segments > MAX_BBONE_SUBDIV)
		bone->segments= MAX_BBONE_SUBDIV;
	
	forward_diff_bezier(0.0, h1[0],		h2[0],			0.0,		data[0],	bone->segments, 4);
	forward_diff_bezier(0.0, h1[1],		length + h2[1],	length,		data[0]+1,	bone->segments, 4);
	forward_diff_bezier(0.0, h1[2],		h2[2],			0.0,		data[0]+2,	bone->segments, 4);
	
	forward_diff_bezier(0.0, 0.390464f*roll, (1.0f-0.390464f)*roll,	roll,	data[0]+3,	bone->segments, 4);
	
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

void init_armature_deform(Object *parent, Object *ob)
{
	bArmature *arm;
	bDeformGroup *dg;
	MDeformVert *dvert;
	int	totverts;
	float	obinv[4][4];
	int i, j;

	arm = get_armature(parent);
	if (!arm)
		return;

	g_defbase = &ob->defbase;
	g_deform = parent;

	Mat4Invert(obinv, ob->obmat);
	Mat4CpyMat4(g_premat, ob->obmat);
	Mat4MulMat4(g_postmat, parent->obmat, obinv);

	Mat4Invert (g_premat, g_postmat);

	/* Store the deformation verts */
	if (ob->type==OB_MESH){
		g_dverts = ((Mesh*)ob->data)->dvert;
		totverts = ((Mesh*)ob->data)->totvert;
	}
	else{
		g_dverts=NULL;
		totverts=0;
	}

	/* bone defmats are already in the channels, chan_mat */
	
	/* Validate channel data in bDeformGroups */

	for (dg=g_defbase->first; dg; dg=dg->next)
		dg->data = (void*)get_pose_channel(parent->pose, dg->name);

	if (g_dverts){
		for (j=0; j<totverts; j++){
			dvert = g_dverts+j;
			for (i=0; i<dvert->totweight; i++){
				bDeformGroup *fg;
				fg = BLI_findlink (g_defbase, dvert->dw[i].def_nr);

				if (fg)
					dvert->dw[i].data = fg->data;
				else
					dvert->dw[i].data = NULL;
			}
		}
	}
}

float dist_to_bone (float vec[3], float b1[3], float b2[3])
{
/*  	float dist=0; */
	float bdelta[3];
	float pdelta[3];
	float hsqr, a, l;

	VecSubf (bdelta, b2, b1);
	l = Normalise (bdelta);

	VecSubf (pdelta, vec, b1);

	a = bdelta[0]*pdelta[0] + bdelta[1]*pdelta[1] + bdelta[2]*pdelta[2];
	hsqr = ((pdelta[0]*pdelta[0]) + (pdelta[1]*pdelta[1]) + (pdelta[2]*pdelta[2]));

	if (a < 0.0F){
		//return 100000;
		/* If we're past the end of the bone, do some weird field attenuation thing */
		return ((b1[0]-vec[0])*(b1[0]-vec[0]) +(b1[1]-vec[1])*(b1[1]-vec[1]) +(b1[2]-vec[2])*(b1[2]-vec[2])) ;
	}
	else if (a > l){
		//return 100000;
		/* If we're past the end of the bone, do some weird field attenuation thing */
		return ((b2[0]-vec[0])*(b2[0]-vec[0]) +(b2[1]-vec[1])*(b2[1]-vec[1]) +(b2[2]-vec[2])*(b2[2]-vec[2])) ;
	}
	else {
		return (hsqr - (a*a));
	}
}

static float calc_armature_deform_bone(Bone *bone, bPoseChannel *pchan, float *vec, float *co)
{
	float	dist, fac, ifac;
	float	cop[3];
	float	bdsqr, contrib=0.0;

	bdsqr = bone->dist*bone->dist;
	VECCOPY (cop, co);

	dist = dist_to_bone(cop, bone->arm_head, bone->arm_tail);
	
	if ((dist) <= bdsqr){
		fac = (dist)/bdsqr;
		ifac = 1.0F-fac;
		
		ifac*=bone->weight;
		contrib= ifac;
		if(contrib>0.0) {

			VECCOPY (cop, co);
			
			Mat4MulVecfl(pchan->chan_mat, cop);
			
			VecSubf (cop, cop, co);	//	Make this a delta from the base position
			cop[0]*=ifac; cop[1]*=ifac; cop[2]*=ifac;
			VecAddf (vec, vec, cop);
		}
	}
	
	return contrib;
}

void calc_bone_deform (bPoseChannel *pchan, float weight, float *vec, float *co, float *contrib)
{
	float	cop[3];

	if (!weight)
		return;

	VECCOPY (cop, co);
	
	Mat4MulVecfl(pchan->chan_mat, cop);
	
	vec[0]+=(cop[0]-co[0])*weight;
	vec[1]+=(cop[1]-co[1])*weight;
	vec[2]+=(cop[2]-co[2])*weight;

	(*contrib)+=weight;
}

void calc_armature_deform (Object *ob, float *co, int index)
{
	bPoseChannel *pchan;
	MDeformVert *dvert = g_dverts+index;
	float	vec[3];
	float	contrib=0.0;
	int		i;

	vec[0]=vec[1]=vec[2]=0;

	/* Apply the object's matrix */
	Mat4MulVecfl(g_premat, co);

	/* using deform vertex groups */
	if (g_dverts){
		
		for (i=0; i<dvert->totweight; i++){
			pchan = (bPoseChannel *)dvert->dw[i].data;
			if (pchan) calc_bone_deform (pchan, dvert->dw[i].weight, vec, co, &contrib);
		}
	}
	else {  /* or use bone distances */
		Bone *bone;
		
		for(pchan= g_deform->pose->chanbase.first; pchan; pchan= pchan->next) {
			bone= pchan->bone;
			if(bone) {
				contrib+= calc_armature_deform_bone(bone, pchan, vec, co);
			}
		}

	}
	if (contrib>0.0){
		vec[0]/=contrib;
		vec[1]/=contrib;
		vec[2]/=contrib;
	}

	VecAddf (co, vec, co);
	Mat4MulVecfl(g_postmat, co);
}

/* ************ END Armature Deform ******************* */

void get_objectspace_bone_matrix (struct Bone* bone, float M_accumulatedMatrix[][4], int root, int posed)
{
	Mat4CpyMat4(M_accumulatedMatrix, bone->arm_mat);
}

#if 0
/* IK in the sense of; connected directly */
static Bone *get_last_ik_bone (Bone *bone)
{
	Bone *curBone;
	
	for (curBone = bone->childbase.first; curBone; curBone=curBone->next){
		if (curBone->flag & BONE_IK_TOPARENT){
			return get_last_ik_bone (curBone);
		}
	}
	
	return bone;
}

#endif

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
		/* for quick detecting of next bone in chain */
		if(bone->flag & BONE_IK_TOPARENT)
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


/* allocates PoseChain, and links that to root bone/channel */
/* note; if we got this working, it can become static too? */
static void initialize_posechain(struct Object *ob, bPoseChannel *pchan_tip)
{
	bPoseChannel *curchan, *pchan_root=NULL, *chanlist[256];
	PoseChain *chain;
	bConstraint *con;
	bKinematicConstraint *data;
	int a, segcount= 0;
	
	/* find IK constraint, and validate it */
	for(con= pchan_tip->constraints.first; con; con= con->next) {
		if(con->type==CONSTRAINT_TYPE_KINEMATIC) break;
	}
	if(con==NULL) return;
	if(con->flag & CONSTRAINT_DISABLE) return;  // not sure...
	
	data=(bKinematicConstraint*)con->data;
	if(data->tar==NULL) return;
	if(data->tar->type==OB_ARMATURE && data->subtarget[0]==0) return;
	
	/* Find the chain's root & count the segments needed */
	for (curchan = pchan_tip; curchan; curchan=curchan->parent){
		pchan_root = curchan;
		/* tip is not in the chain */
		if (curchan!=pchan_tip){
			chanlist[segcount]=curchan;
			segcount++;
		}
		if(segcount>255) break; // also weak
		
		if (!(curchan->bone->flag & BONE_IK_TOPARENT))
			break;
	}
	if (!segcount) return;
	
	/* setup the chain data */
	chain = MEM_callocN(sizeof(PoseChain), "posechain");
	chain->totchannel= segcount;
	chain->solver = IK_CreateChain();
	chain->con= con;
	
	chain->iterations = data->iterations;
	chain->tolerance = data->tolerance;
	
	chain->pchanchain= MEM_callocN(segcount*sizeof(void *), "channel chain");
	for(a=0; a<segcount; a++) {
		chain->pchanchain[a]= chanlist[segcount-a-1];
	}
	
	/* AND! link the chain to the root */
	BLI_addtail(&pchan_root->chain, chain);
}

/* called from within the core where_is_pose loop, all animsystems and constraints
were executed & assigned. Now as last we do an IK pass */
static void execute_posechain(Object *ob, PoseChain *chain)
{
	IK_Segment_Extern	*segs;
	bPoseChannel *pchan;
	float R_parmat[3][3];
	float iR_parmat[3][3];
	float R_bonemat[3][3];
	float rootmat[4][4], imat[4][4];
	float size[3];
	int curseg;
	
	/* first set the goal inverse transform, assuming the root of chain was done ok! */
	pchan= chain->pchanchain[0];
	Mat4One(rootmat);
	VECCOPY(rootmat[3], pchan->pose_head);
	
	Mat4MulMat4 (imat, rootmat, ob->obmat);
	Mat4Invert (chain->goalinv, imat);
	
	/* and set and transform goal */
	get_constraint_target_matrix(chain->con, TARGET_BONE, NULL, rootmat, size, 1.0);   // 1.0=ctime
	VECCOPY (chain->goal, rootmat[3]);
	/* do we need blending? */
	if(chain->con->enforce!=1.0) {
		float vec[3];
		float fac= chain->con->enforce;
		float mfac= 1.0-fac;
		
		pchan= chain->pchanchain[chain->totchannel-1];	// last bone
		VECCOPY(vec, pchan->pose_tail);
		Mat4MulVecfl(ob->obmat, vec);					// world space
		chain->goal[0]= fac*chain->goal[0] + mfac*vec[0];
		chain->goal[1]= fac*chain->goal[1] + mfac*vec[1];
		chain->goal[2]= fac*chain->goal[2] + mfac*vec[2];
	}
	Mat4MulVecfl (chain->goalinv, chain->goal);
	
	/* Now we construct the IK segments */
	segs = MEM_callocN (sizeof(IK_Segment_Extern)*chain->totchannel, "iksegments");
	
	for (curseg=0; curseg<chain->totchannel; curseg++){
		
		pchan= chain->pchanchain[curseg];
		
		/* Get the matrix that transforms from prevbone into this bone */
		Mat3CpyMat4(R_bonemat, pchan->pose_mat);
		
		if (pchan->parent && (pchan->bone->flag & BONE_IK_TOPARENT)) {
			Mat3CpyMat4(R_parmat, pchan->parent->pose_mat);
		}
		else
			Mat3One (R_parmat);
		
		Mat3Inv(iR_parmat, R_parmat);
		
		/* Mult and Copy the matrix into the basis and transpose (IK lib likes it) */
		Mat3MulMat3((void *)segs[curseg].basis, iR_parmat, R_bonemat);
		Mat3Transp((void *)segs[curseg].basis);
		
		/* Fill out the IK segment */
		segs[curseg].length = pchan->bone->length; 
	}
	
	/*	Solve the chain */
	
	IK_LoadChain(chain->solver, segs, chain->totchannel);
	
	IK_SolveChain(chain->solver, chain->goal, chain->tolerance,  
				  chain->iterations,  0.1f, chain->solver->segments);
	
	
	/* not yet free! */
}

void free_posechain (PoseChain *chain)
{
	if (chain->solver) {
		MEM_freeN (chain->solver->segments);
		chain->solver->segments = NULL;
		IK_FreeChain(chain->solver);
	}
	if(chain->pchanchain) MEM_freeN(chain->pchanchain);
	MEM_freeN(chain);
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
	if (chan->bone==NULL || !(chan->bone->flag & BONE_IK_TOPARENT)) {
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
		Mat4MulSerie(pchan->pose_mat, parchan->pose_mat, offs_bone, pchan->chan_mat, NULL, NULL, NULL, NULL, NULL);
	}
	else 
		Mat4MulMat4(pchan->pose_mat, pchan->chan_mat, bone->arm_mat);
	
	
	/* Do constraints */
	if(pchan->constraints.first) {
		static Object conOb;
		static int initialized= 0;
		
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
	bPoseChannel *pchan, *next;
	float imat[4][4];
//	float ctime= (float)G.scene->r.cfra;	/* time only applies constraint location on curve path (now) */

	arm = get_armature(ob);
	
	if(arm==NULL) return;
	if(ob->pose==NULL || (ob->pose->flag & POSE_RECALC)) 
	   armature_rebuild_pose(ob, arm);
	
//	printf("re-evaluate pose %s\n", ob->id.name);
	
	/* In restposition we read the data from the bones */
	if(arm->flag & ARM_RESTPOS) {
		
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

		/* 1. construct the PoseChains, clear flags */
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			pchan->flag &= ~POSE_DONE;
			if(pchan->constflag & PCHAN_HAS_IK) // flag is set on editing constraints
				initialize_posechain(ob, pchan);	// will attach it to root!
		}
		
		/* 2. the main loop, channels are already hierarchical sorted from root to children */
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if(!(pchan->flag & POSE_DONE)) {
				/* 3. if we find an IK root, we handle it separated */
				if(pchan->chain.first) {
					while(pchan->chain.first) {
						PoseChain *chain= pchan->chain.first;
						int a;
						
						/* 4. walk over the chain for regular solving */
						for(a=0; a<chain->totchannel; a++) {
							if(!(chain->pchanchain[a]->flag & POSE_DONE))	// successive chains can set the flag
								where_is_pose_bone(ob, chain->pchanchain[a]);
						}
						/* 5. execute the IK solver */
						execute_posechain(ob, chain);   // calculates 3x3 difference matrices
						/* 6. apply the differences to the channels, we calculate the original differences first */
						for(a=0; a<chain->totchannel; a++)
							make_dmats(chain->pchanchain[a]);
						for(a=0; a<chain->totchannel; a++)
							where_is_ik_bone(chain->pchanchain[a], (void *)chain->solver->segments[a].basis_change);
							// (sets POSE_DONE)
						
						/* 6. and free */
						BLI_remlink(&pchan->chain, chain);
						free_posechain(chain);
					}
				}
				else where_is_pose_bone(ob, pchan);
			}
		}
	}
		
	/* calculating deform matrices */
	for(pchan= ob->pose->chanbase.first; pchan; pchan= next) {
		next= pchan->next;
		
		if(pchan->bone) {
			Mat4Invert(imat, pchan->bone->arm_mat);
			Mat4MulMat4(pchan->chan_mat, imat, pchan->pose_mat);
		}
	}
}


/* *************** helper for selection code ****************** */


Bone *get_indexed_bone (Object *ob, int index)
/*
	Now using pose channel
*/
{
	bPoseChannel *pchan;
	int a= 0;
	
	if(ob->pose==NULL) return NULL;
	
	for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next, a++) {
		if(a==index) return pchan->bone;
	}
	return NULL;
}


/* ****************** Game Blender functions, called by engine ************** */

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
	g_defbase = defbase;
	Mat4CpyMat4 (g_premat, premat);
	Mat4CpyMat4 (g_postmat, postmat);
	
}

void GB_validate_defgroups (Mesh *mesh, ListBase *defbase)
{
	/* Should only be called when the mesh or armature changes */
	int j, i;
	MDeformVert *dvert;
	
	for (j=0; j<mesh->totvert; j++){
		dvert = mesh->dvert+j;
		for (i=0; i<dvert->totweight; i++)
			dvert->dw[i].data = ((bDeformGroup*)BLI_findlink (defbase, dvert->dw[i].def_nr))->data;
	}
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
//		if (pchan) calc_bone_deform (pchan, dvert->dw[i].weight, vec, co, &contrib);
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

