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

/** \file blender/windowmanager/manipulators/WM_manipulator_api.h
 *  \ingroup wm
 *
 * \name Manipulator API
 * \brief API for external use of wmManipulator types.
 *
 * Only included in WM_api.h
 */


#ifndef __WM_MANIPULATOR_API_H__
#define __WM_MANIPULATOR_API_H__

struct ARegion;
struct GHashIterator;
struct IDProperty;
struct Main;
struct PropertyRNA;
struct wmKeyConfig;
struct wmManipulator;
struct wmManipulatorProperty;
struct wmManipulatorPropertyType;
struct wmManipulatorType;
struct wmManipulatorGroup;
struct wmManipulatorGroupType;
struct wmManipulatorMap;
struct wmManipulatorMapType;
struct wmManipulatorMapType_Params;

#include "wm_manipulator_fn.h"

/* -------------------------------------------------------------------- */
/* wmManipulator */

struct wmManipulator *WM_manipulator_new_ptr(
        const struct wmManipulatorType *wt, struct wmManipulatorGroup *mgroup,
        const char *name, struct PointerRNA *properties);
struct wmManipulator *WM_manipulator_new(
        const char *idname, struct wmManipulatorGroup *mgroup,
        const char *name, struct PointerRNA *properties);
void WM_manipulator_free(
        ListBase *manipulatorlist, struct wmManipulatorMap *mmap, struct wmManipulator *mpr,
        struct bContext *C);

void WM_manipulator_name_set(struct wmManipulatorGroup *mgroup, struct wmManipulator *mpr, const char *name);

struct PointerRNA *WM_manipulator_set_operator(
        struct wmManipulator *, struct wmOperatorType *ot, struct IDProperty *properties);

/* callbacks */
void WM_manipulator_set_fn_custom_modal(struct wmManipulator *mpr, wmManipulatorFnModal fn);

void WM_manipulator_set_matrix_location(
        struct wmManipulator *mpr, const float origin[3]);
void WM_manipulator_set_matrix_rotation_from_z_axis(
        struct wmManipulator *mpr, const float z_axis[3]);
void WM_manipulator_set_matrix_rotation_from_yz_axis(
        struct wmManipulator *mpr, const float y_axis[3], const float z_axis[3]);

void WM_manipulator_set_matrix_offset_location(
        struct wmManipulator *mpr, const float origin[3]);
void WM_manipulator_set_matrix_offset_rotation_from_z_axis(
        struct wmManipulator *mpr, const float z_axis[3]);
void WM_manipulator_set_matrix_offset_rotation_from_yz_axis(
        struct wmManipulator *mpr, const float y_axis[3], const float z_axis[3]);

void WM_manipulator_set_flag(struct wmManipulator *mpr, const int flag, const bool enable);
void WM_manipulator_set_scale(struct wmManipulator *mpr, float scale);
void WM_manipulator_set_line_width(struct wmManipulator *mpr, const float line_width);

void WM_manipulator_get_color(const struct wmManipulator *mpr, float col[4]);
void WM_manipulator_set_color(struct wmManipulator *mpr, const float col[4]);
void WM_manipulator_get_color_highlight(const struct wmManipulator *mpr, float col_hi[4]);
void WM_manipulator_set_color_highlight(struct wmManipulator *mpr, const float col[4]);

/* properties */
void WM_manipulator_properties_create_ptr(struct PointerRNA *ptr, struct wmManipulatorType *wt);
void WM_manipulator_properties_create(struct PointerRNA *ptr, const char *opstring);
void WM_manipulator_properties_alloc(struct PointerRNA **ptr, struct IDProperty **properties, const char *wtstring);
void WM_manipulator_properties_sanitize(struct PointerRNA *ptr, const bool no_context);
bool WM_manipulator_properties_default(struct PointerRNA *ptr, const bool do_update);
void WM_manipulator_properties_reset(struct wmManipulator *op);
void WM_manipulator_properties_clear(struct PointerRNA *ptr);
void WM_manipulator_properties_free(struct PointerRNA *ptr);


/* wm_manipulator_type.c */
const struct wmManipulatorType *WM_manipulatortype_find(const char *idname, bool quiet);
void WM_manipulatortype_append(void (*wtfunc)(struct wmManipulatorType *));
void WM_manipulatortype_append_ptr(void (*mnpfunc)(struct wmManipulatorType *, void *), void *userdata);
bool WM_manipulatortype_remove(const char *idname);
void WM_manipulatortype_remove_ptr(struct wmManipulatorType *wt);
void WM_manipulatortype_iter(struct GHashIterator *ghi);

/* wm_manipulator_group_type.c */
struct wmManipulatorGroupType *WM_manipulatorgrouptype_find(const char *idname, bool quiet);
struct wmManipulatorGroupType *WM_manipulatorgrouptype_append(void (*wtfunc)(struct wmManipulatorGroupType *));
struct wmManipulatorGroupType *WM_manipulatorgrouptype_append_ptr(void (*mnpfunc)(struct wmManipulatorGroupType *, void *), void *userdata);
bool WM_manipulatorgrouptype_remove(const char *idname);
void WM_manipulatorgrouptype_remove_ptr(struct wmManipulatorGroupType *wt);
void WM_manipulatorgrouptype_iter(struct GHashIterator *ghi);

struct wmManipulatorGroupTypeRef *WM_manipulatorgrouptype_append_and_link(
        struct wmManipulatorMapType *mmap_type,
        void (*wtfunc)(struct wmManipulatorGroupType *));

/* wm_manipulator_map.c */

/* Dynamic Updates (for RNA runtime registration) */
void WM_manipulatorconfig_update_tag_init(struct wmManipulatorMapType *mmap_type, struct wmManipulatorGroupType *wgt);
void WM_manipulatorconfig_update_tag_remove(struct wmManipulatorMapType *mmap_type, struct wmManipulatorGroupType *wgt);
void WM_manipulatorconfig_update(struct Main *bmain);


/* wm_maniulator_target_props.c */
struct wmManipulatorProperty *WM_manipulator_target_property_array(struct wmManipulator *mpr);
struct wmManipulatorProperty *WM_manipulator_target_property_at_index(
        struct wmManipulator *mpr, int index);
struct wmManipulatorProperty *WM_manipulator_target_property_find(
        struct wmManipulator *mpr, const char *idname);

void WM_manipulator_target_property_def_rna_ptr(
        struct wmManipulator *mpr, const struct wmManipulatorPropertyType *mpr_prop_type,
        struct PointerRNA *ptr, struct PropertyRNA *prop, int index);
void WM_manipulator_target_property_def_rna(
        struct wmManipulator *mpr, const char *idname,
        struct PointerRNA *ptr, const char *propname, int index);

void WM_manipulator_target_property_def_func_ptr(
        struct wmManipulator *mpr, const struct wmManipulatorPropertyType *mpr_prop_type,
        const struct wmManipulatorPropertyFnParams *params);
void WM_manipulator_target_property_def_func(
        struct wmManipulator *mpr, const char *idname,
        const struct wmManipulatorPropertyFnParams *params);

bool WM_manipulator_target_property_is_valid_any(struct wmManipulator *mpr);
bool WM_manipulator_target_property_is_valid(
        const struct wmManipulatorProperty *mpr_prop);
float WM_manipulator_target_property_value_get(
        const struct wmManipulator *mpr, struct wmManipulatorProperty *mpr_prop);
void  WM_manipulator_target_property_value_set(
        struct bContext *C, const struct wmManipulator *mpr, struct wmManipulatorProperty *mpr_prop,
        const float value);

void WM_manipulator_target_property_value_get_array(
        const struct wmManipulator *mpr, struct wmManipulatorProperty *mpr_prop,
        float *value);
void WM_manipulator_target_property_value_set_array(
        struct bContext *C, const struct wmManipulator *mpr, struct wmManipulatorProperty *mpr_prop,
        const float *value);

void WM_manipulator_target_property_range_get(
        const struct wmManipulator *mpr, struct wmManipulatorProperty *mpr_prop,
        float range[2]);

/* definitions */
const struct wmManipulatorPropertyType *WM_manipulatortype_target_property_find(
        const struct wmManipulatorType *wt, const char *idname);
void WM_manipulatortype_target_property_def(
        struct wmManipulatorType *wt, const char *idname, int data_type, int array_length);


/* -------------------------------------------------------------------- */
/* wmManipulatorGroup */

/* Callbacks for 'wmManipulatorGroupType.setup_keymap' */
struct wmKeyMap *WM_manipulatorgroup_keymap_common(
        const struct wmManipulatorGroupType *wgt, struct wmKeyConfig *config);
struct wmKeyMap *WM_manipulatorgroup_keymap_common_select(
        const struct wmManipulatorGroupType *wgt, struct wmKeyConfig *config);


/* -------------------------------------------------------------------- */
/* wmManipulatorMap */

struct wmManipulatorMap *WM_manipulatormap_new_from_type(
        const struct wmManipulatorMapType_Params *mmap_params);
const struct ListBase *WM_manipulatormap_group_list(struct wmManipulatorMap *mmap);
void WM_manipulatormap_tag_refresh(struct wmManipulatorMap *mmap);
void WM_manipulatormap_draw(struct wmManipulatorMap *mmap, const struct bContext *C, const int drawstep);
void WM_manipulatormap_add_handlers(struct ARegion *ar, struct wmManipulatorMap *mmap);
bool WM_manipulatormap_select_all(struct bContext *C, struct wmManipulatorMap *mmap, const int action);
bool WM_manipulatormap_cursor_set(const struct wmManipulatorMap *mmap, struct wmWindow *win);

/* -------------------------------------------------------------------- */
/* wmManipulatorMapType */

struct wmManipulatorMapType *WM_manipulatormaptype_find(
        const struct wmManipulatorMapType_Params *mmap_params);
struct wmManipulatorMapType *WM_manipulatormaptype_ensure(
        const struct wmManipulatorMapType_Params *mmap_params);

struct wmManipulatorGroupTypeRef *WM_manipulatormaptype_group_find(
        struct wmManipulatorMapType *mmap_type,
        const char *idname);
struct wmManipulatorGroupTypeRef *WM_manipulatormaptype_group_find_ptr(
        struct wmManipulatorMapType *mmap_type,
        const struct wmManipulatorGroupType *wgt);
struct wmManipulatorGroupTypeRef *WM_manipulatormaptype_group_link(
        struct wmManipulatorMapType *mmap_type,
        const char *idname);
struct wmManipulatorGroupTypeRef *WM_manipulatormaptype_group_link_ptr(
        struct wmManipulatorMapType *mmap_type,
        struct wmManipulatorGroupType *wgt);

void WM_manipulatormaptype_group_init_runtime(
        const struct Main *bmain, struct wmManipulatorMapType *mmap_type,
        struct wmManipulatorGroupType *wgt);
void WM_manipulatormaptype_group_unlink(
        struct bContext *C, struct Main *bmain, struct wmManipulatorMapType *mmap_type,
        const struct wmManipulatorGroupType *wgt);

void WM_manipulatormaptype_group_free(struct wmManipulatorGroupTypeRef *wgt);

/* -------------------------------------------------------------------- */
/* Manipulator Add/Remove (High level API) */

void WM_manipulator_group_add_ptr_ex(
        struct wmManipulatorGroupType *wgt,
        struct wmManipulatorMapType *mmap_type);
void WM_manipulator_group_add_ptr(
        struct wmManipulatorGroupType *wgt);
void WM_manipulator_group_add(const char *idname);

void WM_manipulator_group_remove_ptr_ex(
        struct Main *bmain, struct wmManipulatorGroupType *wgt,
        struct wmManipulatorMapType *mmap_type);
void WM_manipulator_group_remove_ptr(
        struct Main *bmain, struct wmManipulatorGroupType *wgt);
void WM_manipulator_group_remove(struct Main *bmain, const char *idname);

void WM_manipulator_group_remove_ptr_delayed_ex(
        struct wmManipulatorGroupType *wgt,
        struct wmManipulatorMapType *mmap_type);
void WM_manipulator_group_remove_ptr_delayed(
        struct wmManipulatorGroupType *wgt);
void WM_manipulator_group_remove_delayed(const char *idname);

#endif  /* __WM_MANIPULATOR_API_H__ */
