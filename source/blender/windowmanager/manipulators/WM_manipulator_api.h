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
struct Main;
struct wmKeyConfig;
struct wmManipulator;
struct wmManipulatorType;
struct wmManipulatorGroup;
struct wmManipulatorGroupType;
struct wmManipulatorMap;
struct wmManipulatorMapType;
struct wmManipulatorMapType_Params;

#include "wm_manipulator_fn.h"

/* -------------------------------------------------------------------- */
/* wmManipulator */

struct wmManipulator *WM_manipulator_new(
        const struct wmManipulatorType *mpt,
        struct wmManipulatorGroup *mgroup, const char *name);
void WM_manipulator_delete(
        ListBase *manipulatorlist, struct wmManipulatorMap *mmap, struct wmManipulator *manipulator,
        struct bContext *C);
struct wmManipulatorGroup *WM_manipulator_get_parent_group(struct wmManipulator *manipulator);

void WM_manipulator_set_property(struct wmManipulator *, int slot, struct PointerRNA *ptr, const char *propname);
struct PointerRNA *WM_manipulator_set_operator(struct wmManipulator *, const char *opname);

/* callbacks */
void WM_manipulator_set_fn_custom_handler(struct wmManipulator *manipulator, wmManipulatorFnHandler fn);

void WM_manipulator_set_origin(struct wmManipulator *manipulator, const float origin[3]);
void WM_manipulator_set_offset(struct wmManipulator *manipulator, const float offset[3]);
void WM_manipulator_set_flag(struct wmManipulator *manipulator, const int flag, const bool enable);
void WM_manipulator_set_scale(struct wmManipulator *manipulator, float scale);
void WM_manipulator_set_line_width(struct wmManipulator *manipulator, const float line_width);

void WM_manipulator_get_color(const struct wmManipulator *manipulator, float col[4]);
void WM_manipulator_set_color(struct wmManipulator *manipulator, const float col[4]);
void WM_manipulator_get_color_highlight(const struct wmManipulator *manipulator, float col_hi[4]);
void WM_manipulator_set_color_highlight(struct wmManipulator *manipulator, const float col[4]);

/* manipulator_library_presets.c */
void WM_manipulator_draw_preset_box(const struct wmManipulator *manipulator, float mat[4][4], int select_id);

/* wm_manipulator.c */
const struct wmManipulatorType *WM_manipulatortype_find(const char *idname, bool quiet);
void WM_manipulatortype_append(void (*mnpfunc)(struct wmManipulatorType *));
void WM_manipulatortype_append_ptr(void (*mnpfunc)(struct wmManipulatorType *, void *), void *userdata);
void WM_manipulatortype_iter(struct GHashIterator *ghi);

/* -------------------------------------------------------------------- */
/* wmManipulatorGroup */

struct wmManipulatorGroupType *WM_manipulatorgrouptype_find(
        struct wmManipulatorMapType *mmaptype,
        const char *idname);
struct wmManipulatorGroupType *WM_manipulatorgrouptype_append(
        struct wmManipulatorMapType *mmaptype,
        void (*mgrouptype_func)(struct wmManipulatorGroupType *));
struct wmManipulatorGroupType *WM_manipulatorgrouptype_append_ptr(
        struct wmManipulatorMapType *mmaptype,
        void (*mgrouptype_func)(struct wmManipulatorGroupType *, void *),
        void *userdata);
struct wmManipulatorGroupType *WM_manipulatorgrouptype_append_runtime(
        const struct Main *main, struct wmManipulatorMapType *mmaptype,
        void (*mgrouptype_func)(struct wmManipulatorGroupType *));
struct wmManipulatorGroupType *WM_manipulatorgrouptype_append_ptr_runtime(
        const struct Main *main, struct wmManipulatorMapType *mmaptype,
        void (*mgrouptype_func)(struct wmManipulatorGroupType *, void *),
        void *userdata);

void WM_manipulatorgrouptype_init_runtime(
        const struct Main *bmain, struct wmManipulatorMapType *mmaptype,
        struct wmManipulatorGroupType *mgrouptype);
void WM_manipulatorgrouptype_unregister(
        struct bContext *C, struct Main *bmain, struct wmManipulatorGroupType *mgroup);

struct wmKeyMap *WM_manipulatorgroup_keymap_common(
        const struct wmManipulatorGroupType *mgrouptype, struct wmKeyConfig *config);
struct wmKeyMap *WM_manipulatorgroup_keymap_common_sel(
        const struct wmManipulatorGroupType *mgrouptype, struct wmKeyConfig *config);


/* -------------------------------------------------------------------- */
/* wmManipulatorMap */

struct wmManipulatorMapType *WM_manipulatormaptype_find(
        const struct wmManipulatorMapType_Params *mmap_params);
struct wmManipulatorMapType *WM_manipulatormaptype_ensure(
        const struct wmManipulatorMapType_Params *mmap_params);

struct wmManipulatorMap *WM_manipulatormap_new_from_type(
        const struct wmManipulatorMapType_Params *mmap_params);
void WM_manipulatormap_tag_refresh(struct wmManipulatorMap *mmap);
void WM_manipulatormap_draw(struct wmManipulatorMap *mmap, const struct bContext *C, const int drawstep);
void WM_manipulatormap_add_handlers(struct ARegion *ar, struct wmManipulatorMap *mmap);
bool WM_manipulatormap_select_all(struct bContext *C, struct wmManipulatorMap *mmap, const int action);
bool WM_manipulatormap_cursor_set(const struct wmManipulatorMap *mmap, struct wmWindow *win);

#endif  /* __WM_MANIPULATOR_API_H__ */

