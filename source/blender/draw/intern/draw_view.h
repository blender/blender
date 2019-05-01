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
 * Copyright 2016, Blender Foundation.
 */

/** \file
 * \ingroup draw
 */

#ifndef __DRAW_VIEW_H__
#define __DRAW_VIEW_H__

void DRW_draw_region_info(void);
void DRW_draw_background(bool do_alpha_checker);
void DRW_draw_cursor(void);
void DRW_draw_gizmo_3d(void);
void DRW_draw_gizmo_2d(void);

struct GPUBatch *DRW_draw_background_clipping_batch_from_rv3d(const struct RegionView3D *rv3d);

#endif /* __DRAW_VIEW_H__ */
