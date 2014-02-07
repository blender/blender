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
 * Armature EditMode tools - transforms, chain based editing, and other settings
 */

/** \file blender/editors/armature/armature_edit.c
 *  \ingroup edarmature
 */

#include <assert.h> 

#include "DNA_armature_types.h"
#include "DNA_constraint_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_blenlib.h"
#include "BLI_math.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "BKE_constraint.h"
#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "armature_intern.h"

/* ************************** Object Tools Exports ******************************* */
/* NOTE: these functions are exported to the Object module to be called from the tools there */

void ED_armature_apply_transform(Object *ob, float mat[4][4])
{
	bArmature *arm = ob->data;

	/* Put the armature into editmode */
	ED_armature_to_edit(arm);

	/* Transform the bones*/
	ED_armature_transform_bones(arm, mat);

	/* Turn the list into an armature */
	ED_armature_from_edit(arm);
	ED_armature_edit_free(arm);
}

void ED_armature_transform_bones(struct bArmature *arm, float mat[4][4])
{
	EditBone *ebone;
	float scale = mat4_to_scale(mat);   /* store the scale of the matrix here to use on envelopes */
	float mat3[3][3];

	copy_m3_m4(mat3, mat);
	normalize_m3(mat3);
	/* Do the rotations */
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		float tmat[3][3];
		
		/* find the current bone's roll matrix */
		ED_armature_ebone_to_mat3(ebone, tmat);
		
		/* transform the roll matrix */
		mul_m3_m3m3(tmat, mat3, tmat);
		
		/* transform the bone */
		mul_m4_v3(mat, ebone->head);
		mul_m4_v3(mat, ebone->tail);

		/* apply the transfiormed roll back */
		mat3_to_vec_roll(tmat, NULL, &ebone->roll);
		
		ebone->rad_head *= scale;
		ebone->rad_tail *= scale;
		ebone->dist     *= scale;
		
		/* we could be smarter and scale by the matrix along the x & z axis */
		ebone->xwidth   *= scale;
		ebone->zwidth   *= scale;
	}
}

void ED_armature_transform(struct bArmature *arm, float mat[4][4])
{
	if (arm->edbo) {
		ED_armature_transform_bones(arm, mat);
	}
	else {
		/* Put the armature into editmode */
		ED_armature_to_edit(arm);

		/* Transform the bones */
		ED_armature_transform_bones(arm, mat);

		/* Go back to object mode*/
		ED_armature_from_edit(arm);
		ED_armature_edit_free(arm);
	}
}

/* exported for use in editors/object/ */
/* 0 == do center, 1 == center new, 2 == center cursor */
void ED_armature_origin_set(Scene *scene, Object *ob, float cursor[3], int centermode, int around)
{
	Object *obedit = scene->obedit; // XXX get from context
	EditBone *ebone;
	bArmature *arm = ob->data;
	float cent[3];

	/* Put the armature into editmode */
	if (ob != obedit) {
		ED_armature_to_edit(arm);
		obedit = NULL; /* we cant use this so behave as if there is no obedit */
	}

	/* Find the centerpoint */
	if (centermode == 2) {
		copy_v3_v3(cent, cursor);
		invert_m4_m4(ob->imat, ob->obmat);
		mul_m4_v3(ob->imat, cent);
	}
	else {
		if (around == V3D_CENTROID) {
			int total = 0;
			zero_v3(cent);
			for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
				total += 2;
				add_v3_v3(cent, ebone->head);
				add_v3_v3(cent, ebone->tail);
			}
			if (total) {
				mul_v3_fl(cent, 1.0f / (float)total);
			}
		}
		else {
			float min[3], max[3];
			INIT_MINMAX(min, max);
			for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
				minmax_v3v3_v3(min, max, ebone->head);
				minmax_v3v3_v3(min, max, ebone->tail);
			}
			mid_v3_v3v3(cent, min, max);
		}
	}
	
	/* Do the adjustments */
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		sub_v3_v3(ebone->head, cent);
		sub_v3_v3(ebone->tail, cent);
	}
	
	/* Turn the list into an armature */
	if (obedit == NULL) {
		ED_armature_from_edit(arm);
		ED_armature_edit_free(arm);
	}

	/* Adjust object location for new centerpoint */
	if (centermode && obedit == NULL) {
		mul_mat3_m4_v3(ob->obmat, cent); /* ommit translation part */
		add_v3_v3(ob->loc, cent);
	}
}

/* ********************************* Roll ******************************* */

/* adjust bone roll to align Z axis with vector
 * vec is in local space and is normalized
 */
float ED_rollBoneToVector(EditBone *bone, const float align_axis[3], const short axis_only)
{
	float mat[3][3], nor[3];

	sub_v3_v3v3(nor, bone->tail, bone->head);
	vec_roll_to_mat3(nor, 0.0f, mat);
	
	/* check the bone isn't aligned with the axis */
	if (!is_zero_v3(align_axis) && angle_v3v3(align_axis, mat[2]) > FLT_EPSILON) {
		float vec[3], align_axis_proj[3], roll;
		
		/* project the new_up_axis along the normal */
		project_v3_v3v3(vec, align_axis, nor);
		sub_v3_v3v3(align_axis_proj, align_axis, vec);
		
		if (axis_only) {
			if (angle_v3v3(align_axis_proj, mat[2]) > (float)(M_PI / 2.0)) {
				negate_v3(align_axis_proj);
			}
		}
		
		roll = angle_v3v3(align_axis_proj, mat[2]);
		
		cross_v3_v3v3(vec, mat[2], align_axis_proj);
		
		if (dot_v3v3(vec, nor) < 0) {
			roll = -roll;
		}
		
		return roll;
	}

	return 0.0f;
}


typedef enum eCalcRollTypes {
	CALC_ROLL_X          = 0,
	CALC_ROLL_Y          = 1,
	CALC_ROLL_Z          = 2,
	
	CALC_ROLL_TAN_X      = 3,
	CALC_ROLL_TAN_Z      = 4,

	CALC_ROLL_ACTIVE     = 5,
	CALC_ROLL_VIEW       = 6,
	CALC_ROLL_CURSOR     = 7,
} eCalcRollTypes;

static EnumPropertyItem prop_calc_roll_types[] = {
	{CALC_ROLL_TAN_X, "X", 0, "Local X Tangent", ""},
	{CALC_ROLL_TAN_Z, "Z", 0, "Local Z Tangent", ""},

	{CALC_ROLL_X, "X", 0, "Global X Axis", ""},
	{CALC_ROLL_Y, "Y", 0, "Global Y Axis", ""},
	{CALC_ROLL_Z, "Z", 0, "Global Z Axis", ""},

	{CALC_ROLL_ACTIVE, "ACTIVE", 0, "Active Bone", ""},
	{CALC_ROLL_VIEW, "VIEW", 0, "View Axis", ""},
	{CALC_ROLL_CURSOR, "CURSOR", 0, "Cursor", ""},
	{0, NULL, 0, NULL, NULL}
};


static int armature_calc_roll_exec(bContext *C, wmOperator *op) 
{
	Object *ob = CTX_data_edit_object(C);
	const short type = RNA_enum_get(op->ptr, "type");
	const short axis_only = RNA_boolean_get(op->ptr, "axis_only");
	const short axis_flip = RNA_boolean_get(op->ptr, "axis_flip");

	float imat[3][3];

	bArmature *arm = ob->data;
	EditBone *ebone;

	copy_m3_m4(imat, ob->obmat);
	invert_m3(imat);

	if (type == CALC_ROLL_CURSOR) { /* Cursor */
		Scene *scene = CTX_data_scene(C);
		View3D *v3d = CTX_wm_view3d(C); /* can be NULL */
		float cursor_local[3];
		const float   *cursor = ED_view3d_cursor3d_get(scene, v3d);
		
		
		copy_v3_v3(cursor_local, cursor);
		mul_m3_v3(imat, cursor_local);
		
		/* cursor */
		for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
			if (EBONE_VISIBLE(arm, ebone) && EBONE_EDITABLE(ebone)) {
				float cursor_rel[3];
				sub_v3_v3v3(cursor_rel, cursor_local, ebone->head);
				if (axis_flip) negate_v3(cursor_rel);
				ebone->roll = ED_rollBoneToVector(ebone, cursor_rel, axis_only);
			}
		}
	}
	else if (ELEM(type, CALC_ROLL_TAN_X, CALC_ROLL_TAN_Z)) {
		for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
			if (ebone->parent) {
				bool is_edit        = (EBONE_VISIBLE(arm, ebone)         && EBONE_EDITABLE(ebone));
				bool is_edit_parent = (EBONE_VISIBLE(arm, ebone->parent) && EBONE_EDITABLE(ebone->parent));

				if (is_edit || is_edit_parent) {
					EditBone *ebone_other = ebone->parent;
					float dir_a[3];
					float dir_b[3];
					float vec[3];
					bool is_vec_zero;

					sub_v3_v3v3(dir_a, ebone->tail, ebone->head);
					normalize_v3(dir_a);

					/* find the first bone in the chane with a different direction */
					do {
						sub_v3_v3v3(dir_b, ebone_other->head, ebone_other->tail);
						normalize_v3(dir_b);

						if (type == CALC_ROLL_TAN_Z) {
							cross_v3_v3v3(vec, dir_a, dir_b);
						}
						else {
							add_v3_v3v3(vec, dir_a, dir_b);
						}
					} while ((is_vec_zero = (normalize_v3(vec) < 0.00001f)) &&
					         (ebone_other = ebone_other->parent));

					if (!is_vec_zero) {
						if (axis_flip) negate_v3(vec);

						if (is_edit) {
							ebone->roll = ED_rollBoneToVector(ebone, vec, axis_only);
						}

						/* parentless bones use cross product with child */
						if (is_edit_parent) {
							if (ebone->parent->parent == NULL) {
								ebone->parent->roll = ED_rollBoneToVector(ebone->parent, vec, axis_only);
							}
						}
					}
				}
			}
		}
	}
	else {
		float vec[3] = {0.0f, 0.0f, 0.0f};
		if (type == CALC_ROLL_VIEW) { /* View */
			RegionView3D *rv3d = CTX_wm_region_view3d(C);
			if (rv3d == NULL) {
				BKE_report(op->reports, RPT_ERROR, "No region view3d available");
				return OPERATOR_CANCELLED;
			}
			
			copy_v3_v3(vec, rv3d->viewinv[2]);
			mul_m3_v3(imat, vec);
		}
		else if (type == CALC_ROLL_ACTIVE) {
			float mat[3][3];
			ebone = (EditBone *)arm->act_edbone;
			if (ebone == NULL) {
				BKE_report(op->reports, RPT_ERROR, "No active bone set");
				return OPERATOR_CANCELLED;
			}
			
			ED_armature_ebone_to_mat3(ebone, mat);
			copy_v3_v3(vec, mat[2]);
		}
		else { /* Axis */
			assert(type >= 0 && type <= 5);
			if (type < 3) vec[type] = 1.0f;
			else vec[type - 2] = -1.0f;
			mul_m3_v3(imat, vec);
		}
		
		if (axis_flip) negate_v3(vec);
		
		for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
			if (EBONE_VISIBLE(arm, ebone) && EBONE_EDITABLE(ebone)) {
				/* roll func is a callback which assumes that all is well */
				ebone->roll = ED_rollBoneToVector(ebone, vec, axis_only);
			}
		}
	}
	
	if (arm->flag & ARM_MIRROR_EDIT) {
		for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
			if ((EBONE_VISIBLE(arm, ebone) && EBONE_EDITABLE(ebone)) == 0) {
				EditBone *ebone_mirr = ED_armature_bone_get_mirrored(arm->edbo, ebone);
				if (ebone_mirr && (EBONE_VISIBLE(arm, ebone_mirr) && EBONE_EDITABLE(ebone_mirr))) {
					ebone->roll = -ebone_mirr->roll;
				}
			}
		}
	}
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_calculate_roll(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Recalculate Roll";
	ot->idname = "ARMATURE_OT_calculate_roll";
	ot->description = "Automatically fix alignment of select bones' axes";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = armature_calc_roll_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", prop_calc_roll_types, CALC_ROLL_TAN_X, "Type", "");
	RNA_def_boolean(ot->srna, "axis_flip", 0, "Flip Axis", "Negate the alignment axis");
	RNA_def_boolean(ot->srna, "axis_only", 0, "Shortest Rotation", "Ignore the axis direction, use the shortest rotation to align");
}

/* ******************************** Chain-Based Tools ********************************* */

/* temporary data-structure for merge/fill bones */
typedef struct EditBonePoint {
	struct EditBonePoint *next, *prev;
	
	EditBone *head_owner;       /* EditBone which uses this point as a 'head' point */
	EditBone *tail_owner;       /* EditBone which uses this point as a 'tail' point */
	
	float vec[3];               /* the actual location of the point in local/EditMode space */
} EditBonePoint;

/* find chain-tips (i.e. bones without children) */
static void chains_find_tips(ListBase *edbo, ListBase *list)
{
	EditBone *curBone, *ebo;
	LinkData *ld;
	
	/* note: this is potentially very slow ... there's got to be a better way */
	for (curBone = edbo->first; curBone; curBone = curBone->next) {
		short stop = 0;
		
		/* is this bone contained within any existing chain? (skip if so) */
		for (ld = list->first; ld; ld = ld->next) {
			for (ebo = ld->data; ebo; ebo = ebo->parent) {
				if (ebo == curBone) {
					stop = 1;
					break;
				}
			}
			
			if (stop) break;
		}
		/* skip current bone if it is part of an existing chain */
		if (stop) continue;
		
		/* is any existing chain part of the chain formed by this bone? */
		stop = 0;
		for (ebo = curBone->parent; ebo; ebo = ebo->parent) {
			for (ld = list->first; ld; ld = ld->next) {
				if (ld->data == ebo) {
					ld->data = curBone;
					stop = 1;
					break;
				}
			}
			
			if (stop) break;
		}
		/* current bone has already been added to a chain? */
		if (stop) continue;
		
		/* add current bone to a new chain */
		ld = MEM_callocN(sizeof(LinkData), "BoneChain");
		ld->data = curBone;
		BLI_addtail(list, ld);
	}
}

/* --------------------- */

static void fill_add_joint(EditBone *ebo, short eb_tail, ListBase *points)
{
	EditBonePoint *ebp;
	float vec[3];
	short found = 0;
	
	if (eb_tail) {
		copy_v3_v3(vec, ebo->tail);
	}
	else {
		copy_v3_v3(vec, ebo->head);
	}
	
	for (ebp = points->first; ebp; ebp = ebp->next) {
		if (equals_v3v3(ebp->vec, vec)) {
			if (eb_tail) {
				if ((ebp->head_owner) && (ebp->head_owner->parent == ebo)) {
					/* so this bone's tail owner is this bone */
					ebp->tail_owner = ebo;
					found = 1;
					break;
				}
			}
			else {
				if ((ebp->tail_owner) && (ebo->parent == ebp->tail_owner)) {
					/* so this bone's head owner is this bone */
					ebp->head_owner = ebo;
					found = 1;
					break;
				}
			}
		}
	}
	
	/* allocate a new point if no existing point was related */
	if (found == 0) {
		ebp = MEM_callocN(sizeof(EditBonePoint), "EditBonePoint");
		
		if (eb_tail) {
			copy_v3_v3(ebp->vec, ebo->tail);
			ebp->tail_owner = ebo;
		}
		else {
			copy_v3_v3(ebp->vec, ebo->head);
			ebp->head_owner = ebo;
		}
		
		BLI_addtail(points, ebp);
	}
}

/* bone adding between selected joints */
static int armature_fill_bones_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	bArmature *arm = (obedit) ? obedit->data : NULL;
	Scene *scene = CTX_data_scene(C);
	View3D *v3d = CTX_wm_view3d(C);
	ListBase points = {NULL, NULL};
	int count;

	/* sanity checks */
	if (ELEM(NULL, obedit, arm))
		return OPERATOR_CANCELLED;

	/* loop over all bones, and only consider if visible */
	CTX_DATA_BEGIN(C, EditBone *, ebone, visible_bones)
	{
		if (!(ebone->flag & BONE_CONNECTED) && (ebone->flag & BONE_ROOTSEL))
			fill_add_joint(ebone, 0, &points);
		if (ebone->flag & BONE_TIPSEL) 
			fill_add_joint(ebone, 1, &points);
	}
	CTX_DATA_END;
	
	/* the number of joints determines how we fill:
	 *  1) between joint and cursor (joint=head, cursor=tail)
	 *  2) between the two joints (order is dependent on active-bone/hierarchy)
	 *  3+) error (a smarter method involving finding chains needs to be worked out
	 */
	count = BLI_countlist(&points);
	
	if (count == 0) {
		BKE_report(op->reports, RPT_ERROR, "No joints selected");
		return OPERATOR_CANCELLED;
	}
	else if (count == 1) {
		EditBonePoint *ebp;
		float curs[3];
		
		/* Get Points - selected joint */
		ebp = (EditBonePoint *)points.first;
		
		/* Get points - cursor (tail) */
		invert_m4_m4(obedit->imat, obedit->obmat);
		mul_v3_m4v3(curs, obedit->imat, ED_view3d_cursor3d_get(scene, v3d));
		
		/* Create a bone */
		/* newbone = */ add_points_bone(obedit, ebp->vec, curs);
	}
	else if (count == 2) {
		EditBonePoint *ebp, *ebp2;
		float head[3], tail[3];
		short headtail = 0;
		
		/* check that the points don't belong to the same bone */
		ebp = (EditBonePoint *)points.first;
		ebp2 = ebp->next;
		
		if ((ebp->head_owner == ebp2->tail_owner) && (ebp->head_owner != NULL)) {
			BKE_report(op->reports, RPT_ERROR, "Same bone selected...");
			BLI_freelistN(&points);
			return OPERATOR_CANCELLED;
		}
		if ((ebp->tail_owner == ebp2->head_owner) && (ebp->tail_owner != NULL)) {
			BKE_report(op->reports, RPT_ERROR, "Same bone selected...");
			BLI_freelistN(&points);
			return OPERATOR_CANCELLED;
		}
		
		/* find which one should be the 'head' */
		if ((ebp->head_owner && ebp2->head_owner) || (ebp->tail_owner && ebp2->tail_owner)) {
			/* rule: whichever one is closer to 3d-cursor */
			float curs[3];
			float vecA[3], vecB[3];
			float distA, distB;
			
			/* get cursor location */
			invert_m4_m4(obedit->imat, obedit->obmat);
			mul_v3_m4v3(curs, obedit->imat, ED_view3d_cursor3d_get(scene, v3d));
			
			/* get distances */
			sub_v3_v3v3(vecA, ebp->vec, curs);
			sub_v3_v3v3(vecB, ebp2->vec, curs);
			distA = len_v3(vecA);
			distB = len_v3(vecB);
			
			/* compare distances - closer one therefore acts as direction for bone to go */
			headtail = (distA < distB) ? 2 : 1;
		}
		else if (ebp->head_owner) {
			headtail = 1;
		}
		else if (ebp2->head_owner) {
			headtail = 2;
		}
		
		/* assign head/tail combinations */
		if (headtail == 2) {
			copy_v3_v3(head, ebp->vec);
			copy_v3_v3(tail, ebp2->vec);
		}
		else if (headtail == 1) {
			copy_v3_v3(head, ebp2->vec);
			copy_v3_v3(tail, ebp->vec);
		}
		
		/* add new bone and parent it to the appropriate end */
		if (headtail) {
			EditBone *newbone = add_points_bone(obedit, head, tail);
			
			/* do parenting (will need to set connected flag too) */
			if (headtail == 2) {
				/* ebp tail or head - tail gets priority */
				if (ebp->tail_owner)
					newbone->parent = ebp->tail_owner;
				else
					newbone->parent = ebp->head_owner;
			}
			else {
				/* ebp2 tail or head - tail gets priority */
				if (ebp2->tail_owner)
					newbone->parent = ebp2->tail_owner;
				else
					newbone->parent = ebp2->head_owner;
			}

			/* don't set for bone connecting two head points of bones */
			if (ebp->tail_owner || ebp2->tail_owner) {
				newbone->flag |= BONE_CONNECTED;
			}
		}
	}
	else {
		/* FIXME.. figure out a method for multiple bones */
		BKE_reportf(op->reports, RPT_ERROR, "Too many points selected: %d", count);
		BLI_freelistN(&points);
		return OPERATOR_CANCELLED;
	}
	
	/* updates */
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, obedit);
	
	/* free points */
	BLI_freelistN(&points);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_fill(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Fill Between Joints";
	ot->idname = "ARMATURE_OT_fill";
	ot->description = "Add bone between selected joint(s) and/or 3D-Cursor";
	
	/* callbacks */
	ot->exec = armature_fill_bones_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* --------------------- */

/* this function merges between two bones, removes them and those in-between, 
 * and adjusts the parent relationships for those in-between
 */
static void bones_merge(Object *obedit, EditBone *start, EditBone *end, EditBone *endchild, ListBase *chains)
{
	bArmature *arm = obedit->data;
	EditBone *ebo, *ebone, *newbone;
	LinkData *chain;
	float head[3], tail[3];
	
	/* check if same bone */
	if (start == end) {
		if (G.debug & G_DEBUG) {
			printf("Error: same bone!\n");
			printf("\tstart = %s, end = %s\n", start->name, end->name);
		}
	}
	
	/* step 1: add a new bone
	 *	- head = head/tail of start (default head)
	 *	- tail = head/tail of end (default tail)
	 *	- parent = parent of start
	 */
	if ((start->flag & BONE_TIPSEL) && (start->flag & BONE_SELECTED) == 0) {
		copy_v3_v3(head, start->tail);
	}
	else {
		copy_v3_v3(head, start->head);
	}
	if ((end->flag & BONE_ROOTSEL) && (end->flag & BONE_SELECTED) == 0) {
		copy_v3_v3(tail, end->head);
	}
	else {
		copy_v3_v3(tail, end->tail);
	}
	newbone = add_points_bone(obedit, head, tail);
	newbone->parent = start->parent;

	/* TODO, copy more things to the new bone */
	newbone->flag = start->flag & (BONE_HINGE | BONE_NO_DEFORM | BONE_NO_SCALE |
	                               BONE_NO_CYCLICOFFSET | BONE_NO_LOCAL_LOCATION | BONE_DONE);
	
	/* step 2a: reparent any side chains which may be parented to any bone in the chain of bones to merge 
	 *	- potentially several tips for side chains leading to some tree exist...
	 */
	for (chain = chains->first; chain; chain = chain->next) {
		/* traverse down chain until we hit the bottom or if we run into the tip of the chain of bones we're 
		 * merging (need to stop in this case to avoid corrupting this chain too!) 
		 */
		for (ebone = chain->data; (ebone) && (ebone != end); ebone = ebone->parent) {
			short found = 0;
			
			/* check if this bone is parented to one in the merging chain
			 * ! WATCHIT: must only go check until end of checking chain
			 */
			for (ebo = end; (ebo) && (ebo != start->parent); ebo = ebo->parent) {
				/* side-chain found? --> remap parent to new bone, then we're done with this chain :) */
				if (ebone->parent == ebo) {
					ebone->parent = newbone;
					found = 1;
					break;
				}
			}
			
			/* carry on to the next tip now  */
			if (found) 
				break;
		}
	}
	
	/* step 2b: parent child of end to newbone (child from this chain) */
	if (endchild)
		endchild->parent = newbone;
	
	/* step 3: delete all bones between and including start and end */
	for (ebo = end; ebo; ebo = ebone) {
		ebone = (ebo == start) ? (NULL) : (ebo->parent);
		bone_free(arm, ebo);
	}
	
	newbone->flag |= (BONE_ROOTSEL | BONE_TIPSEL | BONE_SELECTED);
	ED_armature_sync_selection(arm->edbo);
}


static int armature_merge_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	bArmature *arm = (obedit) ? obedit->data : NULL;
	short type = RNA_enum_get(op->ptr, "type");
	
	/* sanity checks */
	if (ELEM(NULL, obedit, arm))
		return OPERATOR_CANCELLED;
	
	/* for now, there's only really one type of merging that's performed... */
	if (type == 1) {
		/* go down chains, merging bones */
		ListBase chains = {NULL, NULL};
		LinkData *chain, *nchain;
		EditBone *ebo;
		
		armature_tag_select_mirrored(arm);
		
		/* get chains (ends on chains) */
		chains_find_tips(arm->edbo, &chains);
		if (BLI_listbase_is_empty(&chains)) return OPERATOR_CANCELLED;
		
		/* each 'chain' is the last bone in the chain (with no children) */
		for (chain = chains.first; chain; chain = nchain) {
			EditBone *bstart = NULL, *bend = NULL;
			EditBone *bchild = NULL, *child = NULL;
			
			/* temporarily remove chain from list of chains */
			nchain = chain->next;
			BLI_remlink(&chains, chain);
			
			/* only consider bones that are visible and selected */
			for (ebo = chain->data; ebo; child = ebo, ebo = ebo->parent) {
				/* check if visible + selected */
				if (EBONE_VISIBLE(arm, ebo) &&
				    ((ebo->flag & BONE_CONNECTED) || (ebo->parent == NULL)) &&
				    (ebo->flag & BONE_SELECTED) )
				{
					/* set either end or start (end gets priority, unless it is already set) */
					if (bend == NULL) {
						bend = ebo;
						bchild = child;
					}
					else 
						bstart = ebo;
				}
				else {
					/* chain is broken... merge any continous segments then clear */
					if (bstart && bend)
						bones_merge(obedit, bstart, bend, bchild, &chains);
					
					bstart = NULL;
					bend = NULL;
					bchild = NULL;
				}
			}
			
			/* merge from bstart to bend if something not merged */
			if (bstart && bend)
				bones_merge(obedit, bstart, bend, bchild, &chains);
			
			/* put back link */
			BLI_insertlinkbefore(&chains, nchain, chain);
		}
		
		armature_tag_unselect(arm);
		
		BLI_freelistN(&chains);
	}
	
	/* updates */
	ED_armature_sync_selection(arm->edbo);
	WM_event_add_notifier(C, NC_OBJECT | ND_POSE, obedit);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_merge(wmOperatorType *ot)
{
	static EnumPropertyItem merge_types[] = {
		{1, "WITHIN_CHAIN", 0, "Within Chains", ""},
		{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name = "Merge Bones";
	ot->idname = "ARMATURE_OT_merge";
	ot->description = "Merge continuous chains of selected bones";
	
	/* callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = armature_merge_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	/* properties */
	ot->prop = RNA_def_enum(ot->srna, "type", merge_types, 0, "Type", "");
}

/* --------------------- */

/* Switch Direction operator:
 * Currently, this does not use context loops, as context loops do not make it
 * easy to retrieve any hierarchical/chain relationships which are necessary for
 * this to be done easily.
 */
 
/* helper to clear BONE_TRANSFORM flags */
static void armature_clear_swap_done_flags(bArmature *arm)
{
	EditBone *ebone;
	
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		ebone->flag &= ~BONE_TRANSFORM;
	}
}

static int armature_switch_direction_exec(bContext *C, wmOperator *UNUSED(op)) 
{
	Object *ob = CTX_data_edit_object(C);
	bArmature *arm = (bArmature *)ob->data;
	ListBase chains = {NULL, NULL};
	LinkData *chain;
	
	/* get chains of bones (ends on chains) */
	chains_find_tips(arm->edbo, &chains);
	if (BLI_listbase_is_empty(&chains)) return OPERATOR_CANCELLED;
	
	/* ensure that mirror bones will also be operated on */
	armature_tag_select_mirrored(arm);
	
	/* clear BONE_TRANSFORM flags 
	 * - used to prevent duplicate/canceling operations from occurring [#34123]
	 * - BONE_DONE cannot be used here as that's already used for mirroring
	 */
	armature_clear_swap_done_flags(arm);
	
	/* loop over chains, only considering selected and visible bones */
	for (chain = chains.first; chain; chain = chain->next) {
		EditBone *ebo, *child = NULL, *parent = NULL;
		
		/* loop over bones in chain */
		for (ebo = chain->data; ebo; ebo = parent) {
			/* parent is this bone's original parent
			 *	- we store this, as the next bone that is checked is this one
			 *	  but the value of ebo->parent may change here...
			 */
			parent = ebo->parent;
			
			/* skip bone if already handled... [#34123] */
			if ((ebo->flag & BONE_TRANSFORM) == 0) {
				/* only if selected and editable */
				if (EBONE_VISIBLE(arm, ebo) && EBONE_EDITABLE(ebo)) {
					/* swap head and tail coordinates */
					SWAP(float, ebo->head[0], ebo->tail[0]);
					SWAP(float, ebo->head[1], ebo->tail[1]);
					SWAP(float, ebo->head[2], ebo->tail[2]);
					
					/* do parent swapping:
					 *	- use 'child' as new parent
					 *	- connected flag is only set if points are coincidental
					 */
					ebo->parent = child;
					if ((child) && equals_v3v3(ebo->head, child->tail))
						ebo->flag |= BONE_CONNECTED;
					else
						ebo->flag &= ~BONE_CONNECTED;
					
					/* get next bones 
					 *	- child will become the new parent of next bone
					 */
					child = ebo;
				}
				else {
					/* not swapping this bone, however, if its 'parent' got swapped, unparent us from it 
					 * as it will be facing in opposite direction
					 */
					if ((parent) && (EBONE_VISIBLE(arm, parent) && EBONE_EDITABLE(parent))) {
						ebo->parent = NULL;
						ebo->flag &= ~BONE_CONNECTED;
					}
					
					/* get next bones
					 *	- child will become new parent of next bone (not swapping occurred, 
					 *	  so set to NULL to prevent infinite-loop)
					 */
					child = NULL;
				}
				
				/* tag as done (to prevent double-swaps) */
				ebo->flag |= BONE_TRANSFORM;
			}
		}
	}
	
	/* free chains */
	BLI_freelistN(&chains);
	
	/* clear temp flags */
	armature_clear_swap_done_flags(arm);
	armature_tag_unselect(arm);
	
	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_switch_direction(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Switch Direction";
	ot->idname = "ARMATURE_OT_switch_direction";
	ot->description = "Change the direction that a chain of bones points in (head <-> tail swap)";
	
	/* api callbacks */
	ot->exec = armature_switch_direction_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************* Align ******************************* */

/* helper to fix a ebone position if its parent has moved due to alignment*/
static void fix_connected_bone(EditBone *ebone)
{
	float diff[3];
	
	if (!(ebone->parent) || !(ebone->flag & BONE_CONNECTED) || equals_v3v3(ebone->parent->tail, ebone->head))
		return;
	
	/* if the parent has moved we translate child's head and tail accordingly */
	sub_v3_v3v3(diff, ebone->parent->tail, ebone->head);
	add_v3_v3(ebone->head, diff);
	add_v3_v3(ebone->tail, diff);
}

/* helper to recursively find chains of connected bones starting at ebone and fix their position */
static void fix_editbone_connected_children(ListBase *edbo, EditBone *ebone)
{
	EditBone *selbone;
	
	for (selbone = edbo->first; selbone; selbone = selbone->next) {
		if ((selbone->parent) && (selbone->parent == ebone) && (selbone->flag & BONE_CONNECTED)) {
			fix_connected_bone(selbone);
			fix_editbone_connected_children(edbo, selbone);
		}
	}
}			

static void bone_align_to_bone(ListBase *edbo, EditBone *selbone, EditBone *actbone)
{
	float selboneaxis[3], actboneaxis[3], length;

	sub_v3_v3v3(actboneaxis, actbone->tail, actbone->head);
	normalize_v3(actboneaxis);

	sub_v3_v3v3(selboneaxis, selbone->tail, selbone->head);
	length =  len_v3(selboneaxis);

	mul_v3_fl(actboneaxis, length);
	add_v3_v3v3(selbone->tail, selbone->head, actboneaxis);
	selbone->roll = actbone->roll;
	
	/* if the bone being aligned has connected descendants they must be moved
	 * according to their parent new position, otherwise they would be left
	 * in an inconsistent state: connected but away from the parent*/
	fix_editbone_connected_children(edbo, selbone);
}

static int armature_align_bones_exec(bContext *C, wmOperator *op) 
{
	Object *ob = CTX_data_edit_object(C);
	bArmature *arm = (bArmature *)ob->data;
	EditBone *actbone = CTX_data_active_bone(C);
	EditBone *actmirb = NULL;
	int num_selected_bones;
	
	/* there must be an active bone */
	if (actbone == NULL) {
		BKE_report(op->reports, RPT_ERROR, "Operation requires an active bone");
		return OPERATOR_CANCELLED;
	}
	else if (arm->flag & ARM_MIRROR_EDIT) {
		/* For X-Axis Mirror Editing option, we may need a mirror copy of actbone
		 * - if there's a mirrored copy of selbone, try to find a mirrored copy of actbone 
		 *   (i.e.  selbone="child.L" and actbone="parent.L", find "child.R" and "parent.R").
		 *   This is useful for arm-chains, for example parenting lower arm to upper arm
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
	num_selected_bones = CTX_DATA_COUNT(C, selected_editable_bones);
	if (num_selected_bones <= 1) {
		/* When only the active bone is selected, and it has a parent,
		 * align it to the parent, as that is the only possible outcome. 
		 */
		if (actbone->parent) {
			bone_align_to_bone(arm->edbo, actbone, actbone->parent);
			
			if ((arm->flag & ARM_MIRROR_EDIT) && (actmirb->parent))
				bone_align_to_bone(arm->edbo, actmirb, actmirb->parent);

			BKE_reportf(op->reports, RPT_INFO, "Aligned bone '%s' to parent", actbone->name);
		}
	}
	else {
		/* Align 'selected' bones to the active one
		 * - the context iterator contains both selected bones and their mirrored copies,
		 *   so we assume that unselected bones are mirrored copies of some selected bone
		 * - since the active one (and/or its mirror) will also be selected, we also need 
		 *   to check that we are not trying to operate on them, since such an operation
		 *   would cause errors
		 */
		
		/* align selected bones to the active one */
		CTX_DATA_BEGIN(C, EditBone *, ebone, selected_editable_bones)
		{
			if (ELEM(ebone, actbone, actmirb) == 0) {
				if (ebone->flag & BONE_SELECTED)
					bone_align_to_bone(arm->edbo, ebone, actbone);
				else
					bone_align_to_bone(arm->edbo, ebone, actmirb);
			}
		}
		CTX_DATA_END;

		BKE_reportf(op->reports, RPT_INFO, "%d bones aligned to bone '%s'", num_selected_bones, actbone->name);
	}

	/* note, notifier might evolve */
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, ob);
	
	return OPERATOR_FINISHED;
}

void ARMATURE_OT_align(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Align Bones";
	ot->idname = "ARMATURE_OT_align";
	ot->description = "Align selected bones to the active bone (or to their parent)";
	
	/* api callbacks */
	ot->exec = armature_align_bones_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************* Split ******************************* */

static int armature_split_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *ob = CTX_data_edit_object(C);
	bArmature *arm = (bArmature *)ob->data;
	EditBone *bone;

	for (bone = arm->edbo->first; bone; bone = bone->next) {
		if (bone->parent && (bone->flag & BONE_SELECTED) != (bone->parent->flag & BONE_SELECTED)) {
			bone->parent = NULL;
			bone->flag &= ~BONE_CONNECTED;
		}
	}
	for (bone = arm->edbo->first; bone; bone = bone->next) {
		ED_armature_ebone_select_set(bone, (bone->flag & BONE_SELECTED) != 0);
	}

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, ob);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_split(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Split";
	ot->idname = "ARMATURE_OT_split";
	ot->description = "Split off selected bones from connected unselected bones";

	/* api callbacks */
	ot->exec = armature_split_exec;
	ot->poll = ED_operator_editarmature;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************* Delete ******************************* */

/* previously delete_armature */
/* only editmode! */
static int armature_delete_selected_exec(bContext *C, wmOperator *UNUSED(op))
{
	bArmature *arm;
	EditBone *curBone, *ebone_next;
	bConstraint *con;
	Object *obedit = CTX_data_edit_object(C); // XXX get from context
	bool changed = false;
	arm = obedit->data;

	/* cancel if nothing selected */
	if (CTX_DATA_COUNT(C, selected_bones) == 0)
		return OPERATOR_CANCELLED;
	
	armature_select_mirrored(arm);
	
	/*  First erase any associated pose channel */
	if (obedit->pose) {
		bPoseChannel *pchan, *pchan_next;
		for (pchan = obedit->pose->chanbase.first; pchan; pchan = pchan_next) {
			pchan_next = pchan->next;
			curBone = ED_armature_bone_find_name(arm->edbo, pchan->name);
			
			if (curBone && (curBone->flag & BONE_SELECTED) && (arm->layer & curBone->layer)) {
				BKE_pose_channel_free(pchan);
				BKE_pose_channels_hash_free(obedit->pose);
				BLI_freelinkN(&obedit->pose->chanbase, pchan);
			}
			else {
				for (con = pchan->constraints.first; con; con = con->next) {
					bConstraintTypeInfo *cti = BKE_constraint_get_typeinfo(con);
					ListBase targets = {NULL, NULL};
					bConstraintTarget *ct;
					
					if (cti && cti->get_constraint_targets) {
						cti->get_constraint_targets(con, &targets);
						
						for (ct = targets.first; ct; ct = ct->next) {
							if (ct->tar == obedit) {
								if (ct->subtarget[0]) {
									curBone = ED_armature_bone_find_name(arm->edbo, ct->subtarget);
									if (curBone && (curBone->flag & BONE_SELECTED) && (arm->layer & curBone->layer)) {
										con->flag |= CONSTRAINT_DISABLE;
										ct->subtarget[0] = 0;
									}
								}
							}
						}
						
						if (cti->flush_constraint_targets)
							cti->flush_constraint_targets(con, &targets, 0);
					}
				}
			}
		}
	}
	
	
	for (curBone = arm->edbo->first; curBone; curBone = ebone_next) {
		ebone_next = curBone->next;
		if (arm->layer & curBone->layer) {
			if (curBone->flag & BONE_SELECTED) {
				if (curBone == arm->act_edbone) arm->act_edbone = NULL;
				ED_armature_edit_bone_remove(arm, curBone);
				changed = true;
			}
		}
	}
	
	if (!changed)
		return OPERATOR_CANCELLED;
	
	ED_armature_sync_selection(arm->edbo);

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_delete(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Delete Selected Bone(s)";
	ot->idname = "ARMATURE_OT_delete";
	ot->description = "Remove selected bones from the armature";
	
	/* api callbacks */
	ot->invoke = WM_operator_confirm;
	ot->exec = armature_delete_selected_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* ********************************* Show/Hide ******************************* */

static int armature_hide_exec(bContext *C, wmOperator *op)
{
	Object *obedit = CTX_data_edit_object(C);
	bArmature *arm = obedit->data;
	EditBone *ebone;
	const int invert = RNA_boolean_get(op->ptr, "unselected") ? BONE_SELECTED : 0;

	/* cancel if nothing selected */
	if (CTX_DATA_COUNT(C, selected_bones) == 0)
		return OPERATOR_CANCELLED;

	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (EBONE_VISIBLE(arm, ebone)) {
			if ((ebone->flag & BONE_SELECTED) != invert) {
				ebone->flag &= ~(BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
				ebone->flag |= BONE_HIDDEN_A;
			}
		}
	}
	ED_armature_validate_active(arm);
	ED_armature_sync_selection(arm->edbo);

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_hide(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Hide Selected Bones";
	ot->idname = "ARMATURE_OT_hide";
	ot->description = "Tag selected bones to not be visible in Edit Mode";
	
	/* api callbacks */
	ot->exec = armature_hide_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* props */
	RNA_def_boolean(ot->srna, "unselected", 0, "Unselected", "Hide unselected rather than selected");
}

static int armature_reveal_exec(bContext *C, wmOperator *UNUSED(op))
{
	Object *obedit = CTX_data_edit_object(C);
	bArmature *arm = obedit->data;
	EditBone *ebone;
	
	for (ebone = arm->edbo->first; ebone; ebone = ebone->next) {
		if (arm->layer & ebone->layer) {
			if (ebone->flag & BONE_HIDDEN_A) {
				ebone->flag |= (BONE_TIPSEL | BONE_SELECTED | BONE_ROOTSEL);
				ebone->flag &= ~BONE_HIDDEN_A;
			}
		}
	}
	ED_armature_validate_active(arm);
	ED_armature_sync_selection(arm->edbo);

	WM_event_add_notifier(C, NC_OBJECT | ND_BONE_SELECT, obedit);

	return OPERATOR_FINISHED;
}

void ARMATURE_OT_reveal(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Reveal Bones";
	ot->idname = "ARMATURE_OT_reveal";
	ot->description = "Unhide all bones that have been tagged to be hidden in Edit Mode";
	
	/* api callbacks */
	ot->exec = armature_reveal_exec;
	ot->poll = ED_operator_editarmature;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

}
