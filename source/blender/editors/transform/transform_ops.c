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
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/transform/transform_ops.c
 *  \ingroup edtransform
 */


#include "MEM_guardedalloc.h"

#include "DNA_scene_types.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_armature.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_screen.h"

#include "transform.h"

typedef struct TransformModeItem
{
	char *idname;
	int		mode;
	void (*opfunc)(wmOperatorType*);
} TransformModeItem;

static float VecOne[3] = {1, 1, 1};

static char OP_TRANSLATION[] = "TRANSFORM_OT_translate";
static char OP_ROTATION[] = "TRANSFORM_OT_rotate";
static char OP_TOSPHERE[] = "TRANSFORM_OT_tosphere";
static char OP_RESIZE[] = "TRANSFORM_OT_resize";
static char OP_SHEAR[] = "TRANSFORM_OT_shear";
static char OP_WARP[] = "TRANSFORM_OT_warp";
static char OP_SHRINK_FATTEN[] = "TRANSFORM_OT_shrink_fatten";
static char OP_PUSH_PULL[] = "TRANSFORM_OT_push_pull";
static char OP_TILT[] = "TRANSFORM_OT_tilt";
static char OP_TRACKBALL[] = "TRANSFORM_OT_trackball";
static char OP_MIRROR[] = "TRANSFORM_OT_mirror";
static char OP_EDGE_SLIDE[] = "TRANSFORM_OT_edge_slide";
static char OP_EDGE_CREASE[] = "TRANSFORM_OT_edge_crease";
static char OP_SEQ_SLIDE[] = "TRANSFORM_OT_seq_slide";

void TRANSFORM_OT_translate(struct wmOperatorType *ot);
void TRANSFORM_OT_rotate(struct wmOperatorType *ot);
void TRANSFORM_OT_tosphere(struct wmOperatorType *ot);
void TRANSFORM_OT_resize(struct wmOperatorType *ot);
void TRANSFORM_OT_shear(struct wmOperatorType *ot);
void TRANSFORM_OT_warp(struct wmOperatorType *ot);
void TRANSFORM_OT_shrink_fatten(struct wmOperatorType *ot);
void TRANSFORM_OT_push_pull(struct wmOperatorType *ot);
void TRANSFORM_OT_tilt(struct wmOperatorType *ot);
void TRANSFORM_OT_trackball(struct wmOperatorType *ot);
void TRANSFORM_OT_mirror(struct wmOperatorType *ot);
void TRANSFORM_OT_edge_slide(struct wmOperatorType *ot);
void TRANSFORM_OT_edge_crease(struct wmOperatorType *ot);
void TRANSFORM_OT_seq_slide(struct wmOperatorType *ot);

static TransformModeItem transform_modes[] =
{
	{OP_TRANSLATION, TFM_TRANSLATION, TRANSFORM_OT_translate},
	{OP_ROTATION, TFM_ROTATION, TRANSFORM_OT_rotate},
	{OP_TOSPHERE, TFM_TOSPHERE, TRANSFORM_OT_tosphere},
	{OP_RESIZE, TFM_RESIZE, TRANSFORM_OT_resize},
	{OP_SHEAR, TFM_SHEAR, TRANSFORM_OT_shear},
	{OP_WARP, TFM_WARP, TRANSFORM_OT_warp},
	{OP_SHRINK_FATTEN, TFM_SHRINKFATTEN, TRANSFORM_OT_shrink_fatten},
	{OP_PUSH_PULL, TFM_PUSHPULL, TRANSFORM_OT_push_pull},
	{OP_TILT, TFM_TILT, TRANSFORM_OT_tilt},
	{OP_TRACKBALL, TFM_TRACKBALL, TRANSFORM_OT_trackball},
	{OP_MIRROR, TFM_MIRROR, TRANSFORM_OT_mirror},
	{OP_EDGE_SLIDE, TFM_EDGE_SLIDE, TRANSFORM_OT_edge_slide},
	{OP_EDGE_CREASE, TFM_CREASE, TRANSFORM_OT_edge_crease},
	{OP_SEQ_SLIDE, TFM_SEQ_SLIDE, TRANSFORM_OT_seq_slide},
	{NULL, 0}
};

EnumPropertyItem transform_mode_types[] =
{
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
	{TFM_BONESIZE, "BONE_SIZE", 0, "Bonesize", ""},
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
	{TFM_SEQ_SLIDE, "SEQSLIDE", 0, "Sequence Slide", ""},
	{0, NULL, 0, NULL, NULL}
};

static int snap_type_exec(bContext *C, wmOperator *op)
{
	ToolSettings *ts= CTX_data_tool_settings(C);

	ts->snap_mode = RNA_enum_get(op->ptr,"type");

	WM_event_add_notifier(C, NC_SCENE|ND_TOOLSETTINGS, NULL); /* header redraw */

	return OPERATOR_FINISHED;
}

static void TRANSFORM_OT_snap_type(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Snap Type";
	ot->description = "Set the snap element type";
	ot->idname = "TRANSFORM_OT_snap_type";

	/* api callbacks */
	ot->invoke = WM_menu_invoke;
	ot->exec = snap_type_exec;

	ot->poll = ED_operator_areaactive;

	/* flags */
	ot->flag = OPTYPE_UNDO;

	/* props */
	ot->prop = RNA_def_enum(ot->srna, "type", snap_element_items, 0, "Type", "Set the snap element type");

}

static int select_orientation_exec(bContext *C, wmOperator *op)
{
	int orientation = RNA_enum_get(op->ptr, "orientation");

	BIF_selectTransformOrientationValue(C, orientation);
	
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_VIEW3D, CTX_wm_view3d(C));

	return OPERATOR_FINISHED;
}

static int select_orientation_invoke(bContext *C, wmOperator *UNUSED(op), wmEvent *UNUSED(event))
{
	uiPopupMenu *pup;
	uiLayout *layout;

	pup= uiPupMenuBegin(C, "Orientation", ICON_NONE);
	layout= uiPupMenuLayout(pup);
	uiItemsEnumO(layout, "TRANSFORM_OT_select_orientation", "orientation");
	uiPupMenuEnd(C, pup);

	return OPERATOR_CANCELLED;
}

static void TRANSFORM_OT_select_orientation(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name   = "Select Orientation";
	ot->description = "Select transformation orientation";
	ot->idname = "TRANSFORM_OT_select_orientation";
	ot->flag   = OPTYPE_UNDO;

	/* api callbacks */
	ot->invoke = select_orientation_invoke;
	ot->exec   = select_orientation_exec;
	ot->poll   = ED_operator_view3d_active;

	prop= RNA_def_property(ot->srna, "orientation", PROP_ENUM, PROP_NONE);
	RNA_def_property_ui_text(prop, "Orientation", "Transformation orientation");
	RNA_def_enum_funcs(prop, rna_TransformOrientation_itemf);
}


static int delete_orientation_exec(bContext *C, wmOperator *UNUSED(op))
{
	View3D *v3d = CTX_wm_view3d(C);
	int selected_index = (v3d->twmode - V3D_MANIP_CUSTOM);

	BIF_removeTransformOrientationIndex(C, selected_index);
	
	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_VIEW3D, CTX_wm_view3d(C));
	WM_event_add_notifier(C, NC_SCENE|NA_EDITED, CTX_data_scene(C));

	return OPERATOR_FINISHED;
}

static int delete_orientation_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	return delete_orientation_exec(C, op);
}

static int delete_orientation_poll(bContext *C)
{
	int selected_index = -1;
	View3D *v3d = CTX_wm_view3d(C);
	
	if (ED_operator_areaactive(C) == 0)
		return 0;
	
	
	if (v3d) {
		selected_index = (v3d->twmode - V3D_MANIP_CUSTOM);
	}
	
	return selected_index >= 0;
}

static void TRANSFORM_OT_delete_orientation(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Delete Orientation";
	ot->description = "Delete transformation orientation";
	ot->idname = "TRANSFORM_OT_delete_orientation";
	ot->flag   = OPTYPE_UNDO;

	/* api callbacks */
	ot->invoke = delete_orientation_invoke;
	ot->exec   = delete_orientation_exec;
	ot->poll   = delete_orientation_poll;
}

static int create_orientation_exec(bContext *C, wmOperator *op)
{
	char name[MAX_NAME];
	int use = RNA_boolean_get(op->ptr, "use");
	int overwrite = RNA_boolean_get(op->ptr, "overwrite");
	
	RNA_string_get(op->ptr, "name", name);

	BIF_createTransformOrientation(C, op->reports, name, use, overwrite);

	WM_event_add_notifier(C, NC_SPACE|ND_SPACE_VIEW3D, CTX_wm_view3d(C));
	WM_event_add_notifier(C, NC_SCENE|NA_EDITED, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

static int create_orientation_invoke(bContext *C, wmOperator *op, wmEvent *UNUSED(event))
{
	return create_orientation_exec(C, op);
}

static void TRANSFORM_OT_create_orientation(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Create Orientation";
	ot->description = "Create transformation orientation from selection";
	ot->idname = "TRANSFORM_OT_create_orientation";
	ot->flag   = OPTYPE_REGISTER|OPTYPE_UNDO;

	/* api callbacks */
	ot->invoke = create_orientation_invoke;
	ot->exec   = create_orientation_exec;
	ot->poll   = ED_operator_areaactive;
	ot->flag   = OPTYPE_REGISTER|OPTYPE_UNDO;

	RNA_def_string(ot->srna, "name", "", MAX_NAME, "Name", "Text to insert at the cursor position");
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
		TransInfo *t = MEM_callocN(sizeof(TransInfo), "TransInfo data2");
		TransformModeItem *tmode;
		int mode = -1;

		for (tmode = transform_modes; tmode->idname; tmode++)
		{
			if (op->type->idname == tmode->idname)
			{
				mode = tmode->mode;
				break;
			}
		}

		if (mode == -1)
		{
			mode = RNA_enum_get(op->ptr, "mode");
		}

		retval = initTransform(C, t, op, event, mode);
		G.moving = 1;

		/* store data */
		if (retval) {
			op->customdata = t;
		}
		else {
			MEM_freeN(t);
		}
	}

	return retval; /* return 0 on error */
}

static int transform_modal(bContext *C, wmOperator *op, wmEvent *event)
{
	int exit_code;

	TransInfo *t = op->customdata;

#if 0
	// stable 2D mouse coords map to different 3D coords while the 3D mouse is active
	// in other words, 2D deltas are no longer good enough!
	// disable until individual 'transformers' behave better

	if (event->type == NDOF_MOTION)
		return OPERATOR_PASS_THROUGH;
#endif

	/* XXX insert keys are called here, and require context */
	t->context= C;
	exit_code = transformEvent(t, event);
	t->context= NULL;

	transformApply(C, t);

	exit_code |= transformEnd(C, t);

	if ((exit_code & OPERATOR_RUNNING_MODAL) == 0)
	{
		transformops_exit(C, op);
		exit_code &= ~OPERATOR_PASS_THROUGH; /* preventively remove passthrough */
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
	
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);

	return OPERATOR_FINISHED;
}

static int transform_invoke(bContext *C, wmOperator *op, wmEvent *event)
{
	if (!transformops_data(C, op, event))
	{
		G.moving = 0;
		return OPERATOR_CANCELLED;
	}

	if (RNA_struct_property_is_set(op->ptr, "value")) {
		return transform_exec(C, op);
	}
	else {
		/* add temp handler */
		WM_event_add_modal_handler(C, op);

		op->flag |= OP_GRAB_POINTER; // XXX maybe we want this with the manipulator only?
		return OPERATOR_RUNNING_MODAL;
	}
}

void Transform_Properties(struct wmOperatorType *ot, int flags)
{
	PropertyRNA *prop;

	if (flags & P_AXIS)
	{
		prop= RNA_def_property(ot->srna, "axis", PROP_FLOAT, PROP_DIRECTION);
		RNA_def_property_array(prop, 3);
		/* Make this not hidden when there's a nice axis selection widget */
		RNA_def_property_flag(prop, PROP_HIDDEN);
		RNA_def_property_ui_text(prop, "Axis", "The axis around which the transformation occurs");

	}

	if (flags & P_CONSTRAINT)
	{
		RNA_def_boolean_vector(ot->srna, "constraint_axis", 3, NULL, "Constraint Axis", "");
		prop= RNA_def_property(ot->srna, "constraint_orientation", PROP_ENUM, PROP_NONE);
		RNA_def_property_ui_text(prop, "Orientation", "Transformation orientation");
		RNA_def_enum_funcs(prop, rna_TransformOrientation_itemf);

		
	}

	if (flags & P_MIRROR)
	{
		RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
	}


	if (flags & P_PROPORTIONAL)
	{
		RNA_def_enum(ot->srna, "proportional", proportional_editing_items, 0, "Proportional Editing", "");
		RNA_def_enum(ot->srna, "proportional_edit_falloff", proportional_falloff_items, 0, "Proportional Editing Falloff", "Falloff type for proportional editing mode");
		RNA_def_float(ot->srna, "proportional_size", 1, 0.00001f, FLT_MAX, "Proportional Size", "", 0.001, 100);
	}

	if (flags & P_SNAP)
	{
		prop= RNA_def_boolean(ot->srna, "snap", 0, "Use Snapping Options", "");
		RNA_def_property_flag(prop, PROP_HIDDEN);

		if (flags & P_GEO_SNAP) {
			prop= RNA_def_enum(ot->srna, "snap_target", snap_target_items, 0, "Target", "");
			RNA_def_property_flag(prop, PROP_HIDDEN);
			prop= RNA_def_float_vector(ot->srna, "snap_point", 3, NULL, -FLT_MAX, FLT_MAX, "Point", "", -FLT_MAX, FLT_MAX);
			RNA_def_property_flag(prop, PROP_HIDDEN);
			
			if (flags & P_ALIGN_SNAP) {
				prop= RNA_def_boolean(ot->srna, "snap_align", 0, "Align with Point Normal", "");
				RNA_def_property_flag(prop, PROP_HIDDEN);
				prop= RNA_def_float_vector(ot->srna, "snap_normal", 3, NULL, -FLT_MAX, FLT_MAX, "Normal", "", -FLT_MAX, FLT_MAX);
				RNA_def_property_flag(prop, PROP_HIDDEN);
			}
		}
	}
	
	if (flags & P_OPTIONS)
	{
		RNA_def_boolean(ot->srna, "texture_space", 0, "Edit Object data texture space", "");
	}

	if (flags & P_CORRECT_UV)
	{
		RNA_def_boolean(ot->srna, "correct_uv", 0, "Correct UV coords when transforming", "");
	}

	// Add confirm method all the time. At the end because it's not really that important and should be hidden only in log, not in keymap edit
	/*prop =*/ RNA_def_boolean(ot->srna, "release_confirm", 0, "Confirm on Release", "Always confirm operation when releasing button");
	//RNA_def_property_flag(prop, PROP_HIDDEN);
}

void TRANSFORM_OT_translate(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Translate";
	ot->description = "Translate selected items";
	ot->idname = OP_TRANSLATION;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float_vector_xyz(ot->srna, "value", 3, NULL, -FLT_MAX, FLT_MAX, "Vector", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_CONSTRAINT|P_PROPORTIONAL|P_MIRROR|P_ALIGN_SNAP|P_OPTIONS);
}

void TRANSFORM_OT_resize(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Resize";
	ot->description = "Resize selected items"; 
	ot->idname = OP_RESIZE;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float_vector(ot->srna, "value", 3, VecOne, -FLT_MAX, FLT_MAX, "Vector", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_CONSTRAINT|P_PROPORTIONAL|P_MIRROR|P_GEO_SNAP|P_OPTIONS);
}


void TRANSFORM_OT_trackball(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Trackball";
	ot->description = "Trackball style rotation of selected items";
	ot->idname = OP_TRACKBALL;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float_vector(ot->srna, "value", 2, VecOne, -FLT_MAX, FLT_MAX, "Angle", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_PROPORTIONAL|P_MIRROR|P_SNAP);
}

void TRANSFORM_OT_rotate(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Rotate";
	ot->description = "Rotate selected items";
	ot->idname = OP_ROTATION;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float_rotation(ot->srna, "value", 1, NULL, -FLT_MAX, FLT_MAX, "Angle", "", -M_PI*2, M_PI*2);

	Transform_Properties(ot, P_AXIS|P_CONSTRAINT|P_PROPORTIONAL|P_MIRROR|P_GEO_SNAP);
}

void TRANSFORM_OT_tilt(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Tilt";
	/* optionals - 
	 * "Tilt selected vertices"
	 * "Specify an extra axis rotation for selected vertices of 3d curve" */
	ot->description = "Tilt selected control vertices of 3d curve"; 
	ot->idname = OP_TILT;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_editcurve_3d;

	RNA_def_float_rotation(ot->srna, "value", 1, NULL, -FLT_MAX, FLT_MAX, "Angle", "", -M_PI*2, M_PI*2);

	Transform_Properties(ot, P_CONSTRAINT|P_PROPORTIONAL|P_MIRROR|P_SNAP);
}

void TRANSFORM_OT_warp(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Warp";
	ot->description = "Warp selected items around the cursor";
	ot->idname = OP_WARP;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float_rotation(ot->srna, "value", 1, NULL, -FLT_MAX, FLT_MAX, "Angle", "", -M_PI*2, M_PI*2);

	Transform_Properties(ot, P_PROPORTIONAL|P_MIRROR|P_SNAP);
	// XXX Warp axis?
}

void TRANSFORM_OT_shear(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Shear";
	ot->description = "Shear selected items along the horizontal screen axis";
	ot->idname = OP_SHEAR;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Offset", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_PROPORTIONAL|P_MIRROR|P_SNAP);
	// XXX Shear axis?
}

void TRANSFORM_OT_push_pull(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Push/Pull";
	ot->description = "Push/Pull selected items";
	ot->idname = OP_PUSH_PULL;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Distance", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_PROPORTIONAL|P_MIRROR|P_SNAP);
}

void TRANSFORM_OT_shrink_fatten(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Shrink/Fatten";
	ot->description = "Shrink/fatten selected vertices along normals";
	ot->idname = OP_SHRINK_FATTEN;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_editmesh;

	RNA_def_float(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Offset", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_PROPORTIONAL|P_MIRROR|P_SNAP);
}

void TRANSFORM_OT_tosphere(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "To Sphere";
	//added "around mesh center" to differentiate between "MESH_OT_vertices_to_sphere()" 
	ot->description = "Move selected vertices outward in a spherical shape around mesh center";
	ot->idname = OP_TOSPHERE;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float_factor(ot->srna, "value", 0, 0, 1, "Factor", "", 0, 1);

	Transform_Properties(ot, P_PROPORTIONAL|P_MIRROR|P_SNAP);
}

void TRANSFORM_OT_mirror(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Mirror";
	ot->description = "Mirror selected vertices around one or more axes";
	ot->idname = OP_MIRROR;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	Transform_Properties(ot, P_CONSTRAINT|P_PROPORTIONAL);
}

void TRANSFORM_OT_edge_slide(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Edge Slide";
	ot->description = "Slide an edge loop along a mesh"; 
	ot->idname = OP_EDGE_SLIDE;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_editmesh;

	RNA_def_float_factor(ot->srna, "value", 0, -1.0f, 1.0f, "Factor", "", -1.0f, 1.0f);

	Transform_Properties(ot, P_MIRROR|P_SNAP|P_CORRECT_UV);
}

void TRANSFORM_OT_edge_crease(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Edge Crease";
	ot->description = "Change the crease of edges";
	ot->idname = OP_EDGE_CREASE;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_editmesh;

	RNA_def_float_factor(ot->srna, "value", 0, -1.0f, 1.0f, "Factor", "", -1.0f, 1.0f);

	Transform_Properties(ot, P_SNAP);
}

void TRANSFORM_OT_seq_slide(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Sequence Slide";
	ot->description = "Slide a sequence strip in time";
	ot->idname = OP_SEQ_SLIDE;
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_sequencer_active;

	RNA_def_float_vector(ot->srna, "value", 2, VecOne, -FLT_MAX, FLT_MAX, "Angle", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_SNAP);
}

void TRANSFORM_OT_transform(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name   = "Transform";
	ot->description = "Transform selected items by mode type";
	ot->idname = "TRANSFORM_OT_transform";
	ot->flag = OPTYPE_REGISTER|OPTYPE_UNDO|OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	prop= RNA_def_enum(ot->srna, "mode", transform_mode_types, TFM_TRANSLATION, "Mode", "");
	RNA_def_property_flag(prop, PROP_HIDDEN);

	RNA_def_float_vector(ot->srna, "value", 4, NULL, -FLT_MAX, FLT_MAX, "Values", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_AXIS|P_CONSTRAINT|P_PROPORTIONAL|P_MIRROR|P_ALIGN_SNAP);
}

void transform_operatortypes(void)
{
	TransformModeItem *tmode;

	for (tmode = transform_modes; tmode->idname; tmode++)
	{
		WM_operatortype_append(tmode->opfunc);
	}

	WM_operatortype_append(TRANSFORM_OT_transform);

	WM_operatortype_append(TRANSFORM_OT_select_orientation);
	WM_operatortype_append(TRANSFORM_OT_create_orientation);
	WM_operatortype_append(TRANSFORM_OT_delete_orientation);

	WM_operatortype_append(TRANSFORM_OT_snap_type);
}

void transform_keymap_for_space(wmKeyConfig *keyconf, wmKeyMap *keymap, int spaceid)
{
	wmKeyMapItem *kmi;
	wmKeyMap *modalmap;
	
	/* transform.c, only adds modal map once, checks if it's there */
	modalmap = transform_modal_keymap(keyconf);

	/* assign map to operators only the first time */
	if (modalmap) {
		TransformModeItem *tmode;

		for (tmode = transform_modes; tmode->idname; tmode++)
		{
			WM_modalkeymap_assign(modalmap, tmode->idname);
		}
		WM_modalkeymap_assign(modalmap, "TRANSFORM_OT_transform");
	}
	
	switch(spaceid)
	{
		case SPACE_VIEW3D:
			WM_keymap_add_item(keymap, OP_TRANSLATION, GKEY, KM_PRESS, 0, 0);

			WM_keymap_add_item(keymap, OP_TRANSLATION, EVT_TWEAK_S, KM_ANY, 0, 0);

			WM_keymap_add_item(keymap, OP_ROTATION, RKEY, KM_PRESS, 0, 0);

			WM_keymap_add_item(keymap, OP_RESIZE, SKEY, KM_PRESS, 0, 0);

			WM_keymap_add_item(keymap, OP_WARP, WKEY, KM_PRESS, KM_SHIFT, 0);

			WM_keymap_add_item(keymap, OP_TOSPHERE, SKEY, KM_PRESS, KM_ALT|KM_SHIFT, 0);

			WM_keymap_add_item(keymap, OP_SHEAR, SKEY, KM_PRESS, KM_ALT|KM_CTRL|KM_SHIFT, 0);

			WM_keymap_add_item(keymap, "TRANSFORM_OT_select_orientation", SPACEKEY, KM_PRESS, KM_ALT, 0);

			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_create_orientation", SPACEKEY, KM_PRESS, KM_CTRL|KM_ALT, 0);
			RNA_boolean_set(kmi->ptr, "use", TRUE);

			WM_keymap_add_item(keymap, OP_MIRROR, MKEY, KM_PRESS, KM_CTRL, 0);

			kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", TABKEY, KM_PRESS, KM_SHIFT, 0);
			RNA_string_set(kmi->ptr, "data_path", "tool_settings.use_snap");

			WM_keymap_add_item(keymap, "TRANSFORM_OT_snap_type", TABKEY, KM_PRESS, KM_SHIFT|KM_CTRL, 0);

			kmi = WM_keymap_add_item(keymap, OP_TRANSLATION, TKEY, KM_PRESS, KM_SHIFT, 0);
			RNA_boolean_set(kmi->ptr, "texture_space", TRUE);

			kmi = WM_keymap_add_item(keymap, OP_RESIZE, TKEY, KM_PRESS, KM_SHIFT|KM_ALT, 0);
			RNA_boolean_set(kmi->ptr, "texture_space", TRUE);

			break;
		case SPACE_ACTION:
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", GKEY, KM_PRESS, 0, 0);
			RNA_enum_set(kmi->ptr, "mode", TFM_TIME_TRANSLATE);
			
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", EVT_TWEAK_S, KM_ANY, 0, 0);
			RNA_enum_set(kmi->ptr, "mode", TFM_TIME_TRANSLATE);
			
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", EKEY, KM_PRESS, 0, 0);
			RNA_enum_set(kmi->ptr, "mode", TFM_TIME_EXTEND);
			
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", SKEY, KM_PRESS, 0, 0);
			RNA_enum_set(kmi->ptr, "mode", TFM_TIME_SCALE);
			
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", TKEY, KM_PRESS, KM_SHIFT, 0);
			RNA_enum_set(kmi->ptr, "mode", TFM_TIME_SLIDE);
			break;
		case SPACE_IPO:
			WM_keymap_add_item(keymap, OP_TRANSLATION, GKEY, KM_PRESS, 0, 0);
			
			WM_keymap_add_item(keymap, OP_TRANSLATION, EVT_TWEAK_S, KM_ANY, 0, 0);
			
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", EKEY, KM_PRESS, 0, 0);
			RNA_enum_set(kmi->ptr, "mode", TFM_TIME_EXTEND);
			
			WM_keymap_add_item(keymap, OP_ROTATION, RKEY, KM_PRESS, 0, 0);
			
			WM_keymap_add_item(keymap, OP_RESIZE, SKEY, KM_PRESS, 0, 0);
			break;
		case SPACE_NLA:
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", GKEY, KM_PRESS, 0, 0);
			RNA_enum_set(kmi->ptr, "mode", TFM_TRANSLATION);
			
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", EVT_TWEAK_S, KM_ANY, 0, 0);
			RNA_enum_set(kmi->ptr, "mode", TFM_TRANSLATION);
			
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", EKEY, KM_PRESS, 0, 0);
			RNA_enum_set(kmi->ptr, "mode", TFM_TIME_EXTEND);
			
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", SKEY, KM_PRESS, 0, 0);
			RNA_enum_set(kmi->ptr, "mode", TFM_TIME_SCALE);
			break;
		case SPACE_NODE:
			WM_keymap_add_item(keymap, OP_TRANSLATION, GKEY, KM_PRESS, 0, 0);

			kmi = WM_keymap_add_item(keymap, OP_TRANSLATION, EVT_TWEAK_A, KM_ANY, 0, 0);
			RNA_boolean_set(kmi->ptr, "release_confirm", TRUE);
			kmi = WM_keymap_add_item(keymap, OP_TRANSLATION, EVT_TWEAK_S, KM_ANY, 0, 0);
			RNA_boolean_set(kmi->ptr, "release_confirm", TRUE);

			WM_keymap_add_item(keymap, OP_ROTATION, RKEY, KM_PRESS, 0, 0);

			WM_keymap_add_item(keymap, OP_RESIZE, SKEY, KM_PRESS, 0, 0);

			/* detach and translate */
			WM_keymap_add_item(keymap, "NODE_OT_move_detach_links", DKEY, KM_PRESS, KM_ALT, 0);

			/* XXX release_confirm is set in the macro operator definition */
			WM_keymap_add_item(keymap, "NODE_OT_move_detach_links", EVT_TWEAK_A, KM_ANY, KM_ALT, 0);
			WM_keymap_add_item(keymap, "NODE_OT_move_detach_links", EVT_TWEAK_S, KM_ANY, KM_ALT, 0);
			break;
		case SPACE_SEQ:
			WM_keymap_add_item(keymap, OP_SEQ_SLIDE, GKEY, KM_PRESS, 0, 0);

			WM_keymap_add_item(keymap, OP_SEQ_SLIDE, EVT_TWEAK_S, KM_ANY, 0, 0);

			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_transform", EKEY, KM_PRESS, 0, 0);
			RNA_enum_set(kmi->ptr, "mode", TFM_TIME_EXTEND);
			break;
		case SPACE_IMAGE:
			WM_keymap_add_item(keymap, OP_TRANSLATION, GKEY, KM_PRESS, 0, 0);

			WM_keymap_add_item(keymap, OP_TRANSLATION, EVT_TWEAK_S, KM_ANY, 0, 0);

			WM_keymap_add_item(keymap, OP_ROTATION, RKEY, KM_PRESS, 0, 0);

			WM_keymap_add_item(keymap, OP_RESIZE, SKEY, KM_PRESS, 0, 0);

			WM_keymap_add_item(keymap, OP_SHEAR, SKEY, KM_PRESS, KM_ALT|KM_CTRL|KM_SHIFT, 0);

			WM_keymap_add_item(keymap, "TRANSFORM_OT_mirror", MKEY, KM_PRESS, KM_CTRL, 0);

			kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", TABKEY, KM_PRESS, KM_SHIFT, 0);
			RNA_string_set(kmi->ptr, "data_path", "tool_settings.use_snap");
			break;
		case SPACE_CLIP:
			WM_keymap_add_item(keymap, OP_TRANSLATION, GKEY, KM_PRESS, 0, 0);
			WM_keymap_add_item(keymap, OP_TRANSLATION, EVT_TWEAK_S, KM_ANY, 0, 0);
			WM_keymap_add_item(keymap, OP_RESIZE, SKEY, KM_PRESS, 0, 0);
			break;
		default:
			break;
	}
}

