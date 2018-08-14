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

#include "MEM_guardedalloc.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"
#include "BLI_string_utils.h"

#include "BKE_context.h"
#include "BKE_action.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_layer.h"

#include "BIF_gl.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_view3d.h"

#include "DEG_depsgraph.h"

#include "armature_intern.h"

/* utility macros for storing a temp int in the bone (selection flag) */
#define EBONE_PREV_FLAG_GET(ebone) ((void)0, (ebone)->temp.i)
#define EBONE_PREV_FLAG_SET(ebone, val) ((ebone)->temp.i = val)

/* **************** PoseMode & EditMode Selection Buffer Queries *************************** */

Base *ED_armature_base_and_ebone_from_select_buffer(
        Base **bases, uint bases_len, int hit, EditBone **r_ebone)
{
	const uint hit_object = hit & 0xFFFF;
	Base *base = NULL;
	EditBone *ebone = NULL;
	/* TODO(campbell): optimize, eg: sort & binary search. */
	for (uint base_index = 0; base_index < bases_len; base_index++) {
		if (bases[base_index]->object->select_color == hit_object) {
			base = bases[base_index];
			break;
		}
	}
	if (base != NULL) {
		const uint hit_bone = (hit & ~BONESEL_ANY) >> 16;
		bArmature *arm = base->object->data;
		ebone = BLI_findlink(arm->edbo, hit_bone);
	}
	*r_ebone = ebone;
	return base;
}

Object *ED_armature_object_and_ebone_from_select_buffer(
        Object **objects, uint objects_len, int hit, EditBone **r_ebone)
{
	const uint hit_object = hit & 0xFFFF;
	Object *ob = NULL;
	EditBone *ebone = NULL;
	/* TODO(campbell): optimize, eg: sort & binary search. */
	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		if (objects[ob_index]->select_color == hit_object) {
			ob = objects[ob_index];
			break;
		}
	}
	if (ob != NULL) {
		const uint hit_bone = (hit & ~BONESEL_ANY) >> 16;
		bArmature *arm = ob->data;
		ebone = BLI_findlink(arm->edbo, hit_bone);
	}
	*r_ebone = ebone;
	return ob;
}

Base *ED_armature_base_and_bone_from_select_buffer(
        Base **bases, uint bases_len, int hit, Bone **r_bone)
{
	const uint hit_object = hit & 0xFFFF;
	Base *base = NULL;
	Bone *bone = NULL;
	/* TODO(campbell): optimize, eg: sort & binary search. */
	for (uint base_index = 0; base_index < bases_len; base_index++) {
		if (bases[base_index]->object->select_color == hit_object) {
			base = bases[base_index];
			break;
		}
	}
	if (base != NULL) {
		if (base->object->pose != NULL) {
			const uint hit_bone = (hit & ~BONESEL_ANY) >> 16;
			bPoseChannel *pchan = BLI_findlink(&base->object->pose->chanbase, hit_bone);;
			bone = pchan ? pchan->bone : NULL;
		}
	}
	*r_bone = bone;
	return base;
}

/* See if there are any selected bones in this buffer */
/* only bones from base are checked on */
void *get_bone_from_selectbuffer(
        Base **bases, uint bases_len, bool is_editmode, const unsigned int *buffer, short hits,
        bool findunsel, bool do_nearest, Base **r_base)
{
	Bone *bone;
	EditBone *ebone;
	void *firstunSel = NULL, *firstSel = NULL, *data;
	Base *firstunSel_base = NULL, *firstSel_base = NULL;
	unsigned int hitresult;
	short i;
	bool takeNext = false;
	int minsel = 0xffffffff, minunsel = 0xffffffff;

	for (i = 0; i < hits; i++) {
		hitresult = buffer[3 + (i * 4)];

		if (!(hitresult & BONESEL_NOSEL)) {
			if (hitresult & BONESEL_ANY) {  /* to avoid including objects in selection */
				Base *base = NULL;
				bool sel;

				hitresult &= ~(BONESEL_ANY);
				/* Determine what the current bone is */
				if (is_editmode == false) {
					base = ED_armature_base_and_bone_from_select_buffer(bases, bases_len, hitresult, &bone);
					if (bone != NULL) {
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
					base = ED_armature_base_and_ebone_from_select_buffer(bases, bases_len, hitresult, &ebone);
					if (findunsel)
						sel = (ebone->flag & BONE_SELECTED);
					else
						sel = !(ebone->flag & BONE_SELECTED);

					data = ebone;
				}

				if (data) {
					if (sel) {
						if (do_nearest) {
							if (minsel > buffer[4 * i + 1]) {
								firstSel = data;
								firstSel_base = base;
								minsel = buffer[4 * i + 1];
							}
						}
						else {
							if (!firstSel) {
								firstSel = data;
								firstSel_base = base;
							}
							takeNext = 1;
						}
					}
					else {
						if (do_nearest) {
							if (minunsel > buffer[4 * i + 1]) {
								firstunSel = data;
								firstunSel_base = base;
								minunsel = buffer[4 * i + 1];
							}
						}
						else {
							if (!firstunSel) {
								firstunSel = data;
								firstunSel_base = base;
							}
							if (takeNext) {
								*r_base = base;
								return data;
							}
						}
					}
				}
			}
		}
	}

	if (firstunSel) {
		*r_base = firstunSel_base;
		return firstunSel;
	}
	else {
		*r_base = firstSel_base;
		return firstSel;
	}
}

/* used by posemode as well editmode */
/* only checks scene->basact! */
/* x and y are mouse coords (area space) */
void *get_nearest_bone(
        bContext *C, const int xy[2], bool findunsel,
        Base **r_base)
{
	ViewContext vc;
	rcti rect;
	unsigned int buffer[MAXPICKBUF];
	short hits;

	ED_view3d_viewcontext_init(C, &vc);

	// rect.xmin = ... mouseco!
	rect.xmin = rect.xmax = xy[0];
	rect.ymin = rect.ymax = xy[1];

	hits = view3d_opengl_select(
	        &vc, buffer, MAXPICKBUF, &rect,
	        VIEW3D_SELECT_PICK_NEAREST, VIEW3D_SELECT_FILTER_NOP);

	*r_base = NULL;

	if (hits > 0) {
		uint bases_len = 0;
		Base **bases;

		if (vc.obedit != NULL) {
			bases = BKE_view_layer_array_from_bases_in_mode(
			        vc.view_layer, &bases_len, {
			            .object_mode = OB_MODE_EDIT});
		}
		else {
			bases = BKE_object_pose_base_array_get(vc.view_layer, &bases_len);
		}

		void *bone = get_bone_from_selectbuffer(
		        bases, bases_len, vc.obedit != NULL, buffer, hits, findunsel, true, r_base);

		MEM_freeN(bases);
		return bone;
	}
	return NULL;
}

/* **************** EditMode stuff ********************** */

/* called in space.c */
/* previously "selectconnected_armature" */
static int armature_select_linked_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	bArmature *arm;
	EditBone *bone, *curBone, *next;
	const bool extend = RNA_boolean_get(op->ptr, "extend");

	view3d_operator_needs_opengl(C);

	Base *base = NULL;
	bone = get_nearest_bone(C, event->mval, !extend, &base);

	if (!bone)
		return OPERATOR_CANCELLED;

	arm = base->object->data;

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

	ED_armature_edit_sync_selection(arm->edbo);

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, base->object);

	return OPERATOR_FINISHED;
}

static bool armature_select_linked_poll(bContext *C)
{
	return (ED_operator_view3d_active(C) && ED_operator_editarmature(C));
}

void ARMATURE_OT_select_linked(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Connected";
	ot->idname = "ARMATURE_OT_select_linked";
	ot->description = "Select bones related to selected ones by parent/child relationships";

	/* api callbacks */
	/* leave 'exec' unset */
	ot->invoke = armature_select_linked_invoke;
	ot->poll = armature_select_linked_poll;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend selection instead of deselecting everything first");
}

/* utility function for get_nearest_editbonepoint */
static int selectbuffer_ret_hits_12(unsigned int *UNUSED(buffer), const int hits12)
{
	return hits12;
}

static int selectbuffer_ret_hits_5(unsigned int *buffer, const int hits12, const int hits5)
{
	const int offs = 4 * hits12;
	memcpy(buffer, buffer + offs, 4 * hits5 * sizeof(unsigned int));
	return hits5;
}

/* does bones and points */
/* note that BONE ROOT only gets drawn for root bones (or without IK) */
static EditBone *get_nearest_editbonepoint(
        ViewContext *vc,
        bool findunsel, bool use_cycle,
        Base **r_base, int *r_selmask)
{
	uint buffer[MAXPICKBUF];
	struct {
		uint hitresult;
		Base *base;
		EditBone *ebone;
	} best = {
		.hitresult = BONESEL_NOSEL,
		.base = NULL,
		.ebone = NULL,
	};

	/* find the bone after the current active bone, so as to bump up its chances in selection.
	 * this way overlapping bones will cycle selection state as with objects. */
	EditBone *ebone_next_act = ((bArmature *)vc->obedit->data)->act_edbone;
	{
		bArmature *arm = (bArmature *)vc->obedit->data;
		if (ebone_next_act &&
		    EBONE_VISIBLE(arm, ebone_next_act) &&
		    ebone_next_act->flag & (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL))
		{
			ebone_next_act = ebone_next_act->next ? ebone_next_act->next : arm->edbo->first;
		}
		else {
			ebone_next_act = NULL;
		}
	}

	bool do_nearest = false;

	/* define if we use solid nearest select or not */
	if (use_cycle) {
		static int last_mval[2] = {-100, -100};

		if (vc->v3d->shading.type > OB_WIRE) {
			do_nearest = true;
			if (len_manhattan_v2v2_int(vc->mval, last_mval) < 3) {
				do_nearest = false;
			}
		}
		copy_v2_v2_int(last_mval, vc->mval);
	}
	else {
		if (vc->v3d->shading.type > OB_WIRE) {
			do_nearest = true;
		}
	}

	/* matching logic from 'mixed_bones_object_selectbuffer' */
	int hits = 0;

	/* we _must_ end cache before return, use 'goto cache_end' */
	view3d_opengl_select_cache_begin();

	{
		const int select_mode = (do_nearest ? VIEW3D_SELECT_PICK_NEAREST : VIEW3D_SELECT_PICK_ALL);
		const eV3DSelectObjectFilter select_filter = VIEW3D_SELECT_FILTER_NOP;

		rcti rect;
		BLI_rcti_init_pt_radius(&rect, vc->mval, 12);
		const int hits12 = view3d_opengl_select(vc, buffer, MAXPICKBUF, &rect, select_mode, select_filter);
		if (hits12 == 1) {
			hits = selectbuffer_ret_hits_12(buffer, hits12);
			goto cache_end;
		}
		else if (hits12 > 0) {
			int offs;

			offs = 4 * hits12;
			BLI_rcti_init_pt_radius(&rect, vc->mval, 5);
			const int hits5 = view3d_opengl_select(
			        vc, buffer + offs, MAXPICKBUF - offs, &rect,
			        select_mode, select_filter);

			if (hits5 == 1) {
				hits = selectbuffer_ret_hits_5(buffer, hits12, hits5);
				goto cache_end;
			}

			if      (hits5 > 0) { hits = selectbuffer_ret_hits_5(buffer,  hits12, hits5); goto cache_end; }
			else                { hits = selectbuffer_ret_hits_12(buffer, hits12); goto cache_end; }
		}
	}

cache_end:
	view3d_opengl_select_cache_end();

	uint bases_len;
	Base **bases = BKE_view_layer_array_from_bases_in_edit_mode_unique_data(vc->view_layer, &bases_len);

	/* See if there are any selected bones in this group */
	if (hits > 0) {
		if (hits == 1) {
			if (!(buffer[3] & BONESEL_NOSEL)) {
				best.hitresult = buffer[3];
				best.base = ED_armature_base_and_ebone_from_select_buffer(
				        bases, bases_len, best.hitresult, &best.ebone);
			}
		}
		else {
			int dep_min = 5;
			for (int i = 0; i < hits; i++) {
				const uint hitresult = buffer[3 + (i * 4)];
				if (!(hitresult & BONESEL_NOSEL)) {
					Base *base = NULL;
					EditBone *ebone;
					base = ED_armature_base_and_ebone_from_select_buffer(bases, bases_len, hitresult, &ebone);
					/* If this fails, selection code is setting the selection ID's incorrectly. */
					BLI_assert(base && ebone);

					int dep;
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
						else {
							dep = 1;
						}
					}
					else {
						/* bone found */
						if (findunsel) {
							if ((ebone->flag & BONE_SELECTED) == 0)
								dep = 3;
							else
								dep = 4;
						}
						else {
							dep = 3;
						}
					}

					if (ebone == ebone_next_act) {
						dep -= 1;
					}

					if (dep < dep_min) {
						dep_min = dep;
						best.hitresult = hitresult;
						best.base = base;
						best.ebone = ebone;
					}
				}
			}
		}

		if (!(best.hitresult & BONESEL_NOSEL)) {
			*r_base = best.base;

			*r_selmask = 0;
			if (best.hitresult & BONESEL_ROOT) {
				*r_selmask |= BONE_ROOTSEL;
			}
			if (best.hitresult & BONESEL_TIP) {
				*r_selmask |= BONE_TIPSEL;
			}
			if (best.hitresult & BONESEL_BONE) {
				*r_selmask |= BONE_SELECTED;
			}
			MEM_freeN(bases);
			return best.ebone;
		}
	}
	*r_selmask = 0;
	*r_base = NULL;
	MEM_freeN(bases);
	return NULL;
}

void ED_armature_edit_deselect_all(Object *obedit)
{
	bArmature *arm = obedit->data;
	EditBone *ebone;

	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
	}
}

void ED_armature_edit_deselect_all_visible(Object *obedit)
{
	bArmature *arm = obedit->data;
	EditBone    *ebone;

	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		/* first and foremost, bone must be visible and selected */
		if (EBONE_VISIBLE(arm, ebone)) {
			ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
		}
	}

	ED_armature_edit_sync_selection(arm->edbo);
}


void ED_armature_edit_deselect_all_multi(struct Object **objects, uint objects_len)
{
	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *obedit = objects[ob_index];
		ED_armature_edit_deselect_all(obedit);
	}
}

void ED_armature_edit_deselect_all_visible_multi(struct Object **objects, uint objects_len)
{
	for (uint ob_index = 0; ob_index < objects_len; ob_index++) {
		Object *obedit = objects[ob_index];
		ED_armature_edit_deselect_all_visible(obedit);
	}
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
bool ED_armature_edit_select_pick(bContext *C, const int mval[2], bool extend, bool deselect, bool toggle)
{
	ViewContext vc;
	EditBone *nearBone = NULL;
	int selmask;
	Base *basact = NULL;

	ED_view3d_viewcontext_init(C, &vc);
	vc.mval[0] = mval[0];
	vc.mval[1] = mval[1];

	nearBone = get_nearest_editbonepoint(&vc, true, true, &basact, &selmask);
	if (nearBone) {
		ED_view3d_viewcontext_init_object(&vc, basact->object);
		bArmature *arm = vc.obedit->data;

		if (!extend && !deselect && !toggle) {
			uint objects_len = 0;
			Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(vc.view_layer, &objects_len);
			ED_armature_edit_deselect_all_multi(objects, objects_len);
			MEM_freeN(objects);
		}

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

		ED_armature_edit_sync_selection(arm->edbo);

		if (nearBone) {
			/* then now check for active status */
			if (ebone_select_flag(nearBone)) {
				arm->act_edbone = nearBone;
			}

			if (vc.view_layer->basact != basact) {
				vc.view_layer->basact = basact;
				DEG_id_tag_update(&vc.scene->id, DEG_TAG_SELECT_UPDATE);
				WM_event_add_notifier(C, NC_SCENE | ND_OB_ACTIVE, vc.scene);
			}
		}

		WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, basact->object);
		return true;
	}

	return false;
}


/* ****************  Selections  ******************/

static int armature_de_select_all_exec(bContext *C, wmOperator *op)
{
	int action = RNA_enum_get(op->ptr, "action");

	if (action == SEL_TOGGLE) {
		/* Determine if there are any selected bones
		 * And therefore whether we are selecting or deselecting */
		action = SEL_SELECT;
		CTX_DATA_BEGIN(C, EditBone *, ebone, visible_bones)
		{
			if (ebone->flag & (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL)) {
				action = SEL_DESELECT;
				break;
			}
		}
		CTX_DATA_END;
	}

	/*	Set the flags */
	CTX_DATA_BEGIN(C, EditBone *, ebone, visible_bones)
	{
		/* ignore bone if selection can't change */
		switch (action) {
			case SEL_SELECT:
				if ((ebone->flag & BONE_UNSELECTABLE) == 0) {
					ebone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
					if (ebone->parent) {
						ebone->parent->flag |= (BONE_TIPSEL);
					}
				}
				break;
			case SEL_DESELECT:
				ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
				break;
			case SEL_INVERT:
				if (ebone->flag & BONE_SELECTED) {
					ebone->flag &= ~(BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
				}
				else {
					if ((ebone->flag & BONE_UNSELECTABLE) == 0) {
						ebone->flag |= (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL);
						if (ebone->parent) {
							ebone->parent->flag |= (BONE_TIPSEL);
						}
					}
				}
				break;
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

/**************** Select more/less **************/

static void armature_select_more(bArmature *arm, EditBone *ebone)
{
	if ((EBONE_PREV_FLAG_GET(ebone) & (BONE_ROOTSEL | BONE_TIPSEL)) != 0) {
		if (EBONE_SELECTABLE(arm, ebone)) {
			ED_armature_ebone_select_set(ebone, true);
		}
	}

	if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
		/* to parent */
		if ((EBONE_PREV_FLAG_GET(ebone) & BONE_ROOTSEL) != 0) {
			if (EBONE_SELECTABLE(arm, ebone->parent)) {
				ED_armature_ebone_selectflag_enable(ebone->parent, (BONE_SELECTED | BONE_ROOTSEL | BONE_TIPSEL));
			}
		}

		/* from parent (difference from select less) */
		if ((EBONE_PREV_FLAG_GET(ebone->parent) & BONE_TIPSEL) != 0) {
			if (EBONE_SELECTABLE(arm, ebone)) {
				ED_armature_ebone_selectflag_enable(ebone, (BONE_SELECTED | BONE_ROOTSEL));
			}
		}
	}
}

static void armature_select_less(bArmature *UNUSED(arm), EditBone *ebone)
{
	if ((EBONE_PREV_FLAG_GET(ebone) & (BONE_ROOTSEL | BONE_TIPSEL)) != (BONE_ROOTSEL | BONE_TIPSEL)) {
		ED_armature_ebone_select_set(ebone, false);
	}

	if (ebone->parent && (ebone->flag & BONE_CONNECTED)) {
		/* to parent */
		if ((EBONE_PREV_FLAG_GET(ebone) & BONE_SELECTED) == 0) {
			ED_armature_ebone_selectflag_disable(ebone->parent, (BONE_SELECTED | BONE_TIPSEL));
		}

		/* from parent (difference from select more) */
		if ((EBONE_PREV_FLAG_GET(ebone->parent) & BONE_SELECTED) == 0) {
			ED_armature_ebone_selectflag_disable(ebone, (BONE_SELECTED | BONE_ROOTSEL));
		}
	}
}

static void armature_select_more_less(Object *ob, bool more)
{
	bArmature *arm = (bArmature *)ob->data;
	EditBone *ebone;

	/* XXX, eventually we shouldn't need this - campbell */
	ED_armature_edit_sync_selection(arm->edbo);

	/* count bones & store selection state */
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		EBONE_PREV_FLAG_SET(ebone, ED_armature_ebone_selectflag_get(ebone));
	}

	/* do selection */
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_VISIBLE(arm, ebone)) {
			if (more) {
				armature_select_more(arm, ebone);
			}
			else {
				armature_select_less(arm, ebone);
			}
		}
	}

	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_VISIBLE(arm, ebone)) {
			if (more == false) {
				if (ebone->flag & BONE_SELECTED) {
					ED_armature_ebone_select_set(ebone, true);
				}
			}
		}
		ebone->temp.p = NULL;
	}

	ED_armature_edit_sync_selection(arm->edbo);
}

static int armature_de_select_more_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	armature_select_more_less(obedit, true);
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_more(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select More";
	ot->idname = "ARMATURE_OT_select_more";
	ot->description = "Select those bones connected to the initial selection";

	/* api callbacks */
	ot->exec = armature_de_select_more_exec;
	ot->poll = ED_operator_editarmature;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int armature_de_select_less_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	armature_select_more_less(obedit, false);
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_less(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Select Less";
	ot->idname = "ARMATURE_OT_select_less";
	ot->description = "Deselect those bones at the boundary of each selection region";

	/* api callbacks */
	ot->exec = armature_de_select_less_exec;
	ot->poll = ED_operator_editarmature;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

enum {
	SIMEDBONE_CHILDREN = 1,
	SIMEDBONE_CHILDREN_IMMEDIATE,
	SIMEDBONE_SIBLINGS,
	SIMEDBONE_LENGTH,
	SIMEDBONE_DIRECTION,
	SIMEDBONE_PREFIX,
	SIMEDBONE_SUFFIX,
	SIMEDBONE_LAYER,
	SIMEDBONE_GROUP,
	SIMEDBONE_SHAPE,
};

static const EnumPropertyItem prop_similar_types[] = {
	{SIMEDBONE_CHILDREN, "CHILDREN", 0, "Children", ""},
	{SIMEDBONE_CHILDREN_IMMEDIATE, "CHILDREN_IMMEDIATE", 0, "Immediate children", ""},
	{SIMEDBONE_SIBLINGS, "SIBLINGS", 0, "Siblings", ""},
	{SIMEDBONE_LENGTH, "LENGTH", 0, "Length", ""},
	{SIMEDBONE_DIRECTION, "DIRECTION", 0, "Direction (Y axis)", ""},
	{SIMEDBONE_PREFIX, "PREFIX", 0, "Prefix", ""},
	{SIMEDBONE_SUFFIX, "SUFFIX", 0, "Suffix", ""},
	{SIMEDBONE_LAYER, "LAYER", 0, "Layer", ""},
	{SIMEDBONE_GROUP, "GROUP", 0, "Group", ""},
	{SIMEDBONE_SHAPE, "SHAPE", 0, "Shape", ""},
	{0, NULL, 0, NULL, NULL}
};


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
				ED_armature_ebone_select_set(ebone, true);
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
				ED_armature_ebone_select_set(ebone, true);
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
				ED_armature_ebone_select_set(ebone, true);
			}
		}
	}
}

static void select_similar_prefix(bArmature *arm, EditBone *ebone_act)
{
	EditBone *ebone;

	char body_tmp[MAXBONENAME];
	char prefix_act[MAXBONENAME];

	BLI_string_split_prefix(ebone_act->name, prefix_act, body_tmp, sizeof(ebone_act->name));

	if (prefix_act[0] == '\0')
		return;

	/* Find matches */
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_SELECTABLE(arm, ebone)) {
			char prefix_other[MAXBONENAME];
			BLI_string_split_prefix(ebone->name, prefix_other, body_tmp, sizeof(ebone->name));
			if (STREQ(prefix_act, prefix_other)) {
				ED_armature_ebone_select_set(ebone, true);
			}
		}
	}
}

static void select_similar_suffix(bArmature *arm, EditBone *ebone_act)
{
	EditBone *ebone;

	char body_tmp[MAXBONENAME];
	char suffix_act[MAXBONENAME];

	BLI_string_split_suffix(ebone_act->name, body_tmp, suffix_act, sizeof(ebone_act->name));

	if (suffix_act[0] == '\0')
		return;

	/* Find matches */
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_SELECTABLE(arm, ebone)) {
			char suffix_other[MAXBONENAME];
			BLI_string_split_suffix(ebone->name, body_tmp, suffix_other, sizeof(ebone->name));
			if (STREQ(suffix_act, suffix_other)) {
				ED_armature_ebone_select_set(ebone, true);
			}
		}
	}
}

/** Use for matching any pose channel data. */
static void select_similar_data_pchan(
        bArmature *arm, Object *obj, EditBone *ebone_active,
        const size_t bytes_size, const int offset)
{
	const bPoseChannel *pchan_active = BKE_pose_channel_find_name(obj->pose, ebone_active->name);
	const char *data_active = (const char *)POINTER_OFFSET(pchan_active, offset);
	for (EditBone *ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_SELECTABLE(arm, ebone)) {
			const bPoseChannel *pchan = BKE_pose_channel_find_name(obj->pose, ebone->name);
			if (pchan) {
				const char *data_test = (const char *)POINTER_OFFSET(pchan, offset);
				if (memcmp(data_active, data_test, bytes_size) == 0) {
					ED_armature_ebone_select_set(ebone, true);
				}
			}
		}
	}
}

static void is_ancestor(EditBone *bone, EditBone *ancestor)
{
	if (bone->temp.ebone == ancestor || bone->temp.ebone == NULL)
		return;

	if (bone->temp.ebone->temp.ebone != NULL && bone->temp.ebone->temp.ebone != ancestor)
		is_ancestor(bone->temp.ebone, ancestor);

	bone->temp.ebone = bone->temp.ebone->temp.ebone;
}

static void select_similar_children(bArmature *arm, EditBone *ebone_act)
{
	EditBone *ebone_iter;

	for (ebone_iter = arm->edbo->first; ebone_iter; ebone_iter = ebone_iter->next) {
		ebone_iter->temp.ebone = ebone_iter->parent;
	}

	for (ebone_iter = arm->edbo->first; ebone_iter; ebone_iter = ebone_iter->next) {
		is_ancestor(ebone_iter, ebone_act);

		if (ebone_iter->temp.ebone == ebone_act && EBONE_SELECTABLE(arm, ebone_iter))
			ED_armature_ebone_select_set(ebone_iter, true);
	}
}

static void select_similar_children_immediate(bArmature *arm, EditBone *ebone_act)
{
	EditBone *ebone_iter;
	for (ebone_iter = arm->edbo->first; ebone_iter; ebone_iter = ebone_iter->next) {
		if (ebone_iter->parent == ebone_act && EBONE_SELECTABLE(arm, ebone_iter)) {
			ED_armature_ebone_select_set(ebone_iter, true);
		}
	}
}

static void select_similar_siblings(bArmature *arm, EditBone *ebone_act)
{
	EditBone *ebone_iter;

	if (ebone_act->parent == NULL)
		return;

	for (ebone_iter = arm->edbo->first; ebone_iter; ebone_iter = ebone_iter->next) {
		if (ebone_iter->parent == ebone_act->parent && EBONE_SELECTABLE(arm, ebone_iter)) {
			ED_armature_ebone_select_set(ebone_iter, true);
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

#define STRUCT_SIZE_AND_OFFSET(_struct, _member) \
	sizeof(((_struct *)NULL)->_member), offsetof(_struct, _member)

	switch (type) {
		case SIMEDBONE_CHILDREN:
			select_similar_children(arm, ebone_act);
			break;
		case SIMEDBONE_CHILDREN_IMMEDIATE:
			select_similar_children_immediate(arm, ebone_act);
			break;
		case SIMEDBONE_SIBLINGS:
			select_similar_siblings(arm, ebone_act);
			break;
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
		case SIMEDBONE_GROUP:
			select_similar_data_pchan(
			        arm, obedit, ebone_act,
			        STRUCT_SIZE_AND_OFFSET(bPoseChannel, agrp_index));
			break;
		case SIMEDBONE_SHAPE:
			select_similar_data_pchan(
			        arm, obedit, ebone_act,
			        STRUCT_SIZE_AND_OFFSET(bPoseChannel, custom));
			break;
	}

#undef STRUCT_SIZE_AND_OFFSET

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
	ot->prop = RNA_def_enum(ot->srna, "type", prop_similar_types, SIMEDBONE_LENGTH, "Type", "");
	RNA_def_float(ot->srna, "threshold", 0.1f, 0.0f, 1.0f, "Threshold", "", 0.0f, 1.0f);
}

/* ********************* select hierarchy operator ************** */

static int armature_select_hierarchy_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	Object *ob;
	bArmature *arm;
	EditBone *ebone_active;
	int direction = RNA_enum_get(op->ptr, "direction");
	const bool add_to_sel = RNA_boolean_get(op->ptr, "extend");
	bool changed = false;

	ob = obedit;
	arm = (bArmature *)ob->data;

	ebone_active = arm->act_edbone;
	if (ebone_active == NULL) {
		return OPERATOR_CANCELLED;
	}

	if (direction == BONE_SELECT_PARENT) {
		if (ebone_active->parent) {
			EditBone *ebone_parent;

			ebone_parent = ebone_active->parent;

			if (EBONE_SELECTABLE(arm, ebone_parent)) {
				arm->act_edbone = ebone_parent;

				if (!add_to_sel) {
					ED_armature_ebone_select_set(ebone_active, false);
				}
				ED_armature_ebone_select_set(ebone_parent, true);

				changed = true;
			}
		}

	}
	else {  /* BONE_SELECT_CHILD */
		EditBone *ebone_iter, *ebone_child = NULL;
		int pass;

		/* first pass, only connected bones (the logical direct child) */
		for (pass = 0; pass < 2 && (ebone_child == NULL); pass++) {
			for (ebone_iter = arm->edbo->first; ebone_iter; ebone_iter = ebone_iter->next) {
				/* possible we have multiple children, some invisible */
				if (EBONE_SELECTABLE(arm, ebone_iter)) {
					if (ebone_iter->parent == ebone_active) {
						if ((pass == 1) || (ebone_iter->flag & BONE_CONNECTED)) {
							ebone_child = ebone_iter;
							break;
						}
					}
				}
			}
		}

		if (ebone_child) {
			arm->act_edbone = ebone_child;

			if (!add_to_sel) {
				ED_armature_ebone_select_set(ebone_active, false);
			}
			ED_armature_ebone_select_set(ebone_child, true);

			changed = true;
		}
	}

	if (changed == false) {
		return OPERATOR_CANCELLED;
	}

	ED_armature_edit_sync_selection(arm->edbo);

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_hierarchy(wmOperatorType *ot)
{
	static const EnumPropertyItem direction_items[] = {
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

/****************** Mirror Select ****************/

/**
 * \note clone of #pose_select_mirror_exec keep in sync
 */
static int armature_select_mirror_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	bArmature *arm = obedit->data;
	EditBone *ebone, *ebone_mirror_act = NULL;
	const bool active_only = RNA_boolean_get(op->ptr, "only_active");
	const bool extend = RNA_boolean_get(op->ptr, "extend");

	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		const int flag = ED_armature_ebone_selectflag_get(ebone);
		EBONE_PREV_FLAG_SET(ebone, flag);
	}

	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_SELECTABLE(arm, ebone)) {
			EditBone *ebone_mirror;
			int flag_new = extend ? EBONE_PREV_FLAG_GET(ebone) : 0;

			if ((ebone_mirror = ED_armature_ebone_get_mirrored(arm->edbo, ebone)) &&
			    (EBONE_VISIBLE(arm, ebone_mirror)))
			{
				const int flag_mirror = EBONE_PREV_FLAG_GET(ebone_mirror);
				flag_new |= flag_mirror;

				if (ebone == arm->act_edbone) {
					ebone_mirror_act = ebone_mirror;
				}

				/* skip all but the active or its mirror */
				if (active_only && !ELEM(arm->act_edbone, ebone, ebone_mirror)) {
					continue;
				}
			}

			ED_armature_ebone_selectflag_set(ebone, flag_new);
		}
	}

	if (ebone_mirror_act) {
		arm->act_edbone = ebone_mirror_act;
	}

	ED_armature_edit_sync_selection(arm->edbo);

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_select_mirror(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Flip Active/Selected Bone";
	ot->idname = "ARMATURE_OT_select_mirror";
	ot->description = "Mirror the bone selection";

	/* api callbacks */
	ot->exec = armature_select_mirror_exec;
	ot->poll = ED_operator_editarmature;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	RNA_def_boolean(ot->srna, "only_active", false, "Active Only", "Only operate on the active bone");
	RNA_def_boolean(ot->srna, "extend", false, "Extend", "Extend the selection");
}


/****************** Select Path ****************/

static bool armature_shortest_path_select(bArmature *arm, EditBone *ebone_parent, EditBone *ebone_child,
                                          bool use_parent, bool is_test)
{
	do {

		if (!use_parent && (ebone_child == ebone_parent))
			break;

		if (is_test) {
			if (!EBONE_SELECTABLE(arm, ebone_child)) {
				return false;
			}
		}
		else {
			ED_armature_ebone_selectflag_set(ebone_child, (BONE_SELECTED | BONE_TIPSEL | BONE_ROOTSEL));
		}

		if (ebone_child == ebone_parent)
			break;

		ebone_child = ebone_child->parent;
	} while (true);

	return true;
}

static int armature_shortest_path_pick_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	Object *obedit = CTX_data_edit_object(C);
	bArmature *arm = obedit->data;
	EditBone *ebone_src, *ebone_dst;
	EditBone *ebone_isect_parent = NULL;
	EditBone *ebone_isect_child[2];
	bool changed;
	Base *base_dst = NULL;

	view3d_operator_needs_opengl(C);

	ebone_src = arm->act_edbone;
	ebone_dst = get_nearest_bone(C, event->mval, false, &base_dst);

	/* fallback to object selection */
	if (ELEM(NULL, ebone_src, ebone_dst) || (ebone_src == ebone_dst)) {
		return OPERATOR_PASS_THROUGH;
	}

	if (base_dst && base_dst->object != obedit) {
		/* Disconnected, ignore. */
		return OPERATOR_CANCELLED;
	}

	ebone_isect_child[0] = ebone_src;
	ebone_isect_child[1] = ebone_dst;


	/* ensure 'ebone_src' is the parent of 'ebone_dst', or set 'ebone_isect_parent' */
	if (ED_armature_ebone_is_child_recursive(ebone_src, ebone_dst)) {
		/* pass */
	}
	else if (ED_armature_ebone_is_child_recursive(ebone_dst, ebone_src)) {
		SWAP(EditBone *, ebone_src, ebone_dst);
	}
	else if ((ebone_isect_parent = ED_armature_ebone_find_shared_parent(ebone_isect_child, 2))) {
		/* pass */
	}
	else {
		/* disconnected bones */
		return OPERATOR_CANCELLED;
	}


	if (ebone_isect_parent) {
		if (armature_shortest_path_select(arm, ebone_isect_parent, ebone_src, false, true) &&
		    armature_shortest_path_select(arm, ebone_isect_parent, ebone_dst, false, true))
		{
			armature_shortest_path_select(arm, ebone_isect_parent, ebone_src, false, false);
			armature_shortest_path_select(arm, ebone_isect_parent, ebone_dst, false, false);
			changed = true;
		}
		else {
			/* unselectable */
			changed = false;
		}
	}
	else {
		if (armature_shortest_path_select(arm, ebone_src, ebone_dst, true, true)) {
			armature_shortest_path_select(arm, ebone_src, ebone_dst, true, false);
			changed = true;
		}
		else {
			/* unselectable */
			changed = false;
		}
	}

	if (changed) {
		arm->act_edbone = ebone_dst;
		ED_armature_edit_sync_selection(arm->edbo);
		WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

		return OPERATOR_FINISHED;
	}
	else {
		BKE_report(op->reports, RPT_WARNING, "Unselectable bone in chain");
		return OPERATOR_CANCELLED;
	}
}

void ARMATURE_OT_shortest_path_pick(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Pick Shortest Path";
	ot->idname = "ARMATURE_OT_shortest_path_pick";
	ot->description = "Select shortest path between two bones";

	/* api callbacks */
	ot->invoke = armature_shortest_path_pick_invoke;
	ot->poll = ED_operator_editarmature;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}
