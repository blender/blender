/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2016 Blender Foundation */

/** \file
 * \ingroup wm
 *
 * \name Gizmo API
 * \brief API for external use of wmGizmo types.
 *
 * Only included in WM_api.h
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

#include "wm_gizmo_fn.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/* wmGizmo */

struct wmGizmo *WM_gizmo_new_ptr(const struct wmGizmoType *gzt,
                                 struct wmGizmoGroup *gzgroup,
                                 struct PointerRNA *properties);
/**
 * \param idname: Must be a valid gizmo type name,
 * if you need to check it exists use #WM_gizmo_new_ptr
 * because callers of this function don't NULL check the return value.
 */
struct wmGizmo *WM_gizmo_new(const char *idname,
                             struct wmGizmoGroup *gzgroup,
                             struct PointerRNA *properties);
/**
 * \warning this doesn't check #wmGizmoMap (highlight, selection etc).
 * Typical use is when freeing the windowing data,
 * where caller can manage clearing selection, highlight... etc.
 */
void WM_gizmo_free(struct wmGizmo *gz);
/**
 * Free \a gizmo and unlink from \a gizmolist.
 * \a gizmolist is allowed to be NULL.
 */
void WM_gizmo_unlink(ListBase *gizmolist,
                     struct wmGizmoMap *gzmap,
                     struct wmGizmo *gz,
                     struct bContext *C);

/**
 * Remove from selection array without running callbacks.
 */
bool WM_gizmo_select_unlink(struct wmGizmoMap *gzmap, struct wmGizmo *gz);
bool WM_gizmo_select_set(struct wmGizmoMap *gzmap, struct wmGizmo *gz, bool select);
bool WM_gizmo_highlight_set(struct wmGizmoMap *gzmap, struct wmGizmo *gz);

/**
 * Special function to run from setup so gizmos start out interactive.
 *
 * We could do this when linking them,
 * but this complicates things since the window update code needs to run first.
 */
void WM_gizmo_modal_set_from_setup(struct wmGizmoMap *gzmap,
                                   struct bContext *C,
                                   struct wmGizmo *gz,
                                   int part_index,
                                   const struct wmEvent *event);

/**
 * Replaces the current gizmo modal.
 * The substitute gizmo start out interactive.
 * It is similar to #WM_gizmo_modal_set_from_setup but without operator initialization.
 */
void WM_gizmo_modal_set_while_modal(struct wmGizmoMap *gzmap,
                                    struct bContext *C,
                                    struct wmGizmo *gz,
                                    const struct wmEvent *event);

struct wmGizmoOpElem *WM_gizmo_operator_get(struct wmGizmo *gz, int part_index);
struct PointerRNA *WM_gizmo_operator_set(struct wmGizmo *gz,
                                         int part_index,
                                         struct wmOperatorType *ot,
                                         struct IDProperty *properties);
int WM_gizmo_operator_invoke(struct bContext *C,
                             struct wmGizmo *gz,
                             struct wmGizmoOpElem *gzop,
                             const struct wmEvent *event);

/* Callbacks. */

void WM_gizmo_set_fn_custom_modal(struct wmGizmo *gz, wmGizmoFnModal fn);

void WM_gizmo_set_matrix_location(struct wmGizmo *gz, const float origin[3]);
/**
 * #wmGizmo.matrix utility, set the orientation by it's Z axis.
 */
void WM_gizmo_set_matrix_rotation_from_z_axis(struct wmGizmo *gz, const float z_axis[3]);
/**
 * #wmGizmo.matrix utility, set the orientation by it's Y/Z axis.
 */
void WM_gizmo_set_matrix_rotation_from_yz_axis(struct wmGizmo *gz,
                                               const float y_axis[3],
                                               const float z_axis[3]);

void WM_gizmo_set_matrix_offset_location(struct wmGizmo *gz, const float offset[3]);
/**
 * #wmGizmo.matrix_offset utility, set the orientation by it's Z axis.
 */
void WM_gizmo_set_matrix_offset_rotation_from_z_axis(struct wmGizmo *gz, const float z_axis[3]);
/**
 * #wmGizmo.matrix_offset utility, set the orientation by it's Y/Z axis.
 */
void WM_gizmo_set_matrix_offset_rotation_from_yz_axis(struct wmGizmo *gz,
                                                      const float y_axis[3],
                                                      const float z_axis[3]);

void WM_gizmo_set_flag(struct wmGizmo *gz, int flag, bool enable);
void WM_gizmo_set_scale(struct wmGizmo *gz, float scale);
void WM_gizmo_set_line_width(struct wmGizmo *gz, float line_width);

void WM_gizmo_get_color(const struct wmGizmo *gz, float color[4]);
void WM_gizmo_set_color(struct wmGizmo *gz, const float color[4]);
void WM_gizmo_get_color_highlight(const struct wmGizmo *gz, float color_hi[4]);
void WM_gizmo_set_color_highlight(struct wmGizmo *gz, const float color[4]);

/**
 * Leaving values NULL use values from #wmGizmo.
 */
struct WM_GizmoMatrixParams {
  const float (*matrix_space)[4];
  const float (*matrix_basis)[4];
  const float (*matrix_offset)[4];
  const float *scale_final;
};

void WM_gizmo_calc_matrix_final_params(const struct wmGizmo *gz,
                                       const struct WM_GizmoMatrixParams *params,
                                       float r_mat[4][4]);
void WM_gizmo_calc_matrix_final_no_offset(const struct wmGizmo *gz, float r_mat[4][4]);

void WM_gizmo_calc_matrix_final(const struct wmGizmo *gz, float r_mat[4][4]);

/* Properties. */

void WM_gizmo_properties_create_ptr(struct PointerRNA *ptr, struct wmGizmoType *gzt);
void WM_gizmo_properties_create(struct PointerRNA *ptr, const char *gtstring);
/**
 * Similar to #WM_gizmo_properties_create
 * except its uses ID properties used for key-maps and macros.
 */
void WM_gizmo_properties_alloc(struct PointerRNA **ptr,
                               struct IDProperty **properties,
                               const char *gtstring);
void WM_gizmo_properties_sanitize(struct PointerRNA *ptr, bool no_context);
/**
 * Set all props to their default.
 *
 * \param do_update: Only update un-initialized props.
 *
 * \note There's nothing specific to gizmos here.
 * This could be made a general function.
 */
bool WM_gizmo_properties_default(struct PointerRNA *ptr, bool do_update);
/**
 * Remove all props without #PROP_SKIP_SAVE.
 */
void WM_gizmo_properties_reset(struct wmGizmo *gz);
void WM_gizmo_properties_clear(struct PointerRNA *ptr);
void WM_gizmo_properties_free(struct PointerRNA *ptr);

/* wm_gizmo_type.c */

const struct wmGizmoType *WM_gizmotype_find(const char *idname, bool quiet);
void WM_gizmotype_append(void (*gtfunc)(struct wmGizmoType *));
void WM_gizmotype_append_ptr(void (*gtfunc)(struct wmGizmoType *, void *), void *userdata);
bool WM_gizmotype_remove(struct bContext *C, struct Main *bmain, const char *idname);
void WM_gizmotype_remove_ptr(struct bContext *C, struct Main *bmain, struct wmGizmoType *gzt);
/**
 * Free but don't remove from #GHash.
 */
void WM_gizmotype_free_ptr(struct wmGizmoType *gzt);
/**
 * Caller must free.
 */
void WM_gizmotype_iter(struct GHashIterator *ghi);

/* wm_gizmo_group_type.c */

struct wmGizmoGroupType *WM_gizmogrouptype_find(const char *idname, bool quiet);
struct wmGizmoGroupType *WM_gizmogrouptype_append(void (*wtfunc)(struct wmGizmoGroupType *));
struct wmGizmoGroupType *WM_gizmogrouptype_append_ptr(void (*wtfunc)(struct wmGizmoGroupType *,
                                                                     void *),
                                                      void *userdata);
/**
 * Caller must free.
 */
void WM_gizmogrouptype_iter(struct GHashIterator *ghi);

/**
 * Append and insert into a gizmo typemap.
 * This is most common for C gizmos which are enabled by default.
 */
struct wmGizmoGroupTypeRef *WM_gizmogrouptype_append_and_link(
    struct wmGizmoMapType *gzmap_type, void (*wtfunc)(struct wmGizmoGroupType *));

/* wm_gizmo_map.c */

/* Dynamic Updates (for RNA runtime registration) */
void WM_gizmoconfig_update_tag_group_type_init(struct wmGizmoMapType *gzmap_type,
                                               struct wmGizmoGroupType *gzgt);
void WM_gizmoconfig_update_tag_group_type_remove(struct wmGizmoMapType *gzmap_type,
                                                 struct wmGizmoGroupType *gzgt);
/**
 * Run in case new types have been added (runs often, early exit where possible).
 * Follows #WM_keyconfig_update conventions.
 */
void WM_gizmoconfig_update(struct Main *bmain);

void WM_gizmoconfig_update_tag_group_remove(struct wmGizmoMap *gzmap);

/* wm_maniulator_target_props.c */

struct wmGizmoProperty *WM_gizmo_target_property_array(struct wmGizmo *gz);
struct wmGizmoProperty *WM_gizmo_target_property_at_index(struct wmGizmo *gz, int index);
struct wmGizmoProperty *WM_gizmo_target_property_find(struct wmGizmo *gz, const char *idname);

void WM_gizmo_target_property_def_rna_ptr(struct wmGizmo *gz,
                                          const struct wmGizmoPropertyType *gz_prop_type,
                                          struct PointerRNA *ptr,
                                          struct PropertyRNA *prop,
                                          int index);
void WM_gizmo_target_property_def_rna(struct wmGizmo *gz,
                                      const char *idname,
                                      struct PointerRNA *ptr,
                                      const char *propname,
                                      int index);

void WM_gizmo_target_property_def_func_ptr(struct wmGizmo *gz,
                                           const struct wmGizmoPropertyType *gz_prop_type,
                                           const struct wmGizmoPropertyFnParams *params);
void WM_gizmo_target_property_def_func(struct wmGizmo *gz,
                                       const char *idname,
                                       const struct wmGizmoPropertyFnParams *params);

void WM_gizmo_target_property_clear_rna_ptr(struct wmGizmo *gz,
                                            const struct wmGizmoPropertyType *gz_prop_type);
void WM_gizmo_target_property_clear_rna(struct wmGizmo *gz, const char *idname);

bool WM_gizmo_target_property_is_valid_any(struct wmGizmo *gz);
bool WM_gizmo_target_property_is_valid(const struct wmGizmoProperty *gz_prop);
float WM_gizmo_target_property_float_get(const struct wmGizmo *gz,
                                         struct wmGizmoProperty *gz_prop);
void WM_gizmo_target_property_float_set(struct bContext *C,
                                        const struct wmGizmo *gz,
                                        struct wmGizmoProperty *gz_prop,
                                        float value);

void WM_gizmo_target_property_float_get_array(const struct wmGizmo *gz,
                                              struct wmGizmoProperty *gz_prop,
                                              float *value);
void WM_gizmo_target_property_float_set_array(struct bContext *C,
                                              const struct wmGizmo *gz,
                                              struct wmGizmoProperty *gz_prop,
                                              const float *value);

bool WM_gizmo_target_property_float_range_get(const struct wmGizmo *gz,
                                              struct wmGizmoProperty *gz_prop,
                                              float range[2]);

int WM_gizmo_target_property_array_length(const struct wmGizmo *gz,
                                          struct wmGizmoProperty *gz_prop);

/* Definitions. */

const struct wmGizmoPropertyType *WM_gizmotype_target_property_find(const struct wmGizmoType *gzt,
                                                                    const char *idname);
void WM_gizmotype_target_property_def(struct wmGizmoType *gzt,
                                      const char *idname,
                                      int data_type,
                                      int array_length);

/* Utilities. */

void WM_gizmo_do_msg_notify_tag_refresh(struct bContext *C,
                                        struct wmMsgSubscribeKey *msg_key,
                                        struct wmMsgSubscribeValue *msg_val);
/**
 * Runs on the "prepare draw" pass, drawing the region clears.
 */
void WM_gizmo_target_property_subscribe_all(struct wmGizmo *gz,
                                            struct wmMsgBus *mbus,
                                            struct ARegion *region);

/**
 * Auto-key function if auto-key is enabled.
 */
void WM_gizmo_target_property_anim_autokey(struct bContext *C,
                                           const struct wmGizmo *gz,
                                           struct wmGizmoProperty *gz_prop);

/* -------------------------------------------------------------------- */
/* wmGizmoGroup */

/* Callbacks for 'wmGizmoGroupType.setup_keymap' */
struct wmKeyMap *WM_gizmogroup_setup_keymap_generic(const struct wmGizmoGroupType *gzgt,
                                                    struct wmKeyConfig *kc);
struct wmKeyMap *WM_gizmogroup_setup_keymap_generic_select(const struct wmGizmoGroupType *gzgt,
                                                           struct wmKeyConfig *kc);
struct wmKeyMap *WM_gizmogroup_setup_keymap_generic_drag(const struct wmGizmoGroupType *gzgt,
                                                         struct wmKeyConfig *kc);
struct wmKeyMap *WM_gizmogroup_setup_keymap_generic_maybe_drag(const struct wmGizmoGroupType *gzgt,
                                                               struct wmKeyConfig *kc);

/* Utility functions (not callbacks). */

struct wmKeyMap *WM_gizmo_keymap_generic_with_keyconfig(struct wmKeyConfig *kc);
struct wmKeyMap *WM_gizmo_keymap_generic(struct wmWindowManager *wm);

struct wmKeyMap *WM_gizmo_keymap_generic_select_with_keyconfig(struct wmKeyConfig *kc);
struct wmKeyMap *WM_gizmo_keymap_generic_select(struct wmWindowManager *wm);

struct wmKeyMap *WM_gizmo_keymap_generic_drag_with_keyconfig(struct wmKeyConfig *kc);
struct wmKeyMap *WM_gizmo_keymap_generic_drag(struct wmWindowManager *wm);

struct wmKeyMap *WM_gizmo_keymap_generic_click_drag_with_keyconfig(struct wmKeyConfig *kc);
struct wmKeyMap *WM_gizmo_keymap_generic_click_drag(struct wmWindowManager *wm);

/**
 * Drag or press depending on preference.
 */
struct wmKeyMap *WM_gizmo_keymap_generic_maybe_drag_with_keyconfig(struct wmKeyConfig *kc);
struct wmKeyMap *WM_gizmo_keymap_generic_maybe_drag(struct wmWindowManager *wm);

void WM_gizmogroup_ensure_init(const struct bContext *C, struct wmGizmoGroup *gzgroup);

/* Sort utilities for use with 'BLI_listbase_sort'. */

int WM_gizmo_cmp_temp_fl(const void *gz_a_ptr, const void *gz_b_ptr);
int WM_gizmo_cmp_temp_fl_reverse(const void *gz_a_ptr, const void *gz_b_ptr);

/* -------------------------------------------------------------------- */
/* wmGizmoMap */

/**
 * Creates a gizmo-map with all registered gizmos for that type
 */
struct wmGizmoMap *WM_gizmomap_new_from_type(const struct wmGizmoMapType_Params *gzmap_params);
/**
 * Re-create the gizmos (use when changing theme settings).
 */
void WM_gizmomap_reinit(struct wmGizmoMap *gzmap);
const struct ListBase *WM_gizmomap_group_list(struct wmGizmoMap *gzmap);
struct wmGizmoGroup *WM_gizmomap_group_find(struct wmGizmoMap *gzmap, const char *idname);
struct wmGizmoGroup *WM_gizmomap_group_find_ptr(struct wmGizmoMap *gzmap,
                                                const struct wmGizmoGroupType *gzgt);

eWM_GizmoFlagMapDrawStep WM_gizmomap_drawstep_from_gizmo_group(const struct wmGizmoGroup *gzgroup);
void WM_gizmomap_tag_refresh_drawstep(struct wmGizmoMap *gzmap, eWM_GizmoFlagMapDrawStep drawstep);
void WM_gizmomap_tag_refresh(struct wmGizmoMap *gzmap);

bool WM_gizmomap_tag_delay_refresh_for_tweak_check(struct wmGizmoMap *gzmap);

void WM_gizmomap_draw(struct wmGizmoMap *gzmap,
                      const struct bContext *C,
                      eWM_GizmoFlagMapDrawStep drawstep);
void WM_gizmomap_add_handlers(struct ARegion *region, struct wmGizmoMap *gzmap);
/**
 * Select/Deselect all selectable gizmos in \a gzmap.
 * \return if selection has changed.
 *
 * TODO: select all by type.
 */
bool WM_gizmomap_select_all(struct bContext *C, struct wmGizmoMap *gzmap, int action);
bool WM_gizmomap_cursor_set(const struct wmGizmoMap *gzmap, struct wmWindow *win);
void WM_gizmomap_message_subscribe(const struct bContext *C,
                                   struct wmGizmoMap *gzmap,
                                   struct ARegion *region,
                                   struct wmMsgBus *mbus);
bool WM_gizmomap_is_any_selected(const struct wmGizmoMap *gzmap);
struct wmGizmo *WM_gizmomap_get_modal(const struct wmGizmoMap *gzmap);

/**
 * \note We could use a callback to define bounds, for now just use matrix location.
 */
bool WM_gizmomap_minmax(const struct wmGizmoMap *gzmap,
                        bool use_hidden,
                        bool use_select,
                        float r_min[3],
                        float r_max[3]);

struct ARegion *WM_gizmomap_tooltip_init(struct bContext *C,
                                         struct ARegion *region,
                                         int *pass,
                                         double *pass_delay,
                                         bool *r_exit_on_event);

/* -------------------------------------------------------------------- */
/* wmGizmoMapType */

struct wmGizmoMapType *WM_gizmomaptype_find(const struct wmGizmoMapType_Params *gzmap_params);
struct wmGizmoMapType *WM_gizmomaptype_ensure(const struct wmGizmoMapType_Params *gzmap_params);

struct wmGizmoGroupTypeRef *WM_gizmomaptype_group_find(struct wmGizmoMapType *gzmap_type,
                                                       const char *idname);
struct wmGizmoGroupTypeRef *WM_gizmomaptype_group_find_ptr(struct wmGizmoMapType *gzmap_type,
                                                           const struct wmGizmoGroupType *gzgt);
/**
 * Use this for registering gizmos on startup.
 * For runtime, use #WM_gizmomaptype_group_link_runtime.
 */
struct wmGizmoGroupTypeRef *WM_gizmomaptype_group_link(struct wmGizmoMapType *gzmap_type,
                                                       const char *idname);
struct wmGizmoGroupTypeRef *WM_gizmomaptype_group_link_ptr(struct wmGizmoMapType *gzmap_type,
                                                           struct wmGizmoGroupType *gzgt);

void WM_gizmomaptype_group_init_runtime_keymap(const struct Main *bmain,
                                               struct wmGizmoGroupType *gzgt);
void WM_gizmomaptype_group_init_runtime(const struct Main *bmain,
                                        struct wmGizmoMapType *gzmap_type,
                                        struct wmGizmoGroupType *gzgt);
wmGizmoGroup *WM_gizmomaptype_group_init_runtime_with_region(struct wmGizmoMapType *gzmap_type,
                                                             struct wmGizmoGroupType *gzgt,
                                                             struct ARegion *region);
void WM_gizmomaptype_group_unlink(struct bContext *C,
                                  struct Main *bmain,
                                  struct wmGizmoMapType *gzmap_type,
                                  const struct wmGizmoGroupType *gzgt);

/**
 * Unlike #WM_gizmomaptype_group_unlink this doesn't maintain correct state, simply free.
 */
void WM_gizmomaptype_group_free(struct wmGizmoGroupTypeRef *gzgt);

/* -------------------------------------------------------------------- */
/* GizmoGroup */

/* Add/Ensure/Remove (High level API) */

void WM_gizmo_group_type_add_ptr_ex(struct wmGizmoGroupType *gzgt,
                                    struct wmGizmoMapType *gzmap_type);
void WM_gizmo_group_type_add_ptr(struct wmGizmoGroupType *gzgt);
void WM_gizmo_group_type_add(const char *idname);

bool WM_gizmo_group_type_ensure_ptr_ex(struct wmGizmoGroupType *gzgt,
                                       struct wmGizmoMapType *gzmap_type);
bool WM_gizmo_group_type_ensure_ptr(struct wmGizmoGroupType *gzgt);
bool WM_gizmo_group_type_ensure(const char *idname);

/**
 * Call #WM_gizmo_group_type_free_ptr after to remove & free.
 */
void WM_gizmo_group_type_remove_ptr_ex(struct Main *bmain,
                                       struct wmGizmoGroupType *gzgt,
                                       struct wmGizmoMapType *gzmap_type);
void WM_gizmo_group_type_remove_ptr(struct Main *bmain, struct wmGizmoGroupType *gzgt);
void WM_gizmo_group_type_remove(struct Main *bmain, const char *idname);

void WM_gizmo_group_type_unlink_delayed_ptr_ex(struct wmGizmoGroupType *gzgt,
                                               struct wmGizmoMapType *gzmap_type);
void WM_gizmo_group_type_unlink_delayed_ptr(struct wmGizmoGroupType *gzgt);
void WM_gizmo_group_type_unlink_delayed(const char *idname);

void WM_gizmo_group_unlink_delayed_ptr_from_space(struct wmGizmoGroupType *gzgt,
                                                  struct wmGizmoMapType *gzmap_type,
                                                  struct ScrArea *area);

void WM_gizmo_group_type_free_ptr(wmGizmoGroupType *gzgt);
bool WM_gizmo_group_type_free(const char *idname);

/**
 * Has the result of unlinking and linking (re-initializes gizmo's).
 */
void WM_gizmo_group_type_reinit_ptr_ex(struct Main *bmain,
                                       struct wmGizmoGroupType *gzgt,
                                       struct wmGizmoMapType *gzmap_type);
void WM_gizmo_group_type_reinit_ptr(struct Main *bmain, struct wmGizmoGroupType *gzgt);
void WM_gizmo_group_type_reinit(struct Main *bmain, const char *idname);

/* Utilities */

bool WM_gizmo_context_check_drawstep(const struct bContext *C, eWM_GizmoFlagMapDrawStep step);

void WM_gizmo_group_remove_by_tool(struct bContext *C,
                                   struct Main *bmain,
                                   const struct wmGizmoGroupType *gzgt,
                                   const struct bToolRef *tref);

void WM_gizmo_group_tag_remove(struct wmGizmoGroup *gzgroup);

/* Wrap Group Type Callbacks. */

bool WM_gizmo_group_type_poll(const struct bContext *C, const struct wmGizmoGroupType *gzgt);
void WM_gizmo_group_refresh(const struct bContext *C, struct wmGizmoGroup *gzgroup);

#ifdef __cplusplus
}
#endif
