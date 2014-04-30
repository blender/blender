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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/screen/screen_context.c
 *  \ingroup edscr
 */


#include <stdlib.h>
#include <string.h>

#include "DNA_object_types.h"
#include "DNA_armature_types.h"
#include "DNA_sequence_types.h"
#include "DNA_scene_types.h"
#include "DNA_screen_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_utildefines.h"


#include "BKE_context.h"
#include "BKE_object.h"
#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_sequencer.h"

#include "RNA_access.h"

#include "ED_armature.h"

#include "WM_api.h"
#include "UI_interface.h"

#include "screen_intern.h"

const char *screen_context_dir[] = {
	"scene", "visible_objects", "visible_bases", "selectable_objects", "selectable_bases",
	"selected_objects", "selected_bases",
	"selected_editable_objects", "selected_editable_bases",
	"visible_bones", "editable_bones", "selected_bones", "selected_editable_bones",
	"visible_pose_bones", "selected_pose_bones", "active_bone", "active_pose_bone",
	"active_base", "active_object", "object", "edit_object",
	"sculpt_object", "vertex_paint_object", "weight_paint_object",
	"image_paint_object", "particle_edit_object",
	"sequences", "selected_sequences", "selected_editable_sequences", /* sequencer */
	"active_operator",
	NULL};

int ed_screen_context(const bContext *C, const char *member, bContextDataResult *result)
{
	bScreen *sc = CTX_wm_screen(C);
	Scene *scene = sc->scene;
	Base *base;
	unsigned int lay = scene->lay;

#if 0  /* Using the context breaks adding objects in the UI. Need to find out why - campbell */
	Object *obact = CTX_data_active_object(C);
	Object *obedit = CTX_data_edit_object(C);
	base = CTX_data_active_base(C);
#else
	Object *obedit = scene->obedit;
	Object *obact = OBACT;
	base = BASACT;
#endif

	if (CTX_data_dir(member)) {
		CTX_data_dir_set(result, screen_context_dir);
		return 1;
	}
	else if (CTX_data_equals(member, "scene")) {
		CTX_data_id_pointer_set(result, &scene->id);
		return 1;
	}
	else if (CTX_data_equals(member, "visible_objects") || CTX_data_equals(member, "visible_bases")) {
		int visible_objects = CTX_data_equals(member, "visible_objects");

		for (base = scene->base.first; base; base = base->next) {
			if (((base->object->restrictflag & OB_RESTRICT_VIEW) == 0) && (base->lay & scene->lay)) {
				if (visible_objects)
					CTX_data_id_list_add(result, &base->object->id);
				else
					CTX_data_list_add(result, &scene->id, &RNA_ObjectBase, base);
			}
		}
		CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
		return 1;
	}
	else if (CTX_data_equals(member, "selectable_objects") || CTX_data_equals(member, "selectable_bases")) {
		int selectable_objects = CTX_data_equals(member, "selectable_objects");

		for (base = scene->base.first; base; base = base->next) {
			if (base->lay & lay) {
				if ((base->object->restrictflag & OB_RESTRICT_VIEW) == 0 && (base->object->restrictflag & OB_RESTRICT_SELECT) == 0) {
					if (selectable_objects)
						CTX_data_id_list_add(result, &base->object->id);
					else
						CTX_data_list_add(result, &scene->id, &RNA_ObjectBase, base);
				}
			}
		}
		CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
		return 1;
	}
	else if (CTX_data_equals(member, "selected_objects") || CTX_data_equals(member, "selected_bases")) {
		int selected_objects = CTX_data_equals(member, "selected_objects");

		for (base = scene->base.first; base; base = base->next) {
			if ((base->flag & SELECT) && (base->lay & scene->lay)) {
				if (selected_objects)
					CTX_data_id_list_add(result, &base->object->id);
				else
					CTX_data_list_add(result, &scene->id, &RNA_ObjectBase, base);
			}
		}
		CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
		return 1;
	}
	else if (CTX_data_equals(member, "selected_editable_objects") || CTX_data_equals(member, "selected_editable_bases")) {
		int selected_editable_objects = CTX_data_equals(member, "selected_editable_objects");

		for (base = scene->base.first; base; base = base->next) {
			if ((base->flag & SELECT) && (base->lay & scene->lay)) {
				if ((base->object->restrictflag & OB_RESTRICT_VIEW) == 0) {
					if (0 == BKE_object_is_libdata(base->object)) {
						if (selected_editable_objects)
							CTX_data_id_list_add(result, &base->object->id);
						else
							CTX_data_list_add(result, &scene->id, &RNA_ObjectBase, base);
					}
				}
			}
		}
		CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
		return 1;
	}
	else if (CTX_data_equals(member, "visible_bones") || CTX_data_equals(member, "editable_bones")) {
		bArmature *arm = (obedit && obedit->type == OB_ARMATURE) ? obedit->data : NULL;
		EditBone *ebone, *flipbone = NULL;
		int editable_bones = CTX_data_equals(member, "editable_bones");
		
		if (arm && arm->edbo) {
			/* Attention: X-Axis Mirroring is also handled here... */
			for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
				/* first and foremost, bone must be visible and selected */
				if (EBONE_VISIBLE(arm, ebone)) {
					/* Get 'x-axis mirror equivalent' bone if the X-Axis Mirroring option is enabled
					 * so that most users of this data don't need to explicitly check for it themselves.
					 * 
					 * We need to make sure that these mirrored copies are not selected, otherwise some
					 * bones will be operated on twice.
					 */
					if (arm->flag & ARM_MIRROR_EDIT)
						flipbone = ED_armature_bone_get_mirrored(arm->edbo, ebone);
					
					/* if we're filtering for editable too, use the check for that instead, as it has selection check too */
					if (editable_bones) {
						/* only selected + editable */
						if (EBONE_EDITABLE(ebone)) {
							CTX_data_list_add(result, &arm->id, &RNA_EditBone, ebone);
						
							if ((flipbone) && !(flipbone->flag & BONE_SELECTED))
								CTX_data_list_add(result, &arm->id, &RNA_EditBone, flipbone);
						}
					}
					else {
						/* only include bones if visible */
						CTX_data_list_add(result, &arm->id, &RNA_EditBone, ebone);
						
						if ((flipbone) && EBONE_VISIBLE(arm, flipbone) == 0)
							CTX_data_list_add(result, &arm->id, &RNA_EditBone, flipbone);
					}
				}
			}
			CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "selected_bones") || CTX_data_equals(member, "selected_editable_bones")) {
		bArmature *arm = (obedit && obedit->type == OB_ARMATURE) ? obedit->data : NULL;
		EditBone *ebone, *flipbone = NULL;
		int selected_editable_bones = CTX_data_equals(member, "selected_editable_bones");
		
		if (arm && arm->edbo) {
			/* Attention: X-Axis Mirroring is also handled here... */
			for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
				/* first and foremost, bone must be visible and selected */
				if (EBONE_VISIBLE(arm, ebone) && (ebone->flag & BONE_SELECTED)) {
					/* Get 'x-axis mirror equivalent' bone if the X-Axis Mirroring option is enabled
					 * so that most users of this data don't need to explicitly check for it themselves.
					 * 
					 * We need to make sure that these mirrored copies are not selected, otherwise some
					 * bones will be operated on twice.
					 */
					if (arm->flag & ARM_MIRROR_EDIT)
						flipbone = ED_armature_bone_get_mirrored(arm->edbo, ebone);
					
					/* if we're filtering for editable too, use the check for that instead, as it has selection check too */
					if (selected_editable_bones) {
						/* only selected + editable */
						if (EBONE_EDITABLE(ebone)) {
							CTX_data_list_add(result, &arm->id, &RNA_EditBone, ebone);
						
							if ((flipbone) && !(flipbone->flag & BONE_SELECTED))
								CTX_data_list_add(result, &arm->id, &RNA_EditBone, flipbone);
						}
					}
					else {
						/* only include bones if selected */
						CTX_data_list_add(result, &arm->id, &RNA_EditBone, ebone);
						
						if ((flipbone) && !(flipbone->flag & BONE_SELECTED))
							CTX_data_list_add(result, &arm->id, &RNA_EditBone, flipbone);
					}
				}
			}
			CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "visible_pose_bones")) {
		Object *obpose = BKE_object_pose_armature_get(obact);
		bArmature *arm = (obpose) ? obpose->data : NULL;
		bPoseChannel *pchan;
		
		if (obpose && obpose->pose && arm) {
			for (pchan = obpose->pose->chanbase.first; pchan; pchan = pchan->next) {
				/* ensure that PoseChannel is on visible layer and is not hidden in PoseMode */
				if (PBONE_VISIBLE(arm, pchan->bone)) {
					CTX_data_list_add(result, &obpose->id, &RNA_PoseBone, pchan);
				}
			}
			CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "selected_pose_bones")) {
		Object *obpose = BKE_object_pose_armature_get(obact);
		bArmature *arm = (obpose) ? obpose->data : NULL;
		bPoseChannel *pchan;
		
		if (obpose && obpose->pose && arm) {
			for (pchan = obpose->pose->chanbase.first; pchan; pchan = pchan->next) {
				/* ensure that PoseChannel is on visible layer and is not hidden in PoseMode */
				if (PBONE_VISIBLE(arm, pchan->bone)) {
					if (pchan->bone->flag & BONE_SELECTED)
						CTX_data_list_add(result, &obpose->id, &RNA_PoseBone, pchan);
				}
			}
			CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "active_bone")) {
		if (obact && obact->type == OB_ARMATURE) {
			bArmature *arm = obact->data;
			if (arm->edbo) {
				if (arm->act_edbone) {
					CTX_data_pointer_set(result, &arm->id, &RNA_EditBone, arm->act_edbone);
					return 1;
				}
			}
			else {
				if (arm->act_bone) {
					CTX_data_pointer_set(result, &arm->id, &RNA_Bone, arm->act_bone);
					return 1;
				}
			}
		}
	}
	else if (CTX_data_equals(member, "active_pose_bone")) {
		bPoseChannel *pchan;
		Object *obpose = BKE_object_pose_armature_get(obact);
		
		pchan = BKE_pose_channel_active(obpose);
		if (pchan) {
			CTX_data_pointer_set(result, &obpose->id, &RNA_PoseBone, pchan);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "active_base")) {
		if (base)
			CTX_data_pointer_set(result, &scene->id, &RNA_ObjectBase, base);

		return 1;
	}
	else if (CTX_data_equals(member, "active_object")) {
		if (obact)
			CTX_data_id_pointer_set(result, &obact->id);

		return 1;
	}
	else if (CTX_data_equals(member, "object")) {
		if (obact)
			CTX_data_id_pointer_set(result, &obact->id);

		return 1;
	}
	else if (CTX_data_equals(member, "edit_object")) {
		/* convenience for now, 1 object per scene in editmode */
		if (obedit)
			CTX_data_id_pointer_set(result, &obedit->id);
		
		return 1;
	}
	else if (CTX_data_equals(member, "sculpt_object")) {
		if (obact && (obact->mode & OB_MODE_SCULPT))
			CTX_data_id_pointer_set(result, &obact->id);

		return 1;
	}
	else if (CTX_data_equals(member, "vertex_paint_object")) {
		if (obact && (obact->mode & OB_MODE_VERTEX_PAINT))
			CTX_data_id_pointer_set(result, &obact->id);

		return 1;
	}
	else if (CTX_data_equals(member, "weight_paint_object")) {
		if (obact && (obact->mode & OB_MODE_WEIGHT_PAINT))
			CTX_data_id_pointer_set(result, &obact->id);

		return 1;
	}
	else if (CTX_data_equals(member, "image_paint_object")) {
		if (obact && (obact->mode & OB_MODE_TEXTURE_PAINT))
			CTX_data_id_pointer_set(result, &obact->id);

		return 1;
	}
	else if (CTX_data_equals(member, "particle_edit_object")) {
		if (obact && (obact->mode & OB_MODE_PARTICLE_EDIT))
			CTX_data_id_pointer_set(result, &obact->id);

		return 1;
	}
	else if (CTX_data_equals(member, "sequences")) {
		Editing *ed = BKE_sequencer_editing_get(scene, false);
		if (ed) {
			Sequence *seq;
			for (seq = ed->seqbasep->first; seq; seq = seq->next) {
				CTX_data_list_add(result, &scene->id, &RNA_Sequence, seq);
			}
			CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "selected_sequences")) {
		Editing *ed = BKE_sequencer_editing_get(scene, false);
		if (ed) {
			Sequence *seq;
			for (seq = ed->seqbasep->first; seq; seq = seq->next) {
				if (seq->flag & SELECT) {
					CTX_data_list_add(result, &scene->id, &RNA_Sequence, seq);
				}
			}
			CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "selected_editable_sequences")) {
		Editing *ed = BKE_sequencer_editing_get(scene, false);
		if (ed) {
			Sequence *seq;
			for (seq = ed->seqbasep->first; seq; seq = seq->next) {
				if (seq->flag & SELECT && !(seq->flag & SEQ_LOCK)) {
					CTX_data_list_add(result, &scene->id, &RNA_Sequence, seq);
				}
			}
			CTX_data_type_set(result, CTX_DATA_TYPE_COLLECTION);
			return 1;
		}
	}
	else if (CTX_data_equals(member, "active_operator")) {
		wmOperator *op = NULL;

		SpaceFile *sfile = CTX_wm_space_file(C);
		if (sfile) {
			op = sfile->op;
		}
		else if ((op = uiContextActiveOperator(C))) {
			/* do nothign */
		}
		else {
			/* note, this checks poll, could be a problem, but this also
			 * happens for the toolbar */
			op = WM_operator_last_redo(C);
		}
		/* TODO, get the operator from popup's */

		if (op && op->ptr) {
			CTX_data_pointer_set(result, NULL, &RNA_Operator, op);
			return 1;
		}
	}
	else {
		return 0; /* not found */
	}

	return -1; /* found but not available */
}

