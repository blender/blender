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

/** \file blender/windowmanager/gizmo/wm_gizmo_fn.h
 *  \ingroup wm
 *
 * Callback function definitions, needed for both Types & API headers.
 */

#ifndef __WM_GIZMO_FN_H__
#define __WM_GIZMO_FN_H__

#include "BLI_compiler_attrs.h"

/* wmGizmoGroup */
typedef bool (*wmGizmoGroupFnPoll)(
        const struct bContext *, struct wmGizmoGroupType *)
        ATTR_WARN_UNUSED_RESULT;
typedef void (*wmGizmoGroupFnInit)(
        const struct bContext *, struct wmGizmoGroup *);
typedef void (*wmGizmoGroupFnRefresh)(
        const struct bContext *, struct wmGizmoGroup *);
typedef void (*wmGizmoGroupFnDrawPrepare)(
        const struct bContext *, struct wmGizmoGroup *);
typedef struct wmKeyMap *(*wmGizmoGroupFnSetupKeymap)(
        const struct wmGizmoGroupType *, struct wmKeyConfig *)
        ATTR_WARN_UNUSED_RESULT;
typedef void (*wmGizmoGroupFnMsgBusSubscribe)(
        const struct bContext *, struct wmGizmoGroup *, struct wmMsgBus *);

/* wmGizmo */
/* See: wmGizmoType for docs on each type. */

typedef void    (*wmGizmoFnSetup)(struct wmGizmo *);
typedef void    (*wmGizmoFnDraw)(const struct bContext *, struct wmGizmo *);
typedef void    (*wmGizmoFnDrawSelect)(const struct bContext *, struct wmGizmo *, int);
typedef int     (*wmGizmoFnTestSelect)(struct bContext *, struct wmGizmo *, const struct wmEvent *);
typedef int     (*wmGizmoFnModal)(struct bContext *, struct wmGizmo *, const struct wmEvent *, eWM_GizmoFlagTweak);
typedef void    (*wmGizmoFnPropertyUpdate)(struct wmGizmo *, struct wmGizmoProperty *);
typedef void    (*wmGizmoFnMatrixBasisGet)(const struct wmGizmo *, float[4][4]);
typedef int     (*wmGizmoFnInvoke)(struct bContext *, struct wmGizmo *, const struct wmEvent *);
typedef void    (*wmGizmoFnExit)(struct bContext *, struct wmGizmo *, const bool);
typedef int     (*wmGizmoFnCursorGet)(struct wmGizmo *);
typedef void    (*wmGizmoFnSelectRefresh)(struct wmGizmo *);
typedef void    (*wmGizmoFnFree)(struct wmGizmo *);

/* wmGizmoProperty ('value' type defined by 'wmGizmoProperty.data_type') */
typedef void (*wmGizmoPropertyFnGet)(
        const struct wmGizmo *, struct wmGizmoProperty *,
        /* typically 'float *' */
        void *value);
typedef void (*wmGizmoPropertyFnSet)(
        const struct wmGizmo *, struct wmGizmoProperty *,
        /* typically 'const float *' */
        const void *value);
typedef void (*wmGizmoPropertyFnRangeGet)(
        const struct wmGizmo *, struct wmGizmoProperty *,
        /* typically 'float[2]' */
        void *range);
typedef void (*wmGizmoPropertyFnFree)(
        const struct wmGizmo *, struct wmGizmoProperty *);

typedef struct wmGizmoPropertyFnParams {
	wmGizmoPropertyFnGet value_get_fn;
	wmGizmoPropertyFnSet value_set_fn;
	wmGizmoPropertyFnRangeGet range_get_fn;
	wmGizmoPropertyFnFree free_fn;
	void *user_data;
} wmGizmoPropertyFnParams;

#endif  /* __WM_GIZMO_FN_H__ */
