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

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "BLI_math.h"
#include "BLI_utildefines.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_armature.h"
#include "BKE_report.h"

#include "RNA_access.h"
#include "RNA_define.h"
#include "RNA_enum_types.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "ED_screen.h"
#include "ED_mesh.h"

#include "transform.h"

typedef struct TransformModeItem {
	char *idname;
	int mode;
	void (*opfunc)(wmOperatorType *);
} TransformModeItem;

static const float VecOne[3] = {1, 1, 1};

static char OP_TRANSLATION[] = "TRANSFORM_OT_translate";
static char OP_ROTATION[] = "TRANSFORM_OT_rotate";
static char OP_TOSPHERE[] = "TRANSFORM_OT_tosphere";
static char OP_RESIZE[] = "TRANSFORM_OT_resize";
static char OP_SKIN_RESIZE[] = "TRANSFORM_OT_skin_resize";
static char OP_SHEAR[] = "TRANSFORM_OT_shear";
static char OP_BEND[] = "TRANSFORM_OT_bend";
static char OP_SHRINK_FATTEN[] = "TRANSFORM_OT_shrink_fatten";
static char OP_PUSH_PULL[] = "TRANSFORM_OT_push_pull";
static char OP_TILT[] = "TRANSFORM_OT_tilt";
static char OP_TRACKBALL[] = "TRANSFORM_OT_trackball";
static char OP_MIRROR[] = "TRANSFORM_OT_mirror";
static char OP_EDGE_SLIDE[] = "TRANSFORM_OT_edge_slide";
static char OP_VERT_SLIDE[] = "TRANSFORM_OT_vert_slide";
static char OP_EDGE_CREASE[] = "TRANSFORM_OT_edge_crease";
static char OP_EDGE_BWEIGHT[] = "TRANSFORM_OT_edge_bevelweight";
static char OP_SEQ_SLIDE[] = "TRANSFORM_OT_seq_slide";

static void TRANSFORM_OT_translate(struct wmOperatorType *ot);
static void TRANSFORM_OT_rotate(struct wmOperatorType *ot);
static void TRANSFORM_OT_tosphere(struct wmOperatorType *ot);
static void TRANSFORM_OT_resize(struct wmOperatorType *ot);
static void TRANSFORM_OT_skin_resize(struct wmOperatorType *ot);
static void TRANSFORM_OT_shear(struct wmOperatorType *ot);
static void TRANSFORM_OT_bend(struct wmOperatorType *ot);
static void TRANSFORM_OT_shrink_fatten(struct wmOperatorType *ot);
static void TRANSFORM_OT_push_pull(struct wmOperatorType *ot);
static void TRANSFORM_OT_tilt(struct wmOperatorType *ot);
static void TRANSFORM_OT_trackball(struct wmOperatorType *ot);
static void TRANSFORM_OT_mirror(struct wmOperatorType *ot);
static void TRANSFORM_OT_edge_slide(struct wmOperatorType *ot);
static void TRANSFORM_OT_vert_slide(struct wmOperatorType *ot);
static void TRANSFORM_OT_edge_crease(struct wmOperatorType *ot);
static void TRANSFORM_OT_edge_bevelweight(struct wmOperatorType *ot);
static void TRANSFORM_OT_seq_slide(struct wmOperatorType *ot);

static TransformModeItem transform_modes[] =
{
	{OP_TRANSLATION, TFM_TRANSLATION, TRANSFORM_OT_translate},
	{OP_ROTATION, TFM_ROTATION, TRANSFORM_OT_rotate},
	{OP_TOSPHERE, TFM_TOSPHERE, TRANSFORM_OT_tosphere},
	{OP_RESIZE, TFM_RESIZE, TRANSFORM_OT_resize},
	{OP_SKIN_RESIZE, TFM_SKIN_RESIZE, TRANSFORM_OT_skin_resize},
	{OP_SHEAR, TFM_SHEAR, TRANSFORM_OT_shear},
	{OP_BEND, TFM_BEND, TRANSFORM_OT_bend},
	{OP_SHRINK_FATTEN, TFM_SHRINKFATTEN, TRANSFORM_OT_shrink_fatten},
	{OP_PUSH_PULL, TFM_PUSHPULL, TRANSFORM_OT_push_pull},
	{OP_TILT, TFM_TILT, TRANSFORM_OT_tilt},
	{OP_TRACKBALL, TFM_TRACKBALL, TRANSFORM_OT_trackball},
	{OP_MIRROR, TFM_MIRROR, TRANSFORM_OT_mirror},
	{OP_EDGE_SLIDE, TFM_EDGE_SLIDE, TRANSFORM_OT_edge_slide},
	{OP_VERT_SLIDE, TFM_VERT_SLIDE, TRANSFORM_OT_vert_slide},
	{OP_EDGE_CREASE, TFM_CREASE, TRANSFORM_OT_edge_crease},
	{OP_EDGE_BWEIGHT, TFM_BWEIGHT, TRANSFORM_OT_edge_bevelweight},
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
	{TFM_SKIN_RESIZE, "SKIN_RESIZE", 0, "Skin Resize", ""},
	{TFM_TOSPHERE, "TOSPHERE", 0, "Tosphere", ""},
	{TFM_SHEAR, "SHEAR", 0, "Shear", ""},
	{TFM_BEND, "BEND", 0, "Bend", ""},
	{TFM_SHRINKFATTEN, "SHRINKFATTEN", 0, "Shrinkfatten", ""},
	{TFM_TILT, "TILT", 0, "Tilt", ""},
	{TFM_TRACKBALL, "TRACKBALL", 0, "Trackball", ""},
	{TFM_PUSHPULL, "PUSHPULL", 0, "Pushpull", ""},
	{TFM_CREASE, "CREASE", 0, "Crease", ""},
	{TFM_MIRROR, "MIRROR", 0, "Mirror", ""},
	{TFM_BONESIZE, "BONE_SIZE", 0, "Bonesize", ""},
	{TFM_BONE_ENVELOPE, "BONE_ENVELOPE", 0, "Bone_Envelope", ""},
	{TFM_CURVE_SHRINKFATTEN, "CURVE_SHRINKFATTEN", 0, "Curve_Shrinkfatten", ""},
	{TFM_MASK_SHRINKFATTEN, "MASK_SHRINKFATTEN", 0, "Mask_Shrinkfatten", ""},
	{TFM_BONE_ROLL, "BONE_ROLL", 0, "Bone_Roll", ""},
	{TFM_TIME_TRANSLATE, "TIME_TRANSLATE", 0, "Time_Translate", ""},
	{TFM_TIME_SLIDE, "TIME_SLIDE", 0, "Time_Slide", ""},
	{TFM_TIME_SCALE, "TIME_SCALE", 0, "Time_Scale", ""},
	{TFM_TIME_EXTEND, "TIME_EXTEND", 0, "Time_Extend", ""},
	{TFM_BAKE_TIME, "BAKE_TIME", 0, "Bake_Time", ""},
	{TFM_BWEIGHT, "BWEIGHT", 0, "Bweight", ""},
	{TFM_ALIGN, "ALIGN", 0, "Align", ""},
	{TFM_EDGE_SLIDE, "EDGESLIDE", 0, "Edge Slide", ""},
	{TFM_SEQ_SLIDE, "SEQSLIDE", 0, "Sequence Slide", ""},
	{0, NULL, 0, NULL, NULL}
};

static int select_orientation_exec(bContext *C, wmOperator *op)
{
	int orientation = RNA_enum_get(op->ptr, "orientation");

	BIF_selectTransformOrientationValue(C, orientation);
	
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, CTX_wm_view3d(C));

	return OPERATOR_FINISHED;
}

static int select_orientation_invoke(bContext *C, wmOperator *UNUSED(op), const wmEvent *UNUSED(event))
{
	uiPopupMenu *pup;
	uiLayout *layout;

	pup = uiPupMenuBegin(C, IFACE_("Orientation"), ICON_NONE);
	layout = uiPupMenuLayout(pup);
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

	prop = RNA_def_property(ot->srna, "orientation", PROP_ENUM, PROP_NONE);
	RNA_def_property_ui_text(prop, "Orientation", "Transformation orientation");
	RNA_def_enum_funcs(prop, rna_TransformOrientation_itemf);
}


static int delete_orientation_exec(bContext *C, wmOperator *UNUSED(op))
{
	View3D *v3d = CTX_wm_view3d(C);
	int selected_index = (v3d->twmode - V3D_MANIP_CUSTOM);

	BIF_removeTransformOrientationIndex(C, selected_index);
	
	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
	WM_event_add_notifier(C, NC_SCENE | NA_EDITED, CTX_data_scene(C));

	return OPERATOR_FINISHED;
}

static int delete_orientation_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
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
	const bool use = RNA_boolean_get(op->ptr, "use");
	const bool overwrite = RNA_boolean_get(op->ptr, "overwrite");
	const bool use_view = RNA_boolean_get(op->ptr, "use_view");
	View3D *v3d = CTX_wm_view3d(C);

	RNA_string_get(op->ptr, "name", name);

	if (use && !v3d) {
		BKE_report(op->reports, RPT_ERROR, "Create Orientation's 'use' parameter only valid in a 3DView context");
		return OPERATOR_CANCELLED;
	}

	BIF_createTransformOrientation(C, op->reports, name, use_view, use, overwrite);

	WM_event_add_notifier(C, NC_SPACE | ND_SPACE_VIEW3D, v3d);
	WM_event_add_notifier(C, NC_SCENE | NA_EDITED, CTX_data_scene(C));
	
	return OPERATOR_FINISHED;
}

static void TRANSFORM_OT_create_orientation(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Create Orientation";
	ot->description = "Create transformation orientation from selection";
	ot->idname = "TRANSFORM_OT_create_orientation";
	ot->flag   = OPTYPE_REGISTER | OPTYPE_UNDO;

	/* api callbacks */
	ot->exec   = create_orientation_exec;
	ot->poll   = ED_operator_areaactive;

	RNA_def_string(ot->srna, "name", NULL, MAX_NAME, "Name", "Name of the new custom orientation");
	RNA_def_boolean(ot->srna, "use_view", FALSE, "Use View",
	                "Use the current view instead of the active object to create the new orientation");
	RNA_def_boolean(ot->srna, "use", FALSE, "Use after creation", "Select orientation after its creation");
	RNA_def_boolean(ot->srna, "overwrite", FALSE, "Overwrite previous",
	                "Overwrite previously created orientation with same name");
}


#ifdef USE_LOOPSLIDE_HACK
/**
 * Special hack for MESH_OT_loopcut_slide so we get back to the selection mode
 */
static void transformops_loopsel_hack(bContext *C, wmOperator *op)
{
	if (op->type->idname == OP_EDGE_SLIDE) {
		if (op->opm && op->opm->opm && op->opm->opm->prev) {
			wmOperator *op_prev = op->opm->opm->prev;
			Scene *scene = CTX_data_scene(C);
			int mesh_select_mode[3];
			PropertyRNA *prop = RNA_struct_find_property(op_prev->ptr, "mesh_select_mode_init");

			if (prop && RNA_property_is_set(op_prev->ptr, prop)) {
				ToolSettings *ts = scene->toolsettings;
				short selectmode_orig;

				RNA_property_boolean_get_array(op_prev->ptr, prop, mesh_select_mode);
				selectmode_orig = ((mesh_select_mode[0] ? SCE_SELECT_VERTEX : 0) |
				                   (mesh_select_mode[1] ? SCE_SELECT_EDGE   : 0) |
				                   (mesh_select_mode[2] ? SCE_SELECT_FACE   : 0));

				/* still switch if we were originally in face select mode */
				if ((ts->selectmode != selectmode_orig) && (selectmode_orig != SCE_SELECT_FACE)) {
					BMEditMesh *em = BKE_editmesh_from_object(scene->obedit);
					em->selectmode = ts->selectmode = selectmode_orig;
					EDBM_selectmode_set(em);
				}
			}
		}
	}
}
#endif  /* USE_LOOPSLIDE_HACK */


static void transformops_exit(bContext *C, wmOperator *op)
{
#ifdef USE_LOOPSLIDE_HACK
	transformops_loopsel_hack(C, op);
#endif

	saveTransform(C, op->customdata, op);
	MEM_freeN(op->customdata);
	op->customdata = NULL;
	G.moving = 0;
}

static int transformops_data(bContext *C, wmOperator *op, const wmEvent *event)
{
	int retval = 1;
	if (op->customdata == NULL) {
		TransInfo *t = MEM_callocN(sizeof(TransInfo), "TransInfo data2");
		TransformModeItem *tmode;
		int mode = -1;

		for (tmode = transform_modes; tmode->idname; tmode++) {
			if (op->type->idname == tmode->idname) {
				mode = tmode->mode;
				break;
			}
		}

		if (mode == -1) {
			mode = RNA_enum_get(op->ptr, "mode");
		}

		retval = initTransform(C, t, op, event, mode);

		/* store data */
		if (retval) {
			G.moving = special_transform_moving(t);
			op->customdata = t;
		}
		else {
			MEM_freeN(t);
		}
	}

	return retval; /* return 0 on error */
}

static int transform_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	int exit_code;

	TransInfo *t = op->customdata;
	const enum TfmMode mode_prev = t->mode;

#if 0
	// stable 2D mouse coords map to different 3D coords while the 3D mouse is active
	// in other words, 2D deltas are no longer good enough!
	// disable until individual 'transformers' behave better

	if (event->type == NDOF_MOTION)
		return OPERATOR_PASS_THROUGH;
#endif

	/* XXX insert keys are called here, and require context */
	t->context = C;
	exit_code = transformEvent(t, event);
	t->context = NULL;

	transformApply(C, t);

	exit_code |= transformEnd(C, t);

	if ((exit_code & OPERATOR_RUNNING_MODAL) == 0) {
		transformops_exit(C, op);
		exit_code &= ~OPERATOR_PASS_THROUGH; /* preventively remove passthrough */
	}
	else {
		if (mode_prev != t->mode) {
			/* WARNING: this is not normal to switch operator types
			 * normally it would not be supported but transform happens
			 * to share callbacks between differernt operators. */
			wmOperatorType *ot_new = NULL;
			TransformModeItem *item = transform_modes;
			while (item->idname) {
				if (item->mode == t->mode) {
					ot_new = WM_operatortype_find(item->idname, false);
					break;
				}
				item++;
			}

			BLI_assert(ot_new != NULL);
			if (ot_new) {
				WM_operator_type_set(op, ot_new);
			}
			/* end suspicious code */
		}
	}

	return exit_code;
}

static void transform_cancel(bContext *C, wmOperator *op)
{
	TransInfo *t = op->customdata;

	t->state = TRANS_CANCEL;
	transformEnd(C, t);
	transformops_exit(C, op);
}

static int transform_exec(bContext *C, wmOperator *op)
{
	TransInfo *t;

	if (!transformops_data(C, op, NULL)) {
		G.moving = 0;
		return OPERATOR_CANCELLED;
	}

	t = op->customdata;

	t->options |= CTX_AUTOCONFIRM;

	transformApply(C, t);

	transformEnd(C, t);

	transformops_exit(C, op);
	
	WM_event_add_notifier(C, NC_OBJECT | ND_TRANSFORM, NULL);

	return OPERATOR_FINISHED;
}

static int transform_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	if (!transformops_data(C, op, event)) {
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

	if (flags & P_AXIS) {
		prop = RNA_def_property(ot->srna, "axis", PROP_FLOAT, PROP_DIRECTION);
		RNA_def_property_array(prop, 3);
		/* Make this not hidden when there's a nice axis selection widget */
		RNA_def_property_flag(prop, PROP_HIDDEN);
		RNA_def_property_ui_text(prop, "Axis", "The axis around which the transformation occurs");
	}

	if (flags & P_CONSTRAINT) {
		RNA_def_boolean_vector(ot->srna, "constraint_axis", 3, NULL, "Constraint Axis", "");
		prop = RNA_def_property(ot->srna, "constraint_orientation", PROP_ENUM, PROP_NONE);
		RNA_def_property_ui_text(prop, "Orientation", "Transformation orientation");
		RNA_def_enum_funcs(prop, rna_TransformOrientation_itemf);
	}

	if (flags & P_MIRROR) {
		prop = RNA_def_boolean(ot->srna, "mirror", 0, "Mirror Editing", "");
		if (flags & P_MIRROR_DUMMY) {
			/* only used so macros can disable this option */
			RNA_def_property_flag(prop, PROP_HIDDEN);
		}
	}


	if (flags & P_PROPORTIONAL) {
		RNA_def_enum(ot->srna, "proportional", proportional_editing_items, 0, "Proportional Editing", "");
		prop = RNA_def_enum(ot->srna, "proportional_edit_falloff", proportional_falloff_items, 0,
		                    "Proportional Editing Falloff", "Falloff type for proportional editing mode");
		RNA_def_property_translation_context(prop, BLF_I18NCONTEXT_ID_CURVE); /* Abusing id_curve :/ */
		RNA_def_float(ot->srna, "proportional_size", 1, 0.00001f, FLT_MAX, "Proportional Size", "", 0.001, 100);
	}

	if (flags & P_SNAP) {
		prop = RNA_def_boolean(ot->srna, "snap", 0, "Use Snapping Options", "");
		RNA_def_property_flag(prop, PROP_HIDDEN);

		if (flags & P_GEO_SNAP) {
			prop = RNA_def_enum(ot->srna, "snap_target", snap_target_items, 0, "Target", "");
			RNA_def_property_flag(prop, PROP_HIDDEN);
			prop = RNA_def_float_vector(ot->srna, "snap_point", 3, NULL, -FLT_MAX, FLT_MAX, "Point", "", -FLT_MAX, FLT_MAX);
			RNA_def_property_flag(prop, PROP_HIDDEN);
			
			if (flags & P_ALIGN_SNAP) {
				prop = RNA_def_boolean(ot->srna, "snap_align", 0, "Align with Point Normal", "");
				RNA_def_property_flag(prop, PROP_HIDDEN);
				prop = RNA_def_float_vector(ot->srna, "snap_normal", 3, NULL, -FLT_MAX, FLT_MAX, "Normal", "", -FLT_MAX, FLT_MAX);
				RNA_def_property_flag(prop, PROP_HIDDEN);
			}
		}
	}

	if ((flags & P_OPTIONS) && !(flags & P_NO_TEXSPACE)) {
		RNA_def_boolean(ot->srna, "texture_space", 0, "Edit Texture Space", "Edit Object data texture space");
		prop = RNA_def_boolean(ot->srna, "remove_on_cancel", 0, "Remove on Cancel", "Remove elements on cancel");
		RNA_def_property_flag(prop, PROP_HIDDEN);
	}

	if (flags & P_CORRECT_UV) {
		RNA_def_boolean(ot->srna, "correct_uv", 0, "Correct UVs", "Correct UV coordinates when transforming");
	}

	if ((flags & P_NO_DEFAULTS) == 0) {
		// Add confirm method all the time. At the end because it's not really that important and should be hidden only in log, not in keymap edit
		/*prop =*/ RNA_def_boolean(ot->srna, "release_confirm", 0, "Confirm on Release", "Always confirm operation when releasing button");
		//RNA_def_property_flag(prop, PROP_HIDDEN);
	}
}

static void TRANSFORM_OT_translate(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Translate";
	ot->description = "Translate (move) selected items";
	ot->idname = OP_TRANSLATION;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float_vector_xyz(ot->srna, "value", 3, NULL, -FLT_MAX, FLT_MAX, "Vector", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_CONSTRAINT | P_PROPORTIONAL | P_MIRROR | P_ALIGN_SNAP | P_OPTIONS);
}

static void TRANSFORM_OT_resize(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Resize";
	ot->description = "Scale (resize) selected items"; 
	ot->idname = OP_RESIZE;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float_vector(ot->srna, "value", 3, VecOne, -FLT_MAX, FLT_MAX, "Vector", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_CONSTRAINT | P_PROPORTIONAL | P_MIRROR | P_GEO_SNAP | P_OPTIONS);
}

static int skin_resize_poll(bContext *C)
{
	struct Object *obedit = CTX_data_edit_object(C);
	if (obedit && obedit->type == OB_MESH) {
		BMEditMesh *em = BKE_editmesh_from_object(obedit);
		return (em && CustomData_has_layer(&em->bm->vdata, CD_MVERT_SKIN));
	}
	return 0;
}

static void TRANSFORM_OT_skin_resize(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Skin Resize";
	ot->description = "Scale selected vertices' skin radii"; 
	ot->idname = OP_SKIN_RESIZE;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = skin_resize_poll;

	RNA_def_float_vector(ot->srna, "value", 3, VecOne, -FLT_MAX, FLT_MAX, "Vector", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_CONSTRAINT | P_PROPORTIONAL | P_MIRROR | P_GEO_SNAP | P_OPTIONS | P_NO_TEXSPACE);
}

static void TRANSFORM_OT_trackball(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name   = "Trackball";
	ot->description = "Trackball style rotation of selected items";
	ot->idname = OP_TRACKBALL;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	/* Maybe we could use float_vector_xyz here too? */
	prop = RNA_def_float_vector(ot->srna, "value", 2, NULL, -FLT_MAX, FLT_MAX, "Angle", "", -FLT_MAX, FLT_MAX);
	RNA_def_property_subtype(prop, PROP_ANGLE);

	Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP);
}

static void TRANSFORM_OT_rotate(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Rotate";
	ot->description = "Rotate selected items";
	ot->idname = OP_ROTATION;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	prop = RNA_def_float(ot->srna, "value", 0.0f, -FLT_MAX, FLT_MAX, "Angle", "", -M_PI * 2, M_PI * 2);
	RNA_def_property_subtype(prop, PROP_ANGLE);

	Transform_Properties(ot, P_AXIS | P_CONSTRAINT | P_PROPORTIONAL | P_MIRROR | P_GEO_SNAP);
}

static void TRANSFORM_OT_tilt(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Tilt";
	/* optionals - 
	 * "Tilt selected vertices"
	 * "Specify an extra axis rotation for selected vertices of 3D curve" */
	ot->description = "Tilt selected control vertices of 3D curve"; 
	ot->idname = OP_TILT;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_editcurve_3d;

	prop = RNA_def_float(ot->srna, "value", 0.0, -FLT_MAX, FLT_MAX, "Angle", "", -M_PI * 2, M_PI * 2);
	RNA_def_property_subtype(prop, PROP_ANGLE);

	Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP);
}

static void TRANSFORM_OT_bend(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Bend";
	ot->description = "Bend selected items between the 3D cursor and the mouse";
	ot->idname = OP_BEND;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	// ot->exec   = transform_exec;  // unsupported
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_region_view3d_active;

	RNA_def_float_rotation(ot->srna, "value", 1, NULL, -FLT_MAX, FLT_MAX, "Angle", "", -M_PI * 2, M_PI * 2);

	Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP);
}

static void TRANSFORM_OT_shear(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Shear";
	ot->description = "Shear selected items along the horizontal screen axis";
	ot->idname = OP_SHEAR;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Offset", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP);
	// XXX Shear axis?
}

static void TRANSFORM_OT_push_pull(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Push/Pull";
	ot->description = "Push/Pull selected items";
	ot->idname = OP_PUSH_PULL;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Distance", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP);
}

static void TRANSFORM_OT_shrink_fatten(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Shrink/Fatten";
	ot->description = "Shrink/fatten selected vertices along normals";
	ot->idname = OP_SHRINK_FATTEN;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_editmesh;

	RNA_def_float(ot->srna, "value", 0, -FLT_MAX, FLT_MAX, "Offset", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP);
}

static void TRANSFORM_OT_tosphere(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "To Sphere";
	//added "around mesh center" to differentiate between "MESH_OT_vertices_to_sphere()" 
	ot->description = "Move selected vertices outward in a spherical shape around mesh center";
	ot->idname = OP_TOSPHERE;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	RNA_def_float_factor(ot->srna, "value", 0, 0, 1, "Factor", "", 0, 1);

	Transform_Properties(ot, P_PROPORTIONAL | P_MIRROR | P_SNAP);
}

static void TRANSFORM_OT_mirror(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Mirror";
	ot->description = "Mirror selected vertices around one or more axes";
	ot->idname = OP_MIRROR;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	Transform_Properties(ot, P_CONSTRAINT | P_PROPORTIONAL);
}

static void TRANSFORM_OT_edge_slide(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Edge Slide";
	ot->description = "Slide an edge loop along a mesh"; 
	ot->idname = OP_EDGE_SLIDE;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_editmesh;

	RNA_def_float_factor(ot->srna, "value", 0, -1.0f, 1.0f, "Factor", "", -1.0f, 1.0f);

	Transform_Properties(ot, P_MIRROR | P_SNAP | P_CORRECT_UV);
}

static void TRANSFORM_OT_vert_slide(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Vertex Slide";
	ot->description = "Slide a vertex along a mesh";
	ot->idname = OP_VERT_SLIDE;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_editmesh;

	RNA_def_float_factor(ot->srna, "value", 0, -10.0f, 10.0f, "Factor", "", -1.0f, 1.0f);

	Transform_Properties(ot, P_MIRROR | P_SNAP);
}

static void TRANSFORM_OT_edge_crease(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Edge Crease";
	ot->description = "Change the crease of edges";
	ot->idname = OP_EDGE_CREASE;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_editmesh;

	RNA_def_float_factor(ot->srna, "value", 0, -1.0f, 1.0f, "Factor", "", -1.0f, 1.0f);

	Transform_Properties(ot, P_SNAP);
}

static void TRANSFORM_OT_edge_bevelweight(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Edge Bevel Weight";
	ot->description = "Change the bevel weight of edges";
	ot->idname = OP_EDGE_BWEIGHT;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_editmesh;

	RNA_def_float_factor(ot->srna, "value", 0, -1.0f, 1.0f, "Factor", "", -1.0f, 1.0f);

	Transform_Properties(ot, P_SNAP);
}

static void TRANSFORM_OT_seq_slide(struct wmOperatorType *ot)
{
	/* identifiers */
	ot->name   = "Sequence Slide";
	ot->description = "Slide a sequence strip in time";
	ot->idname = OP_SEQ_SLIDE;
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_sequencer_active;

	RNA_def_float_vector_xyz(ot->srna, "value", 2, NULL, -FLT_MAX, FLT_MAX, "Vector", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_SNAP);
}

static void TRANSFORM_OT_transform(struct wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name   = "Transform";
	ot->description = "Transform selected items by mode type";
	ot->idname = "TRANSFORM_OT_transform";
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_BLOCKING;

	/* api callbacks */
	ot->invoke = transform_invoke;
	ot->exec   = transform_exec;
	ot->modal  = transform_modal;
	ot->cancel = transform_cancel;
	ot->poll   = ED_operator_screenactive;

	prop = RNA_def_enum(ot->srna, "mode", transform_mode_types, TFM_TRANSLATION, "Mode", "");
	RNA_def_property_flag(prop, PROP_HIDDEN);

	RNA_def_float_vector(ot->srna, "value", 4, NULL, -FLT_MAX, FLT_MAX, "Values", "", -FLT_MAX, FLT_MAX);

	Transform_Properties(ot, P_AXIS | P_CONSTRAINT | P_PROPORTIONAL | P_MIRROR | P_ALIGN_SNAP);
}

void transform_operatortypes(void)
{
	TransformModeItem *tmode;

	for (tmode = transform_modes; tmode->idname; tmode++) {
		WM_operatortype_append(tmode->opfunc);
	}

	WM_operatortype_append(TRANSFORM_OT_transform);

	WM_operatortype_append(TRANSFORM_OT_select_orientation);
	WM_operatortype_append(TRANSFORM_OT_create_orientation);
	WM_operatortype_append(TRANSFORM_OT_delete_orientation);
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

		for (tmode = transform_modes; tmode->idname; tmode++) {
			WM_modalkeymap_assign(modalmap, tmode->idname);
		}
		WM_modalkeymap_assign(modalmap, "TRANSFORM_OT_transform");
	}
	
	switch (spaceid) {
		case SPACE_VIEW3D:
			WM_keymap_add_item(keymap, OP_TRANSLATION, GKEY, KM_PRESS, 0, 0);

			WM_keymap_add_item(keymap, OP_TRANSLATION, EVT_TWEAK_S, KM_ANY, 0, 0);

			WM_keymap_add_item(keymap, OP_ROTATION, RKEY, KM_PRESS, 0, 0);

			WM_keymap_add_item(keymap, OP_RESIZE, SKEY, KM_PRESS, 0, 0);

			WM_keymap_add_item(keymap, OP_BEND, WKEY, KM_PRESS, KM_SHIFT, 0);

			WM_keymap_add_item(keymap, OP_TOSPHERE, SKEY, KM_PRESS, KM_ALT | KM_SHIFT, 0);

			WM_keymap_add_item(keymap, OP_SHEAR, SKEY, KM_PRESS, KM_ALT | KM_CTRL | KM_SHIFT, 0);

			WM_keymap_add_item(keymap, "TRANSFORM_OT_select_orientation", SPACEKEY, KM_PRESS, KM_ALT, 0);

			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_create_orientation", SPACEKEY, KM_PRESS, KM_CTRL | KM_ALT, 0);
			RNA_boolean_set(kmi->ptr, "use", TRUE);

			WM_keymap_add_item(keymap, OP_MIRROR, MKEY, KM_PRESS, KM_CTRL, 0);

			kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", TABKEY, KM_PRESS, KM_SHIFT, 0);
			RNA_string_set(kmi->ptr, "data_path", "tool_settings.use_snap");

			kmi = WM_keymap_add_item(keymap, "WM_OT_context_menu_enum", TABKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
			RNA_string_set(kmi->ptr, "data_path", "tool_settings.snap_element");


			kmi = WM_keymap_add_item(keymap, OP_TRANSLATION, TKEY, KM_PRESS, KM_SHIFT, 0);
			RNA_boolean_set(kmi->ptr, "texture_space", TRUE);

			kmi = WM_keymap_add_item(keymap, OP_RESIZE, TKEY, KM_PRESS, KM_SHIFT | KM_ALT, 0);
			RNA_boolean_set(kmi->ptr, "texture_space", TRUE);

			WM_keymap_add_item(keymap, OP_SKIN_RESIZE, AKEY, KM_PRESS, KM_CTRL, 0);

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
			WM_keymap_add_item(keymap, "NODE_OT_translate_attach", GKEY, KM_PRESS, 0, 0);
			WM_keymap_add_item(keymap, "NODE_OT_translate_attach", EVT_TWEAK_A, KM_ANY, 0, 0);
			WM_keymap_add_item(keymap, "NODE_OT_translate_attach", EVT_TWEAK_S, KM_ANY, 0, 0);
			/* NB: small trick: macro operator poll may fail due to library data edit,
			 * in that case the secondary regular operators are called with same keymap.
			 */
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_translate", GKEY, KM_PRESS, 0, 0);
			RNA_boolean_set(kmi->ptr, "release_confirm", TRUE);
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_translate", EVT_TWEAK_A, KM_ANY, 0, 0);
			RNA_boolean_set(kmi->ptr, "release_confirm", TRUE);
			kmi = WM_keymap_add_item(keymap, "TRANSFORM_OT_translate", EVT_TWEAK_S, KM_ANY, 0, 0);
			RNA_boolean_set(kmi->ptr, "release_confirm", TRUE);

			WM_keymap_add_item(keymap, OP_ROTATION, RKEY, KM_PRESS, 0, 0);

			WM_keymap_add_item(keymap, OP_RESIZE, SKEY, KM_PRESS, 0, 0);

			/* detach and translate */
			WM_keymap_add_item(keymap, "NODE_OT_move_detach_links", DKEY, KM_PRESS, KM_ALT, 0);
			/* XXX release_confirm is set in the macro operator definition */
			WM_keymap_add_item(keymap, "NODE_OT_move_detach_links_release", EVT_TWEAK_A, KM_ANY, KM_ALT, 0);
			WM_keymap_add_item(keymap, "NODE_OT_move_detach_links", EVT_TWEAK_S, KM_ANY, KM_ALT, 0);

			/* dettach and translate */
			WM_keymap_add_item(keymap, "NODE_OT_detach_translate_attach", FKEY, KM_PRESS, KM_ALT, 0);
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

			WM_keymap_add_item(keymap, OP_SHEAR, SKEY, KM_PRESS, KM_ALT | KM_CTRL | KM_SHIFT, 0);

			WM_keymap_add_item(keymap, "TRANSFORM_OT_mirror", MKEY, KM_PRESS, KM_CTRL, 0);

			kmi = WM_keymap_add_item(keymap, "WM_OT_context_toggle", TABKEY, KM_PRESS, KM_SHIFT, 0);
			RNA_string_set(kmi->ptr, "data_path", "tool_settings.use_snap");

			kmi = WM_keymap_add_item(keymap, "WM_OT_context_menu_enum", TABKEY, KM_PRESS, KM_SHIFT | KM_CTRL, 0);
			RNA_string_set(kmi->ptr, "data_path", "tool_settings.snap_uv_element");
			break;
		case SPACE_CLIP:
			WM_keymap_add_item(keymap, OP_TRANSLATION, GKEY, KM_PRESS, 0, 0);
			WM_keymap_add_item(keymap, OP_TRANSLATION, EVT_TWEAK_S, KM_ANY, 0, 0);
			WM_keymap_add_item(keymap, OP_RESIZE, SKEY, KM_PRESS, 0, 0);
			WM_keymap_add_item(keymap, OP_ROTATION, RKEY, KM_PRESS, 0, 0);
			break;
		default:
			break;
	}
}

