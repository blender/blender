/**
 * $Id:
 *
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
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_IPO_INTERN_H
#define ED_IPO_INTERN_H

struct bContext;
struct wmWindowManager;
struct bAnimContext;
struct SpaceIpo;
struct ARegion;

/* internal exports only */

/* ***************************************** */
/* ipo_draw.c */
void graph_draw_channel_names(struct bAnimContext *ac, struct SpaceIpo *sipo, struct ARegion *ar);
void graph_draw_curves(struct bAnimContext *ac, struct SpaceIpo *sipo, struct ARegion *ar);

/* ***************************************** */
/* ipo_header.c */
void graph_header_buttons(const bContext *C, struct ARegion *ar);

/* ***************************************** */
/* ipo_select.c */

void GRAPHEDIT_OT_keyframes_deselectall(struct wmOperatorType *ot);
void GRAPHEDIT_OT_keyframes_borderselect(struct wmOperatorType *ot);
void GRAPHEDIT_OT_keyframes_columnselect(struct wmOperatorType *ot);
void GRAPHEDIT_OT_keyframes_clickselect(struct wmOperatorType *ot);

/* defines for left-right select tool */
enum {
	GRAPHKEYS_LRSEL_TEST	= -1,
	GRAPHKEYS_LRSEL_NONE,
	GRAPHKEYS_LRSEL_LEFT,
	GRAPHKEYS_LRSEL_RIGHT,
} eGraphKeys_LeftRightSelect_Mode;

/* defines for column-select mode */
enum {
	GRAPHKEYS_COLUMNSEL_KEYS	= 0,
	GRAPHKEYS_COLUMNSEL_CFRA,
	GRAPHKEYS_COLUMNSEL_MARKERS_COLUMN,
	GRAPHKEYS_COLUMNSEL_MARKERS_BETWEEN,
} eGraphKeys_ColumnSelect_Mode;

/* ***************************************** */
/* ipo_edit.c */

/* ***************************************** */
/* ipo_ops.c */
void graphedit_keymap(struct wmWindowManager *wm);
void graphedit_operatortypes(void);


#endif /* ED_IPO_INTERN_H */

