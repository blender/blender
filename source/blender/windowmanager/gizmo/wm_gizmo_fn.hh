/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup wm
 *
 * Callback function definitions, needed for both Types & API headers.
 */

#pragma once

#include "BLI_compiler_attrs.h"

struct wmKeyMap;
struct wmMsgBus;

/* wmGizmoGroup */
using wmGizmoGroupFnPoll = bool (*)(const bContext *, wmGizmoGroupType *);
using wmGizmoGroupFnInit = void (*)(const bContext *, wmGizmoGroup *);
using wmGizmoGroupFnRefresh = void (*)(const bContext *, wmGizmoGroup *);
using wmGizmoGroupFnDrawPrepare = void (*)(const bContext *, wmGizmoGroup *);
using wmGizmoGroupFnInvokePrepare = void (*)(const bContext *,
                                             wmGizmoGroup *,
                                             wmGizmo *,
                                             const wmEvent *);
using wmGizmoGroupFnSetupKeymap = wmKeyMap *(*)(const wmGizmoGroupType *, wmKeyConfig *);
using wmGizmoGroupFnMsgBusSubscribe = void (*)(const bContext *, wmGizmoGroup *, wmMsgBus *);

/* wmGizmo */
/* See: wmGizmoType for docs on each type. */

using wmGizmoFnSetup = void (*)(wmGizmo *);
using wmGizmoFnDraw = void (*)(const bContext *, wmGizmo *);
using wmGizmoFnDrawSelect = void (*)(const bContext *, wmGizmo *, int);
using wmGizmoFnTestSelect = int (*)(bContext *, wmGizmo *, const int mval[2]);
using wmGizmoFnModal = int (*)(bContext *, wmGizmo *, const wmEvent *, eWM_GizmoFlagTweak);
using wmGizmoFnPropertyUpdate = void (*)(wmGizmo *, wmGizmoProperty *);
using wmGizmoFnMatrixBasisGet = void (*)(const wmGizmo *, float[4][4]);
using wmGizmoFnInvoke = int (*)(bContext *, wmGizmo *, const wmEvent *);
using wmGizmoFnExit = void (*)(bContext *, wmGizmo *, const bool);
using wmGizmoFnCursorGet = int (*)(wmGizmo *);
using wmGizmoFnScreenBoundsGet = bool (*)(bContext *, wmGizmo *, rcti *r_bounding_box);
using wmGizmoFnSelectRefresh = void (*)(wmGizmo *);
using wmGizmoFnFree = void (*)(wmGizmo *);

/* wmGizmoProperty ('value' type defined by 'wmGizmoProperty.data_type') */
using wmGizmoPropertyFnGet = void (*)(const wmGizmo *,
                                      wmGizmoProperty *,
                                      /* typically 'float *' */
                                      void *value);
using wmGizmoPropertyFnSet = void (*)(const wmGizmo *,
                                      wmGizmoProperty *,
                                      /* typically 'const float *' */
                                      const void *value);
using wmGizmoPropertyFnRangeGet = void (*)(const wmGizmo *,
                                           wmGizmoProperty *,
                                           /* typically 'float[2]' */
                                           void *range);
using wmGizmoPropertyFnFree = void (*)(const wmGizmo *, wmGizmoProperty *);

struct wmGizmoPropertyFnParams {
  wmGizmoPropertyFnGet value_get_fn;
  wmGizmoPropertyFnSet value_set_fn;
  wmGizmoPropertyFnRangeGet range_get_fn;
  wmGizmoPropertyFnFree free_fn;
  void *user_data;
};
