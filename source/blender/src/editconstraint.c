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
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_arithb.h"
#include "BLI_dynstr.h"

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

/* -------------- Get Active Constraint Data ---------------------- */

ListBase *get_active_constraint_channels (Object *ob, int forcevalid)
{
	char ipstr[64];
	
	if (ob == NULL)
		return NULL;
	
	/* See if we are a bone constraint */
	if (ob->flag & OB_POSEMODE) {
		bActionChannel *achan;
		bPoseChannel *pchan;
		
		pchan = get_active_posechannel(ob);
		if (pchan) {
			/* Make sure we have an action */
			if (ob->action == NULL) {
				if (forcevalid == 0)
					return NULL;
				
				ob->action= add_empty_action("Action");
			}
			
			/* Make sure we have an actionchannel */
			achan = get_action_channel(ob->action, pchan->name);
			if (achan == NULL) {
				if (forcevalid == 0)
					return NULL;
				
				achan = MEM_callocN (sizeof(bActionChannel), "ActionChannel");
				
				strcpy(achan->name, pchan->name);
				sprintf(ipstr, "%s.%s", ob->action->id.name+2, achan->name);
				ipstr[23]=0;
				achan->ipo=	add_ipo(ipstr, ID_AC);	
				
				BLI_addtail(&ob->action->chanbase, achan);
			}
			
			return &achan->constraintChannels;
		}
		else 
			return NULL;
	}
	/* else we return object constraints */
	else {
		if (ob->ipoflag & OB_ACTION_OB) {
			bActionChannel *achan = get_action_channel(ob->action, "Object");
			if (achan)
				return &achan->constraintChannels;
			else 
				return NULL;
		}
		
		return &ob->constraintChannels;
	}
}


/* if object in posemode, active bone constraints, else object constraints */
ListBase *get_active_constraints (Object *ob)
{
	if (ob == NULL)
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
bConstraint *get_active_constraint (Object *ob)
{
	ListBase *lb= get_active_constraints(ob);

	if (lb) {
		bConstraint *con;
		
		for (con= lb->first; con; con=con->next) {
			if (con->flag & CONSTRAINT_ACTIVE)
				return con;
		}
	}
	
	return NULL;
}

/* single channel, for ipo */
bConstraintChannel *get_active_constraint_channel (Object *ob)
{
	bConstraint *con;
	bConstraintChannel *chan;
	
	if (ob->flag & OB_POSEMODE) {
		if (ob->action) {
			bPoseChannel *pchan;
			
			pchan = get_active_posechannel(ob);
			if (pchan) {
				for (con= pchan->constraints.first; con; con= con->next) {
					if (con->flag & CONSTRAINT_ACTIVE)
						break;
				}
				
				if (con) {
					bActionChannel *achan = get_action_channel(ob->action, pchan->name);
					if (achan) {
						for (chan= achan->constraintChannels.first; chan; chan= chan->next) {
							if (!strcmp(chan->name, con->name))
								break;
						}
						return chan;
					}
				}
			}
		}
	}
	else {
		for (con= ob->constraints.first; con; con= con->next) {
			if (con->flag & CONSTRAINT_ACTIVE)
				break;
		}
		
		if (con) {
			ListBase *lb= get_active_constraint_channels(ob, 0);
			
			if (lb) {
				for (chan= lb->first; chan; chan= chan->next) {
					if (!strcmp(chan->name, con->name))
						break;
				}
				
				return chan;
			}
		}
	}
	
	return NULL;
}

/* -------------- Constraint Management (Add New, Remove, Rename) -------------------- */

/* Creates a new constraint, initialises its data, and returns it */
bConstraint *add_new_constraint (short type)
{
	bConstraint *con;
	bConstraintTypeInfo *cti;

	con = MEM_callocN(sizeof(bConstraint), "Constraint");
	
	/* Set up a generic constraint datablock */
	con->type = type;
	con->flag |= CONSTRAINT_EXPAND;
	con->enforce = 1.0F;
	strcpy(con->name, "Const");
	
	/* Load the data for it */
	cti = constraint_get_typeinfo(con);
	if (cti) {
		con->data = MEM_callocN(cti->size, cti->structName);
		
		/* only constraints that change any settings need this */
		if (cti->new_data)
			cti->new_data(con->data);
	}
	
	return con;
}

/* Adds the given constraint to the Object-level set of constraints for the given Object */
void add_constraint_to_object (bConstraint *con, Object *ob)
{
	ListBase *list;
	list = &ob->constraints;
	
	if (list) {
		unique_constraint_name(con, list);
		BLI_addtail(list, con);
		
		if (proxylocked_constraints_owner(ob, NULL))
			con->flag |= CONSTRAINT_PROXY_LOCAL;
		
		con->flag |= CONSTRAINT_ACTIVE;
		for (con= con->prev; con; con= con->prev)
			con->flag &= ~CONSTRAINT_ACTIVE;
	}
}

/* helper function for add_constriant - sets the last target for the active constraint */
static void set_constraint_nth_target (bConstraint *con, Object *target, char subtarget[], int index)
{
	bConstraintTypeInfo *cti= constraint_get_typeinfo(con);
	ListBase targets = {NULL, NULL};
	bConstraintTarget *ct;
	int num_targets, i;
	
	if (cti && cti->get_constraint_targets) {
		cti->get_constraint_targets(con, &targets);
		num_targets= BLI_countlist(&targets);
		
		if (index < 0) {
			if (abs(index) < num_targets)
				index= num_targets - abs(index);
			else
				index= num_targets - 1;
		}
		else if (index >= num_targets) {
			index= num_targets - 1;
		}
		
		for (ct=targets.first, i=0; ct; ct= ct->next, i++) {
			if (i == index) {
				ct->tar= target;
				strcpy(ct->subtarget, subtarget);
				break;
			}
		}
		
		if (cti->flush_constraint_targets)
			cti->flush_constraint_targets(con, &targets, 0);
	}
}

/* context: active object in posemode, active channel, optional selected channel */
void add_constraint (short only_IK)
{
	Object *ob= OBACT, *obsel=NULL;
	bPoseChannel *pchanact=NULL, *pchansel=NULL;
	bConstraint *con=NULL;
	Base *base;
	short nr;
	
	/* paranoia checks */
	if ((ob==NULL) || (ob==G.obedit)) 
		return;

	if ((ob->pose) && (ob->flag & OB_POSEMODE)) {
		bArmature *arm= ob->data;
		
		/* find active channel */
		pchanact= get_active_posechannel(ob);
		if (pchanact==NULL) 
			return;
		
		/* find selected bone */
		for (pchansel=ob->pose->chanbase.first; pchansel; pchansel=pchansel->next) {
			if (pchansel != pchanact) {
				if (pchansel->bone->flag & BONE_SELECTED)  {
					if (pchansel->bone->layer & arm->layer)
						break;
				}
			}
		}
	}
	
	/* find selected object */
	for (base= FIRSTBASE; base; base= base->next) {
		if ((TESTBASE(base)) && (base->object!=ob)) 
			obsel= base->object;
	}
	
	/* the only_IK caller has checked for posemode! */
	if (only_IK) {
		for (con= pchanact->constraints.first; con; con= con->next) {
			if (con->type==CONSTRAINT_TYPE_KINEMATIC) break;
		}
		if (con) {
			error("Pose Channel already has IK");
			return;
		}
		
		if (pchansel)
			nr= pupmenu("Add IK Constraint%t|To Active Bone%x10");
		else if (obsel)
			nr= pupmenu("Add IK Constraint%t|To Active Object%x10");
		else 
			nr= pupmenu("Add IK Constraint%t|To New Empty Object%x10|Without Target%x11");
	}
	else {
		if (pchanact) {
			if (pchansel)
				nr= pupmenu("Add Constraint to Active Bone%t|Child Of%x19|Transformation%x20|%l|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|%l|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|Limit Distance%x21|%l|Track To%x3|Floor%x4|Locked Track%x5|Stretch To%x7|%l|Action%x16|Script%x18");
			else if ((obsel) && (obsel->type==OB_CURVE))
				nr= pupmenu("Add Constraint to Active Object%t|Child Of%x19|Transformation%x20|%l|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|%l|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|Limit Distance%x21|%l|Track To%x3|Floor%x4|Locked Track%x5|Follow Path%x6|Clamp To%x17|Stretch To%x7|%l|Action%x16|Script%x18");
			else if (obsel)
				nr= pupmenu("Add Constraint to Active Object%t|Child Of%x19|Transformation%x20|%l|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|%l|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|Limit Distance%x21|%l|Track To%x3|Floor%x4|Locked Track%x5|Stretch To%x7|%l|Action%x16|Script%x18");
			else
				nr= pupmenu("Add Constraint to New Empty Object%t|Child Of%x19|Transformation%x20|%l|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|%l|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|Limit Distance%x21|%l|Track To%x3|Floor%x4|Locked Track%x5|Stretch To%x7|%l|Action%x16|Script%x18");
		}
		else {
			if ((obsel) && (obsel->type==OB_CURVE))
				nr= pupmenu("Add Constraint to Active Object%t|Child Of%x19|Transformation%x20|%l|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|%l|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|Limit Distance%x21|%l|Track To%x3|Floor%x4|Locked Track%x5|Follow Path%x6|Clamp To%x17|%l|Action%x16|Script%x18");
			else if (obsel)
				nr= pupmenu("Add Constraint to Active Object%t|Child Of%x19|Transformation%x20|%l|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|%l|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|Limit Distance%x21|%l|Track To%x3|Floor%x4|Locked Track%x5|%l|Action%x16|Script%x18");
			else
				nr= pupmenu("Add Constraint to New Empty Object%t|Child Of%x19|Transformation%x20|%l|Copy Location%x1|Copy Rotation%x2|Copy Scale%x8|%l|Limit Location%x13|Limit Rotation%x14|Limit Scale%x15|Limit Distance%x21|%l|Track To%x3|Floor%x4|Locked Track%x5|%l|Action%x16|Script%x18");
		}
	}
	
	if (nr < 1) return;
	
	/* handle IK separate */
	if (nr==10 || nr==11) {
		/* ik - prevent weird chains... */
		if (pchansel) {
			bPoseChannel *pchan= pchanact;
			while (pchan) {
				if (pchan==pchansel) break;
				pchan= pchan->parent;
			}
			if (pchan) {
				error("IK root cannot be linked to IK tip");
				return;
			}
			
			pchan= pchansel;
			while (pchan) {
				if (pchan==pchanact) break;
				pchan= pchan->parent;
			}
			if (pchan) {
				error("IK tip cannot be linked to IK root");
				return;
			}		
		}
		
		con = add_new_constraint(CONSTRAINT_TYPE_KINEMATIC);
		BLI_addtail(&pchanact->constraints, con);
		unique_constraint_name(con, &pchanact->constraints);
		pchanact->constflag |= PCHAN_HAS_IK;	/* for draw, but also for detecting while pose solving */
		if (nr==11) 
			pchanact->constflag |= PCHAN_HAS_TARGET;
		if (proxylocked_constraints_owner(ob, pchanact))
			con->flag |= CONSTRAINT_PROXY_LOCAL;
	}
	else {
		/* normal constraints - add data */
		if (nr==1) con = add_new_constraint(CONSTRAINT_TYPE_LOCLIKE);
		else if (nr==2) con = add_new_constraint(CONSTRAINT_TYPE_ROTLIKE);
		else if (nr==3) con = add_new_constraint(CONSTRAINT_TYPE_TRACKTO);
		else if (nr==4) con = add_new_constraint(CONSTRAINT_TYPE_MINMAX);
		else if (nr==5) con = add_new_constraint(CONSTRAINT_TYPE_LOCKTRACK);
		else if (nr==6) {
			Curve *cu= obsel->data;
			cu->flag |= CU_PATH;
			con = add_new_constraint(CONSTRAINT_TYPE_FOLLOWPATH);
		}
		else if (nr==7) con = add_new_constraint(CONSTRAINT_TYPE_STRETCHTO);
		else if (nr==8) con = add_new_constraint(CONSTRAINT_TYPE_SIZELIKE);
		else if (nr==13) con = add_new_constraint(CONSTRAINT_TYPE_LOCLIMIT);
		else if (nr==14) con = add_new_constraint(CONSTRAINT_TYPE_ROTLIMIT);
		else if (nr==15) con = add_new_constraint(CONSTRAINT_TYPE_SIZELIMIT);
		else if (nr==16) {
			/* TODO: add a popup-menu to display list of available actions to use (like for pyconstraints) */
			con = add_new_constraint(CONSTRAINT_TYPE_ACTION);
		}
		else if (nr==17) {
			Curve *cu= obsel->data;
			cu->flag |= CU_PATH;
			con = add_new_constraint(CONSTRAINT_TYPE_CLAMPTO);
		}
		else if (nr==18) {	
			char *menustr;
			int scriptint= 0;
			
			/* popup a list of usable scripts */
			menustr = buildmenu_pyconstraints(NULL, &scriptint);
			scriptint = pupmenu(menustr);
			MEM_freeN(menustr);
			
			/* only add constraint if a script was chosen */
			if (scriptint) {
				/* add constraint */
				con = add_new_constraint(CONSTRAINT_TYPE_PYTHON);
				validate_pyconstraint_cb(con->data, &scriptint);
				
				/* make sure target allowance is set correctly */
				BPY_pyconstraint_update(ob, con);
			}
		}
		else if (nr==19) {
			con = add_new_constraint(CONSTRAINT_TYPE_CHILDOF);
			
			/* if this constraint is being added to a posechannel, make sure
			 * the constraint gets evaluated in pose-space
			 */
			if (pchanact) {
				con->ownspace = CONSTRAINT_SPACE_POSE;
				con->flag |= CONSTRAINT_SPACEONCE;
			}
		}
		else if (nr==20) con = add_new_constraint(CONSTRAINT_TYPE_TRANSFORM);
		else if (nr==21) con = add_new_constraint(CONSTRAINT_TYPE_DISTLIMIT);
		
		if (con==NULL) return;	/* paranoia */
		
		if (pchanact) {
			BLI_addtail(&pchanact->constraints, con);
			unique_constraint_name(con, &pchanact->constraints);
			pchanact->constflag |= PCHAN_HAS_CONST;	/* for draw */
			if (proxylocked_constraints_owner(ob, pchanact))
				con->flag |= CONSTRAINT_PROXY_LOCAL;
		}
		else {
			BLI_addtail(&ob->constraints, con);
			unique_constraint_name(con, &ob->constraints);
			if (proxylocked_constraints_owner(ob, NULL))
				con->flag |= CONSTRAINT_PROXY_LOCAL;
		}
	}
	
	/* set the target */
	if (pchansel) {
		set_constraint_nth_target(con, ob, pchansel->name, 0);
	}
	else if (obsel) {
		set_constraint_nth_target(con, obsel, "", 0);
	}
	else if (ELEM4(nr, 11, 13, 14, 15)==0) {	/* add new empty as target */
		Base *base= BASACT, *newbase;
		Object *obt;
		
		obt= add_object(OB_EMPTY);
		/* set layers OK */
		newbase= BASACT;
		newbase->lay= base->lay;
		obt->lay= newbase->lay;
		
		/* transform cent to global coords for loc */
		if (pchanact) {
			if (only_IK)
				VecMat4MulVecfl(obt->loc, ob->obmat, pchanact->pose_tail);
			else
				VecMat4MulVecfl(obt->loc, ob->obmat, pchanact->pose_head);
		}
		else
			VECCOPY(obt->loc, ob->obmat[3]);
		
		set_constraint_nth_target(con, obt, "", 0);
		
		/* restore, add_object sets active */
		BASACT= base;
		base->flag |= SELECT;
	}
	
	/* active flag */
	con->flag |= CONSTRAINT_ACTIVE;
	for (con= con->prev; con; con= con->prev)
		con->flag &= ~CONSTRAINT_ACTIVE;

	DAG_scene_sort(G.scene);		// sort order of objects
	
	if (pchanact) {
		ob->pose->flag |= POSE_RECALC;	// sort pose channels
		DAG_object_flush_update(G.scene, ob, OB_RECALC_DATA);	// and all its relations
	}
	else
		DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);	// and all its relations

	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWOOPS, 0);
	
	if (only_IK)
		BIF_undo_push("Add IK Constraint");
	else
		BIF_undo_push("Add Constraint");

}

/* Remove all constraints from the active object */
void ob_clear_constraints (void)
{
	Object *ob= OBACT;
	
	/* paranoia checks */
	if ((ob==NULL) || (ob==G.obedit) || (ob->flag & OB_POSEMODE)) 
		return;
	
	/* get user permission */
	if (okee("Clear Constraints")==0) 
		return;
	
	/* do freeing */
	free_constraints(&ob->constraints);
	
	/* do updates */
	DAG_object_flush_update(G.scene, ob, OB_RECALC_OB);
	
	allqueue(REDRAWVIEW3D, 0);
	allqueue(REDRAWBUTSOBJECT, 0);
	allqueue(REDRAWOOPS, 0);
	
	BIF_undo_push("Clear Constraint(s)");
}

/* Rename the given constraint 
 *	- con already has the new name 
 */
void rename_constraint (Object *ob, bConstraint *con, char *oldname)
{
	bConstraint *tcon;
	bConstraintChannel *conchan;
	ListBase *conlist= NULL;
	int from_object= 0;
	char *channame="";
	
	/* get context by searching for con (primitive...) */
	for (tcon= ob->constraints.first; tcon; tcon= tcon->next) {
		if (tcon==con)
			break;
	}
	
	if (tcon) {
		conlist= &ob->constraints;
		channame= "Object";
		from_object= 1;
	}
	else if (ob->pose) {
		bPoseChannel *pchan;
		
		for (pchan=ob->pose->chanbase.first; pchan; pchan=pchan->next) {
			for (tcon= pchan->constraints.first; tcon; tcon= tcon->next) {
				if (tcon==con)
					break;
			}
			if (tcon) 
				break;
		}
		
		if (tcon) {
			conlist= &pchan->constraints;
			channame= pchan->name;
		}
	}
	
	if (conlist==NULL) {
		printf("rename constraint failed\n");	/* should not happen in UI */
		return;
	}
	
	/* first make sure it's a unique name within context */
	unique_constraint_name (con, conlist);

	/* own channels */
	if (from_object) {
		for (conchan= ob->constraintChannels.first; conchan; conchan= conchan->next) {
			if ( strcmp(oldname, conchan->name)==0 )
				BLI_strncpy(conchan->name, con->name, sizeof(conchan->name));
		}
	}
	
	/* own action */
	if (ob->action) {
		bActionChannel *achan= get_action_channel(ob->action, channame);
		if (achan) {
			conchan= get_constraint_channel(&achan->constraintChannels, oldname);
			if (conchan)
				BLI_strncpy(conchan->name, con->name, sizeof(conchan->name));
		}
	}
}


/* ------------- Constraint Sanity Testing ------------------- */

/* checks validity of object pointers, and NULLs,
 * if Bone doesnt exist it sets the CONSTRAINT_DISABLE flag 
 */
static void test_constraints (Object *owner, const char substring[])
{
	bConstraint *curcon;
	ListBase *conlist= NULL;
	int type;
	
	if (owner==NULL) return;
	
	/* Check parents */
	if (strlen(substring)) {
		switch (owner->type) {
			case OB_ARMATURE:
				type = CONSTRAINT_OBTYPE_BONE;
				break;
			default:
				type = CONSTRAINT_OBTYPE_OBJECT;
				break;
		}
	}
	else
		type = CONSTRAINT_OBTYPE_OBJECT;
	
	/* Get the constraint list for this object */
	switch (type) {
		case CONSTRAINT_OBTYPE_OBJECT:
			conlist = &owner->constraints;
			break;
		case CONSTRAINT_OBTYPE_BONE:
			{
				Bone *bone;
				bPoseChannel *chan;
				
				bone = get_named_bone( ((bArmature *)owner->data), substring );
				chan = get_pose_channel(owner->pose, substring);
				if (bone && chan) {
					conlist = &chan->constraints;
				}
			}
			break;
	}
	
	/* Check all constraints - is constraint valid? */
	if (conlist) {
		for (curcon = conlist->first; curcon; curcon=curcon->next) {
			bConstraintTypeInfo *cti= constraint_get_typeinfo(curcon);
			ListBase targets = {NULL, NULL};
			bConstraintTarget *ct;
			
			/* clear disabled-flag first */
			curcon->flag &= ~CONSTRAINT_DISABLE;
			
			/* Check specialised data (settings) for constraints that need this */
			if (curcon->type == CONSTRAINT_TYPE_PYTHON) {
				bPythonConstraint *data = curcon->data;
				
				/* is there are valid script? */
				if (data->text == NULL) {
					curcon->flag |= CONSTRAINT_DISABLE;
				}
				else if (BPY_is_pyconstraint(data->text)==0) {
					curcon->flag |= CONSTRAINT_DISABLE;
				}
				else {	
					/* does the constraint require target input... also validates targets */
					BPY_pyconstraint_update(owner, curcon);
				}
				
				/* targets have already been checked for this */
				continue;
			}
			else if (curcon->type == CONSTRAINT_TYPE_KINEMATIC) {
				bKinematicConstraint *data = curcon->data;
				
				/* bad: we need a separate set of checks here as poletarget is 
				 *		optional... otherwise poletarget must exist too or else
				 *		the constraint is deemed invalid
				 */
				if (exist_object(data->tar) == 0) {
					data->tar = NULL;
					curcon->flag |= CONSTRAINT_DISABLE;
				}
				else if (data->tar == owner) {
					if (!get_named_bone(get_armature(owner), data->subtarget)) {
						curcon->flag |= CONSTRAINT_DISABLE;
					}
				}
				
				if (data->poletar) {
					if (exist_object(data->poletar) == 0) {
						data->poletar = NULL;
						curcon->flag |= CONSTRAINT_DISABLE;
					}
					else if (data->poletar == owner) {
						if (!get_named_bone(get_armature(owner), data->polesubtarget)) {
							curcon->flag |= CONSTRAINT_DISABLE;
						}
					}
				}
				
				/* targets have already been checked for this */
				continue;
			}
			else if (curcon->type == CONSTRAINT_TYPE_ACTION) {
				bActionConstraint *data = curcon->data;
				
				/* validate action */
				if (data->act == NULL) 
					curcon->flag |= CONSTRAINT_DISABLE;
			}
			else if (curcon->type == CONSTRAINT_TYPE_FOLLOWPATH) {
				bFollowPathConstraint *data = curcon->data;
				
				/* don't allow track/up axes to be the same */
				if (data->upflag==data->trackflag)
					curcon->flag |= CONSTRAINT_DISABLE;
				if (data->upflag+3==data->trackflag)
					curcon->flag |= CONSTRAINT_DISABLE;
			}
			else if (curcon->type == CONSTRAINT_TYPE_TRACKTO) {
				bTrackToConstraint *data = curcon->data;
				
				/* don't allow track/up axes to be the same */
				if (data->reserved2==data->reserved1)
					curcon->flag |= CONSTRAINT_DISABLE;
				if (data->reserved2+3==data->reserved1)
					curcon->flag |= CONSTRAINT_DISABLE;
			}
			else if (curcon->type == CONSTRAINT_TYPE_LOCKTRACK) {
				bLockTrackConstraint *data = curcon->data;
				
				if (data->lockflag==data->trackflag)
					curcon->flag |= CONSTRAINT_DISABLE;
				if (data->lockflag+3==data->trackflag)
					curcon->flag |= CONSTRAINT_DISABLE;
			}
			
			/* Check targets for constraints */
			if (cti && cti->get_constraint_targets) {
				cti->get_constraint_targets(curcon, &targets);
				
				/* disable and clear constraints targets that are incorrect */
				for (ct= targets.first; ct; ct= ct->next) {
					/* general validity checks (for those constraints that need this) */
					if (exist_object(ct->tar) == 0) {
						ct->tar = NULL;
						curcon->flag |= CONSTRAINT_DISABLE;
					}
					else if (ct->tar == owner) {
						if (!get_named_bone(get_armature(owner), ct->subtarget)) {
							curcon->flag |= CONSTRAINT_DISABLE;
						}
					}
					
					/* target checks for specific constraints */
					if (ELEM(curcon->type, CONSTRAINT_TYPE_FOLLOWPATH, CONSTRAINT_TYPE_CLAMPTO)) {
						if (ct->tar) {
							if (ct->tar->type != OB_CURVE) {
								ct->tar= NULL;
								curcon->flag |= CONSTRAINT_DISABLE;
							}
							else {
								Curve *cu= ct->tar->data;
								
								/* auto-set 'Path' setting on curve so this works  */
								cu->flag |= CU_PATH;
							}
						}						
					}
				}	
				
				/* free any temporary targets */
				if (cti->flush_constraint_targets)
					cti->flush_constraint_targets(curcon, &targets, 0);
			}
		}
	}
}

static void test_bonelist_constraints (Object *owner, ListBase *list)
{
	Bone *bone;

	for (bone = list->first; bone; bone = bone->next) {
		test_constraints(owner, bone->name);
		test_bonelist_constraints(owner, &bone->childbase);
	}
}

void object_test_constraints (Object *owner)
{
	test_constraints(owner, "");

	if (owner->type==OB_ARMATURE) {
		bArmature *arm= get_armature(owner);
		
		if (arm)
			test_bonelist_constraints(owner, &arm->bonebase);
	}
}

/* ********************** CONSTRAINT-SPECIFIC STUFF ********************* */
/* ------------- PyConstraints ------------------ */

/* this callback sets the text-file to be used for selected menu item */
void validate_pyconstraint_cb (void *arg1, void *arg2)
{
	bPythonConstraint *data = arg1;
	Text *text= NULL;
	int index = *((int *)arg2);
	int i;
	
	/* exception for no script */
	if (index) {
		/* innovative use of a for...loop to search */
		for (text=G.main->text.first, i=1; text && index!=i; i++, text=text->id.next);
	}
	data->text = text;
}

/* this returns a string for the list of usable pyconstraint script names */
char *buildmenu_pyconstraints (Text *con_text, int *pyconindex)
{
	DynStr *pupds= BLI_dynstr_new();
	Text *text;
	char *str;
	char buf[64];
	int i;
	
	/* add title first */
	sprintf(buf, "Scripts: %%t|[None]%%x0|");
	BLI_dynstr_append(pupds, buf);
	
	/* init active-index first */
	if (con_text == NULL)
		*pyconindex= 0;
	
	/* loop through markers, adding them */
	for (text=G.main->text.first, i=1; text; i++, text=text->id.next) {
		/* this is important to ensure that right script is shown as active */
		if (text == con_text) *pyconindex = i;
		
		/* only include valid pyconstraint scripts */
		if (BPY_is_pyconstraint(text)) {
			BLI_dynstr_append(pupds, text->id.name+2);
			
			sprintf(buf, "%%x%d", i);
			BLI_dynstr_append(pupds, buf);
			
			if (text->id.next)
				BLI_dynstr_append(pupds, "|");
		}
	}
	
	/* convert to normal MEM_malloc'd string */
	str= BLI_dynstr_get_cstring(pupds);
	BLI_dynstr_free(pupds);
	
	return str;
}

/* this callback gets called when the 'refresh' button of a pyconstraint gets pressed */
void update_pyconstraint_cb (void *arg1, void *arg2)
{
	Object *owner= (Object *)arg1;
	bConstraint *con= (bConstraint *)arg2;
	
	if (owner && con)
		BPY_pyconstraint_update(owner, con);
}

/* ------------- Child-Of Constraint ------------------ */

/* ChildOf Constraint - set inverse callback */
void childof_const_setinv (void *conv, void *unused)
{
	bConstraint *con= (bConstraint *)conv;
	bChildOfConstraint *data= (bChildOfConstraint *)con->data;
	Object *ob= OBACT;
	bPoseChannel *pchan= NULL;

	/* try to find a pose channel */
	if (ob && ob->pose)
		pchan= get_active_posechannel(ob);
	
	/* calculate/set inverse matrix */
	if (pchan) {
		float pmat[4][4], cinf;
		float imat[4][4], tmat[4][4];
		
		/* make copy of pchan's original pose-mat (for use later) */
		Mat4CpyMat4(pmat, pchan->pose_mat);
		
		/* disable constraint for pose to be solved without it */
		cinf= con->enforce;
		con->enforce= 0.0f;
		
		/* solve pose without constraint */
		where_is_pose(ob);
		
		/* determine effect of constraint by removing the newly calculated 
		 * pchan->pose_mat from the original pchan->pose_mat, thus determining 
		 * the effect of the constraint
		 */
		Mat4Invert(imat, pchan->pose_mat);
		Mat4MulMat4(tmat, imat, pmat);
		Mat4Invert(data->invmat, tmat);
		
		/* recalculate pose with new inv-mat */
		con->enforce= cinf;
		where_is_pose(ob);
	}
	else if (ob) {
		/* use what_does_parent to find inverse - just like for normal parenting.
		 * NOTE: what_does_parent uses a static workob defined in object.c 
		 */
		what_does_parent(ob);
		Mat4Invert(data->invmat, workob.obmat);
	}
	else
		Mat4One(data->invmat);
}

/* ChildOf Constraint - clear inverse callback */
void childof_const_clearinv (void *conv, void *unused)
{
	bConstraint *con= (bConstraint *)conv;
	bChildOfConstraint *data= (bChildOfConstraint *)con->data;
	
	/* simply clear the matrix */
	Mat4One(data->invmat);
}
