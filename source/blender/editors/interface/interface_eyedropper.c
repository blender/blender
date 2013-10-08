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
 * Contributor(s): Blender Foundation, Joshua Leung
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/editors/interface/interface_eyedropper.c
 *  \ingroup edinterface
 */

#include "MEM_guardedalloc.h"

#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "RNA_access.h"

#include "BIF_gl.h"

#include "UI_interface.h"

#include "IMB_colormanagement.h"

#include "interface_intern.h"

#include "WM_api.h"
#include "WM_types.h"

/* for HDR color sampling */
#include "ED_image.h"
#include "ED_node.h"
#include "ED_clip.h"


/** \name Eyedropper (RGB Color)
 * \{ */

typedef struct Eyedropper {
	struct ColorManagedDisplay *display;

	PointerRNA ptr;
	PropertyRNA *prop;
	int index;

	int   accum_start; /* has mouse been presed */
	float accum_col[3];
	int   accum_tot;
} Eyedropper;

static int eyedropper_init(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Eyedropper *eye;

	op->customdata = eye = MEM_callocN(sizeof(Eyedropper), "Eyedropper");

	uiContextActiveProperty(C, &eye->ptr, &eye->prop, &eye->index);

	if ((eye->ptr.data == NULL) ||
	    (eye->prop == NULL) ||
	    (RNA_property_editable(&eye->ptr, eye->prop) == FALSE) ||
	    (RNA_property_array_length(&eye->ptr, eye->prop) < 3) ||
	    (RNA_property_type(eye->prop) != PROP_FLOAT))
	{
		return FALSE;
	}

	if (RNA_property_subtype(eye->prop) == PROP_COLOR) {
		const char *display_device;

		display_device = scene->display_settings.display_device;
		eye->display = IMB_colormanagement_display_get_named(display_device);
	}

	return TRUE;
}

static void eyedropper_exit(bContext *C, wmOperator *op)
{
	WM_cursor_modal_restore(CTX_wm_window(C));

	if (op->customdata)
		MEM_freeN(op->customdata);
	op->customdata = NULL;
}

static int eyedropper_cancel(bContext *C, wmOperator *op)
{
	eyedropper_exit(C, op);
	return OPERATOR_CANCELLED;
}

/* *** eyedropper_color_ helper functions *** */

/**
 * \brief get the color from the screen.
 *
 * Special check for image or nodes where we MAY have HDR pixels which don't display.
 */
static void eyedropper_color_sample_fl(bContext *C, Eyedropper *UNUSED(eye), int mx, int my, float r_col[3])
{

	/* we could use some clever */
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa;
	for (sa = win->screen->areabase.first; sa; sa = sa->next) {
		if (BLI_rcti_isect_pt(&sa->totrct, mx, my)) {
			if (sa->spacetype == SPACE_IMAGE) {
				ARegion *ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
				if (ar && BLI_rcti_isect_pt(&ar->winrct, mx, my)) {
					SpaceImage *sima = sa->spacedata.first;
					int mval[2] = {mx - ar->winrct.xmin,
					               my - ar->winrct.ymin};

					if (ED_space_image_color_sample(sima, ar, mval, r_col)) {
						return;
					}
				}
			}
			else if (sa->spacetype == SPACE_NODE) {
				ARegion *ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
				if (ar && BLI_rcti_isect_pt(&ar->winrct, mx, my)) {
					SpaceNode *snode = sa->spacedata.first;
					int mval[2] = {mx - ar->winrct.xmin,
					               my - ar->winrct.ymin};

					if (ED_space_node_color_sample(snode, ar, mval, r_col)) {
						return;
					}
				}
			}
			else if (sa->spacetype == SPACE_CLIP) {
				ARegion *ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
				if (ar && BLI_rcti_isect_pt(&ar->winrct, mx, my)) {
					SpaceClip *sc = sa->spacedata.first;
					int mval[2] = {mx - ar->winrct.xmin,
					               my - ar->winrct.ymin};

					if (ED_space_clip_color_sample(sc, ar, mval, r_col)) {
						return;
					}
				}
			}
		}
	}

	/* fallback to simple opengl picker */
	glReadBuffer(GL_FRONT);
	glReadPixels(mx, my, 1, 1, GL_RGB, GL_FLOAT, r_col);
	glReadBuffer(GL_BACK);
}

/* sets the sample color RGB, maintaining A */
static void eyedropper_color_set(bContext *C, Eyedropper *eye, const float col[3])
{
	float col_conv[4];

	/* to maintain alpha */
	RNA_property_float_get_array(&eye->ptr, eye->prop, col_conv);

	/* convert from display space to linear rgb space */
	if (eye->display) {
		copy_v3_v3(col_conv, col);
		IMB_colormanagement_display_to_scene_linear_v3(col_conv, eye->display);
	}
	else {
		copy_v3_v3(col_conv, col);
	}

	RNA_property_float_set_array(&eye->ptr, eye->prop, col_conv);

	RNA_property_update(C, &eye->ptr, eye->prop);
}

/* set sample from accumulated values */
static void eyedropper_color_set_accum(bContext *C, Eyedropper *eye)
{
	float col[3];
	mul_v3_v3fl(col, eye->accum_col, 1.0f / (float)eye->accum_tot);
	eyedropper_color_set(C, eye, col);
}

/* single point sample & set */
static void eyedropper_color_sample(bContext *C, Eyedropper *eye, int mx, int my)
{
	float col[3];
	eyedropper_color_sample_fl(C, eye, mx, my, col);
	eyedropper_color_set(C, eye, col);
}

static void eyedropper_color_sample_accum(bContext *C, Eyedropper *eye, int mx, int my)
{
	float col[3];
	eyedropper_color_sample_fl(C, eye, mx, my, col);
	/* delay linear conversion */
	add_v3_v3(eye->accum_col, col);
	eye->accum_tot++;
}

/* main modal status check */
static int eyedropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	Eyedropper *eye = (Eyedropper *)op->customdata;

	switch (event->type) {
		case ESCKEY:
		case RIGHTMOUSE:
			return eyedropper_cancel(C, op);
		case LEFTMOUSE:
			if (event->val == KM_RELEASE) {
				if (eye->accum_tot == 0) {
					eyedropper_color_sample(C, eye, event->x, event->y);
				}
				else {
					eyedropper_color_set_accum(C, eye);
				}
				eyedropper_exit(C, op);
				return OPERATOR_FINISHED;
			}
			else if (event->val == KM_PRESS) {
				/* enable accum and make first sample */
				eye->accum_start = TRUE;
				eyedropper_color_sample_accum(C, eye, event->x, event->y);
			}
			break;
		case MOUSEMOVE:
			if (eye->accum_start) {
				/* button is pressed so keep sampling */
				eyedropper_color_sample_accum(C, eye, event->x, event->y);
				eyedropper_color_set_accum(C, eye);
			}
			break;
		case SPACEKEY:
			if (event->val == KM_RELEASE) {
				eye->accum_tot = 0;
				zero_v3(eye->accum_col);
				eyedropper_color_sample_accum(C, eye, event->x, event->y);
				eyedropper_color_set_accum(C, eye);
			}
			break;
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
		return OPERATOR_CANCELLED;
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
		return OPERATOR_CANCELLED;
	}
}

static int eyedropper_poll(bContext *C)
{
	if (!CTX_wm_window(C)) return 0;
	else return 1;
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
	ot->flag = OPTYPE_BLOCKING;

	/* properties */
}

/** \} */
