/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * \name Gizmo API
 * \brief API for external use of wmGizmo types.
 *
 * Only included in WM_api.hh
 */

#pragma once

struct ARegion;
struct GHashIterator;
struct IDProperty;
struct Main;
struct PropertyRNA;
struct ScrArea;
struct bToolRef;
struct wmGizmo;
struct wmGizmoGroup;
struct wmGizmoGroupType;
struct wmGizmoMap;
struct wmGizmoMapType;
struct wmGizmoMapType_Params;
struct wmGizmoProperty;
struct wmGizmoPropertyType;
struct wmGizmoType;
struct wmKeyConfig;
struct wmMsgSubscribeKey;
struct wmMsgSubscribeValue;
struct wmWindowManager;

#include "wm_gizmo_fn.hh"

/* -------------------------------------------------------------------- */
/* wmGizmo */

wmGizmo *WM_gizmo_new_ptr(const wmGizmoType *gzt, wmGizmoGroup *gzgroup, PointerRNA *properties);
/**
 * \param idname: Must be a valid gizmo type name,
 * if you need to check it exists use #WM_gizmo_new_ptr
 * because callers of this function don't NULL check the return value.
 */
wmGizmo *WM_gizmo_new(const char *idname, wmGizmoGroup *gzgroup, PointerRNA *properties);
/**
 * \warning this doesn't check #wmGizmoMap (highlight, selection etc).
 * Typical use is when freeing the windowing data,
 * where caller can manage clearing selection, highlight... etc.
 */
void WM_gizmo_free(wmGizmo *gz);
/**
 * Free \a gizmo and unlink from \a gizmolist.
 * \a gizmolist is allowed to be NULL.
 */
void WM_gizmo_unlink(ListBase *gizmolist, wmGizmoMap *gzmap, wmGizmo *gz, bContext *C);

/**
 * Remove from selection array without running callbacks.
 */
bool WM_gizmo_select_unlink(wmGizmoMap *gzmap, wmGizmo *gz);
bool WM_gizmo_select_set(wmGizmoMap *gzmap, wmGizmo *gz, bool select);
bool WM_gizmo_highlight_set(wmGizmoMap *gzmap, wmGizmo *gz);

/**
 * Special function to run from setup so gizmos start out interactive.
 *
 * We could do this when linking them,
 * but this complicates things since the window update code needs to run first.
 */
void WM_gizmo_modal_set_from_setup(
    wmGizmoMap *gzmap, bContext *C, wmGizmo *gz, int part_index, const wmEvent *event);

/**
 * Replaces the current gizmo modal.
 * The substitute gizmo start out interactive.
 * It is similar to #WM_gizmo_modal_set_from_setup but without operator initialization.
 */
void WM_gizmo_modal_set_while_modal(wmGizmoMap *gzmap,
                                    bContext *C,
                                    wmGizmo *gz,
                                    const wmEvent *event);

wmGizmoOpElem *WM_gizmo_operator_get(wmGizmo *gz, int part_index);
PointerRNA *WM_gizmo_operator_set(wmGizmo *gz,
                                  int part_index,
                                  wmOperatorType *ot,
                                  IDProperty *properties);
int WM_gizmo_operator_invoke(bContext *C, wmGizmo *gz, wmGizmoOpElem *gzop, const wmEvent *event);

/* Callbacks. */

void WM_gizmo_set_fn_custom_modal(wmGizmo *gz, wmGizmoFnModal fn);

void WM_gizmo_set_matrix_location(wmGizmo *gz, const float origin[3]);
/**
 * #wmGizmo.matrix utility, set the orientation by its Z axis.
 */
void WM_gizmo_set_matrix_rotation_from_z_axis(wmGizmo *gz, const float z_axis[3]);
/**
 * #wmGizmo.matrix utility, set the orientation by its Y/Z axis.
 */
void WM_gizmo_set_matrix_rotation_from_yz_axis(wmGizmo *gz,
                                               const float y_axis[3],
                                               const float z_axis[3]);

void WM_gizmo_set_matrix_offset_location(wmGizmo *gz, const float offset[3]);
/**
 * #wmGizmo.matrix_offset utility, set the orientation by its Z axis.
 */
void WM_gizmo_set_matrix_offset_rotation_from_z_axis(wmGizmo *gz, const float z_axis[3]);
/**
 * #wmGizmo.matrix_offset utility, set the orientation by its Y/Z axis.
 */
void WM_gizmo_set_matrix_offset_rotation_from_yz_axis(wmGizmo *gz,
                                                      const float y_axis[3],
                                                      const float z_axis[3]);

void WM_gizmo_set_flag(wmGizmo *gz, int flag, bool enable);
void WM_gizmo_set_scale(wmGizmo *gz, float scale);
void WM_gizmo_set_line_width(wmGizmo *gz, float line_width);

void WM_gizmo_get_color(const wmGizmo *gz, float color[4]);
void WM_gizmo_set_color(wmGizmo *gz, const float color[4]);
void WM_gizmo_get_color_highlight(const wmGizmo *gz, float color_hi[4]);
void WM_gizmo_set_color_highlight(wmGizmo *gz, const float color[4]);

/**
 * Leaving values NULL use values from #wmGizmo.
 */
struct WM_GizmoMatrixParams {
  const float (*matrix_space)[4];
  const float (*matrix_basis)[4];
  const float (*matrix_offset)[4];
  const float *scale_final;
};

void WM_gizmo_calc_matrix_final_params(const wmGizmo *gz,
                                       const WM_GizmoMatrixParams *params,
                                       float r_mat[4][4]);
void WM_gizmo_calc_matrix_final_no_offset(const wmGizmo *gz, float r_mat[4][4]);

void WM_gizmo_calc_matrix_final(const wmGizmo *gz, float r_mat[4][4]);

/* Properties. */

void WM_gizmo_properties_create_ptr(PointerRNA *ptr, wmGizmoType *gzt);
void WM_gizmo_properties_create(PointerRNA *ptr, const char *gtstring);
/**
 * Similar to #WM_gizmo_properties_create
 * except its uses ID properties used for key-maps and macros.
 */
void WM_gizmo_properties_alloc(PointerRNA **ptr, IDProperty **properties, const char *gtstring);
void WM_gizmo_properties_sanitize(PointerRNA *ptr, bool no_context);
/**
 * Set all props to their default.
 *
 * \param do_update: Only update un-initialized props.
 *
 * \note There's nothing specific to gizmos here.
 * This could be made a general function.
 */
bool WM_gizmo_properties_default(PointerRNA *ptr, bool do_update);
/**
 * Remove all props without #PROP_SKIP_SAVE.
 */
void WM_gizmo_properties_reset(wmGizmo *gz);
void WM_gizmo_properties_clear(PointerRNA *ptr);
void WM_gizmo_properties_free(PointerRNA *ptr);

/* `wm_gizmo_type.cc` */

const wmGizmoType *WM_gizmotype_find(const char *idname, bool quiet);
void WM_gizmotype_append(void (*gtfunc)(wmGizmoType *));
void WM_gizmotype_append_ptr(void (*gtfunc)(wmGizmoType *, void *), void *userdata);
bool WM_gizmotype_remove(bContext *C, Main *bmain, const char *idname);
void WM_gizmotype_remove_ptr(bContext *C, Main *bmain, wmGizmoType *gzt);
/**
 * Free but don't remove from #GHash.
 */
void WM_gizmotype_free_ptr(wmGizmoType *gzt);
/**
 * Caller must free.
 */
void WM_gizmotype_iter(GHashIterator *ghi);

/* `wm_gizmo_group_type.cc` */

wmGizmoGroupType *WM_gizmogrouptype_find(const char *idname, bool quiet);
wmGizmoGroupType *WM_gizmogrouptype_append(void (*wtfunc)(wmGizmoGroupType *));
wmGizmoGroupType *WM_gizmogrouptype_append_ptr(void (*wtfunc)(wmGizmoGroupType *, void *),
                                               void *userdata);
/**
 * Caller must free.
 */
void WM_gizmogrouptype_iter(GHashIterator *ghi);

/**
 * Append and insert into a gizmo type-map.
 * This is most common for C gizmos which are enabled by default.
 */
wmGizmoGroupTypeRef *WM_gizmogrouptype_append_and_link(wmGizmoMapType *gzmap_type,
                                                       void (*wtfunc)(wmGizmoGroupType *));

/* `wm_gizmo_map.cc` */

/* Dynamic Updates (for RNA runtime registration) */
void WM_gizmoconfig_update_tag_group_type_init(wmGizmoMapType *gzmap_type, wmGizmoGroupType *gzgt);
void WM_gizmoconfig_update_tag_group_type_remove(wmGizmoMapType *gzmap_type,
                                                 wmGizmoGroupType *gzgt);
/**
 * Run in case new types have been added (runs often, early exit where possible).
 * Follows #WM_keyconfig_update conventions.
 */
void WM_gizmoconfig_update(Main *bmain);

void WM_gizmoconfig_update_tag_group_remove(wmGizmoMap *gzmap);

/* wm_maniulator_target_props.c */

wmGizmoProperty *WM_gizmo_target_property_array(wmGizmo *gz);
wmGizmoProperty *WM_gizmo_target_property_at_index(wmGizmo *gz, int index);
wmGizmoProperty *WM_gizmo_target_property_find(wmGizmo *gz, const char *idname);

void WM_gizmo_target_property_def_rna_ptr(wmGizmo *gz,
                                          const wmGizmoPropertyType *gz_prop_type,
                                          PointerRNA *ptr,
                                          PropertyRNA *prop,
                                          int index);
void WM_gizmo_target_property_def_rna(
    wmGizmo *gz, const char *idname, PointerRNA *ptr, const char *propname, int index);

void WM_gizmo_target_property_def_func_ptr(wmGizmo *gz,
                                           const wmGizmoPropertyType *gz_prop_type,
                                           const wmGizmoPropertyFnParams *params);
void WM_gizmo_target_property_def_func(wmGizmo *gz,
                                       const char *idname,
                                       const wmGizmoPropertyFnParams *params);

void WM_gizmo_target_property_clear_rna_ptr(wmGizmo *gz, const wmGizmoPropertyType *gz_prop_type);
void WM_gizmo_target_property_clear_rna(wmGizmo *gz, const char *idname);

bool WM_gizmo_target_property_is_valid_any(wmGizmo *gz);
bool WM_gizmo_target_property_is_valid(const wmGizmoProperty *gz_prop);
float WM_gizmo_target_property_float_get(const wmGizmo *gz, wmGizmoProperty *gz_prop);
void WM_gizmo_target_property_float_set(bContext *C,
                                        const wmGizmo *gz,
                                        wmGizmoProperty *gz_prop,
                                        float value);

void WM_gizmo_target_property_float_get_array(const wmGizmo *gz,
                                              wmGizmoProperty *gz_prop,
                                              float *value);
void WM_gizmo_target_property_float_set_array(bContext *C,
                                              const wmGizmo *gz,
                                              wmGizmoProperty *gz_prop,
                                              const float *value);

bool WM_gizmo_target_property_float_range_get(const wmGizmo *gz,
                                              wmGizmoProperty *gz_prop,
                                              float range[2]);

int WM_gizmo_target_property_array_length(const wmGizmo *gz, wmGizmoProperty *gz_prop);

/* Definitions. */

const wmGizmoPropertyType *WM_gizmotype_target_property_find(const wmGizmoType *gzt,
                                                             const char *idname);
void WM_gizmotype_target_property_def(wmGizmoType *gzt,
                                      const char *idname,
                                      int data_type,
                                      int array_length);

/* Utilities. */

void WM_gizmo_do_msg_notify_tag_refresh(bContext *C,
                                        wmMsgSubscribeKey *msg_key,
                                        wmMsgSubscribeValue *msg_val);
/**
 * Runs on the "prepare draw" pass, drawing the region clears.
 */
void WM_gizmo_target_property_subscribe_all(wmGizmo *gz, wmMsgBus *mbus, ARegion *region);

/**
 * Auto-key function if auto-key is enabled.
 */
void WM_gizmo_target_property_anim_autokey(bContext *C,
                                           const wmGizmo *gz,
                                           wmGizmoProperty *gz_prop);

/* -------------------------------------------------------------------- */
/* wmGizmoGroup */

/* Callbacks for 'wmGizmoGroupType.setup_keymap' */
wmKeyMap *WM_gizmogroup_setup_keymap_generic(const wmGizmoGroupType *gzgt, wmKeyConfig *kc);
wmKeyMap *WM_gizmogroup_setup_keymap_generic_select(const wmGizmoGroupType *gzgt, wmKeyConfig *kc);
wmKeyMap *WM_gizmogroup_setup_keymap_generic_drag(const wmGizmoGroupType *gzgt, wmKeyConfig *kc);
wmKeyMap *WM_gizmogroup_setup_keymap_generic_maybe_drag(const wmGizmoGroupType *gzgt,
                                                        wmKeyConfig *kc);

/* Utility functions (not callbacks). */

wmKeyMap *WM_gizmo_keymap_generic_with_keyconfig(wmKeyConfig *kc);
wmKeyMap *WM_gizmo_keymap_generic(wmWindowManager *wm);

wmKeyMap *WM_gizmo_keymap_generic_select_with_keyconfig(wmKeyConfig *kc);
wmKeyMap *WM_gizmo_keymap_generic_select(wmWindowManager *wm);

wmKeyMap *WM_gizmo_keymap_generic_drag_with_keyconfig(wmKeyConfig *kc);
wmKeyMap *WM_gizmo_keymap_generic_drag(wmWindowManager *wm);

wmKeyMap *WM_gizmo_keymap_generic_click_drag_with_keyconfig(wmKeyConfig *kc);
wmKeyMap *WM_gizmo_keymap_generic_click_drag(wmWindowManager *wm);

/**
 * Drag or press depending on preference.
 */
wmKeyMap *WM_gizmo_keymap_generic_maybe_drag_with_keyconfig(wmKeyConfig *kc);
wmKeyMap *WM_gizmo_keymap_generic_maybe_drag(wmWindowManager *wm);

void WM_gizmogroup_ensure_init(const bContext *C, wmGizmoGroup *gzgroup);

/* Sort utilities for use with 'BLI_listbase_sort'. */

int WM_gizmo_cmp_temp_fl(const void *gz_a_ptr, const void *gz_b_ptr);
int WM_gizmo_cmp_temp_fl_reverse(const void *gz_a_ptr, const void *gz_b_ptr);

/* -------------------------------------------------------------------- */
/* wmGizmoMap */

/**
 * Creates a gizmo-map with all registered gizmos for that type
 */
wmGizmoMap *WM_gizmomap_new_from_type(const wmGizmoMapType_Params *gzmap_params);
/**
 * Re-create the gizmos (use when changing theme settings).
 */
void WM_gizmomap_reinit(wmGizmoMap *gzmap);
const ListBase *WM_gizmomap_group_list(wmGizmoMap *gzmap);
wmGizmoGroup *WM_gizmomap_group_find(wmGizmoMap *gzmap, const char *idname);
wmGizmoGroup *WM_gizmomap_group_find_ptr(wmGizmoMap *gzmap, const wmGizmoGroupType *gzgt);

eWM_GizmoFlagMapDrawStep WM_gizmomap_drawstep_from_gizmo_group(const wmGizmoGroup *gzgroup);
void WM_gizmomap_tag_refresh_drawstep(wmGizmoMap *gzmap, eWM_GizmoFlagMapDrawStep drawstep);
void WM_gizmomap_tag_refresh(wmGizmoMap *gzmap);

bool WM_gizmomap_tag_delay_refresh_for_tweak_check(wmGizmoMap *gzmap);

void WM_gizmomap_draw(wmGizmoMap *gzmap, const bContext *C, eWM_GizmoFlagMapDrawStep drawstep);
void WM_gizmomap_add_handlers(ARegion *region, wmGizmoMap *gzmap);
/**
 * Select/Deselect all selectable gizmos in \a gzmap.
 * \return if selection has changed.
 *
 * TODO: select all by type.
 */
bool WM_gizmomap_select_all(bContext *C, wmGizmoMap *gzmap, int action);
bool WM_gizmomap_cursor_set(const wmGizmoMap *gzmap, wmWindow *win);
void WM_gizmomap_message_subscribe(const bContext *C,
                                   wmGizmoMap *gzmap,
                                   ARegion *region,
                                   wmMsgBus *mbus);
bool WM_gizmomap_is_any_selected(const wmGizmoMap *gzmap);
wmGizmo *WM_gizmomap_get_modal(const wmGizmoMap *gzmap);

/**
 * \note We could use a callback to define bounds, for now just use matrix location.
 */
bool WM_gizmomap_minmax(
    const wmGizmoMap *gzmap, bool use_hidden, bool use_select, float r_min[3], float r_max[3]);

ARegion *WM_gizmomap_tooltip_init(
    bContext *C, ARegion *region, int *pass, double *pass_delay, bool *r_exit_on_event);

/* -------------------------------------------------------------------- */
/* wmGizmoMapType */

wmGizmoMapType *WM_gizmomaptype_find(const wmGizmoMapType_Params *gzmap_params);
wmGizmoMapType *WM_gizmomaptype_ensure(const wmGizmoMapType_Params *gzmap_params);

wmGizmoGroupTypeRef *WM_gizmomaptype_group_find(wmGizmoMapType *gzmap_type, const char *idname);
wmGizmoGroupTypeRef *WM_gizmomaptype_group_find_ptr(wmGizmoMapType *gzmap_type,
                                                    const wmGizmoGroupType *gzgt);
/**
 * Use this for registering gizmos on startup.
 * For runtime, use #WM_gizmomaptype_group_link_runtime.
 */
wmGizmoGroupTypeRef *WM_gizmomaptype_group_link(wmGizmoMapType *gzmap_type, const char *idname);
wmGizmoGroupTypeRef *WM_gizmomaptype_group_link_ptr(wmGizmoMapType *gzmap_type,
                                                    wmGizmoGroupType *gzgt);

void WM_gizmomaptype_group_init_runtime_keymap(const Main *bmain, wmGizmoGroupType *gzgt);
void WM_gizmomaptype_group_init_runtime(const Main *bmain,
                                        wmGizmoMapType *gzmap_type,
                                        wmGizmoGroupType *gzgt);
wmGizmoGroup *WM_gizmomaptype_group_init_runtime_with_region(wmGizmoMapType *gzmap_type,
                                                             wmGizmoGroupType *gzgt,
                                                             ARegion *region);
void WM_gizmomaptype_group_unlink(bContext *C,
                                  Main *bmain,
                                  wmGizmoMapType *gzmap_type,
                                  const wmGizmoGroupType *gzgt);

/**
 * Unlike #WM_gizmomaptype_group_unlink this doesn't maintain correct state, simply free.
 */
void WM_gizmomaptype_group_free(wmGizmoGroupTypeRef *gzgt);

/* -------------------------------------------------------------------- */
/* GizmoGroup */

/* Add/Ensure/Remove (High level API) */

void WM_gizmo_group_type_add_ptr_ex(wmGizmoGroupType *gzgt, wmGizmoMapType *gzmap_type);
void WM_gizmo_group_type_add_ptr(wmGizmoGroupType *gzgt);
void WM_gizmo_group_type_add(const char *idname);

bool WM_gizmo_group_type_ensure_ptr_ex(wmGizmoGroupType *gzgt, wmGizmoMapType *gzmap_type);
bool WM_gizmo_group_type_ensure_ptr(wmGizmoGroupType *gzgt);
bool WM_gizmo_group_type_ensure(const char *idname);

/**
 * Call #WM_gizmo_group_type_free_ptr after to remove & free.
 */
void WM_gizmo_group_type_remove_ptr_ex(Main *bmain,
                                       wmGizmoGroupType *gzgt,
                                       wmGizmoMapType *gzmap_type);
void WM_gizmo_group_type_remove_ptr(Main *bmain, wmGizmoGroupType *gzgt);
void WM_gizmo_group_type_remove(Main *bmain, const char *idname);

void WM_gizmo_group_type_unlink_delayed_ptr_ex(wmGizmoGroupType *gzgt, wmGizmoMapType *gzmap_type);
void WM_gizmo_group_type_unlink_delayed_ptr(wmGizmoGroupType *gzgt);
void WM_gizmo_group_type_unlink_delayed(const char *idname);

void WM_gizmo_group_unlink_delayed_ptr_from_space(wmGizmoGroupType *gzgt,
                                                  wmGizmoMapType *gzmap_type,
                                                  ScrArea *area);

void WM_gizmo_group_type_free_ptr(wmGizmoGroupType *gzgt);
bool WM_gizmo_group_type_free(const char *idname);

/**
 * Has the result of unlinking and linking (re-initializes gizmo's).
 */
void WM_gizmo_group_type_reinit_ptr_ex(Main *bmain,
                                       wmGizmoGroupType *gzgt,
                                       wmGizmoMapType *gzmap_type);
void WM_gizmo_group_type_reinit_ptr(Main *bmain, wmGizmoGroupType *gzgt);
void WM_gizmo_group_type_reinit(Main *bmain, const char *idname);

/* Utilities */

bool WM_gizmo_context_check_drawstep(const bContext *C, eWM_GizmoFlagMapDrawStep step);

void WM_gizmo_group_remove_by_tool(bContext *C,
                                   Main *bmain,
                                   const wmGizmoGroupType *gzgt,
                                   const bToolRef *tref);

void WM_gizmo_group_tag_remove(wmGizmoGroup *gzgroup);

/* Wrap Group Type Callbacks. */

bool WM_gizmo_group_type_poll(const bContext *C, const wmGizmoGroupType *gzgt);
void WM_gizmo_group_refresh(const bContext *C, wmGizmoGroup *gzgroup);
