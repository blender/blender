/* SPDX-FileCopyrightText: 2001-2002 NaN Holding BV. All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup edtransform
 */

#include "DNA_space_types.h"
#include "DNA_userdef_types.h"

#include "MEM_guardedalloc.h"

#include "BLI_listbase.h"
#include "BLI_math_matrix.h"
#include "BLI_math_vector.h"
#include "BLI_math_vector.hh"
#include "BLI_rect.h"

#include "BKE_context.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"

#include "ED_node.hh"

#include "UI_view2d.hh"

#include "transform.hh"
#include "transform_convert.hh"
#include "transform_snap.hh"

#include "WM_api.hh"

namespace blender::ed::transform {

struct TransCustomDataNode {
  View2DEdgePanData edgepan_data{};

  /* Compare if the view has changed so we can update with `transformViewUpdate`. */
  rctf viewrect_prev{};

  bool is_new_node = false;

  Map<bNode *, bNode *> old_parent_by_detached_node;
};

/* -------------------------------------------------------------------- */
/** \name Node Transform Creation
 * \{ */

static void create_transform_data_for_node(TransData &td,
                                           TransData2D &td2d,
                                           bNode &node,
                                           const float dpi_fac)
{
  /* Account for parents (nested nodes). */
  float2 loc = float2(node.location) * dpi_fac;

  /* Use top-left corner as the transform origin for nodes. */
  /* Weirdo - but the node system is a mix of free 2d elements and DPI sensitive UI. */
  td2d.loc[0] = loc.x;
  td2d.loc[1] = loc.y;
  td2d.loc[2] = 0.0f;
  td2d.loc2d = td2d.loc; /* Current location. */

  td.loc = td2d.loc;
  copy_v3_v3(td.iloc, td.loc);
  /* Use node center instead of origin (top-left corner). */
  td.center[0] = td2d.loc[0];
  td.center[1] = td2d.loc[1];
  td.center[2] = 0.0f;

  memset(td.axismtx, 0, sizeof(td.axismtx));
  td.axismtx[2][2] = 1.0f;

  td.val = nullptr;

  td.flag = TD_SELECTED;
  td.dist = 0.0f;

  unit_m3(td.mtx);
  unit_m3(td.smtx);

  td.extra = &node;
}

static bool is_node_parent_select(const bNode *node)
{
  while ((node = node->parent)) {
    if (node->flag & NODE_SELECT) {
      return true;
    }
  }
  return false;
}

/**
 * Some nodes are transformed together with other nodes:
 * - Parent frames with shrinking turned on are automatically resized based on their children.
 * - Child nodes of frames that are manually resizable are transformed together with their parent
 *   frame.
 */
static bool transform_tied_to_other_node(bNode *node, VectorSet<bNode *> transformed_nodes)
{
  /* Check for frame nodes that adjust their size based on the contained child nodes. */
  if (node->is_frame()) {
    const NodeFrame *data = static_cast<const NodeFrame *>(node->storage);
    const bool shrinking = data->flag & NODE_FRAME_SHRINK;
    const bool is_parent = !node->direct_children_in_frame().is_empty();

    if (is_parent && shrinking) {
      return true;
    }
  }

  /* Now check for child nodes of manually resized frames. */
  while ((node = node->parent)) {
    const NodeFrame *parent_data = (const NodeFrame *)node->storage;
    const bool parent_shrinking = parent_data->flag & NODE_FRAME_SHRINK;
    const bool parent_transformed = transformed_nodes.contains(node);

    if (parent_transformed && !parent_shrinking) {
      return true;
    }
  }

  return false;
}

static VectorSet<bNode *> get_transformed_nodes(bNodeTree &node_tree)
{
  VectorSet<bNode *> nodes = node_tree.all_nodes();

  /* Keep only nodes that are selected or inside a frame that is selected. */
  nodes.remove_if([&](bNode *node) {
    const bool node_selected = node->flag & NODE_SELECT;
    const bool parent_selected = is_node_parent_select(node);
    return (!node_selected && !parent_selected);
  });

  /* Remove nodes that are transformed together with their parent or child nodes. */
  nodes.remove_if([&](bNode *node) { return transform_tied_to_other_node(node, nodes); });

  return nodes;
}

static void createTransNodeData(bContext *C, TransInfo *t)
{
  SpaceNode *snode = static_cast<SpaceNode *>(t->area->spacedata.first);
  bNodeTree *node_tree = snode->edittree;
  if (!node_tree) {
    return;
  }

  /* Custom data to enable edge panning during the node transform. */
  TransCustomDataNode *customdata = MEM_new<TransCustomDataNode>(__func__);
  UI_view2d_edge_pan_init(t->context,
                          &customdata->edgepan_data,
                          NODE_EDGE_PAN_INSIDE_PAD,
                          NODE_EDGE_PAN_OUTSIDE_PAD,
                          NODE_EDGE_PAN_SPEED_RAMP,
                          NODE_EDGE_PAN_MAX_SPEED,
                          NODE_EDGE_PAN_DELAY,
                          NODE_EDGE_PAN_ZOOM_INFLUENCE);
  customdata->viewrect_prev = customdata->edgepan_data.initial_rect;
  customdata->is_new_node = t->remove_on_cancel;

  space_node::node_insert_on_link_flags_set(
      *snode, *t->region, t->modifiers & MOD_NODE_ATTACH, customdata->is_new_node);
  space_node::node_insert_on_frame_flag_set(*C, *snode, int2(t->mval));

  t->custom.type.data = customdata;
  t->custom.type.free_cb = [](TransInfo *, TransDataContainer *, TransCustomData *custom_data) {
    TransCustomDataNode *data = static_cast<TransCustomDataNode *>(custom_data->data);
    MEM_delete(data);
    custom_data->data = nullptr;
  };

  TransDataContainer *tc = TRANS_DATA_CONTAINER_FIRST_SINGLE(t);

  /* Nodes don't support proportional editing and probably never will. */
  t->flag = t->flag & ~T_PROP_EDIT_ALL;

  VectorSet<bNode *> nodes = get_transformed_nodes(*node_tree);
  if (nodes.is_empty()) {
    return;
  }

  tc->data_len = nodes.size();
  tc->data = MEM_calloc_arrayN<TransData>(tc->data_len, __func__);
  tc->data_2d = MEM_calloc_arrayN<TransData2D>(tc->data_len, __func__);

  for (const int i : nodes.index_range()) {
    create_transform_data_for_node(tc->data[i], tc->data_2d[i], *nodes[i], UI_SCALE_FAC);
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Flush Transform Nodes
 * \{ */

static void node_snap_grid_apply(TransInfo *t)
{
  if (!(transform_snap_is_active(t) &&
        (t->tsnap.mode & (SCE_SNAP_TO_INCREMENT | SCE_SNAP_TO_GRID))))
  {
    return;
  }

  float2 grid_size = t->snap_spatial;
  if (t->modifiers & MOD_PRECISION) {
    grid_size *= t->snap_spatial_precision;
  }

  /* Early exit on unusable grid size. */
  if (math::is_zero(grid_size)) {
    return;
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    for (const int i : IndexRange(tc->data_len)) {
      TransData &td = tc->data[i];
      if (td.flag & TD_SKIP) {
        continue;
      }

      if ((t->flag & T_PROP_EDIT) && (td.factor == 0.0f)) {
        continue;
      }

      /* Nodes are snapped to the grid by first aligning their initial position to the grid and
       * then offsetting them in grid increments.
       *
       * This ensures that multiple unsnapped nodes snap to the grid in sync while moving.
       */

      const float2 inital_location = td.iloc;
      const float2 target_location = td.loc;
      const float2 offset = target_location - inital_location;

      const float2 snapped_inital_location = math::round(inital_location / grid_size) * grid_size;
      const float2 snapped_offset = math::round(offset / grid_size) * grid_size;
      const float2 snapped_target_location = snapped_inital_location + snapped_offset;

      copy_v2_v2(td.loc, snapped_target_location);
    }
  }
}

static void move_child_nodes(bNode &node, const float2 &delta)
{
  for (bNode *child : node.direct_children_in_frame()) {
    child->location[0] += delta.x;
    child->location[1] += delta.y;
    if (child->is_frame()) {
      move_child_nodes(*child, delta);
    }
  }
}

static bool has_selected_parent(const bNode &node)
{
  for (bNode *parent = node.parent; parent; parent = parent->parent) {
    if (parent->flag & NODE_SELECT) {
      return true;
    }
  }
  return false;
}

static void flushTransNodes(TransInfo *t)
{
  const float dpi_fac = UI_SCALE_FAC;
  SpaceNode *snode = static_cast<SpaceNode *>(t->area->spacedata.first);

  TransCustomDataNode *customdata = (TransCustomDataNode *)t->custom.type.data;

  if (t->options & CTX_VIEW2D_EDGE_PAN) {
    if (t->state == TRANS_CANCEL) {
      UI_view2d_edge_pan_cancel(t->context, &customdata->edgepan_data);
    }
    else {
      /* Edge panning functions expect window coordinates, mval is relative to region. */
      const int xy[2] = {
          t->region->winrct.xmin + int(t->mval[0]),
          t->region->winrct.ymin + int(t->mval[1]),
      };
      UI_view2d_edge_pan_apply(t->context, &customdata->edgepan_data, xy);
    }
  }

  float offset[2] = {0.0f, 0.0f};
  if (t->state != TRANS_CANCEL) {
    if (!BLI_rctf_compare(&customdata->viewrect_prev, &t->region->v2d.cur, FLT_EPSILON)) {
      /* Additional offset due to change in view2D rect. */
      BLI_rctf_transform_pt_v(&t->region->v2d.cur, &customdata->viewrect_prev, offset, offset);
      transformViewUpdate(t);
      customdata->viewrect_prev = t->region->v2d.cur;
    }
  }

  if (t->modifiers & MOD_NODE_FRAME) {
    t->modifiers &= ~MOD_NODE_FRAME;
    Vector<bNode *> nodes_to_detach;
    for (bNode *node : snode->edittree->all_nodes()) {
      if (!(node->flag & NODE_SELECT)) {
        continue;
      }
      if (has_selected_parent(*node)) {
        continue;
      }
      if (!node->parent) {
        continue;
      }
      customdata->old_parent_by_detached_node.add(node, node->parent);
      nodes_to_detach.append(node);
    }
    if (nodes_to_detach.is_empty()) {
      WM_operator_name_call(
          t->context, "NODE_OT_attach", wm::OpCallContext::InvokeDefault, nullptr, nullptr);
    }
    else {
      for (bNode *node : nodes_to_detach) {
        bke::node_detach_node(*snode->edittree, *node);
      }
    }
  }

  FOREACH_TRANS_DATA_CONTAINER (t, tc) {
    node_snap_grid_apply(t);

    /* Flush to 2d vector from internally used 3d vector. */
    for (int i = 0; i < tc->data_len; i++) {
      TransData *td = &tc->data[i];
      TransData2D *td2d = &tc->data_2d[i];
      bNode *node = static_cast<bNode *>(td->extra);

      float2 loc = float2(td2d->loc) + offset;

      /* Weirdo - but the node system is a mix of free 2d elements and DPI sensitive UI. */
      loc /= dpi_fac;

      if (node->is_frame()) {
        const float2 delta = loc - float2(node->location);
        move_child_nodes(*node, delta);
      }

      node->location[0] = loc.x;
      node->location[1] = loc.y;
    }

    /* Handle intersection with noodles. */
    if (tc->data_len == 1) {
      space_node::node_insert_on_link_flags_set(
          *snode, *t->region, t->modifiers & MOD_NODE_ATTACH, customdata->is_new_node);
    }
    space_node::node_insert_on_frame_flag_set(*t->context, *snode, int2(t->mval));
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Special After Transform Node
 * \{ */

static void special_aftertrans_update__node(bContext *C, TransInfo *t)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = (SpaceNode *)t->area->spacedata.first;
  bNodeTree *ntree = snode->edittree;
  const TransCustomDataNode &customdata = *(TransCustomDataNode *)t->custom.type.data;

  const bool canceled = (t->state == TRANS_CANCEL);

  if (canceled) {
    for (auto &&[node, parent] : customdata.old_parent_by_detached_node.items()) {
      bke::node_attach_node(*ntree, *node, *parent);
    }
  }
  if (canceled && t->remove_on_cancel) {
    /* Remove selected nodes on cancel. */
    if (ntree) {
      LISTBASE_FOREACH_MUTABLE (bNode *, node, &ntree->nodes) {
        if (node->flag & NODE_SELECT) {
          bke::node_remove_node(bmain, *ntree, *node, true);
        }
      }
      BKE_main_ensure_invariants(*bmain, ntree->id);
    }
  }

  if (!canceled) {
    ED_node_post_apply_transform(C, snode->edittree);
    if (t->modifiers & MOD_NODE_ATTACH) {
      space_node::node_insert_on_link_flags(*bmain, *snode, customdata.is_new_node);
    }
  }

  space_node::node_insert_on_link_flags_clear(*ntree);
  space_node::node_insert_on_frame_flag_clear(*snode);

  wmOperatorType *ot = WM_operatortype_find("NODE_OT_insert_offset", true);
  BLI_assert(ot);
  PointerRNA ptr;
  WM_operator_properties_create_ptr(&ptr, ot);
  WM_operator_name_call_ptr(C, ot, wm::OpCallContext::InvokeDefault, &ptr, nullptr);
  WM_operator_properties_free(&ptr);
}

/** \} */

TransConvertTypeInfo TransConvertType_Node = {
    /*flags*/ (T_POINTS | T_2D_EDIT),
    /*create_trans_data*/ createTransNodeData,
    /*recalc_data*/ flushTransNodes,
    /*special_aftertrans_update*/ special_aftertrans_update__node,
};

}  // namespace blender::ed::transform
