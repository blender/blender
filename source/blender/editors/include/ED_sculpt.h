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

/* sculpt.c */
void ED_operatortypes_sculpt(void);
void sculpt_get_redraw_planes(float planes[4][4], struct ARegion *ar,
                              struct RegionView3D *rv3d, struct Object *ob);
void ED_sculpt_force_update(struct bContext *C);
float *ED_sculpt_get_last_stroke(struct Object *ob);
int ED_sculpt_minmax(struct bContext *C, float min[3], float max[3]);
void ED_sculpt_mask_layers_ensure(struct Object *ob,
                                  struct MultiresModifierData *mmd);

/* paint_ops.c */
void ED_operatortypes_paint(void);
void ED_keymap_paint(struct wmKeyConfig *keyconf);

/* paint_undo.c */
#define UNDO_PAINT_IMAGE    0
#define UNDO_PAINT_MESH     1

int ED_undo_paint_step(struct bContext *C, int type, int step, const char *name);
void ED_undo_paint_free(void);
int ED_undo_paint_valid(int type, const char *name);

#endif
