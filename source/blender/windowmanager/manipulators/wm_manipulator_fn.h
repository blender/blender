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
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/windowmanager/manipulators/wm_manipulator_fn.h
 *  \ingroup wm
 *
 * Callback function definitions, needed for both Types & API headers.
 */


#ifndef __WM_MANIPULATOR_FN_H__
#define __WM_MANIPULATOR_FN_H__

#include "BLI_compiler_attrs.h"

/* wmManipulatorGroup */
typedef bool (*wmManipulatorGroupFnPoll)(const struct bContext *, struct wmManipulatorGroupType *) ATTR_WARN_UNUSED_RESULT;
typedef void (*wmManipulatorGroupFnInit)(const struct bContext *, struct wmManipulatorGroup *);
typedef void (*wmManipulatorGroupFnRefresh)(const struct bContext *, struct wmManipulatorGroup *);
typedef void (*wmManipulatorGroupFnDrawPrepare)(const struct bContext *, struct wmManipulatorGroup *);

/* wmManipulator */
typedef void    (*wmManipulatorFnDraw)(const struct bContext *, struct wmManipulator *);
typedef void    (*wmManipulatorFnDrawSelect)(const struct bContext *, struct wmManipulator *, int);
typedef int     (*wmManipulatorFnIntersect)(struct bContext *, const struct wmEvent *, struct wmManipulator *);
typedef int     (*wmManipulatorFnHandler)(struct bContext *, const struct wmEvent *, struct wmManipulator *, const int);
typedef void    (*wmManipulatorFnPropDataUpdate)(struct wmManipulator *, int);
typedef void    (*wmManipulatorFnFinalPositionGet)(struct wmManipulator *, float[]);
typedef int     (*wmManipulatorFnInvoke)(struct bContext *, const struct wmEvent *, struct wmManipulator *);
typedef void    (*wmManipulatorFnExit)(struct bContext *, struct wmManipulator *, const bool);
typedef int     (*wmManipulatorFnCursorGet)(struct wmManipulator *);
typedef void    (*wmManipulatorFnSelect)(struct bContext *, struct wmManipulator *, const int);

#endif  /* __WM_MANIPULATOR_FN_H__ */
