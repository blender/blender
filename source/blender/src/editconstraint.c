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
#include "BLI_arithb.h"

#include "DNA_action_types.h"
#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_text_types.h"
#include "DNA_view3d_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_ipo.h"
#include "BKE_object.h"
#include "BKE_utildefines.h"

#include "BIF_editaction.h"
#include "BIF_editarmature.h"
#include "BIF_editconstraint.h"
#include "BIF_poseobject.h"
#include "BIF_interface.h"
#include "BIF_screen.h"
#include "BIF_space.h"
#include "BIF_toolbox.h"

#include "BPY_extern.h"

#include "blendef.h"
#include "nla.h"
#include "mydevice.h"


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
				
				ob->action=add_empty_action("Action");
			}
			
			/* Make sure we have an actionchannel */
			achan = get_action_channel(ob->action, pchan->name);
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
	else {
		if(ob->ipoflag & OB_ACTION_OB) {
			bActionChannel *achan = get_action_channel(ob->action, "Object");
			if(achan)
				return &achan->constraintChannels;
			else 
				return NULL;
		}
		
		return &ob->constraintChannels;
	}
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
					bActionChannel *achan = get_action_channel(ob->action, pchan->name);
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
			ListBase *lb= get_active_constraint_channels(ob, 0);

			if(lb) {
				for(chan= lb->first; chan; chan= chan->next)
					if(!strcmp(chan->name, con->name))
						break;
				return chan;
			}
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
	 
	if (constraint_has_target(con)) {
		Object *tar;
		char *subtarget;
		
		tar = get_constraint_target(con, &subtarget);
		if (tar==target) return subtarget;
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
				case CONSTRAINT_TYPE_PYTHON:
				{
					bPythonConstraint *data = curcon->data;
					float dummy_matrix[4][4];
					
					/* is there are valid script? */
					if (!data->text) {
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					else if (!BPY_is_pyconstraint(data->text)) {
						curcon->flag |= CONSTRAINT_DISABLE;
						break;
					}
					data->flag &= ~PYCON_SCRIPTERROR;
					
					/* does the constraint require target input? */
					if (BPY_pyconstraint_targets(data, dummy_matrix))
						data->flag |= PYCON_USETARGETS;
					else
						data->flag &= ~PYCON_USETARGETS;
					
					/* check whether we have a valid target */
					if (data->flag & PYCON_USETARGETS) {
						/* validate target */
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
					}
					else {
						/* don't hold onto target */
						data->tar = NULL;
						BLI_strncpy(data->subtarget, "", 32);
					}
				}
					break;
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
				case CONSTRAINT_TYPE_MINMAX:
				{
					bMinMaxConstraint *data = curcon->data;
					
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
				case CONSTRAINT_TYPE_SIZELIKE:
				{
					bSizeLikeConstraint *data = curcon->data;
				
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
				case CONSTRAINT_TYPE_CLAMPTO:
				{
					bClampToConstraint *data = curcon->data;
					
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
					else {
						Curve *cu= data->tar->data;
						
						/* auto-set 'Path' setting on curve so this works  */
						cu->flag |= CU_PATH;
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

/* context: active object in posemode, active channel, optional selected channel */
void add_constraint(int only_IK)
{
	Object *ob= OBACT, *obsel=NULL;
	bPoseChannel *pchanact=NULL, *pchansel=NULL;
	bConstraint *con=NULL;
	Base *base;
	short nr;
	
	/* paranoia checks */
	if(ob==NULL || ob==G.obedit) return;

	if(ob->pose && (ob->flag & OB_POSEMODE)) {
		bArmature *arm= ob->data;
		
		/* find active channel */
		pchanact= get_active_posechannel(ob);
		if(pchanact==NULL) return;
	
		/* check protection */
		if(ob->proxy && (pchanact->bone->layer & arm->layer_protected)) {
			error("Bone is Proxy protected");
			return;
		}
		
		/* find selected bone */
		for(pchansel= ob->pose->chanbase.first; pchansel; pchansel= pchansel->next) {
			if(pchansel!=pchanact)
				if(pchansel->bone->flag & BONE_SELECTED) 
					if(pchansel->bone->layer & arm->layer)
						break;
		}
	}
	
	/* find selected object */
	for(base= FIRSTBASE; base; base= base->next)
		if( TESTBASE(base) && base->object!=ob ) 
			obsel= base->object;
	
	/* the only_IK caller has checked for posemode! */
	if(only_IK) {
		for(con= pchanact->constraints.first; con; con= con->next) {
			if(con->type==CONSTRAINT_TYPE_KINEMATIC) break;
		}
		if(con) {
			error("Pose Channel already has IK");
			return;
		}
		
		if(pchansel)
			nr= pupmenu("Add IK Constraint%t|To Active Bone%x10");
		else if(obsel)
			nr= pupmenu("Add IK Constraint%t|To Active Object%x10");
		else 
			nr= pupmenu("Add IK Constraint%t|To New Empty Object%x10|Without Target%x11");
	}
	else {
		if(pchanact) {
			if(pchansel)
				nr= pupmenu("Add Constraint to Active Bone%t|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|%l|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|%l|Track To%x3|Floor%x4|Locked Track%x5|Stretch To%x7|Action%x16|%l|Script%x18");
			else if(obsel && obsel->type==OB_CURVE)
				nr= pupmenu("Add Constraint to Active Object%t|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|%l|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|%l|Track To%x3|Floor%x4|Locked Track%x5|Follow Path%x6|Clamp To%x17|Stretch To%x7|Action%x16|%l|Script%x18");
			else if(obsel)
				nr= pupmenu("Add Constraint to Active Object%t|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|%l|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|%l|Track To%x3|Floor%x4|Locked Track%x5|Stretch To%x7|Action%x16|%l|Script%x18");
			else
				nr= pupmenu("Add Constraint to New Empty Object%t|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|%l|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|%l|Track To%x3|Floor%x4|Locked Track%x5|Stretch To%x7|%l|Script%x18");
		}
		else {
			if(obsel && obsel->type==OB_CURVE)
				nr= pupmenu("Add Constraint to Active Object%t|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|%l|Track To%x3|Floor%x4|Locked Track%x5|Follow Path%x6|Clamp To%x17|%l|Script%x18");
			else if(obsel)
				nr= pupmenu("Add Constraint to Active Object%t|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|%l|Track To%x3|Floor%x4|Locked Track%x5|%l|Script%x18");
			else
				nr= pupmenu("Add Constraint to New Empty Object%t|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|%l|Track To%x3|Floor%x4|Locked Track%x5|%l|Script%x18");
		}
	}
	
	if(nr<1) return;
	
	/* handle IK separate */
	if(nr==10 || nr==11) {
		
		/* prevent weird chains... */
		if(pchansel) {
			bPoseChannel *pchan= pchanact;
			while(pchan) {
				if(pchan==pchansel) break;
				pchan= pchan->parent;
			}
			if(pchan) {
				error("IK root cannot be linked to IK tip");
				return;
			}
			pchan= pchansel;
			while(pchan) {
				if(pchan==pchanact) break;
				pchan= pchan->parent;
			}
			if(pchan) {
				error("IK tip cannot be linked to IK root");
				return;
			}		
		}
		
		con = add_new_constraint(CONSTRAINT_TYPE_KINEMATIC);
		BLI_addtail(&pchanact->constraints, con);
		unique_constraint_name(con, &pchanact->constraints);
		pchanact->constflag |= PCHAN_HAS_IK;	// for draw, but also for detecting while pose solving
		if(nr==11) pchanact->constflag |= PCHAN_HAS_TARGET;
	}
	else {
		
		if(nr==1) con = add_new_constraint(CONSTRAINT_TYPE_LOCLIKE);
		else if(nr==2) con = add_new_constraint(CONSTRAINT_TYPE_ROTLIKE);
		else if(nr==3) con = add_new_constraint(CONSTRAINT_TYPE_TRACKTO);
		else if(nr==4) con = add_new_constraint(CONSTRAINT_TYPE_MINMAX);
		else if(nr==5) con = add_new_constraint(CONSTRAINT_TYPE_LOCKTRACK);
		else if(nr==6) {
			Curve *cu= obsel->data;
			cu->flag |= CU_PATH;
			con = add_new_constraint(CONSTRAINT_TYPE_FOLLOWPATH);
		}
		else if(nr==7) con = add_new_constraint(CONSTRAINT_TYPE_STRETCHTO);
		else if(nr==8) con = add_new_constraint(CONSTRAINT_TYPE_SIZELIKE);
		else if(nr==13) con = add_new_constraint(CONSTRAINT_TYPE_LOCLIMIT);
		else if(nr==14) con = add_new_constraint(CONSTRAINT_TYPE_ROTLIMIT);
		else if(nr==15) con = add_new_constraint(CONSTRAINT_TYPE_SIZELIMIT);
		else if(nr==16) con = add_new_constraint(CONSTRAINT_TYPE_ACTION);
		else if(nr==17) {
			Curve *cu= obsel->data;
			cu->flag |= CU_PATH;
			con = add_new_constraint(CONSTRAINT_TYPE_CLAMPTO);
		}
		else if(nr==18) {	
			char *menustr;
			int scriptint= 0, dummy_active=0;
			
			/* popup a list of usable scripts */
			menustr = buildmenu_pyconstraints(NULL, &dummy_active);
			scriptint = pupmenu(menustr);
			MEM_freeN(menustr);
			
			/* only add constraint if a script was chosen */
			if (scriptint) {
				con = add_new_constraint(CONSTRAINT_TYPE_PYTHON);
				validate_pyconstraint_cb(con->data, &scriptint);
			}
		}
		
		if(con==NULL) return;	/* paranoia */
		
		if(pchanact) {
			BLI_addtail(&pchanact->constraints, con);
			unique_constraint_name(con, &pchanact->constraints);
			pchanact->constflag |= PCHAN_HAS_CONST;	/* for draw */
		}
		else {
			BLI_addtail(&ob->constraints, con);
			unique_constraint_name(con, &ob->constraints);
		}
	}
	
	/* set the target */
	if(pchansel) {
		set_constraint_target(con, ob, pchansel->name);
	}
	else if(obsel) {
		set_constraint_target(con, obsel, NULL);
	}
	else if(ELEM4(nr, 11, 13, 14, 15)==0) {	/* add new empty as target */
		Base *base= BASACT, *newbase;
		Object *obt;
		
		obt= add_object(OB_EMPTY);
		/* set layers OK */
		newbase= BASACT;
		newbase->lay= base->lay;
		obt->lay= newbase->lay;
		
		/* transform cent to global coords for loc */
		if(pchanact) {
			if(only_IK)
				VecMat4MulVecfl(obt->loc, ob->obmat, pchanact->pose_tail);
			else
				VecMat4MulVecfl(obt->loc, ob->obmat, pchanact->pose_head);
		}
		else
			VECCOPY(obt->loc, ob->obmat[3]);
		
		set_constraint_target(con, obt, NULL);
		
		/* restore, add_object sets active */
		BASACT= base;
		base->flag |= SELECT;
	}


	/* active flag */
	con->flag |= CONSTRAINT_ACTIVE;
	for(con= con->prev; con; con= con->prev)
		con->flag &= ~CONSTRAINT_ACTIVE;

	DAG_scene_sort(G.scene);		// sort order of objects
	
	if(pchanact) {
		ob->pose->flag |= POSE_RECALC;	// sort pose channels
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);	// and all its relations
	}
	else
		DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);	// and all its relations

	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWBUTSOBJECT, 0);
	allqueue (REDRAWOOPS, 0);
	
	if(only_IK)
		BIF_undo_push("Add IK Constraint");
	else
		BIF_undo_push("Add Constraint");

}

void ob_clear_constraints(void)
{
	Object *ob= OBACT;
	
	/* paranoia checks */
	if(!ob) return;
	if(ob==G.obedit || (ob->flag & OB_POSEMODE)) return;
	
	if(okee("Clear Constraints")==0) return;
	
	free_constraints(&ob->constraints);
	
	DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
	
	allqueue (REDRAWVIEW3D, 0);
	allqueue (REDRAWBUTSOBJECT, 0);
	allqueue (REDRAWOOPS, 0);
	
	BIF_undo_push("Clear Constraint(s)");
	
}

/* con already has the new name */
void rename_constraint(Object *ob, bConstraint *con, char *oldname)
{
	bConstraint *tcon;
	bConstraintChannel *conchan;
	ListBase *conlist= NULL;
	int from_object= 0;
	char *channame="";
	
	/* get context by searching for con (primitive...) */
	for(tcon= ob->constraints.first; tcon; tcon= tcon->next)
		if(tcon==con)
			break;
	
	if(tcon) {
		conlist= &ob->constraints;
		channame= "Object";
		from_object= 1;
	}
	else if(ob->pose) {
		bPoseChannel *pchan;
		
		for(pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			for(tcon= pchan->constraints.first; tcon; tcon= tcon->next) {
				if(tcon==con)
					break;
			}
			if(tcon)
				break;
		}
		if(tcon) {
			conlist= &pchan->constraints;
			channame= pchan->name;
		}
	}
	
	if(conlist==NULL) {
		printf("rename constraint failed\n");	/* should not happen in UI */
		return;
	}
	
	/* first make sure it's a unique name within context */
	unique_constraint_name (con, conlist);

	/* own channels */
	if(from_object) {
		for(conchan= ob->constraintChannels.first; conchan; conchan= conchan->next) {
			if( strcmp(oldname, conchan->name)==0 )
				BLI_strncpy(conchan->name, con->name, sizeof(conchan->name));
		}
	}
	/* own action */
	if(ob->action) {
		bActionChannel *achan= get_action_channel(ob->action, channame);
		if(achan) {
			conchan= get_constraint_channel(&achan->constraintChannels, oldname);
			if(conchan)
				BLI_strncpy(conchan->name, con->name, sizeof(conchan->name));
		}
	}
	
}

/* ********************** CONSTRAINT-SPECIFIC STUFF ********************* */
/* ------------- PyConstraints ------------------ */

/* this callback sets the text-file to be used for selected menu item */
void validate_pyconstraint_cb(void *arg1, void *arg2)
{
	bPythonConstraint *data = arg1;
	Text *text;
	int index = *((int *)arg2);
	int i;
	
	/* innovative use of a for loop to search */
	for (text=G.main->text.first, i=1; text && index!=i; i++, text=text->id.next);
	data->text = text;
}

/* this returns a string for the list of usable pyconstraint script names */
char *buildmenu_pyconstraints(Text *con_text, int *pyconindex)
{
	Text *text;
	char *menustr = MEM_callocN(128, "menustr pyconstraints");
	char *name, stmp[128];
	int buf = 128;
	int used = strlen("Scripts: %t") + 1;
	int i;
		
	sprintf(menustr, "%s", "Scripts: %t");
	
	for (text=G.main->text.first, i=1; text; i++, text=text->id.next) {
		/* this is important to ensure that right script is shown as active */
		if (text == con_text) *pyconindex = i;
		
		/* menu entry is length of name + 3(len(|%X)) + 6 characters for the int.*/
		if (BPY_is_pyconstraint(text)) {
			name= text->id.name;
			if (strlen(name)+used+10 >= buf) {
				char *newbuf = MEM_callocN(buf+128, "menustr pyconstraints 2");
				memcpy(newbuf, menustr, used);
				MEM_freeN(menustr);
				menustr = newbuf;
				buf += 128;
			}
			sprintf(stmp, "|%s%%x%d", name, i);
			strcat(menustr, stmp);
			used += strlen(name)+10;
		}
	}
	
	return menustr;
}
