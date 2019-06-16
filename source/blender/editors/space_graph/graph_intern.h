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
 *
 * The Original Code is Copyright (C) 2008 Blender Foundation.
 * All rights reserved.
 */

/** \file
 * \ingroup spgraph
 */

#ifndef __GRAPH_INTERN_H__
#define __GRAPH_INTERN_H__

struct ARegion;
struct ARegionType;
struct ScrArea;
struct SpaceGraph;
struct bAnimContext;
struct bAnimListElem;
struct bContext;

/* internal exports only */

/* ***************************************** */
/* graph_draw.c */
void graph_draw_channel_names(struct bContext *C, struct bAnimContext *ac, struct ARegion *ar);

void graph_draw_curves(struct bAnimContext *ac,
                       struct SpaceGraph *sipo,
                       struct ARegion *ar,
                       short sel);
void graph_draw_ghost_curves(struct bAnimContext *ac, struct SpaceGraph *sipo, struct ARegion *ar);

/* ***************************************** */
/* graph_select.c */

void deselect_graph_keys(struct bAnimContext *ac, bool test, short sel, bool do_channels);

void GRAPH_OT_select_all(struct wmOperatorType *ot);
void GRAPH_OT_select_box(struct wmOperatorType *ot);
void GRAPH_OT_select_lasso(struct wmOperatorType *ot);
void GRAPH_OT_select_circle(struct wmOperatorType *ot);
void GRAPH_OT_select_column(struct wmOperatorType *ot);
void GRAPH_OT_select_linked(struct wmOperatorType *ot);
void GRAPH_OT_select_more(struct wmOperatorType *ot);
void GRAPH_OT_select_less(struct wmOperatorType *ot);
void GRAPH_OT_select_leftright(struct wmOperatorType *ot);
void GRAPH_OT_clickselect(struct wmOperatorType *ot);

/* defines for left-right select tool */
enum eGraphKeys_LeftRightSelect_Mode {
  GRAPHKEYS_LRSEL_TEST = 0,
  GRAPHKEYS_LRSEL_LEFT,
  GRAPHKEYS_LRSEL_RIGHT,
};

/* defines for column-select mode */
enum eGraphKeys_ColumnSelect_Mode {
  GRAPHKEYS_COLUMNSEL_KEYS = 0,
  GRAPHKEYS_COLUMNSEL_CFRA,
  GRAPHKEYS_COLUMNSEL_MARKERS_COLUMN,
  GRAPHKEYS_COLUMNSEL_MARKERS_BETWEEN,
};

/* ***************************************** */
/* graph_edit.c */

void get_graph_keyframe_extents(struct bAnimContext *ac,
                                float *xmin,
                                float *xmax,
                                float *ymin,
                                float *ymax,
                                const bool do_selected,
                                const bool include_handles);

void GRAPH_OT_previewrange_set(struct wmOperatorType *ot);
void GRAPH_OT_view_all(struct wmOperatorType *ot);
void GRAPH_OT_view_selected(struct wmOperatorType *ot);
void GRAPH_OT_view_frame(struct wmOperatorType *ot);

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
void GRAPH_OT_euler_filter(struct wmOperatorType *ot);

void GRAPH_OT_handle_type(struct wmOperatorType *ot);
void GRAPH_OT_interpolation_type(struct wmOperatorType *ot);
void GRAPH_OT_extrapolation_type(struct wmOperatorType *ot);
void GRAPH_OT_easing_type(struct wmOperatorType *ot);

void GRAPH_OT_frame_jump(struct wmOperatorType *ot);
void GRAPH_OT_snap(struct wmOperatorType *ot);
void GRAPH_OT_mirror(struct wmOperatorType *ot);

/* defines for snap keyframes
 * NOTE: keep in sync with eEditKeyframes_Snap (in ED_keyframes_edit.h)
 */
enum eGraphKeys_Snap_Mode {
  GRAPHKEYS_SNAP_CFRA = 1,
  GRAPHKEYS_SNAP_NEAREST_FRAME,
  GRAPHKEYS_SNAP_NEAREST_SECOND,
  GRAPHKEYS_SNAP_NEAREST_MARKER,
  GRAPHKEYS_SNAP_HORIZONTAL,
  GRAPHKEYS_SNAP_VALUE,
};

/* defines for mirror keyframes
 * NOTE: keep in sync with eEditKeyframes_Mirror (in ED_keyframes_edit.h)
 */
enum eGraphKeys_Mirror_Mode {
  GRAPHKEYS_MIRROR_CFRA = 1,
  GRAPHKEYS_MIRROR_YAXIS,
  GRAPHKEYS_MIRROR_XAXIS,
  GRAPHKEYS_MIRROR_MARKER,
  GRAPHKEYS_MIRROR_VALUE,
};

/* ----------- */

void GRAPH_OT_fmodifier_add(struct wmOperatorType *ot);
void GRAPH_OT_fmodifier_copy(struct wmOperatorType *ot);
void GRAPH_OT_fmodifier_paste(struct wmOperatorType *ot);

/* ----------- */

void GRAPH_OT_driver_variables_copy(struct wmOperatorType *ot);
void GRAPH_OT_driver_variables_paste(struct wmOperatorType *ot);
void GRAPH_OT_driver_delete_invalid(struct wmOperatorType *ot);

/* ----------- */

void GRAPH_OT_ghost_curves_create(struct wmOperatorType *ot);
void GRAPH_OT_ghost_curves_clear(struct wmOperatorType *ot);

/* ***************************************** */
/* graph_buttons.c */

void graph_buttons_register(struct ARegionType *art);

/* ***************************************** */
/* graph_utils.c */

struct bAnimListElem *get_active_fcurve_channel(struct bAnimContext *ac);

bool graphop_visible_keyframes_poll(struct bContext *C);
bool graphop_editable_keyframes_poll(struct bContext *C);
bool graphop_active_fcurve_poll(struct bContext *C);
bool graphop_active_editable_fcurve_ctx_poll(struct bContext *C);
bool graphop_selected_fcurve_poll(struct bContext *C);

/* ***************************************** */
/* graph_ops.c */
void graphedit_keymap(struct wmKeyConfig *keyconf);
void graphedit_operatortypes(void);

#endif /* __GRAPH_INTERN_H__ */
