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

#include <stdio.h> 
#include <string.h>

#include "MEM_guardedalloc.h"
#include "nla.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"

#include "BKE_utildefines.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_constraint.h"
#include "BKE_object.h"
#include "BKE_ipo.h"
#include "BKE_global.h"
#include "BKE_library.h"

#include "blendef.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif
/* Local function prototypes */
static void constraint_target_to_mat4 (Object *ob, const char *substring, float mat[][4], float size[3], float ctime);

/* Functions */

bConstraintChannel *find_constraint_channel (ListBase *list, const char *name){
	bConstraintChannel *chan;

	for (chan = list->first; chan; chan=chan->next){
		if (!strcmp(name, chan->name)){
			return chan;
		}
	}
	return NULL;
}

void do_constraint_channels (ListBase *conbase, ListBase *chanbase, float ctime)
{
	bConstraint *con;
	bConstraintChannel *chan;
	IpoCurve *icu;

	for (con=conbase->first; con; con=con->next){
		chan = find_constraint_channel(chanbase, con->name);
		if (chan && chan->ipo){
			calc_ipo(chan->ipo, ctime);
			for (icu=chan->ipo->curve.first; icu; icu=icu->next){
				switch (icu->adrcode){
				case CO_ENFORCE:
					con->enforce = icu->curval;
					if (con->enforce<0) con->enforce=0;
					else if (con->enforce>1) con->enforce=1;
					break;
				}
			}
		}
	}
}

void Mat4BlendMat4(float out[][4], float dst[][4], float src[][4], float srcweight)
{
	float squat[4], dquat[4], fquat[4];
	float ssize[3], dsize[3], fsize[4];
	float sloc[3], dloc[3], floc[3];
	float mat3[3][3], dstweight;
	float qmat[3][3], smat[3][3];
	int i;


	dstweight = 1.0F-srcweight;

	Mat3CpyMat4(mat3, dst);
	Mat3ToQuat(mat3, dquat);
	Mat3ToSize(mat3, dsize);
	VECCOPY (dloc, dst[3]);

	Mat3CpyMat4(mat3, src);
	Mat3ToQuat(mat3, squat);
	Mat3ToSize(mat3, ssize);
	VECCOPY (sloc, src[3]);
	
	/* Do the actual blend */
	for (i=0; i<3; i++){
		floc[i] = (dloc[i]*dstweight) + (sloc[i]*srcweight);
		fsize[i] = 1.0f + ((dsize[i]-1.0f)*dstweight) + ((ssize[i]-1.0f)*srcweight);
		fquat[i+1] = (dquat[i+1]*dstweight) + (squat[i+1]*srcweight);
	}
	
	/* Do one more iteration for the quaternions only and normalize the quaternion if needed */
	fquat[0] = 1.0f + ((dquat[0]-1.0f)*dstweight) + ((squat[0]-1.0f)*srcweight);
	NormalQuat (fquat);

	QuatToMat3(fquat, qmat);
	SizeToMat3(fsize, smat);

	Mat3MulMat3(mat3, qmat, smat);
	Mat4CpyMat3(out, mat3);
	VECCOPY (out[3], floc);
}

static void constraint_target_to_mat4 (Object *ob, const char *substring, float mat[][4], float size[3], float ctime)
{

	/*	Update the location of the target object */
	where_is_object_time (ob, ctime);	

	/*	Case OBJECT */
	if (!strlen(substring)){
		Mat4CpyMat4 (mat, ob->obmat);
		VECCOPY (size, ob->size);
		return;
	}

	/*	Case BONE */
	else {
		bArmature *arm;
		Bone	*bone;
		float	bmat[4][4];
		float	bsize[3]={1, 1, 1};

		arm = get_armature(ob);

		/**
		 *	Locate the bone (if there is one)
		 *	Ensures that the bone's transformation is fully constrained
		 *	(Cyclical relationships are disallowed elsewhere)
		 */
		bone = get_named_bone(arm, substring);
		if (bone){
			where_is_bone_time(ob, bone, ctime);
			get_objectspace_bone_matrix(bone, bmat, 1, 1);
			VECCOPY(bsize, bone->size);
		} 
		else
			Mat4One (bmat);

		/**
		 *	Multiply the objectspace bonematrix by the skeletons's global
		 *	transform to obtain the worldspace transformation of the target
		 */
		VECCOPY(size, bsize);
		Mat4MulMat4 (mat, bmat, ob->obmat);
	
		return;	
	}
}

void clear_object_constraint_status (Object *ob)
{
	bConstraint *con;

	if (!ob) return;

	/* Clear the object's constraints */
	for (con = ob->constraints.first; con; con=con->next){
		con->flag &= ~CONSTRAINT_DONE;
	}

	/* Clear the object's subdata constraints */
	switch (ob->type){
	case OB_ARMATURE:
		{
			clear_pose_constraint_status (ob);
		}
		break;
	default:
		break;
	}
}

short get_constraint_target (bConstraint *con, short ownertype, void* ownerdata, float mat[][4], float size[3], float ctime)
{
	short valid=0;

	switch (con->type){
	case CONSTRAINT_TYPE_NULL:
		{
			Mat4One(mat);
		}
		break;
	case CONSTRAINT_TYPE_ACTION:
		{
			if (ownertype == TARGET_BONE){
				bActionConstraint *data = (bActionConstraint*)con->data;
				bPose *pose=NULL;
				bPoseChannel *pchan=NULL;
				float tempmat[4][4], imat[4][4], ans[4][4], restmat[4][4], irestmat[4][4];
				float tempmat3[3][3];
				float eul[3], size[3];
				float s,t;
				Bone *curBone;
				Bone tbone;
				int i;
				
				curBone = (Bone*)ownerdata;
				
				if (data->tar){
					constraint_target_to_mat4(data->tar, data->subtarget, tempmat, size, ctime);
					valid=1;
				}
				else
					Mat4One (tempmat);
				
				/* If this is a bone, undo parent transforms */
				if (strlen(data->subtarget)){
					Bone* bone;

					Mat4Invert(imat, data->tar->obmat);
					bone = get_named_bone(get_armature(data->tar), data->subtarget);
					if (bone){
						get_objectspace_bone_matrix(bone, restmat, 1, 0);
						Mat4Invert(irestmat, restmat);
					}
				}
				else{
					Mat4One(imat);
					Mat4One(irestmat);
				}

				Mat4MulSerie(ans, imat, tempmat, irestmat, NULL, NULL, NULL, NULL, NULL);
				
				Mat3CpyMat4(tempmat3, ans);
				Mat3ToEul(tempmat3, eul);
				
				eul[0]*=(float)(180.0/M_PI);
				eul[1]*=(float)(180.0/M_PI);
				eul[2]*=(float)(180.0/M_PI);

				/* Target is the animation */
				s = (eul[data->type]-data->min)/(data->max-data->min);
				if (s<0)
					s=0;
				if (s>1)
					s=1;

				t = ( s * (data->end-data->start)) + data->start;
				
				/* Get the appropriate information from the action */
				pose = MEM_callocN(sizeof(bPose), "pose");
				
				verify_pose_channel(pose, curBone->name);
				get_pose_from_action (&pose, data->act, t);

				/* Find the appropriate channel */
				pchan = get_pose_channel(pose, curBone->name);
				if (pchan){
					memset(&tbone, 0x00, sizeof(Bone));

					VECCOPY (tbone.loc, pchan->loc);
					VECCOPY (tbone.size, pchan->size);				
					for (i=0; i<4; i++)
						tbone.quat[i]=pchan->quat[i];
					
					bone_to_mat4(&tbone, mat);

				}
				else{
					Mat4One(mat);
				}
				/* Clean up */
				clear_pose(pose);
				MEM_freeN(pose);
			}
			
		}
		break;
	case CONSTRAINT_TYPE_LOCLIKE:
		{
			bLocateLikeConstraint *data = (bLocateLikeConstraint*)con->data;

			if (data->tar){
				constraint_target_to_mat4(data->tar, data->subtarget, mat, size, ctime);
				valid=1;
			}
			else
				Mat4One (mat);
		} 
		break;
	case CONSTRAINT_TYPE_ROTLIKE:
		{
			bRotateLikeConstraint *data;
			data = (bRotateLikeConstraint*)con->data;

			if (data->tar){
				constraint_target_to_mat4(data->tar, data->subtarget, mat, size, ctime);
				valid=1;
			}
			else
				Mat4One (mat);
		} 
		break;
	case CONSTRAINT_TYPE_TRACKTO:
		{
			bTrackToConstraint *data;
			data = (bTrackToConstraint*)con->data;

			if (data->tar){
				constraint_target_to_mat4(data->tar, data->subtarget, mat, size, ctime);
				valid=1;
			}
			else
				Mat4One (mat);
		}
		break;
	case CONSTRAINT_TYPE_KINEMATIC:
		{
			bTrackToConstraint *data;
			data = (bTrackToConstraint*)con->data;

			if (data->tar){
				constraint_target_to_mat4(data->tar, data->subtarget, mat, size, ctime);
				valid=1;
			}
			else
				Mat4One (mat);
		} 
		break;
	default:
		Mat4One(mat);
		break;
	}

	return valid;
}

void relink_constraints (struct ListBase *list)
{
	bConstraint *con;

	for (con = list->first; con; con=con->next){
		switch (con->type){
		case CONSTRAINT_TYPE_KINEMATIC:
			{
				bKinematicConstraint *data;
				data = con->data;

				ID_NEW(data->tar);
			}
			break;
		case CONSTRAINT_TYPE_NULL:
			{
			}
			break;
		case CONSTRAINT_TYPE_TRACKTO:
			{
				bTrackToConstraint *data;
				data = con->data;

				ID_NEW(data->tar);
			}
			break;
		case CONSTRAINT_TYPE_ACTION:
			{
				bActionConstraint *data;
				data = con->data;

				ID_NEW(data->tar);
			}
			break;
		case CONSTRAINT_TYPE_LOCLIKE:
			{
				bLocateLikeConstraint *data;
				data = con->data;

				ID_NEW(data->tar);
			}
			break;
		case CONSTRAINT_TYPE_ROTLIKE:
			{
				bRotateLikeConstraint *data;
				data = con->data;

				ID_NEW(data->tar);
			}
			break;
		}
	}
}

void *copy_constraint_channels (ListBase *dst, ListBase *src)
{
	bConstraintChannel *dchan, *schan;
	bConstraintChannel *newact=NULL;

	dst->first=dst->last=NULL;
	duplicatelist(dst, src);
	
	for (dchan=dst->first, schan=src->first; dchan; dchan=dchan->next, schan=schan->next){
		dchan->ipo = copy_ipo(schan->ipo);
	}

	return newact;
}

bConstraintChannel *clone_constraint_channels (ListBase *dst, ListBase *src, bConstraintChannel *oldact)
{
	bConstraintChannel *dchan, *schan;
	bConstraintChannel *newact=NULL;

	dst->first=dst->last=NULL;
	duplicatelist(dst, src);
	
	for (dchan=dst->first, schan=src->first; dchan; dchan=dchan->next, schan=schan->next){
		id_us_plus((ID *)dchan->ipo);
		if (schan==oldact)
			newact=dchan;
	}

	return newact;
}

void copy_constraints (ListBase *dst, ListBase *src)
{
	bConstraint *con;

	dst->first=dst->last=NULL;

	duplicatelist (dst, src);

	/* Update specific data */
	if (!dst->first)
		return;

	for (con = dst->first; con; con=con->next){
		switch (con->type){
		case CONSTRAINT_TYPE_ACTION:
			{
				bActionConstraint *data;

				con->data = MEM_dupallocN (con->data);
				data = (bActionConstraint*) con->data;
			}
			break;
		case CONSTRAINT_TYPE_LOCLIKE:
			{
				bLocateLikeConstraint *data;

				con->data = MEM_dupallocN (con->data);
				data = (bLocateLikeConstraint*) con->data;
			}
			break;
		case CONSTRAINT_TYPE_ROTLIKE:
			{
				bRotateLikeConstraint *data;
				
				con->data = MEM_dupallocN (con->data);
				data = (bRotateLikeConstraint*) con->data;
			}
			break;
		case CONSTRAINT_TYPE_NULL:
			{
				con->data = NULL;
			}
			break;
		case CONSTRAINT_TYPE_TRACKTO:
			{
				bTrackToConstraint *data;
				
				con->data = MEM_dupallocN (con->data);
				data = (bTrackToConstraint*) con->data;
			}
			break;
		case CONSTRAINT_TYPE_KINEMATIC:
			{
				bKinematicConstraint *data;
				
				con->data = MEM_dupallocN (con->data);
				data = (bKinematicConstraint*) con->data;
			}
			break;
		default:
			con->data = MEM_dupallocN (con->data);
			break;
		}
	}
}

void evaluate_constraint (bConstraint *constraint, Object *ob, short ownertype, void *ownerdata, float targetmat[][4])
/* ob is likely to be a workob */
{
	float	M_oldmat[4][4];
	float	M_identity[4][4];
	float	enforce = 1.0;
	
	if (!constraint || !ob)
		return;
	
	Mat4One (M_identity);
	
	/* We've already been calculated */
	if (constraint->flag & CONSTRAINT_DONE){
		return;
	}
	
	switch (constraint->type){
	case CONSTRAINT_TYPE_ACTION:
		{
			float temp[4][4];
			bActionConstraint *data;
			
			data = constraint->data;
			Mat4CpyMat4 (temp, ob->obmat);

			Mat4MulMat4(ob->obmat, targetmat, temp);
		}
		break;
	case CONSTRAINT_TYPE_LOCLIKE:
		{
			bLocateLikeConstraint *data;

			data = constraint->data;
			
			if (data->flag & LOCLIKE_X)
				ob->obmat[3][0] = targetmat[3][0];
			if (data->flag & LOCLIKE_Y)
				ob->obmat[3][1] = targetmat[3][1];
			if (data->flag & LOCLIKE_Z)
				ob->obmat[3][2] = targetmat[3][2];
		}
		break;
	case CONSTRAINT_TYPE_ROTLIKE:
		{
		float	tmat[4][4];
		float	size[3];

		Mat4ToSize(ob->obmat, size);
		
		Mat4CpyMat4 (tmat, targetmat);
		Mat4Ortho(tmat);

		ob->obmat[0][0] = tmat[0][0]*size[0];
		ob->obmat[0][1] = tmat[0][1]*size[1];
		ob->obmat[0][2] = tmat[0][2]*size[2];

		ob->obmat[1][0] = tmat[1][0]*size[0];
		ob->obmat[1][1] = tmat[1][1]*size[1];
		ob->obmat[1][2] = tmat[1][2]*size[2];

		ob->obmat[2][0] = tmat[2][0]*size[0];
		ob->obmat[2][1] = tmat[2][1]*size[1];
		ob->obmat[2][2] = tmat[2][2]*size[2];
		}
		break;
	case CONSTRAINT_TYPE_NULL:
		{
		}
		break;
	case CONSTRAINT_TYPE_TRACKTO:
		{
			bTrackToConstraint *data;
			float	size[3];

			data=(bTrackToConstraint*)constraint->data;			
			
			if (data->tar){
					
				Mat4ToSize (ob->obmat, size);
	
				Mat4CpyMat4 (M_oldmat, ob->obmat);

				// Clear the object's rotation 	
				ob->obmat[0][0]=ob->size[0];
				ob->obmat[0][1]=0;
				ob->obmat[0][2]=0;
				ob->obmat[1][0]=0;
				ob->obmat[1][1]=ob->size[1];
				ob->obmat[1][2]=0;
				ob->obmat[2][0]=0;
				ob->obmat[2][1]=0;
				ob->obmat[2][2]=ob->size[2];
	
				solve_tracking(ob, targetmat);
			}
		}
		break;
	case CONSTRAINT_TYPE_KINEMATIC:
		{
			bKinematicConstraint *data;
			float	imat[4][4];
			float	temp[4][4];
			float totmat[4][4];

			data=(bKinematicConstraint*)constraint->data;

			if (data->tar && ownertype==TARGET_BONE && ownerdata){
				Bone *curBone = (Bone*)ownerdata;
				PoseChain *chain;
				Object *armob;
				
				/* Retrieve the owner armature object from the workob */
				armob = ob->parent;	
				
				/*	Make an IK chain  */				 
				chain = ik_chain_to_posechain(armob, curBone);
				if (!chain)
					return;
				chain->iterations = data->iterations;
				chain->tolerance = data->tolerance;
				
				
				{
					float parmat[4][4];
					
					/* Take the obmat to objectspace */
					Mat4CpyMat4 (temp, curBone->obmat);
					Mat4One (curBone->obmat);
					get_objectspace_bone_matrix(curBone, parmat, 1, 1);
					Mat4CpyMat4 (curBone->obmat, temp);
					Mat4MulMat4 (totmat, parmat, ob->parent->obmat);
					
					Mat4Invert (imat, totmat);
					
					Mat4CpyMat4 (temp, ob->obmat);
					Mat4MulMat4 (ob->obmat, temp, imat);
				}
				
				
				/* Solve it */
				if (chain->solver){
					VECCOPY (chain->goal, targetmat[3]);					
					solve_posechain(chain);
				}
				
				free_posechain(chain);
				
				{
					float parmat[4][4];
					
					/* Take the obmat to worldspace */
					Mat4CpyMat4 (temp, curBone->obmat);
					Mat4One (curBone->obmat);
					get_objectspace_bone_matrix(curBone, parmat, 1, 1);
					Mat4CpyMat4 (curBone->obmat, temp);
					Mat4MulMat4 (totmat, parmat, ob->parent->obmat);
					
					Mat4CpyMat4 (temp, ob->obmat);
					Mat4MulMat4 (ob->obmat, temp, totmat);
					
				}
			}
		}
		break;
	case CONSTRAINT_TYPE_FOLLOWPATH:
		break;
	default:
		printf ("Error: Unknown constraint type\n");
		break;
	}

}

void free_constraint_data (bConstraint *con)
{
	if (con->data){
		switch (con->type){
		default:
			break;
		};
		
		MEM_freeN (con->data);
	}
}

void free_constraints (ListBase *conlist)
{
	bConstraint *con;

	/* Do any specific freeing */
	for (con=conlist->first; con; con=con->next)
	{
		free_constraint_data (con);
	};

	/* Free the whole list */
	BLI_freelistN(conlist);
}

void free_constraint_channels (ListBase *chanbase)
{
	bConstraintChannel *chan;

	for (chan=chanbase->first; chan; chan=chan->next)
	{
		if (chan->ipo){
			chan->ipo->id.us--;
		}
	}

	BLI_freelistN(chanbase);
}
