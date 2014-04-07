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
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): Nicholas Bishop
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file ED_sculpt.h
 *  \ingroup editors
 */

#ifndef __ED_SCULPT_H__
#define __ED_SCULPT_H__

struct ARegion;
struct bContext;
struct MultiresModifierData;
struct Object;
struct RegionView3D;
struct wmKeyConfig;
struct wmWindowManager;
struct ViewContext;
struct rcti;

/* sculpt.c */
void ED_operatortypes_sculpt(void);
void sculpt_get_redraw_planes(float planes[4][4], struct ARegion *ar,
                              struct RegionView3D *rv3d, struct Object *ob);
void ED_sculpt_get_average_stroke(struct Object *ob, float stroke[3]);
bool ED_sculpt_minmax(struct bContext *C, float min[3], float max[3]);
int ED_sculpt_mask_layers_ensure(struct Object *ob,
                                  struct MultiresModifierData *mmd);
int do_sculpt_mask_box_select(struct ViewContext *vc, struct rcti *rect, bool select, bool extend);

enum {
	ED_SCULPT_MASK_LAYER_CALC_VERT = (1 << 0),
	ED_SCULPT_MASK_LAYER_CALC_LOOP = (1 << 1)
};

/* paint_ops.c */
void ED_operatortypes_paint(void);
void ED_keymap_paint(struct wmKeyConfig *keyconf);

/* paint_undo.c */
#define UNDO_PAINT_IMAGE    0
#define UNDO_PAINT_MESH     1

typedef void (*UndoRestoreCb)(struct bContext *C, struct ListBase *lb);
typedef void (*UndoFreeCb)(struct ListBase *lb);

int ED_undo_paint_step(struct bContext *C, int type, int step, const char *name);
void ED_undo_paint_step_num(struct bContext *C, int type, int num);
const char *ED_undo_paint_get_name(int type, int nr, int *active);
void ED_undo_paint_free(void);
int ED_undo_paint_valid(int type, const char *name);
bool ED_undo_paint_empty(int type);
void ED_undo_paint_push_begin(int type, const char *name, UndoRestoreCb restore, UndoFreeCb free);
void ED_undo_paint_push_end(int type);

/* image painting specific undo */
void ED_image_undo_restore(struct bContext *C, struct ListBase *lb);
void ED_image_undo_free(struct ListBase *lb);
void ED_imapaint_clear_partial_redraw(void);
void ED_imapaint_dirty_region(struct Image *ima, struct ImBuf *ibuf, int x, int y, int w, int h);


#endif
