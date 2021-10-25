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

#include "DNA_anim_types.h"
#include "DNA_space_types.h"
#include "DNA_screen_types.h"
#include "DNA_object_types.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"

#include "BLT_translation.h"

#include "BKE_context.h"
#include "BKE_screen.h"
#include "BKE_report.h"
#include "BKE_animsys.h"
#include "BKE_depsgraph.h"
#include "BKE_idcode.h"
#include "BKE_unit.h"

#include "RNA_access.h"
#include "RNA_define.h"

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

/* for Driver eyedropper */
#include "ED_keyframing.h"


/* -------------------------------------------------------------------- */
/* Keymap
 */
/** \name Modal Keymap
 * \{ */

enum {
	EYE_MODAL_CANCEL = 1,
	EYE_MODAL_SAMPLE_CONFIRM,
	EYE_MODAL_SAMPLE_BEGIN,
	EYE_MODAL_SAMPLE_RESET,
};

wmKeyMap *eyedropper_modal_keymap(wmKeyConfig *keyconf)
{
	static EnumPropertyItem modal_items[] = {
		{EYE_MODAL_CANCEL, "CANCEL", 0, "Cancel", ""},
		{EYE_MODAL_SAMPLE_CONFIRM, "SAMPLE_CONFIRM", 0, "Confirm Sampling", ""},
		{EYE_MODAL_SAMPLE_BEGIN, "SAMPLE_BEGIN", 0, "Start Sampling", ""},
		{EYE_MODAL_SAMPLE_RESET, "SAMPLE_RESET", 0, "Reset Sampling", ""},
		{0, NULL, 0, NULL, NULL}
	};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Eyedropper Modal Map");

	/* this function is called for each spacetype, only needs to add map once */
	if (keymap && keymap->modal_items)
		return NULL;

	keymap = WM_modalkeymap_add(keyconf, "Eyedropper Modal Map", modal_items);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, EYE_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_PRESS, KM_ANY, 0, EYE_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, RETKEY, KM_RELEASE, KM_ANY, 0, EYE_MODAL_SAMPLE_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_RELEASE, KM_ANY, 0, EYE_MODAL_SAMPLE_CONFIRM);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_RELEASE, KM_ANY, 0, EYE_MODAL_SAMPLE_CONFIRM);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_ANY, 0, EYE_MODAL_SAMPLE_BEGIN);
	WM_modalkeymap_add_item(keymap, SPACEKEY, KM_RELEASE, KM_ANY, 0, EYE_MODAL_SAMPLE_RESET);

	/* assign to operators */
	WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_color");
	WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_id");
	WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_depth");
	WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_driver");

	return keymap;
}

/** \} */


/* -------------------------------------------------------------------- */
/* Utility Functions
 */
/** \name Generic Shared Functions
 * \{ */

static void eyedropper_draw_cursor_text(const struct bContext *C, ARegion *ar, const char *name)
{
	const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
	wmWindow *win = CTX_wm_window(C);
	int x = win->eventstate->x;
	int y = win->eventstate->y;
	const unsigned char fg[4] = {255, 255, 255, 255};
	const unsigned char bg[4] = {0, 0, 0, 50};


	if ((name[0] == '\0') ||
	    (BLI_rcti_isect_pt(&ar->winrct, x, y) == false))
	{
		return;
	}

	x = x - ar->winrct.xmin;
	y = y - ar->winrct.ymin;

	y += U.widget_unit;

	UI_fontstyle_draw_simple_backdrop(fstyle, x, y, name, fg, bg);
}


/**
 * Utility to retrieve a button representing a RNA property that is currently under the cursor.
 *
 * This is to be used by any eyedroppers which fetch properties (e.g. UI_OT_eyedropper_driver).
 * Especially during modal operations (e.g. as with the eyedroppers), context cannot be relied
 * upon to provide this information, as it is not updated until the operator finishes.
 *
 * \return A button under the mouse which relates to some RNA Property, or NULL
 */
static uiBut *eyedropper_get_property_button_under_mouse(bContext *C, const wmEvent *event)
{
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = BKE_screen_find_area_xy(win->screen, SPACE_TYPE_ANY, event->x, event->y);
	ARegion *ar = BKE_area_find_region_xy(sa, RGN_TYPE_ANY, event->x, event->y);
	
	uiBut *but = ui_but_find_mouse_over(ar, event);
	
	if (ELEM(NULL, but, but->rnapoin.data, but->rnaprop)) {
		return NULL;
	}
	else {
		return but;
	}
}

/** \} */


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

	float init_col[3]; /* for resetting on cancel */

	bool  accum_start; /* has mouse been pressed */
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
 */
static void eyedropper_color_sample_fl(bContext *C, Eyedropper *UNUSED(eye), int mx, int my, float r_col[3])
{

	/* we could use some clever */
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

				if (ED_space_node_color_sample(snode, ar, mval, r_col)) {
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
				else {
					eyedropper_color_set_accum(C, eye);
				}
				eyedropper_exit(C, op);
				return OPERATOR_FINISHED;
			case EYE_MODAL_SAMPLE_BEGIN:
				/* enable accum and make first sample */
				eye->accum_start = true;
				eyedropper_color_sample_accum(C, eye, event->x, event->y);
				break;
			case EYE_MODAL_SAMPLE_RESET:
				eye->accum_tot = 0;
				zero_v3(eye->accum_col);
				eyedropper_color_sample_accum(C, eye, event->x, event->y);
				eyedropper_color_set_accum(C, eye);
				break;
		}
	}
	else if (event->type == MOUSEMOVE) {
		if (eye->accum_start) {
			/* button is pressed so keep sampling */
			eyedropper_color_sample_accum(C, eye, event->x, event->y);
			eyedropper_color_set_accum(C, eye);
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
	PointerRNA ptr;
	PropertyRNA *prop;
	int index_dummy;
	uiBut *but;

	/* Only color buttons */
	if ((CTX_wm_window(C) != NULL) &&
	    (but = UI_context_active_but_prop_get(C, &ptr, &prop, &index_dummy)) &&
	    (but->type == UI_BTYPE_COLOR))
	{
		return 1;
	}

	return 0;
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

	/* properties */
}

/** \} */


/* -------------------------------------------------------------------- */
/* Data Dropper */

/** \name Eyedropper (ID data-blocks)
 *
 * \note: datadropper is only internal name to avoid confusion in this file.
 * \{ */

typedef struct DataDropper {
	PointerRNA ptr;
	PropertyRNA *prop;
	short idcode;
	const char *idcode_name;

	ID *init_id; /* for resetting on cancel */

	ARegionType *art;
	void *draw_handle_pixel;
	char name[200];
} DataDropper;


static void datadropper_draw_cb(const struct bContext *C, ARegion *ar, void *arg)
{
	DataDropper *ddr = arg;
	eyedropper_draw_cursor_text(C, ar, ddr->name);
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
	/* Note we can translate here (instead of on draw time), because this struct has very short lifetime. */
	ddr->idcode_name = TIP_(BKE_idcode_to_name(ddr->idcode));

	PointerRNA ptr = RNA_property_pointer_get(&ddr->ptr, ddr->prop);
	ddr->init_id = ptr.id.data;

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

	WM_event_add_mousemove(C);
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
	ScrArea *sa = BKE_screen_find_area_xy(win->screen, -1, mx, my);

	ScrArea *area_prev = CTX_wm_area(C);
	ARegion *ar_prev = CTX_wm_region(C);

	ddr->name[0] = '\0';

	if (sa) {
		if (sa->spacetype == SPACE_VIEW3D) {
			ARegion *ar = BKE_area_find_region_xy(sa, RGN_TYPE_WINDOW, mx, my);
			if (ar) {
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

static void datadropper_cancel(bContext *C, wmOperator *op)
{
	DataDropper *ddr = op->customdata;
	datadropper_id_set(C, ddr, ddr->init_id);
	datadropper_exit(C, op);
}

/* main modal status check */
static int datadropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	DataDropper *ddr = (DataDropper *)op->customdata;

	/* handle modal keymap */
	if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case EYE_MODAL_CANCEL:
				datadropper_cancel(C, op);
				return OPERATOR_CANCELLED;
			case EYE_MODAL_SAMPLE_CONFIRM:
			{
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
		}
	}
	else if (event->type == MOUSEMOVE) {
		ID *id = NULL;
		datadropper_id_sample_pt(C, ddr, event->x, event->y, &id);
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
	PointerRNA ptr;
	PropertyRNA *prop;
	int index_dummy;
	uiBut *but;

	/* data dropper only supports object data */
	if ((CTX_wm_window(C) != NULL) &&
	    (but = UI_context_active_but_prop_get(C, &ptr, &prop, &index_dummy)) &&
	    (but->type == UI_BTYPE_SEARCH_MENU) &&
	    (but->flag & UI_BUT_VALUE_CLEAR))
	{
		if (prop && RNA_property_type(prop) == PROP_POINTER) {
			StructRNA *type = RNA_property_pointer_type(&ptr, prop);
			const short idcode = RNA_type_to_ID_code(type);
			if ((idcode == ID_OB) || OB_DATA_SUPPORT_ID(idcode)) {
				return 1;
			}
		}
	}

	return 0;
}

void UI_OT_eyedropper_id(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Eyedropper Data-Block";
	ot->idname = "UI_OT_eyedropper_id";
	ot->description = "Sample a data-block from the 3D View to store in a property";

	/* api callbacks */
	ot->invoke = datadropper_invoke;
	ot->modal = datadropper_modal;
	ot->cancel = datadropper_cancel;
	ot->exec = datadropper_exec;
	ot->poll = datadropper_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL;

	/* properties */
}

/** \} */


/* -------------------------------------------------------------------- */
/* Depth Dropper */

/** \name Eyedropper (Depth)
 *
 * \note: depthdropper is only internal name to avoid confusion in this file.
 * \{ */

typedef struct DepthDropper {
	PointerRNA ptr;
	PropertyRNA *prop;

	float init_depth; /* for resetting on cancel */

	bool  accum_start; /* has mouse been presed */
	float accum_depth;
	int   accum_tot;

	ARegionType *art;
	void *draw_handle_pixel;
	char name[200];
} DepthDropper;


static void depthdropper_draw_cb(const struct bContext *C, ARegion *ar, void *arg)
{
	DepthDropper *ddr = arg;
	eyedropper_draw_cursor_text(C, ar, ddr->name);
}


static int depthdropper_init(bContext *C, wmOperator *op)
{
	DepthDropper *ddr;
	int index_dummy;

	SpaceType *st;
	ARegionType *art;

	st = BKE_spacetype_from_id(SPACE_VIEW3D);
	art = BKE_regiontype_from_id(st, RGN_TYPE_WINDOW);

	op->customdata = ddr = MEM_callocN(sizeof(DepthDropper), "DepthDropper");

	UI_context_active_but_prop_get(C, &ddr->ptr, &ddr->prop, &index_dummy);

	/* fallback to the active camera's dof */
	if (ddr->prop == NULL) {
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		if (rv3d && rv3d->persp == RV3D_CAMOB) {
			View3D *v3d = CTX_wm_view3d(C);
			if (v3d->camera && v3d->camera->data && !ID_IS_LINKED_DATABLOCK(v3d->camera->data)) {
				RNA_id_pointer_create(v3d->camera->data, &ddr->ptr);
				ddr->prop = RNA_struct_find_property(&ddr->ptr, "dof_distance");
			}
		}
	}

	if ((ddr->ptr.data == NULL) ||
	    (ddr->prop == NULL) ||
	    (RNA_property_editable(&ddr->ptr, ddr->prop) == false) ||
	    (RNA_property_type(ddr->prop) != PROP_FLOAT))
	{
		return false;
	}

	ddr->art = art;
	ddr->draw_handle_pixel = ED_region_draw_cb_activate(art, depthdropper_draw_cb, ddr, REGION_DRAW_POST_PIXEL);
	ddr->init_depth = RNA_property_float_get(&ddr->ptr, ddr->prop);

	return true;
}

static void depthdropper_exit(bContext *C, wmOperator *op)
{
	WM_cursor_modal_restore(CTX_wm_window(C));

	if (op->customdata) {
		DepthDropper *ddr = (DepthDropper *)op->customdata;

		if (ddr->art) {
			ED_region_draw_cb_exit(ddr->art, ddr->draw_handle_pixel);
		}

		MEM_freeN(op->customdata);

		op->customdata = NULL;
	}
}

/* *** depthdropper id helper functions *** */
/**
 * \brief get the ID from the screen.
 *
 */
static void depthdropper_depth_sample_pt(bContext *C, DepthDropper *ddr, int mx, int my, float *r_depth)
{
	/* we could use some clever */
	wmWindow *win = CTX_wm_window(C);
	ScrArea *sa = BKE_screen_find_area_xy(win->screen, SPACE_TYPE_ANY, mx, my);
	Scene *scene = win->screen->scene;
	UnitSettings *unit = &scene->unit;
	const bool do_split = (unit->flag & USER_UNIT_OPT_SPLIT) != 0;

	ScrArea *area_prev = CTX_wm_area(C);
	ARegion *ar_prev = CTX_wm_region(C);

	ddr->name[0] = '\0';

	if (sa) {
		if (sa->spacetype == SPACE_VIEW3D) {
			ARegion *ar = BKE_area_find_region_xy(sa, RGN_TYPE_WINDOW, mx, my);
			if (ar) {
				View3D *v3d = sa->spacedata.first;
				RegionView3D *rv3d = ar->regiondata;
				/* weak, we could pass in some reference point */
				const float *view_co = v3d->camera ? v3d->camera->obmat[3] : rv3d->viewinv[3];
				const int mval[2] = {
				    mx - ar->winrct.xmin,
				    my - ar->winrct.ymin};
				float co[3];

				CTX_wm_area_set(C, sa);
				CTX_wm_region_set(C, ar);

				/* grr, always draw else we leave stale text */
				ED_region_tag_redraw(ar);

				view3d_operator_needs_opengl(C);

				if (ED_view3d_autodist(scene, ar, v3d, mval, co, true, NULL)) {
					const float mval_center_fl[2] = {
					    (float)ar->winx / 2,
					    (float)ar->winy / 2};
					float co_align[3];

					/* quick way to get view-center aligned point */
					ED_view3d_win_to_3d(v3d, ar, co, mval_center_fl, co_align);

					*r_depth = len_v3v3(view_co, co_align);

					bUnit_AsString(ddr->name, sizeof(ddr->name),
					               (double)*r_depth,
					               4, unit->system, B_UNIT_LENGTH, do_split, false);
				}
				else {
					BLI_strncpy(ddr->name, "Nothing under cursor", sizeof(ddr->name));
				}
			}
		}
	}

	CTX_wm_area_set(C, area_prev);
	CTX_wm_region_set(C, ar_prev);
}

/* sets the sample depth RGB, maintaining A */
static void depthdropper_depth_set(bContext *C, DepthDropper *ddr, const float depth)
{
	RNA_property_float_set(&ddr->ptr, ddr->prop, depth);
	RNA_property_update(C, &ddr->ptr, ddr->prop);
}

/* set sample from accumulated values */
static void depthdropper_depth_set_accum(bContext *C, DepthDropper *ddr)
{
	float depth = ddr->accum_depth;
	if (ddr->accum_tot) {
		depth /= (float)ddr->accum_tot;
	}
	depthdropper_depth_set(C, ddr, depth);
}

/* single point sample & set */
static void depthdropper_depth_sample(bContext *C, DepthDropper *ddr, int mx, int my)
{
	float depth = -1.0f;
	if (depth != -1.0f) {
		depthdropper_depth_sample_pt(C, ddr, mx, my, &depth);
		depthdropper_depth_set(C, ddr, depth);
	}
}

static void depthdropper_depth_sample_accum(bContext *C, DepthDropper *ddr, int mx, int my)
{
	float depth = -1.0f;
	depthdropper_depth_sample_pt(C, ddr, mx, my, &depth);
	if (depth != -1.0f) {
		ddr->accum_depth += depth;
		ddr->accum_tot++;
	}
}

static void depthdropper_cancel(bContext *C, wmOperator *op)
{
	DepthDropper *ddr = op->customdata;
	depthdropper_depth_set(C, ddr, ddr->init_depth);
	depthdropper_exit(C, op);
}

/* main modal status check */
static int depthdropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	DepthDropper *ddr = (DepthDropper *)op->customdata;

	/* handle modal keymap */
	if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case EYE_MODAL_CANCEL:
				depthdropper_cancel(C, op);
				return OPERATOR_CANCELLED;
			case EYE_MODAL_SAMPLE_CONFIRM:
				if (ddr->accum_tot == 0) {
					depthdropper_depth_sample(C, ddr, event->x, event->y);
				}
				else {
					depthdropper_depth_set_accum(C, ddr);
				}
				depthdropper_exit(C, op);
				return OPERATOR_FINISHED;
			case EYE_MODAL_SAMPLE_BEGIN:
				/* enable accum and make first sample */
				ddr->accum_start = true;
				depthdropper_depth_sample_accum(C, ddr, event->x, event->y);
				break;
			case EYE_MODAL_SAMPLE_RESET:
				ddr->accum_tot = 0;
				ddr->accum_depth = 0.0f;
				depthdropper_depth_sample_accum(C, ddr, event->x, event->y);
				depthdropper_depth_set_accum(C, ddr);
				break;
		}
	}
	else if (event->type == MOUSEMOVE) {
		if (ddr->accum_start) {
			/* button is pressed so keep sampling */
			depthdropper_depth_sample_accum(C, ddr, event->x, event->y);
			depthdropper_depth_set_accum(C, ddr);
		}
	}

	return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int depthdropper_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	/* init */
	if (depthdropper_init(C, op)) {
		WM_cursor_modal_set(CTX_wm_window(C), BC_EYEDROPPER_CURSOR);

		/* add temp handler */
		WM_event_add_modal_handler(C, op);

		return OPERATOR_RUNNING_MODAL;
	}
	else {
		depthdropper_exit(C, op);
		return OPERATOR_CANCELLED;
	}
}

/* Repeat operator */
static int depthdropper_exec(bContext *C, wmOperator *op)
{
	/* init */
	if (depthdropper_init(C, op)) {
		/* cleanup */
		depthdropper_exit(C, op);

		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static int depthdropper_poll(bContext *C)
{
	PointerRNA ptr;
	PropertyRNA *prop;
	int index_dummy;
	uiBut *but;

	/* check if there's an active button taking depth value */
	if ((CTX_wm_window(C) != NULL) &&
	    (but = UI_context_active_but_prop_get(C, &ptr, &prop, &index_dummy)) &&
	    (but->type == UI_BTYPE_NUM) &&
	    (prop != NULL))
	{
		if ((RNA_property_type(prop) == PROP_FLOAT) &&
		    (RNA_property_subtype(prop) & PROP_UNIT_LENGTH) &&
		    (RNA_property_array_check(prop) == false))
		{
			return 1;
		}
	}
	else {
		RegionView3D *rv3d = CTX_wm_region_view3d(C);
		if (rv3d && rv3d->persp == RV3D_CAMOB) {
			View3D *v3d = CTX_wm_view3d(C);
			if (v3d->camera && v3d->camera->data && !ID_IS_LINKED_DATABLOCK(v3d->camera->data)) {
				return 1;
			}
		}
	}

	return 0;
}

void UI_OT_eyedropper_depth(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Eyedropper Depth";
	ot->idname = "UI_OT_eyedropper_depth";
	ot->description = "Sample depth from the 3D view";

	/* api callbacks */
	ot->invoke = depthdropper_invoke;
	ot->modal = depthdropper_modal;
	ot->cancel = depthdropper_cancel;
	ot->exec = depthdropper_exec;
	ot->poll = depthdropper_poll;

	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL;

	/* properties */
}

/** \} */

/* -------------------------------------------------------------------- */
/* Eyedropper
 */
 
/* NOTE: This is here (instead of in drivers.c) because we need access the button internals,
 * which we cannot access outside of the interface module
 */

/** \name Eyedropper (Driver Target)
 * \{ */

typedef struct DriverDropper {
	/* Destination property (i.e. where we'll add a driver) */
	PointerRNA ptr;
	PropertyRNA *prop;
	int index;
	
	// TODO: new target?
} DriverDropper;

static bool driverdropper_init(bContext *C, wmOperator *op)
{
	DriverDropper *ddr;
	uiBut *but;

	op->customdata = ddr = MEM_callocN(sizeof(DriverDropper), "DriverDropper");

	but = UI_context_active_but_prop_get(C, &ddr->ptr, &ddr->prop, &ddr->index);

	if ((ddr->ptr.data == NULL) ||
	    (ddr->prop == NULL) ||
	    (RNA_property_editable(&ddr->ptr, ddr->prop) == false) ||
	    (RNA_property_animateable(&ddr->ptr, ddr->prop) == false) ||
	    (but->flag & UI_BUT_DRIVEN))
	{
		return false;
	}
	
	return true;
}

static void driverdropper_exit(bContext *C, wmOperator *op)
{
	WM_cursor_modal_restore(CTX_wm_window(C));

	if (op->customdata) {
		MEM_freeN(op->customdata);
		op->customdata = NULL;
	}
}

static void driverdropper_sample(bContext *C, wmOperator *op, const wmEvent *event)
{
	DriverDropper *ddr = (DriverDropper *)op->customdata;
	uiBut *but = eyedropper_get_property_button_under_mouse(C, event);
	
	short mapping_type = RNA_enum_get(op->ptr, "mapping_type");
	short flag = 0;
	
	/* we can only add a driver if we know what RNA property it corresponds to */
	if (but == NULL) {
		return;
	}
	else {
		/* Get paths for src... */
		PointerRNA *target_ptr = &but->rnapoin;
		PropertyRNA *target_prop = but->rnaprop;
		int target_index = but->rnaindex;
		
		char *target_path = RNA_path_from_ID_to_property(target_ptr, target_prop);
		
		/* ... and destination */
		char *dst_path    = BKE_animdata_driver_path_hack(C, &ddr->ptr, ddr->prop, NULL);
		
		/* Now create driver(s) */
		if (target_path && dst_path) {
			int success = ANIM_add_driver_with_target(op->reports,
			                                          ddr->ptr.id.data, dst_path, ddr->index,
			                                          target_ptr->id.data, target_path, target_index,
			                                          flag, DRIVER_TYPE_PYTHON, mapping_type);
			
			if (success) {
				/* send updates */
				UI_context_update_anim_flag(C);
				DAG_relations_tag_update(CTX_data_main(C));
				DAG_id_tag_update(ddr->ptr.id.data, OB_RECALC_OB | OB_RECALC_DATA);
				WM_event_add_notifier(C, NC_ANIMATION | ND_FCURVES_ORDER, NULL);  // XXX
			}
		}
		
		/* cleanup */
		if (target_path)
			MEM_freeN(target_path);
		if (dst_path)
			MEM_freeN(dst_path);
	}
}

static void driverdropper_cancel(bContext *C, wmOperator *op)
{
	driverdropper_exit(C, op);
}

/* main modal status check */
static int driverdropper_modal(bContext *C, wmOperator *op, const wmEvent *event)
{
	/* handle modal keymap */
	if (event->type == EVT_MODAL_MAP) {
		switch (event->val) {
			case EYE_MODAL_CANCEL:
				driverdropper_cancel(C, op);
				return OPERATOR_CANCELLED;
				
			case EYE_MODAL_SAMPLE_CONFIRM:
				driverdropper_sample(C, op, event);
				driverdropper_exit(C, op);
				
				return OPERATOR_FINISHED;
		}
	}
	
	return OPERATOR_RUNNING_MODAL;
}

/* Modal Operator init */
static int driverdropper_invoke(bContext *C, wmOperator *op, const wmEvent *UNUSED(event))
{
	/* init */
	if (driverdropper_init(C, op)) {
		WM_cursor_modal_set(CTX_wm_window(C), BC_EYEDROPPER_CURSOR);
		
		/* add temp handler */
		WM_event_add_modal_handler(C, op);
		
		return OPERATOR_RUNNING_MODAL;
	}
	else {
		driverdropper_exit(C, op);
		return OPERATOR_CANCELLED;
	}
}

/* Repeat operator */
static int driverdropper_exec(bContext *C, wmOperator *op)
{
	/* init */
	if (driverdropper_init(C, op)) {
		/* cleanup */
		driverdropper_exit(C, op);
		
		return OPERATOR_FINISHED;
	}
	else {
		return OPERATOR_CANCELLED;
	}
}

static int driverdropper_poll(bContext *C)
{
	if (!CTX_wm_window(C)) return 0;
	else return 1;
}

void UI_OT_eyedropper_driver(wmOperatorType *ot)
{
	/* identifiers */
	ot->name = "Eyedropper Driver";
	ot->idname = "UI_OT_eyedropper_driver";
	ot->description = "Pick a property to use as a driver target";
	
	/* api callbacks */
	ot->invoke = driverdropper_invoke;
	ot->modal = driverdropper_modal;
	ot->cancel = driverdropper_cancel;
	ot->exec = driverdropper_exec;
	ot->poll = driverdropper_poll;
	
	/* flags */
	ot->flag = OPTYPE_BLOCKING | OPTYPE_INTERNAL | OPTYPE_UNDO;
	
	/* properties */
	RNA_def_enum(ot->srna, "mapping_type", prop_driver_create_mapping_types, 0,
	             "Mapping Type", "Method used to match target and driven properties");
}

/** \} */
