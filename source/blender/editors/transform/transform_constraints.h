/*
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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edtransform
 */

#ifndef __TRANSFORM_CONSTRAINTS_H__
#define __TRANSFORM_CONSTRAINTS_H__

struct TransInfo;

void constraintNumInput(TransInfo *t, float vec[3]);
void setConstraint(TransInfo *t, int mode, const char text[]);
void setAxisMatrixConstraint(TransInfo *t, int mode, const char text[]);
void setLocalConstraint(TransInfo *t, int mode, const char text[]);
void setUserConstraint(TransInfo *t, short orientation, int mode, const char text[]);
void drawConstraint(TransInfo *t);
void drawPropCircle(const struct bContext *C, TransInfo *t);
void startConstraint(TransInfo *t);
void stopConstraint(TransInfo *t);
void initSelectConstraint(TransInfo *t);
void selectConstraint(TransInfo *t);
void postSelectConstraint(TransInfo *t);
void setNearestAxis(TransInfo *t);
int constraintModeToIndex(const TransInfo *t);
char constraintModeToChar(const TransInfo *t);
bool isLockConstraint(TransInfo *t);
int getConstraintSpaceDimension(TransInfo *t);

#endif
