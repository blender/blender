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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): Joshua Leung, Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include <stdio.h>
#include <string.h>

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
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
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_utildefines.h"
#include "BIK_api.h"

#ifndef DISABLE_PYTHON
#include "BPY_extern.h"
#endif

#include "WM_api.h"
#include "WM_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"
#include "RNA_types.h"

#include "ED_object.h"
#include "ED_screen.h"

#include "UI_interface.h"

#include "object_intern.h"

/* -------------- Get Active Constraint Data ---------------------- */

/* if object in posemode, active bone constraints, else object constraints */
ListBase *get_active_constraints (Object *ob)
{
	if (ob == NULL)
		return NULL;
	
	if (ob->mode & OB_MODE_POSE) {
		bPoseChannel *pchan;
		
		pchan = get_active_posechannel(ob);
		if (pchan)
			return &pchan->constraints;
	}
	else 
		return &ob->constraints;
	
	return NULL;
}

/* Find the list that a given constraint belongs to, and/or also get the posechannel this is from (if applicable) */
ListBase *get_constraint_lb (Object *ob, bConstraint *con, bPoseChannel **pchan_r)
{
	if (pchan_r)
		*pchan_r= NULL;
	
	if (ELEM(NULL, ob, con))
		return NULL;
	
	/* try object constraints first */
	if ((BLI_findindex(&ob->constraints, con) != -1)) {
		return &ob->constraints;
	}
	
	/* if armature, try pose bones too */
	if (ob->pose) {
		bPoseChannel *pchan;
		
		/* try each bone in order 
		 * NOTE: it's not possible to directly look up the active bone yet, so this will have to do
		 */
		for (pchan= ob->pose->chanbase.first; pchan; pchan= pchan->next) {
			if ((BLI_findindex(&pchan->constraints, con) != -1)) {
				
				if (pchan_r)
					*pchan_r= pchan;
				
				return &pchan->constraints;
			}
		}
	}
	
	/* done */
	return NULL;
}

/* single constraint */
bConstraint *get_active_constraint (Object *ob)
{
	return constraints_get_active(get_active_constraints(ob));
}
/* -------------- Constraint Management (Add New, Remove, Rename) -------------------- */
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

#ifndef DISABLE_PYTHON
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
#endif /* DISABLE_PYTHON */

/* this callback gets called when the 'refresh' button of a pyconstraint gets pressed */
void update_pyconstraint_cb (void *arg1, void *arg2)
{
	Object *owner= (Object *)arg1;
	bConstraint *con= (bConstraint *)arg2;
#ifndef DISABLE_PYTHON
	if (owner && con)
		BPY_pyconstraint_update(owner, con);
#endif
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

/* ------------- Constraint Sanity Testing ------------------- */

/* checks validity of object pointers, and NULLs,
 * if Bone doesnt exist it sets the CONSTRAINT_DISABLE flag.
 */
static void test_constraints (Object *owner, bPoseChannel *pchan)
{
	bConstraint *curcon;
	ListBase *conlist= NULL;
	int type;
	
	if (owner==NULL) return;
	
	/* Check parents */
	if (pchan) {
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
			conlist = &pchan->constraints;
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
			
			if (curcon->type == CONSTRAINT_TYPE_KINEMATIC) {
				bKinematicConstraint *data = curcon->data;
				
				/* bad: we need a separate set of checks here as poletarget is 
				 *		optional... otherwise poletarget must exist too or else
				 *		the constraint is deemed invalid
				 */
				/* default IK check ... */
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
				/* ... can be overwritten here */
				BIK_test_constraint(owner, curcon);
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
			else if (curcon->type == CONSTRAINT_TYPE_SPLINEIK) {
				bSplineIKConstraint *data = curcon->data;
				
				/* if the number of points does not match the amount required by the chain length,
				 * free the points array and request a rebind...
				 */
				if ((data->points == NULL) || (data->numpoints != data->chainlen+1))
				{
					/* free the points array */
					if (data->points) {
						MEM_freeN(data->points);
						data->points = NULL;
					}
					
					/* clear the bound flag, forcing a rebind next time this is evaluated */
					data->flag &= ~CONSTRAINT_SPLINEIK_BOUND;
				}
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
					if (ELEM3(curcon->type, CONSTRAINT_TYPE_FOLLOWPATH, CONSTRAINT_TYPE_CLAMPTO, CONSTRAINT_TYPE_SPLINEIK)) {
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

void object_test_constraints (Object *owner)
{
	if(owner->constraints.first)
		test_constraints(owner, NULL);

	if (owner->type==OB_ARMATURE && owner->pose) {
		bPoseChannel *pchan;

		for (pchan= owner->pose->chanbase.first; pchan; pchan= pchan->next)
			if(pchan->constraints.first)
				test_constraints(owner, pchan);
	}
}

/* ********************** CONSTRAINT-SPECIFIC STUFF ********************* */

/* ---------- Distance-Dependent Constraints ---------- */
/* StretchTo, Limit Distance */

static int stretchto_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "constraint", &RNA_StretchToConstraint);
	return (ptr.id.data && ptr.data);
}

static int stretchto_reset_exec (bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "constraint", &RNA_StretchToConstraint);
	
	/* just set original length to 0.0, which will cause a reset on next recalc */
	RNA_float_set(&ptr, "original_length", 0.0f);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, NULL);
	return OPERATOR_FINISHED;
}

void CONSTRAINT_OT_stretchto_reset (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reset Original Length";
	ot->idname= "CONSTRAINT_OT_stretchto_reset";
	ot->description= "Reset original length of bone for Stretch To Constraint";
	
	ot->exec= stretchto_reset_exec;
	ot->poll= stretchto_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}


static int limitdistance_reset_exec (bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "constraint", &RNA_LimitDistanceConstraint);
	
	/* just set distance to 0.0, which will cause a reset on next recalc */
	RNA_float_set(&ptr, "distance", 0.0f);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, NULL);
	return OPERATOR_FINISHED;
}

static int limitdistance_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "constraint", &RNA_LimitDistanceConstraint);
	return (ptr.id.data && ptr.data);
}

void CONSTRAINT_OT_limitdistance_reset (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Reset Distance";
	ot->idname= "CONSTRAINT_OT_limitdistance_reset";
	ot->description= "Reset limiting distance for Limit Distance Constraint";
	
	ot->exec= limitdistance_reset_exec;
	ot->poll= limitdistance_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ------------- Child-Of Constraint ------------------ */

static int childof_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "constraint", &RNA_ChildOfConstraint);
	return (ptr.id.data && ptr.data);
}

/* ChildOf Constraint - set inverse callback */
static int childof_set_inverse_exec (bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "constraint", &RNA_ChildOfConstraint);
	Scene *scene= CTX_data_scene(C);
	Object *ob= ptr.id.data;
	bConstraint *con= ptr.data;
	bChildOfConstraint *data= (bChildOfConstraint *)con->data;
	bPoseChannel *pchan= NULL;

	/* try to find a pose channel */
	// TODO: get from context instead?
	if (ob && ob->pose)
		pchan= get_active_posechannel(ob);
	
	/* calculate/set inverse matrix */
	if (pchan) {
		float pmat[4][4], cinf;
		float imat[4][4], tmat[4][4];
		
		/* make copy of pchan's original pose-mat (for use later) */
		copy_m4_m4(pmat, pchan->pose_mat);
		
		/* disable constraint for pose to be solved without it */
		cinf= con->enforce;
		con->enforce= 0.0f;
		
		/* solve pose without constraint */
		where_is_pose(scene, ob);
		
		/* determine effect of constraint by removing the newly calculated 
		 * pchan->pose_mat from the original pchan->pose_mat, thus determining 
		 * the effect of the constraint
		 */
		invert_m4_m4(imat, pchan->pose_mat);
		mul_m4_m4m4(tmat, imat, pmat);
		invert_m4_m4(data->invmat, tmat);
		
		/* recalculate pose with new inv-mat */
		con->enforce= cinf;
		where_is_pose(scene, ob);
	}
	else if (ob) {
		Object workob;
		/* use what_does_parent to find inverse - just like for normal parenting.
		 * NOTE: what_does_parent uses a static workob defined in object.c 
		 */
		what_does_parent(scene, ob, &workob);
		invert_m4_m4(data->invmat, workob.obmat);
	}
	else
		unit_m4(data->invmat);
		
	WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, ob);
		
	return OPERATOR_FINISHED;
}

void CONSTRAINT_OT_childof_set_inverse (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Set Inverse";
	ot->idname= "CONSTRAINT_OT_childof_set_inverse";
	ot->description= "Set inverse correction for ChildOf constraint";
	
	ot->exec= childof_set_inverse_exec;
	ot->poll= childof_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/* ChildOf Constraint - clear inverse callback */
static int childof_clear_inverse_exec (bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "constraint", &RNA_ChildOfConstraint);
	Object *ob= ptr.id.data;
	bConstraint *con= ptr.data;
	bChildOfConstraint *data= (bChildOfConstraint *)con->data;
	
	/* simply clear the matrix */
	unit_m4(data->invmat);
	
	WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, ob);
	
	return OPERATOR_FINISHED;
}

void CONSTRAINT_OT_childof_clear_inverse (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Clear Inverse";
	ot->idname= "CONSTRAINT_OT_childof_clear_inverse";
	ot->description= "Clear inverse correction for ChildOf constraint";
	
	ot->exec= childof_clear_inverse_exec;
	ot->poll= childof_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

/***************************** BUTTONS ****************************/

void ED_object_constraint_set_active(Object *ob, bConstraint *con)
{	
	ListBase *lb = get_constraint_lb(ob, con, NULL);
	
	/* lets be nice and escape if its active already */
	// NOTE: this assumes that the stack doesn't have other active ones set...
	if ((lb && con) && (con->flag & CONSTRAINT_ACTIVE))
		return;
	
	constraints_set_active(lb, con);
}

void ED_object_constraint_update(Object *ob)
{

	if(ob->pose) update_pose_constraint_flags(ob->pose);

	object_test_constraints(ob);

	if(ob->type==OB_ARMATURE) DAG_id_flush_update(&ob->id, OB_RECALC_DATA|OB_RECALC_OB);
	else DAG_id_flush_update(&ob->id, OB_RECALC_OB);
}

void ED_object_constraint_dependency_update(Scene *scene, Object *ob)
{
	ED_object_constraint_update(ob);

	if(ob->pose) ob->pose->flag |= POSE_RECALC;	// checks & sorts pose channels
    DAG_scene_sort(scene);
}

static int constraint_poll(bContext *C)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "constraint", &RNA_Constraint);
	return (ptr.id.data && ptr.data);
}

static int constraint_delete_exec (bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "constraint", &RNA_Constraint);
	Object *ob= ptr.id.data;
	bConstraint *con= ptr.data;
	ListBase *lb = get_constraint_lb(ob, con, NULL);
	
	/* free the constraint */
	if (remove_constraint(lb, con)) {
		/* there's no active constraint now, so make sure this is the case */
		constraints_set_active(lb, NULL);
		
		/* notifiers */
		WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, ob);
		
		return OPERATOR_FINISHED;
	}
	else {
		/* couldn't remove due to some invalid data */
		return OPERATOR_CANCELLED;
	}
}

void CONSTRAINT_OT_delete (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Delete Constraint";
	ot->idname= "CONSTRAINT_OT_delete";
	ot->description= "Remove constraitn from constraint stack";
	
	/* callbacks */
	ot->exec= constraint_delete_exec;
	ot->poll= constraint_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO; 
}

static int constraint_move_down_exec (bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "constraint", &RNA_Constraint);
	Object *ob= ptr.id.data;
	bConstraint *con= ptr.data;
	
	if (con->next) {
		ListBase *conlist= get_constraint_lb(ob, con, NULL);
		bConstraint *nextCon= con->next;
		
		/* insert the nominated constraint after the one that used to be after it */
		BLI_remlink(conlist, con);
		BLI_insertlinkafter(conlist, nextCon, con);
		
		WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, ob);
		
		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_CANCELLED;
}

void CONSTRAINT_OT_move_down (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Constraint Down";
	ot->idname= "CONSTRAINT_OT_move_down";
	ot->description= "Move constraint down constraint stack";
	
	/* callbacks */
	ot->exec= constraint_move_down_exec;
	ot->poll= constraint_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO; 
}


static int constraint_move_up_exec (bContext *C, wmOperator *op)
{
	PointerRNA ptr= CTX_data_pointer_get_type(C, "constraint", &RNA_Constraint);
	Object *ob= ptr.id.data;
	bConstraint *con= ptr.data;
	
	if (con->prev) {
		ListBase *conlist= get_constraint_lb(ob, con, NULL);
		bConstraint *prevCon= con->prev;
		
		/* insert the nominated constraint before the one that used to be before it */
		BLI_remlink(conlist, con);
		BLI_insertlinkbefore(conlist, prevCon, con);
		
		WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, ob);
		
		return OPERATOR_FINISHED;
	}
	
	return OPERATOR_CANCELLED;
}

void CONSTRAINT_OT_move_up (wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Move Constraint Up";
	ot->idname= "CONSTRAINT_OT_move_up";
	ot->description= "Move constraint up constraint stack";
	
	/* callbacks */
	ot->exec= constraint_move_up_exec;
	ot->poll= constraint_poll;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO; 
}

/***************************** OPERATORS ****************************/

/************************ remove constraint operators *********************/

static int pose_constraints_clear_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	Scene *scene= CTX_data_scene(C);
	
	/* free constraints for all selected bones */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones)
	{
		free_constraints(&pchan->constraints);
		pchan->constflag &= ~(PCHAN_HAS_IK|PCHAN_HAS_SPLINEIK|PCHAN_HAS_CONST);
	}
	CTX_DATA_END;
	
	/* force depsgraph to get recalculated since relationships removed */
	DAG_scene_sort(scene);		/* sort order of objects */	
	
	/* do updates */
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_constraints_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Constraints";
	ot->idname= "POSE_OT_constraints_clear";
	ot->description= "Clear all the constraints for the selected bones";
	
	/* callbacks */
	ot->exec= pose_constraints_clear_exec;
	ot->poll= ED_operator_posemode; // XXX - do we want to ensure there are selected bones too?
}


static int object_constraints_clear_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	Scene *scene= CTX_data_scene(C);
	
	/* do freeing */
	// TODO: we should free constraints for all selected objects instead (to be more consistent with bones)
	free_constraints(&ob->constraints);
	
	/* force depsgraph to get recalculated since relationships removed */
	DAG_scene_sort(scene);		/* sort order of objects */	
	
	/* do updates */
	DAG_id_flush_update(&ob->id, OB_RECALC_OB);
	WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT, ob);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_constraints_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Constraints";
	ot->idname= "OBJECT_OT_constraints_clear";
	ot->description= "Clear all the constraints for the active Object only";
	
	/* callbacks */
	ot->exec= object_constraints_clear_exec;
	ot->poll= ED_operator_object_active_editable;
}

/************************ add constraint operators *********************/

/* get the Object and/or PoseChannel to use as target */
static short get_new_constraint_target(bContext *C, int con_type, Object **tar_ob, bPoseChannel **tar_pchan, short add)
{
	Object *obact= CTX_data_active_object(C);
	bPoseChannel *pchanact= get_active_posechannel(obact);
	short only_curve= 0, only_mesh= 0, only_ob= 0;
	short found= 0;
	
	/* clear tar_ob and tar_pchan fields before use 
	 *	- assume for now that both always exist...
	 */
	*tar_ob= NULL;
	*tar_pchan= NULL;
	
	/* check if constraint type doesn't requires a target
	 *	- if so, no need to get any targets 
	 */
	switch (con_type) {
		/* no-target constraints --------------------------- */
			/* null constraint - shouldn't even be added! */
		case CONSTRAINT_TYPE_NULL:
			/* limit constraints - no targets needed */
		case CONSTRAINT_TYPE_LOCLIMIT:
		case CONSTRAINT_TYPE_ROTLIMIT:
		case CONSTRAINT_TYPE_SIZELIMIT:
		case CONSTRAINT_TYPE_SAMEVOL:
			return 0;
			
		/* restricted target-type constraints -------------- */
		/* NOTE: for these, we cannot try to add a target object if no valid ones are found, since that doesn't work */
			/* curve-based constraints - set the only_curve and only_ob flags */
		case CONSTRAINT_TYPE_CLAMPTO:
		case CONSTRAINT_TYPE_FOLLOWPATH:
		case CONSTRAINT_TYPE_SPLINEIK:
			only_curve= 1;
			only_ob= 1;
			add= 0;
			break;
			
			/* mesh only? */
		case CONSTRAINT_TYPE_SHRINKWRAP:
			only_mesh= 1;
			only_ob= 1;
			add= 0;
			break;
			
			/* object only - add here is ok? */
		case CONSTRAINT_TYPE_RIGIDBODYJOINT:
			only_ob= 1;
			break;
	}
	
	/* if the active Object is Armature, and we can search for bones, do so... */
	if ((obact->type == OB_ARMATURE) && (only_ob == 0)) {
		/* search in list of selected Pose-Channels for target */
		CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones) 
		{
			/* just use the first one that we encounter, as long as it is not the active one */
			if (pchan != pchanact) {
				*tar_ob= obact;
				*tar_pchan= pchan;
				found= 1;
				
				break;
			}
		}
		CTX_DATA_END;
	}
	
	/* if not yet found, try selected Objects... */
	if (found == 0) {
		/* search in selected objects context */
		CTX_DATA_BEGIN(C, Object*, ob, selected_objects) 
		{
			/* just use the first object we encounter (that isn't the active object) 
			 * and which fulfills the criteria for the object-target that we've got 
			 */
			if ( (ob != obact) &&
				 ((!only_curve) || (ob->type == OB_CURVE)) && 
				 ((!only_mesh) || (ob->type == OB_MESH)) )
			{
				/* set target */
				*tar_ob= ob;
				found= 1;
				
				/* perform some special operations on the target */
				if (only_curve) {
					/* Curve-Path option must be enabled for follow-path constraints to be able to work */
					Curve *cu= (Curve *)ob->data;
					cu->flag |= CU_PATH;
				}
				
				break;
			}
		}
		CTX_DATA_END;
	}
	
	/* if still not found, add a new empty to act as a target (if allowed) */
	if ((found == 0) && (add)) {
		Scene *scene= CTX_data_scene(C);
		Base *base= BASACT, *newbase=NULL;
		Object *obt;
		
		/* add new target object */
		obt= add_object(scene, OB_EMPTY);
		
		/* set layers OK */
		newbase= BASACT;
		newbase->lay= base->lay;
		obt->lay= newbase->lay;
		
		/* transform cent to global coords for loc */
		if (pchanact) {
			/* since by default, IK targets the tip of the last bone, use the tip of the active PoseChannel 
			 * if adding a target for an IK Constraint
			 */
			if (con_type == CONSTRAINT_TYPE_KINEMATIC)
				mul_v3_m4v3(obt->loc, obact->obmat, pchanact->pose_tail);
			else
				mul_v3_m4v3(obt->loc, obact->obmat, pchanact->pose_head);
		}
		else
			VECCOPY(obt->loc, obact->obmat[3]);
		
		/* restore, add_object sets active */
		BASACT= base;
		base->flag |= SELECT;
		
		/* make our new target the new object */
		*tar_ob= obt;
		found= 1;
	}
	
	/* return whether there's any target */
	return found;
}

/* used by add constraint operators to add the constraint required */
static int constraint_add_exec(bContext *C, wmOperator *op, Object *ob, ListBase *list, int type, short setTarget)
{
	Scene *scene= CTX_data_scene(C);
	bPoseChannel *pchan;
	bConstraint *con;
	
	if(list == &ob->constraints)
		pchan= NULL;
	else
		pchan= get_active_posechannel(ob);

	/* check if constraint to be added is valid for the given constraints stack */
	if (type == CONSTRAINT_TYPE_NULL) {
		return OPERATOR_CANCELLED;
	}
	if ( (type == CONSTRAINT_TYPE_RIGIDBODYJOINT) && (list != &ob->constraints) ) {
		BKE_report(op->reports, RPT_ERROR, "Rigid Body Joint Constraint can only be added to Objects.");
		return OPERATOR_CANCELLED;
	}
	if ( (type == CONSTRAINT_TYPE_KINEMATIC) && ((!pchan) || (list != &pchan->constraints)) ) {
		BKE_report(op->reports, RPT_ERROR, "IK Constraint can only be added to Bones.");
		return OPERATOR_CANCELLED;
	}
	if ( (type == CONSTRAINT_TYPE_SPLINEIK) && ((!pchan) || (list != &pchan->constraints)) ) {
		BKE_report(op->reports, RPT_ERROR, "Spline IK Constraint can only be added to Bones.");
		return OPERATOR_CANCELLED;
	}
	
	/* create a new constraint of the type requried, and add it to the active/given constraints list */
	if(pchan)
		con = add_pose_constraint(ob, pchan, NULL, type);
	else
		con = add_ob_constraint(ob, NULL, type);
	
	/* get the first selected object/bone, and make that the target
	 *	- apart from the buttons-window add buttons, we shouldn't add in this way
	 */
	if (setTarget) {
		Object *tar_ob= NULL;
		bPoseChannel *tar_pchan= NULL;
		
		/* get the target objects, adding them as need be */
		if (get_new_constraint_target(C, type, &tar_ob, &tar_pchan, 1)) {
			/* method of setting target depends on the type of target we've got 
			 *	- by default, just set the first target (distinction here is only for multiple-targetted constraints)
			 */
			if (tar_pchan)
				set_constraint_nth_target(con, tar_ob, tar_pchan->name, 0);
			else
				set_constraint_nth_target(con, tar_ob, "", 0);
		}
	}
	
	/* do type-specific tweaking to the constraint settings  */
	// TODO: does action constraint need anything here - i.e. spaceonce?
	switch (type) {
		case CONSTRAINT_TYPE_CHILDOF:
		{
			/* if this constraint is being added to a posechannel, make sure
			 * the constraint gets evaluated in pose-space */
			if (ob->mode & OB_MODE_POSE) {
				con->ownspace = CONSTRAINT_SPACE_POSE;
				con->flag |= CONSTRAINT_SPACEONCE;
			}
		}
			break;
			
		case CONSTRAINT_TYPE_PYTHON: // FIXME: this code is not really valid anymore
		{
			char *menustr;
			int scriptint= 0;
#ifndef DISABLE_PYTHON
			/* popup a list of usable scripts */
			menustr = buildmenu_pyconstraints(NULL, &scriptint);
			// XXX scriptint = pupmenu(menustr);
			MEM_freeN(menustr);
			
			/* only add constraint if a script was chosen */
			if (scriptint) {
				/* add constraint */
				validate_pyconstraint_cb(con->data, &scriptint);
				
				/* make sure target allowance is set correctly */
				BPY_pyconstraint_update(ob, con);
			}
#endif
		}
		default:
			break;
	}
	
	/* make sure all settings are valid - similar to above checks, but sometimes can be wrong */
	object_test_constraints(ob);
	
	if (ob->pose)
		update_pose_constraint_flags(ob->pose);
	
	
	/* force depsgraph to get recalculated since new relationships added */
	DAG_scene_sort(scene);		/* sort order of objects */
	
	if ((ob->type==OB_ARMATURE) && (pchan)) {
		ob->pose->flag |= POSE_RECALC;	/* sort pose channels */
		DAG_id_flush_update(&ob->id, OB_RECALC_DATA|OB_RECALC_OB);
	}
	else
		DAG_id_flush_update(&ob->id, OB_RECALC_DATA);
	
	/* notifiers for updates */
	WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT|NA_ADDED, ob);
	
	return OPERATOR_FINISHED;
}

/* ------------------ */

/* dummy operator callback */
static int object_constraint_add_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	int type= RNA_enum_get(op->ptr, "type");
	short with_targets= 0;
	
	/* get active object from context */
	if (sa->spacetype == SPACE_BUTS)
		ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	else
		ob= CTX_data_active_object(C);
	
	if (!ob) {
		BKE_report(op->reports, RPT_ERROR, "No active object to add constraint to.");
		return OPERATOR_CANCELLED;
	}
		
	/* hack: set constraint targets from selected objects in context is allowed when
	 *		operator name included 'with_targets', since the menu doesn't allow multiple properties
	 */
	if (strstr(op->idname, "with_targets"))
		with_targets= 1;

	return constraint_add_exec(C, op, ob, &ob->constraints, type, with_targets);
}

/* dummy operator callback */
static int pose_constraint_add_exec(bContext *C, wmOperator *op)
{
	ScrArea *sa= CTX_wm_area(C);
	Object *ob;
	int type= RNA_enum_get(op->ptr, "type");
	short with_targets= 0;
	
	/* get active object from context */
	if (sa->spacetype == SPACE_BUTS)
		ob= CTX_data_pointer_get_type(C, "object", &RNA_Object).data;
	else
		ob= CTX_data_active_object(C);
	
	if (!ob) {
		BKE_report(op->reports, RPT_ERROR, "No active object to add constraint to.");
		return OPERATOR_CANCELLED;
	}
		
	/* hack: set constraint targets from selected objects in context is allowed when
	 *		operator name included 'with_targets', since the menu doesn't allow multiple properties
	 */
	if (strstr(op->idname, "with_targets"))
		with_targets= 1;
	
	return constraint_add_exec(C, op, ob, get_active_constraints(ob), type, with_targets);
}

/* ------------------ */

void OBJECT_OT_constraint_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Constraint";
	ot->description = "Add a constraint to the active object";
	ot->idname= "OBJECT_OT_constraint_add";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_constraint_add_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", constraint_type_items, 0, "Type", "");
}

void OBJECT_OT_constraint_add_with_targets(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Constraint (with Targets)";
	ot->description = "Add a constraint to the active object, with target (where applicable) set to the selected Objects/Bones";
	ot->idname= "OBJECT_OT_constraint_add_with_targets";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= object_constraint_add_exec;
	ot->poll= ED_operator_object_active_editable;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", constraint_type_items, 0, "Type", "");
}

void POSE_OT_constraint_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Constraint";
	ot->description = "Add a constraint to the active bone";
	ot->idname= "POSE_OT_constraint_add";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= pose_constraint_add_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", constraint_type_items, 0, "Type", "");
}

void POSE_OT_constraint_add_with_targets(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add Constraint (with Targets)";
	ot->description = "Add a constraint to the active bone, with target (where applicable) set to the selected Objects/Bones";
	ot->idname= "POSE_OT_constraint_add_with_targets";
	
	/* api callbacks */
	ot->invoke= WM_menu_invoke;
	ot->exec= pose_constraint_add_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	ot->prop= RNA_def_enum(ot->srna, "type", constraint_type_items, 0, "Type", "");
}

/************************ IK Constraint operators *********************/
/* NOTE: only for Pose-Channels */
// TODO: should these be here, or back in editors/armature/poseobject.c again?

/* present menu with options + validation for targets to use */
static int pose_ik_add_invoke(bContext *C, wmOperator *op, wmEvent *evt)
{
	Object *ob= CTX_data_active_object(C);
	bPoseChannel *pchan= get_active_posechannel(ob);
	bConstraint *con= NULL;
	
	uiPopupMenu *pup;
	uiLayout *layout;
	Object *tar_ob= NULL;
	bPoseChannel *tar_pchan= NULL;
	
	/* must have active bone */
	if (ELEM(NULL, ob, pchan)) {
		BKE_report(op->reports, RPT_ERROR, "Must have active bone to add IK Constraint to.");
		return OPERATOR_CANCELLED;
	}
	
	/* bone must not have any constraints already */
	for (con= pchan->constraints.first; con; con= con->next) {
		if (con->type==CONSTRAINT_TYPE_KINEMATIC) break;
	}
	if (con) {
		BKE_report(op->reports, RPT_ERROR, "Bone already has IK Constraint.");
		return OPERATOR_CANCELLED;
	}
	
	/* prepare popup menu to choose targetting options */
	pup= uiPupMenuBegin(C, "Add IK", 0);
	layout= uiPupMenuLayout(pup);
	
	/* the type of targets we'll set determines the menu entries to show... */
	if (get_new_constraint_target(C, CONSTRAINT_TYPE_KINEMATIC, &tar_ob, &tar_pchan, 0)) {
		/* bone target, or object target? 
		 *	- the only thing that matters is that we want a target...
		 */
		if (tar_pchan)
			uiItemBooleanO(layout, "To Active Bone", 0, "POSE_OT_ik_add", "with_targets", 1);
		else
			uiItemBooleanO(layout, "To Active Object", 0, "POSE_OT_ik_add", "with_targets", 1);
	}
	else {
		/* we have a choice of adding to a new empty, or not setting any target (targetless IK) */
		uiItemBooleanO(layout, "To New Empty Object", 0, "POSE_OT_ik_add", "with_targets", 1);
		uiItemBooleanO(layout, "Without Targets", 0, "POSE_OT_ik_add", "with_targets", 0);
	}
	
	/* finish building the menu, and process it (should result in calling self again) */
	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}

/* call constraint_add_exec() to add the IK constraint */
static int pose_ik_add_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	int with_targets= RNA_boolean_get(op->ptr, "with_targets");
	
	/* add the constraint - all necessary checks should have been done by the invoke() callback already... */
	return constraint_add_exec(C, op, ob, get_active_constraints(ob), CONSTRAINT_TYPE_KINEMATIC, with_targets);
}

void POSE_OT_ik_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Add IK to Bone";
	ot->description= "Add IK Constraint to the active Bone";
	ot->idname= "POSE_OT_ik_add";
	
	/* api callbacks */
	ot->invoke= pose_ik_add_invoke;
	ot->exec= pose_ik_add_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "with_targets", 1, "With Targets", "Assign IK Constraint with targets derived from the select bones/objects");
}

/* ------------------ */

/* remove IK constraints from selected bones */
static int pose_ik_clear_exec(bContext *C, wmOperator *op)
{
	Object *ob= CTX_data_active_object(C);
	
	/* only remove IK Constraints */
	CTX_DATA_BEGIN(C, bPoseChannel*, pchan, selected_pose_bones) 
	{
		bConstraint *con, *next;
		
		// TODO: should we be checking if these contraints were local before we try and remove them?
		for (con= pchan->constraints.first; con; con= next) {
			next= con->next;
			if (con->type==CONSTRAINT_TYPE_KINEMATIC) {
				remove_constraint(&pchan->constraints, con);
			}
		}
		pchan->constflag &= ~(PCHAN_HAS_IK|PCHAN_HAS_TARGET);
	}
	CTX_DATA_END;
	
	/* refresh depsgraph */
	DAG_id_flush_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT|ND_CONSTRAINT|NA_REMOVED, ob);
	
	return OPERATOR_FINISHED;
}

void POSE_OT_ik_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name= "Remove IK";
	ot->description= "Remove all IK Constraints from selected bones";
	ot->idname= "POSE_OT_ik_clear";
	
	/* api callbacks */
	ot->exec= pose_ik_clear_exec;
	ot->poll= ED_operator_posemode;
	
	/* flags */
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO;
}

