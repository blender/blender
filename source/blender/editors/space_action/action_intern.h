/**
 * $Id$
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
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 *
 * 
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */
#ifndef ED_ACTION_INTERN_H
#define ED_ACTION_INTERN_H

struct bContext;
struct bAnimContext;
struct SpaceAction;
struct ARegion;
struct wmWindowManager;
struct wmOperatorType;
struct ActKeysInc;
struct bAnimListElem;

/* internal exports only */

/* ***************************************** */
/* action_draw.c */
void draw_channel_names(struct bContext *C, struct bAnimContext *ac, struct SpaceAction *saction, struct ARegion *ar); 
void draw_channel_strips(struct bAnimContext *ac, struct SpaceAction *saction, struct ARegion *ar);

struct ActKeysInc *init_aki_data(struct bAnimContext *ac, struct bAnimListElem *ale);

/* ***************************************** */
/* action_header.c */
void action_header_buttons(const struct bContext *C, struct ARegion *ar);

/* ***************************************** */
/* action_select.c */

void ACTION_OT_select_all_toggle(struct wmOperatorType *ot);
void ACTION_OT_select_border(struct wmOperatorType *ot);
void ACTION_OT_select_column(struct wmOperatorType *ot);
void ACTION_OT_select_linked(struct wmOperatorType *ot);
void ACTION_OT_select_more(struct wmOperatorType *ot);
void ACTION_OT_select_less(struct wmOperatorType *ot);
void ACTION_OT_clickselect(struct wmOperatorType *ot);

/* defines for left-right select tool */
enum {
	ACTKEYS_LRSEL_TEST	= -1,
	ACTKEYS_LRSEL_NONE,
	ACTKEYS_LRSEL_LEFT,
	ACTKEYS_LRSEL_RIGHT,
} eActKeys_LeftRightSelect_Mode;

/* defines for column-select mode */
enum {
	ACTKEYS_COLUMNSEL_KEYS	= 0,
	ACTKEYS_COLUMNSEL_CFRA,
	ACTKEYS_COLUMNSEL_MARKERS_COLUMN,
	ACTKEYS_COLUMNSEL_MARKERS_BETWEEN,
} eActKeys_ColumnSelect_Mode;

/* ***************************************** */
/* action_edit.c */

void ACTION_OT_previewrange_set(struct wmOperatorType *ot);
void ACTION_OT_view_all(struct wmOperatorType *ot);

void ACTION_OT_copy(struct wmOperatorType *ot);
void ACTION_OT_paste(struct wmOperatorType *ot);

void ACTION_OT_keyframe_insert(struct wmOperatorType *ot);
void ACTION_OT_duplicate(struct wmOperatorType *ot);
void ACTION_OT_delete(struct wmOperatorType *ot);
void ACTION_OT_clean(struct wmOperatorType *ot);
void ACTION_OT_sample(struct wmOperatorType *ot);

void ACTION_OT_keyframe_type(struct wmOperatorType *ot);
void ACTION_OT_handle_type(struct wmOperatorType *ot);
void ACTION_OT_interpolation_type(struct wmOperatorType *ot);
void ACTION_OT_extrapolation_type(struct wmOperatorType *ot);

void ACTION_OT_frame_jump(struct wmOperatorType *ot);

void ACTION_OT_snap(struct wmOperatorType *ot);
void ACTION_OT_mirror(struct wmOperatorType *ot);

void ACTION_OT_new(struct wmOperatorType *ot);

/* defines for snap keyframes 
 * NOTE: keep in sync with eEditKeyframes_Snap (in ED_keyframes_edit.h)
 */
enum {
	ACTKEYS_SNAP_CFRA = 1,
	ACTKEYS_SNAP_NEAREST_FRAME,
	ACTKEYS_SNAP_NEAREST_SECOND,
	ACTKEYS_SNAP_NEAREST_MARKER,	
} eActKeys_Snap_Mode;

/* defines for mirror keyframes 
 * NOTE: keep in sync with eEditKeyframes_Mirror (in ED_keyframes_edit.h)
 */
enum {
	ACTKEYS_MIRROR_CFRA = 1,
	ACTKEYS_MIRROR_YAXIS,
	ACTKEYS_MIRROR_XAXIS,
	ACTKEYS_MIRROR_MARKER,	
} eActKeys_Mirror_Mode;
	
/* ***************************************** */
/* action_ops.c */
void action_operatortypes(void);
void action_keymap(struct wmKeyConfig *keyconf);

#endif /* ED_ACTION_INTERN_H */

