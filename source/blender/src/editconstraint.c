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

#include "BLI_blenlib.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"

#include "BKE_utildefines.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_object.h"
#include "BKE_global.h"
#include "BKE_constraint.h"
#include "BKE_ipo.h"

#include "BIF_editaction.h"
#include "BIF_editarmature.h"
#include "BIF_editconstraint.h"
#include "BIF_poseobject.h"
#include "BIF_interface.h"
#include "BIF_screen.h"
#include "BIF_toolbox.h"

#include "blendef.h"
#include "nla.h"


ListBase *get_active_constraint_channels (Object *ob, int forcevalid)
{
	char ipstr[64];
	
	if (!ob)
		return NULL;
	
	/* See if we are a bone constraint */
	if (ob->flag & OB_POSEMODE) {
		bActionChannel *achan;
		bPoseChannel *pchan;

		pchan = get_active_posechannel(ob);
		if (pchan) {
			
			/* Make sure we have an action */
			if (!ob->action){
				if (!forcevalid)
					return NULL;
				
				ob->action=add_empty_action();
			}
			
			/* Make sure we have an actionchannel */
			achan = get_named_actionchannel(ob->action, pchan->name);
			if (!achan){
				if (!forcevalid)
					return NULL;
				
				achan = MEM_callocN (sizeof(bActionChannel), "actionChannel");

				strcpy (achan->name, pchan->name);
				sprintf (ipstr, "%s.%s", ob->action->id.name+2, achan->name);
				ipstr[23]=0;
				achan->ipo=	add_ipo(ipstr, ID_AC);	
				
				BLI_addtail (&ob->action->chanbase, achan);
			}
			
			return &achan->constraintChannels;
		}
		else return NULL;
	}
	/* else we return object constraints */
	return &ob->constraintChannels;
}


/* if object in posemode, active bone constraints, else object constraints */
ListBase *get_active_constraints(Object *ob)
{
	if (!ob)
		return NULL;

	if (ob->flag & OB_POSEMODE) {
		bPoseChannel *pchan;

		pchan = get_active_posechannel(ob);
		if (pchan)
			return &pchan->constraints;
	}
	else 
		return &ob->constraints;

	return NULL;
}

/* single constraint */
bConstraint *get_active_constraint(Object *ob)
{
	ListBase *lb= get_active_constraints(ob);

	if(lb) {
		bConstraint *con;
		for(con= lb->first; con; con=con->next)
			if(con->flag & CONSTRAINT_ACTIVE)
				return con;
	}
	return NULL;
}

/* single channel, for ipo */
bConstraintChannel *get_active_constraint_channel(Object *ob)
{
	bConstraint *con;
	bConstraintChannel *chan;
	
	if (ob->flag & OB_POSEMODE) {
		if(ob->action) {
			bPoseChannel *pchan;
			
			pchan = get_active_posechannel(ob);
			if(pchan) {
				for(con= pchan->constraints.first; con; con= con->next)
					if(con->flag & CONSTRAINT_ACTIVE)
						break;
				if(con) {
					bActionChannel *achan = get_named_actionchannel(ob->action, pchan->name);
					if(achan) {
						for(chan= achan->constraintChannels.first; chan; chan= chan->next)
							if(!strcmp(chan->name, con->name))
								break;
						return chan;
					}
				}
			}
		}
	}
	else {
		for(con= ob->constraints.first; con; con= con->next)
			if(con->flag & CONSTRAINT_ACTIVE)
				break;
		if(con) {
			for(chan= ob->constraintChannels.first; chan; chan= chan->next)
				if(!strcmp(chan->name, con->name))
					break;
			return chan;
		}
	}
	
	return NULL;
}



bConstraint *add_new_constraint(short type)
{
	bConstraint *con;

	con = MEM_callocN(sizeof(bConstraint), "constraint");

	/* Set up a generic constraint datablock */
	con->type = type;
	con->flag |= CONSTRAINT_EXPAND;
	con->enforce=1.0F;
	/* Load the data for it */
	con->data = new_constraint_data(con->type);
	strcpy (con->name, "Const");
	return con;
}

void add_constraint_to_object(bConstraint *con, Object *ob)
{
	ListBase *list;
	list = &ob->constraints;
	
	if (list) {
		unique_constraint_name(con, list);
		BLI_addtail(list, con);
		
		con->flag |= CONSTRAINT_ACTIVE;
		for(con= con->prev; con; con= con->prev)
			con->flag &= ~CONSTRAINT_ACTIVE;
	}
}


char *get_con_subtarget_name(bConstraint *con, Object *target)
{
	/*
	 * If the target for this constraint is target, return a pointer 
	 * to the name for this constraints subtarget ... NULL otherwise
	 */
	switch (con->type) {

		case CONSTRAINT_TYPE_ACTION:
		{
			bActionConstraint *data = con->data;
			if (data->tar==target) return data->subtarget;
		}
		break;
		case CONSTRAINT_TYPE_LOCLIKE:
		{
			bLocateLikeConstraint *data = con->data;
			if (data->tar==target) return data->subtarget;
		}
		break;
		case CONSTRAINT_TYPE_ROTLIKE:
		{
			bRotateLikeConstraint *data = con->data;
			if (data->tar==target) return data->subtarget;
		}
		break;
		case CONSTRAINT_TYPE_KINEMATIC:
		{
			bKinematicConstraint *data = con->data;
			if (data->tar==target) return data->subtarget;
		}
		break;
		case CONSTRAINT_TYPE_TRACKTO:
		{
			bTrackToConstraint *data = con->data;
			if (data->tar==target) return data->subtarget;
		}
		break;
		case CONSTRAINT_TYPE_LOCKTRACK:
		{
			bLockTrackConstraint *data = con->data;
			if (data->tar==target) return data->subtarget;
		}
		break;
		case CONSTRAINT_TYPE_STRETCHTO:
		{
			bStretchToConstraint *data = con->data;
			if (data->tar==target) return data->subtarget;
		}
		break;
		case CONSTRAINT_TYPE_FOLLOWPATH: 
			/* wonder if this is relevent, since this constraint 
			 * cannot have a subtarget - theeth 
			 */
		{
			/*
			 * bFollowPathConstraint *data = con->data;
			 */
			return NULL;
		}
		break;
	}
	
	return NULL;  
}

/* checks validity of object pointers, and NULLs,
   if Bone doesnt exist it sets the CONSTRAINT_DISABLE flag */
static void test_constraints (Object *owner, const char* substring)
{
	
	bConstraint *curcon;
	ListBase *conlist= NULL;
	int type;
	
	if (owner==NULL) return;
	
	/* Check parents */
	/* Get the constraint list for this object */
	
	if (strlen (substring)){
		switch (owner->type){
			case OB_ARMATURE:
				type = TARGET_BONE;
				break;
			default:
				type = TARGET_OBJECT;
				break;
		}
	}
	else
		type = TARGET_OBJECT;
	
	
	switch (type){
		case TARGET_OBJECT:
			conlist = &owner->constraints;
			break;
		case TARGET_BONE:
			{
				Bone *bone;
				bPoseChannel *chan;
				
				bone = get_named_bone(((bArmature*)owner->data), substring);
				chan = get_pose_channel (owner->pose, substring);
				if (bone && chan){
					conlist = &chan->constraints;
				}
			}
			break;
	}
	
	/* Cycle constraints */
	if (conlist){
		for (curcon = conlist->first; curcon; curcon=curcon->next){
			curcon->flag &= ~CONSTRAINT_DISABLE;
			
			switch (curcon->type){
				case CONSTRAINT_TYPE_ACTION:
				{
					bActionConstraint *data = curcon->data;
					
					if (!exist_object(data->tar)){
						data->tar = NULL;
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					
					if ( (data->tar == owner) &&
						 (!get_named_bone(get_armature(owner), 
										  data->subtarget))) {
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
				}
					break;
				case CONSTRAINT_TYPE_LOCLIKE:
				{
					bLocateLikeConstraint *data = curcon->data;
					
					if (!exist_object(data->tar)){
						data->tar = NULL;
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					
					if ( (data->tar == owner) &&
						 (!get_named_bone(get_armature(owner), 
										  data->subtarget))) {
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
				}
					break;
				case CONSTRAINT_TYPE_ROTLIKE:
				{
					bRotateLikeConstraint *data = curcon->data;
					
					if (!exist_object(data->tar)){
						data->tar = NULL;
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					
					if ( (data->tar == owner) &&
						 (!get_named_bone(get_armature(owner), 
										  data->subtarget))) {
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
				}
					break;
				case CONSTRAINT_TYPE_KINEMATIC:
				{
					bKinematicConstraint *data = curcon->data;
					if (!exist_object(data->tar)){
						data->tar = NULL;
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					
					if ( (data->tar == owner) &&
						 (!get_named_bone(get_armature(owner), 
										  data->subtarget))) {
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
				}
					break;
				case CONSTRAINT_TYPE_TRACKTO:
				{
					bTrackToConstraint *data = curcon->data;
					if (!exist_object(data->tar)) {
						data->tar = NULL;
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					
					if ( (data->tar == owner) &&
						 (!get_named_bone(get_armature(owner), 
										  data->subtarget))) {
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					if (data->reserved2==data->reserved1){
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					if (data->reserved2+3==data->reserved1){
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
				}
					break;
				case CONSTRAINT_TYPE_LOCKTRACK:
				{
					bLockTrackConstraint *data = curcon->data;
					
					if (!exist_object(data->tar)){
						data->tar = NULL;
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					
					if ( (data->tar == owner) &&
						 (!get_named_bone(get_armature(owner), 
										  data->subtarget))) {
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}

					if (data->lockflag==data->trackflag){
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					if (data->lockflag+3==data->trackflag){
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
				}
					break;
				case CONSTRAINT_TYPE_STRETCHTO:
				{
					bStretchToConstraint *data = curcon->data;
					
					if (!exist_object(data->tar)){
						data->tar = NULL;
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					
					if ( (data->tar == owner) &&
						 (!get_named_bone(get_armature(owner), 
										  data->subtarget))) {
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
				}
					break;
				case CONSTRAINT_TYPE_FOLLOWPATH:
				{
					bFollowPathConstraint *data = curcon->data;
					
					if (!exist_object(data->tar)){
						data->tar = NULL;
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					if (data->tar->type != OB_CURVE){
						data->tar = NULL;
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					if (data->upflag==data->trackflag){
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					if (data->upflag+3==data->trackflag){
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
				}
					break;
			}
		}
	}
}

static void test_bonelist_constraints (Object *owner, ListBase *list)
{
	Bone *bone;

	for (bone = list->first; bone; bone=bone->next) {
		
		test_constraints(owner, bone->name);
		test_bonelist_constraints (owner, &bone->childbase);
	}
}

void object_test_constraints (Object *owner)
{
	test_constraints(owner, "");

	if(owner->type==OB_ARMATURE) {
		bArmature *arm;
		arm = get_armature(owner);
		if (arm)
			test_bonelist_constraints (owner, &arm->bonebase);
	}

}


