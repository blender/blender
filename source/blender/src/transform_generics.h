/**
 * $Id$
 *
 * ***** BEGIN GPL/BL DUAL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version. The Blender
 * Foundation also sells licenses for use in proprietary software under
 * the Blender License.  See http://www.blender.org/BL/ for information
 * about this.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL/BL DUAL LICENSE BLOCK *****
 */

#ifndef TRANSFORM_GENERICS_H
#define TRANSFORM_GENERICS_H

void recalcData(TransInfo *t);

void initTransModeFlags(TransInfo *t, int mode);

void drawLine(float *center, float *dir, char axis);

void postTrans (TransInfo *t);

void apply_grid1(float *val, int max_index, float factor);
void apply_grid2(float *val, int max_index, float factor, float factor2);
void apply_grid3(float *val, int max_index, float fac1, float fac2, float fac3);

void applyTransObjects(TransInfo *t);
void restoreTransObjects(TransInfo *t);

void initTrans(TransInfo *t);

void calculateCenterBound(TransInfo *t);
void calculateCenterMedian(TransInfo *t);
void calculateCenterCursor(TransInfo *t);

void calculateCenter(TransInfo *t);

void calculatePropRatio(TransInfo *t);

void snapGrid(TransInfo *t, float *val);

TransInfo * BIF_GetTransInfo();

#endif

