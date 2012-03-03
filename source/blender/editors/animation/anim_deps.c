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
#include "BKE_context.h"
#include "BKE_depsgraph.h"
#include "BKE_global.h"
#include "BKE_node.h"
#include "BKE_sequencer.h"
#include "BKE_utildefines.h"

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

	id= ale->id;
	if (!id)
		return;
	
	/* tag AnimData for refresh so that other views will update in realtime with these changes */
	adt= BKE_animdata_from_id(id);
	if (adt)
		adt->recalc |= ADT_RECALC_ANIM;

	/* update data */
	fcu= (ale->datatype == ALE_FCURVE)? ale->key_data: NULL;
		
	if (fcu && fcu->rna_path) {
		/* if we have an fcurve, call the update for the property we
		   are editing, this is then expected to do the proper redraws
		   and depsgraph updates  */
		PointerRNA id_ptr, ptr;
		PropertyRNA *prop;
		
		RNA_id_pointer_create(id, &id_ptr);
			
		if(RNA_path_resolve(&id_ptr, fcu->rna_path, &ptr, &prop))
			RNA_property_update_main(G.main, scene, &ptr, prop);
	}
	else {
		/* in other case we do standard depsgaph update, ideally
		   we'd be calling property update functions here too ... */
		DAG_id_tag_update(id, OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME); // XXX or do we want something more restrictive?
	}
}

/* tags the given ID block for refreshes (if applicable) due to 
 * Animation Editor editing */
void ANIM_id_update(Scene *UNUSED(scene), ID *id)
{
	if (id) {
		AnimData *adt= BKE_animdata_from_id(id);
		
		/* tag AnimData for refresh so that other views will update in realtime with these changes */
		if (adt)
			adt->recalc |= ADT_RECALC_ANIM;
			
		/* set recalc flags */
		DAG_id_tag_update(id, OB_RECALC_OB|OB_RECALC_DATA|OB_RECALC_TIME); // XXX or do we want something more restrictive?
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
static void animchan_sync_group (bAnimContext *UNUSED(ac), bAnimListElem *ale)
{
	bActionGroup *agrp= (bActionGroup *)ale->data;
	ID *owner_id= ale->id;
	
	/* major priority is selection status
	 * so we need both a group and an owner
	 */
	if (ELEM(NULL, agrp, owner_id))
		return;
		
	/* for standard Objects, check if group is the name of some bone */
	if (GS(owner_id->name) == ID_OB) {
		Object *ob= (Object *)owner_id;
		
		/* check if there are bones, and whether the name matches any 
		 * NOTE: this feature will only really work if groups by default contain the F-Curves for a single bone
		 */
		if (ob->pose) {
			bPoseChannel *pchan= get_pose_channel(ob->pose, agrp->name);
			
			/* if one matches, sync the selection status */
			if (pchan) {
				if (pchan->bone && pchan->bone->flag & BONE_SELECTED)
					agrp->flag |= AGRP_SELECTED;
				else
					agrp->flag &= ~AGRP_SELECTED;
			}
		}
	}
}
 
/* perform syncing updates for F-Curves */
static void animchan_sync_fcurve (bAnimContext *UNUSED(ac), bAnimListElem *ale)
{
	FCurve *fcu= (FCurve *)ale->data;
	ID *owner_id= ale->id;
	
	/* major priority is selection status, so refer to the checks done in anim_filter.c 
	 * skip_fcurve_selected_data() for reference about what's going on here...
	 */
	if (ELEM3(NULL, fcu, fcu->rna_path, owner_id))
		return;
		
	if (GS(owner_id->name) == ID_OB) {
		Object *ob= (Object *)owner_id;
		
		/* only affect if F-Curve involves pose.bones */
		if ((fcu->rna_path) && strstr(fcu->rna_path, "pose.bones")) {
			bPoseChannel *pchan;
			char *bone_name;
			
			/* get bone-name, and check if this bone is selected */
			bone_name= BLI_getQuotedStr(fcu->rna_path, "pose.bones[");
			pchan= get_pose_channel(ob->pose, bone_name);
			if (bone_name) MEM_freeN(bone_name);
			
			/* F-Curve selection depends on whether the bone is selected */
			if ((pchan) && (pchan->bone)) {
				if (pchan->bone->flag & BONE_SELECTED)
					fcu->flag |= FCURVE_SELECTED;
				else
					fcu->flag &= ~FCURVE_SELECTED;
			}
		}
	}
	else if (GS(owner_id->name) == ID_SCE) {
		Scene *scene = (Scene *)owner_id;
		
		/* only affect if F-Curve involves sequence_editor.sequences */
		if ((fcu->rna_path) && strstr(fcu->rna_path, "sequences_all")) {
			Editing *ed= seq_give_editing(scene, FALSE);
			Sequence *seq;
			char *seq_name;
			
			/* get strip name, and check if this strip is selected */
			seq_name= BLI_getQuotedStr(fcu->rna_path, "sequences_all[");
			seq = get_seq_by_name(ed->seqbasep, seq_name, FALSE);
			if (seq_name) MEM_freeN(seq_name);
			
			/* can only add this F-Curve if it is selected */
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
			node_name= BLI_getQuotedStr(fcu->rna_path, "nodes[");
			node = nodeFindNodebyName(ntree, node_name);
			if (node_name) MEM_freeN(node_name);
			
			/* can only add this F-Curve if it is selected */
			if (node) {
				if (node->flag & NODE_SELECT)
					fcu->flag |= FCURVE_SELECTED;
				else
					fcu->flag &= ~FCURVE_SELECTED;
			}
		}
	}
}

/* ---------------- */
 
/* Main call to be exported to animation editors */
void ANIM_sync_animchannels_to_data (const bContext *C)
{
	bAnimContext ac;
	ListBase anim_data = {NULL, NULL};
	bAnimListElem *ale;
	int filter;
	
	/* get animation context info for filtering the channels */
	// TODO: check on whether we need to set the area specially instead, since active area might not be ok?
	if (ANIM_animdata_get_context(C, &ac) == 0)
		return;
	
	/* filter data */
		/* NOTE: we want all channels, since we want to be able to set selection status on some of them even when collapsed */
	filter= ANIMFILTER_DATA_VISIBLE|ANIMFILTER_LIST_CHANNELS;
	ANIM_animdata_filter(&ac, &anim_data, filter, ac.data, ac.datatype);
	
	/* flush settings as appropriate depending on the types of the channels */
	for (ale= anim_data.first; ale; ale= ale->next) {
		switch (ale->type) {
			case ANIMTYPE_GROUP:
				animchan_sync_group(&ac, ale);
				break;
			
			case ANIMTYPE_FCURVE:
				animchan_sync_fcurve(&ac, ale);
				break;
		}
	}
	
	BLI_freelistN(&anim_data);
}
