/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edinterface
 *
 * Share between `interface/eyedropper/` files.
 */

#pragma once

/* `interface_eyedropper.cc` */

void eyedropper_draw_cursor_text_window(const struct wmWindow *window, const char *name);
void eyedropper_draw_cursor_text_region(const int xy[2], const char *name);
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
 * \note Exposed by 'eyedropper_intern.hh' for use with color band picking.
 */
void eyedropper_color_sample_fl(bContext *C, const int m_xy[2], float r_col[3]);

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
