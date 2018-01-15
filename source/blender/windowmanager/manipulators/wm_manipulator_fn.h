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
typedef bool (*wmManipulatorGroupFnPoll)(
        const struct bContext *, struct wmManipulatorGroupType *)
        ATTR_WARN_UNUSED_RESULT;
typedef void (*wmManipulatorGroupFnInit)(
        const struct bContext *, struct wmManipulatorGroup *);
typedef void (*wmManipulatorGroupFnRefresh)(
        const struct bContext *, struct wmManipulatorGroup *);
typedef void (*wmManipulatorGroupFnDrawPrepare)(
        const struct bContext *, struct wmManipulatorGroup *);
typedef struct wmKeyMap *(*wmManipulatorGroupFnSetupKeymap)(
        const struct wmManipulatorGroupType *, struct wmKeyConfig *)
        ATTR_WARN_UNUSED_RESULT;
typedef void (*wmManipulatorGroupFnMsgBusSubscribe)(
        const struct bContext *, struct wmManipulatorGroup *, struct wmMsgBus *);

/* wmManipulator */
/* See: wmManipulatorType for docs on each type. */

typedef void    (*wmManipulatorFnSetup)(struct wmManipulator *);
typedef void    (*wmManipulatorFnDraw)(const struct bContext *, struct wmManipulator *);
typedef void    (*wmManipulatorFnDrawSelect)(const struct bContext *, struct wmManipulator *, int);
typedef int     (*wmManipulatorFnTestSelect)(struct bContext *, struct wmManipulator *, const struct wmEvent *);
typedef int     (*wmManipulatorFnModal)(struct bContext *, struct wmManipulator *, const struct wmEvent *, eWM_ManipulatorTweak);
typedef void    (*wmManipulatorFnPropertyUpdate)(struct wmManipulator *, struct wmManipulatorProperty *);
typedef void    (*wmManipulatorFnMatrixBasisGet)(const struct wmManipulator *, float[4][4]);
typedef int     (*wmManipulatorFnInvoke)(struct bContext *, struct wmManipulator *, const struct wmEvent *);
typedef void    (*wmManipulatorFnExit)(struct bContext *, struct wmManipulator *, const bool);
typedef int     (*wmManipulatorFnCursorGet)(struct wmManipulator *);
typedef void    (*wmManipulatorFnSelectRefresh)(struct wmManipulator *);
typedef void    (*wmManipulatorFnFree)(struct wmManipulator *);

/* wmManipulatorProperty ('value' type defined by 'wmManipulatorProperty.data_type') */
typedef void (*wmManipulatorPropertyFnGet)(
        const struct wmManipulator *, struct wmManipulatorProperty *,
        /* typically 'float *' */
        void *value);
typedef void (*wmManipulatorPropertyFnSet)(
        const struct wmManipulator *, struct wmManipulatorProperty *,
        /* typically 'const float *' */
        const void *value);
typedef void (*wmManipulatorPropertyFnRangeGet)(
        const struct wmManipulator *, struct wmManipulatorProperty *,
        /* typically 'float[2]' */
        void *range);
typedef void (*wmManipulatorPropertyFnFree)(
        const struct wmManipulator *, struct wmManipulatorProperty *);

typedef struct wmManipulatorPropertyFnParams {
	wmManipulatorPropertyFnGet value_get_fn;
	wmManipulatorPropertyFnSet value_set_fn;
	wmManipulatorPropertyFnRangeGet range_get_fn;
	wmManipulatorPropertyFnFree free_fn;
	void *user_data;
} wmManipulatorPropertyFnParams;

#endif  /* __WM_MANIPULATOR_FN_H__ */
