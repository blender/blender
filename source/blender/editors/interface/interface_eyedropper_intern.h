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

#ifndef __INTERFACE_EYEDROPPER_INTERN_H__
#define __INTERFACE_EYEDROPPER_INTERN_H__

/* interface_eyedropper.c */
void eyedropper_draw_cursor_text(const struct bContext *C,
                                 const struct ARegion *region,
                                 const char *name);
uiBut *eyedropper_get_property_button_under_mouse(bContext *C, const wmEvent *event);

/* interface_eyedropper_color.c (expose for color-band picker) */
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

#endif /* __INTERFACE_EYEDROPPER_INTERN_H__ */
