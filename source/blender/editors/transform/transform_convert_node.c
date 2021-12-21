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

#include "BLI_listbase.h"
#include "BLI_math.h"
#include "BLI_rect.h"

#include "BKE_context.h"
#include "BKE_node.h"
#include "BKE_node_tree_update.h"
#include "BKE_report.h"

#include "ED_node.h"

#include "UI_interface.h"
#include "UI_view2d.h"

#include "transform.h"
#include "transform_convert.h"
#include "transform_snap.h"

/* -------------------------------------------------------------------- */
/** \name Node Transform Creation
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

  td->flag = TD_SELECTED;
  td->dist = 0.0f;

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
  SpaceNode *snode = t->area->spacedata.first;

  /* Custom data to enable edge panning during the node transform */
  View2DEdgePanData *customdata = MEM_callocN(sizeof(*customdata), __func__);
  UI_view2d_edge_pan_init(t->context,
                          customdata,
                          NODE_EDGE_PAN_INSIDE_PAD,
                          NODE_EDGE_PAN_OUTSIDE_PAD,
                          NODE_EDGE_PAN_SPEED_RAMP,
                          NODE_EDGE_PAN_MAX_SPEED,
                          NODE_EDGE_PAN_DELAY,
                          NODE_EDGE_PAN_ZOOM_INFLUENCE);
  t->custom.type.data = customdata;
  t->custom.type.use_free = true;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  tc->data_len = 0;

  if (!snode->edittree) {
    return;
  }

  /* Nodes don't support PET and probably never will. */
  t->flag &= ~T_PROP_EDIT_ALL;

  /* set transform flags on nodes */
  LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & NODE_SELECT && is_node_parent_select(node) == false) {
      node->flag |= NODE_TRANSFORM;
      tc->data_len++;
    }
    else {
      node->flag &= ~NODE_TRANSFORM;
    }
  }

  TransData *td = tc->data = MEM_callocN(tc->data_len * sizeof(TransData), "TransNode TransData");
  TransData2D *td2d = tc->data_2d = MEM_callocN(tc->data_len * sizeof(TransData2D),
                                                "TransNode TransData2D");

  LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & NODE_TRANSFORM) {
      NodeToTransData(td++, td2d++, node, dpi_fac);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Node Transform Creation
 * \{ */

void flushTransNodes(TransInfo *t)
{
  const float dpi_fac = UI_DPI_FAC;

  View2DEdgePanData *customdata = (View2DEdgePanData *)t->custom.type.data;

  if (t->options & CTX_VIEW2D_EDGE_PAN) {
    if (t->state == TRANS_CANCEL) {
      UI_view2d_edge_pan_cancel(t->context, customdata);
    }
    else {
      /* Edge panning functions expect window coordinates, mval is relative to region */
      const int xy[2] = {
          t->region->winrct.xmin + t->mval[0],
          t->region->winrct.ymin + t->mval[1],
      };
      UI_view2d_edge_pan_apply(t->context, customdata, xy);
    }
  }

  /* Initial and current view2D rects for additional transform due to view panning and zooming */
  const rctf *rect_src = &customdata->initial_rect;
  const rctf *rect_dst = &t->region->v2d.cur;

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    applyGridAbsolute(t);

    /* flush to 2d vector from internally used 3d vector */
    for (int i = 0; i < tc->data_len; i++) {
      TransData *td = &tc->data[i];
      TransData2D *td2d = &tc->data_2d[i];
      bNode *node = td->extra;

      float loc[2];
      copy_v2_v2(loc, td2d->loc);

      /* additional offset due to change in view2D rect */
      BLI_rctf_transform_pt_v(rect_dst, rect_src, loc, loc);

#ifdef USE_NODE_CENTER
      loc[0] -= 0.5f * BLI_rctf_size_x(&node->totr);
      loc[1] += 0.5f * BLI_rctf_size_y(&node->totr);
#endif

      /* weirdo - but the node system is a mix of free 2d elements and dpi sensitive UI */
      loc[0] /= dpi_fac;
      loc[1] /= dpi_fac;

      /* account for parents (nested nodes) */
      if (node->parent) {
        nodeFromView(node->parent, loc[0], loc[1], &loc[0], &loc[1]);
      }

      node->locx = loc[0];
      node->locy = loc[1];
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
      LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
        if (node->flag & NODE_SELECT) {
          nodeRemoveNode(bmain, ntree, node, true);
        }
      }
      ED_node_tree_propagate_change(C, bmain, ntree);
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
