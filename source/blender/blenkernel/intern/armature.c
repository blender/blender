/**
 * $Id$
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
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
#include "DNA_armature_types.h"
#include "DNA_action_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_view3d_types.h"
#include "DNA_constraint_types.h"

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

/*	Function prototypes */

static void apply_pose_bonechildren (Bone* bone, bPose* pose, int doit);
static float dist_to_bone (float vec[3], float b1[3], float b2[3]);
static Bone *get_named_bone_bonechildren (Bone *bone, const char *name);
static Bone *get_indexed_bone_bonechildren (Bone *bone, int *index);
/*void make_bone_parent_matrix (Bone* bone);*/
static void copy_bonechildren (Bone* newBone, Bone* oldBone);
static void calc_armature_deform_bonechildren (Bone *bone, float *vec, float *co, float *contrib, float obmat[][4]);
static int verify_boneptr_children (Bone *cBone, Bone *tBone);
static void where_is_bonelist_time (Object *ob, ListBase *base, float ctime);
static Bone *get_last_ik_bone (Bone *bone);
static void precalc_bonelist_posemats(ListBase *bonelist, float parlen);

/* Globals */
static float g_premat[4][4];
static float g_postmat[4][4];
static MDeformVert *g_dverts;
static ListBase		*g_defbase;
static bArmature *g_defarm;

/*	Functions */

float get_bone_length (Bone *bone)
{
	float result[3];

	VecSubf (result, bone->tail, bone->head);
	return (float)sqrt(result[0]*result[0] + result[1]*result[1] + result[2]*result[2]);

}

void apply_bonemat(Bone *bone)
{
	float mat[3][3], imat[3][3], tmat[3][3];
	
	if(!bone)
		return;

	Mat3CpyMat4(mat, bone->obmat);
	
	VECCOPY(bone->loc, bone->obmat[3]);
	
	Mat3ToQuat(mat, bone->quat);
	QuatToMat3(bone->quat, tmat);

	Mat3Inv(imat, tmat);
	
	Mat3MulMat3(tmat, imat, mat);
	
	bone->size[0]= tmat[0][0];
	bone->size[1]= tmat[1][1];
	bone->size[2]= tmat[2][2];

}

void GB_build_mats (float parmat[][4], float obmat[][4], float premat[][4], float postmat[][4])
{
	float obinv[4][4];
#if 0
	Mat4Invert(obinv, obmat);
	Mat4CpyMat4(premat, obmat);
	Mat4MulMat4(postmat, parmat, obinv);

	Mat4Invert (postmat, premat);
#else
	Mat4Invert(obinv, obmat);
	Mat4CpyMat4(premat, obmat);
	Mat4MulMat4(postmat, parmat, obinv);

	Mat4Invert (premat, postmat);
#endif
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
	Bone *bone;

	Mat4MulVecfl(g_premat, co);
	
	for (i=0; i<dvert->totweight; i++){
		bone = dvert->dw[i].data;
		if (bone) calc_bone_deform (bone, dvert->dw[i].weight, vec, co, &contrib);
	}
	
	if (contrib){
		vec[0]/=contrib;
		vec[1]/=contrib;
		vec[2]/=contrib;
	}
	
	VecAddf (co, vec, co);
	Mat4MulVecfl(g_postmat, co);
}

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

#if 0
static Bone *get_first_ik_bone (Bone *bone)
{
	Bone *curBone;

	for (curBone = bone; curBone; curBone=curBone->parent){
		if (!bone->parent)
			return curBone;
		if (!bone->flag & BONE_IK_TOPARENT)
			return curBone;
	}

	return bone;
/*	for (curBone = bone->childbase.first; curBone; curBone=curBone->next){
		if (curBone->flag & BONE_IK_TOPARENT){
			return get_last_ik_bone (curBone);
		}
	}
*/
	return bone;

}
#endif

void where_is_bone(Object *ob, Bone *bone)
{
	where_is_bone_time (ob, bone, G.scene->r.cfra);
}

void where_is_bone_time (Object *ob, Bone *bone, float ctime)
{ 
	where_is_bone1_time (ob, get_last_ik_bone(bone), ctime);
}

void rebuild_bone_parent_matrix (Bone *bone)
{
	if (!bone)
		return;

	if (bone->parent)
		rebuild_bone_parent_matrix(bone->parent);

	/* Get the parent inverse */
	if (bone->parent)
		Mat4MulMat4(bone->parmat, bone->parent->obmat, bone->parent->parmat);
	else
		Mat4One (bone->parmat);

}
void where_is_bone1_time (Object *ob, Bone *bone, float ctime)
/* Assumes the pose has already been retrieved from the action */
/* Also assumes where_is_object has been called for owner */
{
	bPose	*pose;
	bPoseChannel	*chan;
	bArmature *arm;
	float	imat[4][4];
	float	totmat[4][4];
	Object conOb;

	pose = ob->pose;
	if (!pose)
		return;
	
	arm = get_armature(ob);

	/* Ensure there is achannel for this bone*/
	verify_pose_channel (pose, bone->name);

	/* Search the pose for a channel with the same name, and copy the
		transformations from the channel into the bone */
	for (chan=pose->chanbase.first; chan; chan=chan->next){
		if (!strcmp (chan->name, bone->name)){

#if 1	/* If 1 attempt to use pose caching features */
			/* Bail out if we've been recalced recently */
			if (chan->flag & PCHAN_DONE){
				Mat4CpyMat4 (bone->obmat, chan->obmat);
				if (bone->parent){
					if ((bone->flag & BONE_IK_TOPARENT))
						where_is_bone1_time (ob, bone->parent, ctime);
					else
						where_is_bone_time (ob, bone->parent, ctime);
				}
				return;
			}
			else
				chan->flag |= PCHAN_DONE;
#endif
			break;
		}
	}

#if 1
	/* Ensure parents have been evaluated */
	if (bone->parent){
		if ((bone->flag & BONE_IK_TOPARENT))
			where_is_bone1_time (ob, bone->parent, ctime);
		else
			where_is_bone_time (ob, bone->parent, ctime);
	}

	/* Build the parent matrix : Depreciated */
//	if (bone->parent)
//		Mat4MulMat4(bone->parmat, bone->parent->obmat, bone->parent->parmat);
//	else
//		Mat4One (bone->parmat);
#endif

	if (arm){
		if ((arm->flag & ARM_RESTPOS) || ((G.obedit && (ob->data == G.obedit->data)))){
			Mat4One (bone->obmat);
			Mat4One (chan->obmat);
			return;
		}
	}

	if (bone->flag & BONE_IK_TOPARENT){
		bone->loc[0]=bone->loc[1]=bone->loc[2]=0.0F;
	}
	bone_to_mat4(bone, bone->obmat);	
	
	/* Do constraints */
	//	clear_workob();
	
	memset(&conOb, 0, sizeof(Object));	
	conOb.size[0]= conOb.size[1]= conOb.size[2]= 1.0;
	
	/* Collect the constraints from the pose */
	conOb.constraints.first = chan->constraints.first;
	conOb.constraints.last = chan->constraints.last;
	
	/* Totmat takes bone's obmat to worldspace */
	
	{
		float parmat[4][4];
		float temp[4][4];
		
		Mat4CpyMat4 (temp, bone->obmat);
		Mat4One (bone->obmat);
		get_objectspace_bone_matrix(bone, parmat, 1, 1);
		Mat4CpyMat4 (bone->obmat, temp);
		Mat4MulMat4 (totmat, parmat, ob->obmat);
	}
	
	/* Build a workob to pass the bone to the constraint solver */
	conOb.data = ob->data;
	conOb.type = ob->type;
	conOb.parent = ob;	
	conOb.trackflag = ob->trackflag;
	conOb.upflag = ob->upflag;

	VECCOPY(conOb.size, bone->size);
	
	Mat4MulMat4 (conOb.obmat, bone->obmat, totmat);
	
	/* Solve */
	solve_constraints (&conOb, TARGET_BONE, (void*)bone, ctime);
	
	{
		float parmat[4][4];
		float temp[4][4];
		
		Mat4CpyMat4 (temp, bone->obmat);
		Mat4One (bone->obmat);
		get_objectspace_bone_matrix(bone, parmat, 1, 1);
		Mat4CpyMat4 (bone->obmat, temp);
		Mat4MulMat4 (totmat, parmat, ob->obmat);
	}

	VECCOPY(bone->size, conOb.size);
	
	/* Take out of worldspace */
	Mat4Invert (imat, totmat);
	Mat4MulMat4 (bone->obmat, conOb.obmat, imat);
	Mat4CpyMat4 (chan->obmat, bone->obmat);

}


bArmature *get_armature(Object *ob)
{
	if(ob==NULL) return NULL;
	if(ob->type==OB_ARMATURE) return ob->data;
	else return NULL;
}

void init_armature_deform(Object *parent, Object *ob)
{
	bArmature *arm;
	bDeformGroup *dg;
	Bone *curBone;
	MDeformVert *dvert;
	int	totverts;
	float	obinv[4][4];
	int i, j;

	arm = get_armature(parent);
	if (!arm)
		return;

	if (ob)
		where_is_object (ob);

#if 1
	apply_pose_armature (arm, parent->pose, 1);	/* Hopefully doit parameter can be set to 0 in future */
	where_is_armature (parent);
#else
	apply_pose_armature (arm, parent->pose, 0);
#endif

	g_defbase = &ob->defbase;
	g_defarm = arm;

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

	/* Precalc bone defmats */
	precalc_armature_posemats (arm);

	for (curBone=arm->bonebase.first; curBone; curBone=curBone->next){
		precalc_bone_defmat(curBone);
	}
	
	/* Validate bone data in bDeformGroups */

	for (dg=g_defbase->first; dg; dg=dg->next)
		dg->data = (void*)get_named_bone(arm, dg->name);

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

void get_bone_root_pos (Bone* bone, float vec[3], int posed)
{
	Bone	*curBone;
	float	mat[4][4];
	
	get_objectspace_bone_matrix(bone, mat, 1, posed);
	VECCOPY (vec, mat[3]);
	return;

	rebuild_bone_parent_matrix(bone);
	if (posed){

		get_objectspace_bone_matrix(bone, mat, 1, posed);
		VECCOPY (vec, mat[3]);
	}
	else {
		vec[0]=vec[1]=vec[2]=0.0F;
		for (curBone=bone; curBone; curBone=curBone->parent){
			if (curBone==bone)
				VecAddf (vec, vec, curBone->head);
			else
				VecAddf (vec, vec, curBone->tail);
		}
	}
}

void get_bone_tip_pos (Bone* bone, float vec[3], int posed)
{
	Bone	*curBone;
	float	mat[4][4], tmat[4][4], rmat[4][4], bmat[4][4], fmat[4][4];

	get_objectspace_bone_matrix(bone, mat, 0, posed);
	VECCOPY (vec, mat[3]);
	return;

	rebuild_bone_parent_matrix(bone);
	if (posed){
	
	Mat4One (mat);

	for (curBone = bone; curBone; curBone=curBone->parent){
		Mat4One (bmat);
		/*	[BMAT] This bone's offset */
		VECCOPY (bmat[3], curBone->head);
		if (curBone==bone){
			Mat4One (tmat);
			VecSubf (tmat[3], curBone->tail, curBone->head);
 			Mat4MulMat4 (bmat, tmat, curBone->obmat);
			VecAddf (bmat[3], bmat[3], curBone->head);
		}
		else
			VecAddf (bmat[3], bmat[3], curBone->obmat[3]);	// Test

		/* [RMAT] Parent's bone length = parent rotmat * bone length */
		if (curBone->parent){
			Mat4One (tmat);
			VecSubf (tmat[3], curBone->parent->tail, curBone->parent->head);
			Mat4MulMat4 (rmat, tmat, curBone->parent->obmat);
			VecSubf (rmat[3], rmat[3], curBone->parent->obmat[3]);
		}
		else
			Mat4One (rmat);

		Mat4MulSerie (fmat, rmat, bmat, mat, 0, 0, 0, 0, 0);
		Mat4CpyMat4 (mat, fmat);
	}

		VECCOPY (vec, mat[3]);
	}
	else{
		vec[0]=vec[1]=vec[2]=0.0F;
		for (curBone=bone; curBone; curBone=curBone->parent){
			VecAddf (vec, vec, curBone->tail);
		}
	}
}

int	verify_boneptr (bArmature *arm, Bone *bone)
{
	/* Ensures that a given bone exists in an armature */
	Bone *curBone;

	if (!arm)
		return 0;

	for (curBone=arm->bonebase.first; curBone; curBone=curBone->next){
		if (verify_boneptr_children (curBone, bone))
			return 1;
	}

	return 0;
}

static int verify_boneptr_children (Bone *cBone, Bone *tBone)
{
	Bone *curBone;

	if (cBone == tBone)
		return 1;

	for (curBone=cBone->childbase.first; curBone; curBone=curBone->next){
		if (verify_boneptr_children (curBone, tBone))
			return 1;
	}
	return 0;
}


static float dist_to_bone (float vec[3], float b1[3], float b2[3])
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

static void calc_armature_deform_bonechildren (Bone *bone, float *vec, float *co, float *contrib, float obmat[][4])
{
	Bone *curBone;
	float	root[3];
	float	tip[3];
	float	dist, fac, ifac;
	float	cop[3];
	float	bdsqr;

	
	get_bone_root_pos (bone, root, 0);
	get_bone_tip_pos (bone, tip, 0);

	bdsqr = bone->dist*bone->dist;
	VECCOPY (cop, co);

	dist = dist_to_bone(cop, root, tip);
	
	if ((dist) <= bdsqr){
		fac = (dist)/bdsqr;
		ifac = 1.0F-fac;
		
		ifac*=bone->weight;
		
		if (!vec)
			(*contrib) +=ifac;
		else{
			ifac*=(1.0F/(*contrib));

			VECCOPY (cop, co);
			
			Mat4MulVecfl(bone->defmat, cop);
			
			VecSubf (cop, cop, co);	//	Make this a delta from the base position
			cop[0]*=ifac; cop[1]*=ifac; cop[2]*=ifac;
			VecAddf (vec, vec, cop);

		}
	}
	
//	calc_bone_deform (bone, bone->weight, vec, co, contrib, obmat);
	for (curBone = bone->childbase.first; curBone; curBone=curBone->next)
		calc_armature_deform_bonechildren (curBone, vec, co, contrib, obmat);
}

void precalc_bone_irestmat (Bone *bone)
{
	float restmat[4][4];

	get_objectspace_bone_matrix(bone, restmat, 1, 0);
	Mat4Invert (bone->irestmat, restmat);
}

static void precalc_bonelist_posemats(ListBase *bonelist, float parlen)
{
	Bone *curBone;
	float length;
	float T_parlen[4][4];
	float T_root[4][4];
	float M_obmat[4][4];
	float R_bmat[4][4];
	float M_accumulatedMatrix[4][4];
	float delta[3];

	for (curBone = bonelist->first; curBone; curBone=curBone->next){

		/* Get the length translation (length along y axis) */
		length = get_bone_length(curBone);

		/* Get the bone's root offset (in the parent's coordinate system) */
		Mat4One (T_root);
		VECCOPY (T_root[3], curBone->head);

		/* Compose the restmat */
		VecSubf(delta, curBone->tail, curBone->head);
		make_boneMatrixvr(R_bmat, delta, curBone->roll);

		/* Retrieve the obmat (user transformation) */
		Mat4CpyMat4 (M_obmat, curBone->obmat);

		/* Compose the accumulated matrix (i.e. parent matrix * parent translation ) */
		if (curBone->parent){
			Mat4One (T_parlen);
			T_parlen[3][1] = parlen;
			Mat4MulMat4 (M_accumulatedMatrix, T_parlen, curBone->parent->posemat);
		}
		else
			Mat4One (M_accumulatedMatrix);

		/* Compose the matrix for this bone  */
		Mat4MulSerie (curBone->posemat, M_accumulatedMatrix, T_root, R_bmat, M_obmat, NULL, NULL, NULL, NULL);

		precalc_bonelist_posemats(&curBone->childbase, length);
	}
}

void precalc_armature_posemats (bArmature *arm)
{
	precalc_bonelist_posemats(&arm->bonebase, 0.0);
}

void precalc_bone_defmat (Bone *bone)
{
	Bone *curBone;
#if 0
	float restmat[4][4];
	float posemat[4][4];
	float imat[4][4];
	
	/* Store restmat and restmat inverse - Calculate once when leaving editmode */
	/* Store all bones' posemats - Do when applied */

	/* EXPENSIVE! Don't do this! */
	get_objectspace_bone_matrix(bone, restmat, 1, 0);
	get_objectspace_bone_matrix(bone, posemat, 1, 1);
	Mat4Invert (imat, restmat);
	Mat4MulMat4 (bone->defmat, imat, posemat);
	/* /EXPENSIVE */
#else
	Mat4MulMat4 (bone->defmat, bone->irestmat, bone->posemat);
#endif
	for (curBone = bone->childbase.first; curBone; curBone=curBone->next){
		precalc_bone_defmat(curBone);
	}
}

void calc_bone_deform (Bone *bone, float weight, float *vec, float *co, float *contrib)
{
	float	cop[3];

	if (!weight)
		return;

	VECCOPY (cop, co);
	
	Mat4MulVecfl(bone->defmat, cop);
	
	vec[0]+=(cop[0]-co[0])*weight;
	vec[1]+=(cop[1]-co[1])*weight;
	vec[2]+=(cop[2]-co[2])*weight;

	(*contrib)+=weight;
}

void calc_armature_deform (Object *ob, float *co, int index)
{
	bArmature *arm;
	Bone *bone;
	Bone	*curBone;
	float	vec[3];
	float	contrib=0;
	int		i;
	MDeformVert *dvert = g_dverts+index;

	arm=g_defarm;
	vec[0]=vec[1]=vec[2]=0;

	/* Apply the object's matrix */
	Mat4MulVecfl(g_premat, co);

	if (g_dverts){
		for (i=0; i<dvert->totweight; i++){
			bone = dvert->dw[i].data;
			if (bone) calc_bone_deform (bone, dvert->dw[i].weight, vec, co, &contrib);
		}
		
		if (contrib){
			vec[0]/=contrib;
			vec[1]/=contrib;
			vec[2]/=contrib;
		}
		VecAddf (co, vec, co);
		Mat4MulVecfl(g_postmat, co);
		return;
	}


	//	Count the number of interested bones
	for (curBone = arm->bonebase.first; curBone; curBone=curBone->next)
		calc_armature_deform_bonechildren (curBone, NULL, co, &contrib, ob->obmat);

	//	Do the deformation
	for (curBone = arm->bonebase.first; curBone; curBone=curBone->next)
		calc_armature_deform_bonechildren (curBone, vec, co, &contrib, ob->obmat);

	VecAddf (co, vec, co);
	Mat4MulVecfl(g_postmat, co);
}

void apply_pose_armature (bArmature* arm, bPose* pose, int doit)
{
	Bone	*curBone;
	for (curBone = arm->bonebase.first; curBone; curBone=curBone->next){
		apply_pose_bonechildren (curBone, pose, doit);
	}
}

void where_is_armature (Object *ob)
{
	where_is_object (ob);
	where_is_armature_time(ob, (float)G.scene->r.cfra);
}

void where_is_armature_time (Object *ob, float ctime)
{
	bArmature *arm;

	arm = get_armature(ob);
	if (!arm)
		return;

	where_is_bonelist_time (ob, &arm->bonebase, ctime);

}

static void where_is_bonelist_time (Object *ob, ListBase *base, float ctime)
{
	Bone *curBone;

	for (curBone=base->first; curBone; curBone=curBone->next){
		if (!curBone->childbase.first)
			where_is_bone1_time (ob, curBone, ctime);

		where_is_bonelist_time(ob, &curBone->childbase, ctime);
	}
}
static void apply_pose_bonechildren (Bone* bone, bPose* pose, int doit)
{
	Bone	*curBone;
	bPoseChannel	*chan;

	if (!pose){
		
		bone->dsize[0]=bone->dsize[1]=bone->dsize[2]=1.0F;
		bone->size[0]=bone->size[1]=bone->size[2]=1.0F;

		bone->dquat[0]=bone->dquat[1]=bone->dquat[2]=bone->dquat[3]=0;
		bone->quat[0]=bone->quat[1]=bone->quat[2]=bone->quat[3]=0.0F;
		
		bone->dloc[0]=bone->dloc[1]=bone->dloc[2]=0.0F;
		bone->loc[0]=bone->loc[1]=bone->loc[2]=0.0F;
	}

	// Ensure there is achannel for this bone 
	verify_pose_channel (pose, bone->name);

	// Search the pose for a channel with the same name 
	if (pose){
		for (chan=pose->chanbase.first; chan; chan=chan->next){
			if (!strcmp (chan->name, bone->name)){
				if (chan->flag & POSE_LOC) 
					memcpy (bone->loc, chan->loc, sizeof (bone->loc));
				if (chan->flag & POSE_SIZE) 
					memcpy (bone->size, chan->size, sizeof (bone->size));
				if (chan->flag & POSE_ROT) 
					memcpy (bone->quat, chan->quat, sizeof (bone->quat));			

				if (doit){
					bone_to_mat4(bone, bone->obmat);
				}
				else{
					Mat4CpyMat4 (bone->obmat, chan->obmat);
				}


				break;
			}
		}
	}
	
	for (curBone = bone->childbase.first; curBone; curBone=curBone->next){
		apply_pose_bonechildren (curBone, pose, doit);
	}
}

void make_boneMatrixvr (float outmatrix[][4],float delta[3], float roll)
/*	Calculates the rest matrix of a bone based
	On its vector and a roll around that vector */
{
	float	nor[3],axis[3],target[3]={0,1,0};
	float	theta;
	float	rMatrix[3][3], bMatrix[3][3], fMatrix[3][3];

	VECCOPY (nor,delta);
	Normalise (nor);

	/*	Find Axis & Amount for bone matrix*/
	Crossf (axis,target,nor);
	Normalise (axis);
	theta=(float) acos (Inpf (target,nor));

	/*	Make Bone matrix*/
	VecRotToMat3(axis, theta, bMatrix);

	/*	Make Roll matrix*/
	VecRotToMat3(nor, roll, rMatrix);
	
	/*	Combine and output result*/
	Mat3MulMat3 (fMatrix,rMatrix,bMatrix);
	Mat4CpyMat3 (outmatrix,fMatrix);
}

void make_boneMatrix (float outmatrix[][4], Bone *bone)
/*	Calculates the rest matrix of a bone based
	On its vector and a roll around that vector */
{
	float	delta[3];
	float	parmat[4][4], imat[4][4], obmat[4][4];

	if (bone->parent){
		VecSubf (delta, bone->parent->tail, bone->parent->head);
		make_boneMatrixvr(parmat, delta, bone->parent->roll);
	}
	else{
		Mat4One (parmat);
	}

	Mat4Invert (imat, parmat);
	
	VecSubf (delta, bone->tail, bone->head);
	make_boneMatrixvr(obmat, delta, bone->roll);

	Mat4MulMat4(outmatrix, obmat, imat);

}


bArmature *add_armature()
{
	bArmature *arm;

	arm= alloc_libblock (&G.main->armature, ID_AR, "Armature");

	if(arm) {

	
	}
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


void bone_to_mat3(Bone *bone, float mat[][3])	/* no parent */
{
	float smat[3][3];
	float rmat[3][3];
/*	float q1[4], vec[3];*/
	
	/* size */
/*	if(bone->ipo) {
		vec[0]= bone->size[0]+bone->dsize[0];
		vec[1]= bone->size[1]+bone->dsize[1];
		vec[2]= bone->size[2]+bone->dsize[2];
		SizeToMat3(vec, smat);
	}
	else 
*/	{
		SizeToMat3(bone->size, smat);
	}

	/* rot */
	/*if(bone->flag & BONE_QUATROT) {
		if(bone->ipo) {
			QuatMul(q1, bone->quat, bone->dquat);
			QuatToMat3(q1, rmat);
		}
		else 
	*/	{
			NormalQuat(bone->quat);
			QuatToMat3(bone->quat, rmat);
		}
/*	}
*/
	Mat3MulMat3(mat, rmat, smat);
}

void bone_to_mat4(Bone *bone, float mat[][4])
{
	float tmat[3][3];
	
	bone_to_mat3(bone, tmat);
	
	Mat4CpyMat3(mat, tmat);
	
	VECCOPY(mat[3], bone->loc);
//	VecAddf(mat[3], mat[3], bone->loc);
/*	if(bone->ipo) {
		mat[3][0]+= bone->dloc[0];
		mat[3][1]+= bone->dloc[1];
		mat[3][2]+= bone->dloc[2];
	}
*/
}

Bone *get_indexed_bone (bArmature *arm, int index)
/*
	Walk the list until the index is reached
*/
{
	Bone *bone=NULL, *curBone;
	int	ref=index;

	if (!arm)
		return NULL;

	for (curBone=arm->bonebase.first; curBone; curBone=curBone->next){
		bone = get_indexed_bone_bonechildren (curBone, &ref);
		if (bone)
			return bone;
	}

	return bone;
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

static Bone *get_indexed_bone_bonechildren (Bone *bone, int *index)
{
	Bone *curBone, *rbone;

	if (!*index)
		return bone;

	(*index)--;

	for (curBone=bone->childbase.first; curBone; curBone=curBone->next){
		rbone=get_indexed_bone_bonechildren (curBone, index);
		if (rbone)
			return rbone;
	}

	return NULL;
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

void make_displists_by_armature (Object *ob)
{
	Base *base;

	if (ob){
		for (base= G.scene->base.first; base; base= base->next){
			if ((ob==base->object->parent) && (base->lay & G.scene->lay))
				if (base->object->partype==PARSKEL )
					makeDispList(base->object);		
		}
	}
}	

void get_objectspace_bone_matrix (struct Bone* bone, float M_accumulatedMatrix[][4], int root, int posed)
/* Gets matrix that transforms the bone to object space */
/* This function is also used to compute the orientation of the bone for display */
{
	Bone	*curBone;

	Bone	*bonelist[256];
	int		bonecount=0, i;

	Mat4One (M_accumulatedMatrix);

	/* Build a list of bones from tip to root */
	for (curBone=bone; curBone; curBone=curBone->parent){
		bonelist[bonecount] = curBone;
		bonecount++;
	}

	/* Count through the inverted list (i.e. iterate from root to tip)*/
	for (i=0; i<bonecount; i++){
		float T_root[4][4];
		float T_len[4][4];
		float R_bmat[4][4];
		float M_obmat[4][4];
		float M_boneMatrix[4][4];
		float delta[3];

		curBone = bonelist[bonecount-i-1];

		/* Get the length translation (length along y axis) */
		Mat4One (T_len);
		T_len[3][1] = get_bone_length(curBone);

		if ((curBone == bone) && (root))
			Mat4One (T_len);

		/* Get the bone's root offset (in the parent's coordinate system) */
		Mat4One (T_root);
		VECCOPY (T_root[3], curBone->head);

		/* Compose the restmat */
		VecSubf(delta, curBone->tail, curBone->head);
		make_boneMatrixvr(R_bmat, delta, curBone->roll);


		/* Retrieve the obmat (user transformation) */
		if (posed)
			Mat4CpyMat4 (M_obmat, curBone->obmat);
		else
			Mat4One (M_obmat);

		/* Compose the matrix for this bone  */
#if 0
		Mat4MulSerie (M_boneMatrix, M_accumulatedMatrix, T_root, M_obmat, R_bmat, T_len, NULL, NULL, NULL);
#else
		Mat4MulSerie (M_boneMatrix, M_accumulatedMatrix, T_root, R_bmat, M_obmat, T_len, NULL, NULL, NULL);
#endif
		Mat4CpyMat4 (M_accumulatedMatrix, M_boneMatrix);
	}


}

void solve_posechain (PoseChain *chain)
{
	float	goal[3];
	int	i;
	Bone *curBone;
	float M_obmat[4][4];
	float M_basischange[4][4];
	bPoseChannel *chan;

	if (!chain->solver) return;

	/**
	 *	Transform the goal from worldspace
	 * 	to the coordinate system of the root
	 *	of the chain.  The matrix for this
	 *	was computed when the chain was built
	 *	in ik_chain_to_posechain
	 */

	VECCOPY (goal, chain->goal);
	Mat4MulVecfl (chain->goalinv, goal);

	/*	Solve the chain */

	IK_SolveChain(chain->solver,
		goal,
		chain->tolerance,  
		chain->iterations,
		0.1f,
		chain->solver->segments);
 
	/* Copy the results back into the bones */
	for (i = chain->solver->num_segments-1, curBone=chain->target->parent; i>=0; i--, curBone=curBone->parent){

		/* Retrieve the delta rotation from the solver */
		Mat4One(M_basischange);
		Mat4CpyMat3(M_basischange, chain->solver->segments[i].basis_change);
	
 
		/**
		 *	Multiply the bone's usertransform by the 
		 *	basis change to get the new usertransform
		 */

		Mat4CpyMat4 (M_obmat, curBone->obmat);
		Mat4MulMat4 (curBone->obmat, M_basischange, M_obmat);

		/* Store the solve results on the childrens' channels */
		for (chan = chain->pose->chanbase.first; chan; chan=chan->next){
			if (!strcmp (chan->name, curBone->name)){
				Mat4CpyMat4 (chan->obmat, curBone->obmat);
				break;
			}
		}

	}
}

void free_posechain (PoseChain *chain)
{
	if (chain->solver) {
		MEM_freeN (chain->solver->segments);
		chain->solver->segments = NULL;
		IK_FreeChain(chain->solver);
	}
	MEM_freeN (chain);
}

PoseChain *ik_chain_to_posechain (Object *ob, Bone *bone)
{
	IK_Segment_Extern	*segs;
	PoseChain	*chain = NULL;
	Bone		*curBone, *rootBone;
	int			segcount, curseg, icurseg;
	float	imat[4][4];
	Bone *bonelist[256];
	float rootmat[4][4];
	float	bonespace[4][4];

	/**
	 *	Some interesting variables in this function:
	 *
	 *	Bone->obmat		Bone's user transformation;
	 *					It is initialized in where_is_bone1_time
	 *
	 *	rootmat			Bone's coordinate system, computed by
	 *					get_objectspace_bone_matrix.  Takes all
	 *					parents transformations into account.
	 */



	/* Ensure that all of the bone parent matrices are correct */

	/* Find the chain's root & count the segments needed */
	segcount = 0;
	for (curBone = bone; curBone; curBone=curBone->parent){
		rootBone = curBone;
		if (curBone!=bone){
			bonelist[segcount]=curBone;
			segcount++;
		}
		if (!curBone->parent)
			break;
		else if (!(curBone->flag & BONE_IK_TOPARENT))
			break;
	}

	if (!segcount)
		return NULL;


	/**
	 *	Initialize a record to store information about the original bones
	 *	This will be the return value for this function.
	 */

	chain = MEM_callocN(sizeof(PoseChain), "posechain");	
	chain->solver = IK_CreateChain();
	chain->target = bone;
	chain->root = rootBone;
	chain->pose = ob->pose;

	/* Allocate some IK segments */
	segs = MEM_callocN (sizeof(IK_Segment_Extern)*segcount, "iksegments");


	/**
	 * Remove the offset from the first bone in the chain and take the target to chainspace
	 */


	get_objectspace_bone_matrix(rootBone, bonespace, 1, 1);
	Mat4One (rootmat);
	VECCOPY (rootmat[3], bonespace[3]);

	/* Take the target to bonespace */
	Mat4MulMat4 (imat, rootmat, ob->obmat);
	Mat4Invert (chain->goalinv, imat);


	/**
	 *	Build matrices from the root to the tip 
	 *	We count backwards through the bone list (which is sorted tip to root)
	 *	and forwards through the ik_segment list
	 */

	for (curseg = segcount-1; curseg>=0; curseg--){
		float M_basismat[4][4];
		float R_parmat[4][4];
		float iR_parmat[4][4];
		float R_bonemat[4][4];

		/* Retrieve the corresponding bone for this segment */
		icurseg=segcount-curseg-1;
		curBone = bonelist[curseg];
		
		/* Get the basis matrix */
		Mat4One (R_parmat);
		get_objectspace_bone_matrix(curBone, R_bonemat, 1, 1);
		R_bonemat[3][0]=R_bonemat[3][1]=R_bonemat[3][2]=0.0F;
		
		if (curBone->parent && (curBone->flag & BONE_IK_TOPARENT)){
			get_objectspace_bone_matrix(curBone->parent, R_parmat, 1, 1);
			R_parmat[3][0]=R_parmat[3][1]=R_parmat[3][2]=0.0F;
		}
		
		Mat4Invert(iR_parmat, R_parmat);
		Mat4MulMat4(M_basismat, R_bonemat, iR_parmat);
		
		/* Copy the matrix into the basis and transpose */
		Mat3CpyMat4(segs[icurseg].basis, M_basismat);
		Mat3Transp(segs[icurseg].basis);

		/* Fill out the IK segment */
		segs[icurseg].length = get_bone_length(curBone);

	};

	IK_LoadChain(chain->solver, segs, segcount);
	return chain;
}



void precalc_bonelist_irestmats (ListBase* bonelist)
{
	Bone *curbone;

	if (!bonelist)
		return;

	for (curbone = bonelist->first; curbone; curbone=curbone->next){
		precalc_bone_irestmat(curbone);
		precalc_bonelist_irestmats(&curbone->childbase);
	}
}
