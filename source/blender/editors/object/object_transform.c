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
 * Contributor(s): Blender Foundation, 2002-2008 full recode
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/object/object_transform.c
 *  \ingroup edobj
 */


#include <stdlib.h>
#include <string.h>

#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_key_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_meta_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_group_types.h"
#include "DNA_lattice_types.h"

#include "BLI_math.h"
#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_curve.h"
#include "BKE_depsgraph.h"
#include "BKE_main.h"
#include "BKE_mball.h"
#include "BKE_mesh.h"
#include "BKE_object.h"
#include "BKE_report.h"
#include "BKE_tessmesh.h"
#include "BKE_multires.h"
#include "BKE_armature.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_armature.h"
#include "ED_keyframing.h"
#include "ED_mesh.h"
#include "ED_screen.h"
#include "ED_view3d.h"

#include "object_intern.h"

/*************************** Clear Transformation ****************************/

/* clear location of object */
static void object_clear_loc(Object *ob)
{
	/* clear location if not locked */
	if ((ob->protectflag & OB_LOCK_LOCX) == 0)
		ob->loc[0] = ob->dloc[0] = 0.0f;
	if ((ob->protectflag & OB_LOCK_LOCY) == 0)
		ob->loc[1] = ob->dloc[1] = 0.0f;
	if ((ob->protectflag & OB_LOCK_LOCZ) == 0)
		ob->loc[2] = ob->dloc[2] = 0.0f;
}

/* clear rotation of object */
static void object_clear_rot(Object *ob)
{
	/* clear rotations that aren't locked */
	if (ob->protectflag & (OB_LOCK_ROTX | OB_LOCK_ROTY | OB_LOCK_ROTZ | OB_LOCK_ROTW)) {
		if (ob->protectflag & OB_LOCK_ROT4D) {
			/* perform clamping on a component by component basis */
			if (ob->rotmode == ROT_MODE_AXISANGLE) {
				if ((ob->protectflag & OB_LOCK_ROTW) == 0)
					ob->rotAngle = ob->drotAngle = 0.0f;
				if ((ob->protectflag & OB_LOCK_ROTX) == 0)
					ob->rotAxis[0] = ob->drotAxis[0] = 0.0f;
				if ((ob->protectflag & OB_LOCK_ROTY) == 0)
					ob->rotAxis[1] = ob->drotAxis[1] = 0.0f;
				if ((ob->protectflag & OB_LOCK_ROTZ) == 0)
					ob->rotAxis[2] = ob->drotAxis[2] = 0.0f;
					
				/* check validity of axis - axis should never be 0,0,0 (if so, then we make it rotate about y) */
				if (IS_EQF(ob->rotAxis[0], ob->rotAxis[1]) && IS_EQF(ob->rotAxis[1], ob->rotAxis[2]))
					ob->rotAxis[1] = 1.0f;
				if (IS_EQF(ob->drotAxis[0], ob->drotAxis[1]) && IS_EQF(ob->drotAxis[1], ob->drotAxis[2]))
					ob->drotAxis[1] = 1.0f;
			}
			else if (ob->rotmode == ROT_MODE_QUAT) {
				if ((ob->protectflag & OB_LOCK_ROTW) == 0)
					ob->quat[0] = ob->dquat[0] = 1.0f;
				if ((ob->protectflag & OB_LOCK_ROTX) == 0)
					ob->quat[1] = ob->dquat[1] = 0.0f;
				if ((ob->protectflag & OB_LOCK_ROTY) == 0)
					ob->quat[2] = ob->dquat[2] = 0.0f;
				if ((ob->protectflag & OB_LOCK_ROTZ) == 0)
					ob->quat[3] = ob->dquat[3] = 0.0f;
					
				// TODO: does this quat need normalizing now?
			}
			else {
				/* the flag may have been set for the other modes, so just ignore the extra flag... */
				if ((ob->protectflag & OB_LOCK_ROTX) == 0)
					ob->rot[0] = ob->drot[0] = 0.0f;
				if ((ob->protectflag & OB_LOCK_ROTY) == 0)
					ob->rot[1] = ob->drot[1] = 0.0f;
				if ((ob->protectflag & OB_LOCK_ROTZ) == 0)
					ob->rot[2] = ob->drot[2] = 0.0f;
			}
		}
		else {
			/* perform clamping using euler form (3-components) */
			// FIXME: deltas are not handled for these cases yet...
			float eul[3], oldeul[3], quat1[4] = {0};
			
			if (ob->rotmode == ROT_MODE_QUAT) {
				copy_qt_qt(quat1, ob->quat);
				quat_to_eul(oldeul, ob->quat);
			}
			else if (ob->rotmode == ROT_MODE_AXISANGLE) {
				axis_angle_to_eulO(oldeul, EULER_ORDER_DEFAULT, ob->rotAxis, ob->rotAngle);
			}
			else {
				copy_v3_v3(oldeul, ob->rot);
			}
			
			eul[0] = eul[1] = eul[2] = 0.0f;
			
			if (ob->protectflag & OB_LOCK_ROTX)
				eul[0] = oldeul[0];
			if (ob->protectflag & OB_LOCK_ROTY)
				eul[1] = oldeul[1];
			if (ob->protectflag & OB_LOCK_ROTZ)
				eul[2] = oldeul[2];
			
			if (ob->rotmode == ROT_MODE_QUAT) {
				eul_to_quat(ob->quat, eul);
				/* quaternions flip w sign to accumulate rotations correctly */
				if ((quat1[0] < 0.0f && ob->quat[0] > 0.0f) || (quat1[0] > 0.0f && ob->quat[0] < 0.0f)) {
					mul_qt_fl(ob->quat, -1.0f);
				}
			}
			else if (ob->rotmode == ROT_MODE_AXISANGLE) {
				eulO_to_axis_angle(ob->rotAxis, &ob->rotAngle, eul, EULER_ORDER_DEFAULT);
			}
			else {
				copy_v3_v3(ob->rot, eul);
			}
		}
	}                        // Duplicated in source/blender/editors/armature/editarmature.c
	else { 
		if (ob->rotmode == ROT_MODE_QUAT) {
			unit_qt(ob->quat);
			unit_qt(ob->dquat);
		}
		else if (ob->rotmode == ROT_MODE_AXISANGLE) {
			unit_axis_angle(ob->rotAxis, &ob->rotAngle);
			unit_axis_angle(ob->drotAxis, &ob->drotAngle);
		}
		else {
			zero_v3(ob->rot);
			zero_v3(ob->drot);
		}
	}
}

/* clear scale of object */
static void object_clear_scale(Object *ob)
{
	/* clear scale factors which are not locked */
	if ((ob->protectflag & OB_LOCK_SCALEX) == 0) {
		ob->dscale[0] = 1.0f;
		ob->size[0] = 1.0f;
	}
	if ((ob->protectflag & OB_LOCK_SCALEY) == 0) {
		ob->dscale[1] = 1.0f;
		ob->size[1] = 1.0f;
	}
	if ((ob->protectflag & OB_LOCK_SCALEZ) == 0) {
		ob->dscale[2] = 1.0f;
		ob->size[2] = 1.0f;
	}
}

/* --------------- */

/* generic exec for clear-transform operators */
static int object_clear_transform_generic_exec(bContext *C, wmOperator *op, 
                                               void (*clear_func)(Object *), const char default_ksName[])
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	KeyingSet *ks;
	
	/* sanity checks */
	if (ELEM(NULL, clear_func, default_ksName)) {
		BKE_report(op->reports, RPT_ERROR, "Programming error: missing clear transform func or Keying Set Name");
		return OPERATOR_CANCELLED;
	}
	
	/* get KeyingSet to use */
	ks = ANIM_get_keyingset_for_autokeying(scene, default_ksName);
	
	/* operate on selected objects only if they aren't in weight-paint mode 
	 * (so that object-transform clearing won't be applied at same time as bone-clearing)
	 */
	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (!(ob->mode & OB_MODE_WEIGHT_PAINT)) {
			/* run provided clearing function */
			clear_func(ob);

			ED_autokeyframe_object(C, scene, ob, ks);

			/* tag for updates */
			DAG_id_tag_update(&ob->id, OB_RECALC_OB);
		}
	}
	CTX_DATA_END;
	
	/* this is needed so children are also updated */
	DAG_ids_flush_update(bmain, 0);

	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

	return OPERATOR_FINISHED;
}

/* --------------- */


static int object_location_clear_exec(bContext *C, wmOperator *op)
{
	return object_clear_transform_generic_exec(C, op, object_clear_loc, ANIM_KS_LOCATION_ID);
}

void OBJECT_OT_location_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Location";
	ot->description = "Clear the object's location";
	ot->idname = "OBJECT_OT_location_clear";
	
	/* api callbacks */
	ot->exec = object_location_clear_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int object_rotation_clear_exec(bContext *C, wmOperator *op)
{
	return object_clear_transform_generic_exec(C, op, object_clear_rot, ANIM_KS_ROTATION_ID);
}

void OBJECT_OT_rotation_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Rotation";
	ot->description = "Clear the object's rotation";
	ot->idname = "OBJECT_OT_rotation_clear";
	
	/* api callbacks */
	ot->exec = object_rotation_clear_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int object_scale_clear_exec(bContext *C, wmOperator *op)
{
	return object_clear_transform_generic_exec(C, op, object_clear_scale, ANIM_KS_SCALING_ID);
}

void OBJECT_OT_scale_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Scale";
	ot->description = "Clear the object's scale";
	ot->idname = "OBJECT_OT_scale_clear";
	
	/* api callbacks */
	ot->exec = object_scale_clear_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/* --------------- */

static int object_origin_clear_exec(bContext *C, wmOperator *UNUSED(op))
{
	Main *bmain = CTX_data_main(C);
	float *v1, *v3;
	float mat[3][3];

	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if (ob->parent) {
			/* vectors pointed to by v1 and v3 will get modified */
			v1 = ob->loc;
			v3 = ob->parentinv[3];
			
			copy_m3_m4(mat, ob->parentinv);
			negate_v3_v3(v3, v1);
			mul_m3_v3(mat, v3);
		}

		DAG_id_tag_update(&ob->id, OB_RECALC_OB);
	}
	CTX_DATA_END;

	DAG_ids_flush_update(bmain, 0);
	
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	
	return OPERATOR_FINISHED;
}

void OBJECT_OT_origin_clear(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Clear Origin";
	ot->description = "Clear the object's origin";
	ot->idname = "OBJECT_OT_origin_clear";
	
	/* api callbacks */
	ot->exec = object_origin_clear_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/*************************** Apply Transformation ****************************/

/* use this when the loc/size/rot of the parent has changed but the children
* should stay in the same place, e.g. for apply-size-rot or object center */
static void ignore_parent_tx(Main *bmain, Scene *scene, Object *ob)
{
	Object workob;
	Object *ob_child;
	
	/* a change was made, adjust the children to compensate */
	for (ob_child = bmain->object.first; ob_child; ob_child = ob_child->id.next) {
		if (ob_child->parent == ob) {
			BKE_object_apply_mat4(ob_child, ob_child->obmat, TRUE, FALSE);
			BKE_object_workob_calc_parent(scene, ob_child, &workob);
			invert_m4_m4(ob_child->parentinv, workob.obmat);
		}
	}
}

static int apply_objects_internal(bContext *C, ReportList *reports, int apply_loc, int apply_rot, int apply_scale)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	float rsmat[3][3], tmat[3][3], obmat[3][3], iobmat[3][3], mat[4][4], scale;
	int a, change = 0;
	
	/* first check if we can execute */
	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{

		if (ob->type == OB_MESH) {
			if (ID_REAL_USERS(ob->data) > 1) {
				BKE_report(reports, RPT_ERROR, "Can't apply to a multi user mesh, doing nothing");
				return OPERATOR_CANCELLED;
			}
		}
		else if (ob->type == OB_ARMATURE) {
			if (ID_REAL_USERS(ob->data) > 1) {
				BKE_report(reports, RPT_ERROR, "Can't apply to a multi user armature, doing nothing");
				return OPERATOR_CANCELLED;
			}
		}
		else if (ob->type == OB_LATTICE) {
			if (ID_REAL_USERS(ob->data) > 1) {
				BKE_report(reports, RPT_ERROR, "Can't apply to a multi user lattice, doing nothing");
				return OPERATOR_CANCELLED;
			}
		}
		else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
			Curve *cu;

			if (ID_REAL_USERS(ob->data) > 1) {
				BKE_report(reports, RPT_ERROR, "Can't apply to a multi user curve, doing nothing");
				return OPERATOR_CANCELLED;
			}

			cu = ob->data;

			if (!(cu->flag & CU_3D) && (apply_rot || apply_loc)) {
				BKE_report(reports, RPT_ERROR, "Neither rotation nor location could be applied to a 2d curve, doing nothing");
				return OPERATOR_CANCELLED;
			}
			if (cu->key) {
				BKE_report(reports, RPT_ERROR, "Can't apply to a curve with vertex keys, doing nothing");
				return OPERATOR_CANCELLED;
			}
		}
	}
	CTX_DATA_END;
	
	/* now execute */
	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{

		/* calculate rotation/scale matrix */
		if (apply_scale && apply_rot)
			BKE_object_to_mat3(ob, rsmat);
		else if (apply_scale)
			BKE_object_scale_to_mat3(ob, rsmat);
		else if (apply_rot) {
			float tmat[3][3], timat[3][3];

			/* simple rotation matrix */
			BKE_object_rot_to_mat3(ob, rsmat);

			/* correct for scale, note mul_m3_m3m3 has swapped args! */
			BKE_object_scale_to_mat3(ob, tmat);
			invert_m3_m3(timat, tmat);
			mul_m3_m3m3(rsmat, timat, rsmat);
			mul_m3_m3m3(rsmat, rsmat, tmat);
		}
		else
			unit_m3(rsmat);

		copy_m4_m3(mat, rsmat);

		/* calculate translation */
		if (apply_loc) {
			copy_v3_v3(mat[3], ob->loc);

			if (!(apply_scale && apply_rot)) {
				/* correct for scale and rotation that is still applied */
				BKE_object_to_mat3(ob, obmat);
				invert_m3_m3(iobmat, obmat);
				mul_m3_m3m3(tmat, rsmat, iobmat);
				mul_m3_v3(tmat, mat[3]);
			}
		}

		/* apply to object data */
		if (ob->type == OB_MESH) {
			Mesh *me = ob->data;
			MVert *mvert;

			multiresModifier_scale_disp(scene, ob);
			
			/* adjust data */
			mvert = me->mvert;
			for (a = 0; a < me->totvert; a++, mvert++)
				mul_m4_v3(mat, mvert->co);
			
			if (me->key) {
				KeyBlock *kb;
				
				for (kb = me->key->block.first; kb; kb = kb->next) {
					float *fp = kb->data;
					
					for (a = 0; a < kb->totelem; a++, fp += 3)
						mul_m4_v3(mat, fp);
				}
			}
			
			/* update normals */
			BKE_mesh_calc_normals_mapping(me->mvert, me->totvert, me->mloop, me->mpoly, me->totloop, me->totpoly, NULL, NULL, 0, NULL, NULL);
		}
		else if (ob->type == OB_ARMATURE) {
			ED_armature_apply_transform(ob, mat);
		}
		else if (ob->type == OB_LATTICE) {
			Lattice *lt = ob->data;
			BPoint *bp = lt->def;
			int a = lt->pntsu * lt->pntsv * lt->pntsw;
			
			while (a--) {
				mul_m4_v3(mat, bp->vec);
				bp++;
			}
		}
		else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
			Curve *cu = ob->data;

			Nurb *nu;
			BPoint *bp;
			BezTriple *bezt;

			scale = mat3_to_scale(rsmat);

			for (nu = cu->nurb.first; nu; nu = nu->next) {
				if (nu->type == CU_BEZIER) {
					a = nu->pntsu;
					for (bezt = nu->bezt; a--; bezt++) {
						mul_m4_v3(mat, bezt->vec[0]);
						mul_m4_v3(mat, bezt->vec[1]);
						mul_m4_v3(mat, bezt->vec[2]);
						bezt->radius *= scale;
					}
					BKE_nurb_handles_calc(nu);
				}
				else {
					a = nu->pntsu * nu->pntsv;
					for (bp = nu->bp; a--; bp++)
						mul_m4_v3(mat, bp->vec);
				}
			}
		}
		else
			continue;

		if (apply_loc)
			zero_v3(ob->loc);
		if (apply_scale)
			ob->size[0] = ob->size[1] = ob->size[2] = 1.0f;
		if (apply_rot) {
			zero_v3(ob->rot);
			unit_qt(ob->quat);
			unit_axis_angle(ob->rotAxis, &ob->rotAngle);
		}

		BKE_object_where_is_calc(scene, ob);
		if (ob->type == OB_ARMATURE) {
			BKE_pose_where_is(scene, ob); /* needed for bone parents */
		}

		ignore_parent_tx(bmain, scene, ob);

		DAG_id_tag_update(&ob->id, OB_RECALC_OB | OB_RECALC_DATA);

		change = 1;
	}
	CTX_DATA_END;

	if (!change)
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	return OPERATOR_FINISHED;
}

static int visual_transform_apply_exec(bContext *C, wmOperator *UNUSED(op))
{
	Scene *scene = CTX_data_scene(C);
	int change = 0;
	
	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		BKE_object_where_is_calc(scene, ob);
		BKE_object_apply_mat4(ob, ob->obmat, TRUE, TRUE);
		BKE_object_where_is_calc(scene, ob);

		/* update for any children that may get moved */
		DAG_id_tag_update(&ob->id, OB_RECALC_OB);
	
		change = 1;
	}
	CTX_DATA_END;

	if (!change)
		return OPERATOR_CANCELLED;

	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	return OPERATOR_FINISHED;
}

void OBJECT_OT_visual_transform_apply(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Apply Visual Transform";
	ot->description = "Apply the object's visual transformation to its data";
	ot->idname = "OBJECT_OT_visual_transform_apply";
	
	/* api callbacks */
	ot->exec = visual_transform_apply_exec;
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

static int object_transform_apply_exec(bContext *C, wmOperator *op)
{
	const int loc = RNA_boolean_get(op->ptr, "location");
	const int rot = RNA_boolean_get(op->ptr, "rotation");
	const int sca = RNA_boolean_get(op->ptr, "scale");

	if (loc || rot || sca) {
		return apply_objects_internal(C, op->reports, loc, rot, sca);
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

void OBJECT_OT_transform_apply(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Apply Object Transform";
	ot->description = "Apply the object's transformation to its data";
	ot->idname = "OBJECT_OT_transform_apply";

	/* api callbacks */
	ot->exec = object_transform_apply_exec;
	ot->poll = ED_operator_objectmode;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

	RNA_def_boolean(ot->srna, "location", 0, "Location", "");
	RNA_def_boolean(ot->srna, "rotation", 0, "Rotation", "");
	RNA_def_boolean(ot->srna, "scale", 0, "Scale", "");
}

/********************* Set Object Center ************************/

enum {
	GEOMETRY_TO_ORIGIN = 0,
	ORIGIN_TO_GEOMETRY,
	ORIGIN_TO_CURSOR
};

static int object_origin_set_exec(bContext *C, wmOperator *op)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	Object *tob;
	float cursor[3], cent[3], cent_neg[3], centn[3], min[3], max[3];
	int centermode = RNA_enum_get(op->ptr, "type");
	int around = RNA_enum_get(op->ptr, "center"); /* initialized from v3d->around */

	/* keep track of what is changed */
	int tot_change = 0, tot_lib_error = 0, tot_multiuser_arm_error = 0;

	if (obedit && centermode != GEOMETRY_TO_ORIGIN) {
		BKE_report(op->reports, RPT_ERROR, "Operation cannot be performed in EditMode");
		return OPERATOR_CANCELLED;
	}
	else {
		/* get the view settings if 'around' isn't set and the view is available */
		View3D *v3d = CTX_wm_view3d(C);
		copy_v3_v3(cursor, give_cursor(scene, v3d));
		if (v3d && !RNA_struct_property_is_set(op->ptr, "center"))
			around = v3d->around;
	}

	zero_v3(cent);

	if (obedit) {
		INIT_MINMAX(min, max);

		if (obedit->type == OB_MESH) {
			Mesh *me = obedit->data;
			BMEditMesh *em = me->edit_btmesh;
			BMVert *eve;
			BMIter iter;
			int total = 0;
			
			if (centermode == ORIGIN_TO_CURSOR) {
				copy_v3_v3(cent, cursor);
				invert_m4_m4(obedit->imat, obedit->obmat);
				mul_m4_v3(obedit->imat, cent);
			}
			else {
				BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
					if (around == V3D_CENTROID) {
						total++;
						add_v3_v3(cent, eve->co);
						mul_v3_fl(cent, 1.0f / (float)total);
					}
					else {
						DO_MINMAX(eve->co, min, max);
						mid_v3_v3v3(cent, min, max);
					}
				}
			}
			
			BM_ITER_MESH (eve, &iter, em->bm, BM_VERTS_OF_MESH) {
				sub_v3_v3(eve->co, cent);
			}

			EDBM_mesh_normals_update(em);
			tot_change++;
			DAG_id_tag_update(&obedit->id, OB_RECALC_DATA);
		}
	}

	/* reset flags */
	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		ob->flag &= ~OB_DONE;
	}
	CTX_DATA_END;

	for (tob = bmain->object.first; tob; tob = tob->id.next) {
		if (tob->data)
			((ID *)tob->data)->flag &= ~LIB_DOIT;
		if (tob->dup_group)
			((ID *)tob->dup_group)->flag &= ~LIB_DOIT;
	}

	CTX_DATA_BEGIN (C, Object *, ob, selected_editable_objects)
	{
		if ((ob->flag & OB_DONE) == 0) {
			int do_inverse_offset = FALSE;
			ob->flag |= OB_DONE;

			if (centermode == ORIGIN_TO_CURSOR) {
				copy_v3_v3(cent, cursor);
				invert_m4_m4(ob->imat, ob->obmat);
				mul_m4_v3(ob->imat, cent);
			}
			
			if (ob->data == NULL) {
				/* special support for dupligroups */
				if ((ob->transflag & OB_DUPLIGROUP) && ob->dup_group && (ob->dup_group->id.flag & LIB_DOIT) == 0) {
					if (ob->dup_group->id.lib) {
						tot_lib_error++;
					}
					else {
						if (centermode == ORIGIN_TO_CURSOR) { /* done */ }
						else {
							/* only bounds support */
							INIT_MINMAX(min, max);
							BKE_object_minmax_dupli(scene, ob, min, max);
							mid_v3_v3v3(cent, min, max);
							invert_m4_m4(ob->imat, ob->obmat);
							mul_m4_v3(ob->imat, cent);
						}
						
						add_v3_v3(ob->dup_group->dupli_ofs, cent);

						tot_change++;
						ob->dup_group->id.flag |= LIB_DOIT;
						do_inverse_offset = TRUE;
					}
				}
			}
			else if (((ID *)ob->data)->lib) {
				tot_lib_error++;
			}

			if (obedit == NULL && ob->type == OB_MESH) {
				Mesh *me = ob->data;

				if (centermode == ORIGIN_TO_CURSOR) { /* done */ }
				else if (around == V3D_CENTROID) { BKE_mesh_center_median(me, cent); }
				else { BKE_mesh_center_bounds(me, cent); }

				negate_v3_v3(cent_neg, cent);
				BKE_mesh_translate(me, cent_neg, 1);

				tot_change++;
				me->id.flag |= LIB_DOIT;
				do_inverse_offset = TRUE;
			}
			else if (ELEM(ob->type, OB_CURVE, OB_SURF)) {
				Curve *cu = ob->data;

				if (centermode == ORIGIN_TO_CURSOR) { /* done */ }
				else if (around == V3D_CENTROID) { BKE_curve_center_median(cu, cent); }
				else { BKE_curve_center_bounds(cu, cent);   }

				/* don't allow Z change if curve is 2D */
				if ((ob->type == OB_CURVE) && !(cu->flag & CU_3D))
					cent[2] = 0.0;

				negate_v3_v3(cent_neg, cent);
				BKE_curve_translate(cu, cent_neg, 1);

				tot_change++;
				cu->id.flag |= LIB_DOIT;
				do_inverse_offset = TRUE;

				if (obedit) {
					if (centermode == GEOMETRY_TO_ORIGIN) {
						DAG_id_tag_update(&obedit->id, OB_RECALC_DATA);
					}
					break;
				}
			}
			else if (ob->type == OB_FONT) {
				/* get from bb */

				Curve *cu = ob->data;

				if (cu->bb == NULL && (centermode != ORIGIN_TO_CURSOR)) {
					/* do nothing*/
				}
				else {
					if (centermode == ORIGIN_TO_CURSOR) {
						/* done */
					}
					else {
						cent[0] = 0.5f * (cu->bb->vec[4][0] + cu->bb->vec[0][0]);
						cent[1] = 0.5f * (cu->bb->vec[0][1] + cu->bb->vec[2][1]) - 0.5f;    /* extra 0.5 is the height o above line */
					}

					cent[2] = 0.0f;

					cu->xof = cu->xof - (cent[0] / cu->fsize);
					cu->yof = cu->yof - (cent[1] / cu->fsize);

					tot_change++;
					cu->id.flag |= LIB_DOIT;
					do_inverse_offset = TRUE;
				}
			}
			else if (ob->type == OB_ARMATURE) {
				bArmature *arm = ob->data;

				if (ID_REAL_USERS(arm) > 1) {
#if 0
					BKE_report(op->reports, RPT_ERROR, "Can't apply to a multi user armature");
					return;
#endif
					tot_multiuser_arm_error++;
				}
				else {
					/* Function to recenter armatures in editarmature.c
					 * Bone + object locations are handled there.
					 */
					docenter_armature(scene, ob, cursor, centermode, around);

					tot_change++;
					arm->id.flag |= LIB_DOIT;
					/* do_inverse_offset= TRUE; */ /* docenter_armature() handles this */

					BKE_object_where_is_calc(scene, ob);
					BKE_pose_where_is(scene, ob); /* needed for bone parents */

					ignore_parent_tx(bmain, scene, ob);

					if (obedit)
						break;
				}
			}
			else if (ob->type == OB_MBALL) {
				MetaBall *mb = ob->data;

				if (centermode == ORIGIN_TO_CURSOR) { /* done */ }
				else if (around == V3D_CENTROID) { BKE_metaball_center_median(mb, cent); }
				else { BKE_metaball_center_bounds(mb, cent);    }

				negate_v3_v3(cent_neg, cent);
				BKE_metaball_translate(mb, cent_neg);

				tot_change++;
				mb->id.flag |= LIB_DOIT;
				do_inverse_offset = TRUE;

				if (obedit) {
					if (centermode == GEOMETRY_TO_ORIGIN) {
						DAG_id_tag_update(&obedit->id, OB_RECALC_DATA);
					}
					break;
				}
			}

			/* offset other selected objects */
			if (do_inverse_offset && (centermode != GEOMETRY_TO_ORIGIN)) {
				/* was the object data modified
				 * note: the functions above must set 'cent' */
				copy_v3_v3(centn, cent);
				mul_mat3_m4_v3(ob->obmat, centn); /* ommit translation part */
				add_v3_v3(ob->loc, centn);

				BKE_object_where_is_calc(scene, ob);
				if (ob->type == OB_ARMATURE) {
					BKE_pose_where_is(scene, ob); /* needed for bone parents */
				}

				ignore_parent_tx(bmain, scene, ob);
				
				/* other users? */
				CTX_DATA_BEGIN (C, Object *, ob_other, selected_editable_objects)
				{
					if ((ob_other->flag & OB_DONE) == 0 &&
					    ((ob->data && (ob->data == ob_other->data)) ||
					     (ob->dup_group == ob_other->dup_group &&
					      (ob->transflag | ob_other->transflag) & OB_DUPLIGROUP)))
					{
						ob_other->flag |= OB_DONE;
						DAG_id_tag_update(&ob_other->id, OB_RECALC_OB | OB_RECALC_DATA);

						copy_v3_v3(centn, cent);
						mul_mat3_m4_v3(ob_other->obmat, centn); /* ommit translation part */
						add_v3_v3(ob_other->loc, centn);

						BKE_object_where_is_calc(scene, ob_other);
						if (ob_other->type == OB_ARMATURE) {
							BKE_pose_where_is(scene, ob_other); /* needed for bone parents */
						}
						ignore_parent_tx(bmain, scene, ob_other);
					}
				}
				CTX_DATA_END;
			}
		}
	}
	CTX_DATA_END;

	for (tob = bmain->object.first; tob; tob = tob->id.next)
		if (tob->data && (((ID *)tob->data)->flag & LIB_DOIT))
			DAG_id_tag_update(&tob->id, OB_RECALC_OB | OB_RECALC_DATA);

	if (tot_change) {
		DAG_ids_flush_update(bmain, 0);
		WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);
	}

	/* Warn if any errors occurred */
	if (tot_lib_error + tot_multiuser_arm_error) {
		BKE_reportf(op->reports, RPT_WARNING, "%i Object(s) Not Centered, %i Changed:", tot_lib_error + tot_multiuser_arm_error, tot_change);
		if (tot_lib_error)
			BKE_reportf(op->reports, RPT_WARNING, "|%i linked library objects", tot_lib_error);
		if (tot_multiuser_arm_error)
			BKE_reportf(op->reports, RPT_WARNING, "|%i multiuser armature object(s)", tot_multiuser_arm_error);
	}

	return OPERATOR_FINISHED;
}

void OBJECT_OT_origin_set(wmOperatorType *ot)
{
	static EnumPropertyItem prop_set_center_types[] = {
		{GEOMETRY_TO_ORIGIN, "GEOMETRY_ORIGIN", 0, "Geometry to Origin", "Move object geometry to object origin"},
		{ORIGIN_TO_GEOMETRY, "ORIGIN_GEOMETRY", 0, "Origin to Geometry", "Move object origin to center of object geometry"},
		{ORIGIN_TO_CURSOR, "ORIGIN_CURSOR", 0, "Origin to 3D Cursor", "Move object origin to position of the 3d cursor"},
		{0, NULL, 0, NULL, NULL}
	};
	
	static EnumPropertyItem prop_set_bounds_types[] = {
		{V3D_CENTROID, "MEDIAN", 0, "Median Center", ""},
		{V3D_CENTER, "BOUNDS", 0, "Bounds Center", ""},
		{0, NULL, 0, NULL, NULL}
	};
	
	/* identifiers */
	ot->name = "Set Origin";
	ot->description = "Set the object's origin, by either moving the data, or set to center of data, or use 3d cursor";
	ot->idname = "OBJECT_OT_origin_set";
	
	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = object_origin_set_exec;
	
	ot->poll = ED_operator_scene_editable;
	
	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
	
	ot->prop = RNA_def_enum(ot->srna, "type", prop_set_center_types, 0, "Type", "");
	RNA_def_enum(ot->srna, "center", prop_set_bounds_types, V3D_CENTROID, "Center", "");
}

