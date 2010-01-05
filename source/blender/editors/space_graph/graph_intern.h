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
#ifndef ED_GRAPH_INTERN_H
#define ED_GRAPH_INTERN_H

struct bContext;
struct wmWindowManager;
struct bAnimContext;
struct bAnimListElem;
struct FCurve;
struct FModifier;
struct SpaceIpo;
struct ScrArea;
struct ARegion;
struct ARegionType;
struct View2DGrid;

/* internal exports only */

/* ***************************************** */
/* space_graph.c */
struct ARegion *graph_has_buttons_region(struct ScrArea *sa);

/* ***************************************** */
/* graph_draw.c */
void graph_draw_channel_names(struct bContext *C, struct bAnimContext *ac, struct SpaceIpo *sipo, struct ARegion *ar);

void graph_draw_curves(struct bAnimContext *ac, struct SpaceIpo *sipo, struct ARegion *ar, struct View2DGrid *grid, short sel);
void graph_draw_ghost_curves(struct bAnimContext *ac, struct SpaceIpo *sipo, struct ARegion *ar, struct View2DGrid *grid);

/* ***************************************** */
/* graph_header.c */
void graph_header_buttons(const bContext *C, struct ARegion *ar);

/* ***************************************** */
/* graph_select.c */

void GRAPH_OT_select_all_toggle(struct wmOperatorType *ot);
void GRAPH_OT_select_border(struct wmOperatorType *ot);
void GRAPH_OT_select_column(struct wmOperatorType *ot);
void GRAPH_OT_clickselect(struct wmOperatorType *ot);

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
/* graph_edit.c */

void get_graph_keyframe_extents (struct bAnimContext *ac, float *xmin, float *xmax, float *ymin, float *ymax);

void GRAPH_OT_previewrange_set(struct wmOperatorType *ot);
void GRAPH_OT_view_all(struct wmOperatorType *ot);

void GRAPH_OT_click_insert(struct wmOperatorType *ot);
void GRAPH_OT_keyframe_insert(struct wmOperatorType *ot);

void GRAPH_OT_copy(struct wmOperatorType *ot);
void GRAPH_OT_paste(struct wmOperatorType *ot);

void GRAPH_OT_duplicate(struct wmOperatorType *ot);
void GRAPH_OT_delete(struct wmOperatorType *ot);
void GRAPH_OT_clean(struct wmOperatorType *ot);
void GRAPH_OT_sample(struct wmOperatorType *ot);
void GRAPH_OT_bake(struct wmOperatorType *ot);
void GRAPH_OT_sound_bake(struct wmOperatorType *ot);
void GRAPH_OT_smooth(struct wmOperatorType *ot);

void GRAPH_OT_handle_type(struct wmOperatorType *ot);
void GRAPH_OT_interpolation_type(struct wmOperatorType *ot);
void GRAPH_OT_extrapolation_type(struct wmOperatorType *ot);

void GRAPH_OT_frame_jump(struct wmOperatorType *ot);
void GRAPH_OT_snap(struct wmOperatorType *ot);
void GRAPH_OT_mirror(struct wmOperatorType *ot);

/* defines for snap keyframes 
 * NOTE: keep in sync with eEditKeyframes_Snap (in ED_keyframes_edit.h)
 */
enum {
	GRAPHKEYS_SNAP_CFRA = 1,
	GRAPHKEYS_SNAP_NEAREST_FRAME,
	GRAPHKEYS_SNAP_NEAREST_SECOND,
	GRAPHKEYS_SNAP_NEAREST_MARKER,	
	GRAPHKEYS_SNAP_HORIZONTAL,
	GRAPHKEYS_SNAP_VALUE,
} eGraphKeys_Snap_Mode;

/* defines for mirror keyframes 
 * NOTE: keep in sync with eEditKeyframes_Mirror (in ED_keyframes_edit.h)
 */
enum {
	GRAPHKEYS_MIRROR_CFRA = 1,
	GRAPHKEYS_MIRROR_YAXIS,
	GRAPHKEYS_MIRROR_XAXIS,
	GRAPHKEYS_MIRROR_MARKER,
	GRAPHKEYS_MIRROR_VALUE,
} eGraphKeys_Mirror_Mode;

/* ----------- */

void GRAPH_OT_fmodifier_add(struct wmOperatorType *ot);

/* ----------- */

void GRAPH_OT_ghost_curves_create(struct wmOperatorType *ot);
void GRAPH_OT_ghost_curves_clear(struct wmOperatorType *ot);

/* ***************************************** */
/* graph_buttons.c */
void GRAPH_OT_properties(struct wmOperatorType *ot);
void graph_buttons_register(struct ARegionType *art);

/* ***************************************** */
/* graph_utils.c */

struct bAnimListElem *get_active_fcurve_channel(struct bAnimContext *ac);

short fcurve_needs_draw_fmodifier_controls(struct FCurve *fcu, struct FModifier *fcm);

int graphop_visible_keyframes_poll(struct bContext *C);
int graphop_editable_keyframes_poll(struct bContext *C);
int graphop_active_fcurve_poll(struct bContext *C);
int graphop_selected_fcurve_poll(struct bContext *C);

/* ***************************************** */
/* graph_ops.c */
void graphedit_keymap(struct wmKeyConfig *keyconf);
void graphedit_operatortypes(void);


#endif /* ED_GRAPH_INTERN_H */

