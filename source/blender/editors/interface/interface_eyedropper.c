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

#include "DNA_space_types.h"
#include "DNA_screen_types.h"

#include "BLI_blenlib.h"
#include "BLI_math_vector.h"

#include "BKE_context.h"
#include "BKE_screen.h"

#include "UI_interface.h"

#include "WM_api.h"
#include "WM_types.h"

#include "interface_intern.h"

#include "interface_eyedropper_intern.h"  /* own include */

/* -------------------------------------------------------------------- */
/* Keymap
 */
/** \name Modal Keymap
 * \{ */


wmKeyMap *eyedropper_modal_keymap(wmKeyConfig *keyconf)
{
	static const EnumPropertyItem modal_items[] = {
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
	WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_colorband");
	WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_color");
	WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_color_crypto");
	WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_id");
	WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_depth");
	WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_driver");

	return keymap;
}

wmKeyMap *eyedropper_colorband_modal_keymap(wmKeyConfig *keyconf)
{
	static const EnumPropertyItem modal_items_point[] = {
		{EYE_MODAL_POINT_CANCEL, "CANCEL", 0, "Cancel", ""},
		{EYE_MODAL_POINT_SAMPLE, "SAMPLE_SAMPLE", 0, "Sample a point", ""},
		{EYE_MODAL_POINT_CONFIRM, "SAMPLE_CONFIRM", 0, "Confirm Sampling", ""},
		{EYE_MODAL_POINT_RESET, "SAMPLE_RESET", 0, "Reset Sampling", ""},
		{0, NULL, 0, NULL, NULL}
	};

	wmKeyMap *keymap = WM_modalkeymap_get(keyconf, "Eyedropper ColorBand PointSampling Map");
	if (keymap && keymap->modal_items)
		return keymap;

	keymap = WM_modalkeymap_add(keyconf, "Eyedropper ColorBand PointSampling Map", modal_items_point);

	/* items for modal map */
	WM_modalkeymap_add_item(keymap, ESCKEY, KM_PRESS, KM_ANY, 0, EYE_MODAL_CANCEL);
	WM_modalkeymap_add_item(keymap, BACKSPACEKEY, KM_PRESS, KM_ANY, 0, EYE_MODAL_POINT_REMOVE_LAST);
	WM_modalkeymap_add_item(keymap, RIGHTMOUSE, KM_PRESS, KM_ANY, 0, EYE_MODAL_POINT_CONFIRM);
	WM_modalkeymap_add_item(keymap, RETKEY, KM_RELEASE, KM_ANY, 0, EYE_MODAL_POINT_CONFIRM);
	WM_modalkeymap_add_item(keymap, PADENTER, KM_RELEASE, KM_ANY, 0, EYE_MODAL_POINT_CONFIRM);
	WM_modalkeymap_add_item(keymap, LEFTMOUSE, KM_PRESS, KM_ANY, 0, EYE_MODAL_POINT_SAMPLE);
	WM_modalkeymap_add_item(keymap, SPACEKEY, KM_RELEASE, KM_ANY, 0, EYE_MODAL_POINT_RESET);

	/* assign to operators */
	WM_modalkeymap_assign(keymap, "UI_OT_eyedropper_colorband_point");

	return keymap;
}

/** \} */


/* -------------------------------------------------------------------- */
/* Utility Functions
 */
/** \name Generic Shared Functions
 * \{ */

void eyedropper_draw_cursor_text(const struct bContext *C, const ARegion *ar, const char *name)
{
	const uiFontStyle *fstyle = UI_FSTYLE_WIDGET;
	wmWindow *win = CTX_wm_window(C);
	int x = win->eventstate->x;
	int y = win->eventstate->y;
	const float col_fg[4] = {1.0f, 1.0f, 1.0f, 1.0f};
	const float col_bg[4] = {0.0f, 0.0f, 0.0f, 0.2f};


	if ((name[0] == '\0') ||
	    (BLI_rcti_isect_pt(&ar->winrct, x, y) == false))
	{
		return;
	}

	x = x - ar->winrct.xmin;
	y = y - ar->winrct.ymin;

	y += U.widget_unit;

	UI_fontstyle_draw_simple_backdrop(fstyle, x, y, name, col_fg, col_bg);
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
uiBut *eyedropper_get_property_button_under_mouse(bContext *C, const wmEvent *event)
{
	bScreen *screen = CTX_wm_screen(C);
	ScrArea *sa = BKE_screen_find_area_xy(screen, SPACE_TYPE_ANY, event->x, event->y);
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
