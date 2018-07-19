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
#include "BLI_linklist_stack.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_editmesh.h"
#include "BKE_unit.h"
#include "BKE_layer.h"
#include "BKE_mesh.h"

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
	BMBackup mesh_backup;
} BevelObjectStore;


typedef struct {
	float initial_length[NUM_VALUE_KINDS];
	float scale[NUM_VALUE_KINDS];
	NumInput num_input[NUM_VALUE_KINDS];
	float shift_value[NUM_VALUE_KINDS]; /* The current value when shift is pressed. Negative when shift not active. */
	bool is_modal;

	BevelObjectStore *ob_store;
	uint ob_store_len;

	/* modal only */
	float mcenter[2];
	void *draw_handle_pixel;
	short twflag;
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

		ED_area_status_text(sa, msg);
	}
}

static void bevel_harden_normals(BMEditMesh *em, BMOperator *bmop, float face_strength, int hnmode)
{
	BKE_editmesh_lnorspace_update(em);
	BM_normals_loops_edges_tag(em->bm, true);
	const int cd_clnors_offset = CustomData_get_offset(&em->bm->ldata, CD_CUSTOMLOOPNORMAL);

	BMesh *bm = em->bm;
	BMFace *f;
	BMLoop *l, *l_cur, *l_first;
	BMIter fiter;

	BMOpSlot *nslot = BMO_slot_get(bmop->slots_out, "normals.out");

	BM_ITER_MESH(f, &fiter, bm, BM_FACES_OF_MESH) {
		l_cur = l_first = BM_FACE_FIRST_LOOP(f);
		do {
			if (BM_elem_flag_test(l_cur->v, BM_ELEM_SELECT) && (!BM_elem_flag_test(l_cur->e, BM_ELEM_TAG) ||
				(!BM_elem_flag_test(l_cur, BM_ELEM_TAG) && BM_loop_check_cyclic_smooth_fan(l_cur))))
			{
				if (!BM_elem_flag_test(l_cur->e, BM_ELEM_TAG) && !BM_elem_flag_test(l_cur->prev->e, BM_ELEM_TAG)) {
					const int loop_index = BM_elem_index_get(l_cur);
					short *clnors = BM_ELEM_CD_GET_VOID_P(l_cur, cd_clnors_offset);
					BKE_lnor_space_custom_normal_to_data(bm->lnor_spacearr->lspacearr[loop_index], f->no, clnors);
				}
				else {
					BMVert *v_pivot = l_cur->v;
					float *calc_n = BLI_ghash_lookup(nslot->data.ghash, v_pivot);

					BMEdge *e_next;
					const BMEdge *e_org = l_cur->e;
					BMLoop *lfan_pivot, *lfan_pivot_next;

					lfan_pivot = l_cur;
					e_next = lfan_pivot->e;
					BLI_SMALLSTACK_DECLARE(loops, BMLoop *);
					float cn_wght[3] = { 0.0f, 0.0f, 0.0f }, cn_unwght[3] = { 0.0f, 0.0f, 0.0f };

					while (true) {
						lfan_pivot_next = BM_vert_step_fan_loop(lfan_pivot, &e_next);
						if (lfan_pivot_next) {
							BLI_assert(lfan_pivot_next->v == v_pivot);
						}
						else {
							e_next = (lfan_pivot->e == e_next) ? lfan_pivot->prev->e : lfan_pivot->e;
						}

						BLI_SMALLSTACK_PUSH(loops, lfan_pivot);
						float cur[3];
						mul_v3_v3fl(cur, lfan_pivot->f->no, BM_face_calc_area(lfan_pivot->f));
						add_v3_v3(cn_wght, cur);

						if(BM_elem_flag_test(lfan_pivot->f, BM_ELEM_SELECT))
							add_v3_v3(cn_unwght, cur);

						if (!BM_elem_flag_test(e_next, BM_ELEM_TAG) || (e_next == e_org)) {
							break;
						}
						lfan_pivot = lfan_pivot_next;
					}

					normalize_v3(cn_wght);
					normalize_v3(cn_unwght);
					if (calc_n) {
						mul_v3_fl(cn_wght, face_strength);
						mul_v3_fl(calc_n, 1.0f - face_strength);
						add_v3_v3(calc_n, cn_wght);
						normalize_v3(calc_n);
					}
					while ((l = BLI_SMALLSTACK_POP(loops))) {
						const int l_index = BM_elem_index_get(l);
						short *clnors = BM_ELEM_CD_GET_VOID_P(l, cd_clnors_offset);
						if (calc_n) {
							BKE_lnor_space_custom_normal_to_data(bm->lnor_spacearr->lspacearr[l_index], calc_n, clnors);
						}
						else
							BKE_lnor_space_custom_normal_to_data(bm->lnor_spacearr->lspacearr[l_index], cn_unwght, clnors);
					}
					BLI_ghash_remove(nslot->data.ghash, v_pivot, NULL, MEM_freeN);
				}
			}
		} while ((l_cur = l_cur->next) != l_first);
	}
}

static bool edbm_bevel_init(bContext *C, wmOperator *op, const bool is_modal)
{
	Scene *scene = CTX_data_scene(C);
	BevelData *opdata;
	ViewLayer *view_layer = CTX_data_view_layer(C);
	float pixels_per_inch;
	int i;

	if (is_modal) {
		RNA_float_set(op->ptr, "offset", 0.0f);
	}

	op->customdata = opdata = MEM_mallocN(sizeof(BevelData), "beveldata_mesh_operator");
	uint objects_used_len = 0;

	{
		uint ob_store_len = 0;
		Object **objects = BKE_view_layer_array_from_objects_in_edit_mode_unique_data(view_layer, &ob_store_len);
		opdata->ob_store = MEM_malloc_arrayN(ob_store_len, sizeof(*opdata->ob_store), __func__);
		for (uint ob_index = 0; ob_index < ob_store_len; ob_index++) {
			Object *obedit = objects[ob_index];
			BMEditMesh *em = BKE_editmesh_from_object(obedit);
			if (em->bm->totvertsel > 0) {
				opdata->ob_store[objects_used_len].em = em;
				objects_used_len++;
			}
		}
		MEM_freeN(objects);
		opdata->ob_store_len = objects_used_len;
	}

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

		for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
			opdata->ob_store[ob_index].mesh_backup = EDBM_redo_state_store(opdata->ob_store[ob_index].em);
		}
		opdata->draw_handle_pixel = ED_region_draw_cb_activate(ar->type, ED_region_draw_mouse_line_cb,
			opdata->mcenter, REGION_DRAW_POST_PIXEL);
		G.moving = G_TRANSFORM_EDIT;

		if (v3d) {
			opdata->twflag = v3d->twflag;
			v3d->twflag = 0;
		}
	}

	return true;
}

static bool edbm_bevel_calc(wmOperator *op)
{
	BevelData *opdata = op->customdata;
	BMEditMesh *em;
	BMOperator bmop;
	bool changed = false;

	const float offset = RNA_float_get(op->ptr, "offset");
	const int offset_type = RNA_enum_get(op->ptr, "offset_type");
	const int segments = RNA_int_get(op->ptr, "segments");
	const float profile = RNA_float_get(op->ptr, "profile");
	const bool vertex_only = RNA_boolean_get(op->ptr, "vertex_only");
	const bool clamp_overlap = RNA_boolean_get(op->ptr, "clamp_overlap");
	int material = RNA_int_get(op->ptr, "material");
	const bool loop_slide = RNA_boolean_get(op->ptr, "loop_slide");
	const bool mark_seam = RNA_boolean_get(op->ptr, "mark_seam");
	const bool mark_sharp = RNA_boolean_get(op->ptr, "mark_sharp");
	const float hn_strength = RNA_float_get(op->ptr, "strength");
	const int hnmode = RNA_enum_get(op->ptr, "hnmode");


	for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
		em = opdata->ob_store[ob_index].em;

		/* revert to original mesh */
		if (opdata->is_modal) {
			EDBM_redo_state_restore(opdata->ob_store[ob_index].mesh_backup, em, false);
		}

		if (em->ob) {
			material = CLAMPIS(material, -1, em->ob->totcol - 1);
		}

		EDBM_op_init(em, &bmop, op,
			"bevel geom=%hev offset=%f segments=%i vertex_only=%b offset_type=%i profile=%f clamp_overlap=%b "
			"material=%i loop_slide=%b mark_seam=%b mark_sharp=%b strength=%f hnmode=%i",
			BM_ELEM_SELECT, offset, segments, vertex_only, offset_type, profile,
			clamp_overlap, material, loop_slide, mark_seam, mark_sharp, hn_strength, hnmode);

		BMO_op_exec(em->bm, &bmop);

		if (offset != 0.0f) {
			/* not essential, but we may have some loose geometry that
			 * won't get bevel'd and better not leave it selected */
			EDBM_flag_disable_all(em, BM_ELEM_SELECT);
			BMO_slot_buffer_hflag_enable(em->bm, bmop.slots_out, "faces.out", BM_FACE, BM_ELEM_SELECT, true);
		}

		if(hnmode != BEVEL_HN_NONE)
			bevel_harden_normals(em, &bmop, hn_strength, hnmode);

		/* no need to de-select existing geometry */
		if (!EDBM_op_finish(em, &bmop, op, true)) {
			continue;
		}

		EDBM_mesh_normals_update(em);

		EDBM_update_generic(em, true, true);
		changed = true;
	}
	return changed;
}

static void edbm_bevel_exit(bContext *C, wmOperator *op)
{
	BevelData *opdata = op->customdata;

	ScrArea *sa = CTX_wm_area(C);

	if (sa) {
		ED_area_status_text(sa, NULL);
	}

	if (opdata->is_modal) {
		View3D *v3d = CTX_wm_view3d(C);
		ARegion *ar = CTX_wm_region(C);
		for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
			EDBM_redo_state_free(&opdata->ob_store[ob_index].mesh_backup, NULL, false);
		}
		ED_region_draw_cb_exit(ar->type, opdata->draw_handle_pixel);
		if (v3d) {
			v3d->twflag = opdata->twflag;
		}
		G.moving = 0;
	}
	MEM_SAFE_FREE(opdata->ob_store);
	MEM_SAFE_FREE(op->customdata);
	op->customdata = NULL;
}

static void edbm_bevel_cancel(bContext *C, wmOperator *op)
{
	BevelData *opdata = op->customdata;
	if (opdata->is_modal) {
		for (uint ob_index = 0; ob_index < opdata->ob_store_len; ob_index++) {
			EDBM_redo_state_free(&opdata->ob_store[ob_index].mesh_backup, opdata->ob_store[ob_index].em, true);
			EDBM_update_generic(opdata->ob_store[ob_index].em, false, true);
		}
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
			case UKEY:
				if (event->val == KM_RELEASE)
					break;
				else {
					bool mark_seam = RNA_boolean_get(op->ptr, "mark_seam");
					RNA_boolean_set(op->ptr, "mark_seam", !mark_seam);
					edbm_bevel_calc(op);
					handled = true;
					break;
				}
			case KKEY:
				if (event->val == KM_RELEASE)
					break;
				else {
					bool mark_sharp = RNA_boolean_get(op->ptr, "mark_sharp");
					RNA_boolean_set(op->ptr, "mark_sharp", !mark_sharp);
					edbm_bevel_calc(op);
					handled = true;
					break;
				}
				
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

	static EnumPropertyItem harden_normals_items[] = {
		{ BEVEL_HN_NONE, "HN_NONE", 0, "Off", "Do not use Harden Normals" },
		{ BEVEL_HN_FACE, "HN_FACE", 0, "Face Area", "Use faces as weight" },
		{ BEVEL_HN_ADJ, "HN_ADJ", 0, "Vertex average", "Use adjacent vertices as weight" },
		{ 0, NULL, 0, NULL, NULL },
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
	prop = RNA_def_float(ot->srna, "offset", 0.0f, -1e6f, 1e6f, "Amount", "", 0.0f, 100.0f);
	RNA_def_property_float_array_funcs_runtime(prop, NULL, NULL, mesh_ot_bevel_offset_range_func);
	RNA_def_int(ot->srna, "segments", 1, 1, SEGMENTS_HARD_MAX, "Segments", "Segments for curved edge", 1, 8);
	RNA_def_float(ot->srna, "profile", 0.5f, PROFILE_HARD_MIN, 1.0f, "Profile",
		"Controls profile shape (0.5 = round)", PROFILE_HARD_MIN, 1.0f);
	RNA_def_boolean(ot->srna, "vertex_only", false, "Vertex Only", "Bevel only vertices");
	RNA_def_boolean(ot->srna, "clamp_overlap", false, "Clamp Overlap",
		"Do not allow beveled edges/vertices to overlap each other");
	RNA_def_boolean(ot->srna, "loop_slide", true, "Loop Slide", "Prefer slide along edge to even widths");
	RNA_def_boolean(ot->srna, "mark_seam", false, "Mark Seams", "Mark Seams along beveled edges");
	RNA_def_boolean(ot->srna, "mark_sharp", false, "Mark Sharp", "Mark beveled edges as sharp");
	RNA_def_int(ot->srna, "material", -1, -1, INT_MAX, "Material",
		"Material for bevel faces (-1 means use adjacent faces)", -1, 100);
	RNA_def_float(ot->srna, "strength", 0.5f, 0.0f, 1.0f, "Normal Strength", "Strength of calculated normal", 0.0f, 1.0f);
	RNA_def_enum(ot->srna, "hnmode", harden_normals_items, BEVEL_HN_NONE, "Normal Mode", "Weighting mode for Harden Normals");
}
