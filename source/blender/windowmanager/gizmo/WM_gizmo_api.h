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
 * The Original Code is Copyright (C) 2016 Blender Foundation.
 * All rights reserved.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/gizmo/WM_gizmo_api.h
 *  \ingroup wm
 *
 * \name Gizmo API
 * \brief API for external use of wmGizmo types.
 *
 * Only included in WM_api.h
 */


#ifndef __WM_GIZMO_API_H__
#define __WM_GIZMO_API_H__

struct ARegion;
struct GHashIterator;
struct IDProperty;
struct Main;
struct PropertyRNA;
struct wmKeyConfig;
struct wmGizmo;
struct wmGizmoProperty;
struct wmGizmoPropertyType;
struct wmGizmoType;
struct wmGizmoGroup;
struct wmGizmoGroupType;
struct wmGizmoMap;
struct wmGizmoMapType;
struct wmGizmoMapType_Params;
struct wmMsgSubscribeKey;
struct wmMsgSubscribeValue;

#include "wm_gizmo_fn.h"

/* -------------------------------------------------------------------- */
/* wmGizmo */

struct wmGizmo *WM_gizmo_new_ptr(
        const struct wmGizmoType *gzt, struct wmGizmoGroup *gzgroup,
        struct PointerRNA *properties);
struct wmGizmo *WM_gizmo_new(
        const char *idname, struct wmGizmoGroup *gzgroup,
        struct PointerRNA *properties);
void WM_gizmo_free(struct wmGizmo *gz);
void WM_gizmo_unlink(
        ListBase *gizmolist, struct wmGizmoMap *gzmap, struct wmGizmo *gz,
        struct bContext *C);

void WM_gizmo_name_set(struct wmGizmoGroup *gzgroup, struct wmGizmo *gz, const char *name);

bool WM_gizmo_select_unlink(struct wmGizmoMap *gzmap, struct wmGizmo *gz);
bool WM_gizmo_select_set(struct wmGizmoMap *gzmap, struct wmGizmo *gz, bool select);
void WM_gizmo_highlight_set(struct wmGizmoMap *gzmap, struct wmGizmo *gz);

void WM_gizmo_modal_set_from_setup(
        struct wmGizmoMap *gzmap, struct bContext *C,
        struct wmGizmo *gz, int part_index, const struct wmEvent *event);

struct wmGizmoOpElem *WM_gizmo_operator_get(
        struct wmGizmo *gz, int part_index);
struct PointerRNA *WM_gizmo_operator_set(
        struct wmGizmo *gz, int part_index,
        struct wmOperatorType *ot, struct IDProperty *properties);

/* callbacks */
void WM_gizmo_set_fn_custom_modal(struct wmGizmo *gz, wmGizmoFnModal fn);

void WM_gizmo_set_matrix_location(
        struct wmGizmo *gz, const float origin[3]);
void WM_gizmo_set_matrix_rotation_from_z_axis(
        struct wmGizmo *gz, const float z_axis[3]);
void WM_gizmo_set_matrix_rotation_from_yz_axis(
        struct wmGizmo *gz, const float y_axis[3], const float z_axis[3]);

void WM_gizmo_set_matrix_offset_location(
        struct wmGizmo *gz, const float origin[3]);
void WM_gizmo_set_matrix_offset_rotation_from_z_axis(
        struct wmGizmo *gz, const float z_axis[3]);
void WM_gizmo_set_matrix_offset_rotation_from_yz_axis(
        struct wmGizmo *gz, const float y_axis[3], const float z_axis[3]);

void WM_gizmo_set_flag(struct wmGizmo *gz, const int flag, const bool enable);
void WM_gizmo_set_scale(struct wmGizmo *gz, const float scale);
void WM_gizmo_set_line_width(struct wmGizmo *gz, const float line_width);

void WM_gizmo_get_color(const struct wmGizmo *gz, float color[4]);
void WM_gizmo_set_color(struct wmGizmo *gz, const float color[4]);
void WM_gizmo_get_color_highlight(const struct wmGizmo *gz, float color_hi[4]);
void WM_gizmo_set_color_highlight(struct wmGizmo *gz, const float color[4]);

/**
 * Leaving values NULL use values from #wmGizmo.
 */
struct WM_GizmoMatrixParams {
	const float(*matrix_space)[4];
	const float(*matrix_basis)[4];
	const float(*matrix_offset)[4];
	const float *scale_final;
};

void WM_gizmo_calc_matrix_final_params(
        const struct wmGizmo *gz, const struct WM_GizmoMatrixParams *params,
        float r_mat[4][4]);
void WM_gizmo_calc_matrix_final_no_offset(
        const struct wmGizmo *gz, float r_mat[4][4]);

void WM_gizmo_calc_matrix_final(
        const struct wmGizmo *gz, float r_mat[4][4]);

/* properties */
void WM_gizmo_properties_create_ptr(struct PointerRNA *ptr, struct wmGizmoType *gzt);
void WM_gizmo_properties_create(struct PointerRNA *ptr, const char *opstring);
void WM_gizmo_properties_alloc(struct PointerRNA **ptr, struct IDProperty **properties, const char *wtstring);
void WM_gizmo_properties_sanitize(struct PointerRNA *ptr, const bool no_context);
bool WM_gizmo_properties_default(struct PointerRNA *ptr, const bool do_update);
void WM_gizmo_properties_reset(struct wmGizmo *op);
void WM_gizmo_properties_clear(struct PointerRNA *ptr);
void WM_gizmo_properties_free(struct PointerRNA *ptr);


/* wm_gizmo_type.c */
const struct wmGizmoType *WM_gizmotype_find(const char *idname, bool quiet);
void WM_gizmotype_append(void (*wtfunc)(struct wmGizmoType *));
void WM_gizmotype_append_ptr(void (*mnpfunc)(struct wmGizmoType *, void *), void *userdata);
bool WM_gizmotype_remove(struct bContext *C, struct Main *bmain, const char *idname);
void WM_gizmotype_remove_ptr(struct bContext *C, struct Main *bmain, struct wmGizmoType *gzt);
void WM_gizmotype_iter(struct GHashIterator *ghi);

/* wm_gizmo_group_type.c */
struct wmGizmoGroupType *WM_gizmogrouptype_find(const char *idname, bool quiet);
struct wmGizmoGroupType *WM_gizmogrouptype_append(void (*wtfunc)(struct wmGizmoGroupType *));
struct wmGizmoGroupType *WM_gizmogrouptype_append_ptr(void (*mnpfunc)(struct wmGizmoGroupType *, void *), void *userdata);
bool WM_gizmogrouptype_free(const char *idname);
void WM_gizmogrouptype_free_ptr(struct wmGizmoGroupType *wt);
void WM_gizmogrouptype_iter(struct GHashIterator *ghi);

struct wmGizmoGroupTypeRef *WM_gizmogrouptype_append_and_link(
        struct wmGizmoMapType *gzmap_type,
        void (*wtfunc)(struct wmGizmoGroupType *));

/* wm_gizmo_map.c */

/* Dynamic Updates (for RNA runtime registration) */
void WM_gizmoconfig_update_tag_init(struct wmGizmoMapType *gzmap_type, struct wmGizmoGroupType *gzgt);
void WM_gizmoconfig_update_tag_remove(struct wmGizmoMapType *gzmap_type, struct wmGizmoGroupType *gzgt);
void WM_gizmoconfig_update(struct Main *bmain);


/* wm_maniulator_target_props.c */
struct wmGizmoProperty *WM_gizmo_target_property_array(struct wmGizmo *gz);
struct wmGizmoProperty *WM_gizmo_target_property_at_index(
        struct wmGizmo *gz, int index);
struct wmGizmoProperty *WM_gizmo_target_property_find(
        struct wmGizmo *gz, const char *idname);

void WM_gizmo_target_property_def_rna_ptr(
        struct wmGizmo *gz, const struct wmGizmoPropertyType *gz_prop_type,
        struct PointerRNA *ptr, struct PropertyRNA *prop, int index);
void WM_gizmo_target_property_def_rna(
        struct wmGizmo *gz, const char *idname,
        struct PointerRNA *ptr, const char *propname, int index);

void WM_gizmo_target_property_def_func_ptr(
        struct wmGizmo *gz, const struct wmGizmoPropertyType *gz_prop_type,
        const struct wmGizmoPropertyFnParams *params);
void WM_gizmo_target_property_def_func(
        struct wmGizmo *gz, const char *idname,
        const struct wmGizmoPropertyFnParams *params);

void WM_gizmo_target_property_clear_rna_ptr(
        struct wmGizmo *gz, const struct wmGizmoPropertyType *gz_prop_type);
void WM_gizmo_target_property_clear_rna(
        struct wmGizmo *gz, const char *idname);

bool WM_gizmo_target_property_is_valid_any(struct wmGizmo *gz);
bool WM_gizmo_target_property_is_valid(
        const struct wmGizmoProperty *gz_prop);
float WM_gizmo_target_property_value_get(
        const struct wmGizmo *gz, struct wmGizmoProperty *gz_prop);
void  WM_gizmo_target_property_value_set(
        struct bContext *C, const struct wmGizmo *gz, struct wmGizmoProperty *gz_prop,
        const float value);

void WM_gizmo_target_property_value_get_array(
        const struct wmGizmo *gz, struct wmGizmoProperty *gz_prop,
        float *value);
void WM_gizmo_target_property_value_set_array(
        struct bContext *C, const struct wmGizmo *gz, struct wmGizmoProperty *gz_prop,
        const float *value);

bool WM_gizmo_target_property_range_get(
        const struct wmGizmo *gz, struct wmGizmoProperty *gz_prop,
        float range[2]);

int WM_gizmo_target_property_array_length(
        const struct wmGizmo *gz, struct wmGizmoProperty *gz_prop);

/* definitions */
const struct wmGizmoPropertyType *WM_gizmotype_target_property_find(
        const struct wmGizmoType *gzt, const char *idname);
void WM_gizmotype_target_property_def(
        struct wmGizmoType *gzt, const char *idname, int data_type, int array_length);

/* utilities */
void WM_gizmo_do_msg_notify_tag_refresh(
        struct bContext *C, struct wmMsgSubscribeKey *msg_key, struct wmMsgSubscribeValue *msg_val);
void WM_gizmo_target_property_subscribe_all(
        struct wmGizmo *gz, struct wmMsgBus *mbus, struct ARegion *ar);

/* -------------------------------------------------------------------- */
/* wmGizmoGroup */

/* Callbacks for 'wmGizmoGroupType.setup_keymap' */
struct wmKeyMap *WM_gizmogroup_keymap_common(
        const struct wmGizmoGroupType *gzgt, struct wmKeyConfig *config);
struct wmKeyMap *WM_gizmogroup_keymap_common_select(
        const struct wmGizmoGroupType *gzgt, struct wmKeyConfig *config);


/* -------------------------------------------------------------------- */
/* wmGizmoMap */

struct wmGizmoMap *WM_gizmomap_new_from_type(
        const struct wmGizmoMapType_Params *gzmap_params);
const struct ListBase *WM_gizmomap_group_list(struct wmGizmoMap *gzmap);
struct wmGizmoGroup *WM_gizmomap_group_find(
        struct wmGizmoMap *gzmap,
        const char *idname);
struct wmGizmoGroup *WM_gizmomap_group_find_ptr(
        struct wmGizmoMap *gzmap,
        const struct wmGizmoGroupType *gzgt);
void WM_gizmomap_tag_refresh(struct wmGizmoMap *gzmap);
void WM_gizmomap_draw(
        struct wmGizmoMap *gzmap, const struct bContext *C, const eWM_GizmoFlagMapDrawStep drawstep);
void WM_gizmomap_add_handlers(struct ARegion *ar, struct wmGizmoMap *gzmap);
bool WM_gizmomap_select_all(struct bContext *C, struct wmGizmoMap *gzmap, const int action);
bool WM_gizmomap_cursor_set(const struct wmGizmoMap *gzmap, struct wmWindow *win);
void WM_gizmomap_message_subscribe(
        struct bContext *C, struct wmGizmoMap *gzmap, struct ARegion *ar, struct wmMsgBus *mbus);
bool WM_gizmomap_is_any_selected(const struct wmGizmoMap *gzmap);
bool WM_gizmomap_minmax(
        const struct wmGizmoMap *gzmap, bool use_hidden, bool use_select,
        float r_min[3], float r_max[3]);

struct ARegion *WM_gizmomap_tooltip_init(
        struct bContext *C, struct ARegion *ar, bool *r_exit_on_event);

/* -------------------------------------------------------------------- */
/* wmGizmoMapType */

struct wmGizmoMapType *WM_gizmomaptype_find(
        const struct wmGizmoMapType_Params *gzmap_params);
struct wmGizmoMapType *WM_gizmomaptype_ensure(
        const struct wmGizmoMapType_Params *gzmap_params);

struct wmGizmoGroupTypeRef *WM_gizmomaptype_group_find(
        struct wmGizmoMapType *gzmap_type,
        const char *idname);
struct wmGizmoGroupTypeRef *WM_gizmomaptype_group_find_ptr(
        struct wmGizmoMapType *gzmap_type,
        const struct wmGizmoGroupType *gzgt);
struct wmGizmoGroupTypeRef *WM_gizmomaptype_group_link(
        struct wmGizmoMapType *gzmap_type,
        const char *idname);
struct wmGizmoGroupTypeRef *WM_gizmomaptype_group_link_ptr(
        struct wmGizmoMapType *gzmap_type,
        struct wmGizmoGroupType *gzgt);

void WM_gizmomaptype_group_init_runtime_keymap(
        const struct Main *bmain,
        struct wmGizmoGroupType *gzgt);
void WM_gizmomaptype_group_init_runtime(
        const struct Main *bmain, struct wmGizmoMapType *gzmap_type,
        struct wmGizmoGroupType *gzgt);
void WM_gizmomaptype_group_unlink(
        struct bContext *C, struct Main *bmain, struct wmGizmoMapType *gzmap_type,
        const struct wmGizmoGroupType *gzgt);

void WM_gizmomaptype_group_free(struct wmGizmoGroupTypeRef *gzgt);

/* -------------------------------------------------------------------- */
/* GizmoGroup */

/* Add/Ensure/Remove (High level API) */

void WM_gizmo_group_type_add_ptr_ex(
        struct wmGizmoGroupType *gzgt,
        struct wmGizmoMapType *gzmap_type);
void WM_gizmo_group_type_add_ptr(
        struct wmGizmoGroupType *gzgt);
void WM_gizmo_group_type_add(const char *idname);

void WM_gizmo_group_type_ensure_ptr_ex(
        struct wmGizmoGroupType *gzgt,
        struct wmGizmoMapType *gzmap_type);
void WM_gizmo_group_type_ensure_ptr(
        struct wmGizmoGroupType *gzgt);
void WM_gizmo_group_type_ensure(const char *idname);

void WM_gizmo_group_type_remove_ptr_ex(
        struct Main *bmain, struct wmGizmoGroupType *gzgt,
        struct wmGizmoMapType *gzmap_type);
void WM_gizmo_group_type_remove_ptr(
        struct Main *bmain, struct wmGizmoGroupType *gzgt);
void WM_gizmo_group_type_remove(struct Main *bmain, const char *idname);

void WM_gizmo_group_type_unlink_delayed_ptr_ex(
        struct wmGizmoGroupType *gzgt,
        struct wmGizmoMapType *gzmap_type);
void WM_gizmo_group_type_unlink_delayed_ptr(
        struct wmGizmoGroupType *gzgt);
void WM_gizmo_group_type_unlink_delayed(const char *idname);

/* Utilities */
bool WM_gizmo_context_check_drawstep(const struct bContext *C, eWM_GizmoFlagMapDrawStep step);

bool WM_gizmo_group_type_poll(const struct bContext *C, const struct wmGizmoGroupType *gzgt);

#endif  /* __WM_GIZMO_API_H__ */
