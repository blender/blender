/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2019 Blender Foundation. All rights reserved. */

/** \file
 * \ingroup editors
 */

#pragma once

#include "BLI_linklist.h"
#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_threads.h"

#include "DNA_lineart_types.h"

#include <math.h>
#include <string.h>

struct LineartEdge;
struct LineartRenderBuffer;
struct LineartStaticMemPool;
struct LineartStaticMemPoolNode;

void *lineart_list_append_pointer_pool(ListBase *h, struct LineartStaticMemPool *smp, void *data);
void *lineart_list_append_pointer_pool_sized(ListBase *h,
                                             struct LineartStaticMemPool *smp,
                                             void *data,
                                             int size);
void *lineart_list_append_pointer_pool_thread(ListBase *h,
                                              struct LineartStaticMemPool *smp,
                                              void *data);
void *lineart_list_append_pointer_pool_sized_thread(ListBase *h,
                                                    LineartStaticMemPool *smp,
                                                    void *data,
                                                    int size);
void *list_push_pointer_static(ListBase *h, struct LineartStaticMemPool *smp, void *p);
void *list_push_pointer_static_sized(ListBase *h,
                                     struct LineartStaticMemPool *smp,
                                     void *p,
                                     int size);

void *lineart_list_pop_pointer_no_free(ListBase *h);
void lineart_list_remove_pointer_item_no_free(ListBase *h, LinkData *lip);

struct LineartStaticMemPoolNode *lineart_mem_new_static_pool(struct LineartStaticMemPool *smp,
                                                             size_t size);
void *lineart_mem_acquire(struct LineartStaticMemPool *smp, size_t size);
void *lineart_mem_acquire_thread(struct LineartStaticMemPool *smp, size_t size);
void lineart_mem_destroy(struct LineartStaticMemPool *smp);

void lineart_prepend_edge_direct(void **list_head, void *node);
void lineart_prepend_pool(LinkNode **first, struct LineartStaticMemPool *smp, void *link);

void lineart_matrix_ortho_44d(double (*mProjection)[4],
                              double xMin,
                              double xMax,
                              double yMin,
                              double yMax,
                              double zMin,
                              double zMax);
void lineart_matrix_perspective_44d(
    double (*mProjection)[4], double fFov_rad, double fAspect, double zMin, double zMax);

int lineart_count_intersection_segment_count(struct LineartRenderBuffer *rb);

void lineart_count_and_print_render_buffer_memory(struct LineartRenderBuffer *rb);

#define LRT_ITER_ALL_LINES_BEGIN \
  LineartEdge *e, *next_e; \
  void **current_head; \
  e = rb->contour.first; \
  if (!e) { \
    e = rb->crease.first; \
  } \
  if (!e) { \
    e = rb->material.first; \
  } \
  if (!e) { \
    e = rb->edge_mark.first; \
  } \
  if (!e) { \
    e = rb->intersection.first; \
  } \
  if (!e) { \
    e = rb->floating.first; \
  } \
  for (current_head = &rb->contour.first; e; e = next_e) { \
    next_e = e->next;

#define LRT_ITER_ALL_LINES_NEXT \
  while (!next_e) { \
    if (current_head == &rb->contour.first) { \
      current_head = &rb->crease.first; \
    } \
    else if (current_head == &rb->crease.first) { \
      current_head = &rb->material.first; \
    } \
    else if (current_head == &rb->material.first) { \
      current_head = &rb->edge_mark.first; \
    } \
    else if (current_head == &rb->edge_mark.first) { \
      current_head = &rb->intersection.first; \
    } \
    else if (current_head == &rb->intersection.first) { \
      current_head = &rb->floating.first; \
    } \
    else { \
      break; \
    } \
    next_e = *current_head; \
  }

#define LRT_ITER_ALL_LINES_END \
  LRT_ITER_ALL_LINES_NEXT \
  }

#define LRT_BOUND_AREA_CROSSES(b1, b2) \
  ((b1)[0] < (b2)[1] && (b1)[1] > (b2)[0] && (b1)[3] < (b2)[2] && (b1)[2] > (b2)[3])

/* Initial bounding area row/column count, setting 4 is the simplest way algorithm could function
 * efficiently. */
#define LRT_BA_ROWS 4

#ifdef __cplusplus
extern "C" {
#endif

void lineart_sort_adjacent_items(LineartAdjacentEdge *ai, int length);

#ifdef __cplusplus
}
#endif
