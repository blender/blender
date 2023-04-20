/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

/** \file
 * \ingroup edtransform
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------- */
/** \name Types/
 * \{ */

struct ARegion;
struct bContext;
struct bScreen;
struct RegionView3D;
struct Scene;
struct ScrArea;
struct TransformBounds;
struct TransInfo;
struct wmGizmoGroup;
struct wmGizmoGroupType;
struct wmMsgBus;

/** \} */

/* -------------------------------------------------------------------- */
/** \name Gizmo
 * \{ */

/* transform_gizmo_3d.c */

#define GIZMO_AXIS_LINE_WIDTH 2.0f

void gizmo_prepare_mat(const struct bContext *C,
                       struct RegionView3D *rv3d,
                       const struct TransformBounds *tbounds);
void gizmo_xform_message_subscribe(struct wmGizmoGroup *gzgroup,
                                   struct wmMsgBus *mbus,
                                   struct Scene *scene,
                                   struct bScreen *screen,
                                   struct ScrArea *area,
                                   struct ARegion *region,
                                   void (*type_fn)(struct wmGizmoGroupType *));

/**
 * Set the #T_NO_GIZMO flag.
 *
 * \note This maintains the conventional behavior of not displaying the gizmo if the operator has
 * been triggered by shortcuts.
 */
void transform_gizmo_3d_model_from_constraint_and_mode_init(TransInfo *t);

/**
 * Change the gizmo and its orientation to match the transform state.
 *
 * \note This used while the modal operator is running so changes to the constraint or mode show
 * the gizmo associated with that state, as if it had been the initial gizmo dragged.
 */
void transform_gizmo_3d_model_from_constraint_and_mode_set(TransInfo *t);

/**
 * Restores the non-modal state of the gizmo.
 */
void transform_gizmo_3d_model_from_constraint_and_mode_restore(TransInfo *t);

/** \} */

#ifdef __cplusplus
}
#endif
