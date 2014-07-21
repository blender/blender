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

/** \file ED_paint.h
 *  \ingroup editors
 */

#ifndef __ED_PAINT_H__
#define __ED_PAINT_H__

struct bContext;
struct RegionView3D;
struct wmKeyConfig;
struct wmOperator;

/* paint_ops.c */
void ED_operatortypes_paint(void);
void ED_operatormacros_paint(void);
void ED_keymap_paint(struct wmKeyConfig *keyconf);

/* paint_undo.c */
enum {
	UNDO_PAINT_IMAGE    = 0,
	UNDO_PAINT_MESH     = 1,
};

typedef void (*UndoRestoreCb)(struct bContext *C, struct ListBase *lb);
typedef void (*UndoFreeCb)(struct ListBase *lb);
typedef bool (*UndoCleanupCb)(struct bContext *C, struct ListBase *lb);

int ED_undo_paint_step(struct bContext *C, int type, int step, const char *name);
void ED_undo_paint_step_num(struct bContext *C, int type, int num);
const char *ED_undo_paint_get_name(struct bContext *C, int type, int nr, int *active);
void ED_undo_paint_free(void);
int ED_undo_paint_valid(int type, const char *name);
bool ED_undo_paint_empty(int type);
void ED_undo_paint_push_begin(int type, const char *name, UndoRestoreCb restore, UndoFreeCb free, UndoCleanupCb cleanup);
void ED_undo_paint_push_end(int type);

/* paint_image.c */
/* image painting specific undo */
void ED_image_undo_restore(struct bContext *C, struct ListBase *lb);
void ED_image_undo_free(struct ListBase *lb);
void ED_imapaint_clear_partial_redraw(void);
void ED_imapaint_dirty_region(struct Image *ima, struct ImBuf *ibuf, int x, int y, int w, int h);
void ED_imapaint_bucket_fill(struct bContext *C, float color[3], struct wmOperator *op);

#endif /* __ED_PAINT_H__ */
