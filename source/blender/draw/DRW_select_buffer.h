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
struct rcti;

typedef struct SELECTID_ObjectData {
  DrawData dd;

  uint drawn_index;
  bool is_drawn;
} SELECTID_ObjectData;

struct ObjectOffsets {
  /* For convenience only. */
  union {
    uint offset;
    uint face_start;
  };
  union {
    uint face;
    uint edge_start;
  };
  union {
    uint edge;
    uint vert_start;
  };
  uint vert;
};

typedef struct SELECTID_Context {
  /* All context objects */
  struct Object **objects;

  /* Array with only drawn objects. When a new object is found within the rect,
   * it is added to the end of the list.
   * The list is reset to any viewport or context update. */
  struct Object **objects_drawn;
  struct ObjectOffsets *index_offsets;
  uint objects_len;
  uint objects_drawn_len;

  /** Total number of element indices `index_offsets[object_drawn_len - 1].vert`. */
  uint index_drawn_len;

  short select_mode;

  /* rect is used to check which objects whose indexes need to be drawn. */
  rcti last_rect;

  /* To check for updates. */
  float persmat[4][4];
  bool is_dirty;
} SELECTID_Context;

/* draw_select_buffer.c */
bool DRW_select_buffer_elem_get(const uint sel_id,
                                uint *r_elem,
                                uint *r_base_index,
                                char *r_elem_type);
uint DRW_select_buffer_context_offset_for_object_elem(struct Depsgraph *depsgraph,
                                                      struct Object *object,
                                                      char elem_type);
uint *DRW_select_buffer_read(struct Depsgraph *depsgraph,
                             struct ARegion *region,
                             struct View3D *v3d,
                             const rcti *rect,
                             uint *r_buf_len);
uint *DRW_select_buffer_bitmap_from_rect(struct Depsgraph *depsgraph,
                                         struct ARegion *region,
                                         struct View3D *v3d,
                                         const struct rcti *rect,
                                         uint *r_bitmap_len);
uint *DRW_select_buffer_bitmap_from_circle(struct Depsgraph *depsgraph,
                                           struct ARegion *region,
                                           struct View3D *v3d,
                                           const int center[2],
                                           const int radius,
                                           uint *r_bitmap_len);
uint *DRW_select_buffer_bitmap_from_poly(struct Depsgraph *depsgraph,
                                         struct ARegion *region,
                                         struct View3D *v3d,
                                         const int poly[][2],
                                         const int poly_len,
                                         const struct rcti *rect,
                                         uint *r_bitmap_len);
uint DRW_select_buffer_sample_point(struct Depsgraph *depsgraph,
                                    struct ARegion *region,
                                    struct View3D *v3d,
                                    const int center[2]);
uint DRW_select_buffer_find_nearest_to_point(struct Depsgraph *depsgraph,
                                             struct ARegion *region,
                                             struct View3D *v3d,
                                             const int center[2],
                                             const uint id_min,
                                             const uint id_max,
                                             uint *dist);
void DRW_select_buffer_context_create(struct Base **bases,
                                      const uint bases_len,
                                      short select_mode);

#endif /* __DRW_SELECT_BUFFER_H__ */
