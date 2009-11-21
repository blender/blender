/**
 * $Id$
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

#include "DNA_scene_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BLI_math.h"

#include "BKE_utildefines.h"
#include "BKE_context.h"
#include "BKE_global.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "ED_screen.h"

#include "transform.h"

typedef struct TransformModeItem
{
	char *idname;
	int		mode;
} TransformModeItem;

static float VecOne[3] = {1, 1, 1};

char OP_TRANSLATION[] = "TFM_OT_translate";
char OP_ROTATION[] = "TFM_OT_rotate";
char OP_TOSPHERE[] = "TFM_OT_tosphere";
char OP_RESIZE[] = "TFM_OT_resize";
char OP_SHEAR[] = "TFM_OT_shear";
char OP_WARP[] = "TFM_OT_warp";
char OP_SHRINK_FATTEN[] = "TFM_OT_shrink_fatten";
char OP_TILT[] = "TFM_OT_tilt";
char OP_TRACKBALL[] = "TFM_OT_trackball";
char OP_MIRROR[] = "TFM_OT_mirror";
char OP_EDGE_SLIDE[] = "TFM_OT_edge_slide";


TransformModeItem transform_modes[] =
{
	{OP_TRANSLATION, TFM_TRANSLATION},
	{OP_ROTATION, TFM_ROTATION},
	{OP_TOSPHERE, TFM_TOSPHERE},
	{OP_RESIZE, TFM_RESIZE},
	{OP_SHEAR, TFM_SHEAR},
	{OP_WARP, TFM_WARP},
	{OP_SHRINK_FATTEN, TFM_SHRINKFATTEN},
	{OP_TILT, TFM_TILT},
	{OP_TRACKBALL, TFM_TRACKBALL},
	{OP_MIRROR, TFM_MIRROR},
	{OP_EDGE_SLIDE, TFM_EDGE_SLIDE},
	{NULL, 0}
};

static int select_orientation_exec(bContext *C, wmOperator *op)
{
	int orientation = RNA_enum_get(op->ptr, "orientation");

	BIF_selectTransformOrientationValue(C, orientation);
	
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_VIEW3D, CTX_wm_view3d(C));

	return OPERATOR_FINISHED;
}

static int select_orientation_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, "Orientation", 0);
	layout= uiPupMenuLayout(pup);
	uiItemsEnumO(layout, "TFM_OT_select_orientation", "orientation");
	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

void TFM_OT_select_orientation(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name   = "Select Orientation";
	ot->description= "Select transformation orientation.";
	ot->idname = "TFM_OT_select_orientation";
	ot->flag   = OPTYPE_UNDO;

	/* api callbacks */
	ot->invoke = select_orientation_invoke;
	ot->exec   = select_orientation_exec;
	ot->poll   = ED_operator_areaactive;

	prop= RNA_def_property(ot->srna, "orientation", PROP_ENUM, PROP_NONE);
	RNA_def_property_ui_text(prop, "Orientation", "Transformation orientation.");
	RNA_def_enum_funcs(prop, rna_TransformOrientation_itemf);
}


static int delete_orientation_exec(bContext *C, wmOperator *op)
{
	View3D *v3d = CTX_wm_view3d(C);
	int selected_index = (v3d->twmode - V3D_MANIP_CUSTOM);

	BIF_removeTransformOrientationIndex(C, selected_index);
	
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_VIEW3D, CTX_wm_view3d(C));

	return OPERATOR_FINISHED;
}

static int delete_orientation_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	return delete_orientation_exec(C, op);
}

static int delete_orientation_poll(bContext *C)
{
	int selected_index = -1;
	View3D *v3d = CTX_wm_view3d(C);
	
	if (ED_operator_areaactive(C) == 0)
		return 0;
	
	
	if(v3d) {
		selected_index = (v3d->twmode - V3D_MANIP_CUSTOM);
	}
	
	return selected_index >= 0;
}

void TFM_OT_delete_orientation(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Delete Orientation";
	ot->description= "Delete transformation orientation.";
	ot->idname = "TFM_OT_delete_orientation";
	ot->flag   = OPTYPE_UNDO;

	/* api callbacks */
	ot->invoke = delete_orientation_invoke;
	ot->exec   = delete_orientation_exec;
	ot->poll   = delete_orientation_poll;
}

static int create_orientation_exec(bContext *C, wmOperator *op)
{
	char name[36];
	int use = RNA_boolean_get(op->ptr, "use");
	int overwrite = RNA_boolean_get(op->ptr, "overwrite");
	
	RNA_string_get(op->ptr, "name", name);

	BIF_createTransformOrientation(C, op->reports, name, use, overwrite);

	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_VIEW3D, CTX_wm_view3d(C));
	
	return OPERATOR_FINISHED;
}

static int create_orientation_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	return create_orientation_exec(C, op);
}

void TFM_OT_create_orientation(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Create Orientation";
	ot->description= "Create transformation orientation from selection.";
	ot->idname = "TFM_OT_create_orientation";
	ot->flag   = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* api callbacks */
	ot->invoke = create_orientation_invoke;
	ot->exec   = create_orientation_exec;
	ot->poll   = ED_operator_areaactive;
	ot->flag   = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_string(ot->srna, "name", "", 35, "Name", "Text to insert at the cursor position.");
	RNA_def_boolean(ot->srna, "use", 0, "Use after creation", "Select orientation after its creation");
	RNA_def_boolean(ot->srna, "overwrite", 0, "Overwrite previous", "Overwrite previously created orientation with same name");
}

static void transformops_exit(bContext *C, wmOperator *op)
{
	saveTransform(C, op->customdata, op);
	MEM_freeN(op->customdata);
	op->customdata = NULL;
	G.moving = 0;
}

static int transformops_data(bContext *C, wmOperator *op, wmEvent *event)
{
	int retval = 1;
	if (op->customdata == NULL)
	{
		TransInfo *t = MEM_callocN(sizeof(TransInfo), "TransInfo data");
		TransformModeItem *tmode;
		int mode = -1;

		for (tmode = transform_modes; tmode->idname; tmode++)
		{
			if (op->type->idname == tmode->idname)
			{
				mode = tmode->mode;
			}
		}

		if (mode == -1)
		{
			mode = RNA_int_get(op->ptr, "mode");
		}

		retval = initTransform(C, t, op, event, mode);
		G.moving = 1;

		/* store data */
		op->customdata = t;
	}

	return retval; /* return 0 on error */
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

	if (!transformops_data(C, op, NULL))
	{
		G.moving = 0;
		return OPERATOR_CANCELLED;
	}

	t = op->customdata;

	t->options |= CTX_AUTOCONFIRM;

	transformApply(C, t);

	transformEnd(C, t);

	transformops_exit(C, op);

	return OPERATOR_FINISHED;
}

static int transform_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if (!transformops_data(C, op, event))
	{
		G.moving = 0;
		return OPERATOR_CANCELLED;
	}

	if(RNA_property_is_set(op->ptr, "value")) {
		return transform_exec(C, op);
	}
	else {
		TransInfo *t = op->customdata;

		/* add temp handler */
		WM_event_add_modal_handler(C, op);

		t->flag |= T_MODAL; // XXX meh maybe somewhere else

		op->flag |= OP_GRAB_POINTER; // XXX maybe we want this with the manipulator only?
		return OPERATOR_RUNNING_MODAL;
	}
}

void Properties_Proportional(struct wmOperatorType *ot)
{
	RNA_def_enum(ot->srna, "proportional", proportional_editing_items, 0, "Proportional Editing", "");
	RNA_def_enum(ot->srna, "proportional_editing_falloff", proportional_falloff_items, 0, "Proportional Editing Falloff", "Falloff type for proportional editing mode.");
	RNA_def_float(ot->srna, "proportional_size", 1, 0, FLT_MAX, "Proportional Size", "", 0, 100);
}

void Properties_Snapping(struct wmOperatorType *ot, short align)
{
	RNA_def_boolean(ot->srna, "snap", 0, "Snap to Point", "");
	RNA_def_enum(ot->srna, "snap_mode", snap_mode_items, 0, "Mode", "");
	RNA_def_float_vector(ot->srna, "snap_point", 3, NULL, -FLT_MAX, FLT_MAX, "Point", "", -FLT_MAX, FLT_MAX);

	if (align)
	{
		RNA_def_boolean(ot->srna, "snap_align", 0, "Align with Point Normal", "");
		RNA_def_float_vector(ot->srna, "snap_normal", 3, NULL, -FLT_MAX, FLT_MAX, "Normal", "", -FLT_MAX, FLT_MAX);
	}
}

void Properties_Constraints(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	RNA_def_boolean_vector(ot->srna, "constraint_axis", 3, NULL, "Constraint Axis", "");
	prop= RNA_def_property(ot->srna, "constraint_orientation", PROP_ENUM, PROP_NONE);
	RNA_def_property_ui_text(prop, "Orientation", "Transformation orientation.");
	RNA_def_enum_funcs(prop, rna_TransformOrientation_itemf);
}

void TFM_OT_translate(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Translate";
	ot->description= "Translate selected items.";
	ot->idname = OP_TRANSLATION;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_areaactive;

	RNA_def_float_vector(ot->srna, "value", 3, NULL, -FLT_MAX, FLT_MAX, "Vector", "", -FLT_MAX, FLT_MAX);

	Properties_Proportional(ot);

	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");

	Properties_Constraints(ot);

	Properties_Snapping(ot, 1);
}

void TFM_OT_resize(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Resize";
	ot->description= "Resize selected items."; 
	ot->idname = OP_RESIZE;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_areaactive;

	RNA_def_float_vector(ot->srna, "value", 3, VecOne, -FLT_MAX, FLT_MAX, "Vector", "", -FLT_MAX, FLT_MAX);

	Properties_Proportional(ot);

	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");

	Properties_Constraints(ot);

	Properties_Snapping(ot, 0);
}


void TFM_OT_trackball(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Trackball";
	ot->description= "Trackball style rotation of selected items.";
	ot->idname = OP_TRACKBALL;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_areaactive;

	RNA_def_float_vector(ot->srna, "value", 2, VecOne, -FLT_MAX, FLT_MAX, "angle", "", -FLT_MAX, FLT_MAX);

	Properties_Proportional(ot);

	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

void TFM_OT_rotate(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Rotate";
	ot->description= "Rotate selected items.";
	ot->idname = OP_ROTATION;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_areaactive;

	RNA_def_float_rotation(ot->srna, "value", 1, NULL, -FLT_MAX, FLT_MAX, "Angle", "", -M_PI*2, M_PI*2);

	Properties_Proportional(ot);

	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");

	Properties_Constraints(ot);

	Properties_Snapping(ot, 0);
}

void TFM_OT_tilt(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Tilt";
    /*optionals - 
        "Tilt selected vertices."
        "Specify an extra axis rotation for selected vertices of 3d curve." */
	ot->description= "Tilt selected control vertices of 3d curve."; 
	ot->idname = OP_TILT;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_editcurve;

	RNA_def_float_rotation(ot->srna, "value", 1, NULL, -FLT_MAX, FLT_MAX, "Angle", "", -M_PI*2, M_PI*2);

	Properties_Proportional(ot);

	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");

	Properties_Constraints(ot);
}

void TFM_OT_warp(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Warp";
	ot->description= "Warp selected items around the cursor.";
	ot->idname = OP_WARP;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_areaactive;

	RNA_def_float_rotation(ot->srna, "value", 1, NULL, -FLT_MAX, FLT_MAX, "Angle", "", 0, 1);

	Properties_Proportional(ot);

	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");

	// XXX Shear axis?
//	Properties_Constraints(ot);
}

void TFM_OT_shear(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Shear";
	ot->description= "Shear selected items along the horizontal screen axis.";
	ot->idname = OP_SHEAR;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_areaactive;

	RNA_def_float(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Offset", "", -FLT_MAX, FLT_MAX);

	Properties_Proportional(ot);

	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");

	// XXX Shear axis?
//	Properties_Constraints(ot);
}

void TFM_OT_shrink_fatten(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Shrink/Fatten";
	ot->description= "Shrink/fatten selected vertices along normals.";
	ot->idname = OP_SHRINK_FATTEN;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_editmesh;

	RNA_def_float(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Offset", "", -FLT_MAX, FLT_MAX);

	Properties_Proportional(ot);

	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

void TFM_OT_tosphere(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "To Sphere";
    //added "around mesh center" to differentiate between "MESH_OT_vertices_to_sphere()" 
	ot->description= "Move selected vertices outward in a spherical shape around mesh center.";
	ot->idname = OP_TOSPHERE;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_areaactive;

	RNA_def_float_factor(ot->srna, "value", 0, 0, 1, "Factor", "", 0, 1);

	Properties_Proportional(ot);

	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

void TFM_OT_mirror(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Mirror";
	ot->description= "Mirror selected vertices around one or more axes.";
	ot->idname = OP_MIRROR;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_areaactive;

	Properties_Proportional(ot);
	Properties_Constraints(ot);
}

void TFM_OT_edge_slide(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Edge Slide";
	ot->description= "Slide an edge loop along a mesh."; 
	ot->idname = OP_EDGE_SLIDE;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_editmesh;

	RNA_def_float_factor(ot->srna, "value", 0, -1.0f, 1.0f, "Factor", "", -1.0f, 1.0f);

	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
}

void TFM_OT_transform(struct wmOperatorType *ot)
{
	static EnumPropertyItem transform_mode_types[] = {
			{TFM_INIT, "INIT", 0, "Init", ""},
			{TFM_DUMMY, "DUMMY", 0, "Dummy", ""},
			{TFM_TRANSLATION, "TRANSLATION", 0, "Translation", ""},
			{TFM_ROTATION, "ROTATION", 0, "Rotation", ""},
			{TFM_RESIZE, "RESIZE", 0, "Resize", ""},
			{TFM_TOSPHERE, "TOSPHERE", 0, "Tosphere", ""},
			{TFM_SHEAR, "SHEAR", 0, "Shear", ""},
			{TFM_WARP, "WARP", 0, "Warp", ""},
			{TFM_SHRINKFATTEN, "SHRINKFATTEN", 0, "Shrinkfatten", ""},
			{TFM_TILT, "TILT", 0, "Tilt", ""},
			{TFM_TRACKBALL, "TRACKBALL", 0, "Trackball", ""},
			{TFM_PUSHPULL, "PUSHPULL", 0, "Pushpull", ""},
			{TFM_CREASE, "CREASE", 0, "Crease", ""},
			{TFM_MIRROR, "MIRROR", 0, "Mirror", ""},
			{TFM_BONESIZE, "BONESIZE", 0, "Bonesize", ""},
			{TFM_BONE_ENVELOPE, "BONE_ENVELOPE", 0, "Bone_Envelope", ""},
			{TFM_CURVE_SHRINKFATTEN, "CURVE_SHRINKFATTEN", 0, "Curve_Shrinkfatten", ""},
			{TFM_BONE_ROLL, "BONE_ROLL", 0, "Bone_Roll", ""},
			{TFM_TIME_TRANSLATE, "TIME_TRANSLATE", 0, "Time_Translate", ""},
			{TFM_TIME_SLIDE, "TIME_SLIDE", 0, "Time_Slide", ""},
			{TFM_TIME_SCALE, "TIME_SCALE", 0, "Time_Scale", ""},
			{TFM_TIME_EXTEND, "TIME_EXTEND", 0, "Time_Extend", ""},
			{TFM_BAKE_TIME, "BAKE_TIME", 0, "Bake_Time", ""},
			{TFM_BEVEL, "BEVEL", 0, "Bevel", ""},
			{TFM_BWEIGHT, "BWEIGHT", 0, "Bweight", ""},
			{TFM_ALIGN, "ALIGN", 0, "Align", ""},
			{TFM_EDGE_SLIDE, "EDGESLIDE", 0, "Edge Slide", ""},
			{0, NULL, 0, NULL, NULL}
	};

	/* identifiers */
	ot->name   = "Transform";
	ot->description= "Transform selected items by mode type.";
	ot->idname = "TFM_OT_transform";
	ot->flag= OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel  = transform_cancel;
	ot->poll   = ED_operator_areaactive;

	RNA_def_enum(ot->srna, "mode", transform_mode_types, 0, "Mode", "");

	RNA_def_float_vector(ot->srna, "value", 4, NULL, -FLT_MAX, FLT_MAX, "Values", "", -FLT_MAX, FLT_MAX);

	Properties_Proportional(ot);
	RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");

	Properties_Constraints(ot);
}

void transform_operatortypes(void)
{
	WM_operatortype_append(TFM_OT_transform);
	WM_operatortype_append(TFM_OT_translate);
	WM_operatortype_append(TFM_OT_rotate);
	WM_operatortype_append(TFM_OT_tosphere);
	WM_operatortype_append(TFM_OT_resize);
	WM_operatortype_append(TFM_OT_shear);
	WM_operatortype_append(TFM_OT_warp);
	WM_operatortype_append(TFM_OT_shrink_fatten);
	WM_operatortype_append(TFM_OT_tilt);
	WM_operatortype_append(TFM_OT_trackball);
	WM_operatortype_append(TFM_OT_mirror);
	WM_operatortype_append(TFM_OT_edge_slide);

	WM_operatortype_append(TFM_OT_select_orientation);
	WM_operatortype_append(TFM_OT_create_orientation);
	WM_operatortype_append(TFM_OT_delete_orientation);
}

void transform_keymap_for_space(struct wmKeyConfig *keyconf, struct wmKeyMap *keymap, int spaceid)
{
	wmKeyMapItem *km;
	
	/* transform.c, only adds modal map once, checks if it's there */
	transform_modal_keymap(keyconf);
	
	switch(spaceid)
	{
		case SPACE_VIEW3D:
			km = WM_keymap_add_item(keymap, "TFM_OT_translate", GKEY, KM_PRESS, 0, 0);

			km= WM_keymap_add_item(keymap, "TFM_OT_translate", EVT_TWEAK_S, KM_ANY, 0, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_rotate", RKEY, KM_PRESS, 0, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_resize", SKEY, KM_PRESS, 0, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_warp", WKEY, KM_PRESS, KM_SHIFT, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_tosphere", SKEY, KM_PRESS, KM_ALT|KM_SHIFT, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_shear", SKEY, KM_PRESS, KM_ALT|KM_CTRL|KM_SHIFT, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_shrink_fatten", SKEY, KM_PRESS, KM_ALT, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_tilt", TKEY, KM_PRESS, 0, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_select_orientation", SPACEKEY, KM_PRESS, KM_ALT, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_create_orientation", SPACEKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
			RNA_boolean_set(km->ptr, "use", 1);

			km = WM_keymap_add_item(keymap, "TFM_OT_mirror", MKEY, KM_PRESS, KM_CTRL, 0);

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
			km= WM_keymap_add_item(keymap, "TFM_OT_translate", GKEY, KM_PRESS, 0, 0);

			km= WM_keymap_add_item(keymap, "TFM_OT_translate", EVT_TWEAK_S, KM_ANY, 0, 0);

				// XXX the 'mode' identifier here is not quite right
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_EXTEND);

			km = WM_keymap_add_item(keymap, "TFM_OT_rotate", RKEY, KM_PRESS, 0, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_resize", SKEY, KM_PRESS, 0, 0);
			break;
		case SPACE_NLA:
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", GKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TRANSLATION);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EVT_TWEAK_S, KM_ANY, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TRANSLATION);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_EXTEND);
			
			km= WM_keymap_add_item(keymap, "TFM_OT_transform", SKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_SCALE);
			break;
		case SPACE_NODE:
			km= WM_keymap_add_item(keymap, "TFM_OT_translate", GKEY, KM_PRESS, 0, 0);

			km= WM_keymap_add_item(keymap, "TFM_OT_translate", EVT_TWEAK_A, KM_ANY, 0, 0);
			km= WM_keymap_add_item(keymap, "TFM_OT_translate", EVT_TWEAK_S, KM_ANY, 0, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_rotate", RKEY, KM_PRESS, 0, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_resize", SKEY, KM_PRESS, 0, 0);
			break;
		case SPACE_SEQ:
			km= WM_keymap_add_item(keymap, "TFM_OT_translate", GKEY, KM_PRESS, 0, 0);

			km= WM_keymap_add_item(keymap, "TFM_OT_translate", EVT_TWEAK_S, KM_ANY, 0, 0);

			km= WM_keymap_add_item(keymap, "TFM_OT_transform", EKEY, KM_PRESS, 0, 0);
			RNA_int_set(km->ptr, "mode", TFM_TIME_EXTEND);
			break;
		case SPACE_IMAGE:
			km = WM_keymap_add_item(keymap, "TFM_OT_translate", GKEY, KM_PRESS, 0, 0);

			km= WM_keymap_add_item(keymap, "TFM_OT_translate", EVT_TWEAK_S, KM_ANY, 0, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_rotate", RKEY, KM_PRESS, 0, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_resize", SKEY, KM_PRESS, 0, 0);

			km = WM_keymap_add_item(keymap, "TFM_OT_mirror", MKEY, KM_PRESS, KM_CTRL, 0);
			break;
		default:
			break;
	}
}

