/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2001-2002 NaN Holding BV. All rights reserved. */

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

struct TransCustomDataNode {
  View2DEdgePanData edgepan_data;

  /* Compare if the view has changed so we can update with `transformViewUpdate`. */
  rctf viewrect_prev;
};

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
  /* Weirdo - but the node system is a mix of free 2d elements and DPI sensitive UI. */
#ifdef USE_NODE_CENTER
  td2d->loc[0] = (locx * dpi_fac) + (BLI_rctf_size_x(&node->runtime->totr) * +0.5f);
  td2d->loc[1] = (locy * dpi_fac) + (BLI_rctf_size_y(&node->runtime->totr) * -0.5f);
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

  td->ext = nullptr;
  td->val = nullptr;

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

static void createTransNodeData(bContext * /*C*/, TransInfo *t)
{
  const float dpi_fac = UI_DPI_FAC;
  SpaceNode *snode = static_cast<SpaceNode *>(t->area->spacedata.first);

  /* Custom data to enable edge panning during the node transform */
  TransCustomDataNode *customdata = MEM_cnew<TransCustomDataNode>(__func__);
  UI_view2d_edge_pan_init(t->context,
                          &customdata->edgepan_data,
                          NODE_EDGE_PAN_INSIDE_PAD,
                          NODE_EDGE_PAN_OUTSIDE_PAD,
                          NODE_EDGE_PAN_SPEED_RAMP,
                          NODE_EDGE_PAN_MAX_SPEED,
                          NODE_EDGE_PAN_DELAY,
                          NODE_EDGE_PAN_ZOOM_INFLUENCE);
  customdata->viewrect_prev = customdata->edgepan_data.initial_rect;

  t->custom.type.data = customdata;
  t->custom.type.use_free = true;

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  tc->data_len = 0;

  if (!snode->edittree) {
    return;
  }

  /* Nodes don't support PET and probably never will. */
  t->flag = t->flag & ~T_PROP_EDIT_ALL;

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

  if (tc->data_len == 0) {
    return;
  }

  TransData *td = tc->data = MEM_cnew_array<TransData>(tc->data_len, __func__);
  TransData2D *td2d = tc->data_2d = MEM_cnew_array<TransData2D>(tc->data_len, __func__);

  LISTBASE_FOREACH (bNode *, node, &snode->edittree->nodes) {
    if (node->flag & NODE_TRANSFORM) {
      NodeToTransData(td++, td2d++, node, dpi_fac);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flush Transform Nodes
 * \{ */

static void node_snap_grid_apply(TransInfo *t)
{
  int i;

  if (!(activeSnap(t) && (t->tsnap.mode & (SCE_SNAP_MODE_INCREMENT | SCE_SNAP_MODE_GRID)))) {
    return;
  }

  float grid_size[2];
  copy_v2_v2(grid_size, t->snap_spatial);
  if (t->modifiers & MOD_PRECISION) {
    mul_v2_fl(grid_size, t->snap_spatial_precision);
  }

  /* Early exit on unusable grid size. */
  if (is_zero_v2(grid_size)) {
    return;
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    TransData *td;

    for (i = 0, td = tc->data; i < tc->data_len; i++, td++) {
      float iloc[2], loc[2], tvec[2];
      if (td->flag & TD_SKIP) {
        continue;
      }

      if ((t->flag & T_PROP_EDIT) && (td->factor == 0.0f)) {
        continue;
      }

      copy_v2_v2(iloc, td->loc);

      loc[0] = roundf(iloc[0] / grid_size[0]) * grid_size[0];
      loc[1] = roundf(iloc[1] / grid_size[1]) * grid_size[1];

      sub_v2_v2v2(tvec, loc, iloc);
      add_v2_v2(td->loc, tvec);
    }
  }
}

static void flushTransNodes(TransInfo *t)
{
  using namespace blender::ed;
  const float dpi_fac = UI_DPI_FAC;
  SpaceNode *snode = static_cast<SpaceNode *>(t->area->spacedata.first);

  TransCustomDataNode *customdata = (TransCustomDataNode *)t->custom.type.data;

  if (t->options & CTX_VIEW2D_EDGE_PAN) {
    if (t->state == TRANS_CANCEL) {
      UI_view2d_edge_pan_cancel(t->context, &customdata->edgepan_data);
    }
    else {
      /* Edge panning functions expect window coordinates, mval is relative to region */
      const int xy[2] = {
          t->region->winrct.xmin + t->mval[0],
          t->region->winrct.ymin + t->mval[1],
      };
      UI_view2d_edge_pan_apply(t->context, &customdata->edgepan_data, xy);
    }
  }

  float offset[2] = {0.0f, 0.0f};
  if (t->state != TRANS_CANCEL) {
    if (!BLI_rctf_compare(&customdata->viewrect_prev, &t->region->v2d.cur, FLT_EPSILON)) {
      /* Additional offset due to change in view2D rect. */
      BLI_rctf_transform_pt_v(&t->region->v2d.cur, &customdata->viewrect_prev, offset, offset);
      tranformViewUpdate(t);
      customdata->viewrect_prev = t->region->v2d.cur;
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    node_snap_grid_apply(t);

    /* flush to 2d vector from internally used 3d vector */
    for (int i = 0; i < tc->data_len; i++) {
      TransData *td = &tc->data[i];
      TransData2D *td2d = &tc->data_2d[i];
      bNode *node = static_cast<bNode *>(td->extra);

      float loc[2];
      add_v2_v2v2(loc, td2d->loc, offset);

#ifdef USE_NODE_CENTER
      loc[0] -= 0.5f * BLI_rctf_size_x(&node->runtime->totr);
      loc[1] += 0.5f * BLI_rctf_size_y(&node->runtime->totr);
#endif

      /* Weirdo - but the node system is a mix of free 2d elements and DPI sensitive UI. */
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
      space_node::node_insert_on_link_flags_set(*snode, *t->region);
    }
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Node
 * \{ */

static void special_aftertrans_update__node(bContext *C, TransInfo *t)
{
  using namespace blender::ed;
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = (SpaceNode *)t->area->spacedata.first;
  bNodeTree *ntree = snode->edittree;

  const bool canceled = (t->state == TRANS_CANCEL);

  if (canceled && t->remove_on_cancel) {
    /* remove selected nodes on cancel */
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
    space_node::node_insert_on_link_flags(*bmain, *snode);
  }

  space_node::node_insert_on_link_flags_clear(*ntree);
}

/** \} */

TransConvertTypeInfo TransConvertType_Node = {
    /* flags */ (T_POINTS | T_2D_EDIT),
    /* createTransData */ createTransNodeData,
    /* recalcData */ flushTransNodes,
    /* special_aftertrans_update */ special_aftertrans_update__node,
};
