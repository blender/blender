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

#ifndef __DRW_SELECT_BUFFER_H__
#define __DRW_SELECT_BUFFER_H__

#include "BLI_sys_types.h" /* for bool and uint */

struct ARegion;
struct Base;
struct Depsgraph;
struct Object;
struct View3D;
struct ViewLayer;
struct rcti;

/* select_buffer.c */
void DRW_select_buffer_context_create(struct Base **bases,
                                      const uint bases_len,
                                      short select_mode);
bool DRW_select_buffer_elem_get(const uint sel_id,
                                uint *r_elem,
                                uint *r_base_index,
                                char *r_elem_type);
uint DRW_select_buffer_context_offset_for_object_elem(const uint base_index, char elem_type);
uint *DRW_select_buffer_read(const struct rcti *rect, uint *r_buf_len);
void DRW_draw_select_id_object(struct Depsgraph *depsgraph,
                               struct ViewLayer *view_layer,
                               struct ARegion *ar,
                               struct View3D *v3d,
                               struct Object *ob,
                               short select_mode);
uint *DRW_select_buffer_bitmap_from_rect(const struct rcti *rect, uint *r_bitmap_len);
uint *DRW_select_buffer_bitmap_from_circle(const int center[2],
                                           const int radius,
                                           uint *r_bitmap_len);
uint *DRW_select_buffer_bitmap_from_poly(const int poly[][2],
                                         const int poly_len,
                                         const struct rcti *rect);
uint DRW_select_buffer_sample_point(const int center[2]);
uint DRW_select_buffer_find_nearest_to_point(const int center[2],
                                             const uint id_min,
                                             const uint id_max,
                                             uint *dist);

#endif /* __DRW_SELECT_BUFFER_H__ */
