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
 * API's and Operators for selecting armature bones in EditMode
 */

/** \file blender/editors/armature/armature_select.c
 *  \ingroup edarmature
 */

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_deform.h"
#include "BKE_report.h"

#include "BIF_gl.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "armature_intern.h"

/* **************** PoseMode & EditMode Selection Buffer Queries *************************** */

/* only for opengl selection indices */
Bone *get_indexed_bone(Object *ob, int index)
{
	bPoseChannel *pchan;
	if (ob->pose == NULL) return NULL;
	index >>= 16;     // bone selection codes use left 2 bytes
	
	pchan = BLI_findlink(&ob->pose->chanbase, index);
	return pchan ? pchan->bone : NULL;
}

/* See if there are any selected bones in this buffer */
/* only bones from base are checked on */
void *get_bone_from_selectbuffer(Scene *scene, Base *base, unsigned int *buffer, short hits, short findunsel)
{
	Object *obedit = scene->obedit; // XXX get from context
	Bone *bone;
	EditBone *ebone;
	void *firstunSel = NULL, *firstSel = NULL, *data;
	unsigned int hitresult;
	short i, takeNext = 0, sel;
	
	for (i = 0; i < hits; i++) {
		hitresult = buffer[3 + (i * 4)];
		
		if (!(hitresult & BONESEL_NOSEL)) { // -1
			if (hitresult & BONESEL_ANY) {  // to avoid including objects in selection
				
				hitresult &= ~(BONESEL_ANY);
				/* Determine what the current bone is */
				if (obedit == NULL || base->object != obedit) {
					/* no singular posemode, so check for correct object */
					if (base->selcol == (hitresult & 0xFFFF)) {
						bone = get_indexed_bone(base->object, hitresult);
						
						if (findunsel)
							sel = (bone->flag & BONE_SELECTED);
						else
							sel = !(bone->flag & BONE_SELECTED);
						
						data = bone;
					}
					else {
						data = NULL;
						sel = 0;
					}
				}
				else {
					bArmature *arm = obedit->data;
					
					ebone = BLI_findlink(arm->edbo, hitresult);
					if (findunsel)
						sel = (ebone->flag & BONE_SELECTED);
					else
						sel = !(ebone->flag & BONE_SELECTED);
					
					data = ebone;
				}
				
				if (data) {
					if (sel) {
						if (!firstSel) firstSel = data;
						takeNext = 1;
					}
					else {
						if (!firstunSel)
							firstunSel = data;
						if (takeNext)
							return data;
					}
				}
			}
		}
	}
	
	if (firstunSel)
		return firstunSel;
	else 
		return firstSel;
}

/* used by posemode as well editmode */
/* only checks scene->basact! */
/* x and y are mouse coords (area space) */
void *get_nearest_bone(bContext *C, short findunsel, int x, int y)
{
	ViewContext vc;
	rcti rect;
	unsigned int buffer[MAXPICKBUF];
	short hits;
	
	view3d_set_viewcontext(C, &vc);
	
	// rect.xmin = ... mouseco!
	rect.xmin = rect.xmax = x;
	rect.ymin = rect.ymax = y;
	
	glInitNames();
	hits = view3d_opengl_select(&vc, buffer, MAXPICKBUF, &rect);

	if (hits > 0)
		return get_bone_from_selectbuffer(vc.scene, vc.scene->basact, buffer, hits, findunsel);
	
	return NULL;
}

/* **************** EditMode stuff ********************** */

/* called in space.c */
/* previously "selectconnected_armature" */
static int armature_select_linked_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	bArmature *arm;
	EditBone *bone, *curBone, *next;
	int extend = RNA_boolean_get(op->ptr, "extend");
	Object *obedit = CTX_data_edit_object(C);
	arm = obedit->data;

	view3d_operator_needs_opengl(C);

	if (extend)
		bone = get_nearest_bone(C, 0, event->mval[0], event->mval[1]);
	else
		bone = get_nearest_bone(C, 1, event->mval[0], event->mval[1]);

	if (!bone)
		return OPERATOR_CANCELLED;

	/* Select parents */
	for (curBone = bone; curBone; curBone = next) {
		if ((curBone->flag & BONE_UNSELECTABLE) == 0) {
			if (extend) {
				curBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
			}
			else {
				curBone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
			}
		}
		
		if (curBone->flag & BONE_CONNECTED)
			next = curBone->parent;
		else
			next = NULL;
	}

	/* Select children */
	while (bone) {
		for (curBone = arm->edbo->first; curBone; curBone = next) {
			next = curBone->next;
			if ((curBone->parent == bone) && (curBone->flag & BONE_UNSELECTABLE) == 0) {
				if (curBone->flag & BONE_CONNECTED) {
					if (extend)
						curBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
					else
						curBone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
					bone = curBone;
					break;
				}
				else {
					bone = NULL;
					break;
				}
			}
		}
		if (!curBone)
			bone = NULL;
	}
	
	ED_armature_sync_selection(arm->edbo);
	
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);
	
	return OPERATOR_FINISHED;
}

static int armature_select_linked_poll(bContext *C)
{
	return (ED_operator_view3d_active(C) && ED_operator_editarmature(C) );
}

void ARMATURE_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Connected";
	ot->idname = "ARMATURE_OT_select_linked";
	ot->description = "Select bones related to selected ones by parent/child relationships";
	
	/* api callbacks */
	ot->exec = NULL;
	ot->invoke = armature_select_linked_invoke;
	ot->poll = armature_select_linked_poll;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_boolean(ot->srna, "extend", FALSE, "Extend", "Extend selection instead of deselecting everything first");
}

/* does bones and points */
/* note that BONE ROOT only gets drawn for root bones (or without IK) */
static EditBone *get_nearest_editbonepoint(ViewContext *vc, const int mval[2],
                                           ListBase *edbo, int findunsel, int *selmask)
{
	bArmature *arm = (bArmature *)vc->obedit->data;
	EditBone *ebone_next_act = arm->act_edbone;

	EditBone *ebone;
	rcti rect;
	unsigned int buffer[MAXPICKBUF];
	unsigned int hitresult, besthitresult = BONESEL_NOSEL;
	int i, mindep = 4;
	short hits;

	glInitNames();
	
	/* find the bone after the current active bone, so as to bump up its chances in selection.
	 * this way overlapping bones will cycle selection state as with objects. */
	if (ebone_next_act &&
	    EBONE_VISIBLE(arm, ebone_next_act) &&
	    ebone_next_act->flag & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL))
	{
		ebone_next_act = ebone_next_act->next ? ebone_next_act->next : arm->edbo->first;
	}
	else {
		ebone_next_act = NULL;
	}

	rect.xmin = mval[0] - 5;
	rect.xmax = mval[0] + 5;
	rect.ymin = mval[1] - 5;
	rect.ymax = mval[1] + 5;

	hits = view3d_opengl_select(vc, buffer, MAXPICKBUF, &rect);
	if (hits == 0) {
		rect.xmin = mval[0] - 12;
		rect.xmax = mval[0] + 12;
		rect.ymin = mval[1] - 12;
		rect.ymax = mval[1] + 12;
		hits = view3d_opengl_select(vc, buffer, MAXPICKBUF, &rect);
	}
	/* See if there are any selected bones in this group */
	if (hits > 0) {
		
		if (hits == 1) {
			if (!(buffer[3] & BONESEL_NOSEL)) 
				besthitresult = buffer[3];
		}
		else {
			for (i = 0; i < hits; i++) {
				hitresult = buffer[3 + (i * 4)];
				if (!(hitresult & BONESEL_NOSEL)) {
					int dep;
					
					ebone = BLI_findlink(edbo, hitresult & ~BONESEL_ANY);
					
					/* clicks on bone points get advantage */
					if (hitresult & (BONESEL_ROOT | BONESEL_TIP)) {
						/* but also the unselected one */
						if (findunsel) {
							if ( (hitresult & BONESEL_ROOT) && (ebone->flag & BONE_ROOTSEL) == 0)
								dep = 1;
							else if ( (hitresult & BONESEL_TIP) && (ebone->flag & BONE_TIPSEL) == 0)
								dep = 1;
							else 
								dep = 2;
						}
						else dep = 2;
					}
					else {
						/* bone found */
						if (findunsel) {
							if ((ebone->flag & BONE_SELECTED) == 0)
								dep = 2;
							else
								dep = 3;
						}
						else dep = 3;
					}

					if (ebone == ebone_next_act) {
						dep -= 1;
					}

					if (dep < mindep) {
						mindep = dep;
						besthitresult = hitresult;
					}
				}
			}
		}
		
		if (!(besthitresult & BONESEL_NOSEL)) {
			
			ebone = BLI_findlink(edbo, besthitresult & ~BONESEL_ANY);
			
			*selmask = 0;
			if (besthitresult & BONESEL_ROOT)
				*selmask |= BONE_ROOTSEL;
			if (besthitresult & BONESEL_TIP)
				*selmask |= BONE_TIPSEL;
			if (besthitresult & BONESEL_BONE)
				*selmask |= BONE_SELECTED;
			return ebone;
		}
	}
	*selmask = 0;
	return NULL;
}



/* toggle==0: deselect
 * toggle==1: swap (based on test)
 * toggle==2: swap (no test), CURRENTLY UNUSED
 */
void ED_armature_deselect_all(Object *obedit, int toggle)
{
	bArmature *arm = obedit->data;
	EditBone    *eBone;
	int sel = 1;
	
	if (toggle == 1) {
		/* Determine if there are any selected bones
		 * and therefore whether we are selecting or deselecting */
		for (eBone = arm->edbo->first; eBone; eBone = eBone->next) {
			//			if (arm->layer & eBone->layer) {
			if (eBone->flag & (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL)) {
				sel = 0;
				break;
			}
			//			}
		}
	}
	else sel = toggle;
	
	/*	Set the flags */
	for (eBone = arm->edbo->first; eBone; eBone = eBone->next) {
		if (sel == 2) {
			/* invert selection of bone */
			if (EBONE_VISIBLE(arm, eBone)) {
				eBone->flag ^= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
				if (arm->act_edbone == eBone)
					arm->act_edbone = NULL;
			}
		}
		else if (sel == 1) {
			/* select bone */
			if (EBONE_VISIBLE(arm, eBone)) {
				eBone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
				if (eBone->parent)
					eBone->parent->flag |= (BONE_TIPSEL);
			}
		}
		else {
			/* deselect bone */
			eBone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
			if (arm->act_edbone == eBone)
				arm->act_edbone = NULL;
		}
	}
	
	ED_armature_sync_selection(arm->edbo);
}

void ED_armature_deselect_all_visible(Object *obedit)
{
	bArmature *arm = obedit->data;
	EditBone    *ebone;

	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		/* first and foremost, bone must be visible and selected */
		if (EBONE_SELECTABLE(arm, ebone)) {
			ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
		}
	}

	ED_armature_sync_selection(arm->edbo);
}

/* accounts for connected parents */
static int ebone_select_flag(EditBone *ebone)
{
	if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
		return ((ebone->parent->flag & BONE_TIPSEL) ? BONE_ROOTSEL : 0) | (ebone->flag & (BONE_SELECTED | BONE_TIPSEL));
	}
	else {
		return ebone->flag & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL);
	}
}

/* context: editmode armature in view3d */
int mouse_armature(bContext *C, const int mval[2], int extend, int deselect, int toggle)
{
	Object *obedit = CTX_data_edit_object(C);
	bArmature *arm = obedit->data;
	ViewContext vc;
	EditBone *nearBone = NULL;
	int selmask;

	view3d_set_viewcontext(C, &vc);
	
	BIF_sk_selectStroke(C, mval, extend);
	
	nearBone = get_nearest_editbonepoint(&vc, mval, arm->edbo, 1, &selmask);
	if (nearBone) {

		if (!extend && !deselect && !toggle)
			ED_armature_deselect_all(obedit, 0);
		
		/* by definition the non-root connected bones have no root point drawn,
		 * so a root selection needs to be delivered to the parent tip */
		
		if (selmask & BONE_SELECTED) {
			if (nearBone->parent && (nearBone->flag & BONE_CONNECTED)) {
				/* click in a chain */
				if (extend) {
					/* select this bone */
					nearBone->flag |= BONE_TIPSEL;
					nearBone->parent->flag |= BONE_TIPSEL;
				}
				else if (deselect) {
					/* deselect this bone */
					nearBone->flag &= ~(BONE_TIPSEL | BONE_SELECTED);
					/* only deselect parent tip if it is not selected */
					if (!(nearBone->parent->flag & BONE_SELECTED))
						nearBone->parent->flag &= ~BONE_TIPSEL;
				}
				else if (toggle) {
					/* hold shift inverts this bone's selection */
					if (nearBone->flag & BONE_SELECTED) {
						/* deselect this bone */
						nearBone->flag &= ~(BONE_TIPSEL | BONE_SELECTED);
						/* only deselect parent tip if it is not selected */
						if (!(nearBone->parent->flag & BONE_SELECTED))
							nearBone->parent->flag &= ~BONE_TIPSEL;
					}
					else {
						/* select this bone */
						nearBone->flag |= BONE_TIPSEL;
						nearBone->parent->flag |= BONE_TIPSEL;
					}
				}
				else {
					/* select this bone */
					nearBone->flag |= BONE_TIPSEL;
					nearBone->parent->flag |= BONE_TIPSEL;
				}
			}
			else {
				if (extend) {
					nearBone->flag |= (BONE_TIPSEL | BONE_ROOTSEL);
				}
				else if (deselect) {
					nearBone->flag &= ~(BONE_TIPSEL | BONE_ROOTSEL);
				}
				else if (toggle) {
					/* hold shift inverts this bone's selection */
					if (nearBone->flag & BONE_SELECTED)
						nearBone->flag &= ~(BONE_TIPSEL | BONE_ROOTSEL);
					else
						nearBone->flag |= (BONE_TIPSEL | BONE_ROOTSEL);
				}
				else
					nearBone->flag |= (BONE_TIPSEL | BONE_ROOTSEL);
			}
		}
		else {
			if (extend)
				nearBone->flag |= selmask;
			else if (deselect)
				nearBone->flag &= ~selmask;
			else if (toggle && (nearBone->flag & selmask))
				nearBone->flag &= ~selmask;
			else
				nearBone->flag |= selmask;
		}
		
		ED_armature_sync_selection(arm->edbo);
		
		if (nearBone) {
			/* then now check for active status */
			if (ebone_select_flag(nearBone)) {
				arm->act_edbone = nearBone;
			}
		}
		
		WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, vc.obedit);
		return 1;
	}

	return 0;
}


/* ****************  Selections  ******************/

static int armature_select_inverse_exec(bContext *C, wmOperator *UNUSED(op))
{
	/*	Set the flags */
	CTX_DATA_BEGIN(C, EditBone *, ebone, visible_bones)
	{
		/* ignore bone if selection can't change */
		if ((ebone->flag & BONE_UNSELECTABLE) == 0) {
			/* select bone */
			ebone->flag ^= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
		}
	}
	CTX_DATA_END;

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, NULL);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_inverse(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Inverse";
	ot->idname = "ARMATURE_OT_select_inverse";
	ot->description = "Flip the selection status of bones (selected -> unselected, unselected -> selected)";
	
	/* api callbacks */
	ot->exec = armature_select_inverse_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
}
static int armature_de_select_all_exec(bContext *C, wmOperator *op)
{
	int action = RNA_enum_get(op->ptr, "action");

	if (action == SEL_TOGGLE) {
		action = SEL_SELECT;
		/* Determine if there are any selected bones
		 * And therefore whether we are selecting or deselecting */
		if (CTX_DATA_COUNT(C, selected_bones) > 0)
			action = SEL_DESELECT;
	}
	
	/*	Set the flags */
	CTX_DATA_BEGIN(C, EditBone *, ebone, visible_bones)
	{
		/* ignore bone if selection can't change */
		if ((ebone->flag & BONE_UNSELECTABLE) == 0) {
			switch (action) {
				case SEL_SELECT:
					ebone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
					if (ebone->parent)
						ebone->parent->flag |= (BONE_TIPSEL);
					break;
				case SEL_DESELECT:
					ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
					break;
				case SEL_INVERT:
					if (ebone->flag & BONE_SELECTED) {
						ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
					}
					else {
						ebone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
						if (ebone->parent)
							ebone->parent->flag |= (BONE_TIPSEL);
					}
					break;
			}
		}
	}
	CTX_DATA_END;

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, NULL);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_all(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "(De)select All";
	ot->idname = "ARMATURE_OT_select_all";
	ot->description = "Toggle selection status of all bones";
	
	/* api callbacks */
	ot->exec = armature_de_select_all_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	WM_operator_properties_select_all(ot);
}

enum {
	SIMEDBONE_LENGTH = 1,
	SIMEDBONE_DIRECTION,
	SIMEDBONE_PREFIX,
	SIMEDBONE_SUFFIX,
	SIMEDBONE_LAYER
};

static EnumPropertyItem prop_similar_types[] = {
	{SIMEDBONE_LENGTH, "LENGTH", 0, "Length", ""},
	{SIMEDBONE_DIRECTION, "DIRECTION", 0, "Direction (Y axis)", ""},
	{SIMEDBONE_PREFIX, "PREFIX", 0, "Prefix", ""},
	{SIMEDBONE_SUFFIX, "SUFFIX", 0, "Suffix", ""},
	{SIMEDBONE_LAYER, "LAYER", 0, "Layer", ""},
	{0, NULL, 0, NULL, NULL}
};

/* could be used in more places */
static void ED_armature_edit_bone_select(EditBone *ebone)
{
	BLI_assert((ebone->flag & BONE_UNSELECTABLE) == 0);
	ebone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);

	if ((ebone->flag & BONE_CONNECTED) && (ebone->parent != NULL)) {
		ebone->parent->flag |= BONE_TIPSEL;
	}
}

static void select_similar_length(bArmature *arm, EditBone *ebone_act, const float thresh)
{
	EditBone *ebone;

	/* thresh is always relative to current length */
	const float len_min = ebone_act->length / (1.0f + thresh);
	const float len_max = ebone_act->length * (1.0f + thresh);

	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_SELECTABLE(arm, ebone)) {
			if ((ebone->length >= len_min) &&
			    (ebone->length <= len_max))
			{
				ED_armature_edit_bone_select(ebone);
			}
		}
	}
}

static void select_similar_direction(bArmature *arm, EditBone *ebone_act, const float thresh)
{
	EditBone *ebone;
	float dir_act[3];
	sub_v3_v3v3(dir_act, ebone_act->head, ebone_act->tail);

	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_SELECTABLE(arm, ebone)) {
			float dir[3];
			sub_v3_v3v3(dir, ebone->head, ebone->tail);

			if (angle_v3v3(dir_act, dir) / (float)M_PI < thresh) {
				ED_armature_edit_bone_select(ebone);
			}
		}
	}
}

static void select_similar_layer(bArmature *arm, EditBone *ebone_act)
{
	EditBone *ebone;

	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_SELECTABLE(arm, ebone)) {
			if (ebone->layer & ebone_act->layer) {
				ED_armature_edit_bone_select(ebone);
			}
		}
	}
}

static void select_similar_prefix(bArmature *arm, EditBone *ebone_act)
{
	EditBone *ebone;

	char body_tmp[MAX_VGROUP_NAME];
	char prefix_act[MAX_VGROUP_NAME];

	BKE_deform_split_prefix(ebone_act->name, prefix_act, body_tmp);

	if (prefix_act[0] == '\0')
		return;

	/* Find matches */
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_SELECTABLE(arm, ebone)) {
			char prefix_other[MAX_VGROUP_NAME];
			BKE_deform_split_prefix(ebone->name, prefix_other, body_tmp);
			if (!strcmp(prefix_act, prefix_other)) {
				ED_armature_edit_bone_select(ebone);
			}
		}
	}
}

static void select_similar_suffix(bArmature *arm, EditBone *ebone_act)
{
	EditBone *ebone;

	char body_tmp[MAX_VGROUP_NAME];
	char suffix_act[MAX_VGROUP_NAME];

	BKE_deform_split_suffix(ebone_act->name, body_tmp, suffix_act);

	if (suffix_act[0] == '\0')
		return;

	/* Find matches */
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_SELECTABLE(arm, ebone)) {
			char suffix_other[MAX_VGROUP_NAME];
			BKE_deform_split_suffix(ebone->name, body_tmp, suffix_other);
			if (!strcmp(suffix_act, suffix_other)) {
				ED_armature_edit_bone_select(ebone);
			}
		}
	}
}

static int armature_select_similar_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	bArmature *arm = obedit->data;
	EditBone *ebone_act = CTX_data_active_bone(C);

	/* Get props */
	int type = RNA_enum_get(op->ptr, "type");
	float thresh = RNA_float_get(op->ptr, "threshold");

	/* Check for active bone */
	if (ebone_act == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Operation requires an active bone");
		return OPERATOR_CANCELLED;
	}

	switch (type) {
		case SIMEDBONE_LENGTH:
			select_similar_length(arm, ebone_act, thresh);
			break;
		case SIMEDBONE_DIRECTION:
			select_similar_direction(arm, ebone_act, thresh);
			break;
		case SIMEDBONE_PREFIX:
			select_similar_prefix(arm, ebone_act);
			break;
		case SIMEDBONE_SUFFIX:
			select_similar_suffix(arm, ebone_act);
			break;
		case SIMEDBONE_LAYER:
			select_similar_layer(arm, ebone_act);
			break;
	}

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_similar(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Similar";
	ot->idname = "ARMATURE_OT_select_similar";

	/* callback functions */
	ot->invoke = WM_menu_invoke;
	ot->exec = armature_select_similar_exec;
	ot->poll = ED_operator_editarmature;
	ot->description = "Select similar bones by property types";

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_similar_types, 0, "Type", "");
	RNA_def_float(ot->srna, "threshold", 0.1f, 0.0f, 1.0f, "Threshold", "", 0.0f, 1.0f);
}

/* ********************* select hierarchy operator ************** */

/* Get the first available child of an editbone */
static EditBone *editbone_get_child(bArmature *arm, EditBone *pabone, short use_visibility)
{
	EditBone *curbone, *chbone = NULL;
	
	for (curbone = arm->edbo->first; curbone; curbone = curbone->next) {
		if (curbone->parent == pabone) {
			if (use_visibility) {
				if ((arm->layer & curbone->layer) && !(pabone->flag & BONE_HIDDEN_A)) {
					chbone = curbone;
				}
			}
			else
				chbone = curbone;
		}
	}
	
	return chbone;
}

static int armature_select_hierarchy_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Object *ob;
	bArmature *arm;
	EditBone *curbone, *pabone, *chbone;
	int direction = RNA_enum_get(op->ptr, "direction");
	int add_to_sel = RNA_boolean_get(op->ptr, "extend");
	
	ob = obedit;
	arm = (bArmature *)ob->data;
	
	for (curbone = arm->edbo->first; curbone; curbone = curbone->next) {
		/* only work on bone if it is visible and its selection can change */
		if (EBONE_SELECTABLE(arm, curbone)) {
			if (curbone == arm->act_edbone) {
				if (direction == BONE_SELECT_PARENT) {
					if (curbone->parent == NULL) continue;
					else pabone = curbone->parent;
					
					if (EBONE_VISIBLE(arm, pabone)) {
						pabone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
						arm->act_edbone = pabone;
						if (pabone->parent) pabone->parent->flag |= BONE_TIPSEL;
						
						if (!add_to_sel) curbone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
						break;
					}
					
				}
				else { // BONE_SELECT_CHILD
					chbone = editbone_get_child(arm, curbone, 1);
					if (chbone == NULL) continue;
					
					if (EBONE_SELECTABLE(arm, chbone)) {
						chbone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
						arm->act_edbone = chbone;
						
						if (!add_to_sel) {
							curbone->flag &= ~(BONE_SELECTED | BONE_ROOTSEL);
							if (curbone->parent) curbone->parent->flag &= ~BONE_TIPSEL;
						}
						break;
					}
				}
			}
		}
	}
	
	ED_armature_sync_selection(arm->edbo);
	
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_hierarchy(wmOperatorType *ot)
{
	static EnumPropertyItem direction_items[] = {
		{BONE_SELECT_PARENT, "PARENT", 0, "Select Parent", ""},
		{BONE_SELECT_CHILD, "CHILD", 0, "Select Child", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name = "Select Hierarchy";
	ot->idname = "ARMATURE_OT_select_hierarchy";
	ot->description = "Select immediate parent/children of selected bones";
	
	/* api callbacks */
	ot->exec = armature_select_hierarchy_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_enum(ot->srna, "direction", direction_items,
	             BONE_SELECT_PARENT, "Direction", "");
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}

