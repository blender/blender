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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file value2d_gizmo.c
 *  \ingroup edgizmolib
 *
 * \name Value Gizmo
 *
 * \brief Gizmo that can be used to click and drag a value.
 *
 * Use this in cases where it may be useful to have a tool,
 * but the tool doesn't relate to an on-screen handle.
 * eg: smooth or randomize.
 *
 * Exactly how this maps X/Y axis, and draws - may change.
 * The purpose here is to avoid having to write custom modal handlers for each operator.
 *
 * So we can use a single gizmo to make redoing an operator seem modal.
 */

#include <float.h>
#include <math.h>

#include "MEM_guardedalloc.h"

#include "BKE_context.h"
#include "BLI_string.h"

#include "ED_screen.h"
#include "ED_gizmo_library.h"

#include "WM_types.h"
#include "WM_api.h"

/* own includes */
#include "../gizmo_geometry.h"
#include "../gizmo_library_intern.h"

/* -------------------------------------------------------------------- */
/** \name Value Gizmo
 *
 * \{ */

typedef struct ValueInteraction {
	struct {
		float mval[2];
		float prop_value;
	} init;
	struct {
		float prop_value;
		eWM_GizmoFlagTweak tweak_flag;
	} prev;
	float range[2];
} ValueInteraction;

static void gizmo_value_draw(const bContext *UNUSED(C), wmGizmo *UNUSED(gz))
{
	/* pass */
}

static int gizmo_value_modal(
        bContext *C, wmGizmo *gz, const wmEvent *event,
        eWM_GizmoFlagTweak tweak_flag)
{
	ValueInteraction *inter = gz->interaction_data;
	if ((event->type != MOUSEMOVE) && (inter->prev.tweak_flag == tweak_flag)) {
		return OPERATOR_RUNNING_MODAL;
	}
	ARegion *ar = CTX_wm_region(C);
	const float value_scale = 4.0f;  /* Could be option. */
	const float value_range = inter->range[1] - inter->range[0];
	float value_delta = (
	        inter->init.prop_value +
	        (((event->mval[0] - inter->init.mval[0]) / ar->winx) * value_range)) * value_scale;


	if (tweak_flag & WM_GIZMO_TWEAK_SNAP) {
		const double snap = 0.1;
		value_delta = (float)roundf((double)value_delta / snap) * snap;

	}
	if (tweak_flag & WM_GIZMO_TWEAK_PRECISE) {
		value_delta *= 0.1f;
	}
	const float value_final = inter->init.prop_value + value_delta;

	if (value_final != inter->prev.prop_value) {
		/* set the property for the operator and call its modal function */
		wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
		if (WM_gizmo_target_property_is_valid(gz_prop)) {
			WM_gizmo_target_property_float_set(C, gz, gz_prop, value_final);
		}

		{
			ScrArea *sa = CTX_wm_area(C);
			char str[64];
			SNPRINTF(str, "%.4f", value_final);
			ED_area_status_text(sa, str);
		}
	}

	inter->prev.prop_value = value_final;
	inter->prev.tweak_flag = tweak_flag;

	return OPERATOR_RUNNING_MODAL;
}


static int gizmo_value_invoke(
        bContext *UNUSED(C), wmGizmo *gz, const wmEvent *event)
{
	ValueInteraction *inter = MEM_callocN(sizeof(ValueInteraction), __func__);

	inter->init.mval[0] = event->mval[0];
	inter->init.mval[1] = event->mval[1];
	inter->prev.prop_value = -FLT_MAX;

	wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
	if (WM_gizmo_target_property_is_valid(gz_prop)) {
		inter->init.prop_value = WM_gizmo_target_property_float_get(gz, gz_prop);
		if (!WM_gizmo_target_property_float_range_get(gz, gz_prop, inter->range)) {
			inter->range[0] = 0.0f;
			inter->range[1] = 1.0f;
		}
	}

	gz->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
}

static void gizmo_value_exit(bContext *C, wmGizmo *gz, const bool cancel)
{
	ScrArea *sa = CTX_wm_area(C);
	ED_area_status_text(sa, NULL);
	if (cancel) {
		ValueInteraction *inter = gz->interaction_data;
		wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
		if (WM_gizmo_target_property_is_valid(gz_prop)) {
			WM_gizmo_target_property_float_set(C, gz, gz_prop, inter->init.prop_value);
		}
	}
}

static int gizmo_value_test_select(
        bContext *UNUSED(C), wmGizmo *UNUSED(gz), const int UNUSED(mval[2]))
{
	return 0;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Value Gizmo API
 *
 * \{ */

static void GIZMO_GT_value_2d(wmGizmoType *gzt)
{
	/* identifiers */
	gzt->idname = "GIZMO_GT_value_2d";

	/* api callbacks */
	gzt->draw = gizmo_value_draw;
	gzt->invoke = gizmo_value_invoke;
	gzt->exit = gizmo_value_exit;
	gzt->modal = gizmo_value_modal;
	gzt->test_select = gizmo_value_test_select;

	gzt->struct_size = sizeof(wmGizmo);

	WM_gizmotype_target_property_def(gzt, "offset", PROP_FLOAT, 1);
	/* Options: relative / absolute */
}

void ED_gizmotypes_value_2d(void)
{
	WM_gizmotype_append(GIZMO_GT_value_2d);
}

/** \} */
