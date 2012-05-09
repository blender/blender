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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 *
 * The Original Code is: all of this file.
 *
 * Contributor(s): none yet.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

#ifndef __BLI_LASSO_H__
#define __BLI_LASSO_H__

/** \file BLI_lasso.h
 *  \ingroup bli
 */

struct rcti;

void BLI_lasso_boundbox(struct rcti *rect, int mcords[][2], short moves);
int  BLI_lasso_is_point_inside(int mcords[][2], short moves, int sx, int sy, const int error_value);
int  BLI_lasso_is_edge_inside(int mcords[][2], short moves, int x0, int y0, int x1, int y1, const int error_value);

#endif
