/* SPDX-FileCopyrightText: 2016 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_sys_types.h" /* for bool and uint */

#ifdef __cplusplus
extern "C" {
#endif

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

bool DRW_select_buffer_elem_get(uint sel_id, uint *r_elem, uint *r_base_index, char *r_elem_type);
uint DRW_select_buffer_context_offset_for_object_elem(struct Depsgraph *depsgraph,
                                                      struct Object *object,
                                                      char elem_type);
/**
 * Main function to read a block of pixels from the select frame buffer.
 */
uint *DRW_select_buffer_read(struct Depsgraph *depsgraph,
                             struct ARegion *region,
                             struct View3D *v3d,
                             const rcti *rect,
                             uint *r_buf_len);
/**
 * \param rect: The rectangle to sample indices from (min/max inclusive).
 * \returns a #BLI_bitmap the length of \a bitmap_len or NULL on failure.
 */
uint *DRW_select_buffer_bitmap_from_rect(struct Depsgraph *depsgraph,
                                         struct ARegion *region,
                                         struct View3D *v3d,
                                         const struct rcti *rect,
                                         uint *r_bitmap_len);
/**
 * \param center: Circle center.
 * \param radius: Circle radius.
 * \param r_bitmap_len: Number of indices in the selection id buffer.
 * \returns a #BLI_bitmap the length of \a r_bitmap_len or NULL on failure.
 */
uint *DRW_select_buffer_bitmap_from_circle(struct Depsgraph *depsgraph,
                                           struct ARegion *region,
                                           struct View3D *v3d,
                                           const int center[2],
                                           int radius,
                                           uint *r_bitmap_len);
/**
 * \param poly: The polygon coordinates.
 * \param face_len: Length of the polygon.
 * \param rect: Polygon boundaries.
 * \returns a #BLI_bitmap.
 */
uint *DRW_select_buffer_bitmap_from_poly(struct Depsgraph *depsgraph,
                                         struct ARegion *region,
                                         struct View3D *v3d,
                                         const int poly[][2],
                                         int face_len,
                                         const struct rcti *rect,
                                         uint *r_bitmap_len);
/**
 * Samples a single pixel.
 */
uint DRW_select_buffer_sample_point(struct Depsgraph *depsgraph,
                                    struct ARegion *region,
                                    struct View3D *v3d,
                                    const int center[2]);
/**
 * Find the selection id closest to \a center.
 * \param dist: Use to initialize the distance,
 * when found, this value is set to the distance of the selection that's returned.
 */
uint DRW_select_buffer_find_nearest_to_point(struct Depsgraph *depsgraph,
                                             struct ARegion *region,
                                             struct View3D *v3d,
                                             const int center[2],
                                             uint id_min,
                                             uint id_max,
                                             uint *dist);
void DRW_select_buffer_context_create(struct Base **bases, uint bases_len, short select_mode);

#ifdef __cplusplus
}
#endif
