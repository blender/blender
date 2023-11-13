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

struct wmMsgBus;

#ifdef __cplusplus
extern "C" {
#endif

/* wmGizmoGroup */
typedef bool (*wmGizmoGroupFnPoll)(const struct bContext *,
                                   struct wmGizmoGroupType *) ATTR_WARN_UNUSED_RESULT;
typedef void (*wmGizmoGroupFnInit)(const struct bContext *, struct wmGizmoGroup *);
typedef void (*wmGizmoGroupFnRefresh)(const struct bContext *, struct wmGizmoGroup *);
typedef void (*wmGizmoGroupFnDrawPrepare)(const struct bContext *, struct wmGizmoGroup *);
typedef void (*wmGizmoGroupFnInvokePrepare)(const struct bContext *,
                                            struct wmGizmoGroup *,
                                            struct wmGizmo *,
                                            const struct wmEvent *);
typedef struct wmKeyMap *(*wmGizmoGroupFnSetupKeymap)(const struct wmGizmoGroupType *,
                                                      struct wmKeyConfig *)ATTR_WARN_UNUSED_RESULT;
typedef void (*wmGizmoGroupFnMsgBusSubscribe)(const struct bContext *,
                                              struct wmGizmoGroup *,
                                              struct wmMsgBus *);

/* wmGizmo */
/* See: wmGizmoType for docs on each type. */

typedef void (*wmGizmoFnSetup)(struct wmGizmo *);
typedef void (*wmGizmoFnDraw)(const struct bContext *, struct wmGizmo *);
typedef void (*wmGizmoFnDrawSelect)(const struct bContext *, struct wmGizmo *, int);
typedef int (*wmGizmoFnTestSelect)(struct bContext *, struct wmGizmo *, const int mval[2]);
typedef int (*wmGizmoFnModal)(struct bContext *,
                              struct wmGizmo *,
                              const struct wmEvent *,
                              eWM_GizmoFlagTweak);
typedef void (*wmGizmoFnPropertyUpdate)(struct wmGizmo *, struct wmGizmoProperty *);
typedef void (*wmGizmoFnMatrixBasisGet)(const struct wmGizmo *, float[4][4]);
typedef int (*wmGizmoFnInvoke)(struct bContext *, struct wmGizmo *, const struct wmEvent *);
typedef void (*wmGizmoFnExit)(struct bContext *, struct wmGizmo *, const bool);
typedef int (*wmGizmoFnCursorGet)(struct wmGizmo *);
typedef bool (*wmGizmoFnScreenBoundsGet)(struct bContext *,
                                         struct wmGizmo *,
                                         rcti *r_bounding_box) ATTR_WARN_UNUSED_RESULT;
typedef void (*wmGizmoFnSelectRefresh)(struct wmGizmo *);
typedef void (*wmGizmoFnFree)(struct wmGizmo *);

/* wmGizmoProperty ('value' type defined by 'wmGizmoProperty.data_type') */
typedef void (*wmGizmoPropertyFnGet)(const struct wmGizmo *,
                                     struct wmGizmoProperty *,
                                     /* typically 'float *' */
                                     void *value);
typedef void (*wmGizmoPropertyFnSet)(const struct wmGizmo *,
                                     struct wmGizmoProperty *,
                                     /* typically 'const float *' */
                                     const void *value);
typedef void (*wmGizmoPropertyFnRangeGet)(const struct wmGizmo *,
                                          struct wmGizmoProperty *,
                                          /* typically 'float[2]' */
                                          void *range);
typedef void (*wmGizmoPropertyFnFree)(const struct wmGizmo *, struct wmGizmoProperty *);

typedef struct wmGizmoPropertyFnParams {
  wmGizmoPropertyFnGet value_get_fn;
  wmGizmoPropertyFnSet value_set_fn;
  wmGizmoPropertyFnRangeGet range_get_fn;
  wmGizmoPropertyFnFree free_fn;
  void *user_data;
} wmGizmoPropertyFnParams;

#ifdef __cplusplus
}
#endif
