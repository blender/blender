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

/** \file \ingroup editors
 */

#ifndef __ED_PAINT_H__
#define __ED_PAINT_H__

struct ImBuf;
struct Image;
struct UndoStep;
struct UndoType;
struct bContext;
struct wmKeyConfig;
struct wmOperator;

/* paint_ops.c */
void ED_operatortypes_paint(void);
void ED_operatormacros_paint(void);
void ED_keymap_paint(struct wmKeyConfig *keyconf);

/* paint_image.c */
void ED_imapaint_clear_partial_redraw(void);
void ED_imapaint_dirty_region(struct Image *ima, struct ImBuf *ibuf, int x, int y, int w, int h, bool find_old);
void ED_imapaint_bucket_fill(struct bContext *C, float color[3], struct wmOperator *op);

/* paint_image_undo.c */
void ED_image_undo_push_begin(const char *name, int paint_mode);
void ED_image_undo_push_end(void);
void ED_image_undo_restore(struct UndoStep *us);

void ED_image_undosys_type(struct UndoType *ut);

/* paint_curve_undo.c */
void ED_paintcurve_undo_push_begin(const char *name);
void ED_paintcurve_undo_push_end(void);

void ED_paintcurve_undosys_type(struct UndoType *ut);

#endif /* __ED_PAINT_H__ */
