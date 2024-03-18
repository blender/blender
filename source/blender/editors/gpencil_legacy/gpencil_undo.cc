/* SPDX-FileCopyrightText: 2011 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edgpencil
 */

#include <cstdlib>
#include <cstring>

#include "MEM_guardedalloc.h"

#include "DNA_gpencil_legacy_types.h"
#include "DNA_listBase.h"
#include "DNA_object_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_listbase.h"

#include "BKE_blender_undo.hh"
#include "BKE_context.hh"
#include "BKE_gpencil_legacy.h"
#include "BKE_undo_system.hh"

#include "ED_gpencil_legacy.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "DEG_depsgraph.hh"

#include "gpencil_intern.h"

struct bGPundonode {
  bGPundonode *next, *prev;

  char name[BKE_UNDO_STR_MAX];
  bGPdata *gpd;
};

static ListBase undo_nodes = {nullptr, nullptr};
static bGPundonode *cur_node = nullptr;

int ED_gpencil_session_active()
{
  return (BLI_listbase_is_empty(&undo_nodes) == false);
}

int ED_undo_gpencil_step(bContext *C, const int step)
{
  bGPdata **gpd_ptr = nullptr, *new_gpd = nullptr;

  gpd_ptr = ED_gpencil_data_get_pointers(C, nullptr);

  const eUndoStepDir undo_step = (eUndoStepDir)step;
  if (undo_step == STEP_UNDO) {
    if (cur_node->prev) {
      cur_node = cur_node->prev;
      new_gpd = cur_node->gpd;
    }
  }
  else if (undo_step == STEP_REDO) {
    if (cur_node->next) {
      cur_node = cur_node->next;
      new_gpd = cur_node->gpd;
    }
  }

  if (new_gpd) {
    if (gpd_ptr) {
      if (*gpd_ptr) {
        bGPdata *gpd = *gpd_ptr;
        bGPDlayer *gpld;

        BKE_gpencil_free_layers(&gpd->layers);

        /* copy layers */
        BLI_listbase_clear(&gpd->layers);

        LISTBASE_FOREACH (bGPDlayer *, gpl, &gpd->layers) {
          /* make a copy of source layer and its data */
          gpld = BKE_gpencil_layer_duplicate(gpl, true, true);
          BLI_addtail(&gpd->layers, gpld);
        }
      }
    }
    /* drawing batch cache is dirty now */
    DEG_id_tag_update(&new_gpd->id, ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY);
    new_gpd->flag |= GP_DATA_CACHE_IS_DIRTY;
  }

  WM_event_add_notifier(C, NC_GPENCIL | NA_EDITED, nullptr);

  return OPERATOR_FINISHED;
}

void gpencil_undo_init(bGPdata *gpd)
{
  gpencil_undo_push(gpd);
}

static void gpencil_undo_free_node(bGPundonode *undo_node)
{
  /* HACK: animdata wasn't duplicated, so it shouldn't be freed here,
   * or else the real copy will segfault when accessed
   */
  undo_node->gpd->adt = nullptr;

  BKE_gpencil_free_data(undo_node->gpd, false);
  MEM_freeN(undo_node->gpd);
}

void gpencil_undo_push(bGPdata *gpd)
{
  bGPundonode *undo_node;

  if (cur_node) {
    /* Remove all undone nodes from stack. */
    undo_node = cur_node->next;

    while (undo_node) {
      bGPundonode *next_node = undo_node->next;

      gpencil_undo_free_node(undo_node);
      BLI_freelinkN(&undo_nodes, undo_node);

      undo_node = next_node;
    }
  }

  /* limit number of undo steps to the maximum undo steps
   * - to prevent running out of memory during **really**
   *   long drawing sessions (triggering swapping)
   */
  /* TODO: Undo-memory constraint is not respected yet,
   * but can be added if we have any need for it. */
  if (U.undosteps && !BLI_listbase_is_empty(&undo_nodes)) {
    /* remove anything older than n-steps before cur_node */
    int steps = 0;

    undo_node = (cur_node) ? cur_node : static_cast<bGPundonode *>(undo_nodes.last);
    while (undo_node) {
      bGPundonode *prev_node = undo_node->prev;

      if (steps >= U.undosteps) {
        gpencil_undo_free_node(undo_node);
        BLI_freelinkN(&undo_nodes, undo_node);
      }

      steps++;
      undo_node = prev_node;
    }
  }

  /* create new undo node */
  undo_node = MEM_cnew<bGPundonode>("gpencil undo node");
  undo_node->gpd = BKE_gpencil_data_duplicate(nullptr, gpd, true);

  cur_node = undo_node;

  BLI_addtail(&undo_nodes, undo_node);
}

void gpencil_undo_finish()
{
  bGPundonode *undo_node = static_cast<bGPundonode *>(undo_nodes.first);

  while (undo_node) {
    gpencil_undo_free_node(undo_node);
    undo_node = undo_node->next;
  }

  BLI_freelistN(&undo_nodes);

  cur_node = nullptr;
}
