/* SPDX-FileCopyrightText: 2016 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup draw
 */

#pragma once

#include "BLI_map.hh"
#include "BLI_set.hh"
#include "DNA_ID.h"

#include "BLI_array.hh"
#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_sys_types.h" /* for bool and uint */

struct ARegion;
struct Base;
struct Depsgraph;
struct Object;
struct RegionView3D;
struct View3D;
struct rcti;

/**
 * Indices inside the selection frame-buffer associated with the elements of a mesh.
 */
struct ElemIndexRanges {
  /** Range for each element type. */
  blender::IndexRange face;
  blender::IndexRange edge;
  blender::IndexRange vert;
  /** Combined range for the whole object. */
  blender::IndexRange total;
};

struct SELECTID_Context {
  /** All selectable evaluated objects. */
  blender::Vector<Object *> objects;
  /** Map of the selectable objects from `objects` to their indices ranges. */
  blender::Map<Object *, ElemIndexRanges> elem_ranges;

  /**
   * Maximum index value that can be contained inside the selection frame-buffer.
   * Each object/element type has different range which are described inside `elem_ranges`.
   */
  uint max_index_drawn_len;

  short select_mode;

  /** To check for updates. */
  blender::float4x4 persmat;
  uint64_t depsgraph_last_update = 0;

  bool is_dirty(Depsgraph *depsgraph, RegionView3D *rv3d);
};

/* `draw_select_buffer.cc` */

bool DRW_select_buffer_elem_get(uint sel_id, uint &r_elem, uint &r_base_index, char &r_elem_type);

/**
 * Assume it is called right after `DRW_select_buffer_bitmap_from_*` so that the same evaluated
 * object is used for drawing and as parameter to this function.
 */
uint DRW_select_buffer_context_offset_for_object_elem(Depsgraph *depsgraph,
                                                      Object *object,
                                                      char elem_type);
/**
 * Main function to read a block of pixels from the select frame buffer.
 */
uint *DRW_select_buffer_read(
    Depsgraph *depsgraph, ARegion *region, View3D *v3d, const rcti *rect, uint *r_buf_len);
/**
 * \param rect: The rectangle to sample indices from (min/max inclusive).
 * \returns a #BLI_bitmap the length of \a bitmap_len or NULL on failure.
 */
uint *DRW_select_buffer_bitmap_from_rect(
    Depsgraph *depsgraph, ARegion *region, View3D *v3d, const rcti *rect, uint *r_bitmap_len);
/**
 * \param center: Circle center.
 * \param radius: Circle radius.
 * \param r_bitmap_len: Number of indices in the selection id buffer.
 * \returns a #BLI_bitmap the length of \a r_bitmap_len or NULL on failure.
 */
uint *DRW_select_buffer_bitmap_from_circle(Depsgraph *depsgraph,
                                           ARegion *region,
                                           View3D *v3d,
                                           const int center[2],
                                           int radius,
                                           uint *r_bitmap_len);
/**
 * \param poly: The polygon coordinates.
 * \param face_len: Length of the polygon.
 * \param rect: Polygon boundaries.
 * \returns a #BLI_bitmap.
 */
uint *DRW_select_buffer_bitmap_from_poly(Depsgraph *depsgraph,
                                         ARegion *region,
                                         View3D *v3d,
                                         blender::Span<blender::int2> poly,
                                         const rcti *rect,
                                         uint *r_bitmap_len);
/**
 * Samples a single pixel.
 */
uint DRW_select_buffer_sample_point(Depsgraph *depsgraph,
                                    ARegion *region,
                                    View3D *v3d,
                                    const int center[2]);
/**
 * Find the selection id closest to \a center.
 * \param dist: Use to initialize the distance,
 * when found, this value is set to the distance of the selection that's returned.
 */
uint DRW_select_buffer_find_nearest_to_point(Depsgraph *depsgraph,
                                             ARegion *region,
                                             View3D *v3d,
                                             const int center[2],
                                             uint id_min,
                                             uint id_max,
                                             uint *dist);
void DRW_select_buffer_context_create(Depsgraph *depsgraph,
                                      blender::Span<Base *> bases,
                                      short select_mode);
