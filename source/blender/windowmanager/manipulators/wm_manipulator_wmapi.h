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

/** \file blender/windowmanager/manipulators/wm_manipulator_wmapi.h
 *  \ingroup wm
 *
 * \name Manipulators Window Manager API
 * API for usage in window manager code only. It should contain all functionality
 * needed to hook up the manipulator system with Blender's window manager. It's
 * mostly the event system that needs to communicate with manipulator code.
 *
 * Only included in wm.h and lower level files.
 */


#ifndef __WM_MANIPULATOR_WMAPI_H__
#define __WM_MANIPULATOR_WMAPI_H__

struct wmEventHandler;
struct wmManipulatorMap;
struct wmOperatorType;
struct wmOperator;


/* -------------------------------------------------------------------- */
/* wmManipulator */

/* wm_manipulator_type.c, for init/exit */
void wm_manipulatortype_free(void);
void wm_manipulatortype_init(void);

/* wm_manipulatorgroup_type.c, for init/exit */
void wm_manipulatorgrouptype_free(void);
void wm_manipulatorgrouptype_init(void);

/* -------------------------------------------------------------------- */
/* wmManipulatorGroup */

void MANIPULATORGROUP_OT_manipulator_select(struct wmOperatorType *ot);
void MANIPULATORGROUP_OT_manipulator_tweak(struct wmOperatorType *ot);

bool wm_manipulatorgroup_is_any_selected(const struct wmManipulatorGroup *mgroup);

/* -------------------------------------------------------------------- */
/* wmManipulatorMap */

void wm_manipulatormap_remove(struct wmManipulatorMap *mmap);

void wm_manipulators_keymap(struct wmKeyConfig *keyconf);

void wm_manipulatormaps_handled_modal_update(
        bContext *C, struct wmEvent *event, struct wmEventHandler *handler);
void wm_manipulatormap_handler_context(bContext *C, struct wmEventHandler *handler);

struct wmManipulator *wm_manipulatormap_highlight_find(
        struct wmManipulatorMap *mmap, bContext *C, const struct wmEvent *event,
        int *r_part);
void wm_manipulatormap_highlight_set(
        struct wmManipulatorMap *mmap, const bContext *C,
        struct wmManipulator *mpr, int part);
struct wmManipulator *wm_manipulatormap_highlight_get(struct wmManipulatorMap *mmap);
void wm_manipulatormap_modal_set(
        struct wmManipulatorMap *mmap, bContext *C, struct wmManipulator *mpr,
        const struct wmEvent *event, bool enable);

struct wmManipulator *wm_manipulatormap_modal_get(struct wmManipulatorMap *mmap);
struct wmManipulator **wm_manipulatormap_selected_get(wmManipulatorMap *mmap, int *r_selected_len);
struct ListBase *wm_manipulatormap_groups_get(wmManipulatorMap *mmap);

/* -------------------------------------------------------------------- */
/* wmManipulatorMapType */

void wm_manipulatormaptypes_free(void);

#endif  /* __WM_MANIPULATOR_WMAPI_H__ */

