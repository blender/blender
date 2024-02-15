/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#pragma once

struct TransInfo;

void constraintNumInput(TransInfo *t, float vec[3]);
/**
 * Snap to the nearest point on the axis to the edge/line element.
 */
void transform_constraint_snap_axis_to_edge(const TransInfo *t,
                                            const float axis[3],
                                            float r_out[3]);
/**
 * Snap to the intersection of the axis and the plane defined by the face.
 */
void transform_constraint_snap_axis_to_face(const TransInfo *t,
                                            const float axis[3],
                                            float r_out[3]);
void setConstraint(TransInfo *t, int mode, const char text[]);
/**
 * Applies individual `td->axismtx` constraints.
 */
void setAxisMatrixConstraint(TransInfo *t, int mode, const char text[]);
void setLocalConstraint(TransInfo *t, int mode, const char text[]);
/**
 * Set the constraint according to the user defined orientation
 *
 * `ftext` is a format string passed to #BLI_snprintf. It will add the name of
 * the orientation where %s is (logically).
 */
void setUserConstraint(TransInfo *t, int mode, const char text_[]);
void drawConstraint(TransInfo *t);
/**
 * Called from drawview.c, as an extra per-window draw option.
 */
void drawPropCircle(TransInfo *t);
/**
 * Draws two lines to indicate a proportional editing range that is only defined in one axis.
 */
void drawPropRange(TransInfo *t);
void startConstraint(TransInfo *t);
void stopConstraint(TransInfo *t);
void initSelectConstraint(TransInfo *t);
void selectConstraint(TransInfo *t);
void postSelectConstraint(TransInfo *t);
void setNearestAxis(TransInfo *t);
int constraintModeToIndex(const TransInfo *t);
char constraintModeToChar(const TransInfo *t);
bool isLockConstraint(const TransInfo *t);
/**
 * Returns the dimension of the constraint space.
 *
 * For that reason, the flags always needs to be set to properly evaluate here,
 * even if they aren't actually used in the callback function.
 * (Which could happen for weird constraints not yet designed. Along a path for example.)
 */
int getConstraintSpaceDimension(const TransInfo *t);
