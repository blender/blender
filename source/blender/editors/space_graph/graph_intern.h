/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spgraph
 */

#pragma once

struct ARegion;
struct ARegionType;
struct SpaceGraph;
struct bAnimContext;
struct bAnimListElem;
struct bContext;

#ifdef __cplusplus
extern "C" {
#endif

/* internal exports only */

/* ***************************************** */
/* `graph_draw.cc` */

/**
 * Left hand part.
 */
void graph_draw_channel_names(struct bContext *C,
                              struct bAnimContext *ac,
                              struct ARegion *region,
                              const ListBase /* bAnimListElem */ &anim_data);

/**
 * This is called twice from `space_graph.cc`, #graph_main_region_draw()
 * Unselected then selected F-Curves are drawn so that they do not occlude each other.
 */
void graph_draw_curves(struct bAnimContext *ac,
                       struct SpaceGraph *sipo,
                       struct ARegion *region,
                       short sel);
/**
 * Draw the 'ghost' F-Curves (i.e. snapshots of the curve)
 * \note unit mapping has already been applied to the values, so do not try and apply again.
 */
void graph_draw_ghost_curves(struct bAnimContext *ac,
                             struct SpaceGraph *sipo,
                             struct ARegion *region);

/* ***************************************** */
/* `graph_select.cc` */

/**
 * Deselects keyframes in the Graph Editor
 * - This is called by the deselect all operator, as well as other ones!
 *
 * - test: check if select or deselect all
 * - sel: how to select keyframes
 *   0 = deselect
 *   1 = select
 *   2 = invert
 * - do_channels: whether to affect selection status of channels
 */
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
void GRAPH_OT_select_key_handles(struct wmOperatorType *ot);
void GRAPH_OT_clickselect(struct wmOperatorType *ot);

/* defines for left-right select tool */
enum eGraphKeys_LeftRightSelect_Mode {
  GRAPHKEYS_LRSEL_TEST = 0,
  GRAPHKEYS_LRSEL_LEFT,
  GRAPHKEYS_LRSEL_RIGHT,
};

/* Defines for key/handles selection. */
enum eGraphKey_SelectKeyHandles_Action {
  GRAPHKEYS_KEYHANDLESSEL_SELECT = 0,
  GRAPHKEYS_KEYHANDLESSEL_DESELECT,
  /* Leave the selection status as-is. */
  GRAPHKEYS_KEYHANDLESSEL_KEEP,
};

/* defines for column-select mode */
enum eGraphKeys_ColumnSelect_Mode {
  GRAPHKEYS_COLUMNSEL_KEYS = 0,
  GRAPHKEYS_COLUMNSEL_CFRA,
  GRAPHKEYS_COLUMNSEL_MARKERS_COLUMN,
  GRAPHKEYS_COLUMNSEL_MARKERS_BETWEEN,
};

/* ***************************************** */
/* `graph_edit.cc` */

/**
 * Get the min/max keyframes.
 * \note it should return total bound-box, filter for selection only can be argument.
 */
void get_graph_keyframe_extents(struct bAnimContext *ac,
                                float *xmin,
                                float *xmax,
                                float *ymin,
                                float *ymax,
                                bool do_sel_only,
                                bool include_handles);

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
void GRAPH_OT_blend_to_neighbor(struct wmOperatorType *ot);
void GRAPH_OT_breakdown(struct wmOperatorType *ot);
void GRAPH_OT_ease(struct wmOperatorType *ot);
void GRAPH_OT_blend_offset(struct wmOperatorType *ot);
void GRAPH_OT_blend_to_ease(struct wmOperatorType *ot);
void GRAPH_OT_match_slope(struct wmOperatorType *ot);
void GRAPH_OT_shear(struct wmOperatorType *ot);
void GRAPH_OT_scale_average(struct wmOperatorType *ot);
void GRAPH_OT_push_pull(struct wmOperatorType *ot);
void GRAPH_OT_time_offset(struct wmOperatorType *ot);
void GRAPH_OT_scale_from_neighbor(struct wmOperatorType *ot);
void GRAPH_OT_decimate(struct wmOperatorType *ot);
void GRAPH_OT_blend_to_default(struct wmOperatorType *ot);
void GRAPH_OT_butterworth_smooth(struct wmOperatorType *ot);
void GRAPH_OT_gaussian_smooth(struct wmOperatorType *ot);
void GRAPH_OT_bake_keys(struct wmOperatorType *ot);
void GRAPH_OT_keys_to_samples(struct wmOperatorType *ot);
void GRAPH_OT_samples_to_keys(struct wmOperatorType *ot);
void GRAPH_OT_sound_to_samples(struct wmOperatorType *ot);
void GRAPH_OT_smooth(struct wmOperatorType *ot);
void GRAPH_OT_euler_filter(struct wmOperatorType *ot);

void GRAPH_OT_handle_type(struct wmOperatorType *ot);
void GRAPH_OT_interpolation_type(struct wmOperatorType *ot);
void GRAPH_OT_extrapolation_type(struct wmOperatorType *ot);
void GRAPH_OT_easing_type(struct wmOperatorType *ot);

void GRAPH_OT_frame_jump(struct wmOperatorType *ot);
void GRAPH_OT_keyframe_jump(struct wmOperatorType *ot);
void GRAPH_OT_snap_cursor_value(struct wmOperatorType *ot);
void GRAPH_OT_snap(struct wmOperatorType *ot);
void GRAPH_OT_equalize_handles(struct wmOperatorType *ot);
void GRAPH_OT_mirror(struct wmOperatorType *ot);

/* defines for snap keyframes
 * NOTE: keep in sync with eEditKeyframes_Snap (in ED_keyframes_edit.hh)
 */
enum eGraphKeys_Snap_Mode {
  GRAPHKEYS_SNAP_CFRA = 1,
  GRAPHKEYS_SNAP_NEAREST_FRAME,
  GRAPHKEYS_SNAP_NEAREST_SECOND,
  GRAPHKEYS_SNAP_NEAREST_MARKER,
  GRAPHKEYS_SNAP_HORIZONTAL,
  GRAPHKEYS_SNAP_VALUE,
};

/* Defines for equalize keyframe handles.
 * NOTE: Keep in sync with eEditKeyframes_Equalize (in ED_keyframes_edit.hh).
 */
enum eGraphKeys_Equalize_Mode {
  GRAPHKEYS_EQUALIZE_LEFT = 1,
  GRAPHKEYS_EQUALIZE_RIGHT,
  GRAPHKEYS_EQUALIZE_BOTH,
};

/* defines for mirror keyframes
 * NOTE: keep in sync with eEditKeyframes_Mirror (in ED_keyframes_edit.hh)
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
/* `graph_buttons.cc` */

void graph_buttons_register(struct ARegionType *art);

/* ***************************************** */
/* `graph_utils.cc` */

/**
 * Find 'active' F-Curve.
 * It must be editable, since that's the purpose of these buttons (subject to change).
 * We return the 'wrapper' since it contains valuable context info (about hierarchy),
 * which will need to be freed when the caller is done with it.
 *
 * \note curve-visible flag isn't included,
 * otherwise selecting a curve via list to edit is too cumbersome.
 */
struct bAnimListElem *get_active_fcurve_channel(struct bAnimContext *ac);

/**
 * Check if there are any visible keyframes (for selection tools).
 */
bool graphop_visible_keyframes_poll(struct bContext *C);
/**
 * Check if there are any visible + editable keyframes (for editing tools).
 */
bool graphop_editable_keyframes_poll(struct bContext *C);
/**
 * Has active F-Curve that's editable.
 */
bool graphop_active_fcurve_poll(struct bContext *C);
/**
 * Has active F-Curve in the context that's editable.
 */
bool graphop_active_editable_fcurve_ctx_poll(struct bContext *C);
/**
 * Has selected F-Curve that's editable.
 */
bool graphop_selected_fcurve_poll(struct bContext *C);

/* ***************************************** */
/* `graph_ops.cc` */

void graphedit_keymap(struct wmKeyConfig *keyconf);
void graphedit_operatortypes(void);

#ifdef __cplusplus
}
#endif
