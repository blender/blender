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
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_report.h"
#include "BKE_idcode.h"

#include "RNA_access.h"

#include "BIF_gl.h"

#include "UI_interface.h"

#include "IMB_colormanagement.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

/* for HDR color sampling */
#include "ED_image.h"
#include "ED_node.h"
#include "ED_clip.h"

/* for ID data eyedropper */
#include "ED_space_api.h"
#include "ED_screen.h"
#include "ED_view3d.h"


/* -------------------------------------------------------------------- */
/* Eyedropper
 */

/** \name Eyedropper (RGB Color)
 * \{ */

typedef struct Eyedropper {
	struct ColorManagedDisplay *display;

	PointerRNA ptr;
	PropertyRNA *prop;
	int index;

	bool  accum_start; /* has mouse been presed */
	float accum_col[3];
	int   accum_tot;
} Eyedropper;

static bool eyedropper_init(bContext *C, wmOperator *op)
{
	Scene *scene = CTX_data_scene(C);
	Eyedropper *eye;

	op->customdata = eye = MEM_callocN(sizeof(Eyedropper), "Eyedropper");

	UI_context_active_but_prop_get(C, &eye->ptr, &eye->prop, &eye->index);

	if ((eye->ptr.data == NULL) ||
	    (eye->prop == NULL) ||
	    (RNA_property_editable(&eye->ptr, eye->prop) == false) ||
	    (RNA_property_array_length(&eye->ptr, eye->prop) < 3) ||
	    (RNA_property_type(eye->prop) != PROP_FLOAT))
	{
		return false;
	}

	if (RNA_property_subtype(eye->prop) == PROP_COLOR) {
		const char *display_device;

		display_device = scene->display_settings.display_device;
		eye->display = IMB_colormanagement_display_get_named(display_device);
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

static void eyedropper_cancel(bContext *C, wmOperator *op)
{
	eyedropper_exit(C, op);
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

					if (ED_space_image_color_sample(CTX_data_scene(C), sima, ar, mval, r_col)) {
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

					if (ED_space_node_color_sample(CTX_data_scene(C), snode, ar, mval, r_col)) {
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

					if (ED_space_clip_color_sample(CTX_data_scene(C), sc, ar, mval, r_col)) {
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
			eyedropper_cancel(C, op);
			return OPERATOR_CANCELLED;
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
				eye->accum_start = true;
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
	ot->description = "Sample a data-block from the 3D view";

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


/* -------------------------------------------------------------------- */
/* Data Dropper
 *
 * note: datadropper is only internal name to avoid confusion in this file
 */

/** \name Eyedropper (ID data-blocks)
 * \{ */

typedef struct DataDropper {
	PointerRNA ptr;
	PropertyRNA *prop;
	short idcode;
	const char *idcode_name;

	ARegionType *art;
	void *draw_handle_pixel;
	char name[200];
} DataDropper;


static void datadropper_draw_cb(const struct bContext *C, ARegion *ar, void *arg)
{
	DataDropper *ddr = arg;
	int width;
	const char *name = ddr->name;
	wmWindow *win = CTX_wm_window(C);
	int x = win->eventstate->x;
	int y = win->eventstate->y;

	if ((name[0] == '\0') ||
	    (BLI_rcti_isect_pt(&ar->winrct, x, y) == false))
	{
		return;
	}

	width = UI_fontstyle_string_width(name);
	x = x - ar->winrct.xmin;
	y = y - ar->winrct.ymin;

	y += 20;

	glColor4ub(0, 0, 0, 50);

	UI_draw_roundbox_corner_set(UI_CNR_ALL | UI_RB_ALPHA);
	UI_draw_roundbox(x, y, x + width + 8, y + 15, 4);

	glColor4ub(255, 255, 255, 255);
	UI_draw_string(x + 4, y + 4, name);
}


static int datadropper_init(bContext *C, wmOperator *op)
{
	DataDropper *ddr;
	int index_dummy;
	StructRNA *type;

	SpaceType *st;
	ARegionType *art;

	st = BKE_spacetype_from_id(SPACE_VIEW3D);
	art = BKE_regiontype_from_id(st, RGN_TYPE_WINDOW);

	op->customdata = ddr = MEM_callocN(sizeof(DataDropper), "DataDropper");

	UI_context_active_but_prop_get(C, &ddr->ptr, &ddr->prop, &index_dummy);

	if ((ddr->ptr.data == NULL) ||
	    (ddr->prop == NULL) ||
	    (RNA_property_editable(&ddr->ptr, ddr->prop) == false) ||
	    (RNA_property_type(ddr->prop) != PROP_POINTER))
	{
		return false;
	}

	ddr->art = art;
	ddr->draw_handle_pixel = ED_region_draw_cb_activate(art, datadropper_draw_cb, ddr, REGION_DRAW_POST_PIXEL);

	type = RNA_property_pointer_type(&ddr->ptr, ddr->prop);
	ddr->idcode = RNA_type_to_ID_code(type);
	BLI_assert(ddr->idcode != 0);
	ddr->idcode_name = BKE_idcode_to_name(ddr->idcode);

	return true;
}

static void datadropper_exit(bContext *C, wmOperator *op)
{
	WM_cursor_modal_restore(CTX_wm_window(C));

	if (op->customdata) {
		DataDropper *ddr = (DataDropper *)op->customdata;

		if (ddr->art) {
			ED_region_draw_cb_exit(ddr->art, ddr->draw_handle_pixel);
		}

		MEM_freeN(op->customdata);

		op->customdata = NULL;
	}
}

static void datadropper_cancel(bContext *C, wmOperator *op)
{
	datadropper_exit(C, op);
}

/* *** datadropper id helper functions *** */
/**
 * \brief get the ID from the screen.
 *
 */
static void datadropper_id_sample_pt(bContext *C, DataDropper *ddr, int mx, int my, ID **r_id)
{

	/* we could use some clever */
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa;

	ScrArea *area_prev = CTX_wm_area(C);
	ARegion *ar_prev = CTX_wm_region(C);

	ddr->name[0] = '\0';

	for (sa = win->screen->areabase.first; sa; sa = sa->next) {
		if (BLI_rcti_isect_pt(&sa->totrct, mx, my)) {
			if (sa->spacetype == SPACE_VIEW3D) {
				ARegion *ar = BKE_area_find_region_type(sa, RGN_TYPE_WINDOW);
				if (ar && BLI_rcti_isect_pt(&ar->winrct, mx, my)) {
					const int mval[2] = {
					    mx - ar->winrct.xmin,
					    my - ar->winrct.ymin};
					Base *base;

					CTX_wm_area_set(C, sa);
					CTX_wm_region_set(C, ar);

					/* grr, always draw else we leave stale text */
					ED_region_tag_redraw(ar);

					base = ED_view3d_give_base_under_cursor(C, mval);
					if (base) {
						Object *ob = base->object;
						ID *id = NULL;
						if (ddr->idcode == ID_OB) {
							id = (ID *)ob;
						}
						else if (ob->data) {
							if (GS(((ID *)ob->data)->name) == ddr->idcode) {
								id = (ID *)ob->data;
							}
							else {
								BLI_snprintf(ddr->name, sizeof(ddr->name), "Incompatible, expected a %s",
								             ddr->idcode_name);
							}
						}

						if (id) {
							BLI_snprintf(ddr->name, sizeof(ddr->name), "%s: %s",
							             ddr->idcode_name, id->name + 2);
							*r_id = id;
						}

						break;
					}
				}
			}
		}
	}

	CTX_wm_area_set(C, area_prev);
	CTX_wm_region_set(C, ar_prev);
}

/* sets the ID, returns success */
static bool datadropper_id_set(bContext *C, DataDropper *ddr, ID *id)
{
	PointerRNA ptr_value;

	RNA_id_pointer_create(id, &ptr_value);

	RNA_property_pointer_set(&ddr->ptr, ddr->prop, ptr_value);

	RNA_property_update(C, &ddr->ptr, ddr->prop);

	ptr_value = RNA_property_pointer_get(&ddr->ptr, ddr->prop);

	return (ptr_value.id.data == id);
}

/* single point sample & set */
static bool datadropper_id_sample(bContext *C, DataDropper *ddr, int mx, int my)
{
	ID *id = NULL;

	datadropper_id_sample_pt(C, ddr, mx, my, &id);
	return datadropper_id_set(C, ddr, id);
}

/* main modal status check */
static int datadropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	DataDropper *ddr = (DataDropper *)op->customdata;

	switch (event->type) {
		case ESCKEY:
		case RIGHTMOUSE:
			datadropper_cancel(C, op);
			return OPERATOR_CANCELLED;
		case LEFTMOUSE:
			if (event->val == KM_RELEASE) {
				bool success;

				success = datadropper_id_sample(C, ddr, event->x, event->y);
				datadropper_exit(C, op);

				if (success) {
					return OPERATOR_FINISHED;
				}
				else {
					BKE_report(op->reports, RPT_WARNING, "Failed to set value");
					return OPERATOR_CANCELLED;
				}
			}
			break;
		case MOUSEMOVE:
		{
			ID *id = NULL;
			datadropper_id_sample_pt(C, ddr, event->x, event->y, &id);
			break;
		}
	}

	return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int datadropper_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	/* init */
	if (datadropper_init(C, op)) {
		WM_cursor_modal_set(CTX_wm_window(C), BC_EYEDROPPER_CURSOR);

		/* add temp handler */
		WM_event_add_modal_handler(C, op);

		return OPERATOR_RUNNING_MODAL;
	}
	else {
		datadropper_exit(C, op);
		return OPERATOR_CANCELLED;
	}
}

/* Repeat operator */
static int datadropper_exec(bContext *C, wmOperator *op)
{
	/* init */
	if (datadropper_init(C, op)) {
		/* cleanup */
		datadropper_exit(C, op);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static int datadropper_poll(bContext *C)
{
	if (!CTX_wm_window(C)) return 0;
	else return 1;
}

void UI_OT_eyedropper_id(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Eyedropper Datablock";
	ot->idname = "UI_OT_eyedropper_id";
	ot->description = "Sample a color from the Blender Window to store in a property";

	/* api callbacks */
	ot->invoke = datadropper_invoke;
	ot->modal = datadropper_modal;
	ot->cancel = datadropper_cancel;
	ot->exec = datadropper_exec;
	ot->poll = datadropper_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING;

	/* properties */
}

/** \} */
