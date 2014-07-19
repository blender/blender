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
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/animation/anim_deps.c
 *  \ingroup edanimation
 */


#include <string.h>

#include "MEM_guardedalloc.h"

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_node_types.h"
#include "DNA_scene_types.h"
#include "DNA_sequence_types.h"

#include "BLI_blenlib.h"
#include "BLI_utildefines.h"

#include "BKE_animsys.h"
#include "BKE_action.h"
#include "BKE_fcurve.h"
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_node.h"
#include "BKE_sequencer.h"

#include "RNA_access.h"

#include "ED_anim_api.h"

/* **************************** depsgraph tagging ******************************** */

/* tags the given anim list element for refreshes (if applicable)
 * due to Animation Editor editing 
 */
void ANIM_list_elem_update(Scene *scene, bAnimListElem *ale)
{
	ID *id;
	FCurve *fcu;
	AnimData *adt;

	id = ale->id;
	if (!id)
		return;
	
	/* tag AnimData for refresh so that other views will update in realtime with these changes */
	adt = BKE_animdata_from_id(id);
	if (adt)
		adt->recalc |= ADT_RECALC_ANIM;

	/* update data */
	fcu = (ale->datatype == ALE_FCURVE) ? ale->key_data : NULL;
		
	if (fcu && fcu->rna_path) {
		/* if we have an fcurve, call the update for the property we
		 * are editing, this is then expected to do the proper redraws
		 * and depsgraph updates  */
		PointerRNA id_ptr, ptr;
		PropertyRNA *prop;
		
		RNA_id_pointer_create(id, &id_ptr);
			
		if (RNA_path_resolve_property(&id_ptr, fcu->rna_path, &ptr, &prop))
			RNA_property_update_main(G.main, scene, &ptr, prop);
	}
	else {
		/* in other case we do standard depsgaph update, ideally
		 * we'd be calling property update functions here too ... */
		DAG_id_tag_update(id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME); // XXX or do we want something more restrictive?
	}
}

/* tags the given ID block for refreshes (if applicable) due to 
 * Animation Editor editing */
void ANIM_id_update(Scene *UNUSED(scene), ID *id)
{
	if (id) {
		AnimData *adt = BKE_animdata_from_id(id);
		
		/* tag AnimData for refresh so that other views will update in realtime with these changes */
		if (adt)
			adt->recalc |= ADT_RECALC_ANIM;
			
		/* set recalc flags */
		DAG_id_tag_update(id, OB_RECALC_OB | OB_RECALC_DATA | OB_RECALC_TIME); // XXX or do we want something more restrictive?
	}
}

/* **************************** animation data <-> data syncing ******************************** */
/* This code here is used to synchronize the
 *	- selection (to find selected data easier)
 *	- ... (insert other relevant items here later) 
 * status in relevant Blender data with the status stored in animation channels.
 *
 * This should be called in the refresh() callbacks for various editors in 
 * response to appropriate notifiers.
 */

/* perform syncing updates for Action Groups */
static void animchan_sync_group(bAnimContext *ac, bAnimListElem *ale, bActionGroup **active_agrp)
{
	bActionGroup *agrp = (bActionGroup *)ale->data;
	ID *owner_id = ale->id;
	
	/* major priority is selection status
	 * so we need both a group and an owner
	 */
	if (ELEM(NULL, agrp, owner_id))
		return;
		
	/* for standard Objects, check if group is the name of some bone */
	if (GS(owner_id->name) == ID_OB) {
		Object *ob = (Object *)owner_id;
		
		/* check if there are bones, and whether the name matches any 
		 * NOTE: this feature will only really work if groups by default contain the F-Curves for a single bone
		 */
		if (ob->pose) {
			bPoseChannel *pchan = BKE_pose_channel_find_name(ob->pose, agrp->name);
			bArmature *arm = ob->data;
			
			if (pchan) {
				bActionGroup *bgrp;
				
				/* if one matches, sync the selection status */
				if ((pchan->bone) && (pchan->bone->flag & BONE_SELECTED))
					agrp->flag |= AGRP_SELECTED;
				else
					agrp->flag &= ~AGRP_SELECTED;
					
				/* also sync active group status */
				if ((ob == ac->obact) && (pchan->bone == arm->act_bone)) {
					/* if no previous F-Curve has active flag, then we're the first and only one to get it */
					if (*active_agrp == NULL) {
						agrp->flag |= AGRP_ACTIVE;
						*active_agrp = agrp;
					}
					else {
						/* someone else has already taken it - set as not active */
						agrp->flag &= ~AGRP_ACTIVE;
					}
				}
				else {
					/* this can't possibly be active now */
					agrp->flag &= ~AGRP_ACTIVE;
				}
				
				/* sync group colors */
				bgrp = (bActionGroup *)BLI_findlink(&ob->pose->agroups, (pchan->agrp_index - 1));
				if (bgrp) {
					agrp->customCol = bgrp->customCol;
					action_group_colors_sync(agrp, bgrp);
				}
			}
		}
	}
}
 
/* perform syncing updates for F-Curves */
static void animchan_sync_fcurve(bAnimContext *ac, bAnimListElem *ale, FCurve **active_fcurve)
{
	FCurve *fcu = (FCurve *)ale->data;
	ID *owner_id = ale->id;
	
	/* major priority is selection status, so refer to the checks done in anim_filter.c 
	 * skip_fcurve_selected_data() for reference about what's going on here...
	 */
	if (ELEM(NULL, fcu, fcu->rna_path, owner_id))
		return;
	
	if (GS(owner_id->name) == ID_OB) {
		Object *ob = (Object *)owner_id;
		
		/* only affect if F-Curve involves pose.bones */
		if ((fcu->rna_path) && strstr(fcu->rna_path, "pose.bones")) {
			bArmature *arm = (bArmature *)ob->data;
			bPoseChannel *pchan;
			char *bone_name;
			
			/* get bone-name, and check if this bone is selected */
			bone_name = BLI_str_quoted_substrN(fcu->rna_path, "pose.bones[");
			pchan = BKE_pose_channel_find_name(ob->pose, bone_name);
			if (bone_name) MEM_freeN(bone_name);
			
			/* F-Curve selection depends on whether the bone is selected */
			if ((pchan) && (pchan->bone)) {
				/* F-Curve selection */
				if (pchan->bone->flag & BONE_SELECTED)
					fcu->flag |= FCURVE_SELECTED;
				else
					fcu->flag &= ~FCURVE_SELECTED;
					
				/* Active F-Curve - it should be the first one for this bone on the 
				 * active object to be considered as active
				 */
				if ((ob == ac->obact) && (pchan->bone == arm->act_bone)) {
					/* if no previous F-Curve has active flag, then we're the first and only one to get it */
					if (*active_fcurve == NULL) {
						fcu->flag |= FCURVE_ACTIVE;
						*active_fcurve = fcu;
					}
					else {
						/* someone else has already taken it - set as not active */
						fcu->flag &= ~FCURVE_ACTIVE;
					}
				}
				else {
					/* this can't possibly be active now */
					fcu->flag &= ~FCURVE_ACTIVE;
				}
			}
		}
	}
	else if (GS(owner_id->name) == ID_SCE) {
		Scene *scene = (Scene *)owner_id;
		
		/* only affect if F-Curve involves sequence_editor.sequences */
		if ((fcu->rna_path) && strstr(fcu->rna_path, "sequences_all")) {
			Editing *ed = BKE_sequencer_editing_get(scene, false);
			Sequence *seq;
			char *seq_name;
			
			/* get strip name, and check if this strip is selected */
			seq_name = BLI_str_quoted_substrN(fcu->rna_path, "sequences_all[");
			seq = BKE_sequence_get_by_name(ed->seqbasep, seq_name, false);
			if (seq_name) MEM_freeN(seq_name);
			
			/* update selection status */
			if (seq) {
				if (seq->flag & SELECT)
					fcu->flag |= FCURVE_SELECTED;
				else
					fcu->flag &= ~FCURVE_SELECTED;
			}
		}
	}
	else if (GS(owner_id->name) == ID_NT) {
		bNodeTree *ntree = (bNodeTree *)owner_id;
		
		/* check for selected nodes */
		if ((fcu->rna_path) && strstr(fcu->rna_path, "nodes")) {
			bNode *node;
			char *node_name;
			
			/* get strip name, and check if this strip is selected */
			node_name = BLI_str_quoted_substrN(fcu->rna_path, "nodes[");
			node = nodeFindNodebyName(ntree, node_name);
			if (node_name) MEM_freeN(node_name);
			
			/* update selection/active status */
			if (node) {
				/* update selection status */
				if (node->flag & NODE_SELECT)
					fcu->flag |= FCURVE_SELECTED;
				else
					fcu->flag &= ~FCURVE_SELECTED;
					
				/* update active status */
				/* XXX: this may interfere with setting bones as active if both exist at once;
				 * then again, if that's the case, production setups aren't likely to be animating
				 * nodes while working with bones?
				 */
				if (node->flag & NODE_ACTIVE) {
					if (*active_fcurve == NULL) {
						fcu->flag |= FCURVE_ACTIVE;
						*active_fcurve = fcu;
					}
					else {
						fcu->flag &= ~FCURVE_ACTIVE;
					}
				}
				else {
					fcu->flag &= ~FCURVE_ACTIVE;
				}
			}
		}
	}
}

/* ---------------- */
 
/* Main call to be exported to animation editors */
void ANIM_sync_animchannels_to_data(const bContext *C)
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	bActionGroup *active_agrp = NULL;
	FCurve *active_fcurve = NULL;
	
	/* get animation context info for filtering the channels */
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return;
	
	/* filter data */
	/* NOTE: we want all channels, since we want to be able to set selection status on some of them even when collapsed 
	 *       However, don't include duplicates so that selection statuses don't override each other
	 */
	filter = ANIMFILTER_DATA_VISIBLE | ANIMFILTER_LIST_CHANNELS | ANIMFILTER_NODUPLIS;
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* flush settings as appropriate depending on the types of the channels */
	for (ale = anim_data.first; ale; ale = ale->next) {
		switch (ale->type) {
			case ANIMTYPE_GROUP:
				animchan_sync_group(&ac, ale, &active_agrp);
				break;
			
			case ANIMTYPE_FCURVE:
				animchan_sync_fcurve(&ac, ale, &active_fcurve);
				break;
		}
	}
	
	ANIM_animdata_freelist(&anim_data);
}

void ANIM_animdata_update(bAnimContext *ac, ListBase *anim_data)
{
	bAnimListElem *ale;

	if (ELEM(ac->datatype, ANIMCONT_GPENCIL, ANIMCONT_MASK)) {
#ifdef DEBUG
		/* quiet assert */
		for (ale = anim_data->first; ale; ale = ale->next) {
			ale->update = 0;
		}
#endif
		return;
	}

	for (ale = anim_data->first; ale; ale = ale->next) {
		FCurve *fcu = ale->key_data;

		if (ale->update & ANIM_UPDATE_ORDER) {
			ale->update &= ~ANIM_UPDATE_ORDER;
			if (fcu)
				sort_time_fcurve(fcu);
		}

		if (ale->update & ANIM_UPDATE_HANDLES) {
			ale->update &= ~ANIM_UPDATE_HANDLES;
			if (fcu)
				calchandles_fcurve(fcu);
		}

		if (ale->update & ANIM_UPDATE_DEPS) {
			ale->update &= ~ANIM_UPDATE_DEPS;
			ANIM_list_elem_update(ac->scene, ale);
		}

		BLI_assert(ale->update == 0);
	}
}

void ANIM_animdata_freelist(ListBase *anim_data)
{
#ifndef NDEBUG
	bAnimListElem *ale, *ale_next;
	for (ale = anim_data->first; ale; ale = ale_next) {
		ale_next = ale->next;
		BLI_assert(ale->update == 0);
		MEM_freeN(ale);
	}
	BLI_listbase_clear(anim_data);
#else
	BLI_freelistN(anim_data);
#endif
}
