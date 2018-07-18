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
 * The Original Code is Copyright (C) 2009 Blender Foundation.
 * All rights reserved.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_eyedropper_color.c
 *  \ingroup edinterface
 *
 * Eyedropper (RGB Color)
 *
 * Defines:
 * - #UI_OT_eyedropper_color
 */

#include "MEM_guardedalloc.h"

#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "BLI_math_vector.h"
#include "BLI_string.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_screen.h"

#include "RNA_access.h"

#include "BIF_gl.h"

#include "UI_interface.h"

#include "IMB_colormanagement.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

#include "ED_image.h"
#include "ED_node.h"
#include "ED_clip.h"

#include "interface_eyedropper_intern.h"

typedef struct Eyedropper {
	struct ColorManagedDisplay *display;

	PointerRNA ptr;
	PropertyRNA *prop;
	int index;

	float init_col[3]; /* for resetting on cancel */

	bool  accum_start; /* has mouse been pressed */
	float accum_col[3];
	int   accum_tot;

	bool accumulate; /* Color picking for cryptomatte, without accumulation. */
} Eyedropper;

static bool eyedropper_init(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Eyedropper *eye;

	op->customdata = eye = MEM_callocN(sizeof(Eyedropper), "Eyedropper");
	eye->accumulate = !STREQ(op->type->idname, "UI_OT_eyedropper_color_crypto");

	UI_context_active_but_prop_get(C, &eye->ptr, &eye->prop, &eye->index);

	if ((eye->ptr.data == NULL) ||
	    (eye->prop == NULL) ||
	    (RNA_property_editable(&eye->ptr, eye->prop) == false) ||
	    (RNA_property_array_length(&eye->ptr, eye->prop) < 3) ||
	    (RNA_property_type(eye->prop) != PROP_FLOAT))
	{
		return false;
	}

	if (RNA_property_subtype(eye->prop) != PROP_COLOR) {
		const char *display_device;
		float col[4];

		display_device = scene->display_settings.display_device;
		eye->display = IMB_colormanagement_display_get_named(display_device);

		/* store inital color */
		RNA_property_float_get_array(&eye->ptr, eye->prop, col);
		if (eye->display) {
			IMB_colormanagement_display_to_scene_linear_v3(col, eye->display);
		}
		copy_v3_v3(eye->init_col, col);
	}

	return true;
}

static void eyedropper_exit(bContext *C, wmOperator *op)
{
	WM_cursor_modal_restore(CTX_wm_window(C));

	if (op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata = NULL;
	}
}

/* *** eyedropper_color_ helper functions *** */

/**
 * \brief get the color from the screen.
 *
 * Special check for image or nodes where we MAY have HDR pixels which don't display.
 *
 * \note Exposed by 'interface_eyedropper_intern.h' for use with color band picking.
 */
void eyedropper_color_sample_fl(bContext *C, int mx, int my, float r_col[3])
{
	/* we could use some clever */
	Main *bmain = CTX_data_main(C);
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = BKE_screen_find_area_xy(win->screen, SPACE_TYPE_ANY, mx, my);
	const char *display_device = CTX_data_scene(C)->display_settings.display_device;
	struct ColorManagedDisplay *display = IMB_colormanagement_display_get_named(display_device);

	if (sa) {
		if (sa->spacetype == SPACE_IMAGE) {
			ARegion *ar = BKE_area_find_region_xy(sa, RGN_TYPE_WINDOW, mx, my);
			if (ar) {
				SpaceImage *sima = sa->spacedata.first;
				int mval[2] = {mx - ar->winrct.xmin,
				               my - ar->winrct.ymin};

				if (ED_space_image_color_sample(sima, ar, mval, r_col)) {
					return;
				}
			}
		}
		else if (sa->spacetype == SPACE_NODE) {
			ARegion *ar = BKE_area_find_region_xy(sa, RGN_TYPE_WINDOW, mx, my);
			if (ar) {
				SpaceNode *snode = sa->spacedata.first;
				int mval[2] = {mx - ar->winrct.xmin,
				               my - ar->winrct.ymin};

				if (ED_space_node_color_sample(bmain, snode, ar, mval, r_col)) {
					return;
				}
			}
		}
		else if (sa->spacetype == SPACE_CLIP) {
			ARegion *ar = BKE_area_find_region_xy(sa, RGN_TYPE_WINDOW, mx, my);
			if (ar) {
				SpaceClip *sc = sa->spacedata.first;
				int mval[2] = {mx - ar->winrct.xmin,
				               my - ar->winrct.ymin};

				if (ED_space_clip_color_sample(sc, ar, mval, r_col)) {
					return;
				}
			}
		}
	}

	/* fallback to simple opengl picker */
	glReadBuffer(GL_FRONT);
	glReadPixels(mx, my, 1, 1, GL_RGB, GL_FLOAT, r_col);
	glReadBuffer(GL_BACK);

	IMB_colormanagement_display_to_scene_linear_v3(r_col, display);
}

/* sets the sample color RGB, maintaining A */
static void eyedropper_color_set(bContext *C, Eyedropper *eye, const float col[3])
{
	float col_conv[4];

	/* to maintain alpha */
	RNA_property_float_get_array(&eye->ptr, eye->prop, col_conv);

	/* convert from linear rgb space to display space */
	if (eye->display) {
		copy_v3_v3(col_conv, col);
		IMB_colormanagement_scene_linear_to_display_v3(col_conv, eye->display);
	}
	else {
		copy_v3_v3(col_conv, col);
	}

	RNA_property_float_set_array(&eye->ptr, eye->prop, col_conv);

	RNA_property_update(C, &eye->ptr, eye->prop);
}

static void eyedropper_color_sample(bContext *C, Eyedropper *eye, int mx, int my)
{
	/* Accumulate color. */
	float col[3];
	eyedropper_color_sample_fl(C, mx, my, col);

	if (eye->accumulate) {
		add_v3_v3(eye->accum_col, col);
		eye->accum_tot++;
	}
	else {
		copy_v3_v3(eye->accum_col, col);
		eye->accum_tot = 1;
	}

	/* Apply to property. */
	float accum_col[3];
	if (eye->accum_tot > 1) {
		mul_v3_v3fl(accum_col, eye->accum_col, 1.0f / (float)eye->accum_tot);
	}
	else {
		copy_v3_v3(accum_col, eye->accum_col);
	}
	eyedropper_color_set(C, eye, accum_col);
}

static void eyedropper_cancel(bContext *C, wmOperator *op)
{
	Eyedropper *eye = op->customdata;
	eyedropper_color_set(C, eye, eye->init_col);
	eyedropper_exit(C, op);
}

/* main modal status check */
static int eyedropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	Eyedropper *eye = (Eyedropper *)op->customdata;

	/* handle modal keymap */
	if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case EYE_MODAL_CANCEL:
				eyedropper_cancel(C, op);
				return OPERATOR_CANCELLED;
			case EYE_MODAL_SAMPLE_CONFIRM:
				if (eye->accum_tot == 0) {
					eyedropper_color_sample(C, eye, event->x, event->y);
				}
				eyedropper_exit(C, op);
				return OPERATOR_FINISHED;
			case EYE_MODAL_SAMPLE_BEGIN:
				/* enable accum and make first sample */
				eye->accum_start = true;
				eyedropper_color_sample(C, eye, event->x, event->y);
				break;
			case EYE_MODAL_SAMPLE_RESET:
				eye->accum_tot = 0;
				zero_v3(eye->accum_col);
				eyedropper_color_sample(C, eye, event->x, event->y);
				break;
		}
	}
	else if (ELEM(event->type, MOUSEMOVE, INBETWEEN_MOUSEMOVE)) {
		if (eye->accum_start) {
			/* button is pressed so keep sampling */
			eyedropper_color_sample(C, eye, event->x, event->y);
		}
	}

	return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int eyedropper_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	/* init */
	if (eyedropper_init(C, op)) {
		WM_cursor_modal_set(CTX_wm_window(C), BC_EYEDROPPER_CURSOR);

		/* add temp handler */
		WM_event_add_modal_handler(C, op);

		return OPERATOR_RUNNING_MODAL;
	}
	else {
		eyedropper_exit(C, op);
		return OPERATOR_PASS_THROUGH;
	}
}

/* Repeat operator */
static int eyedropper_exec(bContext *C, wmOperator *op)
{
	/* init */
	if (eyedropper_init(C, op)) {

		/* do something */

		/* cleanup */
		eyedropper_exit(C, op);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_PASS_THROUGH;
	}
}

static bool eyedropper_poll(bContext *C)
{
	/* Actual test for active button happens later, since we don't
	 * know which one is active until mouse over. */
	return (CTX_wm_window(C) != NULL);
}

void UI_OT_eyedropper_color(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Eyedropper";
	ot->idname = "UI_OT_eyedropper_color";
	ot->description = "Sample a color from the Blender Window to store in a property";

	/* api callbacks */
	ot->invoke = eyedropper_invoke;
	ot->modal = eyedropper_modal;
	ot->cancel = eyedropper_cancel;
	ot->exec = eyedropper_exec;
	ot->poll = eyedropper_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL;
}

void UI_OT_eyedropper_color_crypto(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Cryptomatte Eyedropper";
	ot->idname = "UI_OT_eyedropper_color_crypto";
	ot->description = "Pick a color from Cryptomatte node Pick output image";

	/* api callbacks */
	ot->invoke = eyedropper_invoke;
	ot->modal = eyedropper_modal;
	ot->cancel = eyedropper_cancel;
	ot->exec = eyedropper_exec;
	ot->poll = eyedropper_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL;
}
