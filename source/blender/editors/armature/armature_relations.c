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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation, 2002-2009 full recode.
 *
 * ***** END GPL LICENSE BLOCK *****
 *
 * Operators for relations between bones and for transferring bones between armature objects
 */

/** \file blender/editors/armature/armature_relations.c
 *  \ingroup edarmature
 */

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BLF_translation.h"

#include "BKE_action.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_object.h"
#include "ED_screen.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "armature_intern.h"

/* *************************************** Join *************************************** */
/* NOTE: no operator define here as this is exported to the Object-level operator */

static void joined_armature_fix_links_constraints(
        Object *tarArm, Object *srcArm, bPoseChannel *pchan, EditBone *curbone,
        ListBase *lb)
{
	bConstraint *con;

	for (con = lb->first; con; con = con->next) {
		bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
		ListBase targets = {NULL, NULL};
		bConstraintTarget *ct;

		/* constraint targets */
		if (cti && cti->get_constraint_targets) {
			cti->get_constraint_targets(con, &targets);

			for (ct = targets.first; ct; ct = ct->next) {
				if (ct->tar == srcArm) {
					if (ct->subtarget[0] == '\0') {
						ct->tar = tarArm;
					}
					else if (STREQ(ct->subtarget, pchan->name)) {
						ct->tar = tarArm;
						BLI_strncpy(ct->subtarget, curbone->name, sizeof(ct->subtarget));
					}
				}
			}

			if (cti->flush_constraint_targets)
				cti->flush_constraint_targets(con, &targets, 0);
		}

		/* action constraint? (pose constraints only) */
		if (con->type == CONSTRAINT_TYPE_ACTION) {
			bActionConstraint *data = con->data; // XXX old animation system
			bAction *act;
			bActionChannel *achan;

			if (data->act) {
				act = data->act;

				for (achan = act->chanbase.first; achan; achan = achan->next) {
					if (STREQ(achan->name, pchan->name)) {
						BLI_strncpy(achan->name, curbone->name, sizeof(achan->name));
					}
				}
			}
		}

	}
}

/* Helper function for armature joining - link fixing */
static void joined_armature_fix_links(Object *tarArm, Object *srcArm, bPoseChannel *pchan, EditBone *curbone)
{
	Object *ob;
	bPose *pose;
	bPoseChannel *pchant;
	
	/* let's go through all objects in database */
	for (ob = G.main->object.first; ob; ob = ob->id.next) {
		/* do some object-type specific things */
		if (ob->type == OB_ARMATURE) {
			pose = ob->pose;
			for (pchant = pose->chanbase.first; pchant; pchant = pchant->next) {
				joined_armature_fix_links_constraints(tarArm, srcArm, pchan, curbone, &pchant->constraints);
			}
		}
			
		/* fix object-level constraints */
		if (ob != srcArm) {
			joined_armature_fix_links_constraints(tarArm, srcArm, pchan, curbone, &ob->constraints);
		}
		
		/* See if an object is parented to this armature */
		if (ob->parent && (ob->parent == srcArm)) {
			/* Is object parented to a bone of this src armature? */
			if (ob->partype == PARBONE) {
				/* bone name in object */
				if (STREQ(ob->parsubstr, pchan->name)) {
					BLI_strncpy(ob->parsubstr, curbone->name, sizeof(ob->parsubstr));
				}
			}
			
			/* make tar armature be new parent */
			ob->parent = tarArm;
		}
	}
}

/* join armature exec is exported for use in object->join objects operator... */
int join_armature_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object  *ob = CTX_data_active_object(C);
	bArmature *arm = (ob) ? ob->data : NULL;
	bPose *pose, *opose;
	bPoseChannel *pchan, *pchann;
	EditBone *curbone;
	float mat[4][4], oimat[4][4];
	bool ok = false;
	
	/*	Ensure we're not in editmode and that the active object is an armature*/
	if (!ob || ob->type != OB_ARMATURE)
		return OPERATOR_CANCELLED;
	if (!arm || arm->edbo)
		return OPERATOR_CANCELLED;
	
	CTX_DATA_BEGIN(C, Base *, base, selected_editable_bases)
	{
		if (base->object == ob) {
			ok = true;
			break;
		}
	}
	CTX_DATA_END;

	/* that way the active object is always selected */
	if (ok == false) {
		BKE_report(op->reports, RPT_WARNING, "Active object is not a selected armature");
		return OPERATOR_CANCELLED;
	}

	/* Get editbones of active armature to add editbones to */
	ED_armature_to_edit(arm);
	
	/* get pose of active object and move it out of posemode */
	pose = ob->pose;
	ob->mode &= ~OB_MODE_POSE;

	CTX_DATA_BEGIN(C, Base *, base, selected_editable_bases)
	{
		if ((base->object->type == OB_ARMATURE) && (base->object != ob)) {
			bArmature *curarm = base->object->data;
			
			/* Make a list of editbones in current armature */
			ED_armature_to_edit(base->object->data);
			
			/* Get Pose of current armature */
			opose = base->object->pose;
			base->object->mode &= ~OB_MODE_POSE;
			//BASACT->flag &= ~OB_MODE_POSE;
			
			/* Find the difference matrix */
			invert_m4_m4(oimat, ob->obmat);
			mul_m4_m4m4(mat, oimat, base->object->obmat);
			
			/* Copy bones and posechannels from the object to the edit armature */
			for (pchan = opose->chanbase.first; pchan; pchan = pchann) {
				pchann = pchan->next;
				curbone = ED_armature_bone_find_name(curarm->edbo, pchan->name);
				
				/* Get new name */
				unique_editbone_name(arm->edbo, curbone->name, NULL);
				
				/* Transform the bone */
				{
					float premat[4][4];
					float postmat[4][4];
					float difmat[4][4];
					float imat[4][4];
					float temp[3][3];
					
					/* Get the premat */
					ED_armature_ebone_to_mat3(curbone, temp);
					
					unit_m4(premat); /* mul_m4_m3m4 only sets 3x3 part */
					mul_m4_m3m4(premat, temp, mat);
					
					mul_m4_v3(mat, curbone->head);
					mul_m4_v3(mat, curbone->tail);
					
					/* Get the postmat */
					ED_armature_ebone_to_mat3(curbone, temp);
					copy_m4_m3(postmat, temp);
					
					/* Find the roll */
					invert_m4_m4(imat, premat);
					mul_m4_m4m4(difmat, imat, postmat);
					
					curbone->roll -= (float)atan2(difmat[2][0], difmat[2][2]);
				}
				
				/* Fix Constraints and Other Links to this Bone and Armature */
				joined_armature_fix_links(ob, base->object, pchan, curbone);
				
				/* Rename pchan */
				BLI_strncpy(pchan->name, curbone->name, sizeof(pchan->name));
				
				/* Jump Ship! */
				BLI_remlink(curarm->edbo, curbone);
				BLI_addtail(arm->edbo, curbone);
				
				BLI_remlink(&opose->chanbase, pchan);
				BLI_addtail(&pose->chanbase, pchan);
				BKE_pose_channels_hash_free(opose);
				BKE_pose_channels_hash_free(pose);
			}
			
			ED_base_object_free_and_unlink(bmain, scene, base);
		}
	}
	CTX_DATA_END;
	
	DAG_relations_tag_update(bmain);  /* because we removed object(s) */

	ED_armature_from_edit(arm);
	ED_armature_edit_free(arm);

	WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, scene);
	
	return OPERATOR_FINISHED;
}

/* *********************************** Separate *********************************************** */

/* Helper function for armature separating - link fixing */
static void separated_armature_fix_links(Object *origArm, Object *newArm)
{
	Object *ob;
	bPoseChannel *pchan;
	bConstraint *con;
	ListBase *opchans, *npchans;
	
	/* get reference to list of bones in original and new armatures  */
	opchans = &origArm->pose->chanbase;
	npchans = &newArm->pose->chanbase;
	
	/* let's go through all objects in database */
	for (ob = G.main->object.first; ob; ob = ob->id.next) {
		/* do some object-type specific things */
		if (ob->type == OB_ARMATURE) {
			for (pchan = ob->pose->chanbase.first; pchan; pchan = pchan->next) {
				for (con = pchan->constraints.first; con; con = con->next) {
					bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;
					
					/* constraint targets */
					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);
						
						for (ct = targets.first; ct; ct = ct->next) {
							/* any targets which point to original armature are redirected to the new one only if:
							 *	- the target isn't origArm/newArm itself
							 *	- the target is one that can be found in newArm/origArm
							 */
							if (ct->subtarget[0] != 0) {
								if (ct->tar == origArm) {
									if (BLI_findstring(npchans, ct->subtarget, offsetof(bPoseChannel, name))) {
										ct->tar = newArm;
									}
								}
								else if (ct->tar == newArm) {
									if (BLI_findstring(opchans, ct->subtarget, offsetof(bPoseChannel, name))) {
										ct->tar = origArm;
									}
								}
							}
						}

						if (cti->flush_constraint_targets) {
							cti->flush_constraint_targets(con, &targets, 0);
						}
					}
				}
			}
		}
			
		/* fix object-level constraints */
		if (ob != origArm) {
			for (con = ob->constraints.first; con; con = con->next) {
				bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(con);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;
				
				/* constraint targets */
				if (cti && cti->get_constraint_targets) {
					cti->get_constraint_targets(con, &targets);
					
					for (ct = targets.first; ct; ct = ct->next) {
						/* any targets which point to original armature are redirected to the new one only if:
						 *	- the target isn't origArm/newArm itself
						 *	- the target is one that can be found in newArm/origArm
						 */
						if (ct->subtarget[0] != '\0') {
							if (ct->tar == origArm) {
								if (BLI_findstring(npchans, ct->subtarget, offsetof(bPoseChannel, name))) {
									ct->tar = newArm;
								}
							}
							else if (ct->tar == newArm) {
								if (BLI_findstring(opchans, ct->subtarget, offsetof(bPoseChannel, name))) {
									ct->tar = origArm;
								}
							}
						}
					}
					
					if (cti->flush_constraint_targets) {
						cti->flush_constraint_targets(con, &targets, 0);
					}
				}
			}
		}
		
		/* See if an object is parented to this armature */
		if (ob->parent && (ob->parent == origArm)) {
			/* Is object parented to a bone of this src armature? */
			if ((ob->partype == PARBONE) && (ob->parsubstr[0] != '\0')) {
				if (BLI_findstring(npchans, ob->parsubstr, offsetof(bPoseChannel, name))) {
					ob->parent = newArm;
				}
			}
		}
	}
}

/* Helper function for armature separating - remove certain bones from the given armature 
 *	sel: remove selected bones from the armature, otherwise the unselected bones are removed
 *  (ob is not in editmode)
 */
static void separate_armature_bones(Object *ob, short sel) 
{
	bArmature *arm = (bArmature *)ob->data;
	bPoseChannel *pchan, *pchann;
	EditBone *curbone;
	
	/* make local set of editbones to manipulate here */
	ED_armature_to_edit(arm);
	
	/* go through pose-channels, checking if a bone should be removed */
	for (pchan = ob->pose->chanbase.first; pchan; pchan = pchann) {
		pchann = pchan->next;
		curbone = ED_armature_bone_find_name(arm->edbo, pchan->name);
		
		/* check if bone needs to be removed */
		if ( (sel && (curbone->flag & BONE_SELECTED)) ||
		     (!sel && !(curbone->flag & BONE_SELECTED)) )
		{
			EditBone *ebo;
			bPoseChannel *pchn;
			
			/* clear the bone->parent var of any bone that had this as its parent  */
			for (ebo = arm->edbo->first; ebo; ebo = ebo->next) {
				if (ebo->parent == curbone) {
					ebo->parent = NULL;
					ebo->temp = NULL; /* this is needed to prevent random crashes with in ED_armature_from_edit */
					ebo->flag &= ~BONE_CONNECTED;
				}
			}
			
			/* clear the pchan->parent var of any pchan that had this as its parent */
			for (pchn = ob->pose->chanbase.first; pchn; pchn = pchn->next) {
				if (pchn->parent == pchan)
					pchn->parent = NULL;
			}
			
			/* free any of the extra-data this pchan might have */
			BKE_pose_channel_free(pchan);
			BKE_pose_channels_hash_free(ob->pose);
			
			/* get rid of unneeded bone */
			bone_free(arm, curbone);
			BLI_freelinkN(&ob->pose->chanbase, pchan);
		}
	}
	
	/* exit editmode (recalculates pchans too) */
	ED_armature_from_edit(ob->data);
	ED_armature_edit_free(ob->data);
}

/* separate selected bones into their armature */
static int separate_armature_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Object *oldob, *newob;
	Base *oldbase, *newbase;
	
	/* sanity checks */
	if (obedit == NULL)
		return OPERATOR_CANCELLED;
	
	/* set wait cursor in case this takes a while */
	WM_cursor_wait(1);
	
	/* we are going to do this as follows (unlike every other instance of separate):
	 *	1. exit editmode +posemode for active armature/base. Take note of what this is.
	 *	2. duplicate base - BASACT is the new one now
	 *	3. for each of the two armatures, enter editmode -> remove appropriate bones -> exit editmode + recalc
	 *	4. fix constraint links
	 *	5. make original armature active and enter editmode
	 */

	/* 1) only edit-base selected */
	/* TODO: use context iterators for this? */
	CTX_DATA_BEGIN(C, Base *, base, visible_bases)
	{
		if (base->object == obedit) base->flag |= SELECT;
		else base->flag &= ~SELECT;
	}
	CTX_DATA_END;
	
	/* 1) store starting settings and exit editmode */
	oldob = obedit;
	oldbase = BASACT;
	oldob->mode &= ~OB_MODE_POSE;
	//oldbase->flag &= ~OB_POSEMODE;
	
	ED_armature_from_edit(obedit->data);
	ED_armature_edit_free(obedit->data);
	
	/* 2) duplicate base */
	newbase = ED_object_add_duplicate(bmain, scene, oldbase, USER_DUP_ARM); /* only duplicate linked armature */
	DAG_relations_tag_update(bmain);

	newob = newbase->object;
	newbase->flag &= ~SELECT;
	
	
	/* 3) remove bones that shouldn't still be around on both armatures */
	separate_armature_bones(oldob, 1);
	separate_armature_bones(newob, 0);
	
	
	/* 4) fix links before depsgraph flushes */ // err... or after?
	separated_armature_fix_links(oldob, newob);
	
	DAG_id_tag_update(&oldob->id, OB_RECALC_DATA);  /* this is the original one */
	DAG_id_tag_update(&newob->id, OB_RECALC_DATA);  /* this is the separated one */
	
	
	/* 5) restore original conditions */
	obedit = oldob;
	
	ED_armature_to_edit(obedit->data);
	
	BKE_report(op->reports, RPT_INFO, "Separated bones");

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, obedit);
	
	/* recalc/redraw + cleanup */
	WM_cursor_wait(0);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_separate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Separate Bones";
	ot->idname = "ARMATURE_OT_separate";
	ot->description = "Isolate selected bones into a separate armature";
	
	/* callbacks */
	ot->exec = separate_armature_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ******************************************** Parenting ************************************************* */

/* armature parenting options */
#define ARM_PAR_CONNECT 1
#define ARM_PAR_OFFSET  2


/* check for null, before calling! */
static void bone_connect_to_existing_parent(EditBone *bone)
{
	bone->flag |= BONE_CONNECTED;
	copy_v3_v3(bone->head, bone->parent->tail);
	bone->rad_head = bone->parent->rad_tail;
}

static void bone_connect_to_new_parent(ListBase *edbo, EditBone *selbone, EditBone *actbone, short mode)
{
	EditBone *ebone;
	float offset[3];
	
	if ((selbone->parent) && (selbone->flag & BONE_CONNECTED))
		selbone->parent->flag &= ~(BONE_TIPSEL);
	
	/* make actbone the parent of selbone */
	selbone->parent = actbone;
	
	/* in actbone tree we cannot have a loop */
	for (ebone = actbone->parent; ebone; ebone = ebone->parent) {
		if (ebone->parent == selbone) {
			ebone->parent = NULL;
			ebone->flag &= ~BONE_CONNECTED;
		}
	}
	
	if (mode == ARM_PAR_CONNECT) {
		/* Connected: Child bones will be moved to the parent tip */
		selbone->flag |= BONE_CONNECTED;
		sub_v3_v3v3(offset, actbone->tail, selbone->head);
		
		copy_v3_v3(selbone->head, actbone->tail);
		selbone->rad_head = actbone->rad_tail;
		
		add_v3_v3(selbone->tail, offset);
		
		/* offset for all its children */
		for (ebone = edbo->first; ebone; ebone = ebone->next) {
			EditBone *par;
			
			for (par = ebone->parent; par; par = par->parent) {
				if (par == selbone) {
					add_v3_v3(ebone->head, offset);
					add_v3_v3(ebone->tail, offset);
					break;
				}
			}
		}
	}
	else {
		/* Offset: Child bones will retain their distance from the parent tip */
		selbone->flag &= ~BONE_CONNECTED;
	}
}


static EnumPropertyItem prop_editarm_make_parent_types[] = {
	{ARM_PAR_CONNECT, "CONNECTED", 0, "Connected", ""},
	{ARM_PAR_OFFSET, "OFFSET", 0, "Keep Offset", ""},
	{0, NULL, 0, NULL, NULL}
};

static int armature_parent_set_exec(bContext *C, wmOperator *op) 
{
	Object *ob = CTX_data_edit_object(C);
	bArmature *arm = (bArmature *)ob->data;
	EditBone *actbone = CTX_data_active_bone(C);
	EditBone *actmirb = NULL;
	short val = RNA_enum_get(op->ptr, "type");
	
	/* there must be an active bone */
	if (actbone == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Operation requires an active bone");
		return OPERATOR_CANCELLED;
	}
	else if (arm->flag & ARM_MIRROR_EDIT) {
		/* For X-Axis Mirror Editing option, we may need a mirror copy of actbone
		 * - if there's a mirrored copy of selbone, try to find a mirrored copy of actbone 
		 *   (i.e.  selbone="child.L" and actbone="parent.L", find "child.R" and "parent.R").
		 * This is useful for arm-chains, for example parenting lower arm to upper arm
		 * - if there's no mirrored copy of actbone (i.e. actbone = "parent.C" or "parent")
		 *   then just use actbone. Useful when doing upper arm to spine.
		 */
		actmirb = ED_armature_bone_get_mirrored(arm->edbo, actbone);
		if (actmirb == NULL) 
			actmirb = actbone;
	}
	
	/* if there is only 1 selected bone, we assume that that is the active bone, 
	 * since a user will need to have clicked on a bone (thus selecting it) to make it active
	 */
	if (CTX_DATA_COUNT(C, selected_editable_bones) <= 1) {
		/* When only the active bone is selected, and it has a parent,
		 * connect it to the parent, as that is the only possible outcome. 
		 */
		if (actbone->parent) {
			bone_connect_to_existing_parent(actbone);
			
			if ((arm->flag & ARM_MIRROR_EDIT) && (actmirb->parent))
				bone_connect_to_existing_parent(actmirb);
		}
	}
	else {
		/* Parent 'selected' bones to the active one
		 * - the context iterator contains both selected bones and their mirrored copies,
		 *   so we assume that unselected bones are mirrored copies of some selected bone
		 * - since the active one (and/or its mirror) will also be selected, we also need 
		 *  to check that we are not trying to operate on them, since such an operation
		 *	would cause errors
		 */
		
		/* parent selected bones to the active one */
		CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones)
		{
			if (ELEM(ebone, actbone, actmirb) == 0) {
				if (ebone->flag & BONE_SELECTED) 
					bone_connect_to_new_parent(arm->edbo, ebone, actbone, val);
				else
					bone_connect_to_new_parent(arm->edbo, ebone, actmirb, val);
			}
		}
		CTX_DATA_END;
	}
	

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
	
	return OPERATOR_FINISHED;
}

static int armature_parent_set_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
	EditBone *actbone = CTX_data_active_bone(C);
	uiPopupMenu *pup = uiPupMenuBegin(C, CTX_IFACE_(BLF_I18NCONTEXT_OPERATOR_DEFAULT, "Make Parent"), ICON_NONE);
	uiLayout *layout = uiPupMenuLayout(pup);
	int allchildbones = 0;
	
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones)
	{
		if (ebone != actbone) {
			if (ebone->parent != actbone) allchildbones = 1;
		}
	}
	CTX_DATA_END;

	uiItemEnumO(layout, "ARMATURE_OT_parent_set", NULL, 0, "type", ARM_PAR_CONNECT);
	
	/* ob becomes parent, make the associated menus */
	if (allchildbones)
		uiItemEnumO(layout, "ARMATURE_OT_parent_set", NULL, 0, "type", ARM_PAR_OFFSET);
		
	uiPupMenuEnd(C, pup);
	
	return OPERATOR_CANCELLED;
}

void ARMATURE_OT_parent_set(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Make Parent";
	ot->idname = "ARMATURE_OT_parent_set";
	ot->description = "Set the active bone as the parent of the selected bones";
	
	/* api callbacks */
	ot->invoke = armature_parent_set_invoke;
	ot->exec = armature_parent_set_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_enum(ot->srna, "type", prop_editarm_make_parent_types, 0, "ParentType", "Type of parenting");
}



static EnumPropertyItem prop_editarm_clear_parent_types[] = {
	{1, "CLEAR", 0, "Clear Parent", ""},
	{2, "DISCONNECT", 0, "Disconnect Bone", ""},
	{0, NULL, 0, NULL, NULL}
};

static void editbone_clear_parent(EditBone *ebone, int mode)
{
	if (ebone->parent) {
		/* for nice selection */
		ebone->parent->flag &= ~(BONE_TIPSEL);
	}
	
	if (mode == 1) ebone->parent = NULL;
	ebone->flag &= ~BONE_CONNECTED;
}

static int armature_parent_clear_exec(bContext *C, wmOperator *op) 
{
	Object *ob = CTX_data_edit_object(C);
	bArmature *arm = (bArmature *)ob->data;
	int val = RNA_enum_get(op->ptr, "type");
		
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones)
	{
		editbone_clear_parent(ebone, val);
	}
	CTX_DATA_END;
	
	ED_armature_sync_selection(arm->edbo);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_parent_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Parent";
	ot->idname = "ARMATURE_OT_parent_clear";
	ot->description = "Remove the parent-child relationship between selected bones and their parents";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = armature_parent_clear_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	ot->prop = RNA_def_enum(ot->srna, "type", prop_editarm_clear_parent_types, 0, "ClearType", "What way to clear parenting");
}

