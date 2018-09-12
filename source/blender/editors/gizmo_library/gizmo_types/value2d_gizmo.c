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

#include "MEM_guardedalloc.h"

#include "BKE_context.h"

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
	float init_mval[2];
	float init_prop_value;
	float range[2];
} ValueInteraction;

static void gizmo_value_draw(const bContext *UNUSED(C), wmGizmo *UNUSED(gz))
{
	/* pass */
}

static int gizmo_value_modal(
        bContext *C, wmGizmo *gz, const wmEvent *event,
        eWM_GizmoFlagTweak UNUSED(tweak_flag))
{
	ARegion *ar = CTX_wm_region(C);
	ValueInteraction *inter = gz->interaction_data;
	const float value_scale = 4.0f;  /* Could be option. */
	const float value_range = inter->range[1] - inter->range[0];
	const float value_delta = (
	        inter->init_prop_value +
	        (((event->mval[0] - inter->init_mval[0]) / ar->winx) * value_range)) * value_scale;

	/* set the property for the operator and call its modal function */
	wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
	if (WM_gizmo_target_property_is_valid(gz_prop)) {
		WM_gizmo_target_property_float_set(C, gz, gz_prop, inter->init_prop_value + value_delta);
	}
	return OPERATOR_RUNNING_MODAL;
}


static int gizmo_value_invoke(
        bContext *UNUSED(C), wmGizmo *gz, const wmEvent *event)
{
	ValueInteraction *inter = MEM_callocN(sizeof(ValueInteraction), __func__);

	inter->init_mval[0] = event->mval[0];
	inter->init_mval[1] = event->mval[1];

	wmGizmoProperty *gz_prop = WM_gizmo_target_property_find(gz, "offset");
	if (WM_gizmo_target_property_is_valid(gz_prop)) {
		inter->init_prop_value = WM_gizmo_target_property_float_get(gz, gz_prop);
		if (!WM_gizmo_target_property_float_range_get(gz, gz_prop, inter->range)) {
			inter->range[0] = 0.0f;
			inter->range[1] = 1.0f;
		}
	}

	gz->interaction_data = inter;

	return OPERATOR_RUNNING_MODAL;
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
