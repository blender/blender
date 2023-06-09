/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup bmesh
 *
 * BMesh Walker API.
 *
 * NOTE(@joeedh): Details on design.
 *
 * Original design: walkers directly emulation recursive functions.
 * functions save their state onto a #BMWalker.worklist, and also add new states
 * to implement recursive or looping behavior.
 * Generally only one state push per call with a specific state is desired.
 *
 * basic design pattern: the walker step function goes through its
 * list of possible choices for recursion, and recurses (by pushing a new state)
 * using the first non-visited one.  This choice is the flagged as visited using the #GHash.
 * Each step may push multiple new states onto the #BMWalker.worklist at once.
 *
 * - Walkers use tool flags, not header flags.
 * - Walkers now use #GHash for storing visited elements,
 *   rather than stealing flags. #GHash can be rewritten
 *   to be faster if necessary, in the far future :) .
 * - tools should ALWAYS have necessary error handling
 *   for if walkers fail.
 */

#include <stdlib.h>
#include <string.h> /* for memcpy */

#include "BLI_listbase.h"
#include "BLI_utildefines.h"

#include "bmesh.h"

#include "bmesh_walkers_private.h"

void *BMW_begin(BMWalker *walker, void *start)
{
  BLI_assert(((BMHeader *)start)->htype & walker->begin_htype);

  walker->begin(walker, start);

  return BMW_current_state(walker) ? walker->step(walker) : NULL;
}

void BMW_init(BMWalker *walker,
              BMesh *bm,
              int type,
              short mask_vert,
              short mask_edge,
              short mask_face,
              BMWFlag flag,
              int layer)
{
  memset(walker, 0, sizeof(BMWalker));

  walker->layer = layer;
  walker->flag = flag;
  walker->bm = bm;

  walker->mask_vert = mask_vert;
  walker->mask_edge = mask_edge;
  walker->mask_face = mask_face;

  walker->visit_set = BLI_gset_ptr_new("bmesh walkers");
  walker->visit_set_alt = BLI_gset_ptr_new("bmesh walkers sec");

  if (UNLIKELY(type >= BMW_MAXWALKERS || type < 0)) {
    fprintf(stderr,
            "%s: Invalid walker type in BMW_init; type: %d, "
            "searchmask: (v:%d, e:%d, f:%d), flag: %u, layer: %d\n",
            __func__,
            type,
            mask_vert,
            mask_edge,
            mask_face,
            flag,
            layer);
    BLI_assert(0);
    return;
  }

  if (type != BMW_CUSTOM) {
    walker->begin_htype = bm_walker_types[type]->begin_htype;
    walker->begin = bm_walker_types[type]->begin;
    walker->yield = bm_walker_types[type]->yield;
    walker->step = bm_walker_types[type]->step;
    walker->structsize = bm_walker_types[type]->structsize;
    walker->order = bm_walker_types[type]->order;
    walker->valid_mask = bm_walker_types[type]->valid_mask;

    /* safety checks */
    /* if this raises an error either the caller is wrong or
     * 'bm_walker_types' needs updating */
    BLI_assert(mask_vert == 0 || (walker->valid_mask & BM_VERT));
    BLI_assert(mask_edge == 0 || (walker->valid_mask & BM_EDGE));
    BLI_assert(mask_face == 0 || (walker->valid_mask & BM_FACE));
  }

  walker->worklist = BLI_mempool_create(walker->structsize, 0, 128, BLI_MEMPOOL_NOP);
  BLI_listbase_clear(&walker->states);
}

void BMW_end(BMWalker *walker)
{
  BLI_mempool_destroy(walker->worklist);
  BLI_gset_free(walker->visit_set, NULL);
  BLI_gset_free(walker->visit_set_alt, NULL);
}

void *BMW_step(BMWalker *walker)
{
  BMHeader *head;

  head = BMW_walk(walker);

  return head;
}

int BMW_current_depth(BMWalker *walker)
{
  return walker->depth;
}

void *BMW_walk(BMWalker *walker)
{
  void *current = NULL;

  while (BMW_current_state(walker)) {
    current = walker->step(walker);
    if (current) {
      return current;
    }
  }
  return NULL;
}

void *BMW_current_state(BMWalker *walker)
{
  BMwGenericWalker *currentstate = walker->states.first;
  if (currentstate) {
    /* Automatic update of depth. For most walkers that
     * follow the standard "Step" pattern of:
     * - read current state
     * - remove current state
     * - push new states
     * - return walk result from just-removed current state
     * this simple automatic update should keep track of depth
     * just fine. Walkers that deviate from that pattern may
     * need to manually update the depth if they care about
     * keeping it correct. */
    walker->depth = currentstate->depth + 1;
  }
  return currentstate;
}

void BMW_state_remove(BMWalker *walker)
{
  void *oldstate;
  oldstate = BMW_current_state(walker);
  BLI_remlink(&walker->states, oldstate);
  BLI_mempool_free(walker->worklist, oldstate);
}

void *BMW_state_add(BMWalker *walker)
{
  BMwGenericWalker *newstate;
  newstate = BLI_mempool_alloc(walker->worklist);
  newstate->depth = walker->depth;
  switch (walker->order) {
    case BMW_DEPTH_FIRST:
      BLI_addhead(&walker->states, newstate);
      break;
    case BMW_BREADTH_FIRST:
      BLI_addtail(&walker->states, newstate);
      break;
    default:
      BLI_assert(0);
      break;
  }
  return newstate;
}

void BMW_reset(BMWalker *walker)
{
  while (BMW_current_state(walker)) {
    BMW_state_remove(walker);
  }
  walker->depth = 0;
  BLI_gset_clear(walker->visit_set, NULL);
  BLI_gset_clear(walker->visit_set_alt, NULL);
}
