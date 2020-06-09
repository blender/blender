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
 * The Original Code is Copyright (C) 2001-2002 by NaN Holding BV.
 * All rights reserved.
 */

/** \file
 * \ingroup edtransform
 */

#include "DNA_space_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_math.h"

#include "BKE_context.h"
#include "BKE_node.h"
#include "BKE_report.h"

#include "ED_node.h"

#include "UI_interface.h"

#include "transform.h"
#include "transform_convert.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/** \name Node Transform Creation
 *
 * \{ */

/* transcribe given node into TransData2D for Transforming */
static void NodeToTransData(TransData *td, TransData2D *td2d, bNode *node, const float dpi_fac)
{
  float locx, locy;

  /* account for parents (nested nodes) */
  if (node->parent) {
    nodeToView(node->parent, node->locx, node->locy, &locx, &locy);
  }
  else {
    locx = node->locx;
    locy = node->locy;
  }

  /* use top-left corner as the transform origin for nodes */
  /* weirdo - but the node system is a mix of free 2d elements and dpi sensitive UI */
#ifdef USE_NODE_CENTER
  td2d->loc[0] = (locx * dpi_fac) + (BLI_rctf_size_x(&node->totr) * +0.5f);
  td2d->loc[1] = (locy * dpi_fac) + (BLI_rctf_size_y(&node->totr) * -0.5f);
#else
  td2d->loc[0] = locx * dpi_fac;
  td2d->loc[1] = locy * dpi_fac;
#endif
  td2d->loc[2] = 0.0f;
  td2d->loc2d = td2d->loc; /* current location */

  td->flag = 0;

  td->loc = td2d->loc;
  copy_v3_v3(td->iloc, td->loc);
  /* use node center instead of origin (top-left corner) */
  td->center[0] = td2d->loc[0];
  td->center[1] = td2d->loc[1];
  td->center[2] = 0.0f;

  memset(td->axismtx, 0, sizeof(td->axismtx));
  td->axismtx[2][2] = 1.0f;

  td->ext = NULL;
  td->val = NULL;

  td->flag |= TD_SELECTED;
  td->dist = 0.0;

  unit_m3(td->mtx);
  unit_m3(td->smtx);

  td->extra = node;
}

static bool is_node_parent_select(bNode *node)
{
  while ((node = node->parent)) {
    if (node->flag & NODE_TRANSFORM) {
      return true;
    }
  }
  return false;
}

void createTransNodeData(TransInfo *t)
{
  const float dpi_fac = UI_DPI_FAC;
  TransData *td;
  TransData2D *td2d;
  SpaceNode *snode = t->area->spacedata.first;
  bNode *node;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  tc->data_len = 0;

  if (!snode->edittree) {
    return;
  }

  /* nodes dont support PET and probably never will */
  t->flag &= ~T_PROP_EDIT_ALL;

  /* set transform flags on nodes */
  for (node = snode->edittree->nodes.first; node; node = node->next) {
    if (node->flag & NODE_SELECT && is_node_parent_select(node) == false) {
      node->flag |= NODE_TRANSFORM;
      tc->data_len++;
    }
    else {
      node->flag &= ~NODE_TRANSFORM;
    }
  }

  td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransNode TransData");
  td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D), "TransNode TransData2D");

  for (node = snode->edittree->nodes.first; node; node = node->next) {
    if (node->flag & NODE_TRANSFORM) {
      NodeToTransData(td++, td2d++, node, dpi_fac);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Transform Creation
 *
 * \{ */

void flushTransNodes(TransInfo *t)
{
  const float dpi_fac = UI_DPI_FAC;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    int a;
    TransData *td;
    TransData2D *td2d;

    applyGridAbsolute(t);

    /* flush to 2d vector from internally used 3d vector */
    for (a = 0, td = tc->data, td2d = tc->data_2d; a < tc->data_len; a++, td++, td2d++) {
      bNode *node = td->extra;
      float locx, locy;

      /* weirdo - but the node system is a mix of free 2d elements and dpi sensitive UI */
#ifdef USE_NODE_CENTER
      locx = (td2d->loc[0] - (BLI_rctf_size_x(&node->totr)) * +0.5f) / dpi_fac;
      locy = (td2d->loc[1] - (BLI_rctf_size_y(&node->totr)) * -0.5f) / dpi_fac;
#else
      locx = td2d->loc[0] / dpi_fac;
      locy = td2d->loc[1] / dpi_fac;
#endif

      /* account for parents (nested nodes) */
      if (node->parent) {
        nodeFromView(node->parent, locx, locy, &node->locx, &node->locy);
      }
      else {
        node->locx = locx;
        node->locy = locy;
      }
    }

    /* handle intersection with noodles */
    if (tc->data_len == 1) {
      ED_node_link_intersect_test(t->area, 1);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Node
 * \{ */

void special_aftertrans_update__node(bContext *C, TransInfo *t)
{
  struct Main *bmain = CTX_data_main(C);
  const bool canceled = (t->state == TRANS_CANCEL);

  SpaceNode *snode = (SpaceNode *)t->area->spacedata.first;
  if (canceled && t->remove_on_cancel) {
    /* remove selected nodes on cancel */
    bNodeTree *ntree = snode->edittree;
    if (ntree) {
      bNode *node, *node_next;
      for (node = ntree->nodes.first; node; node = node_next) {
        node_next = node->next;
        if (node->flag & NODE_SELECT) {
          nodeRemoveNode(bmain, ntree, node, true);
        }
      }
      ntreeUpdateTree(bmain, ntree);
    }
  }

  if (!canceled) {
    ED_node_post_apply_transform(C, snode->edittree);
    ED_node_link_insert(bmain, t->area);
  }

  /* clear link line */
  ED_node_link_intersect_test(t->area, 0);
}

/** \} */
