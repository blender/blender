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

/* internal exports only */

/* ***************************************** */
/* `graph_draw.cc` */

/**
 * Left hand part.
 */
void graph_draw_channel_names(bContext *C,
                              bAnimContext *ac,
                              ARegion *region,
                              const ListBase /* bAnimListElem */ &anim_data);

/**
 * This is called twice from `space_graph.cc`, #graph_main_region_draw()
 * Unselected then selected F-Curves are drawn so that they do not occlude each other.
 */
void graph_draw_curves(bAnimContext *ac, SpaceGraph *sipo, ARegion *region, short sel);
/**
 * Draw the 'ghost' F-Curves (i.e. snapshots of the curve)
 * \note unit mapping has already been applied to the values, so do not try and apply again.
 */
void graph_draw_ghost_curves(bAnimContext *ac, SpaceGraph *sipo, ARegion *region);

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
void deselect_graph_keys(bAnimContext *ac, bool test, short sel, bool do_channels);

void GRAPH_OT_select_all(wmOperatorType *ot);
void GRAPH_OT_select_box(wmOperatorType *ot);
void GRAPH_OT_select_lasso(wmOperatorType *ot);
void GRAPH_OT_select_circle(wmOperatorType *ot);
void GRAPH_OT_select_column(wmOperatorType *ot);
void GRAPH_OT_select_linked(wmOperatorType *ot);
void GRAPH_OT_select_more(wmOperatorType *ot);
void GRAPH_OT_select_less(wmOperatorType *ot);
void GRAPH_OT_select_leftright(wmOperatorType *ot);
void GRAPH_OT_select_key_handles(wmOperatorType *ot);
void GRAPH_OT_clickselect(wmOperatorType *ot);

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
void get_graph_keyframe_extents(bAnimContext *ac,
                                float *xmin,
                                float *xmax,
                                float *ymin,
                                float *ymax,
                                bool do_sel_only,
                                bool include_handles);

void GRAPH_OT_previewrange_set(wmOperatorType *ot);
void GRAPH_OT_view_all(wmOperatorType *ot);
void GRAPH_OT_view_selected(wmOperatorType *ot);
void GRAPH_OT_view_frame(wmOperatorType *ot);

void GRAPH_OT_click_insert(wmOperatorType *ot);
void GRAPH_OT_keyframe_insert(wmOperatorType *ot);

void GRAPH_OT_copy(wmOperatorType *ot);
void GRAPH_OT_paste(wmOperatorType *ot);

void GRAPH_OT_duplicate(wmOperatorType *ot);
void GRAPH_OT_delete(wmOperatorType *ot);
void GRAPH_OT_clean(wmOperatorType *ot);
void GRAPH_OT_blend_to_neighbor(wmOperatorType *ot);
void GRAPH_OT_breakdown(wmOperatorType *ot);
void GRAPH_OT_ease(wmOperatorType *ot);
void GRAPH_OT_blend_offset(wmOperatorType *ot);
void GRAPH_OT_blend_to_ease(wmOperatorType *ot);
void GRAPH_OT_match_slope(wmOperatorType *ot);
void GRAPH_OT_shear(wmOperatorType *ot);
void GRAPH_OT_scale_average(wmOperatorType *ot);
void GRAPH_OT_push_pull(wmOperatorType *ot);
void GRAPH_OT_time_offset(wmOperatorType *ot);
void GRAPH_OT_scale_from_neighbor(wmOperatorType *ot);
void GRAPH_OT_decimate(wmOperatorType *ot);
void GRAPH_OT_blend_to_default(wmOperatorType *ot);
void GRAPH_OT_butterworth_smooth(wmOperatorType *ot);
void GRAPH_OT_gaussian_smooth(wmOperatorType *ot);
void GRAPH_OT_bake_keys(wmOperatorType *ot);
void GRAPH_OT_keys_to_samples(wmOperatorType *ot);
void GRAPH_OT_samples_to_keys(wmOperatorType *ot);
void GRAPH_OT_sound_to_samples(wmOperatorType *ot);
void GRAPH_OT_smooth(wmOperatorType *ot);
void GRAPH_OT_euler_filter(wmOperatorType *ot);

void GRAPH_OT_handle_type(wmOperatorType *ot);
void GRAPH_OT_interpolation_type(wmOperatorType *ot);
void GRAPH_OT_extrapolation_type(wmOperatorType *ot);
void GRAPH_OT_easing_type(wmOperatorType *ot);

void GRAPH_OT_frame_jump(wmOperatorType *ot);
void GRAPH_OT_keyframe_jump(wmOperatorType *ot);
void GRAPH_OT_snap_cursor_value(wmOperatorType *ot);
void GRAPH_OT_snap(wmOperatorType *ot);
void GRAPH_OT_equalize_handles(wmOperatorType *ot);
void GRAPH_OT_mirror(wmOperatorType *ot);

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

void GRAPH_OT_fmodifier_add(wmOperatorType *ot);
void GRAPH_OT_fmodifier_copy(wmOperatorType *ot);
void GRAPH_OT_fmodifier_paste(wmOperatorType *ot);

/* ----------- */

void GRAPH_OT_driver_variables_copy(wmOperatorType *ot);
void GRAPH_OT_driver_variables_paste(wmOperatorType *ot);
void GRAPH_OT_driver_delete_invalid(wmOperatorType *ot);

/* ----------- */

void GRAPH_OT_ghost_curves_create(wmOperatorType *ot);
void GRAPH_OT_ghost_curves_clear(wmOperatorType *ot);

/* ***************************************** */
/* `graph_buttons.cc` */

void graph_buttons_register(ARegionType *art);

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
bAnimListElem *get_active_fcurve_channel(bAnimContext *ac);

/**
 * Check if there are any visible keyframes (for selection tools).
 */
bool graphop_visible_keyframes_poll(bContext *C);
/**
 * Check if there are any visible + editable keyframes (for editing tools).
 */
bool graphop_editable_keyframes_poll(bContext *C);
/**
 * Has active F-Curve that's editable.
 */
bool graphop_active_fcurve_poll(bContext *C);
/**
 * Has active F-Curve in the context that's editable.
 */
bool graphop_active_editable_fcurve_ctx_poll(bContext *C);
/**
 * Has selected F-Curve that's editable.
 */
bool graphop_selected_fcurve_poll(bContext *C);

/* ***************************************** */
/* `graph_ops.cc` */

void graphedit_keymap(wmKeyConfig *keyconf);
void graphedit_operatortypes();
