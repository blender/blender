/*
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
 */

/** \file
 * \ingroup edinterface
 *
 * Share between interface_eyedropper_*.c files.
 */

#pragma once

/* interface_eyedropper.c */

void eyedropper_draw_cursor_text_window(const struct wmWindow *window, const char *name);
void eyedropper_draw_cursor_text_region(int x, int y, const char *name);
/**
 * Utility to retrieve a button representing a RNA property that is currently under the cursor.
 *
 * This is to be used by any eyedroppers which fetch properties (e.g. UI_OT_eyedropper_driver).
 * Especially during modal operations (e.g. as with the eyedroppers), context cannot be relied
 * upon to provide this information, as it is not updated until the operator finishes.
 *
 * \return A button under the mouse which relates to some RNA Property, or NULL
 */
uiBut *eyedropper_get_property_button_under_mouse(bContext *C, const wmEvent *event);
void datadropper_win_area_find(const struct bContext *C,
                               const int mval[2],
                               int r_mval[2],
                               struct wmWindow **r_win,
                               struct ScrArea **r_area);

/* interface_eyedropper_color.c (expose for color-band picker) */

/**
 * \brief get the color from the screen.
 *
 * Special check for image or nodes where we MAY have HDR pixels which don't display.
 *
 * \note Exposed by 'interface_eyedropper_intern.h' for use with color band picking.
 */
void eyedropper_color_sample_fl(bContext *C, int mx, int my, float r_col[3]);

/* Used for most eye-dropper operators. */
enum {
  EYE_MODAL_CANCEL = 1,
  EYE_MODAL_SAMPLE_CONFIRM,
  EYE_MODAL_SAMPLE_BEGIN,
  EYE_MODAL_SAMPLE_RESET,
};

/* Color-band point sample. */
enum {
  EYE_MODAL_POINT_CANCEL = 1,
  EYE_MODAL_POINT_SAMPLE,
  EYE_MODAL_POINT_CONFIRM,
  EYE_MODAL_POINT_RESET,
  EYE_MODAL_POINT_REMOVE_LAST,
};
