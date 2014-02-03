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
 * Operators and API's for creating bones
 */

/** \file blender/editors/armature/armature_add.c
 *  \ingroup edarmature
 */

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_action.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_idprop.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "armature_intern.h"

/* *************** Adding stuff in editmode *************** */

/* default bone add, returns it selected, but without tail set */
/* XXX should be used everywhere, now it mallocs bones still locally in functions */
EditBone *ED_armature_edit_bone_add(bArmature *arm, const char *name)
{
	EditBone *bone = MEM_callocN(sizeof(EditBone), "eBone");
	
	BLI_strncpy(bone->name, name, sizeof(bone->name));
	unique_editbone_name(arm->edbo, bone->name, NULL);
	
	BLI_addtail(arm->edbo, bone);
	
	bone->flag |= BONE_TIPSEL;
	bone->weight = 1.0f;
	bone->dist = 0.25f;
	bone->xwidth = 0.1f;
	bone->zwidth = 0.1f;
	bone->ease1 = 1.0f;
	bone->ease2 = 1.0f;
	bone->rad_head = 0.10f;
	bone->rad_tail = 0.05f;
	bone->segments = 1;
	bone->layer = arm->layer;
	
	return bone;
}

void add_primitive_bone(Object *obedit_arm, bool view_aligned)
{
	bArmature *arm = obedit_arm->data;
	EditBone *bone;

	ED_armature_deselect_all(obedit_arm, 0);
	
	/* Create a bone */
	bone = ED_armature_edit_bone_add(arm, "Bone");

	arm->act_edbone = bone;

	zero_v3(bone->head);
	zero_v3(bone->tail);

	if (view_aligned)
		bone->tail[1] = 1.0f;
	else
		bone->tail[2] = 1.0f;
}


/* previously addvert_armature */
/* the ctrl-click method */
static int armature_click_extrude_exec(bContext *C, wmOperator *UNUSED(op))
{
	View3D *v3d;
	bArmature *arm;
	EditBone *ebone, *newbone, *flipbone;
	float mat[3][3], imat[3][3];
	const float *curs;
	int a, to_root = 0;
	Object *obedit;
	Scene *scene;

	scene = CTX_data_scene(C);
	v3d = CTX_wm_view3d(C);
	obedit = CTX_data_edit_object(C);
	arm = obedit->data;
	
	/* find the active or selected bone */
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_VISIBLE(arm, ebone)) {
			if (ebone->flag & BONE_TIPSEL || arm->act_edbone == ebone)
				break;
		}
	}
	
	if (ebone == NULL) {
		for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
			if (EBONE_VISIBLE(arm, ebone)) {
				if (ebone->flag & BONE_ROOTSEL || arm->act_edbone == ebone)
					break;
			}
		}
		if (ebone == NULL) 
			return OPERATOR_CANCELLED;
		
		to_root = 1;
	}
	
	ED_armature_deselect_all(obedit, 0);
	
	/* we re-use code for mirror editing... */
	flipbone = NULL;
	if (arm->flag & ARM_MIRROR_EDIT)
		flipbone = ED_armature_bone_get_mirrored(arm->edbo, ebone);

	for (a = 0; a < 2; a++) {
		if (a == 1) {
			if (flipbone == NULL)
				break;
			else {
				SWAP(EditBone *, flipbone, ebone);
			}
		}
		
		newbone = ED_armature_edit_bone_add(arm, ebone->name);
		arm->act_edbone = newbone;
		
		if (to_root) {
			copy_v3_v3(newbone->head, ebone->head);
			newbone->rad_head = ebone->rad_tail;
			newbone->parent = ebone->parent;
		}
		else {
			copy_v3_v3(newbone->head, ebone->tail);
			newbone->rad_head = ebone->rad_tail;
			newbone->parent = ebone;
			newbone->flag |= BONE_CONNECTED;
		}
		
		curs = ED_view3d_cursor3d_get(scene, v3d);
		copy_v3_v3(newbone->tail, curs);
		sub_v3_v3v3(newbone->tail, newbone->tail, obedit->obmat[3]);
		
		if (a == 1)
			newbone->tail[0] = -newbone->tail[0];
		
		copy_m3_m4(mat, obedit->obmat);
		invert_m3_m3(imat, mat);
		mul_m3_v3(imat, newbone->tail);
		
		newbone->length = len_v3v3(newbone->head, newbone->tail);
		newbone->rad_tail = newbone->length * 0.05f;
		newbone->dist = newbone->length * 0.25f;
		
	}
	
	ED_armature_sync_selection(arm->edbo);

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

static int armature_click_extrude_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	/* TODO most of this code is copied from set3dcursor_invoke,
	 * it would be better to reuse code in set3dcursor_invoke */

	/* temporarily change 3d cursor position */
	Scene *scene;
	ARegion *ar;
	View3D *v3d;
	float *fp, tvec[3], oldcurs[3], mval_f[2];
	int retv;

	scene = CTX_data_scene(C);
	ar = CTX_wm_region(C);
	v3d = CTX_wm_view3d(C);
	
	fp = ED_view3d_cursor3d_get(scene, v3d);
	
	copy_v3_v3(oldcurs, fp);

	VECCOPY2D(mval_f, event->mval);
	ED_view3d_win_to_3d(ar, fp, mval_f, tvec);
	copy_v3_v3(fp, tvec);

	/* extrude to the where new cursor is and store the operation result */
	retv = armature_click_extrude_exec(C, op);

	/* restore previous 3d cursor position */
	copy_v3_v3(fp, oldcurs);

	return retv;
}

void ARMATURE_OT_click_extrude(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Click-Extrude";
	ot->idname = "ARMATURE_OT_click_extrude";
	ot->description = "Create a new bone going from the last selected joint to the mouse position";
	
	/* api callbacks */
	ot->invoke = armature_click_extrude_invoke;
	ot->exec = armature_click_extrude_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
}

/* adds an EditBone between the nominated locations (should be in the right space) */
EditBone *add_points_bone(Object *obedit, float head[3], float tail[3])
{
	EditBone *ebo;
	
	ebo = ED_armature_edit_bone_add(obedit->data, "Bone");
	
	copy_v3_v3(ebo->head, head);
	copy_v3_v3(ebo->tail, tail);
	
	return ebo;
}


static EditBone *get_named_editbone(ListBase *edbo, const char *name)
{
	EditBone  *eBone;

	if (name) {
		for (eBone = edbo->first; eBone; eBone = eBone->next) {
			if (!strcmp(name, eBone->name))
				return eBone;
		}
	}

	return NULL;
}

/* Call this before doing any duplications
 * */
void preEditBoneDuplicate(ListBase *editbones)
{
	EditBone *eBone;
	
	/* clear temp */
	for (eBone = editbones->first; eBone; eBone = eBone->next) {
		eBone->temp = NULL;
	}
}

/*
 * Note: When duplicating cross objects, editbones here is the list of bones
 * from the SOURCE object but ob is the DESTINATION object
 * */
void updateDuplicateSubtargetObjects(EditBone *dupBone, ListBase *editbones, Object *src_ob, Object *dst_ob)
{
	/* If an edit bone has been duplicated, lets
	 * update it's constraints if the subtarget
	 * they point to has also been duplicated
	 */
	EditBone     *oldtarget, *newtarget;
	bPoseChannel *pchan;
	bConstraint  *curcon;
	ListBase     *conlist;
	
	if ((pchan = BKE_pose_channel_verify(dst_ob->pose, dupBone->name))) {
		if ((conlist = &pchan->constraints)) {
			for (curcon = conlist->first; curcon; curcon = curcon->next) {
				/* does this constraint have a subtarget in
				 * this armature?
				 */
				bConstraintTypeInfo *cti = BKE_constraint_get_typeinfo(curcon);
				ListBase targets = {NULL, NULL};
				bConstraintTarget *ct;
				
				if (cti && cti->get_constraint_targets) {
					cti->get_constraint_targets(curcon, &targets);
					
					for (ct = targets.first; ct; ct = ct->next) {
						if ((ct->tar == src_ob) && (ct->subtarget[0])) {
							ct->tar = dst_ob; /* update target */ 
							oldtarget = get_named_editbone(editbones, ct->subtarget);
							if (oldtarget) {
								/* was the subtarget bone duplicated too? If
								 * so, update the constraint to point at the 
								 * duplicate of the old subtarget.
								 */
								if (oldtarget->temp) {
									newtarget = (EditBone *) oldtarget->temp;
									BLI_strncpy(ct->subtarget, newtarget->name, sizeof(ct->subtarget));
								}
							}
						}
					}
					
					if (cti->flush_constraint_targets)
						cti->flush_constraint_targets(curcon, &targets, 0);
				}
			}
		}
	}
}

void updateDuplicateSubtarget(EditBone *dupBone, ListBase *editbones, Object *ob)
{
	updateDuplicateSubtargetObjects(dupBone, editbones, ob, ob);
}


EditBone *duplicateEditBoneObjects(EditBone *curBone, const char *name, ListBase *editbones,
                                   Object *src_ob, Object *dst_ob)
{
	EditBone *eBone = MEM_mallocN(sizeof(EditBone), "addup_editbone");
	
	/*	Copy data from old bone to new bone */
	memcpy(eBone, curBone, sizeof(EditBone));
	
	curBone->temp = eBone;
	eBone->temp = curBone;
	
	if (name != NULL) {
		BLI_strncpy(eBone->name, name, sizeof(eBone->name));
	}

	unique_editbone_name(editbones, eBone->name, NULL);
	BLI_addtail(editbones, eBone);
	
	/* copy the ID property */
	if (curBone->prop)
		eBone->prop = IDP_CopyProperty(curBone->prop);

	/* Lets duplicate the list of constraints that the
	 * current bone has.
	 */
	if (src_ob->pose) {
		bPoseChannel *chanold, *channew;
		
		chanold = BKE_pose_channel_verify(src_ob->pose, curBone->name);
		if (chanold) {
			/* WARNING: this creates a new posechannel, but there will not be an attached bone
			 *		yet as the new bones created here are still 'EditBones' not 'Bones'.
			 */
			channew = BKE_pose_channel_verify(dst_ob->pose, eBone->name);

			if (channew) {
				BKE_pose_channel_copy_data(channew, chanold);
			}
		}
	}
	
	return eBone;
}

EditBone *duplicateEditBone(EditBone *curBone, const char *name, ListBase *editbones, Object *ob)
{
	return duplicateEditBoneObjects(curBone, name, editbones, ob, ob);
}

/* previously adduplicate_armature */
static int armature_duplicate_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
	bArmature *arm;
	EditBone    *eBone = NULL;
	EditBone    *curBone;
	EditBone    *firstDup = NULL; /*	The beginning of the duplicated bones in the edbo list */

	Object *obedit = CTX_data_edit_object(C);
	arm = obedit->data;

	/* cancel if nothing selected */
	if (CTX_DATA_COUNT(C, selected_bones) == 0)
		return OPERATOR_CANCELLED;
	
	ED_armature_sync_selection(arm->edbo); // XXX why is this needed?

	preEditBoneDuplicate(arm->edbo);

	/* Select mirrored bones */
	if (arm->flag & ARM_MIRROR_EDIT) {
		for (curBone = arm->edbo->first; curBone; curBone = curBone->next) {
			if (EBONE_VISIBLE(arm, curBone)) {
				if (curBone->flag & BONE_SELECTED) {
					eBone = ED_armature_bone_get_mirrored(arm->edbo, curBone);
					if (eBone)
						eBone->flag |= BONE_SELECTED;
				}
			}
		}
	}

	
	/*	Find the selected bones and duplicate them as needed */
	for (curBone = arm->edbo->first; curBone && curBone != firstDup; curBone = curBone->next) {
		if (EBONE_VISIBLE(arm, curBone)) {
			if (curBone->flag & BONE_SELECTED) {
				
				eBone = duplicateEditBone(curBone, curBone->name, arm->edbo, obedit);
				
				if (!firstDup)
					firstDup = eBone;

			}
		}
	}

	/*	Run though the list and fix the pointers */
	for (curBone = arm->edbo->first; curBone && curBone != firstDup; curBone = curBone->next) {
		if (EBONE_VISIBLE(arm, curBone)) {
			if (curBone->flag & BONE_SELECTED) {
				eBone = (EditBone *) curBone->temp;
				
				if (!curBone->parent) {
					/* If this bone has no parent,
					 * Set the duplicate->parent to NULL
					 */
					eBone->parent = NULL;
				}
				else if (curBone->parent->temp) {
					/* If this bone has a parent that was duplicated,
					 * Set the duplicate->parent to the curBone->parent->temp
					 */
					eBone->parent = (EditBone *)curBone->parent->temp;
				}
				else {
					/* If this bone has a parent that IS not selected,
					 * Set the duplicate->parent to the curBone->parent
					 */
					eBone->parent = (EditBone *) curBone->parent;
					eBone->flag &= ~BONE_CONNECTED;
				}
				
				/* Lets try to fix any constraint subtargets that might
				 * have been duplicated 
				 */
				updateDuplicateSubtarget(eBone, arm->edbo, obedit);
			}
		}
	}
	
	/* correct the active bone */
	if (arm->act_edbone) {
		eBone = arm->act_edbone;
		if (eBone->temp)
			arm->act_edbone = eBone->temp;
	}

	/*	Deselect the old bones and select the new ones */
	for (curBone = arm->edbo->first; curBone && curBone != firstDup; curBone = curBone->next) {
		if (EBONE_VISIBLE(arm, curBone))
			curBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
	}

	ED_armature_validate_active(arm);

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}


void ARMATURE_OT_duplicate(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Duplicate Selected Bone(s)";
	ot->idname = "ARMATURE_OT_duplicate";
	ot->description = "Make copies of the selected bones within the same armature";
	
	/* api callbacks */
	ot->exec = armature_duplicate_selected_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ------------------------------------------ */

/* previously extrude_armature */
/* context; editmode armature */
/* if forked && mirror-edit: makes two bones with flipped names */
static int armature_extrude_exec(bContext *C, wmOperator *op)
{
	Object *obedit;
	bArmature *arm;
	EditBone *newbone, *ebone, *flipbone, *first = NULL;
	int a, totbone = 0, do_extrude;
	bool forked = RNA_boolean_get(op->ptr, "forked");

	obedit = CTX_data_edit_object(C);
	arm = obedit->data;

	/* since we allow root extrude too, we have to make sure selection is OK */
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_VISIBLE(arm, ebone)) {
			if (ebone->flag & BONE_ROOTSEL) {
				if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
					if (ebone->parent->flag & BONE_TIPSEL)
						ebone->flag &= ~BONE_ROOTSEL;
				}
			}
		}
	}
	
	/* Duplicate the necessary bones */
	for (ebone = arm->edbo->first; ((ebone) && (ebone != first)); ebone = ebone->next) {
		if (EBONE_VISIBLE(arm, ebone)) {
			/* we extrude per definition the tip */
			do_extrude = FALSE;
			if (ebone->flag & (BONE_TIPSEL | BONE_SELECTED)) {
				do_extrude = TRUE;
			}
			else if (ebone->flag & BONE_ROOTSEL) {
				/* but, a bone with parent deselected we do the root... */
				if (ebone->parent && (ebone->parent->flag & BONE_TIPSEL)) {
					/* pass */
				}
				else {
					do_extrude = 2;
				}
			}
			
			if (do_extrude) {
				/* we re-use code for mirror editing... */
				flipbone = NULL;
				if (arm->flag & ARM_MIRROR_EDIT) {
					flipbone = ED_armature_bone_get_mirrored(arm->edbo, ebone);
					if (flipbone) {
						forked = 0;  // we extrude 2 different bones
						if (flipbone->flag & (BONE_TIPSEL | BONE_ROOTSEL | BONE_SELECTED))
							/* don't want this bone to be selected... */
							flipbone->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
					}
					if ((flipbone == NULL) && (forked))
						flipbone = ebone;
				}
				
				for (a = 0; a < 2; a++) {
					if (a == 1) {
						if (flipbone == NULL)
							break;
						else {
							SWAP(EditBone *, flipbone, ebone);
						}
					}
					
					totbone++;
					newbone = MEM_callocN(sizeof(EditBone), "extrudebone");
					
					if (do_extrude == TRUE) {
						copy_v3_v3(newbone->head, ebone->tail);
						copy_v3_v3(newbone->tail, newbone->head);
						newbone->parent = ebone;
						
						newbone->flag = ebone->flag & (BONE_TIPSEL | BONE_RELATIVE_PARENTING);  // copies it, in case mirrored bone
						
						if (newbone->parent) newbone->flag |= BONE_CONNECTED;
					}
					else {
						copy_v3_v3(newbone->head, ebone->head);
						copy_v3_v3(newbone->tail, ebone->head);
						newbone->parent = ebone->parent;
						
						newbone->flag = BONE_TIPSEL;
						
						if (newbone->parent && (ebone->flag & BONE_CONNECTED)) {
							newbone->flag |= BONE_CONNECTED;
						}
					}
					
					newbone->weight = ebone->weight;
					newbone->dist = ebone->dist;
					newbone->xwidth = ebone->xwidth;
					newbone->zwidth = ebone->zwidth;
					newbone->ease1 = ebone->ease1;
					newbone->ease2 = ebone->ease2;
					newbone->rad_head = ebone->rad_tail; // don't copy entire bone...
					newbone->rad_tail = ebone->rad_tail;
					newbone->segments = 1;
					newbone->layer = ebone->layer;
					
					BLI_strncpy(newbone->name, ebone->name, sizeof(newbone->name));
					
					if (flipbone && forked) {   // only set if mirror edit
						if (strlen(newbone->name) < (MAXBONENAME - 2)) {
							if (a == 0) strcat(newbone->name, "_L");
							else strcat(newbone->name, "_R");
						}
					}
					unique_editbone_name(arm->edbo, newbone->name, NULL);
					
					/* Add the new bone to the list */
					BLI_addtail(arm->edbo, newbone);
					if (!first)
						first = newbone;
					
					/* restore ebone if we were flipping */
					if (a == 1 && flipbone)
						SWAP(EditBone *, flipbone, ebone);
				}
			}
			
			/* Deselect the old bone */
			ebone->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
		}
	}
	/* if only one bone, make this one active */
	if (totbone == 1 && first) arm->act_edbone = first;

	if (totbone == 0) return OPERATOR_CANCELLED;

	/* Transform the endpoints */
	ED_armature_sync_selection(arm->edbo);

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_extrude(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Extrude";
	ot->idname = "ARMATURE_OT_extrude";
	ot->description = "Create new bones from the selected joints";
	
	/* api callbacks */
	ot->exec = armature_extrude_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* props */
	RNA_def_boolean(ot->srna, "forked", 0, "Forked", "");
}

/* ********************** Bone Add *************************************/

/*op makes a new bone and returns it with its tip selected */

static int armature_bone_primitive_add_exec(bContext *C, wmOperator *op) 
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	Object *obedit = CTX_data_edit_object(C);
	EditBone *bone;
	float obmat[3][3], curs[3], viewmat[3][3], totmat[3][3], imat[3][3];
	char name[MAXBONENAME];
	
	RNA_string_get(op->ptr, "name", name);
	
	copy_v3_v3(curs, ED_view3d_cursor3d_get(CTX_data_scene(C), CTX_wm_view3d(C)));

	/* Get inverse point for head and orientation for tail */
	invert_m4_m4(obedit->imat, obedit->obmat);
	mul_m4_v3(obedit->imat, curs);

	if (rv3d && (U.flag & USER_ADD_VIEWALIGNED))
		copy_m3_m4(obmat, rv3d->viewmat);
	else unit_m3(obmat);
	
	copy_m3_m4(viewmat, obedit->obmat);
	mul_m3_m3m3(totmat, obmat, viewmat);
	invert_m3_m3(imat, totmat);
	
	ED_armature_deselect_all(obedit, 0);
	
	/*	Create a bone	*/
	bone = ED_armature_edit_bone_add(obedit->data, name);

	copy_v3_v3(bone->head, curs);
	
	if (rv3d && (U.flag & USER_ADD_VIEWALIGNED))
		add_v3_v3v3(bone->tail, bone->head, imat[1]);   // bone with unit length 1
	else
		add_v3_v3v3(bone->tail, bone->head, imat[2]);   // bone with unit length 1, pointing up Z

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_bone_primitive_add(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Add Bone";
	ot->idname = "ARMATURE_OT_bone_primitive_add";
	ot->description = "Add a new bone located at the 3D-Cursor";
	
	/* api callbacks */
	ot->exec = armature_bone_primitive_add_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	RNA_def_string(ot->srna, "name", "Bone", MAXBONENAME, "Name", "Name of the newly created bone");
	
}

/* ********************** Subdivide *******************************/

/* Subdivide Operators:
 * This group of operators all use the same 'exec' callback, but they are called
 * through several different operators - a combined menu (which just calls the exec in the 
 * appropriate ways), and two separate ones.
 */

static int armature_subdivide_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	bArmature *arm = obedit->data;
	EditBone *newbone, *tbone;
	int cuts, i;
	
	/* there may not be a number_cuts property defined (for 'simple' subdivide) */
	cuts = RNA_int_get(op->ptr, "number_cuts");
	
	/* loop over all editable bones */
	// XXX the old code did this in reverse order though!
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones)
	{
		for (i = cuts + 1; i > 1; i--) {
			/* compute cut ratio first */
			float cutratio = 1.0f / (float)i;
			float cutratioI = 1.0f - cutratio;
			
			float val1[3];
			float val2[3];
			float val3[3];
			
			newbone = MEM_mallocN(sizeof(EditBone), "ebone subdiv");
			*newbone = *ebone;
			BLI_addtail(arm->edbo, newbone);
			
			/* calculate location of newbone->head */
			copy_v3_v3(val1, ebone->head);
			copy_v3_v3(val2, ebone->tail);
			copy_v3_v3(val3, newbone->head);
			
			val3[0] = val1[0] * cutratio + val2[0] * cutratioI;
			val3[1] = val1[1] * cutratio + val2[1] * cutratioI;
			val3[2] = val1[2] * cutratio + val2[2] * cutratioI;
			
			copy_v3_v3(newbone->head, val3);
			copy_v3_v3(newbone->tail, ebone->tail);
			copy_v3_v3(ebone->tail, newbone->head);
			
			newbone->rad_head = ((ebone->rad_head * cutratio) + (ebone->rad_tail * cutratioI));
			ebone->rad_tail = newbone->rad_head;
			
			newbone->flag |= BONE_CONNECTED;

			newbone->prop = NULL;

			unique_editbone_name(arm->edbo, newbone->name, NULL);
			
			/* correct parent bones */
			for (tbone = arm->edbo->first; tbone; tbone = tbone->next) {
				if (tbone->parent == ebone)
					tbone->parent = newbone;
			}
			newbone->parent = ebone;
		}
	}
	CTX_DATA_END;
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_subdivide(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Subdivide Multi";
	ot->idname = "ARMATURE_OT_subdivide";
	ot->description = "Break selected bones into chains of smaller bones";
	
	/* api callbacks */
	ot->exec = armature_subdivide_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* Properties */
	prop = RNA_def_int(ot->srna, "number_cuts", 1, 1, INT_MAX, "Number of Cuts", "", 1, 10);
	/* avoid re-using last var because it can cause _very_ high poly meshes and annoy users (or worse crash) */
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
}
