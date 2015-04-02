/*
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
 * The Original Code is Copyright (C) 2015 Blender Foundation
 * This is a new part of Blender
 *
 * Contributor(s): Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/space_action/action_edit.c
 *  \ingroup spaction
 */


#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>


#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "DNA_anim_types.h"
#include "DNA_gpencil_types.h"
#include "DNA_key_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_mask_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_global.h"
#include "BKE_library.h"
#include "BKE_key.h"
#include "BKE_main.h"
#include "BKE_nla.h"
#include "BKE_context.h"
#include "BKE_report.h"

#include "UI_view2d.h"

#include "ED_anim_api.h"
#include "ED_gpencil.h"
#include "ED_keyframing.h"
#include "ED_keyframes_edit.h"
#include "ED_screen.h"
#include "ED_markers.h"
#include "ED_mask.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "action_intern.h"

/* ************************************************************************** */
/* ACTION MANAGEMENT */

/* Helper function to find the active AnimData block from the Action Editor context */
static AnimData *actedit_animdata_from_context(bContext *C)
{
	SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
	Object *ob = CTX_data_active_object(C);
	AnimData *adt = NULL;
	
	/* Get AnimData block to use */
	if (saction->mode == SACTCONT_ACTION) {
		/* Currently, "Action Editor" means object-level only... */
		if (ob) {
			adt = ob->adt;
		}
	}
	else if (saction->mode == SACTCONT_SHAPEKEY) {
		Key *key = BKE_key_from_object(ob);
		if (key) {
			adt = key->adt;
		}
	}
	
	return adt;
}

/* -------------------------------------------------------------------- */

/* Create new action */
static bAction *action_create_new(bContext *C, bAction *oldact)
{
	ScrArea *sa = CTX_wm_area(C);
	bAction *action;
	
	/* create action - the way to do this depends on whether we've got an
	 * existing one there already, in which case we make a copy of it
	 * (which is useful for "versioning" actions within the same file)
	 */
	if (oldact && GS(oldact->id.name) == ID_AC) {
		/* make a copy of the existing action */
		action = BKE_action_copy(oldact);
	}
	else {
		/* just make a new (empty) action */
		action = add_empty_action(CTX_data_main(C), "Action");
	}
	
	/* when creating new ID blocks, there is already 1 user (as for all new datablocks), 
	 * but the RNA pointer code will assign all the proper users instead, so we compensate
	 * for that here
	 */
	BLI_assert(action->id.us == 1);
	action->id.us--;
	
	/* set ID-Root type */
	if (sa->spacetype == SPACE_ACTION) {
		SpaceAction *saction = (SpaceAction *)sa->spacedata.first;
		
		if (saction->mode == SACTCONT_SHAPEKEY)
			action->idroot = ID_KE;
		else
			action->idroot = ID_OB;
	}
	
	return action;
}

/* Change the active action used by the action editor */
static void actedit_change_action(bContext *C, bAction *act)
{
	bScreen *screen = CTX_wm_screen(C);
	SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
	
	PointerRNA ptr, idptr;
	PropertyRNA *prop;
	
	/* create RNA pointers and get the property */
	RNA_pointer_create(&screen->id, &RNA_SpaceDopeSheetEditor, saction, &ptr);
	prop = RNA_struct_find_property(&ptr, "action");
	
	/* NOTE: act may be NULL here, so better to just use a cast here */
	RNA_id_pointer_create((ID *)act, &idptr);
	
	/* set the new pointer, and force a refresh */
	RNA_property_pointer_set(&ptr, prop, idptr);
	RNA_property_update(C, &ptr, prop);
}

/* ******************** New Action Operator *********************** */

/* Criteria:
 *  1) There must be an dopesheet/action editor, and it must be in a mode which uses actions...
 *        OR
 *     The NLA Editor is active (i.e. Animation Data panel -> new action)
 *  2) The associated AnimData block must not be in tweakmode
 */
static int action_new_poll(bContext *C)
{
	Scene *scene = CTX_data_scene(C);
	
	/* Check tweakmode is off (as you don't want to be tampering with the action in that case) */
	/* NOTE: unlike for pushdown, this operator needs to be run when creating an action from nothing... */
	if (!(scene->flag & SCE_NLA_EDIT_ON)) {
		if (ED_operator_action_active(C)) {
			SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
			Object *ob = CTX_data_active_object(C);
			
			/* For now, actions are only for the active object, and on object and shapekey levels... */
			if (saction->mode == SACTCONT_ACTION) {
				/* XXX: This assumes that actions are assigned to the active object */
				if (ob)
					return true;
			}
			else if (saction->mode == SACTCONT_SHAPEKEY) {
				Key *key = BKE_key_from_object(ob);
				if (key)
					return true;
			}
		}
		else if (ED_operator_nla_active(C)) {
			return true;
		}
	}
	
	/* something failed... */
	return false;
}

static int action_new_exec(bContext *C, wmOperator *UNUSED(op))
{
	PointerRNA ptr, idptr;
	PropertyRNA *prop;
	
	/* hook into UI */
	UI_context_active_but_prop_get_templateID(C, &ptr, &prop);
	
	if (prop) {
		bAction *action = NULL, *oldact = NULL;
		AnimData *adt = NULL;
		PointerRNA oldptr;
		
		oldptr = RNA_property_pointer_get(&ptr, prop);
		oldact = (bAction *)oldptr.id.data;
		
		/* stash the old action to prevent it from being lost */
		if (ptr.type == &RNA_AnimData) {
			adt = ptr.data;
		}
		else if (ptr.type == &RNA_SpaceDopeSheetEditor) {
			adt = actedit_animdata_from_context(C);
		}
		
		/* Perform stashing operation - But only if there is an action */
		if (adt && oldact) {
			/* stash the action */
			if (BKE_nla_action_stash(adt)) {
				/* The stash operation will remove the user already
				 * (and unlink the action from the AnimData action slot).
				 * Hence, we must unset the ref to the action in the
				 * action editor too (if this is where we're being called from)
				 * first before setting the new action once it is created,
				 * or else the user gets decremented twice!
				 */
				if (ptr.type == &RNA_SpaceDopeSheetEditor) {
					SpaceAction *saction = (SpaceAction *)ptr.data;
					saction->action = NULL;
				}
			}
			else {
				//printf("WARNING: Failed to stash %s. It may already exist in the NLA stack though\n", oldact->id.name);
			}
		}
		
		/* create action */
		action = action_create_new(C, oldact);
		
		/* set this new action
		 * NOTE: we can't use actedit_change_action, as this function is also called from the NLA
		 */
		RNA_id_pointer_create(&action->id, &idptr);
		RNA_property_pointer_set(&ptr, prop, idptr);
		RNA_property_update(C, &ptr, prop);
	}
	
	/* set notifier that keyframes have changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_KEYFRAME | NA_ADDED, NULL);
	
	return OPERATOR_FINISHED;
}
 
void ACTION_OT_new(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "New Action";
	ot->idname = "ACTION_OT_new";
	ot->description = "Create new action";
	
	/* api callbacks */
	ot->exec = action_new_exec;
	ot->poll = action_new_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************* Action Push-Down Operator ******************** */

/* Criteria:
 *  1) There must be an dopesheet/action editor, and it must be in a mode which uses actions 
 *  2) There must be an action active
 *  3) The associated AnimData block must not be in tweakmode
 */
static int action_pushdown_poll(bContext *C)
{
	if (ED_operator_action_active(C)) {
		SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
		Scene *scene = CTX_data_scene(C);
		Object *ob = CTX_data_active_object(C);
		
		/* Check for actions and that tweakmode is off */
		if ((saction->action) && !(scene->flag & SCE_NLA_EDIT_ON)) {
			/* For now, actions are only for the active object, and on object and shapekey levels... */
			if (saction->mode == SACTCONT_ACTION) {
				return (ob->adt != NULL);
			}
			else if (saction->mode == SACTCONT_SHAPEKEY) {
				Key *key = BKE_key_from_object(ob);
				
				return (key && key->adt);
			}
		}	
	}
	
	/* something failed... */
	return false;
}

static int action_pushdown_exec(bContext *C, wmOperator *op)
{
	SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
	AnimData *adt = actedit_animdata_from_context(C);
	
	/* Do the deed... */
	if (adt) {
		/* Perform the pushdown operation
		 * - This will deal with all the AnimData-side usercounts
		 */
		if (action_has_motion(adt->action) == 0) {
			/* action may not be suitable... */
			BKE_report(op->reports, RPT_WARNING, "Action must have at least one keyframe or F-Modifier");
			return OPERATOR_CANCELLED;
		}
		else {
			/* action can be safely added */
			BKE_nla_action_pushdown(adt);
		}
		
		/* Stop displaying this action in this editor
		 * NOTE: The editor itself doesn't set a user...
		 */
		saction->action = NULL;
	}
	
	/* Send notifiers that stuff has changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
	return OPERATOR_FINISHED;
}

void ACTION_OT_push_down(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Push Down Action";
	ot->idname = "ACTION_OT_push_down";
	ot->description = "Push action down on to the NLA stack as a new strip";
	
	/* callbacks */
	ot->exec = action_pushdown_exec;
	ot->poll = action_pushdown_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************* Action Stash Operator ******************** */

static int action_stash_exec(bContext *C, wmOperator *op)
{
	SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
	AnimData *adt = actedit_animdata_from_context(C);
	
	/* Perform stashing operation */
	if (adt) {
		/* don't do anything if this action is empty... */
		if (action_has_motion(adt->action) == 0) {
			/* action may not be suitable... */
			BKE_report(op->reports, RPT_WARNING, "Action must have at least one keyframe or F-Modifier");
			return OPERATOR_CANCELLED;
		}
		else {
			/* stash the action */
			if (BKE_nla_action_stash(adt)) {
				/* The stash operation will remove the user already,
				 * so the flushing step later shouldn't double up
				 * the usercount fixes. Hence, we must unset this ref
				 * first before setting the new action.
				 */
				saction->action = NULL;
			}
			else {
				/* action has already been added - simply warn about this, and clear */
				BKE_report(op->reports, RPT_ERROR, "Action has already been stashed");
			}
			
			/* clear action refs from editor, and then also the backing data (not necessary) */
			actedit_change_action(C, NULL);
		}
	}
	
	/* Send notifiers that stuff has changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
	return OPERATOR_FINISHED;
}

void ACTION_OT_stash(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Stash Action";
	ot->idname = "ACTION_OT_stash";
	ot->description = "Store this action in the NLA stack as a non-contributing strip for later use";
	
	/* callbacks */
	ot->exec = action_stash_exec;
	ot->poll = action_pushdown_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_boolean(ot->srna, "create_new", true, "Create New Action", 
	                           "Create a new action once the existing one has been safely stored");
}

/* ----------------- */

/* Criteria:
 *  1) There must be an dopesheet/action editor, and it must be in a mode which uses actions 
 *  2) The associated AnimData block must not be in tweakmode
 */
static int action_stash_create_poll(bContext *C)
{
	if (ED_operator_action_active(C)) {
		SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
		Scene *scene = CTX_data_scene(C);
		
		/* Check tweakmode is off (as you don't want to be tampering with the action in that case) */
		/* NOTE: unlike for pushdown, this operator needs to be run when creating an action from nothing... */
		if (!(scene->flag & SCE_NLA_EDIT_ON)) {
			/* For now, actions are only for the active object, and on object and shapekey levels... */
			return ELEM(saction->mode, SACTCONT_ACTION, SACTCONT_SHAPEKEY);
		}
	}
	
	/* something failed... */
	return false;
}

static int action_stash_create_exec(bContext *C, wmOperator *op)
{
	SpaceAction *saction = (SpaceAction *)CTX_wm_space_data(C);
	AnimData *adt = actedit_animdata_from_context(C);
	
	/* Check for no action... */
	if (saction->action == NULL) {
		/* just create a new action */
		bAction *action = action_create_new(C, NULL);
		actedit_change_action(C, action);
	}
	else if (adt) {
		/* Perform stashing operation */
		if (action_has_motion(adt->action) == 0) {
			/* don't do anything if this action is empty... */
			BKE_report(op->reports, RPT_WARNING, "Action must have at least one keyframe or F-Modifier");
			return OPERATOR_CANCELLED;
		}
		else {
			/* stash the action */
			if (BKE_nla_action_stash(adt)) {
				bAction *new_action = NULL;
				
				/* create new action not based on the old one (since the "new" operator already does that) */
				new_action = action_create_new(C, NULL);
				
				/* The stash operation will remove the user already,
				 * so the flushing step later shouldn't double up
				 * the usercount fixes. Hence, we must unset this ref
				 * first before setting the new action.
				 */
				saction->action = NULL;
				actedit_change_action(C, new_action);
			}
			else {
				/* action has already been added - simply warn about this, and clear */
				BKE_report(op->reports, RPT_ERROR, "Action has already been stashed");
				actedit_change_action(C, NULL);
			}
		}
	}
	
	/* Send notifiers that stuff has changed */
	WM_event_add_notifier(C, NC_ANIMATION | ND_NLA_ACTCHANGE, NULL);
	return OPERATOR_FINISHED;
}

void ACTION_OT_stash_and_create(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Stash Action";
	ot->idname = "ACTION_OT_stash_and_create";
	ot->description = "Store this action in the NLA stack as a non-contributing strip for later use, and create a new action";
	
	/* callbacks */
	ot->exec = action_stash_create_exec;
	ot->poll = action_stash_create_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

