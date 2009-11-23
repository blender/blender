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
 *				 Full recode, Joshua Leung, 2009
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>
#include <math.h>
#include <stdlib.h>
#include <stddef.h>	

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_key_types.h"
#include "DNA_nla_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_anim.h"
#include "BKE_armature.h"
#include "BKE_blender.h"
#include "BKE_constraint.h"
#include "BKE_displist.h"
#include "BKE_global.h"
#include "BKE_fcurve.h"
#include "BKE_key.h"
#include "BKE_lattice.h"
#include "BKE_library.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"
#include "BIK_api.h"

#include "BLI_math.h"
#include "BLI_blenlib.h"
#include "BLI_ghash.h"

#include "RNA_access.h"
#include "RNA_types.h"

/* *********************** NOTE ON POSE AND ACTION **********************

  - Pose is the local (object level) component of armature. The current
    object pose is saved in files, and (will be) is presorted for dependency
  - Actions have fewer (or other) channels, and write data to a Pose
  - Currently ob->pose data is controlled in where_is_pose only. The (recalc)
    event system takes care of calling that
  - The NLA system (here too) uses Poses as interpolation format for Actions
  - Therefore we assume poses to be static, and duplicates of poses have channels in
    same order, for quick interpolation reasons

  ****************************** (ton) ************************************ */

/* ***************** Library data level operations on action ************** */

bAction *add_empty_action(const char name[])
{
	bAction *act;
	
	act= alloc_libblock(&G.main->action, ID_AC, name);
	act->id.flag |= LIB_FAKEUSER; // XXX this is nasty for new users... maybe we don't want this anymore
	act->id.us++;
	
	return act;
}	

// does copy_fcurve...
void make_local_action(bAction *act)
{
	// Object *ob;
	bAction *actn;
	int local=0, lib=0;
	
	if (act->id.lib==0) return;
	if (act->id.us==1) {
		act->id.lib= 0;
		act->id.flag= LIB_LOCAL;
		//make_local_action_channels(act);
		new_id(0, (ID *)act, 0);
		return;
	}
	
#if 0	// XXX old animation system
	ob= G.main->object.first;
	while(ob) {
		if(ob->action==act) {
			if(ob->id.lib) lib= 1;
			else local= 1;
		}
		ob= ob->id.next;
	}
#endif
	
	if(local && lib==0) {
		act->id.lib= 0;
		act->id.flag= LIB_LOCAL;
		//make_local_action_channels(act);
		new_id(0, (ID *)act, 0);
	}
	else if(local && lib) {
		actn= copy_action(act);
		actn->id.us= 0;
		
#if 0	// XXX old animation system
		ob= G.main->object.first;
		while(ob) {
			if(ob->action==act) {
				
				if(ob->id.lib==0) {
					ob->action = actn;
					actn->id.us++;
					act->id.us--;
				}
			}
			ob= ob->id.next;
		}
#endif	// XXX old animation system
	}
}


void free_action (bAction *act)
{
	/* sanity check */
	if (act == NULL)
		return;
	
	/* Free F-Curves */
	free_fcurves(&act->curves);
	
	/* Free groups */
	if (act->groups.first)
		BLI_freelistN(&act->groups);
		
	/* Free pose-references (aka local markers) */
	if (act->markers.first)
		BLI_freelistN(&act->markers);
}

bAction *copy_action (bAction *src)
{
	bAction *dst = NULL;
	bActionGroup *dgrp, *sgrp;
	FCurve *dfcu, *sfcu;
	
	if (src == NULL) 
		return NULL;
	dst= copy_libblock(src);
	
	/* duplicate the lists of groups and markers */
	BLI_duplicatelist(&dst->groups, &src->groups);
	BLI_duplicatelist(&dst->markers, &src->markers);
	
	/* copy F-Curves, fixing up the links as we go */
	dst->curves.first= dst->curves.last= NULL;
	
	for (sfcu= src->curves.first; sfcu; sfcu= sfcu->next) {
		/* duplicate F-Curve */
		dfcu= copy_fcurve(sfcu);
		BLI_addtail(&dst->curves, dfcu);
		
		/* fix group links (kindof bad list-in-list search, but this is the most reliable way) */
		for (dgrp=dst->groups.first, sgrp=src->groups.first; dgrp && sgrp; dgrp=dgrp->next, sgrp=sgrp->next) {
			if (sfcu->grp == sgrp) {
				dfcu->grp= dgrp;
				
				if (dgrp->channels.first == sfcu)
					dgrp->channels.first= dfcu;
				if (dgrp->channels.last == sfcu)
					dgrp->channels.last= dfcu;
					
				break;
			}
		}
	}
	
	dst->id.flag |= LIB_FAKEUSER; // XXX this is nasty for new users... maybe we don't want this anymore
	dst->id.us++;
	
	return dst;
}

/* *************** Action Groups *************** */

/* Get the active action-group for an Action */
bActionGroup *get_active_actiongroup (bAction *act)
{
	bActionGroup *agrp= NULL;
	
	if (act && act->groups.first) {	
		for (agrp= act->groups.first; agrp; agrp= agrp->next) {
			if (agrp->flag & AGRP_ACTIVE)
				break;
		}
	}
	
	return agrp;
}

/* Make the given Action-Group the active one */
void set_active_action_group (bAction *act, bActionGroup *agrp, short select)
{
	bActionGroup *grp;
	
	/* sanity checks */
	if (act == NULL)
		return;
	
	/* Deactive all others */
	for (grp= act->groups.first; grp; grp= grp->next) {
		if ((grp==agrp) && (select))
			grp->flag |= AGRP_ACTIVE;
		else	
			grp->flag &= ~AGRP_ACTIVE;
	}
}

/* Add given channel into (active) group 
 *	- assumes that channel is not linked to anything anymore
 *	- always adds at the end of the group 
 */
void action_groups_add_channel (bAction *act, bActionGroup *agrp, FCurve *fcurve)
{
	FCurve *fcu;
	short done=0;
	
	/* sanity checks */
	if (ELEM3(NULL, act, agrp, fcurve))
		return;
	
	/* if no channels, just add to two lists at the same time */
	if (act->curves.first == NULL) {
		fcurve->next = fcurve->prev = NULL;
		
		agrp->channels.first = agrp->channels.last = fcurve;
		act->curves.first = act->curves.last = fcurve;
		
		fcurve->grp= agrp;
		return;
	}
	
	/* try to find a channel to slot this in before/after */
	for (fcu= act->curves.first; fcu; fcu= fcu->next) {
		/* if channel has no group, then we have ungrouped channels, which should always occur after groups */
		if (fcu->grp == NULL) {
			BLI_insertlinkbefore(&act->curves, fcu, fcurve);
			
			if (agrp->channels.first == NULL)
				agrp->channels.first= fcurve;
			agrp->channels.last= fcurve;
			
			done= 1;
			break;
		}
		
		/* if channel has group after current, we can now insert (otherwise we have gone too far) */
		else if (fcu->grp == agrp->next) {
			BLI_insertlinkbefore(&act->curves, fcu, fcurve);
			
			if (agrp->channels.first == NULL)
				agrp->channels.first= fcurve;
			agrp->channels.last= fcurve;
			
			done= 1;
			break;
		}
		
		/* if channel has group we're targeting, check whether it is the last one of these */
		else if (fcu->grp == agrp) {
			if ((fcu->next) && (fcu->next->grp != agrp)) {
				BLI_insertlinkafter(&act->curves, fcu, fcurve);
				agrp->channels.last= fcurve;
				done= 1;
				break;
			}
			else if (fcu->next == NULL) {
				BLI_addtail(&act->curves, fcurve);
				agrp->channels.last= fcurve;
				done= 1;
				break;
			}
		}
		
		/* if channel has group before target, check whether the next one is something after target */
		else if (fcu->grp == agrp->prev) {
			if (fcu->next) {
				if ((fcu->next->grp != fcu->grp) && (fcu->next->grp != agrp)) {
					BLI_insertlinkafter(&act->curves, fcu, fcurve);
					
					agrp->channels.first= fcurve;
					agrp->channels.last= fcurve;
					
					done= 1;
					break;
				}
			}
			else {
				BLI_insertlinkafter(&act->curves, fcu, fcurve);
				
				agrp->channels.first= fcurve;
				agrp->channels.last= fcurve;
				
				done= 1;
				break;
			}
		}
	}
	
	/* only if added, set channel as belonging to this group */
	if (done) {
		//printf("FCurve added to group \n");
		fcurve->grp= agrp;
	}
	else {
		printf("Error: FCurve '%s' couldn't be added to Group '%s' \n", fcurve->rna_path, agrp->name);
		BLI_addtail(&act->curves, fcurve);
	}
}	

/* Remove the given channel from all groups */
void action_groups_remove_channel (bAction *act, FCurve *fcu)
{
	/* sanity checks */
	if (ELEM(NULL, act, fcu))	
		return;
	
	/* check if any group used this directly */
	if (fcu->grp) {
		bActionGroup *agrp= fcu->grp;
		
		if (agrp->channels.first == agrp->channels.last) {
			if (agrp->channels.first == fcu) {
				agrp->channels.first= NULL;
				agrp->channels.last= NULL;
			}
		}
		else if (agrp->channels.first == fcu) {
			if ((fcu->next) && (fcu->next->grp==agrp))
				agrp->channels.first= fcu->next;
			else
				agrp->channels.first= NULL;
		}
		else if (agrp->channels.last == fcu) {
			if ((fcu->prev) && (fcu->prev->grp==agrp))
				agrp->channels.last= fcu->prev;
			else
				agrp->channels.last= NULL;
		}
		
		fcu->grp= NULL;
	}
	
	/* now just remove from list */
	BLI_remlink(&act->curves, fcu);
}

/* Find a group with the given name */
bActionGroup *action_groups_find_named (bAction *act, const char name[])
{
	bActionGroup *grp;
	
	/* sanity checks */
	if (ELEM3(NULL, act, act->groups.first, name) || (name[0] == 0))
		return NULL;
		
	/* do string comparisons */
	for (grp= act->groups.first; grp; grp= grp->next) {
		if (strcmp(grp->name, name) == 0)
			return grp;
	}
	
	/* not found */
	return NULL;
}

/* *************** Pose channels *************** */

/* usually used within a loop, so we got a N^2 slowdown */
bPoseChannel *get_pose_channel(const bPose *pose, const char *name)
{
	bPoseChannel *chan;
	
	if (ELEM(NULL, pose, name) || (name[0] == 0))
		return NULL;
	
	for (chan=pose->chanbase.first; chan; chan=chan->next) {
		if (chan->name[0] == name[0]) {
			if (!strcmp (chan->name, name))
				return chan;
		}
	}

	return NULL;
}

/* Use with care, not on Armature poses but for temporal ones */
/* (currently used for action constraints and in rebuild_pose) */
bPoseChannel *verify_pose_channel(bPose* pose, const char* name)
{
	bPoseChannel *chan;
	
	if (pose == NULL)
		return NULL;
	
	/* See if this channel exists */
	for (chan=pose->chanbase.first; chan; chan=chan->next) {
		if (!strcmp (name, chan->name))
			return chan;
	}
	
	/* If not, create it and add it */
	chan = MEM_callocN(sizeof(bPoseChannel), "verifyPoseChannel");
	
	strncpy(chan->name, name, 31);
	/* init vars to prevent math errors */
	chan->quat[0] = chan->rotAxis[1]= 1.0f;
	chan->size[0] = chan->size[1] = chan->size[2] = 1.0f;
	
	chan->limitmin[0]= chan->limitmin[1]= chan->limitmin[2]= -180.0f;
	chan->limitmax[0]= chan->limitmax[1]= chan->limitmax[2]= 180.0f;
	chan->stiffness[0]= chan->stiffness[1]= chan->stiffness[2]= 0.0f;
	chan->ikrotweight = chan->iklinweight = 0.0f;
	unit_m4(chan->constinv);
	
	BLI_addtail(&pose->chanbase, chan);
	
	return chan;
}

/* Find the active posechannel for an object (we can't just use pose, as layer info is in armature) */
bPoseChannel *get_active_posechannel (Object *ob)
{
	bArmature *arm= (ob) ? ob->data : NULL;
	bPoseChannel *pchan;
	
	if ELEM3(NULL, ob, ob->pose, arm)
		return NULL;
	
	/* find active */
	for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
		if ((pchan->bone) && (pchan->bone == arm->act_bone) && (pchan->bone->layer & arm->layer))
			return pchan;
	}
	
	return NULL;
}

const char *get_ikparam_name(bPose *pose)
{
	if (pose) {
		switch (pose->iksolver) {
		case IKSOLVER_LEGACY:
			return NULL;
		case IKSOLVER_ITASC:
			return "bItasc";
		}
	}
	return NULL;
}
/* dst should be freed already, makes entire duplicate */
void copy_pose (bPose **dst, bPose *src, int copycon)
{
	bPose *outPose;
	bPoseChannel	*pchan;
	ListBase listb;
	
	if (!src){
		*dst=NULL;
		return;
	}
	
	if (*dst==src) {
		printf("copy_pose source and target are the same\n");
		*dst=NULL;
		return;
	}
	
	outPose= MEM_callocN(sizeof(bPose), "pose");
	
	BLI_duplicatelist(&outPose->chanbase, &src->chanbase);
	
	outPose->iksolver = src->iksolver;
	outPose->ikdata = NULL;
	outPose->ikparam = MEM_dupallocN(src->ikparam);
	
	// TODO: rename this argument...
	if (copycon) {
		for (pchan=outPose->chanbase.first; pchan; pchan=pchan->next) {
			copy_constraints(&listb, &pchan->constraints);  // copy_constraints NULLs listb
			pchan->constraints= listb;
			pchan->path= NULL;
		}
		
		/* for now, duplicate Bone Groups too when doing this */
		BLI_duplicatelist(&outPose->agroups, &src->agroups);
	}
	
	*dst=outPose;
}

void init_pose_itasc(bItasc *itasc)
{
	if (itasc) {
		itasc->iksolver = IKSOLVER_ITASC;
		itasc->minstep = 0.01f;
		itasc->maxstep = 0.06f;
		itasc->numiter = 100;
		itasc->numstep = 4;
		itasc->precision = 0.005f;
		itasc->flag = ITASC_AUTO_STEP|ITASC_INITIAL_REITERATION;
		itasc->feedback = 20.f;
		itasc->maxvel = 50.f;
		itasc->solver = ITASC_SOLVER_SDLS;
		itasc->dampmax = 0.5;
		itasc->dampeps = 0.15;
	}
}
void init_pose_ikparam(bPose *pose)
{
	bItasc *itasc;
	switch (pose->iksolver) {
	case IKSOLVER_ITASC:
		itasc = MEM_callocN(sizeof(bItasc), "itasc");
		init_pose_itasc(itasc);
		pose->ikparam = itasc;
		break;
	case IKSOLVER_LEGACY:
	default:
		pose->ikparam = NULL;
		break;
	}
}

void free_pose_channels(bPose *pose) 
{
	bPoseChannel *pchan;
	
	if (pose->chanbase.first) {
		for (pchan = pose->chanbase.first; pchan; pchan=pchan->next){
			if (pchan->path)
				MEM_freeN(pchan->path);
			free_constraints(&pchan->constraints);
		}
		BLI_freelistN(&pose->chanbase);
	}
}

void free_pose(bPose *pose)
{
	if (pose) {
		/* free pose-channels */
		free_pose_channels(pose);
		
		/* free pose-groups */
		if (pose->agroups.first)
			BLI_freelistN(&pose->agroups);

		/* free IK solver state */
		BIK_clear_data(pose);

		/* free IK solver param */
		if (pose->ikparam)
			MEM_freeN(pose->ikparam);

		/* free pose */
		MEM_freeN(pose);
	}
}

static void copy_pose_channel_data(bPoseChannel *pchan, const bPoseChannel *chan)
{
	bConstraint *pcon, *con;
	
	VECCOPY(pchan->loc, chan->loc);
	VECCOPY(pchan->size, chan->size);
	VECCOPY(pchan->eul, chan->eul);
	VECCOPY(pchan->rotAxis, chan->rotAxis);
	pchan->rotAngle= chan->rotAngle;
	QUATCOPY(pchan->quat, chan->quat);
	pchan->rotmode= chan->rotmode;
	copy_m4_m4(pchan->chan_mat, (float(*)[4])chan->chan_mat);
	copy_m4_m4(pchan->pose_mat, (float(*)[4])chan->pose_mat);
	pchan->flag= chan->flag;
	
	con= chan->constraints.first;
	for(pcon= pchan->constraints.first; pcon; pcon= pcon->next, con= con->next) {
		pcon->enforce= con->enforce;
		pcon->headtail= con->headtail;
	}
}

/* checks for IK constraint, Spline IK, and also for Follow-Path constraint.
 * can do more constraints flags later 
 */
/* pose should be entirely OK */
void update_pose_constraint_flags(bPose *pose)
{
	bPoseChannel *pchan, *parchan;
	bConstraint *con;
	
	/* clear */
	for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
		pchan->constflag= 0;
	}
	pose->flag &= ~POSE_CONSTRAINTS_TIMEDEPEND;
	
	/* detect */
	for (pchan= pose->chanbase.first; pchan; pchan=pchan->next) {
		for (con= pchan->constraints.first; con; con= con->next) {
			if (con->type==CONSTRAINT_TYPE_KINEMATIC) {
				bKinematicConstraint *data = (bKinematicConstraint*)con->data;
				
				pchan->constflag |= PCHAN_HAS_IK;
				
				if(data->tar==NULL || (data->tar->type==OB_ARMATURE && data->subtarget[0]==0))
					pchan->constflag |= PCHAN_HAS_TARGET;
				
				/* negative rootbone = recalc rootbone index. used in do_versions */
				if(data->rootbone<0) {
					data->rootbone= 0;
					
					if(data->flag & CONSTRAINT_IK_TIP) parchan= pchan;
					else parchan= pchan->parent;
					
					while(parchan) {
						data->rootbone++;
						if((parchan->bone->flag & BONE_CONNECTED)==0)
							break;
						parchan= parchan->parent;
					}
				}
			}
			else if (con->type == CONSTRAINT_TYPE_FOLLOWPATH) {
				bFollowPathConstraint *data= (bFollowPathConstraint *)con->data;
				
				/* for drawing constraint colors when color set allows this */
				pchan->constflag |= PCHAN_HAS_CONST;
				
				/* if we have a valid target, make sure that this will get updated on frame-change
				 * (needed for when there is no anim-data for this pose)
				 */
				if ((data->tar) && (data->tar->type==OB_CURVE))
					pose->flag |= POSE_CONSTRAINTS_TIMEDEPEND;
			}
			else if (con->type == CONSTRAINT_TYPE_SPLINEIK)
				pchan->constflag |= PCHAN_HAS_SPLINEIK;
			else 
				pchan->constflag |= PCHAN_HAS_CONST;
		}
	}
}

/* Clears all BONE_UNKEYED flags for every pose channel in every pose 
 * This should only be called on frame changing, when it is acceptable to
 * do this. Otherwise, these flags should not get cleared as poses may get lost.
 */
void framechange_poses_clear_unkeyed(void)
{
	Object *ob;
	bPose *pose;
	bPoseChannel *pchan;
	
	/* This needs to be done for each object that has a pose */
	// TODO: proxies may/may not be correctly handled here... (this needs checking) 
	for (ob= G.main->object.first; ob; ob= ob->id.next) {
		/* we only need to do this on objects with a pose */
		if ( (pose= ob->pose) ) {
			for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
				if (pchan->bone) 
					pchan->bone->flag &= ~BONE_UNKEYED;
			}
		}
	}
}

/* ************************** Bone Groups ************************** */

/* Adds a new bone-group */
void pose_add_group (Object *ob)
{
	bPose *pose= (ob) ? ob->pose : NULL;
	bActionGroup *grp;
	
	if (ELEM(NULL, ob, ob->pose))
		return;
	
	grp= MEM_callocN(sizeof(bActionGroup), "PoseGroup");
	strcpy(grp->name, "Group");
	BLI_addtail(&pose->agroups, grp);
	BLI_uniquename(&pose->agroups, grp, "Group", '.', offsetof(bActionGroup, name), 32);
	
	pose->active_group= BLI_countlist(&pose->agroups);
}

/* Remove the active bone-group */
void pose_remove_group (Object *ob)
{
	bPose *pose= (ob) ? ob->pose : NULL;
	bActionGroup *grp = NULL;
	bPoseChannel *pchan;
	
	/* sanity checks */
	if (ELEM(NULL, ob, pose))
		return;
	if (pose->active_group <= 0)
		return;
	
	/* get group to remove */
	grp= BLI_findlink(&pose->agroups, pose->active_group-1);
	if (grp) {
		/* adjust group references (the trouble of using indices!):
		 *	- firstly, make sure nothing references it 
		 *	- also, make sure that those after this item get corrected
		 */
		for (pchan= pose->chanbase.first; pchan; pchan= pchan->next) {
			if (pchan->agrp_index == pose->active_group)
				pchan->agrp_index= 0;
			else if (pchan->agrp_index > pose->active_group)
				pchan->agrp_index--;
		}
		
		/* now, remove it from the pose */
		BLI_freelinkN(&pose->agroups, grp);
		pose->active_group= 0;
	}
}

/* ************** F-Curve Utilities for Actions ****************** */

/* Check if the given action has any keyframes */
short action_has_motion(const bAction *act)
{
	FCurve *fcu;
	
	/* return on the first F-Curve that has some keyframes/samples defined */
	if (act) {
		for (fcu= act->curves.first; fcu; fcu= fcu->next) {
			if (fcu->totvert)
				return 1;
		}
	}
	
	/* nothing found */
	return 0;
}

/* Calculate the extents of given action */
void calc_action_range(const bAction *act, float *start, float *end, short incl_modifiers)
{
	FCurve *fcu;
	float min=999999999.0f, max=-999999999.0f;
	short foundvert=0, foundmod=0;

	if (act) {
		for (fcu= act->curves.first; fcu; fcu= fcu->next) {
			/* if curve has keyframes, consider them first */
			if (fcu->totvert) {
				float nmin, nmax;
				
				/* get extents for this curve */
				calc_fcurve_range(fcu, &nmin, &nmax);
				
				/* compare to the running tally */
				min= MIN2(min, nmin);
				max= MAX2(max, nmax);
				
				foundvert= 1;
			}
			
			/* if incl_modifiers is enabled, need to consider modifiers too
			 *	- only really care about the last modifier
			 */
			if ((incl_modifiers) && (fcu->modifiers.last)) {
				FModifier *fcm= fcu->modifiers.last;
				
				/* only use the maximum sensible limits of the modifiers if they are more extreme */
				switch (fcm->type) {
					case FMODIFIER_TYPE_LIMITS: /* Limits F-Modifier */
					{
						FMod_Limits *fmd= (FMod_Limits *)fcm->data;
						
						if (fmd->flag & FCM_LIMIT_XMIN) {
							min= MIN2(min, fmd->rect.xmin);
						}
						if (fmd->flag & FCM_LIMIT_XMAX) {
							max= MAX2(max, fmd->rect.xmax);
						}
					}
						break;
						
					case FMODIFIER_TYPE_CYCLES: /* Cycles F-Modifier */
					{
						FMod_Cycles *fmd= (FMod_Cycles *)fcm->data;
						
						if (fmd->before_mode != FCM_EXTRAPOLATE_NONE)
							min= MINAFRAMEF;
						if (fmd->after_mode != FCM_EXTRAPOLATE_NONE)
							max= MAXFRAMEF;
					}
						break;
						
					// TODO: function modifier may need some special limits
						
					default: /* all other standard modifiers are on the infinite range... */
						min= MINAFRAMEF;
						max= MAXFRAMEF;
						break;
				}
				
				foundmod= 1;
			}
		}
	}	
	
	if (foundvert || foundmod) {
		if(min==max) max+= 1.0f;
		*start= min;
		*end= max;
	}
	else {
		*start= 0.0f;
		*end= 1.0f;
	}
}

/* Return flags indicating which transforms the given object/posechannel has 
 *	- if 'curves' is provided, a list of links to these curves are also returned
 */
short action_get_item_transforms (bAction *act, Object *ob, bPoseChannel *pchan, ListBase *curves)
{
	PointerRNA ptr;
	FCurve *fcu;
	char *basePath=NULL;
	short flags=0;
	
	/* build PointerRNA from provided data to obtain the paths to use */
	if (pchan)
		RNA_pointer_create((ID *)ob, &RNA_PoseChannel, pchan, &ptr);
	else if (ob)
		RNA_id_pointer_create((ID *)ob, &ptr);
	else	
		return 0;
		
	/* get the basic path to the properties of interest */
	basePath= RNA_path_from_ID_to_struct(&ptr);
	if (basePath == NULL)
		return 0;
		
	/* search F-Curves for the given properties 
	 *	- we cannot use the groups, since they may not be grouped in that way...
	 */
	for (fcu= act->curves.first; fcu; fcu= fcu->next) {
		char *bPtr=NULL, *pPtr=NULL;
		
		/* if enough flags have been found, we can stop checking unless we're also getting the curves */
		if ((flags == ACT_TRANS_ALL) && (curves == NULL))
			break;
			
		/* just in case... */
		if (fcu->rna_path == NULL)
			continue;
		
		/* step 1: check for matching base path */
		bPtr= strstr(fcu->rna_path, basePath);
		
		if (bPtr) {
			/* step 2: check for some property with transforms 
			 *	- to speed things up, only check for the ones not yet found 
			 * 	  unless we're getting the curves too
			 *	- if we're getting the curves, the BLI_genericNodeN() creates a LinkData
			 *	  node wrapping the F-Curve, which then gets added to the list
			 *	- once a match has been found, the curve cannot possibly be any other one
			 */
			if ((curves) || (flags & ACT_TRANS_LOC) == 0) {
				pPtr= strstr(fcu->rna_path, "location");
				if ((pPtr) && (pPtr >= bPtr)) {
					flags |= ACT_TRANS_LOC;
					
					if (curves) 
						BLI_addtail(curves, BLI_genericNodeN(fcu));
					continue;
				}
			}
			
			if ((curves) || (flags & ACT_TRANS_SCALE) == 0) {
				pPtr= strstr(fcu->rna_path, "scale");
				if ((pPtr) && (pPtr >= bPtr)) {
					flags |= ACT_TRANS_SCALE;
					
					if (curves) 
						BLI_addtail(curves, BLI_genericNodeN(fcu));
					continue;
				}
			}
			
			if ((curves) || (flags & ACT_TRANS_ROT) == 0) {
				pPtr= strstr(fcu->rna_path, "rotation");
				if ((pPtr) && (pPtr >= bPtr)) {
					flags |= ACT_TRANS_ROT;
					
					if (curves) 
						BLI_addtail(curves, BLI_genericNodeN(fcu));
					continue;
				}
			}
		}
	}
	
	/* free basePath */
	MEM_freeN(basePath);
	
	/* return flags found */
	return flags;
}

/* ************** Pose Management Tools ****************** */

/* Copy the data from the action-pose (src) into the pose */
/* both args are assumed to be valid */
/* exported to game engine */
/* Note! this assumes both poses are aligned, this isnt always true when dealing with user poses */
void extract_pose_from_pose(bPose *pose, const bPose *src)
{
	const bPoseChannel *schan;
	bPoseChannel *pchan= pose->chanbase.first;

	if (pose==src) {
		printf("extract_pose_from_pose source and target are the same\n");
		return;
	}

	for (schan=src->chanbase.first; (schan && pchan); schan=schan->next, pchan= pchan->next) {
		copy_pose_channel_data(pchan, schan);
	}
}

/* for do_all_pose_actions, clears the pose. Now also exported for proxy and tools */
void rest_pose(bPose *pose)
{
	bPoseChannel *pchan;
	int i;
	
	if (!pose)
		return;
	
	memset(pose->stride_offset, 0, sizeof(pose->stride_offset));
	memset(pose->cyclic_offset, 0, sizeof(pose->cyclic_offset));
	
	for (pchan=pose->chanbase.first; pchan; pchan= pchan->next) {
		for (i=0; i<3; i++) {
			pchan->loc[i]= 0.0f;
			pchan->quat[i+1]= 0.0f;
			pchan->eul[i]= 0.0f;
			pchan->size[i]= 1.0f;
			pchan->rotAxis[i]= 0.0f;
		}
		pchan->quat[0]= pchan->rotAxis[1]= 1.0f;
		pchan->rotAngle= 0.0f;
		
		pchan->flag &= ~(POSE_LOC|POSE_ROT|POSE_SIZE);
	}
}

/* both poses should be in sync */
void copy_pose_result(bPose *to, bPose *from)
{
	bPoseChannel *pchanto, *pchanfrom;
	
	if(to==NULL || from==NULL) {
		printf("pose result copy error\n"); // debug temp
		return;
	}

	if (to==from) {
		printf("copy_pose_result source and target are the same\n");
		return;
	}


	for(pchanfrom= from->chanbase.first; pchanfrom; pchanfrom= pchanfrom->next) {
		pchanto= get_pose_channel(to, pchanfrom->name);
		if(pchanto) {
			copy_m4_m4(pchanto->pose_mat, pchanfrom->pose_mat);
			copy_m4_m4(pchanto->chan_mat, pchanfrom->chan_mat);
			
			/* used for local constraints */
			VECCOPY(pchanto->loc, pchanfrom->loc);
			QUATCOPY(pchanto->quat, pchanfrom->quat);
			VECCOPY(pchanto->eul, pchanfrom->eul);
			VECCOPY(pchanto->size, pchanfrom->size);
			
			VECCOPY(pchanto->pose_head, pchanfrom->pose_head);
			VECCOPY(pchanto->pose_tail, pchanfrom->pose_tail);
			pchanto->flag= pchanfrom->flag;
		}
	}
}

/* For the calculation of the effects of an Action at the given frame on an object 
 * This is currently only used for the Action Constraint 
 */
void what_does_obaction (Scene *scene, Object *ob, Object *workob, bPose *pose, bAction *act, char groupname[], float cframe)
{
	bActionGroup *agrp= action_groups_find_named(act, groupname);
	
	/* clear workob */
	clear_workob(workob);
	
	/* init workob */
	copy_m4_m4(workob->obmat, ob->obmat);
	copy_m4_m4(workob->parentinv, ob->parentinv);
	copy_m4_m4(workob->constinv, ob->constinv);
	workob->parent= ob->parent;
	workob->track= ob->track;

	workob->trackflag= ob->trackflag;
	workob->upflag= ob->upflag;
	
	workob->partype= ob->partype;
	workob->par1= ob->par1;
	workob->par2= ob->par2;
	workob->par3= ob->par3;

	workob->constraints.first = ob->constraints.first;
	workob->constraints.last = ob->constraints.last;
	
	workob->pose= pose;	/* need to set pose too, since this is used for both types of Action Constraint */

	strcpy(workob->parsubstr, ob->parsubstr);
	strcpy(workob->id.name, "OB<ConstrWorkOb>"); /* we don't use real object name, otherwise RNA screws with the real thing */
	
	/* if we're given a group to use, it's likely to be more efficient (though a bit more dangerous) */
	if (agrp) {
		/* specifically evaluate this group only */
		PointerRNA id_ptr;
		
		/* get RNA-pointer for the workob's ID */
		RNA_id_pointer_create(&workob->id, &id_ptr);
		
		/* execute action for this group only */
		animsys_evaluate_action_group(&id_ptr, act, agrp, NULL, cframe);
	}
	else {
		AnimData adt;
		
		/* init animdata, and attach to workob */
		memset(&adt, 0, sizeof(AnimData));
		workob->adt= &adt;
		
		adt.recalc= ADT_RECALC_ANIM;
		adt.action= act;
		
		/* execute effects of Action on to workob (or it's PoseChannels) */
		BKE_animsys_evaluate_animdata(&workob->id, &adt, cframe, ADT_RECALC_ANIM);
	}
}

/* ********** NLA with non-poses works with ipo channels ********** */

#if 0 // XXX OLD ANIMATION SYSTEM (TO BE REMOVED)

/* ************************ Blending with NLA *************** */

static void blend_pose_strides(bPose *dst, bPose *src, float srcweight, short mode)
{
	float dstweight;
	
	switch (mode){
		case ACTSTRIPMODE_BLEND:
			dstweight = 1.0F - srcweight;
			break;
		case ACTSTRIPMODE_ADD:
			dstweight = 1.0F;
			break;
		default :
			dstweight = 1.0F;
	}
	
	interp_v3_v3v3(dst->stride_offset, dst->stride_offset, src->stride_offset, srcweight);
}


/* 

bone matching diagram, strips A and B

                 .------------------------.
                 |         A              |
                 '------------------------'
				 .          .             b2
                 .          .-------------v----------.
                 .      	|         B   .          |
                 .          '------------------------'
                 .          .             .
                 .          .             .
offset:          .    0     .    A-B      .  A-b2+B     
                 .          .             .

*/


static void blend_pose_offset_bone(bActionStrip *strip, bPose *dst, bPose *src, float srcweight, short mode)
{
	/* matching offset bones */
	/* take dst offset, and put src on on that location */
	
	if(strip->offs_bone[0]==0)
		return;
	
	/* are we also blending with matching bones? */
	if(strip->prev && strip->start>=strip->prev->start) {
		bPoseChannel *dpchan= get_pose_channel(dst, strip->offs_bone);
		if(dpchan) {
			bPoseChannel *spchan= get_pose_channel(src, strip->offs_bone);
			if(spchan) {
				float vec[3];
				
				/* dst->ctime has the internal strip->prev action time */
				/* map this time to nla time */
				
				float ctime= get_actionstrip_frame(strip, src->ctime, 1);
				
				if( ctime > strip->prev->end) {
					bActionChannel *achan;
					
					/* add src to dest, minus the position of src on strip->prev->end */
					
					ctime= get_actionstrip_frame(strip, strip->prev->end, 0);
					
					achan= get_action_channel(strip->act, strip->offs_bone);
					if(achan && achan->ipo) {
						bPoseChannel pchan;
						/* Evaluates and sets the internal ipo value */
						calc_ipo(achan->ipo, ctime);
						/* This call also sets the pchan flags */
						execute_action_ipo(achan, &pchan);
						
						/* store offset that moves src to location of pchan */
						sub_v3_v3v3(vec, dpchan->loc, pchan.loc);
						
						mul_mat3_m4_v3(dpchan->bone->arm_mat, vec);
					}
				}
				else {
					/* store offset that moves src to location of dst */
					
					sub_v3_v3v3(vec, dpchan->loc, spchan->loc);
					mul_mat3_m4_v3(dpchan->bone->arm_mat, vec);
				}
				
				/* if blending, we only add with factor scrweight */
				mul_v3_fl(vec, srcweight);
				
				add_v3_v3v3(dst->cyclic_offset, dst->cyclic_offset, vec);
			}
		}
	}
	
	add_v3_v3v3(dst->cyclic_offset, dst->cyclic_offset, src->cyclic_offset);
}

/* added "sizecorr" here, to allow armatures to be scaled and still have striding.
   Only works for uniform scaling. In general I'd advise against scaling armatures ever though! (ton)
*/
static float stridechannel_frame(Object *ob, float sizecorr, bActionStrip *strip, Path *path, float pathdist, float *stride_offset)
{
	bAction *act= strip->act;
	const char *name= strip->stridechannel;
	bActionChannel *achan= get_action_channel(act, name);
	int stride_axis= strip->stride_axis;

	if(achan && achan->ipo) {
		IpoCurve *icu= NULL;
		float minx=0.0f, maxx=0.0f, miny=0.0f, maxy=0.0f;
		int foundvert= 0;
		
		if(stride_axis==0) stride_axis= AC_LOC_X;
		else if(stride_axis==1) stride_axis= AC_LOC_Y;
		else stride_axis= AC_LOC_Z;
		
		/* calculate the min/max */
		for (icu=achan->ipo->curve.first; icu; icu=icu->next) {
			if(icu->adrcode==stride_axis) {
				if(icu->totvert>1) {
					foundvert= 1;
					minx= icu->bezt[0].vec[1][0];
					maxx= icu->bezt[icu->totvert-1].vec[1][0];
					
					miny= icu->bezt[0].vec[1][1];
					maxy= icu->bezt[icu->totvert-1].vec[1][1];
				}
				break;
			}
		}
		
		if(foundvert && miny!=maxy) {
			float stridelen= sizecorr*fabs(maxy-miny), striptime;
			float actiondist, pdist, pdistNewNormalized, offs;
			float vec1[4], vec2[4], dir[3];
			
			/* internal cycling, actoffs is in frames */
			offs= stridelen*strip->actoffs/(maxx-minx);
			
			/* amount path moves object */
			pdist = (float)fmod (pathdist+offs, stridelen);
			striptime= pdist/stridelen;
			
			/* amount stride bone moves */
			actiondist= sizecorr*eval_icu(icu, minx + striptime*(maxx-minx)) - miny;
			
			pdist = fabs(actiondist) - pdist;
			pdistNewNormalized = (pathdist+pdist)/path->totdist;
			
			/* now we need to go pdist further (or less) on cu path */
			where_on_path(ob, (pathdist)/path->totdist, vec1, dir);	/* vec needs size 4 */
			if (pdistNewNormalized <= 1) {
				// search for correction in positive path-direction
				where_on_path(ob, pdistNewNormalized, vec2, dir);	/* vec needs size 4 */
				sub_v3_v3v3(stride_offset, vec2, vec1);
			}
			else {
				// we reached the end of the path, search backwards instead
				where_on_path(ob, (pathdist-pdist)/path->totdist, vec2, dir);	/* vec needs size 4 */
				sub_v3_v3v3(stride_offset, vec1, vec2);
			}
			mul_mat3_m4_v3(ob->obmat, stride_offset);
			return striptime;
		}
	}
	return 0.0f;
}

static void cyclic_offs_bone(Object *ob, bPose *pose, bActionStrip *strip, float time)
{
	/* only called when strip has cyclic, so >= 1.0f works... */
	if(time >= 1.0f) {
		bActionChannel *achan= get_action_channel(strip->act, strip->offs_bone);

		if(achan && achan->ipo) {
			IpoCurve *icu= NULL;
			Bone *bone;
			float min[3]={0.0f, 0.0f, 0.0f}, max[3]={0.0f, 0.0f, 0.0f};
			int index=0, foundvert= 0;
			
			/* calculate the min/max */
			for (icu=achan->ipo->curve.first; icu; icu=icu->next) {
				if(icu->totvert>1) {
					
					if(icu->adrcode==AC_LOC_X)
						index= 0;
					else if(icu->adrcode==AC_LOC_Y)
						index= 1;
					else if(icu->adrcode==AC_LOC_Z)
						index= 2;
					else
						continue;
				
					foundvert= 1;
					min[index]= icu->bezt[0].vec[1][1];
					max[index]= icu->bezt[icu->totvert-1].vec[1][1];
				}
			}
			if(foundvert) {
				/* bring it into armature space */
				sub_v3_v3v3(min, max, min);
				bone= get_named_bone(ob->data, strip->offs_bone);	/* weak */
				if(bone) {
					mul_mat3_m4_v3(bone->arm_mat, min);
					
					/* dominant motion, cyclic_offset was cleared in rest_pose */
					if (strip->flag & (ACTSTRIP_CYCLIC_USEX | ACTSTRIP_CYCLIC_USEY | ACTSTRIP_CYCLIC_USEZ)) {
						if (strip->flag & ACTSTRIP_CYCLIC_USEX) pose->cyclic_offset[0]= time*min[0];
						if (strip->flag & ACTSTRIP_CYCLIC_USEY) pose->cyclic_offset[1]= time*min[1];
						if (strip->flag & ACTSTRIP_CYCLIC_USEZ) pose->cyclic_offset[2]= time*min[2];
					} else {
						if( fabs(min[0]) >= fabs(min[1]) && fabs(min[0]) >= fabs(min[2]))
							pose->cyclic_offset[0]= time*min[0];
						else if( fabs(min[1]) >= fabs(min[0]) && fabs(min[1]) >= fabs(min[2]))
							pose->cyclic_offset[1]= time*min[1];
						else
							pose->cyclic_offset[2]= time*min[2];
					}
				}
			}
		}
	}
}

/* simple case for now; only the curve path with constraint value > 0.5 */
/* blending we might do later... */
static Object *get_parent_path(Object *ob)
{
	bConstraint *con;
	
	if(ob->parent && ob->parent->type==OB_CURVE)
		return ob->parent;
	
	for (con = ob->constraints.first; con; con=con->next) {
		if(con->type==CONSTRAINT_TYPE_FOLLOWPATH) {
			if(con->enforce>0.5f) {
				bFollowPathConstraint *data= con->data;
				return data->tar;
			}
		}
	}
	return NULL;
}

/* ************** do the action ************ */

/* ----- nla, etc. --------- */

static void do_nla(Scene *scene, Object *ob, int blocktype)
{
	bPose *tpose= NULL;
	Key *key= NULL;
	ListBase tchanbase={NULL, NULL}, chanbase={NULL, NULL};
	bActionStrip *strip, *striplast=NULL, *stripfirst=NULL;
	float striptime, frametime, length, actlength;
	float blendfac, stripframe;
	float scene_cfra= frame_to_float(scene, scene->r.cfra); 
	int	doit, dostride;
	
	if(blocktype==ID_AR) {
		copy_pose(&tpose, ob->pose, 1);
		rest_pose(ob->pose);		// potentially destroying current not-keyed pose
	}
	else {
		key= ob_get_key(ob);
	}
	
	/* check on extend to left or right, when no strip is hit by 'cfra' */
	for (strip=ob->nlastrips.first; strip; strip=strip->next) {
		/* escape loop on a hit */
		if( scene_cfra >= strip->start && scene_cfra <= strip->end + 0.1f)	/* note 0.1 comes back below */
			break;
		if(scene_cfra < strip->start) {
			if(stripfirst==NULL)
				stripfirst= strip;
			else if(stripfirst->start > strip->start)
				stripfirst= strip;
		}
		else if(scene_cfra > strip->end) {
			if(striplast==NULL)
				striplast= strip;
			else if(striplast->end < strip->end)
				striplast= strip;
		}
	}
	if(strip==NULL) {	/* extend */
		if(striplast)
			scene_cfra= striplast->end;
		else if(stripfirst)
			scene_cfra= stripfirst->start;
	}
	
	/* and now go over all strips */
	for (strip=ob->nlastrips.first; strip; strip=strip->next){
		doit=dostride= 0;
		
		if (strip->act && !(strip->flag & ACTSTRIP_MUTE)) {	/* so theres an action */
			
			/* Determine if the current frame is within the strip's range */
			length = strip->end-strip->start;
			actlength = strip->actend-strip->actstart;
			striptime = (scene_cfra-(strip->start)) / length;
			stripframe = (scene_cfra-(strip->start)) ;

			if (striptime>=0.0){
				
				if(blocktype==ID_AR) 
					rest_pose(tpose);
				
				/* To handle repeat, we add 0.1 frame extra to make sure the last frame is included */
				if (striptime < 1.0f + 0.1f/length) {
					
					/* Handle path */
					if ((strip->flag & ACTSTRIP_USESTRIDE) && (blocktype==ID_AR) && (ob->ipoflag & OB_DISABLE_PATH)==0){
						Object *parent= get_parent_path(ob);
						
						if (parent) {
							Curve *cu = parent->data;
							float ctime, pdist;
							
							if (cu->flag & CU_PATH){
								/* Ensure we have a valid path */
								if(cu->path==NULL || cu->path->data==NULL) makeDispListCurveTypes(scene, parent, 0);
								if(cu->path) {
									
									/* Find the position on the path */
									ctime= bsystem_time(scene, ob, scene_cfra, 0.0);
									
									if(calc_ipo_spec(cu->ipo, CU_SPEED, &ctime)==0) {
										/* correct for actions not starting on zero */
										ctime= (ctime - strip->actstart)/cu->pathlen;
										CLAMP(ctime, 0.0, 1.0);
									}
									pdist = ctime*cu->path->totdist;
									
									if(tpose && strip->stridechannel[0]) {
										striptime= stridechannel_frame(parent, ob->size[0], strip, cu->path, pdist, tpose->stride_offset);
									}									
									else {
										if (strip->stridelen) {
											striptime = pdist / strip->stridelen;
											striptime = (float)fmod (striptime+strip->actoffs, 1.0);
										}
										else
											striptime = 0;
									}
									
									frametime = (striptime * actlength) + strip->actstart;
									frametime= bsystem_time(scene, ob, frametime, 0.0);
									
									if(blocktype==ID_AR) {
										extract_pose_from_action (tpose, strip->act, frametime);
									}
									else if(blocktype==ID_OB) {
										extract_ipochannels_from_action(&tchanbase, &ob->id, strip->act, "Object", frametime);
										if(key)
											extract_ipochannels_from_action(&tchanbase, &key->id, strip->act, "Shape", frametime);
									}
									doit=dostride= 1;
								}
							}
						}
					}
					/* To handle repeat, we add 0.1 frame extra to make sure the last frame is included */
					else  {
						
						/* Mod to repeat */
						if(strip->repeat!=1.0f) {
							float cycle= striptime*strip->repeat;
							
							striptime = (float)fmod (cycle, 1.0f + 0.1f/length);
							cycle-= striptime;
							
							if(blocktype==ID_AR)
								cyclic_offs_bone(ob, tpose, strip, cycle);
						}

						frametime = (striptime * actlength) + strip->actstart;
						frametime= nla_time(scene, frametime, (float)strip->repeat);
							
						if(blocktype==ID_AR) {
							extract_pose_from_action (tpose, strip->act, frametime);
						}
						else if(blocktype==ID_OB) {
							extract_ipochannels_from_action(&tchanbase, &ob->id, strip->act, "Object", frametime);
							if(key)
								extract_ipochannels_from_action(&tchanbase, &key->id, strip->act, "Shape", frametime);
						}
						
						doit=1;
					}
				}
				/* Handle extend */
				else {
					if (strip->flag & ACTSTRIP_HOLDLASTFRAME){
						/* we want the strip to hold on the exact fraction of the repeat value */
						
						frametime = actlength * (strip->repeat-(int)strip->repeat);
						if(frametime<=0.000001f) frametime= actlength;	/* rounding errors... */
						frametime= bsystem_time(scene, ob, frametime+strip->actstart, 0.0);
						
						if(blocktype==ID_AR)
							extract_pose_from_action (tpose, strip->act, frametime);
						else if(blocktype==ID_OB) {
							extract_ipochannels_from_action(&tchanbase, &ob->id, strip->act, "Object", frametime);
							if(key)
								extract_ipochannels_from_action(&tchanbase, &key->id, strip->act, "Shape", frametime);
						}
						
						/* handle cycle hold */
						if(strip->repeat!=1.0f) {
							if(blocktype==ID_AR)
								cyclic_offs_bone(ob, tpose, strip, strip->repeat-1.0f);
						}
						
						doit=1;
					}
				}
				
				/* Handle blendin & blendout */
				if (doit){
					/* Handle blendin */
					
					if (strip->blendin>0.0 && stripframe<=strip->blendin && scene_cfra>=strip->start){
						blendfac = stripframe/strip->blendin;
					}
					else if (strip->blendout>0.0 && stripframe>=(length-strip->blendout) && scene_cfra<=strip->end){
						blendfac = (length-stripframe)/(strip->blendout);
					}
					else
						blendfac = 1;
					
					if(blocktype==ID_AR) {/* Blend this pose with the accumulated pose */
						/* offset bone, for matching cycles */
						blend_pose_offset_bone (strip, ob->pose, tpose, blendfac, strip->mode);
						
						blend_poses (ob->pose, tpose, blendfac, strip->mode);
						if(dostride)
							blend_pose_strides (ob->pose, tpose, blendfac, strip->mode);
					}
					else {
						blend_ipochannels(&chanbase, &tchanbase, blendfac, strip->mode);
						BLI_freelistN(&tchanbase);
					}
				}
			}					
		}
	}
	
	if(blocktype==ID_OB) {
		execute_ipochannels(&chanbase);
	}
	else if(blocktype==ID_AR) {
		/* apply stride offset to object */
		add_v3_v3v3(ob->obmat[3], ob->obmat[3], ob->pose->stride_offset);
	}
	
	/* free */
	if (tpose)
		free_pose(tpose);
	if(chanbase.first)
		BLI_freelistN(&chanbase);
}

#endif // XXX OLD ANIMATION SYSTEM (TO BE REMOVED)
