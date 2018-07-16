/*
 * Copyright 2018, Blender Foundation.
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
 * Contributor(s): Blender Institute
 *
 */

/** \file draw_debug.h
 *  \ingroup draw
 */

#ifndef __DRAW_DEBUG_H__
#define __DRAW_DEBUG_H__

struct BoundBox;

void DRW_debug_modelmat_reset(void);
void DRW_debug_modelmat(const float modelmat[4][4]);

void DRW_debug_line_v3v3(const float v1[3], const float v2[3], const float color[4]);
void DRW_debug_polygon_v3(const float (*v)[3], const int vert_len, const float color[4]);
void DRW_debug_m4(const float m[4][4]);
void DRW_debug_m4_as_bbox(const float m[4][4], const float color[4], const bool invert);
void DRW_debug_bbox(const BoundBox *bbox, const float color[4]);
void DRW_debug_sphere(const float center[3], const float radius, const float color[4]);

#endif /* __DRAW_DEBUG_H__ */
