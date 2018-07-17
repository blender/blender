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
 * The Original Code is Copyright (C) 2016 by Mike Erwin.
 * All rights reserved.
 *
 * Contributor(s): Blender Foundation
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/gpu/gwn_imm_util.h
 *  \ingroup gpu
 *
 * Gawain element list (AKA index buffer)
 */

#ifndef __GWN_IMM_UTIL_H__
#define __GWN_IMM_UTIL_H__

#include <stdlib.h>

/* Draw 2D rectangles (replaces glRect functions) */
/* caller is reponsible for vertex format & shader */
void immRectf(uint pos, float x1, float y1, float x2, float y2);
void immRecti(uint pos, int x1, int y1, int x2, int y2);

/* Same as immRectf/immRecti but does not call immBegin/immEnd. To use with GWN_PRIM_TRIS. */
void immRectf_fast_with_color(uint pos, uint col, float x1, float y1, float x2, float y2, const float color[4]);
void immRecti_fast_with_color(uint pos, uint col, int x1, int y1, int x2, int y2, const float color[4]);

#endif /* __GWN_IMM_UTIL_H__ */
