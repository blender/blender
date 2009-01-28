/**
 * $Id: transform_ops.c 17542 2008-11-23 15:27:53Z theeth $
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#include "MEM_guardedalloc.h"

#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "BLI_arithb.h"

#include "BKE_utildefines.h"
#include "BKE_context.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "ED_screen.h"

#include "transform.h"


static int select_orientation_exec(bContext *C, wmOperator *op)
{
	int orientation = RNA_int_get(op->ptr, "orientation");
	
	if (orientation > -1)
	{
		BIF_selectTransformOrientationValue(C, orientation);
		return OPERATOR_FINISHED;
	}
	else
	{
		return OPERATOR_CANCELLED;
	}
}

static int select_orientation_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	char *string = BIF_menustringTransformOrientation(C, "Orientation");
	
	op->customdata = string;
	
	uiPupmenuOperator(C, 0, op, "orientation", string);
	
	return OPERATOR_RUNNING_MODAL;
}
	
void TFM_OT_select_orientation(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Select Orientation";
	ot->idname = "TFM_OT_select_orientation";

	/* api callbacks */
	ot->invoke = select_orientation_invoke;
	ot->exec   = select_orientation_exec;
	ot->poll   = ED_operator_areaactive;

	RNA_def_int(ot->srna, "orientation", -1, INT_MIN, INT_MAX, "Orientation", "DOC_BROKEN", INT_MIN, INT_MAX);
}

static void transformops_exit(bContext *C, wmOperator *op)
{
	saveTransform(C, op->customdata, op);
	MEM_freeN(op->customdata);
	op->customdata = NULL;
}

static void transformops_data(bContext *C, wmOperator *op, wmEvent *event)
{
	if (op->customdata == NULL)
	{
		TransInfo *t = MEM_callocN(sizeof(TransInfo), "TransInfo data");
		
		initTransform(C, t, op, event);
	
		/* store data */
		op->customdata = t;
	}
}

static int transform_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	int exit_code;
	
	TransInfo *t = op->customdata;
	
	transformEvent(t, event);
	
	transformApply(C, t);
	
	
	exit_code = transformEnd(C, t);
	
	if (exit_code != OPERATOR_RUNNING_MODAL)
	{
		transformops_exit(C, op);
	}

	return exit_code;
}

static int transform_cancel(bContext *C, wmOperator *op)
{
	TransInfo *t = op->customdata;
	
	t->state = TRANS_CANCEL;
	transformEnd(C, t);
	transformops_exit(C, op);
	
	return OPERATOR_CANCELLED;
}

static int transform_exec(bContext *C, wmOperator *op)
{
	TransInfo *t;

	transformops_data(C, op, NULL);

	t = op->customdata;

	t->options |= CTX_AUTOCONFIRM;

	transformApply(C, t);
	
	transformEnd(C, t);

	transformops_exit(C, op);
	
	return OPERATOR_FINISHED;
}

static int transform_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	transformops_data(C, op, event);

	if(RNA_property_is_set(op->ptr, "values")) {
		return transform_exec(C, op);
	}
	else {
		/* add temp handler */
		WM_event_add_modal_handler(C, &CTX_wm_window(C)->handlers, op);

		return OPERATOR_RUNNING_MODAL;
	}
}

void TFM_OT_transform(struct wmOperatorType *ot)
{
	static const float mtx[3][3] = {{1, 0, 0},{0, 1, 0},{0, 0, 1}};
	static EnumPropertyItem transform_mode_types[] = {
			{TFM_INIT, "INIT", "Init", ""},
			{TFM_DUMMY, "DUMMY", "Dummy", ""},
			{TFM_TRANSLATION, "TRANSLATION", "Translation", ""},
			{TFM_ROTATION, "ROTATION", "Rotation", ""},
			{TFM_RESIZE, "RESIZE", "Resize", ""},
			{TFM_TOSPHERE, "TOSPHERE", "Tosphere", ""},
			{TFM_SHEAR, "SHEAR", "Shear", ""},
			{TFM_WARP, "WARP", "Warp", ""},
			{TFM_SHRINKFATTEN, "SHRINKFATTEN", "Shrinkfatten", ""},
			{TFM_TILT, "TILT", "Tilt", ""},
			{TFM_LAMP_ENERGY, "LAMP_ENERGY", "Lamp_Energy", ""},
			{TFM_TRACKBALL, "TRACKBALL", "Trackball", ""},
			{TFM_PUSHPULL, "PUSHPULL", "Pushpull", ""},
			{TFM_CREASE, "CREASE", "Crease", ""},
			{TFM_MIRROR, "MIRROR", "Mirror", ""},
			{TFM_BONESIZE, "BONESIZE", "Bonesize", ""},
			{TFM_BONE_ENVELOPE, "BONE_ENVELOPE", "Bone_Envelope", ""},
			{TFM_CURVE_SHRINKFATTEN, "CURVE_SHRINKFATTEN", "Curve_Shrinkfatten", ""},
			{TFM_BONE_ROLL, "BONE_ROLL", "Bone_Roll", ""},
			{TFM_TIME_TRANSLATE, "TIME_TRANSLATE", "Time_Translate", ""},
			{TFM_TIME_SLIDE, "TIME_SLIDE", "Time_Slide", ""},
			{TFM_TIME_SCALE, "TIME_SCALE", "Time_Scale", ""},
			{TFM_TIME_EXTEND, "TIME_EXTEND", "Time_Extend", ""},
			{TFM_BAKE_TIME, "BAKE_TIME", "Bake_Time", ""},
			{TFM_BEVEL, "BEVEL", "Bevel", ""},
			{TFM_BWEIGHT, "BWEIGHT", "Bweight", ""},
			{TFM_ALIGN, "ALIGN", "Align", ""},
			{0, NULL, NULL, NULL}
	};
	
	/* identifiers */
	ot->name   = "Transform";
	ot->idname = "TFM_OT_transform";
	ot->flag= OPTYPE_REGISTER;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_areaactive;

	RNA_def_enum(ot->srna, "mode", transform_mode_types, 0, "Mode", "");
	RNA_def_int(ot->srna, "options", 0, INT_MIN, INT_MAX, "Options", "", INT_MIN, INT_MAX);
	
	RNA_def_float_vector(ot->srna, "values", 4, NULL, -FLT_MAX, FLT_MAX, "Values", "", -FLT_MAX, FLT_MAX);

	RNA_def_int(ot->srna, "constraint_orientation", 0, INT_MIN, INT_MAX, "Constraint Orientation", "", INT_MIN, INT_MAX);
	RNA_def_int(ot->srna, "constraint_mode", 0, INT_MIN, INT_MAX, "Constraint Mode", "", INT_MIN, INT_MAX);

	RNA_def_float_matrix(ot->srna, "constraint_matrix", 9, mtx[0], -FLT_MAX, FLT_MAX, "Constraint Matrix", "", -FLT_MAX, FLT_MAX);
}

void transform_operatortypes(void)
{
	WM_operatortype_append(TFM_OT_transform);
	WM_operatortype_append(TFM_OT_select_orientation);
}
 
void transform_keymap_for_space(struct wmWindowManager *wm, struct ListBase *keymap, int spaceid)
{
	wmKeymapItem *km;
	switch(spaceid)
	{
		case SPACE_VIEW3D:
			km = WM_keymap_add_item(keymap, "TFM_OT_transform", GKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TRANSLATION);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EVT_TWEAK_S, KM_ANY, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TRANSLATION);
			
			km = WM_keymap_add_item(keymap, "TFM_OT_transform", RKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_ROTATION);

			km = WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_RESIZE);

			km = WM_keymap_add_item(keymap, "TFM_OT_transform", WKEY, KM_PRESS, KM_SHIFT, 0);
			RNA_int_set(km->ptr, "mode", TFM_WARP);

			km = WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, KM_CTRL|KM_SHIFT, 0);
			RNA_int_set(km->ptr, "mode", TFM_TOSPHERE);
			
			km = WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, KM_ALT|KM_CTRL|KM_SHIFT, 0);
			RNA_int_set(km->ptr, "mode", TFM_SHEAR);
			
			km = WM_keymap_add_item(keymap, "TFM_OT_select_orientation", SPACEKEY, KM_PRESS, KM_ALT, 0);

			break;
		case SPACE_ACTION:
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", GKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_TRANSLATE);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EVT_TWEAK_S, KM_ANY, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_TRANSLATE);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_EXTEND);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_SCALE);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", TKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_SLIDE);
			break;
		case SPACE_IPO:
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", GKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TRANSLATION);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EVT_TWEAK_S, KM_ANY, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TRANSLATION);
			
				// XXX the 'mode' identifier here is not quite right
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_EXTEND);
			
			km = WM_keymap_add_item(keymap, "TFM_OT_transform", RKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_ROTATION);
			
			km = WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_RESIZE);
			break;
		case SPACE_NODE:
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", GKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TRANSLATION);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EVT_TWEAK_A, KM_ANY, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TRANSLATION);
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EVT_TWEAK_S, KM_ANY, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TRANSLATION);
			
			km = WM_keymap_add_item(keymap, "TFM_OT_transform", RKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_ROTATION);
			
			km = WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_RESIZE);
			break;
		case SPACE_SEQ:
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", GKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TRANSLATION);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EVT_TWEAK_S, KM_ANY, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TRANSLATION);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_EXTEND);
			break;
		default:
			break;
	}
}
