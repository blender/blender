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
 * Contributor(s): Joseph Eagar, Howard Trickey, Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_bevel.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_string.h"
#include "BLI_math.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_editmesh.h"
#include "BKE_unit.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"

#include "ED_mesh.h"
#include "ED_numinput.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "mesh_intern.h"  /* own include */


#define MVAL_PIXEL_MARGIN  5.0f

#define PROFILE_HARD_MIN 0.0f

#define SEGMENTS_HARD_MAX 1000

/* which value is mouse movement and numeric input controlling? */
#define OFFSET_VALUE 0
#define OFFSET_VALUE_PERCENT 1
#define PROFILE_VALUE 2
#define SEGMENTS_VALUE 3
#define NUM_VALUE_KINDS 4

static const char *value_rna_name[NUM_VALUE_KINDS] = {"offset", "offset", "profile", "segments"};
static const float value_clamp_min[NUM_VALUE_KINDS] = {0.0f, 0.0f, PROFILE_HARD_MIN, 1.0f};
static const float value_clamp_max[NUM_VALUE_KINDS] = {1e6, 100.0f, 1.0f, SEGMENTS_HARD_MAX};
static const float value_start[NUM_VALUE_KINDS] = {0.0f, 0.0f, 0.5f, 1.0f};
static const float value_scale_per_inch[NUM_VALUE_KINDS] = { 0.0f, 100.0f, 1.0f, 4.0f};

typedef struct {
	BMEditMesh *em;
	float initial_length[NUM_VALUE_KINDS];
	float scale[NUM_VALUE_KINDS];
	NumInput num_input[NUM_VALUE_KINDS]; 
	float shift_value[NUM_VALUE_KINDS]; /* The current value when shift is pressed. Negative when shift not active. */
	bool is_modal;

	/* modal only */
	float mcenter[2];
	BMBackup mesh_backup;
	void *draw_handle_pixel;
	short twtype;
	short value_mode;  /* Which value does mouse movement and numeric input affect? */
	float segments;     /* Segments as float so smooth mouse pan works in small increments */
} BevelData;

static void edbm_bevel_update_header(bContext *C, wmOperator *op)
{
	const char *str = IFACE_("Confirm: (Enter/LMB), Cancel: (Esc/RMB), Mode: %s (M), Clamp Overlap: %s (C), "
	                         "Vertex Only: %s (V), Profile Control: %s (P), Offset: %s, Segments: %d, Profile: %.3f");

	char msg[UI_MAX_DRAW_STR];
	ScrArea *sa = CTX_wm_area(C);
	Scene *sce = CTX_data_scene(C);

	if (sa) {
		BevelData *opdata = op->customdata;
		char offset_str[NUM_STR_REP_LEN];
		const char *type_str;
		PropertyRNA *prop = RNA_struct_find_property(op->ptr, "offset_type");

		if (hasNumInput(&opdata->num_input[OFFSET_VALUE])) {
			outputNumInput(&opdata->num_input[OFFSET_VALUE], offset_str, &sce->unit);
		}
		else {
			BLI_snprintf(offset_str, NUM_STR_REP_LEN, "%f", RNA_float_get(op->ptr, "offset"));
		}

		RNA_property_enum_name_gettexted(C, op->ptr, prop, RNA_property_enum_get(op->ptr, prop), &type_str);

		BLI_snprintf(msg, sizeof(msg), str, type_str,
		             WM_bool_as_string(RNA_boolean_get(op->ptr, "clamp_overlap")),
		             WM_bool_as_string(RNA_boolean_get(op->ptr, "vertex_only")),
		             WM_bool_as_string(opdata->value_mode == PROFILE_VALUE),
		             offset_str, RNA_int_get(op->ptr, "segments"), RNA_float_get(op->ptr, "profile"));

		ED_area_headerprint(sa, msg);
	}
}

static bool edbm_bevel_init(bContext *C, wmOperator *op, const bool is_modal)
{
	Object *obedit = CTX_data_edit_object(C);
	Scene *scene = CTX_data_scene(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);
	BevelData *opdata;
	float pixels_per_inch;
	int i;

	if (em->bm->totvertsel == 0) {
		return false;
	}

	op->customdata = opdata = MEM_mallocN(sizeof(BevelData), "beveldata_mesh_operator");

	opdata->em = em;
	opdata->is_modal = is_modal;
	opdata->value_mode = OFFSET_VALUE;
	opdata->segments = (float) RNA_int_get(op->ptr, "segments");
	pixels_per_inch = U.dpi * U.pixelsize;

	for (i = 0; i < NUM_VALUE_KINDS; i++) {
		opdata->shift_value[i] = -1.0f;
		opdata->initial_length[i] = -1.0f;
		/* note: scale for OFFSET_VALUE will get overwritten in edbm_bevel_invoke */
		opdata->scale[i] = value_scale_per_inch[i] / pixels_per_inch; 

		initNumInput(&opdata->num_input[i]);
		opdata->num_input[i].idx_max = 0;
		opdata->num_input[i].val_flag[0] |= NUM_NO_NEGATIVE;
		if (i == SEGMENTS_VALUE) {
			opdata->num_input[i].val_flag[0] |= NUM_NO_FRACTION | NUM_NO_ZERO;
		}
		if (i == OFFSET_VALUE) {
			opdata->num_input[i].unit_sys = scene->unit.system;
		}
		opdata->num_input[i].unit_type[0] = B_UNIT_NONE;  /* Not sure this is a factor or a unit? */
	}

	/* avoid the cost of allocating a bm copy */
	if (is_modal) {
		View3D *v3d = CTX_wm_view3d(C);
		ARegion *ar = CTX_wm_region(C);

		opdata->mesh_backup = EDBM_redo_state_store(em);
		opdata->draw_handle_pixel = ED_region_draw_cb_activate(ar->type, ED_region_draw_mouse_line_cb,
			opdata->mcenter, REGION_DRAW_POST_PIXEL);
		G.moving = G_TRANSFORM_EDIT;

		if (v3d) {
			opdata->twtype = v3d->twtype;
			v3d->twtype = 0;
		}
	}

	return true;
}

static bool edbm_bevel_calc(wmOperator *op)
{
	BevelData *opdata = op->customdata;
	BMEditMesh *em = opdata->em;
	BMOperator bmop;
	const float offset = RNA_float_get(op->ptr, "offset");
	const int offset_type = RNA_enum_get(op->ptr, "offset_type");
	const int segments = RNA_int_get(op->ptr, "segments");
	const float profile = RNA_float_get(op->ptr, "profile");
	const bool vertex_only = RNA_boolean_get(op->ptr, "vertex_only");
	const bool clamp_overlap = RNA_boolean_get(op->ptr, "clamp_overlap");
	int material = RNA_int_get(op->ptr, "material");
	const bool loop_slide = RNA_boolean_get(op->ptr, "loop_slide");

	/* revert to original mesh */
	if (opdata->is_modal) {
		EDBM_redo_state_restore(opdata->mesh_backup, em, false);
	}

	if (em->ob) {
		material = CLAMPIS(material, -1, em->ob->totcol - 1);
	}

	EDBM_op_init(em, &bmop, op,
	             "bevel geom=%hev offset=%f segments=%i vertex_only=%b offset_type=%i profile=%f clamp_overlap=%b "
	             "material=%i loop_slide=%b",
	             BM_ELEM_SELECT, offset, segments, vertex_only, offset_type, profile,
	             clamp_overlap, material, loop_slide);

	BMO_op_exec(em->bm, &bmop);

	if (offset != 0.0f) {
		/* not essential, but we may have some loose geometry that
		 * won't get bevel'd and better not leave it selected */
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);
	}

	/* no need to de-select existing geometry */
	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return false;
	}

	EDBM_mesh_normals_update(opdata->em);

	EDBM_update_generic(opdata->em, true, true);

	return true;
}

static void edbm_bevel_exit(bContext *C, wmOperator *op)
{
	BevelData *opdata = op->customdata;

	ScrArea *sa = CTX_wm_area(C);

	if (sa) {
		ED_area_headerprint(sa, NULL);
	}

	if (opdata->is_modal) {
		View3D *v3d = CTX_wm_view3d(C);
		ARegion *ar = CTX_wm_region(C);
		EDBM_redo_state_free(&opdata->mesh_backup, NULL, false);
		ED_region_draw_cb_exit(ar->type, opdata->draw_handle_pixel);
		if (v3d) {
			v3d->twtype = opdata->twtype;
		}
		G.moving = 0;
	}
	MEM_freeN(opdata);
	op->customdata = NULL;
}

static void edbm_bevel_cancel(bContext *C, wmOperator *op)
{
	BevelData *opdata = op->customdata;
	if (opdata->is_modal) {
		EDBM_redo_state_free(&opdata->mesh_backup, opdata->em, true);
		EDBM_update_generic(opdata->em, false, true);
	}

	edbm_bevel_exit(C, op);

	/* need to force redisplay or we may still view the modified result */
	ED_region_tag_redraw(CTX_wm_region(C));
}

/* bevel! yay!!*/
static int edbm_bevel_exec(bContext *C, wmOperator *op)
{
	if (!edbm_bevel_init(C, op, false)) {
		return OPERATOR_CANCELLED;
	}

	if (!edbm_bevel_calc(op)) {
		edbm_bevel_cancel(C, op);
		return OPERATOR_CANCELLED;
	}

	edbm_bevel_exit(C, op);

	return OPERATOR_FINISHED;
}

static void edbm_bevel_calc_initial_length(wmOperator *op, const wmEvent *event, bool mode_changed)
{
	BevelData *opdata;
	float mlen[2], len, value, sc, st;
	int vmode;

	opdata = op->customdata;
	mlen[0] = opdata->mcenter[0] - event->mval[0];
	mlen[1] = opdata->mcenter[1] - event->mval[1];
	len = len_v2(mlen);
	vmode = opdata->value_mode;
	if (mode_changed || opdata->initial_length[vmode] == -1.0f) {
		/* If current value is not default start value, adjust len so that 
		 * the scaling and offset in edbm_bevel_mouse_set_value will
		 * start at current value */
		value = (vmode == SEGMENTS_VALUE) ?
			opdata->segments : RNA_float_get(op->ptr, value_rna_name[vmode]);
		sc = opdata->scale[vmode];
		st = value_start[vmode];
		if (value != value_start[vmode]) {
			len = (st + sc * (len - MVAL_PIXEL_MARGIN) - value) / sc;
		}
	}
	opdata->initial_length[opdata->value_mode] = len;
}

static int edbm_bevel_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	/* TODO make modal keymap (see fly mode) */
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	BevelData *opdata;
	float center_3d[3];

	if (!edbm_bevel_init(C, op, true)) {
		return OPERATOR_CANCELLED;
	}

	opdata = op->customdata;

	/* initialize mouse values */
	if (!calculateTransformCenter(C, V3D_AROUND_CENTER_MEAN, center_3d, opdata->mcenter)) {
		/* in this case the tool will likely do nothing,
		 * ideally this will never happen and should be checked for above */
		opdata->mcenter[0] = opdata->mcenter[1] = 0;
	}

	/* for OFFSET_VALUE only, the scale is the size of a pixel under the mouse in 3d space */
	opdata->scale[OFFSET_VALUE] = rv3d ? ED_view3d_pixel_size(rv3d, center_3d) : 1.0f;

	edbm_bevel_calc_initial_length(op, event, false);

	edbm_bevel_update_header(C, op);

	if (!edbm_bevel_calc(op)) {
		edbm_bevel_cancel(C, op);
		return OPERATOR_CANCELLED;
	}

	WM_event_add_modal_handler(C, op);

	return OPERATOR_RUNNING_MODAL;
}

static void edbm_bevel_mouse_set_value(wmOperator *op, const wmEvent *event)
{
	BevelData *opdata = op->customdata;
	int vmode = opdata->value_mode;
	float mdiff[2];
	float value;

	mdiff[0] = opdata->mcenter[0] - event->mval[0];
	mdiff[1] = opdata->mcenter[1] - event->mval[1];

	value = ((len_v2(mdiff) - MVAL_PIXEL_MARGIN) - opdata->initial_length[vmode]);

	/* Scale according to value mode */
	value = value_start[vmode] + value * opdata->scale[vmode];

	/* Fake shift-transform... */
	if (event->shift) {
		if (opdata->shift_value[vmode] < 0.0f) {
			opdata->shift_value[vmode] = (vmode == SEGMENTS_VALUE) ?
				opdata->segments : RNA_float_get(op->ptr, value_rna_name[vmode]);
		}
		value = (value - opdata->shift_value[vmode]) * 0.1f + opdata->shift_value[vmode];
	}
	else if (opdata->shift_value[vmode] >= 0.0f) {
		opdata->shift_value[vmode] = -1.0f;
	}

	/* clamp accordingto value mode, and store value back */
	CLAMP(value, value_clamp_min[vmode], value_clamp_max[vmode]);
	if (vmode == SEGMENTS_VALUE) {
		opdata->segments = value;
		RNA_int_set(op->ptr, "segments", (int)(value + 0.5f));
	}
	else {
		RNA_float_set(op->ptr, value_rna_name[vmode], value);
	}
}

static void edbm_bevel_numinput_set_value(wmOperator *op)
{
	BevelData *opdata = op->customdata;
	float value;
	int vmode;

	vmode = opdata->value_mode;
	value = (vmode == SEGMENTS_VALUE) ?
		opdata->segments : RNA_float_get(op->ptr, value_rna_name[vmode]);
	applyNumInput(&opdata->num_input[vmode], &value);
	CLAMP(value, value_clamp_min[vmode], value_clamp_max[vmode]);
	if (vmode == SEGMENTS_VALUE) {
		opdata->segments = value;
		RNA_int_set(op->ptr, "segments", (int)value);
	}
	else {
		RNA_float_set(op->ptr, value_rna_name[vmode], value);
	}
}

static int edbm_bevel_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	BevelData *opdata = op->customdata;
	const bool has_numinput = hasNumInput(&opdata->num_input[opdata->value_mode]);

	/* Modal numinput active, try to handle numeric inputs first... */
	if (event->val == KM_PRESS && has_numinput && handleNumInput(C, &opdata->num_input[opdata->value_mode], event)) {
		edbm_bevel_numinput_set_value(op);
		edbm_bevel_calc(op);
		edbm_bevel_update_header(C, op);
		return OPERATOR_RUNNING_MODAL;
	}
	else {
		bool handled = false;
		switch (event->type) {
			case ESCKEY:
			case RIGHTMOUSE:
				edbm_bevel_cancel(C, op);
				return OPERATOR_CANCELLED;

			case MOUSEMOVE:
				if (!has_numinput) {
					edbm_bevel_mouse_set_value(op, event);
					edbm_bevel_calc(op);
					edbm_bevel_update_header(C, op);
					handled = true;
				}
				break;

			case LEFTMOUSE:
			case PADENTER:
			case RETKEY:
				if (event->val == KM_PRESS) {
					edbm_bevel_calc(op);
					edbm_bevel_exit(C, op);
					return OPERATOR_FINISHED;
				}
				break;

			case MOUSEPAN: {
				float delta = 0.02f * (event->y - event->prevy);
				if (opdata->segments >= 1 && opdata->segments + delta < 1)
					opdata->segments = 1;
				else
					opdata->segments += delta;
				RNA_int_set(op->ptr, "segments", (int)opdata->segments);
				edbm_bevel_calc(op);
				edbm_bevel_update_header(C, op);
				handled = true;
				break;
			}

			/* Note this will prevent padplus and padminus to ever activate modal numinput.
			 * This is not really an issue though, as we only expect positive values here...
			 * Else we could force them to only modify segments number when shift is pressed, or so.
			 */

			case WHEELUPMOUSE:  /* change number of segments */
			case PADPLUSKEY:
				if (event->val == KM_RELEASE)
					break;

				opdata->segments = opdata->segments + 1;
				RNA_int_set(op->ptr, "segments", (int)opdata->segments);
				edbm_bevel_calc(op);
				edbm_bevel_update_header(C, op);
				handled = true;
				break;

			case WHEELDOWNMOUSE:  /* change number of segments */
			case PADMINUS:
				if (event->val == KM_RELEASE)
					break;

				opdata->segments = max_ff(opdata->segments - 1, 1);
				RNA_int_set(op->ptr, "segments", (int)opdata->segments);
				edbm_bevel_calc(op);
				edbm_bevel_update_header(C, op);
				handled = true;
				break;

			case MKEY:
				if (event->val == KM_RELEASE)
					break;

				{
					PropertyRNA *prop = RNA_struct_find_property(op->ptr, "offset_type");
					int type = RNA_property_enum_get(op->ptr, prop);
					type++;
					if (type > BEVEL_AMT_PERCENT) {
						type = BEVEL_AMT_OFFSET;
					}
					if (opdata->value_mode == OFFSET_VALUE && type == BEVEL_AMT_PERCENT)
						opdata->value_mode = OFFSET_VALUE_PERCENT;
					else if (opdata->value_mode == OFFSET_VALUE_PERCENT && type != BEVEL_AMT_PERCENT)
						opdata->value_mode = OFFSET_VALUE;
					RNA_property_enum_set(op->ptr, prop, type);
					if (opdata->initial_length[opdata->value_mode] == -1.0f)
						edbm_bevel_calc_initial_length(op, event, true);
				}
				/* Update offset accordingly to new offset_type. */
				if (!has_numinput &&
				    (opdata->value_mode == OFFSET_VALUE || opdata->value_mode == OFFSET_VALUE_PERCENT))
				{
					edbm_bevel_mouse_set_value(op, event);
				}		
				edbm_bevel_calc(op);
				edbm_bevel_update_header(C, op);
				handled = true;
				break;
			case CKEY:
				if (event->val == KM_RELEASE)
					break;

				{
					PropertyRNA *prop = RNA_struct_find_property(op->ptr, "clamp_overlap");
					RNA_property_boolean_set(op->ptr, prop, !RNA_property_boolean_get(op->ptr, prop));
				}
				edbm_bevel_calc(op);
				edbm_bevel_update_header(C, op);
				handled = true;
				break;
			case PKEY:
				if (event->val == KM_RELEASE)
					break;
				if (opdata->value_mode == PROFILE_VALUE) {
					opdata->value_mode = OFFSET_VALUE;
				}
				else {
					opdata->value_mode = PROFILE_VALUE;
				}
				edbm_bevel_calc_initial_length(op, event, true);
				break;
			case SKEY:
				if (event->val == KM_RELEASE)
					break;
				if (opdata->value_mode == SEGMENTS_VALUE) {
					opdata->value_mode = OFFSET_VALUE;
				}
				else {
					opdata->value_mode = SEGMENTS_VALUE;
				}
				edbm_bevel_calc_initial_length(op, event, true);
				break;
			case VKEY:
				if (event->val == KM_RELEASE)
					break;
				
				{
					PropertyRNA *prop = RNA_struct_find_property(op->ptr, "vertex_only");
					RNA_property_boolean_set(op->ptr, prop, !RNA_property_boolean_get(op->ptr, prop));
				}
				edbm_bevel_calc(op);
				edbm_bevel_update_header(C, op);
				handled = true;
				break;
				
		}

		/* Modal numinput inactive, try to handle numeric inputs last... */
		if (!handled && event->val == KM_PRESS && handleNumInput(C, &opdata->num_input[opdata->value_mode], event)) {
			edbm_bevel_numinput_set_value(op);
			edbm_bevel_calc(op);
			edbm_bevel_update_header(C, op);
			return OPERATOR_RUNNING_MODAL;
		}
	}

	return OPERATOR_RUNNING_MODAL;
}

static void mesh_ot_bevel_offset_range_func(PointerRNA *ptr, PropertyRNA *UNUSED(prop),
                                            float *min, float *max, float *softmin, float *softmax)
{
	const int offset_type = RNA_enum_get(ptr, "offset_type");

	*min = -FLT_MAX;
	*max = FLT_MAX;
	*softmin = 0.0f;
	*softmax = (offset_type == BEVEL_AMT_PERCENT) ? 100.0f : 1.0f;
}

void MESH_OT_bevel(wmOperatorType *ot)
{
	PropertyRNA *prop;

	static const EnumPropertyItem offset_type_items[] = {
		{BEVEL_AMT_OFFSET, "OFFSET", 0, "Offset", "Amount is offset of new edges from original"},
		{BEVEL_AMT_WIDTH, "WIDTH", 0, "Width", "Amount is width of new face"},
		{BEVEL_AMT_DEPTH, "DEPTH", 0, "Depth", "Amount is perpendicular distance from original edge to bevel face"},
		{BEVEL_AMT_PERCENT, "PERCENT", 0, "Percent", "Amount is percent of adjacent edge length"},
		{0, NULL, 0, NULL, NULL},
	};

	/* identifiers */
	ot->name = "Bevel";
	ot->description = "Edge Bevel";
	ot->idname = "MESH_OT_bevel";

	/* api callbacks */
	ot->exec = edbm_bevel_exec;
	ot->invoke = edbm_bevel_invoke;
	ot->modal = edbm_bevel_modal;
	ot->cancel = edbm_bevel_cancel;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_CURSOR | OPTYPE_BLOCKING;

	RNA_def_enum(ot->srna, "offset_type", offset_type_items, 0, "Amount Type", "What distance Amount measures");
	prop = RNA_def_float(ot->srna, "offset", 0.0f, -1e6f, 1e6f, "Amount", "", 0.0f, 1.0f);
	RNA_def_property_float_array_funcs_runtime(prop, NULL, NULL, mesh_ot_bevel_offset_range_func);
	RNA_def_property_flag(prop, PROP_SKIP_SAVE);
	RNA_def_int(ot->srna, "segments", 1, 1, SEGMENTS_HARD_MAX, "Segments", "Segments for curved edge", 1, 8);
	RNA_def_float(ot->srna, "profile", 0.5f, PROFILE_HARD_MIN, 1.0f, "Profile",
		"Controls profile shape (0.5 = round)", PROFILE_HARD_MIN, 1.0f);
	RNA_def_boolean(ot->srna, "vertex_only", false, "Vertex Only", "Bevel only vertices");
	RNA_def_boolean(ot->srna, "clamp_overlap", false, "Clamp Overlap",
		"Do not allow beveled edges/vertices to overlap each other");
	RNA_def_boolean(ot->srna, "loop_slide", true, "Loop Slide", "Prefer slide along edge to even widths");
	RNA_def_int(ot->srna, "material", -1, -1, INT_MAX, "Material",
		"Material for bevel faces (-1 means use adjacent faces)", -1, 100);
}
