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
 * Contributor(s): Campbell Barton
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/mesh/editmesh_inset.c
 *  \ingroup edmesh
 */

#include "MEM_guardedalloc.h"

#include "DNA_object_types.h"

#include "BLI_string.h"
#include "BLI_math.h"

#include "BLF_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_editmesh.h"
#include "BKE_unit.h"

#include "RNA_define.h"
#include "RNA_access.h"

#include "WM_api.h"
#include "WM_types.h"

#include "ED_mesh.h"
#include "ED_numinput.h"
#include "ED_screen.h"
#include "ED_space_api.h"
#include "ED_transform.h"
#include "ED_view3d.h"

#include "mesh_intern.h"  /* own include */


#define HEADER_LENGTH 180

typedef struct {
	float old_thickness;
	float old_depth;
	bool modify_depth;
	float initial_length;
	float pixel_size;  /* use when mouse input is interpreted as spatial distance */
	bool is_modal;
	bool shift;
	float shift_amount;
	BMEditMesh *em;
	NumInput num_input;

	/* modal only */
	float mcenter[2];
	BMBackup mesh_backup;
	void *draw_handle_pixel;
	short twtype;
} InsetData;


static void edbm_inset_update_header(wmOperator *op, bContext *C)
{
	InsetData *opdata = op->customdata;

	const char *str = IFACE_("Confirm: Enter/LClick, Cancel: (Esc/RClick), Thickness: %s, "
	                         "Depth (Ctrl to tweak): %s (%s), Outset (O): (%s), Boundary (B): (%s), Individual (I): (%s)");

	char msg[HEADER_LENGTH];
	ScrArea *sa = CTX_wm_area(C);
	Scene *sce = CTX_data_scene(C);

	if (sa) {
		char flts_str[NUM_STR_REP_LEN * 2];
		if (hasNumInput(&opdata->num_input))
			outputNumInput(&opdata->num_input, flts_str, &sce->unit);
		else {
			BLI_snprintf(flts_str, NUM_STR_REP_LEN, "%f", RNA_float_get(op->ptr, "thickness"));
			BLI_snprintf(flts_str + NUM_STR_REP_LEN, NUM_STR_REP_LEN, "%f", RNA_float_get(op->ptr, "depth"));
		}
		BLI_snprintf(msg, HEADER_LENGTH, str,
		             flts_str,
		             flts_str + NUM_STR_REP_LEN,
		             WM_bool_as_string(opdata->modify_depth),
		             WM_bool_as_string(RNA_boolean_get(op->ptr, "use_outset")),
		             WM_bool_as_string(RNA_boolean_get(op->ptr, "use_boundary")),
		             WM_bool_as_string(RNA_boolean_get(op->ptr, "use_individual"))
		            );

		ED_area_headerprint(sa, msg);
	}
}


static bool edbm_inset_init(bContext *C, wmOperator *op, const bool is_modal)
{
	InsetData *opdata;
	Scene *scene = CTX_data_scene(C);
	Object *obedit = CTX_data_edit_object(C);
	BMEditMesh *em = BKE_editmesh_from_object(obedit);

	if (em->bm->totvertsel == 0) {
		return false;
	}

	op->customdata = opdata = MEM_mallocN(sizeof(InsetData), "inset_operator_data");

	opdata->old_thickness = 0.01;
	opdata->old_depth = 0.0;
	opdata->modify_depth = false;
	opdata->shift = false;
	opdata->shift_amount = 0.0f;
	opdata->is_modal = is_modal;
	opdata->em = em;

	initNumInput(&opdata->num_input);
	opdata->num_input.idx_max = 1; /* Two elements. */
	opdata->num_input.unit_sys = scene->unit.system;
	opdata->num_input.unit_type[0] = B_UNIT_LENGTH;
	opdata->num_input.unit_type[1] = B_UNIT_LENGTH;

	if (is_modal) {
		View3D *v3d = CTX_wm_view3d(C);
		ARegion *ar = CTX_wm_region(C);

		opdata->mesh_backup = EDBM_redo_state_store(em);
		opdata->draw_handle_pixel = ED_region_draw_cb_activate(ar->type, ED_region_draw_mouse_line_cb, opdata->mcenter, REGION_DRAW_POST_PIXEL);
		G.moving = G_TRANSFORM_EDIT;
		opdata->twtype = v3d->twtype;
		v3d->twtype = 0;
	}

	return true;
}

static void edbm_inset_exit(bContext *C, wmOperator *op)
{
	InsetData *opdata;
	ScrArea *sa = CTX_wm_area(C);

	opdata = op->customdata;

	if (opdata->is_modal) {
		View3D *v3d = CTX_wm_view3d(C);
		ARegion *ar = CTX_wm_region(C);
		EDBM_redo_state_free(&opdata->mesh_backup, NULL, false);
		ED_region_draw_cb_exit(ar->type, opdata->draw_handle_pixel);
		v3d->twtype = opdata->twtype;
		G.moving = 0;
	}

	if (sa) {
		ED_area_headerprint(sa, NULL);
	}
	MEM_freeN(op->customdata);
}

static void edbm_inset_cancel(bContext *C, wmOperator *op)
{
	InsetData *opdata;

	opdata = op->customdata;
	if (opdata->is_modal) {
		EDBM_redo_state_free(&opdata->mesh_backup, opdata->em, true);
		EDBM_update_generic(opdata->em, false, true);
	}

	edbm_inset_exit(C, op);

	/* need to force redisplay or we may still view the modified result */
	ED_region_tag_redraw(CTX_wm_region(C));
}

static bool edbm_inset_calc(wmOperator *op)
{
	InsetData *opdata;
	BMEditMesh *em;
	BMOperator bmop;

	const bool use_boundary        = RNA_boolean_get(op->ptr, "use_boundary");
	const bool use_even_offset     = RNA_boolean_get(op->ptr, "use_even_offset");
	const bool use_relative_offset = RNA_boolean_get(op->ptr, "use_relative_offset");
	const bool use_edge_rail       = RNA_boolean_get(op->ptr, "use_edge_rail");
	const float thickness          = RNA_float_get(op->ptr,   "thickness");
	const float depth              = RNA_float_get(op->ptr,   "depth");
	const bool use_outset          = RNA_boolean_get(op->ptr, "use_outset");
	const bool use_select_inset    = RNA_boolean_get(op->ptr, "use_select_inset"); /* not passed onto the BMO */
	const bool use_individual      = RNA_boolean_get(op->ptr, "use_individual");
	const bool use_interpolate     = RNA_boolean_get(op->ptr, "use_interpolate");

	opdata = op->customdata;
	em = opdata->em;

	if (opdata->is_modal) {
		EDBM_redo_state_restore(opdata->mesh_backup, em, false);
	}

	if (use_individual) {
		EDBM_op_init(em, &bmop, op,
		             "inset_individual faces=%hf use_even_offset=%b  use_relative_offset=%b"
		             "use_interpolate=%b thickness=%f depth=%f",
		             BM_ELEM_SELECT, use_even_offset, use_relative_offset, use_interpolate,
		             thickness, depth);
	}
	else {
		EDBM_op_init(em, &bmop, op,
		             "inset_region faces=%hf use_boundary=%b use_even_offset=%b use_relative_offset=%b"
		             " use_interpolate=%b thickness=%f depth=%f use_outset=%b use_edge_rail=%b",
		             BM_ELEM_SELECT, use_boundary, use_even_offset, use_relative_offset, use_interpolate,
		             thickness, depth, use_outset, use_edge_rail);
	}
	BMO_op_exec(em->bm, &bmop);

	if (use_select_inset) {
		/* deselect original faces/verts */
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);
	}
	else {
		EDBM_flag_disable_all(em, BM_ELEM_SELECT);
		BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_in, "faces", BM_FACE, BM_ELEM_SELECT, true);
	}

	if (!EDBM_op_finish(em, &bmop, op, true)) {
		return false;
	}
	else {
		EDBM_update_generic(em, true, true);
		return true;
	}
}

static int edbm_inset_exec(bContext *C, wmOperator *op)
{
	if (!edbm_inset_init(C, op, false)) {
		return OPERATOR_CANCELLED;
	}

	if (!edbm_inset_calc(op)) {
		edbm_inset_exit(C, op);
		return OPERATOR_CANCELLED;
	}

	edbm_inset_exit(C, op);
	return OPERATOR_FINISHED;
}

static int edbm_inset_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
	RegionView3D *rv3d = CTX_wm_region_view3d(C);
	InsetData *opdata;
	float mlen[2];
	float center_3d[3];

	if (!edbm_inset_init(C, op, true)) {
		return OPERATOR_CANCELLED;
	}

	opdata = op->customdata;

	/* initialize mouse values */
	if (!calculateTransformCenter(C, V3D_CENTROID, center_3d, opdata->mcenter)) {
		/* in this case the tool will likely do nothing,
		 * ideally this will never happen and should be checked for above */
		opdata->mcenter[0] = opdata->mcenter[1] = 0;
	}
	mlen[0] = opdata->mcenter[0] - event->mval[0];
	mlen[1] = opdata->mcenter[1] - event->mval[1];
	opdata->initial_length = len_v2(mlen);
	opdata->pixel_size = rv3d ? ED_view3d_pixel_size(rv3d, center_3d) : 1.0f;

	edbm_inset_calc(op);

	edbm_inset_update_header(op, C);

	WM_event_add_modal_handler(C, op);
	return OPERATOR_RUNNING_MODAL;
}

static int edbm_inset_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	InsetData *opdata = op->customdata;
	const bool has_numinput = hasNumInput(&opdata->num_input);

	/* Modal numinput active, try to handle numeric inputs first... */
	if (event->val == KM_PRESS && has_numinput && handleNumInput(C, &opdata->num_input, event)) {
		float amounts[2] = {RNA_float_get(op->ptr, "thickness"),
		                    RNA_float_get(op->ptr, "depth")};
		applyNumInput(&opdata->num_input, amounts);
		amounts[0] = max_ff(amounts[0], 0.0f);
		RNA_float_set(op->ptr, "thickness", amounts[0]);
		RNA_float_set(op->ptr, "depth", amounts[1]);

		if (edbm_inset_calc(op)) {
			edbm_inset_update_header(op, C);
			return OPERATOR_RUNNING_MODAL;
		}
		else {
			edbm_inset_cancel(C, op);
			return OPERATOR_CANCELLED;
		}
	}
	else {
		bool handled = false;
		switch (event->type) {
			case ESCKEY:
			case RIGHTMOUSE:
				edbm_inset_cancel(C, op);
				return OPERATOR_CANCELLED;

			case MOUSEMOVE:
				if (!has_numinput) {
					float mdiff[2];
					float amount;

					mdiff[0] = opdata->mcenter[0] - event->mval[0];
					mdiff[1] = opdata->mcenter[1] - event->mval[1];

					if (opdata->modify_depth)
						amount = opdata->old_depth     + ((len_v2(mdiff) - opdata->initial_length) * opdata->pixel_size);
					else
						amount = opdata->old_thickness - ((len_v2(mdiff) - opdata->initial_length) * opdata->pixel_size);

					/* Fake shift-transform... */
					if (opdata->shift)
						amount = (amount - opdata->shift_amount) * 0.1f + opdata->shift_amount;

					if (opdata->modify_depth)
						RNA_float_set(op->ptr, "depth", amount);
					else {
						amount = max_ff(amount, 0.0f);
						RNA_float_set(op->ptr, "thickness", amount);
					}

					if (edbm_inset_calc(op))
						edbm_inset_update_header(op, C);
					else {
						edbm_inset_cancel(C, op);
						return OPERATOR_CANCELLED;
					}
					handled = true;
				}
				break;

			case LEFTMOUSE:
			case PADENTER:
			case RETKEY:
				edbm_inset_calc(op);
				edbm_inset_exit(C, op);
				return OPERATOR_FINISHED;

			case LEFTSHIFTKEY:
			case RIGHTSHIFTKEY:
				if (event->val == KM_PRESS) {
					if (opdata->modify_depth)
						opdata->shift_amount = RNA_float_get(op->ptr, "depth");
					else
						opdata->shift_amount = RNA_float_get(op->ptr, "thickness");
					opdata->shift = true;
					handled = true;
				}
				else {
					opdata->shift_amount = 0.0f;
					opdata->shift = false;
					handled = true;
				}
				break;

			case LEFTCTRLKEY:
			case RIGHTCTRLKEY:
			{
				float mlen[2];

				mlen[0] = opdata->mcenter[0] - event->mval[0];
				mlen[1] = opdata->mcenter[1] - event->mval[1];

				if (event->val == KM_PRESS) {
					opdata->old_thickness = RNA_float_get(op->ptr, "thickness");
					if (opdata->shift)
						opdata->shift_amount = opdata->old_thickness;
					opdata->modify_depth = true;
				}
				else {
					opdata->old_depth = RNA_float_get(op->ptr, "depth");
					if (opdata->shift)
						opdata->shift_amount = opdata->old_depth;
					opdata->modify_depth = false;
				}
				opdata->initial_length = len_v2(mlen);

				edbm_inset_update_header(op, C);
				handled = true;
				break;
			}

			case OKEY:
				if (event->val == KM_PRESS) {
					const bool use_outset = RNA_boolean_get(op->ptr, "use_outset");
					RNA_boolean_set(op->ptr, "use_outset", !use_outset);
					if (edbm_inset_calc(op)) {
						edbm_inset_update_header(op, C);
					}
					else {
						edbm_inset_cancel(C, op);
						return OPERATOR_CANCELLED;
					}
					handled = true;
				}
				break;
			case BKEY:
				if (event->val == KM_PRESS) {
					const bool use_boundary = RNA_boolean_get(op->ptr, "use_boundary");
					RNA_boolean_set(op->ptr, "use_boundary", !use_boundary);
					if (edbm_inset_calc(op)) {
						edbm_inset_update_header(op, C);
					}
					else {
						edbm_inset_cancel(C, op);
						return OPERATOR_CANCELLED;
					}
					handled = true;
				}
				break;
			case IKEY:
				if (event->val == KM_PRESS) {
					const bool use_individual = RNA_boolean_get(op->ptr, "use_individual");
					RNA_boolean_set(op->ptr, "use_individual", !use_individual);
					if (edbm_inset_calc(op)) {
						edbm_inset_update_header(op, C);
					}
					else {
						edbm_inset_cancel(C, op);
						return OPERATOR_CANCELLED;
					}
					handled = true;
				}
				break;
		}

		/* Modal numinput inactive, try to handle numeric inputs last... */
		if (!handled && event->val == KM_PRESS && handleNumInput(C, &opdata->num_input, event)) {
			float amounts[2] = {RNA_float_get(op->ptr, "thickness"),
			                    RNA_float_get(op->ptr, "depth")};
			applyNumInput(&opdata->num_input, amounts);
			amounts[0] = max_ff(amounts[0], 0.0f);
			RNA_float_set(op->ptr, "thickness", amounts[0]);
			RNA_float_set(op->ptr, "depth", amounts[1]);

			if (edbm_inset_calc(op)) {
				edbm_inset_update_header(op, C);
				return OPERATOR_RUNNING_MODAL;
			}
			else {
				edbm_inset_cancel(C, op);
				return OPERATOR_CANCELLED;
			}
		}
	}

	return OPERATOR_RUNNING_MODAL;
}


void MESH_OT_inset(wmOperatorType *ot)
{
	PropertyRNA *prop;

	/* identifiers */
	ot->name = "Inset Faces";
	ot->idname = "MESH_OT_inset";
	ot->description = "Inset new faces into selected faces";

	/* api callbacks */
	ot->invoke = edbm_inset_invoke;
	ot->modal = edbm_inset_modal;
	ot->exec = edbm_inset_exec;
	ot->cancel = edbm_inset_cancel;
	ot->poll = ED_operator_editmesh;

	/* flags */
	ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO | OPTYPE_GRAB_POINTER | OPTYPE_BLOCKING;

	/* properties */
	RNA_def_boolean(ot->srna, "use_boundary",        true, "Boundary",  "Inset face boundaries");
	RNA_def_boolean(ot->srna, "use_even_offset",     true, "Offset Even",      "Scale the offset to give more even thickness");
	RNA_def_boolean(ot->srna, "use_relative_offset", false, "Offset Relative", "Scale the offset by surrounding geometry");
	RNA_def_boolean(ot->srna, "use_edge_rail",       false, "Edge Rail", "Inset the region along existing edges");

	prop = RNA_def_float(ot->srna, "thickness", 0.01f, 0.0f, FLT_MAX, "Thickness", "", 0.0f, 10.0f);
	/* use 1 rather then 10 for max else dragging the button moves too far */
	RNA_def_property_ui_range(prop, 0.0, 1.0, 0.01, 4);
	prop = RNA_def_float(ot->srna, "depth", 0.0f, -FLT_MAX, FLT_MAX, "Depth", "", -10.0f, 10.0f);
	RNA_def_property_ui_range(prop, -10.0f, 10.0f, 0.01, 4);

	RNA_def_boolean(ot->srna, "use_outset", false, "Outset", "Outset rather than inset");
	RNA_def_boolean(ot->srna, "use_select_inset", true, "Select Outer", "Select the new inset faces");
	RNA_def_boolean(ot->srna, "use_individual", false, "Individual", "Individual Face Inset");
	RNA_def_boolean(ot->srna, "use_interpolate", true, "Interpolate", "Blend face data across the inset");
}
