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
 * Contributor(s): 2007, Joshua Leung, major recode
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#include <stdio.h> 
#include <string.h>
#include <math.h>

#include "MEM_guardedalloc.h"
#include "nla.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_scene_types.h"

#include "BKE_utildefines.h"
#include "BKE_action.h"
#include "BKE_anim.h" // for the curve calculation part
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_constraint.h"
#include "BKE_displist.h"
#include "BKE_object.h"
#include "BKE_ipo.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_idprop.h"

#include "BPY_extern.h"

#include "blendef.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifndef M_PI
#define M_PI		3.14159265358979323846
#endif


/* ********************* Data level ****************** */

void free_constraint_data (bConstraint *con)
{
	if (con->data) {
		/* any constraint-type specific stuff here */
		switch (con->type) {
			case CONSTRAINT_TYPE_PYTHON:
			{
				bPythonConstraint *data= con->data;
				IDP_FreeProperty(data->prop);
				MEM_freeN(data->prop);
			}
				break;
		}
		
		MEM_freeN(con->data);
	}
}

void free_constraints (ListBase *conlist)
{
	bConstraint *con;
	
	/* Do any specific freeing */
	for (con=conlist->first; con; con=con->next) {
		free_constraint_data(con);
	}
	
	/* Free the whole list */
	BLI_freelistN(conlist);
}

void free_constraint_channels (ListBase *chanbase)
{
	bConstraintChannel *chan;
	
	for (chan=chanbase->first; chan; chan=chan->next) {
		if (chan->ipo) {
			chan->ipo->id.us--;
		}
	}
	
	BLI_freelistN(chanbase);
}

void relink_constraints (struct ListBase *list)
{
	bConstraint *con;
	
	for (con = list->first; con; con=con->next) {
		/* check if constraint has a target that needs relinking */
		if (constraint_has_target(con)) {
			Object *tar;
			char *subtarget;
			
			tar = get_constraint_target(con, &subtarget);
			ID_NEW(tar);
		}
	}
}

void copy_constraint_channels (ListBase *dst, ListBase *src)
{
	bConstraintChannel *dchan, *schan;
	
	dst->first=dst->last=NULL;
	duplicatelist(dst, src);
	
	for (dchan=dst->first, schan=src->first; dchan; dchan=dchan->next, schan=schan->next) {
		dchan->ipo = copy_ipo(schan->ipo);
	}
}

void clone_constraint_channels (ListBase *dst, ListBase *src)
{
	bConstraintChannel *dchan, *schan;
	
	dst->first=dst->last=NULL;
	duplicatelist(dst, src);
	
	for (dchan=dst->first, schan=src->first; dchan; dchan=dchan->next, schan=schan->next) {
		id_us_plus((ID *)dchan->ipo);
	}
}

void copy_constraints (ListBase *dst, ListBase *src)
{
	bConstraint *con, *srccon;
	
	dst->first= dst->last= NULL;
	duplicatelist (dst, src);
	
	for (con = dst->first, srccon=src->first; con; srccon=srccon->next, con=con->next) {
		con->data = MEM_dupallocN (con->data);
		
		/* only do specific constraints if required */
		if (con->type == CONSTRAINT_TYPE_PYTHON) {
			bPythonConstraint *pycon = (bPythonConstraint *)con->data;
			bPythonConstraint *opycon = (bPythonConstraint *)srccon->data;
			
			pycon->prop = IDP_CopyProperty(opycon->prop);
		}
	}
}

/* **************** Editor Functions **************** */

char constraint_has_target (bConstraint *con) 
{
	switch (con->type) {
	case CONSTRAINT_TYPE_PYTHON:
		{
			bPythonConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	case CONSTRAINT_TYPE_TRACKTO:
		{
			bTrackToConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	case CONSTRAINT_TYPE_KINEMATIC:
		{
			bKinematicConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	case CONSTRAINT_TYPE_FOLLOWPATH:
		{
			bFollowPathConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	case CONSTRAINT_TYPE_ROTLIKE:
		{
			bRotateLikeConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	case CONSTRAINT_TYPE_LOCLIKE:
		{
			bLocateLikeConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	case CONSTRAINT_TYPE_SIZELIKE:
		{
			bSizeLikeConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	case CONSTRAINT_TYPE_MINMAX:
		{
			bMinMaxConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	case CONSTRAINT_TYPE_ACTION:
		{
			bActionConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	case CONSTRAINT_TYPE_LOCKTRACK:
		{
			bLockTrackConstraint *data = con->data;
			if (data->tar) return 1;
		}
	case CONSTRAINT_TYPE_STRETCHTO:
		{
			bStretchToConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	case CONSTRAINT_TYPE_RIGIDBODYJOINT:
		{
			bRigidBodyJointConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	case CONSTRAINT_TYPE_CLAMPTO:
		{
			bClampToConstraint *data = con->data;
			if (data->tar) return 1;
		}
			break;
	case CONSTRAINT_TYPE_CHILDOF:
		{
			bChildOfConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	case CONSTRAINT_TYPE_TRANSFORM:
		{
			bTransformConstraint *data = con->data;
			if (data->tar) return 1;
		}
		break;
	}
	
	/* Unknown types or CONSTRAINT_TYPE_NULL or no target */
	return 0;
}

Object *get_constraint_target(bConstraint *con, char **subtarget)
{
	/* If the target for this constraint is target, return a pointer 
	 * to the name for this constraints subtarget ... NULL otherwise
	 */
	switch (con->type) {
	case CONSTRAINT_TYPE_PYTHON:
		{
			bPythonConstraint *data=con->data;
			*subtarget = data->subtarget;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_ACTION:
		{
			bActionConstraint *data = con->data;
			*subtarget= data->subtarget;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_LOCLIKE:
		{
			bLocateLikeConstraint *data = con->data;
			*subtarget= data->subtarget;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_ROTLIKE:
		{
			bRotateLikeConstraint *data = con->data;
			*subtarget= data->subtarget;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_SIZELIKE:
		{
			bSizeLikeConstraint *data = con->data;
			*subtarget= data->subtarget;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_KINEMATIC:
		{
			bKinematicConstraint *data = con->data;
			*subtarget= data->subtarget;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_TRACKTO:
		{
			bTrackToConstraint *data = con->data;
			*subtarget= data->subtarget;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_MINMAX:
		{
			bMinMaxConstraint *data = con->data;
			*subtarget= data->subtarget;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_LOCKTRACK:
		{
			bLockTrackConstraint *data = con->data;
			*subtarget= data->subtarget;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_FOLLOWPATH: 
		{
			bFollowPathConstraint *data = con->data;
			*subtarget= NULL;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_STRETCHTO:
		{
			bStretchToConstraint *data = con->data;
			*subtarget= data->subtarget;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_RIGIDBODYJOINT: 
		{
			bRigidBodyJointConstraint *data = con->data;
			*subtarget= NULL;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_CLAMPTO:
		{
			bClampToConstraint *data = con->data;
			*subtarget= NULL;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_CHILDOF:	
		{
			bChildOfConstraint *data = con->data;
			*subtarget= data->subtarget;
			return data->tar;
		}
		break;
	case CONSTRAINT_TYPE_TRANSFORM:	
		{
			bTransformConstraint *data = con->data;
			*subtarget= data->subtarget;
			return data->tar;
		}
		break;
	default:
		*subtarget= NULL;
		break;
	}
	
	return NULL;  
}

void set_constraint_target(bConstraint *con, Object *ob, char *subtarget)
{
	/* Set the target for this constraint */
	switch (con->type) {
		case CONSTRAINT_TYPE_PYTHON:
		{
			bPythonConstraint *data = con->data;
			data->tar= ob;
			if (subtarget) BLI_strncpy(data->subtarget, subtarget, 32);
		}
			break;		
		case CONSTRAINT_TYPE_ACTION:
		{
			bActionConstraint *data = con->data;
			data->tar= ob;
			if (subtarget) BLI_strncpy(data->subtarget, subtarget, 32);
		}
			break;
		case CONSTRAINT_TYPE_LOCLIKE:
		{
			bLocateLikeConstraint *data = con->data;
			data->tar= ob;
			if (subtarget) BLI_strncpy(data->subtarget, subtarget, 32);
		}
			break;
		case CONSTRAINT_TYPE_ROTLIKE:
		{
			bRotateLikeConstraint *data = con->data;
			data->tar= ob;
			if (subtarget) BLI_strncpy(data->subtarget, subtarget, 32);
		}
			break;
		case CONSTRAINT_TYPE_SIZELIKE:
		{
			bSizeLikeConstraint *data = con->data;
			data->tar= ob;
			if (subtarget) BLI_strncpy(data->subtarget, subtarget, 32);
		}
			break;
		case CONSTRAINT_TYPE_KINEMATIC:
		{
			bKinematicConstraint *data = con->data;
			data->tar= ob;
			if (subtarget) BLI_strncpy(data->subtarget, subtarget, 32);
		}
			break;
		case CONSTRAINT_TYPE_TRACKTO:
		{
			bTrackToConstraint *data = con->data;
			data->tar= ob;
			if (subtarget) BLI_strncpy(data->subtarget, subtarget, 32);
		}
			break;
		case CONSTRAINT_TYPE_LOCKTRACK:
		{
			bLockTrackConstraint *data = con->data;
			data->tar= ob;
			if (subtarget) BLI_strncpy(data->subtarget, subtarget, 32);
		}
			break;
		case CONSTRAINT_TYPE_FOLLOWPATH: 
		{
			bFollowPathConstraint *data = con->data;
			data->tar= ob;
		}
			break;
		case CONSTRAINT_TYPE_STRETCHTO:
		{
			bStretchToConstraint *data = con->data;
			data->tar= ob;
			if (subtarget) BLI_strncpy(data->subtarget, subtarget, 32);
		}
			break;
		case CONSTRAINT_TYPE_RIGIDBODYJOINT: 
		{
			bRigidBodyJointConstraint *data = con->data;
			data->tar= ob;
		}
			break;
		case CONSTRAINT_TYPE_MINMAX:
		{
			bMinMaxConstraint *data = (bMinMaxConstraint*)con->data;
			data->tar= ob;
			if (subtarget) BLI_strncpy(data->subtarget, subtarget, 32);
		}
			break;
		case CONSTRAINT_TYPE_CLAMPTO: 
		{
			bClampToConstraint *data = con->data;
			data->tar= ob;
		}
			break;
		case CONSTRAINT_TYPE_CHILDOF:
		{
			bChildOfConstraint *data = con->data;
			data->tar= ob;
			if (subtarget) BLI_strncpy(data->subtarget, subtarget, 32);
		}
			break;
		case CONSTRAINT_TYPE_TRANSFORM:
		{
			bTransformConstraint *data = con->data;
			data->tar= ob;
			if (subtarget) BLI_strncpy(data->subtarget, subtarget, 32);
		}
			break;
	}
}

void unique_constraint_name (bConstraint *con, ListBase *list)
{
	bConstraint *curcon;
	char tempname[64];
	int	number = 1, exists = 0;
	char *dot;
	
	/* See if we are given an empty string */
	if (con->name[0] == '\0') {
		/* give it default name first */
		strcpy(con->name, "Const");
	}
	
	/* See if we even need to do this */
	for (curcon = list->first; curcon; curcon=curcon->next) {
		if (curcon != con) {
			if (!strcmp(curcon->name, con->name)) {
				exists = 1;
				break;
			}
		}
	}
	
	if (!exists)
		return;

	/*	Strip off the suffix */
	dot=strchr(con->name, '.');
	if (dot)
		*dot=0;
	
	for (number = 1; number <= 999; number++) {
		sprintf(tempname, "%s.%03d", con->name, number);
		
		exists = 0;
		for (curcon=list->first; curcon; curcon=curcon->next) {
			if (con!=curcon) {
				if (!strcmp(curcon->name, tempname)) {
					exists = 1;
					break;
				}
			}
		}
		if (!exists) {
			strcpy(con->name, tempname);
			return;
		}
	}
}

void *new_constraint_data (short type)
{
	void *result;
	
	switch (type) {
	case CONSTRAINT_TYPE_PYTHON:
		{
			bPythonConstraint *data;
			data = MEM_callocN(sizeof(bPythonConstraint), "pythonConstraint");
			
			/* everything should be set correctly by calloc, except for the prop->type constant.*/
			data->prop = MEM_callocN(sizeof(IDProperty), "PyConstraintProps");
			data->prop->type = IDP_GROUP;
			
			result = data;
		}
		break;		
	case CONSTRAINT_TYPE_KINEMATIC:
		{
			bKinematicConstraint *data;
			data = MEM_callocN(sizeof(bKinematicConstraint), "kinematicConstraint");

			data->weight= (float)1.0;
			data->orientweight= (float)1.0;
			data->iterations = 500;
			data->flag= CONSTRAINT_IK_TIP|CONSTRAINT_IK_STRETCH|CONSTRAINT_IK_POS;
			
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_TRACKTO:
		{
			bTrackToConstraint *data;
			data = MEM_callocN(sizeof(bTrackToConstraint), "tracktoConstraint");
			
			data->reserved1 = TRACK_Y;
			data->reserved2 = UP_Z;
			
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_MINMAX:
		{
			bMinMaxConstraint *data;
			data = MEM_callocN(sizeof(bMinMaxConstraint), "minmaxConstraint");
			
			data->minmaxflag = TRACK_Z;
			data->offset = 0.0f;
			data->cache[0] = data->cache[1] = data->cache[2] = 0.0f;
			data->flag = 0;
			
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_LOCLIKE:
		{
			bLocateLikeConstraint *data;
			data = MEM_callocN(sizeof(bLocateLikeConstraint), "LocLikeConstraint");
			data->flag = LOCLIKE_X|LOCLIKE_Y|LOCLIKE_Z;
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_ROTLIKE:
		{
			bRotateLikeConstraint *data;
			data = MEM_callocN(sizeof(bRotateLikeConstraint), "RotLikeConstraint");
			data->flag = ROTLIKE_X|ROTLIKE_Y|ROTLIKE_Z;
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_SIZELIKE:
		{
			bSizeLikeConstraint *data;
			data = MEM_callocN(sizeof(bLocateLikeConstraint), "SizeLikeConstraint");
			data->flag = SIZELIKE_X|SIZELIKE_Y|SIZELIKE_Z;
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_ACTION:
		{
			bActionConstraint *data;
			data = MEM_callocN(sizeof(bActionConstraint), "ActionConstraint");
			
			/* set type to 20 (Loc X), as 0 is Rot X for backwards compatability */
			data->type = 20;
			
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_LOCKTRACK:
		{
			bLockTrackConstraint *data;
			data = MEM_callocN(sizeof(bLockTrackConstraint), "locktrackConstraint");
			
			data->trackflag = TRACK_Y;
			data->lockflag = LOCK_Z;
			
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_FOLLOWPATH:
		{
			bFollowPathConstraint *data;
			data = MEM_callocN(sizeof(bFollowPathConstraint), "followpathConstraint");

			data->trackflag = TRACK_Y;
			data->upflag = UP_Z;
			data->offset = 0;
			data->followflag = 0;

			result = data;
		}
		break;
	case CONSTRAINT_TYPE_STRETCHTO:
		{
			bStretchToConstraint *data;
			data = MEM_callocN(sizeof(bStretchToConstraint), "StretchToConstraint");

			data->volmode = 0;
			data->plane = 0;
			data->orglength = 0.0; 
			data->bulge = 1.0;
			result = data;
		}
		break; 
	case CONSTRAINT_TYPE_LOCLIMIT:
		{
			bLocLimitConstraint *data;
			data = MEM_callocN(sizeof(bLocLimitConstraint), "LocLimitConstraint");
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_ROTLIMIT:
		{
			bRotLimitConstraint *data;
			data = MEM_callocN(sizeof(bRotLimitConstraint), "RotLimitConstraint");
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_SIZELIMIT:
		{
			bSizeLimitConstraint *data;
			data = MEM_callocN(sizeof(bSizeLimitConstraint), "SizeLimitConstraint");
			result = data;
		}
		break;
    case CONSTRAINT_TYPE_RIGIDBODYJOINT:
		{
			bRigidBodyJointConstraint *data;
			data = MEM_callocN(sizeof(bRigidBodyJointConstraint), "RigidBodyToConstraint");
			
			// removed code which set target of this constraint  
            data->type=1;
			
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_CLAMPTO:
		{
			bClampToConstraint *data;
			data = MEM_callocN(sizeof(bClampToConstraint), "ClampToConstraint");
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_CHILDOF:
		{
			bChildOfConstraint *data;
			data = MEM_callocN(sizeof(bChildOfConstraint), "ChildOfConstraint");
			
			data->flag = (CHILDOF_LOCX | CHILDOF_LOCY | CHILDOF_LOCZ |
							CHILDOF_ROTX |CHILDOF_ROTY | CHILDOF_ROTZ |
							CHILDOF_SIZEX | CHILDOF_SIZEY | CHILDOF_SIZEZ);
			Mat4One(data->invmat);
			
			result = data;
		}
		break;
	case CONSTRAINT_TYPE_TRANSFORM:
		{
			bTransformConstraint *data;
			data = MEM_callocN(sizeof(bTransformConstraint), "TransformationConstraint");
			
			data->map[0]= 0;
			data->map[1]= 1;
			data->map[2]= 2;
			
			result = data;
		}
		break;
  
   	default:
		result = NULL;
		break;
	}

	return result;
}

bConstraintChannel *get_constraint_channel (ListBase *list, const char *name)
{
	bConstraintChannel *chan;

	for (chan = list->first; chan; chan=chan->next) {
		if (!strcmp(name, chan->name)) {
			return chan;
		}
	}
	return NULL;
}

/* finds or creates new constraint channel */
bConstraintChannel *verify_constraint_channel (ListBase *list, const char *name)
{
	bConstraintChannel *chan;
	
	chan= get_constraint_channel (list, name);
	if(chan==NULL) {
		chan= MEM_callocN(sizeof(bConstraintChannel), "new constraint chan");
		BLI_addtail(list, chan);
		strcpy(chan->name, name);
	}
	
	return chan;
}


/* ***************** Evaluating ********************* */

/* package an object/bone for use in constraint evaluation */
/* This function MEM_calloc's a bConstraintOb struct, that will need to be freed after evaluation */
bConstraintOb *constraints_make_evalob (Object *ob, void *subdata, short datatype)
{
	bConstraintOb *cob;
	
	/* create regardless of whether we have any data! */
	cob= MEM_callocN(sizeof(bConstraintOb), "bConstraintOb");
	
	/* based on type of available data */
	switch (datatype) {
		case TARGET_OBJECT:
		{
			/* disregard subdata... calloc should set other values right */
			if (ob) {
				cob->ob = ob;
				cob->type = datatype;
				Mat4CpyMat4(cob->matrix, ob->obmat);
				Mat4CpyMat4(cob->startmat, ob->obmat);
			}
			else 
				Mat4One(cob->matrix);
		}
			break;
		case TARGET_BONE:
		{
			/* only set if we have valid bone, otherwise default */
			if (ob && subdata) {
				cob->ob = ob;
				cob->pchan = (bPoseChannel *)subdata;
				cob->type = datatype;
				
				/* matrix in world-space */
				Mat4MulMat4 (cob->matrix, cob->pchan->pose_mat, ob->obmat);
				Mat4CpyMat4(cob->startmat, cob->matrix);
			}
			else
				Mat4One(cob->matrix);
				Mat4CpyMat4(cob->startmat, cob->matrix);
		}
			break;
			
		default: // other types not yet handled
			Mat4One(cob->matrix);
			break;
	}
	
	return cob;
}

/* cleanup after constraint evaluation */
void constraints_clear_evalob (bConstraintOb *cob)
{
	float delta[4][4], imat[4][4];
	
	/* prevent crashes */
	if (cob == NULL) 
		return;
	
	/* calculate delta of constraints evaluation */
	Mat4Invert(imat, cob->startmat);
	Mat4MulMat4(delta, cob->matrix, imat);
	
	/* copy matrices back to source */
	switch (cob->type) {
		case TARGET_OBJECT:
		{
			/* copy new ob-matrix back to owner */
			Mat4CpyMat4(cob->ob->obmat, cob->matrix);
			
			/* copy inverse of delta back to owner */
			Mat4Invert(cob->ob->constinv, delta);
		}
			break;
		case TARGET_BONE:
		{
			/* copy new pose-matrix back to owner */
			Mat4MulMat4(cob->pchan->pose_mat, cob->matrix, cob->ob->imat);
			
			/* copy inverse of delta back to owner */
			Mat4Invert(cob->pchan->constinv, delta);
		}
			break;
	}
	
	/* free tempolary struct */
	MEM_freeN(cob);
}

/* -------------------------------- Constraint Channels ---------------------------- */

/* does IPO's of constraint channels only */
void do_constraint_channels (ListBase *conbase, ListBase *chanbase, float ctime)
{
	bConstraint *con;
	bConstraintChannel *chan;
	IpoCurve *icu= NULL;
	
	/* for each Constraint, calculate its Influence from the corresponding ConstraintChannel */
	for (con=conbase->first; con; con=con->next) {
		chan = get_constraint_channel(chanbase, con->name);
		if (chan && chan->ipo) {
			calc_ipo(chan->ipo, ctime);
			for (icu=chan->ipo->curve.first; icu; icu=icu->next) {
				switch (icu->adrcode) {
					case CO_ENFORCE:
					{
						con->enforce = icu->curval;
						if (con->enforce<0.0f) con->enforce= 0.0f;
						else if (con->enforce>1.0f) con->enforce= 1.0f;
					}
						break;
				}
			}
		}
	}
}

/* ------------------------------- Space-Conversion API ---------------------------- */

/* This function is responsible for the correct transformations/conversions 
 * of a matrix from one space to another for constraint evaluation.
 * For now, this is only implemented for Objects and PoseChannels.
 */
static void constraint_mat_convertspace (Object *ob, bPoseChannel *pchan, float mat[][4], short from, short to)
{
	float tempmat[4][4];
	float diff_mat[4][4];
	float imat[4][4];
	
	/* prevent crashes in these unlikely events  */
	if (ob==NULL || mat==NULL) return;
	/* optimise trick - check if need to do anything */
	if (from == to) return;
	
	/* are we dealing with pose-channels or objects */
	if (pchan) {
		/* pose channels */
		switch (from) {
			case CONSTRAINT_SPACE_WORLD: /* ---------- FROM WORLDSPACE ---------- */
			{
				/* world to pose */
				if (to==CONSTRAINT_SPACE_POSE || to==CONSTRAINT_SPACE_LOCAL || to==CONSTRAINT_SPACE_PARLOCAL) {
					Mat4Invert(imat, ob->obmat);
					Mat4CpyMat4(tempmat, mat);
					Mat4MulMat4(mat, tempmat, imat);
				}
				/* pose to local */
				if (to == CONSTRAINT_SPACE_LOCAL) {
					/* call self with slightly different values */
					constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_POSE, to);
				}
				/* pose to local + parent */
				else if (to == CONSTRAINT_SPACE_PARLOCAL) {
					/* call self with slightly different values */
					constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_POSE, to);
				}
			}
				break;
			case CONSTRAINT_SPACE_POSE:	/* ---------- FROM POSESPACE ---------- */
			{
				/* pose to world */
				if (to == CONSTRAINT_SPACE_WORLD) {
					Mat4CpyMat4(tempmat, mat);
					Mat4MulMat4(mat, tempmat, ob->obmat);
				}
				/* pose to local */
				else if (to == CONSTRAINT_SPACE_LOCAL) {
					if (pchan->bone) {
						if (pchan->parent && pchan->parent->bone) {
							float offs_bone[4][4];
								
							/* construct offs_bone the same way it is done in armature.c */
							Mat4CpyMat3(offs_bone, pchan->bone->bone_mat);
							VECCOPY(offs_bone[3], pchan->bone->head);
							offs_bone[3][1]+= pchan->parent->bone->length;
							
							if (pchan->bone->flag & BONE_HINGE) {
								/* pose_mat = par_pose-space_location * chan_mat */
								float tmat[4][4];
								
								/* the rotation of the parent restposition */
								Mat4CpyMat4(tmat, pchan->parent->bone->arm_mat);
								
								/* the location of actual parent transform */
								VECCOPY(tmat[3], offs_bone[3]);
								offs_bone[3][0]= offs_bone[3][1]= offs_bone[3][2]= 0.0f;
								Mat4MulVecfl(pchan->parent->pose_mat, tmat[3]);
								
								Mat4MulMat4(diff_mat, offs_bone, tmat);
								Mat4Invert(imat, diff_mat);
							}
							else {
								/* pose_mat = par_pose_mat * bone_mat * chan_mat */
								Mat4MulMat4(diff_mat, pchan->parent->pose_mat, offs_bone);
								Mat4Invert(imat, diff_mat);
							}
						}
						else {
							/* pose_mat = chan_mat * arm_mat */
							Mat4Invert(imat, pchan->bone->arm_mat);
						}
						
						Mat4CpyMat4(tempmat, mat);
						Mat4MulMat4(mat, tempmat, imat);
					}
				}
				/* pose to local with parent */
				else if (to == CONSTRAINT_SPACE_PARLOCAL) {
					if (pchan->bone) {
						Mat4Invert(imat, pchan->bone->arm_mat);
						Mat4CpyMat4(tempmat, mat);
						Mat4MulMat4(mat, tempmat, imat);
					}
				}
			}
				break;
			case CONSTRAINT_SPACE_LOCAL: /* ------------ FROM LOCALSPACE --------- */
			{
				/* local to pose */
				if (to==CONSTRAINT_SPACE_POSE || to==CONSTRAINT_SPACE_WORLD) {
					/* do inverse procedure that was done for pose to local */
					if (pchan->bone) {
						/* we need the posespace_matrix = local_matrix + (parent_posespace_matrix + restpos) */						
						if (pchan->parent) {
							float offs_bone[4][4];
								
							/* construct offs_bone the same way it is done in armature.c */
							Mat4CpyMat3(offs_bone, pchan->bone->bone_mat);
							VECCOPY(offs_bone[3], pchan->bone->head);
							offs_bone[3][1]+= pchan->parent->bone->length;
							
							Mat4MulMat4(diff_mat, offs_bone, pchan->parent->pose_mat);
							Mat4CpyMat4(tempmat, mat);
							Mat4MulMat4(mat, tempmat, diff_mat);
						}
						else {
							Mat4CpyMat4(diff_mat, pchan->bone->arm_mat);
							
							Mat4CpyMat4(tempmat, mat);
							Mat4MulMat4(mat, tempmat, diff_mat);
						}
					}
				}
				/* local to world */
				if (to == CONSTRAINT_SPACE_WORLD) {
					/* call self with slightly different values */
					constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_POSE, to);
				}
			}
				break;
			case CONSTRAINT_SPACE_PARLOCAL: /* -------------- FROM LOCAL WITH PARENT ---------- */
			{
				/* local to pose */
				if (to==CONSTRAINT_SPACE_POSE || to==CONSTRAINT_SPACE_WORLD) {
					if (pchan->bone) {					
						Mat4CpyMat4(diff_mat, pchan->bone->arm_mat);
						Mat4CpyMat4(tempmat, mat);
						Mat4MulMat4(mat, diff_mat, tempmat);
					}
				}
				/* local to world */
				if (to == CONSTRAINT_SPACE_WORLD) {
					/* call self with slightly different values */
					constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_POSE, to);
				}
			}
				break;
		}
	}
	else {
		/* objects */
		if (from==CONSTRAINT_SPACE_WORLD && to==CONSTRAINT_SPACE_LOCAL) {
			/* check if object has a parent - otherwise this won't work */
			if (ob->parent) {
				/* 'subtract' parent's effects from owner */
				Mat4MulMat4(diff_mat, ob->parentinv, ob->parent->obmat);
				Mat4Invert(imat, diff_mat);
				Mat4CpyMat4(tempmat, mat);
				Mat4MulMat4(mat, tempmat, imat);
			}
		}
		else if (from==CONSTRAINT_SPACE_LOCAL && to==CONSTRAINT_SPACE_WORLD) {
			/* check that object has a parent - otherwise this won't work */
			if (ob->parent) {
				/* 'add' parent's effect back to owner */
				Mat4CpyMat4(tempmat, mat);
				Mat4MulMat4(diff_mat, ob->parentinv, ob->parent->obmat);
				Mat4MulMat4(mat, tempmat, diff_mat);
			}
		}
	}
}

/* ------------------------------- Target ---------------------------- */

/* generic function to get the appropriate matrix for most target cases */
/* The cases where the target can be object data have not been implemented */
static void constraint_target_to_mat4 (Object *ob, const char *substring, float mat[][4], short from, short to)
{
	/*	Case OBJECT */
	if (!strlen(substring)) {
		Mat4CpyMat4(mat, ob->obmat);
		constraint_mat_convertspace(ob, NULL, mat, from, to);
	}
	/* 	Case VERTEXGROUP */
	else if (ELEM(ob->type, OB_MESH, OB_LATTICE)) {
		/* devise a matrix from the data in the vertexgroup */
		/* TODO: will be handled in other files */
	}
	/*	Case BONE */
	else {
		bPoseChannel *pchan;
		
		pchan = get_pose_channel(ob->pose, substring);
		if (pchan) {
			/* Multiply the PoseSpace accumulation/final matrix for this
			 * PoseChannel by the Armature Object's Matrix to get a worldspace
			 * matrix.
			 */
			Mat4MulMat4(mat, pchan->pose_mat, ob->obmat);
		} 
		else
			Mat4CpyMat4(mat, ob->obmat);
			
		/* convert matrix space as required */
		constraint_mat_convertspace(ob, pchan, mat, from, to);
	}
}


/* stupid little cross product function, 0:x, 1:y, 2:z axes */
static int basis_cross(int n, int m)
{
	if(n-m == 1) return 1;
	if(n-m == -1) return -1;
	if(n-m == 2) return -1;
	if(n-m == -2) return 1;
	else return 0;
}

static void vectomat(float *vec, float *target_up, short axis, short upflag, short flags, float m[][3])
{
	float n[3];
	float u[3]; /* vector specifying the up axis */
	float proj[3];
	float right[3];
	float neg = -1;
	int right_index;
	
	VecCopyf(n, vec);
	if(Normalize(n) == 0.0) { 
		n[0] = 0.0;
		n[1] = 0.0;
		n[2] = 1.0;
	}
	if(axis > 2) axis -= 3;
	else VecMulf(n,-1);

	/* n specifies the transformation of the track axis */

	if(flags & TARGET_Z_UP) { 
		/* target Z axis is the global up axis */
		u[0] = target_up[0];
		u[1] = target_up[1];
		u[2] = target_up[2];
	}
	else { 
		/* world Z axis is the global up axis */
		u[0] = 0;
		u[1] = 0;
		u[2] = 1;
	}

	/* project the up vector onto the plane specified by n */
	Projf(proj, u, n); /* first u onto n... */
	VecSubf(proj, u, proj); /* then onto the plane */
	/* proj specifies the transformation of the up axis */

	if(Normalize(proj) == 0.0) { /* degenerate projection */
		proj[0] = 0.0;
		proj[1] = 1.0;
		proj[2] = 0.0;
	}

	/* Normalized cross product of n and proj specifies transformation of the right axis */
	Crossf(right, proj, n);
	Normalize(right);

	if(axis != upflag) {
		right_index = 3 - axis - upflag;
		neg = (float) basis_cross(axis, upflag);

		/* account for up direction, track direction */
		m[right_index][0] = neg * right[0];
		m[right_index][1] = neg * right[1];
		m[right_index][2] = neg * right[2];

		m[upflag][0] = proj[0];
		m[upflag][1] = proj[1];
		m[upflag][2] = proj[2];

		m[axis][0] = n[0];
		m[axis][1] = n[1];
		m[axis][2] = n[2];
	}
	/* identity matrix - don't do anything if the two axes are the same */
	else {
		m[0][0]= m[1][1]= m[2][2]= 1.0;
		m[0][1]= m[0][2]= m[0][3]= 0.0;
		m[1][0]= m[1][2]= m[1][3]= 0.0;
		m[2][0]= m[2][1]= m[2][3]= 0.0;
	}
}


/* called during solve_constraints */
/* also for make_parent, to find correct inverse of "follow path" */
/* warning: ownerdata is PoseChannel or Object */
/* ctime is global time, uncorrected for local bsystem_time */
short get_constraint_target_matrix (bConstraint *con, short ownertype, void *ownerdata, float mat[][4], float ctime)
{
	short valid=0;

	switch (con->type) {
	case CONSTRAINT_TYPE_NULL:
		{
			Mat4One(mat);
		}
		break;
	case CONSTRAINT_TYPE_ACTION:
		{
			if (ownertype == TARGET_BONE) {
				extern void chan_calc_mat(bPoseChannel *chan);
				bActionConstraint *data = (bActionConstraint*)con->data;
				bPose *pose;
				bPoseChannel *pchan, *tchan;
				float tempmat[4][4], vec[3];
				float s, t;
				short axis;
				
				/* initialise return matrix */
				Mat4One(mat);
				
				/* only continue if there is a target  */
				if (data->tar==NULL) return 0;
				
				/* get the transform matrix of the target */
				constraint_target_to_mat4(data->tar, data->subtarget, tempmat, CONSTRAINT_SPACE_WORLD, con->tarspace); // FIXME: change these spaces 
				
				/* determine where in transform range target is */
				/* data->type is mapped as follows for backwards compatability:
				 *	00,01,02	- rotation (it used to be like this)
				 * 	10,11,12	- scaling
				 *	20,21,22	- location
				 */
				if (data->type < 10) {
					/* extract rotation (is in whatever space target should be in) */
					Mat4ToEul(tempmat, vec);
					vec[0] *= (float)(180.0/M_PI);
					vec[1] *= (float)(180.0/M_PI);
					vec[2] *= (float)(180.0/M_PI);
					axis= data->type;
				}
				else if (data->type < 20) {
					/* extract scaling (is in whatever space target should be in) */
					Mat4ToSize(tempmat, vec);
					axis= data->type - 10;
				}
				else {
					/* extract location */
					VECCOPY(vec, tempmat[3]);
					axis= data->type - 20;
				}
				
				/* Target defines the animation */
				s = (vec[axis]-data->min) / (data->max-data->min);
				CLAMP(s, 0, 1);
				t = ( s * (data->end-data->start)) + data->start;
				
				/* Get the appropriate information from the action, we make temp pose */
				pose = MEM_callocN(sizeof(bPose), "pose");
				
				pchan = ownerdata;
				tchan= verify_pose_channel(pose, pchan->name);
				extract_pose_from_action(pose, data->act, t);
				
				chan_calc_mat(tchan);
				
				Mat4CpyMat4(mat, tchan->chan_mat);
				
				/* Clean up */
				free_pose_channels(pose);
				MEM_freeN(pose);
			}
		}
		break;
	case CONSTRAINT_TYPE_LOCLIKE:
		{
			bLocateLikeConstraint *data = (bLocateLikeConstraint*)con->data;
			Object *ob= data->tar;
			
			if (data->tar) {
				if (strlen(data->subtarget)) {
					bPoseChannel *pchan;
					float tmat[4][4];
					
					pchan = get_pose_channel(ob->pose, data->subtarget);
					if (pchan) {
						Mat4CpyMat4(tmat, pchan->pose_mat);
						
						if (data->flag & LOCLIKE_TIP) { 
							VECCOPY(tmat[3], pchan->pose_tail);
						}
							
						Mat4MulMat4(mat, tmat, ob->obmat);
					}
					else 
						Mat4CpyMat4(mat, ob->obmat);
						
					/* convert matrix space as required  */
					constraint_mat_convertspace(ob, pchan, mat, CONSTRAINT_SPACE_WORLD, con->tarspace);
				}
				else {
					Mat4CpyMat4(mat, ob->obmat);
					
					/* convert matrix space as required */
					constraint_mat_convertspace(ob, NULL, mat, CONSTRAINT_SPACE_WORLD, con->tarspace);
				}
				valid=1;
			}
			else
				Mat4One(mat);
		} 
		break;
	case CONSTRAINT_TYPE_ROTLIKE:
		{
			bRotateLikeConstraint *data;
			data = (bRotateLikeConstraint*)con->data;
			
			if (data->tar) {
				constraint_target_to_mat4(data->tar, data->subtarget, mat, CONSTRAINT_SPACE_WORLD, con->tarspace);
				valid=1;
			}
			else
				Mat4One(mat);
		} 
		break;
	case CONSTRAINT_TYPE_SIZELIKE:
		{
			bSizeLikeConstraint *data;
			data = (bSizeLikeConstraint*)con->data;
			
			if (data->tar) {
				constraint_target_to_mat4(data->tar, data->subtarget, mat, CONSTRAINT_SPACE_WORLD, con->tarspace);
				valid=1;
			}
			else
				Mat4One(mat);
		} 
		break;
	case CONSTRAINT_TYPE_MINMAX:
		{
			bMinMaxConstraint *data = (bMinMaxConstraint*)con->data;
			
			if (data->tar) {
				constraint_target_to_mat4(data->tar, data->subtarget, mat, CONSTRAINT_SPACE_WORLD, con->tarspace);
				valid=1;
			}
			else
				Mat4One(mat);
		} 
		break;
	case CONSTRAINT_TYPE_TRACKTO:
		{
			bTrackToConstraint *data;
			data = (bTrackToConstraint*)con->data;
			
			if (data->tar) {
				constraint_target_to_mat4(data->tar, data->subtarget, mat, CONSTRAINT_SPACE_WORLD, con->tarspace);
				valid=1;
			}
			else
				Mat4One (mat);
		}
		break;
	case CONSTRAINT_TYPE_KINEMATIC:
		{
			bKinematicConstraint *data;
			data = (bKinematicConstraint*)con->data;
			
			if (data->tar) {
				constraint_target_to_mat4(data->tar, data->subtarget, mat, CONSTRAINT_SPACE_WORLD, con->tarspace);
				valid=1;
			}
			else if (data->flag & CONSTRAINT_IK_AUTO) {
				Object *ob= (Object *)ownerdata;
				
				if (ob==NULL)
					Mat4One(mat);
				else {
					float vec[3];
					/* move grabtarget into world space */
					VECCOPY(vec, data->grabtarget);
					Mat4MulVecfl(ob->obmat, vec);
					Mat4CpyMat4(mat, ob->obmat);
					VECCOPY(mat[3], vec);
				}
			}
			else
				Mat4One(mat);
		} 
		break;
	case CONSTRAINT_TYPE_LOCKTRACK:
		{
			bLockTrackConstraint *data;
			data = (bLockTrackConstraint*)con->data;
			
			if (data->tar) {
				constraint_target_to_mat4(data->tar, data->subtarget, mat, CONSTRAINT_SPACE_WORLD, con->tarspace); 
				valid=1;
			}
			else
				Mat4One(mat);
		} 
		break;
	case CONSTRAINT_TYPE_FOLLOWPATH:
		{
			bFollowPathConstraint *data;
			data = (bFollowPathConstraint*)con->data;
			
			if (data->tar) {
				Curve *cu;
				float q[4], vec[4], dir[3], *quat, x1;
				float totmat[4][4];
				float curvetime;
				
				Mat4One(totmat);
				Mat4One(mat);
				
				cu= data->tar->data;
				
				/* note: when creating constraints that follow path, the curve gets the CU_PATH set now,
					currently for paths to work it needs to go through the bevlist/displist system (ton) */
				
				if (cu->path==NULL || cu->path->data==NULL) /* only happens on reload file, but violates depsgraph still... fix! */
					makeDispListCurveTypes(data->tar, 0);
				if (cu->path && cu->path->data) {
					curvetime= bsystem_time(data->tar, data->tar->parent, (float)ctime, 0.0) - data->offset;
					
					if (calc_ipo_spec(cu->ipo, CU_SPEED, &curvetime)==0) {
						curvetime /= cu->pathlen;
						CLAMP(curvetime, 0.0, 1.0);
					}
					
					if (where_on_path(data->tar, curvetime, vec, dir) ) {
						if (data->followflag) {
							quat= vectoquat(dir, (short) data->trackflag, (short) data->upflag);
							
							Normalize(dir);
							q[0]= (float)cos(0.5*vec[3]);
							x1= (float)sin(0.5*vec[3]);
							q[1]= -x1*dir[0];
							q[2]= -x1*dir[1];
							q[3]= -x1*dir[2];
							QuatMul(quat, q, quat);
							
							QuatToMat4(quat, totmat);
						}
						VECCOPY(totmat[3], vec);
						
						Mat4MulSerie(mat, data->tar->obmat, totmat, NULL, NULL, NULL, NULL, NULL, NULL);
					}
				}
				valid=1;
			}
			else
				Mat4One(mat);
		}
		break;
	case CONSTRAINT_TYPE_STRETCHTO:
		{
			bStretchToConstraint *data;
			data = (bStretchToConstraint*)con->data;
			
			if (data->tar) {
				constraint_target_to_mat4(data->tar, data->subtarget, mat, CONSTRAINT_SPACE_WORLD, con->tarspace);
				valid = 1;
			}
			else
				Mat4One(mat);
		}
		break;
	case CONSTRAINT_TYPE_PYTHON:
		{
			bPythonConstraint *data;
			data = (bPythonConstraint*)con->data;
			
			/* special exception for curves - depsgraph issues */
			if (data->tar && data->tar->type == OB_CURVE) {
				Curve *cu= data->tar->data;
				
				/* this check is to make sure curve objects get updated on file load correctly.*/
				if (cu->path==NULL || cu->path->data==NULL) /* only happens on reload file, but violates depsgraph still... fix! */
					makeDispListCurveTypes(data->tar, 0);				
			}
			
			/* if the script doesn't set the target matrix for any reason, fall back to standard methods */
			if (BPY_pyconstraint_targets(data, mat) < 1) {
				if (data->tar) {
					constraint_target_to_mat4(data->tar, data->subtarget, mat, CONSTRAINT_SPACE_WORLD, con->tarspace);
					valid = 1;
				}
				else
					Mat4One(mat);
			}
		}
		break;
	case CONSTRAINT_TYPE_CLAMPTO:
		{
			bClampToConstraint *data;
			data = (bClampToConstraint*)con->data;
			
			if (data->tar) {
				Curve *cu= data->tar->data;
				
				/* note; when creating constraints that follow path, the curve gets the CU_PATH set now,
					currently for paths to work it needs to go through the bevlist/displist system (ton) */
				
				if (cu->path==NULL || cu->path->data==NULL) /* only happens on reload file, but violates depsgraph still... fix! */
					makeDispListCurveTypes(data->tar, 0);
				
				valid = 1;
			}
			
			Mat4One(mat);
		}
		break;
	case CONSTRAINT_TYPE_CHILDOF:
		{
			bChildOfConstraint *data;
			data= (bChildOfConstraint *)con->data;
			
			if (data->tar) {
				constraint_target_to_mat4(data->tar, data->subtarget, mat, CONSTRAINT_SPACE_WORLD, con->tarspace);
				valid = 1;
			}
			else
				Mat4One(mat);
		}
		break;
	case CONSTRAINT_TYPE_TRANSFORM:
		{
			bTransformConstraint *data;
			data= (bTransformConstraint *)con->data;
			
			if (data->tar) {
				constraint_target_to_mat4(data->tar, data->subtarget, mat, CONSTRAINT_SPACE_WORLD, con->tarspace);
				valid = 1;
			}
			else
				Mat4One(mat);
		}
		break;

	default:
		Mat4One(mat);
		break;
	}

	return valid;
}

/* ---------------------------------------------- Constraint Evaluation ------------------------------------------------- */

/* This is only called during solve_constraints to solve a particular constraint.
 * It works on ownermat, and uses targetmat to help accomplish its tasks.
 */
static void evaluate_constraint (bConstraint *constraint, float ownermat[][4], float targetmat[][4])
{
	if (constraint == NULL || constraint->data == NULL)
		return;
		
	switch (constraint->type) {
	case CONSTRAINT_TYPE_NULL:
	case CONSTRAINT_TYPE_KINEMATIC: /* removed */
		break;
	case CONSTRAINT_TYPE_PYTHON:
		{
			bPythonConstraint *data;
			
			data = constraint->data;
			BPY_pyconstraint_eval(data, ownermat, targetmat);
		} 
		break;
	case CONSTRAINT_TYPE_ACTION:
		{
			bActionConstraint *data;
			float temp[4][4];
			
			data = constraint->data;
			Mat4CpyMat4(temp, ownermat);
			
			Mat4MulMat4(ownermat, targetmat, temp);
		}
		break;
	case CONSTRAINT_TYPE_LOCLIKE:
		{
			bLocateLikeConstraint *data;
			float offset[3] = {0.0f, 0.0f, 0.0f};

			data = constraint->data;
			
			if (data->flag & LOCLIKE_OFFSET)
				VECCOPY(offset, ownermat[3]);
			
			if (data->flag & LOCLIKE_X) {
				ownermat[3][0] = targetmat[3][0];
				
				if(data->flag & LOCLIKE_X_INVERT) ownermat[3][0] *= -1;
				ownermat[3][0] += offset[0];
			}
			if (data->flag & LOCLIKE_Y) {
				ownermat[3][1] = targetmat[3][1];
				
				if(data->flag & LOCLIKE_Y_INVERT) ownermat[3][1] *= -1;
				ownermat[3][1] += offset[1];
			}
			if (data->flag & LOCLIKE_Z) {
				ownermat[3][2] = targetmat[3][2];
				
				if(data->flag & LOCLIKE_Z_INVERT) ownermat[3][2] *= -1;
				ownermat[3][2] += offset[2];
			}
		}
		break;
	case CONSTRAINT_TYPE_ROTLIKE:
		{
			bRotateLikeConstraint *data;
			float	loc[3];
			float	eul[3], obeul[3];
			float	size[3];
			short changed= 0;
			
			data = constraint->data;
			
			VECCOPY(loc, ownermat[3]);
			Mat4ToSize(ownermat, size);
			
			Mat4ToEul(targetmat, eul);
			Mat4ToEul(ownermat, obeul);
			
			if ((data->flag & ROTLIKE_X)==0) {
				eul[0] = obeul[0];
				changed = 1;
			}
			else if (data->flag & ROTLIKE_X_INVERT) {
				eul[0] *= -1;
				changed = 1;
			}	
			if ((data->flag & ROTLIKE_Y)==0) {
				eul[1] = obeul[1];
				changed = 1;
			}
			else if (data->flag & ROTLIKE_Y_INVERT) {
				eul[1] *= -1;
				changed = 1;
			}
			if ((data->flag & ROTLIKE_Z)==0) {
				eul[2] = obeul[2];
				changed = 1;
			}
			else if (data->flag & ROTLIKE_Z_INVERT) {
				eul[2] *= -1;
				changed = 1;
			}
			
			
			if (changed) {
				compatible_eul(eul, obeul);
				LocEulSizeToMat4(ownermat, loc, eul, size);
			}
			else {
				float quat[4];
				
				Mat4ToQuat(targetmat, quat);
				LocQuatSizeToMat4(ownermat, loc, quat, size);
			}	
		}
		break;
 	case CONSTRAINT_TYPE_SIZELIKE:
 		{
 			bSizeLikeConstraint *data;
			float obsize[3], size[3];
			
 			data = constraint->data;
 
 			Mat4ToSize(targetmat, size);
 			Mat4ToSize(ownermat, obsize);
 			
 			if ((data->flag & SIZELIKE_X) && obsize[0] != 0)
 				VecMulf(ownermat[0], size[0] / obsize[0]);
 			if ((data->flag & SIZELIKE_Y) && obsize[1] != 0)
 				VecMulf(ownermat[1], size[1] / obsize[1]);
 			if ((data->flag & SIZELIKE_Z) && obsize[2] != 0)
 				VecMulf(ownermat[2], size[2] / obsize[2]);
  		}
  		break;
	case CONSTRAINT_TYPE_MINMAX:
		{
			bMinMaxConstraint *data;
			float obmat[4][4], imat[4][4], tarmat[4][4], tmat[4][4];
			float val1, val2;
			int index;
			
			data = constraint->data;
			
			Mat4CpyMat4(obmat, ownermat);
			Mat4CpyMat4(tarmat, targetmat);
			
			if (data->flag & MINMAX_USEROT) {
				/* take rotation of target into account by doing the transaction in target's localspace */
				Mat4Invert(imat, tarmat);
				Mat4MulMat4(tmat, obmat, imat);
				Mat4CpyMat4(obmat, tmat);
				Mat4One(tarmat);
			}
			
			switch (data->minmaxflag) {
			case TRACK_Z:
				val1 = tarmat[3][2];
				val2 = obmat[3][2]-data->offset;
				index = 2;
				break;
			case TRACK_Y:
				val1 = tarmat[3][1];
				val2 = obmat[3][1]-data->offset;
				index = 1;
				break;
			case TRACK_X:
				val1 = tarmat[3][0];
				val2 = obmat[3][0]-data->offset;
				index = 0;
				break;
			case TRACK_nZ:
				val2 = tarmat[3][2];
				val1 = obmat[3][2]-data->offset;
				index = 2;
				break;
			case TRACK_nY:
				val2 = tarmat[3][1];
				val1 = obmat[3][1]-data->offset;
				index = 1;
				break;
			case TRACK_nX:
				val2 = tarmat[3][0];
				val1 = obmat[3][0]-data->offset;
				index = 0;
				break;
			default:
				return;
			}
			
			if (val1 > val2) {
				obmat[3][index] = tarmat[3][index] + data->offset;
				if (data->flag & MINMAX_STICKY) {
					if (data->flag & MINMAX_STUCK) {
						VECCOPY(obmat[3], data->cache);
					} 
					else {
						VECCOPY(data->cache, obmat[3]);
						data->flag |= MINMAX_STUCK;
					}
				}
				if (data->flag & MINMAX_USEROT) {
					/* get out of localspace */
					Mat4MulMat4(tmat, obmat, targetmat);
					Mat4CpyMat4(ownermat, tmat);
				} 
				else {			
					VECCOPY(ownermat[3], obmat[3]);
				}
			} 
			else {
				data->flag &= ~MINMAX_STUCK;
			}
		}
		break;
	case CONSTRAINT_TYPE_TRACKTO:
		{
			bTrackToConstraint *data;
			float size[3], vec[3];
			float totmat[3][3];
			float tmat[4][4];

			data = constraint->data;			
			
			if (data->tar) {
				/* Get size property, since ob->size is only the object's own relative size, not its global one */
				Mat4ToSize (ownermat, size);
				
				/* Clear the object's rotation */ 	
				ownermat[0][0]=size[0];
				ownermat[0][1]=0;
				ownermat[0][2]=0;
				ownermat[1][0]=0;
				ownermat[1][1]=size[1];
				ownermat[1][2]=0;
				ownermat[2][0]=0;
				ownermat[2][1]=0;
				ownermat[2][2]=size[2];
				
				VecSubf(vec, ownermat[3], targetmat[3]);
				vectomat(vec, ownermat[2], 
						(short)data->reserved1, (short)data->reserved2, 
						data->flags, totmat);
				
				Mat4CpyMat4(tmat, ownermat);
				Mat4MulMat34(ownermat, totmat, tmat);
			}
		}
		break;
	case CONSTRAINT_TYPE_LOCKTRACK:
		{
			bLockTrackConstraint *data;
			float vec[3],vec2[3];
			float totmat[3][3];
			float tmpmat[3][3];
			float invmat[3][3];
			float tmat[4][4];
			float mdet;

			data = constraint->data;			
			
			if (data->tar) {
				/* Vector object -> target */
				VecSubf(vec, targetmat[3], ownermat[3]);
				switch (data->lockflag){
				case LOCK_X: /* LOCK X */
				{
					switch (data->trackflag) {
						case TRACK_Y: /* LOCK X TRACK Y */
						{
							/* Projection of Vector on the plane */
							Projf(vec2, vec, ownermat[0]);
							VecSubf(totmat[1], vec, vec2);
							Normalize(totmat[1]);
							
							/* the x axis is fixed */
							totmat[0][0] = ownermat[0][0];
							totmat[0][1] = ownermat[0][1];
							totmat[0][2] = ownermat[0][2];
							Normalize(totmat[0]);
							
							/* the z axis gets mapped onto a third orthogonal vector */
							Crossf(totmat[2], totmat[0], totmat[1]);
						}
							break;
						case TRACK_Z: /* LOCK X TRACK Z */
						{
							/* Projection of Vector on the plane */
							Projf(vec2, vec, ownermat[0]);
							VecSubf(totmat[2], vec, vec2);
							Normalize(totmat[2]);

							/* the x axis is fixed */
							totmat[0][0] = ownermat[0][0];
							totmat[0][1] = ownermat[0][1];
							totmat[0][2] = ownermat[0][2];
							Normalize(totmat[0]);
					
							/* the z axis gets mapped onto a third orthogonal vector */
							Crossf(totmat[1], totmat[2], totmat[0]);
						}
							break;
						case TRACK_nY: /* LOCK X TRACK -Y */
						{
							/* Projection of Vector on the plane */
							Projf(vec2, vec, ownermat[0]);
							VecSubf(totmat[1], vec, vec2);
							Normalize(totmat[1]);
							VecMulf(totmat[1],-1);
							
							/* the x axis is fixed */
							totmat[0][0] = ownermat[0][0];
							totmat[0][1] = ownermat[0][1];
							totmat[0][2] = ownermat[0][2];
							Normalize(totmat[0]);
							
							/* the z axis gets mapped onto a third orthogonal vector */
							Crossf(totmat[2], totmat[0], totmat[1]);
						}
							break;
						case TRACK_nZ: /* LOCK X TRACK -Z */
						{
							/* Projection of Vector on the plane */
							Projf(vec2, vec, ownermat[0]);
							VecSubf(totmat[2], vec, vec2);
							Normalize(totmat[2]);
							VecMulf(totmat[2],-1);
								
							/* the x axis is fixed */
							totmat[0][0] = ownermat[0][0];
							totmat[0][1] = ownermat[0][1];
							totmat[0][2] = ownermat[0][2];
							Normalize(totmat[0]);
								
							/* the z axis gets mapped onto a third orthogonal vector */
							Crossf(totmat[1], totmat[2], totmat[0]);
						}
							break;
						default:
						{
							totmat[0][0] = 1;totmat[0][1] = 0;totmat[0][2] = 0;
							totmat[1][0] = 0;totmat[1][1] = 1;totmat[1][2] = 0;
							totmat[2][0] = 0;totmat[2][1] = 0;totmat[2][2] = 1;
						}
							break;
					}
				}
					break;
				case LOCK_Y: /* LOCK Y */
				{
					switch (data->trackflag) {
						case TRACK_X: /* LOCK Y TRACK X */
						{
							/* Projection of Vector on the plane */
							Projf(vec2, vec, ownermat[1]);
							VecSubf(totmat[0], vec, vec2);
							Normalize(totmat[0]);
							
							/* the y axis is fixed */
							totmat[1][0] = ownermat[1][0];
							totmat[1][1] = ownermat[1][1];
							totmat[1][2] = ownermat[1][2];
							Normalize(totmat[1]);
							
							/* the z axis gets mapped onto a third orthogonal vector */
							Crossf(totmat[2], totmat[0], totmat[1]);
						}
							break;
						case TRACK_Z: /* LOCK Y TRACK Z */
						{
							/* Projection of Vector on the plane */
							Projf(vec2, vec, ownermat[1]);
							VecSubf(totmat[2], vec, vec2);
							Normalize(totmat[2]);

							/* the y axis is fixed */
							totmat[1][0] = ownermat[1][0];
							totmat[1][1] = ownermat[1][1];
							totmat[1][2] = ownermat[1][2];
							Normalize(totmat[1]);
							
							/* the z axis gets mapped onto a third orthogonal vector */
							Crossf(totmat[0], totmat[1], totmat[2]);
						}
							break;
						case TRACK_nX: /* LOCK Y TRACK -X */
						{
							/* Projection of Vector on the plane */
							Projf(vec2, vec, ownermat[1]);
							VecSubf(totmat[0], vec, vec2);
							Normalize(totmat[0]);
							VecMulf(totmat[0],-1);

							/* the y axis is fixed */
							totmat[1][0] = ownermat[1][0];
							totmat[1][1] = ownermat[1][1];
							totmat[1][2] = ownermat[1][2];
							Normalize(totmat[1]);
							
							/* the z axis gets mapped onto a third orthogonal vector */
							Crossf(totmat[2], totmat[0], totmat[1]);
						}
							break;
						case TRACK_nZ: /* LOCK Y TRACK -Z */
						{
							/* Projection of Vector on the plane */
							Projf(vec2, vec, ownermat[1]);
							VecSubf(totmat[2], vec, vec2);
							Normalize(totmat[2]);
							VecMulf(totmat[2],-1);

							/* the y axis is fixed */
							totmat[1][0] = ownermat[1][0];
							totmat[1][1] = ownermat[1][1];
							totmat[1][2] = ownermat[1][2];
							Normalize(totmat[1]);
							
							/* the z axis gets mapped onto a third orthogonal vector */
							Crossf(totmat[0], totmat[1], totmat[2]);
						}
							break;
						default:
						{
							totmat[0][0] = 1;totmat[0][1] = 0;totmat[0][2] = 0;
							totmat[1][0] = 0;totmat[1][1] = 1;totmat[1][2] = 0;
							totmat[2][0] = 0;totmat[2][1] = 0;totmat[2][2] = 1;
						}
							break;
					}
				}
					break;
				case LOCK_Z: /* LOCK Z */
				{
					switch (data->trackflag) {
						case TRACK_X: /* LOCK Z TRACK X */
						{
							/* Projection of Vector on the plane */
							Projf(vec2, vec, ownermat[2]);
							VecSubf(totmat[0], vec, vec2);
							Normalize(totmat[0]);
							
							/* the z axis is fixed */
							totmat[2][0] = ownermat[2][0];
							totmat[2][1] = ownermat[2][1];
							totmat[2][2] = ownermat[2][2];
							Normalize(totmat[2]);
							
							/* the x axis gets mapped onto a third orthogonal vector */
							Crossf(totmat[1], totmat[2], totmat[0]);
						}
							break;
						case TRACK_Y: /* LOCK Z TRACK Y */
						{
							/* Projection of Vector on the plane */
							Projf(vec2, vec, ownermat[2]);
							VecSubf(totmat[1], vec, vec2);
							Normalize(totmat[1]);
							
							/* the z axis is fixed */
							totmat[2][0] = ownermat[2][0];
							totmat[2][1] = ownermat[2][1];
							totmat[2][2] = ownermat[2][2];
							Normalize(totmat[2]);
								
							/* the x axis gets mapped onto a third orthogonal vector */
							Crossf(totmat[0], totmat[1], totmat[2]);
						}
							break;
						case TRACK_nX: /* LOCK Z TRACK -X */
						{
							/* Projection of Vector on the plane */
							Projf(vec2, vec, ownermat[2]);
							VecSubf(totmat[0], vec, vec2);
							Normalize(totmat[0]);
							VecMulf(totmat[0],-1);
							
							/* the z axis is fixed */
							totmat[2][0] = ownermat[2][0];
							totmat[2][1] = ownermat[2][1];
							totmat[2][2] = ownermat[2][2];
							Normalize(totmat[2]);
							
							/* the x axis gets mapped onto a third orthogonal vector */
							Crossf(totmat[1], totmat[2], totmat[0]);
						}
							break;
						case TRACK_nY: /* LOCK Z TRACK -Y */
						{
							/* Projection of Vector on the plane */
							Projf(vec2, vec, ownermat[2]);
							VecSubf(totmat[1], vec, vec2);
							Normalize(totmat[1]);
							VecMulf(totmat[1],-1);
							
							/* the z axis is fixed */
							totmat[2][0] = ownermat[2][0];
							totmat[2][1] = ownermat[2][1];
							totmat[2][2] = ownermat[2][2];
							Normalize(totmat[2]);
								
							/* the x axis gets mapped onto a third orthogonal vector */
							Crossf(totmat[0], totmat[1], totmat[2]);
						}
							break;
						default:
						{
								totmat[0][0] = 1;totmat[0][1] = 0;totmat[0][2] = 0;
								totmat[1][0] = 0;totmat[1][1] = 1;totmat[1][2] = 0;
								totmat[2][0] = 0;totmat[2][1] = 0;totmat[2][2] = 1;
						}
							break;
					}
				}
					break;
				default:
					{
						totmat[0][0] = 1;totmat[0][1] = 0;totmat[0][2] = 0;
						totmat[1][0] = 0;totmat[1][1] = 1;totmat[1][2] = 0;
						totmat[2][0] = 0;totmat[2][1] = 0;totmat[2][2] = 1;
					}
					break;
				}
				/* Block to keep matrix heading */
				tmpmat[0][0] = ownermat[0][0];tmpmat[0][1] = ownermat[0][1];tmpmat[0][2] = ownermat[0][2];
				tmpmat[1][0] = ownermat[1][0];tmpmat[1][1] = ownermat[1][1];tmpmat[1][2] = ownermat[1][2];
				tmpmat[2][0] = ownermat[2][0];tmpmat[2][1] = ownermat[2][1];tmpmat[2][2] = ownermat[2][2];
				Normalize(tmpmat[0]);
				Normalize(tmpmat[1]);
				Normalize(tmpmat[2]);
				Mat3Inv(invmat,tmpmat);
				Mat3MulMat3(tmpmat, totmat, invmat);
				totmat[0][0] = tmpmat[0][0];totmat[0][1] = tmpmat[0][1];totmat[0][2] = tmpmat[0][2];
				totmat[1][0] = tmpmat[1][0];totmat[1][1] = tmpmat[1][1];totmat[1][2] = tmpmat[1][2];
				totmat[2][0] = tmpmat[2][0];totmat[2][1] = tmpmat[2][1];totmat[2][2] = tmpmat[2][2];

				Mat4CpyMat4(tmat, ownermat);

				mdet = Det3x3(	totmat[0][0],totmat[0][1],totmat[0][2],
								totmat[1][0],totmat[1][1],totmat[1][2],
								totmat[2][0],totmat[2][1],totmat[2][2]);
				if (mdet==0)
				{
					totmat[0][0] = 1;totmat[0][1] = 0;totmat[0][2] = 0;
					totmat[1][0] = 0;totmat[1][1] = 1;totmat[1][2] = 0;
					totmat[2][0] = 0;totmat[2][1] = 0;totmat[2][2] = 1;
				}

				/* apply out transformaton to the object */
				Mat4MulMat34(ownermat, totmat, tmat);
			}
		}
		break;
	case CONSTRAINT_TYPE_FOLLOWPATH:
		{
			bFollowPathConstraint *data;
			float obmat[4][4];
			float size[3], obsize[3];
			
			data = constraint->data;			
			
			if (data->tar) {
				/* get Object local transform (loc/rot/size) to determine transformation from path */
				//object_to_mat4(ob, obmat);
				Mat4CpyMat4(obmat, ownermat); // FIXME!!!
				
				/* get scaling of object before applying constraint */
				Mat4ToSize(ownermat, size);
				
				/* apply targetmat - containing location on path, and rotation */
 				Mat4MulSerie(ownermat, targetmat, obmat, NULL, NULL, NULL, NULL, NULL, NULL);
				
				/* un-apply scaling caused by path */
				Mat4ToSize(ownermat, obsize);
				if (obsize[0] != 0)
	 				VecMulf(ownermat[0], size[0] / obsize[0]);
	 			if (obsize[1] != 0)
	 				VecMulf(ownermat[1], size[1] / obsize[1]);
	 			if (obsize[2] != 0)
	 				VecMulf(ownermat[2], size[2] / obsize[2]);
			}
		}
		break;
	case CONSTRAINT_TYPE_STRETCHTO:
        {
            bStretchToConstraint *data;
            float size[3],scale[3],vec[3],xx[3],zz[3],orth[3];
            float totmat[3][3];
            float tmat[4][4];
            float dist;
			
            data = constraint->data;            
            Mat4ToSize (ownermat, size);
            
            if (data->tar) {
                /* store X orientation before destroying obmat */
                xx[0] = ownermat[0][0];
                xx[1] = ownermat[0][1];
                xx[2] = ownermat[0][2];
                Normalize(xx);
				
                /* store Z orientation before destroying obmat */
                zz[0] = ownermat[2][0];
                zz[1] = ownermat[2][1];
                zz[2] = ownermat[2][2];
                Normalize(zz);
				
				VecSubf(vec, ownermat[3], targetmat[3]);
				vec[0] /= size[0];
				vec[1] /= size[1];
				vec[2] /= size[2];

				dist = Normalize(vec);
                //dist = VecLenf( ob->obmat[3], targetmat[3]);

                if (data->orglength == 0)  data->orglength = dist;
                if (data->bulge ==0) data->bulge = 1.0;

                scale[1] = dist/data->orglength;
                switch (data->volmode) {
                /* volume preserving scaling */
                case VOLUME_XZ :
                    scale[0] = 1.0f - (float)sqrt(data->bulge) + (float)sqrt(data->bulge*(data->orglength/dist));
                    scale[2] = scale[0];
                    break;
                case VOLUME_X:
                    scale[0] = 1.0f + data->bulge * (data->orglength /dist - 1);
                    scale[2] = 1.0;
                    break;
                case VOLUME_Z:
                    scale[0] = 1.0;
                    scale[2] = 1.0f + data->bulge * (data->orglength /dist - 1);
                    break;
                    /* don't care for volume */
                case NO_VOLUME:
                    scale[0] = 1.0;
                    scale[2] = 1.0;
                    break;
                default: /* should not happen, but in case*/
                    return;    
                } /* switch (data->volmode) */

                /* Clear the object's rotation and scale */
				ownermat[0][0]=size[0]*scale[0];
				ownermat[0][1]=0;
				ownermat[0][2]=0;
				ownermat[1][0]=0;
				ownermat[1][1]=size[1]*scale[1];
				ownermat[1][2]=0;
				ownermat[2][0]=0;
				ownermat[2][1]=0;
				ownermat[2][2]=size[2]*scale[2];
                
				VecSubf(vec, ownermat[3], targetmat[3]);
				Normalize(vec);
				
                /* new Y aligns  object target connection*/
                totmat[1][0] = -vec[0];
                totmat[1][1] = -vec[1];
                totmat[1][2] = -vec[2];
                switch (data->plane) {
                case PLANE_X:
                    /* build new Z vector */
                    /* othogonal to "new Y" "old X! plane */
                    Crossf(orth, vec, xx);
                    Normalize(orth);
                    
                    /* new Z*/
                    totmat[2][0] = orth[0];
                    totmat[2][1] = orth[1];
                    totmat[2][2] = orth[2];
                    
                    /* we decided to keep X plane*/
                    Crossf(xx, orth, vec);
                    Normalize(xx);
                    totmat[0][0] = xx[0];
                    totmat[0][1] = xx[1];
                    totmat[0][2] = xx[2];
                    break;
                case PLANE_Z:
                    /* build new X vector */
                    /* othogonal to "new Y" "old Z! plane */
                    Crossf(orth, vec, zz);
                    Normalize(orth);
                    
                    /* new X */
                    totmat[0][0] = -orth[0];
                    totmat[0][1] = -orth[1];
                    totmat[0][2] = -orth[2];
                    
                    /* we decided to keep Z */
                    Crossf(zz, orth, vec);
                    Normalize(zz);
                    totmat[2][0] = zz[0];
                    totmat[2][1] = zz[1];
                    totmat[2][2] = zz[2];
                    break;
                } /* switch (data->plane) */
                
                Mat4CpyMat4(tmat, ownermat);
				
                Mat4MulMat34(ownermat, totmat, tmat);
            }
        }
        break;
	case CONSTRAINT_TYPE_LOCLIMIT:
		{
			bLocLimitConstraint *data;

			data = constraint->data;
			
			if (data->flag & LIMIT_XMIN) {
				if(ownermat[3][0] < data->xmin)
					ownermat[3][0] = data->xmin;
			}
			if (data->flag & LIMIT_XMAX) {
				if (ownermat[3][0] > data->xmax)
					ownermat[3][0] = data->xmax;
			}
			if (data->flag & LIMIT_YMIN) {
				if(ownermat[3][1] < data->ymin)
					ownermat[3][1] = data->ymin;
			}
			if (data->flag & LIMIT_YMAX) {
				if (ownermat[3][1] > data->ymax)
					ownermat[3][1] = data->ymax;
			}
			if (data->flag & LIMIT_ZMIN) {
				if(ownermat[3][2] < data->zmin) 
					ownermat[3][2] = data->zmin;
			}
			if (data->flag & LIMIT_ZMAX) {
				if (ownermat[3][2] > data->zmax)
					ownermat[3][2] = data->zmax;
			}
		}
		break;
	case CONSTRAINT_TYPE_ROTLIMIT:
		{
			bRotLimitConstraint *data;
			float loc[3];
			float eul[3];
			float size[3];
			
			data = constraint->data;
			
			VECCOPY(loc, ownermat[3]);
			Mat4ToSize(ownermat, size);
			
			Mat4ToEul(ownermat, eul);
			
			/* eulers: radians to degrees! */
			eul[0] = (eul[0] / M_PI * 180);
			eul[1] = (eul[1] / M_PI * 180);
			eul[2] = (eul[2] / M_PI * 180);
			
			/* limiting of euler values... */
			if (data->flag & LIMIT_XROT) {
				if (eul[0] < data->xmin) 
					eul[0] = data->xmin;
					
				if (eul[0] > data->xmax)
					eul[0] = data->xmax;
			}
			if (data->flag & LIMIT_YROT) {
				if (eul[1] < data->ymin)
					eul[1] = data->ymin;
					
				if (eul[1] > data->ymax)
					eul[1] = data->ymax;
			}
			if (data->flag & LIMIT_ZROT) {
				if (eul[2] < data->zmin)
					eul[2] = data->zmin;
					
				if (eul[2] > data->zmax)
					eul[2] = data->zmax;
			}
				
			/* eulers: degrees to radians ! */
			eul[0] = (eul[0] / 180 * M_PI); 
			eul[1] = (eul[1] / 180 * M_PI);
			eul[2] = (eul[2] / 180 * M_PI);
			
			LocEulSizeToMat4(ownermat, loc, eul, size);
		}
		break;
	case CONSTRAINT_TYPE_SIZELIMIT:
		{
			bSizeLimitConstraint *data;
			float obsize[3], size[3];
			
			data = constraint->data;
			
			Mat4ToSize(ownermat, size);
			Mat4ToSize(ownermat, obsize);
			
			if (data->flag & LIMIT_XMIN) {
				if (size[0] < data->xmin) 
					size[0] = data->xmin;	
			}
			if (data->flag & LIMIT_XMAX) {
				if (size[0] > data->xmax) 
					size[0] = data->xmax;
			}
			if (data->flag & LIMIT_YMIN) {
				if (size[1] < data->ymin) 
					size[1] = data->ymin;	
			}
			if (data->flag & LIMIT_YMAX) {
				if (size[1] > data->ymax) 
					size[1] = data->ymax;
			}
			if (data->flag & LIMIT_ZMIN) {
				if (size[2] < data->zmin) 
					size[2] = data->zmin;	
			}
			if (data->flag & LIMIT_ZMAX) {
				if (size[2] > data->zmax) 
					size[2] = data->zmax;
			}
			
			VecMulf(ownermat[0], size[0]/obsize[0]);
			VecMulf(ownermat[1], size[1]/obsize[1]);
			VecMulf(ownermat[2], size[2]/obsize[2]);
		}
		break;
	case CONSTRAINT_TYPE_RIGIDBODYJOINT:
        {
			/* Do nothing. The GameEngine will take care of this.*/
        }
        break;	
	case CONSTRAINT_TYPE_CLAMPTO:
		{
			bClampToConstraint *data;
			Curve *cu;
			float obmat[4][4], targetMatrix[4][4], ownLoc[3];
			float curveMin[3], curveMax[3];
			
			data = constraint->data;
			
			/* prevent crash if user deletes curve */
			if ((data->tar == NULL) || (data->tar->type != OB_CURVE) ) 
				return;
			else
				cu= data->tar->data;
			
			Mat4CpyMat4(obmat, ownermat);
			Mat4One(targetMatrix);
			VECCOPY(ownLoc, obmat[3]);
			
			INIT_MINMAX(curveMin, curveMax)
			minmax_object(data->tar, curveMin, curveMax);
			
			/* get targetmatrix */
			if (cu->path && cu->path->data) {
				float vec[4], dir[3], totmat[4][4];
				float curvetime;
				short clamp_axis;
				
				/* find best position on curve */
				/* 1. determine which axis to sample on? */
				if (data->flag==CLAMPTO_AUTO) {
					float size[3];
					VecSubf(size, curveMax, curveMin);
					
					/* find axis along which the bounding box has the greatest
					 * extent. Otherwise, default to the x-axis, as that is quite
					 * frequently used.
					 */
					if ((size[2]>size[0]) && (size[2]>size[1]))
						clamp_axis= CLAMPTO_Z;
					else if ((size[1]>size[0]) && (size[1]>size[2]))
						clamp_axis= CLAMPTO_Y;
					else
						clamp_axis = CLAMPTO_X;
				}
				else 
					clamp_axis= data->flag;
					
				/* 2. determine position relative to curve on a 0-1 scale */
				if (clamp_axis > 0) clamp_axis--;
				if (ownLoc[clamp_axis] <= curveMin[clamp_axis])
					curvetime = 0.0;
				else if (ownLoc[clamp_axis] >= curveMax[clamp_axis])
					curvetime = 1.0;
				else
					curvetime = (ownLoc[clamp_axis] - curveMin[clamp_axis]) / (curveMax[clamp_axis] - curveMin[clamp_axis]);
				
				/* 3. position on curve */
				if(where_on_path(data->tar, curvetime, vec, dir) ) {
					Mat4One(totmat);
					VECCOPY(totmat[3], vec);
					
					Mat4MulSerie(targetMatrix, data->tar->obmat, totmat, NULL, NULL, NULL, NULL, NULL, NULL);
				}
			}
			
			/* obtain final object position */
			VECCOPY(ownermat[3], targetMatrix[3]);
		}
		break;
	case CONSTRAINT_TYPE_CHILDOF: 
		{
			bChildOfConstraint *data;
			
			data = constraint->data;
			
			/* only evaluate if there is a target */
			if (data->tar) {
				float parmat[4][4], invmat[4][4], tempmat[4][4];
				float loc[3], eul[3], size[3];
				float loco[3], eulo[3], sizo[3];
				
				/* get offset (parent-inverse) matrix */
				Mat4CpyMat4(invmat, data->invmat);
				
				/* extract components of both matrices */
				VECCOPY(loc, targetmat[3]);
				Mat4ToEul(targetmat, eul);
				Mat4ToSize(targetmat, size);
				
				VECCOPY(loco, invmat[3]);
				Mat4ToEul(invmat, eulo);
				Mat4ToSize(invmat, sizo);
				
				/* disable channels not enabled */
				if (!(data->flag & CHILDOF_LOCX)) loc[0]= loco[0]= 0.0f;
				if (!(data->flag & CHILDOF_LOCY)) loc[1]= loco[1]= 0.0f;
				if (!(data->flag & CHILDOF_LOCZ)) loc[2]= loco[2]= 0.0f;
				if (!(data->flag & CHILDOF_ROTX)) eul[0]= eulo[0]= 0.0f;
				if (!(data->flag & CHILDOF_ROTY)) eul[1]= eulo[1]= 0.0f;
				if (!(data->flag & CHILDOF_ROTZ)) eul[2]= eulo[2]= 0.0f;
				if (!(data->flag & CHILDOF_SIZEX)) size[0]= sizo[0]= 1.0f;
				if (!(data->flag & CHILDOF_SIZEY)) size[1]= sizo[1]= 1.0f;
				if (!(data->flag & CHILDOF_SIZEZ)) size[2]= sizo[2]= 1.0f;
				
				/* make new target mat and offset mat */
				LocEulSizeToMat4(targetmat, loc, eul, size);
				LocEulSizeToMat4(invmat, loco, eulo, sizo);
				
				/* multiply target (parent matrix) by offset (parent inverse) to get 
				 * the effect of the parent that will be exherted on the owner
				 */
				Mat4MulMat4(parmat, invmat, targetmat);
				
				/* now multiply the parent matrix by the owner matrix to get the 
				 * the effect of this constraint (i.e.  owner is 'parented' to parent)
				 */
				Mat4CpyMat4(tempmat, ownermat);
				Mat4MulMat4(ownermat, tempmat, parmat); 
			}
		}
		break;
	case CONSTRAINT_TYPE_TRANSFORM:
		{
			bTransformConstraint *data;
			
			data = constraint->data;
			
			/* only work if there is a target */
			if (data->tar) {
				float loc[3], eul[3], size[3];
				float dvec[3], sval[3];
				short i;
				
				/* obtain target effect */
				switch (data->from) {
					case 2:	/* scale */
					{
						Mat4ToSize(targetmat, dvec);
					}
						break;
					case 1: /* rotation */
					{
						/* copy, and reduce to smallest rotation distance */
						Mat4ToEul(targetmat, dvec);
						
						/* reduce rotation */
						for (i=0; i<3; i++)
							dvec[i]= fmod(dvec[i], M_PI*2);
					}
						break;
					default: /* location */
					{
						VECCOPY(dvec, targetmat[3]);
					}
						break;
				}
				
				/* extract components of owner's matrix */
				VECCOPY(loc, ownermat[3]);
				Mat4ToEul(ownermat, eul);
				Mat4ToSize(ownermat, size);
				
				/* determine where in range current transforms lie */
				if (data->expo) {
					for (i=0; i<3; i++) {
						if (data->from_max[i] - data->from_min[i])
							sval[i]= (dvec[i] - data->from_min[i]) / (data->from_max[i] - data->from_min[i]);
						else
							sval[i]= 0.0f;
					}
				}
				else {
					/* clamp transforms out of range */
					for (i=0; i<3; i++) {
						CLAMP(dvec[i], data->from_min[i], data->from_max[i]);
						if (data->from_max[i] - data->from_min[i])
							sval[i]= (dvec[i] - data->from_min[i]) / (data->from_max[i] - data->from_min[i]);
						else
							sval[i]= 0.0f;
					}
				}
				
				/* convert radian<->degree */
				if (data->from==1 && data->to==0) {
					/* from radians to degrees */
					for (i=0; i<3; i++) 
						sval[i] = sval[i] / M_PI * 180;
				}
				else if (data->from==0 && data->to==1) {
					/* from degrees to radians */
					for (i=0; i<3; i++) 
						sval[i] = sval[i] / 180 * M_PI;
				}
				else if (data->from == data->to == 1) {
					/* degrees to radians */
					for (i=0; i<3; i++) 
						sval[i] = sval[i] / 180 * M_PI;
				}
				
				/* apply transforms */
				switch (data->to) {
					case 2: /* scaling */
						for (i=0; i<3; i++)
							size[i]= data->to_min[i] + (sval[data->map[i]] * (data->to_max[i] - data->to_min[i])); 
						break;
					case 1: /* rotation */
						for (i=0; i<3; i++) {
							float tmin, tmax;
							
							/* convert destination min/max ranges from degrees to radians */
							tmin= data->to_min[i] / M_PI * 180;
							tmax= data->to_max[i] / M_PI * 180;
							
							eul[i]= tmin + (sval[data->map[i]] * (tmax - tmin)); 
						}
						break;
					default: /* location */
						for (i=0; i<3; i++)
							loc[i] += (data->to_min[i] + (sval[data->map[i]] * (data->to_max[i] - data->to_min[i]))); 
						break;
				}
				
				/* apply to matrix */
				LocEulSizeToMat4(ownermat, loc, eul, size);
			}
		}
		break;
	default:
		printf("Error: Unknown constraint type\n");
		break;
	}
}

/* this function is called whenever constraints need to be evaluated  */
void solve_constraints (ListBase *conlist, bConstraintOb *cob, float ctime)
{
	bConstraint *con;
	void *ownerdata;
	float tarmat[4][4], oldmat[4][4];
	float solution[4][4], delta[4][4], imat[4][4];
	float enf;

	/* check that there is a valid constraint object to evaluate */
	if (cob == NULL)
		return;
	
	/* loop over available constraints, solving and blending them */
	for (con= conlist->first; con; con= con->next) {
		/* this we can skip completely */
		if (con->flag & CONSTRAINT_DISABLE) continue;
		/* and inverse kinematics is solved seperate */ 
		if (con->type==CONSTRAINT_TYPE_KINEMATIC) continue;
		/* rigidbody is really a game-engine thing - and is not solved here */
		if (con->type==CONSTRAINT_TYPE_RIGIDBODYJOINT) continue;
		
		/* influence of constraint */
		/* value should have been set from IPO's/Constraint Channels already */
		enf = con->enforce;
		
		/* move target/owner into right spaces */
		constraint_mat_convertspace(cob->ob, cob->pchan, cob->matrix, CONSTRAINT_SPACE_WORLD, con->ownspace);
		
		/* Get the target matrix */
		ownerdata= ((cob->pchan)? (void *)cob->pchan : (void *)cob->ob);
		get_constraint_target_matrix(con, cob->type, ownerdata, tarmat, ctime);
		
		Mat4CpyMat4(oldmat, cob->matrix);
		
		/* solve the constraint */
		evaluate_constraint(con, cob->matrix, tarmat);
		
		/* Interpolate the enforcement, to blend result of constraint into final owner transform */
		/* 1. Remove effects of original matrix from constraint solution ==> delta */
		Mat4Invert(imat, oldmat);
		Mat4CpyMat4(solution, cob->matrix);
		Mat4MulMat4(delta, solution, imat);
		
		/* 2. If constraint influence is not full strength, then interpolate
		 * 	identity_matrix --> delta_matrix to get the effect the constraint actually exerts
		 */
		if (enf < 1.0) {
			float identity[4][4];
			Mat4One(identity);
			Mat4BlendMat4(delta, identity, delta, enf);
		}
		
		/* 3. Now multiply the delta by the matrix in use before the evaluation */
		Mat4MulMat4(cob->matrix, delta, oldmat);
		
		/* move target/owner back into worldspace for next constraint/other business */
		if ((con->flag & CONSTRAINT_SPACEONCE) == 0) 
			constraint_mat_convertspace(cob->ob, cob->pchan, cob->matrix, con->ownspace, CONSTRAINT_SPACE_WORLD);
	}
}
