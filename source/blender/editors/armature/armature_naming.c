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
 * Operators and API's for renaming bones both in and out of Edit Mode
 */

/** \file blender/editors/armature/armature_naming.c
 *  \ingroup edarmature
 */

#include <string.h>

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_ghash.h"

#include "BLF_translation.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_modifier.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_screen.h"

#include "armature_intern.h"

/* This file contains functions/API's for renaming bones and/or working with them */

/* ************************************************** */
/* EditBone Names */

/* note: there's a unique_bone_name() too! */
static bool editbone_unique_check(void *arg, const char *name)
{
	struct {ListBase *lb; void *bone; } *data = arg;
	EditBone *dupli = ED_armature_bone_find_name(data->lb, name);
	return dupli && dupli != data->bone;
}

void unique_editbone_name(ListBase *edbo, char *name, EditBone *bone)
{
	struct {ListBase *lb; void *bone; } data;
	data.lb = edbo;
	data.bone = bone;

	BLI_uniquename_cb(editbone_unique_check, &data, DATA_("Bone"), '.', name, sizeof(bone->name));
}

/* ************************************************** */
/* Bone Renaming - API */

static bool bone_unique_check(void *arg, const char *name)
{
	return BKE_armature_find_bone_name((bArmature *)arg, name) != NULL;
}

static void unique_bone_name(bArmature *arm, char *name)
{
	BLI_uniquename_cb(bone_unique_check, (void *)arm, DATA_("Bone"), '.', name, sizeof(((Bone *)NULL)->name));
}

/* helper call for armature_bone_rename */
static void constraint_bone_name_fix(Object *ob, ListBase *conlist, const char *oldname, const char *newname)
{
	bConstraint *curcon;
	bConstraintTarget *ct;
	
	for (curcon = conlist->first; curcon; curcon = curcon->next) {
		bConstraintTypeInfo *cti = BKE_constraint_typeinfo_get(curcon);
		ListBase targets = {NULL, NULL};
		
		/* constraint targets */
		if (cti && cti->get_constraint_targets) {
			cti->get_constraint_targets(curcon, &targets);
			
			for (ct = targets.first; ct; ct = ct->next) {
				if (ct->tar == ob) {
					if (STREQ(ct->subtarget, oldname)) {
						BLI_strncpy(ct->subtarget, newname, MAXBONENAME);
					}
				}
			}
			
			if (cti->flush_constraint_targets)
				cti->flush_constraint_targets(curcon, &targets, 0);
		}
		
		/* action constraints */
		if (curcon->type == CONSTRAINT_TYPE_ACTION) {
			bActionConstraint *actcon = (bActionConstraint *)curcon->data;
			BKE_action_fix_paths_rename(&ob->id, actcon->act, "pose.bones", oldname, newname, 0, 0, 1);
		}
	}
}

/* called by UI for renaming a bone */
/* warning: make sure the original bone was not renamed yet! */
/* seems messy, but thats what you get with not using pointers but channel names :) */
void ED_armature_bone_rename(bArmature *arm, const char *oldnamep, const char *newnamep)
{
	Object *ob;
	char newname[MAXBONENAME];
	char oldname[MAXBONENAME];
	
	/* names better differ! */
	if (!STREQLEN(oldnamep, newnamep, MAXBONENAME)) {
		
		/* we alter newname string... so make copy */
		BLI_strncpy(newname, newnamep, MAXBONENAME);
		/* we use oldname for search... so make copy */
		BLI_strncpy(oldname, oldnamep, MAXBONENAME);
		
		/* now check if we're in editmode, we need to find the unique name */
		if (arm->edbo) {
			EditBone *eBone = ED_armature_bone_find_name(arm->edbo, oldname);
			
			if (eBone) {
				unique_editbone_name(arm->edbo, newname, NULL);
				BLI_strncpy(eBone->name, newname, MAXBONENAME);
			}
			else {
				return;
			}
		}
		else {
			Bone *bone = BKE_armature_find_bone_name(arm, oldname);
			
			if (bone) {
				unique_bone_name(arm, newname);
				BLI_strncpy(bone->name, newname, MAXBONENAME);
			}
			else {
				return;
			}
		}
		
		/* do entire dbase - objects */
		for (ob = G.main->object.first; ob; ob = ob->id.next) {
			ModifierData *md;
			
			/* we have the object using the armature */
			if (arm == ob->data) {
				Object *cob;
				
				/* Rename the pose channel, if it exists */
				if (ob->pose) {
					bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, oldname);
					if (pchan) {
						GHash *gh = ob->pose->chanhash;

						/* remove the old hash entry, and replace with the new name */
						if (gh) {
							BLI_assert(BLI_ghash_haskey(gh, pchan->name));
							BLI_ghash_remove(gh, pchan->name, NULL, NULL);
						}

						BLI_strncpy(pchan->name, newname, MAXBONENAME);

						if (gh) {
							BLI_ghash_insert(gh, pchan->name, pchan);
						}
					}

					BLI_assert(BKE_pose_channels_is_valid(ob->pose) == true);
				}
				
				/* Update any object constraints to use the new bone name */
				for (cob = G.main->object.first; cob; cob = cob->id.next) {
					if (cob->constraints.first)
						constraint_bone_name_fix(ob, &cob->constraints, oldname, newname);
					if (cob->pose) {
						bPoseChannel *pchan;
						for (pchan = cob->pose->chanbase.first; pchan; pchan = pchan->next) {
							constraint_bone_name_fix(ob, &pchan->constraints, oldname, newname);
						}
					}
				}
			}
			
			/* See if an object is parented to this armature */
			if (ob->parent && (ob->parent->data == arm)) {
				if (ob->partype == PARBONE) {
					/* bone name in object */
					if (STREQ(ob->parsubstr, oldname))
						BLI_strncpy(ob->parsubstr, newname, MAXBONENAME);
				}
			}
			
			if (modifiers_usesArmature(ob, arm)) {
				bDeformGroup *dg = defgroup_find_name(ob, oldname);
				if (dg) {
					BLI_strncpy(dg->name, newname, MAXBONENAME);
				}
			}
			
			/* fix modifiers that might be using this name */
			for (md = ob->modifiers.first; md; md = md->next) {
				switch (md->type) {
					case eModifierType_Hook:
					{
						HookModifierData *hmd = (HookModifierData *)md;

						if (hmd->object && (hmd->object->data == arm)) {
							if (STREQ(hmd->subtarget, oldname))
								BLI_strncpy(hmd->subtarget, newname, MAXBONENAME);
						}
						break;
					}
					case eModifierType_UVWarp:
					{
						UVWarpModifierData *umd = (UVWarpModifierData *)md;

						if (umd->object_src && (umd->object_src->data == arm)) {
							if (STREQ(umd->bone_src, oldname))
								BLI_strncpy(umd->bone_src, newname, MAXBONENAME);
						}
						if (umd->object_dst && (umd->object_dst->data == arm)) {
							if (STREQ(umd->bone_dst, oldname))
								BLI_strncpy(umd->bone_dst, newname, MAXBONENAME);
						}
						break;
					}
					default:
						break;
				}
			}
		}
		
		/* Fix all animdata that may refer to this bone - we can't just do the ones attached to objects, since
		 * other ID-blocks may have drivers referring to this bone [#29822]
		 */
		// XXX: the ID here is for armatures, but most bone drivers are actually on the object instead...
		{
			
			BKE_all_animdata_fix_paths_rename(&arm->id, "pose.bones", oldname, newname);
		}
		
		/* correct view locking */
		{
			bScreen *screen;
			for (screen = G.main->screen.first; screen; screen = screen->id.next) {
				ScrArea *sa;
				/* add regions */
				for (sa = screen->areabase.first; sa; sa = sa->next) {
					SpaceLink *sl;
					for (sl = sa->spacedata.first; sl; sl = sl->next) {
						if (sl->spacetype == SPACE_VIEW3D) {
							View3D *v3d = (View3D *)sl;
							if (v3d->ob_centre && v3d->ob_centre->data == arm) {
								if (STREQ(v3d->ob_centre_bone, oldname)) {
									BLI_strncpy(v3d->ob_centre_bone, newname, MAXBONENAME);
								}
							}
						}
					}
				}
			}
		}
	}
}

/* ************************************************** */
/* Bone Renaming - EditMode */

static int armature_flip_names_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_edit_object(C);
	bArmature *arm;
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) 
		return OPERATOR_CANCELLED;
	arm = ob->data;
	
	/* loop through selected bones, auto-naming them */
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones)
	{
		char name_flip[MAXBONENAME];
		BKE_deform_flip_side_name(name_flip, ebone->name, true);
		ED_armature_bone_rename(arm, ebone->name, name_flip);
	}
	CTX_DATA_END;
	
	/* since we renamed stuff... */
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	/* copied from #rna_Bone_update_renamed */
	/* redraw view */
	WM_event_add_notifier(C, NC_GEOM | ND_DATA, ob->data);

	/* update animation channels */
	WM_event_add_notifier(C, NC_ANIMATION | ND_ANIMCHAN, ob->data);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_flip_names(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Flip Names";
	ot->idname = "ARMATURE_OT_flip_names";
	ot->description = "Flips (and corrects) the axis suffixes of the names of selected bones";
	
	/* api callbacks */
	ot->exec = armature_flip_names_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}


static int armature_autoside_names_exec(bContext *C, wmOperator *op)
{
	Object *ob = CTX_data_edit_object(C);
	bArmature *arm;
	char newname[MAXBONENAME];
	short axis = RNA_enum_get(op->ptr, "type");
	
	/* paranoia checks */
	if (ELEM(NULL, ob, ob->pose)) 
		return OPERATOR_CANCELLED;
	arm = ob->data;
	
	/* loop through selected bones, auto-naming them */
	CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones)
	{
		BLI_strncpy(newname, ebone->name, sizeof(newname));
		if (bone_autoside_name(newname, 1, axis, ebone->head[axis], ebone->tail[axis]))
			ED_armature_bone_rename(arm, ebone->name, newname);
	}
	CTX_DATA_END;
	
	/* since we renamed stuff... */
	DAG_id_tag_update(&ob->id, OB_RECALC_DATA);

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_autoside_names(wmOperatorType *ot)
{
	static EnumPropertyItem axis_items[] = {
		{0, "XAXIS", 0, "X-Axis", "Left/Right"},
		{1, "YAXIS", 0, "Y-Axis", "Front/Back"},
		{2, "ZAXIS", 0, "Z-Axis", "Top/Bottom"},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name = "AutoName by Axis";
	ot->idname = "ARMATURE_OT_autoside_names";
	ot->description = "Automatically renames the selected bones according to which side of the target axis they fall on";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = armature_autoside_names_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* settings */
	ot->prop = RNA_def_enum(ot->srna, "type", axis_items, 0, "Axis", "Axis tag names with");
}

