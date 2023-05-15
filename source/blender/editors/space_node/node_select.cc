/* SPDX-License-Identifier: GPL-2.0-or-later
 * Copyright 2008 Blender Foundation */

/** \file
 * \ingroup spnode
 */

#include <array>
#include <cstdlib>

#include "DNA_node_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_lasso_2d.h"
#include "BLI_listbase.h"
#include "BLI_rect.h"
#include "BLI_string.h"
#include "BLI_string_search.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"
#include "BKE_workspace.h"

#include "ED_node.h"  /* own include */
#include "ED_node.hh" /* own include */
#include "ED_screen.h"
#include "ED_select_utils.h"
#include "ED_view3d.h"
#include "ED_viewer_path.hh"

#include "RNA_access.h"
#include "RNA_define.h"

#include "WM_api.h"
#include "WM_types.h"

#include "UI_interface.h"
#include "UI_resources.h"
#include "UI_view2d.h"

#include "DEG_depsgraph.h"

#include "MEM_guardedalloc.h"

#include "node_intern.hh" /* own include */

namespace blender::ed::space_node {

static bool is_event_over_node_or_socket(const bContext &C, const wmEvent &event);

/**
 * Function to detect if there is a visible view3d that uses workbench in texture mode.
 * This function is for fixing #76970 for Blender 2.83. The actual fix should add a mechanism in
 * the depsgraph that can be used by the draw engines to check if they need to be redrawn.
 *
 * We don't want to add these risky changes this close before releasing 2.83 without good testing
 * hence this workaround. There are still cases were too many updates happen. For example when you
 * have both a Cycles and workbench with textures viewport.
 */
static bool has_workbench_in_texture_color(const wmWindowManager *wm,
                                           const Scene *scene,
                                           const Object *ob)
{
  LISTBASE_FOREACH (wmWindow *, win, &wm->windows) {
    if (win->scene != scene) {
      continue;
    }
    const bScreen *screen = BKE_workspace_active_screen_get(win->workspace_hook);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      if (area->spacetype == SPACE_VIEW3D) {
        const View3D *v3d = (const View3D *)area->spacedata.first;

        if (ED_view3d_has_workbench_in_texture_color(scene, ob, v3d)) {
          return true;
        }
      }
    }
  }
  return false;
}

/* -------------------------------------------------------------------- */
/** \name Public Node Selection API
 * \{ */

rctf node_frame_rect_inside(const bNode &node)
{
  const float margin = 1.5f * U.widget_unit;
  rctf frame_inside = {
      node.runtime->totr.xmin,
      node.runtime->totr.xmax,
      node.runtime->totr.ymin,
      node.runtime->totr.ymax,
  };

  BLI_rctf_pad(&frame_inside, -margin, -margin);

  return frame_inside;
}

bool node_or_socket_isect_event(const bContext &C, const wmEvent &event)
{
  return is_event_over_node_or_socket(C, event);
}

static bool node_frame_select_isect_mouse(const bNode &node, const float2 &mouse)
{
  /* Frame nodes are selectable by their borders (including their whole rect - as for other nodes -
   * would prevent e.g. box selection of nodes inside that frame). */
  const rctf frame_inside = node_frame_rect_inside(node);
  if (BLI_rctf_isect_pt(&node.runtime->totr, mouse.x, mouse.y) &&
      !BLI_rctf_isect_pt(&frame_inside, mouse.x, mouse.y))
  {
    return true;
  }

  return false;
}

static bNode *node_under_mouse_select(bNodeTree &ntree, const float2 mouse)
{
  LISTBASE_FOREACH_BACKWARD (bNode *, node, &ntree.nodes) {
    switch (node->type) {
      case NODE_FRAME: {
        if (node_frame_select_isect_mouse(*node, mouse)) {
          return node;
        }
        break;
      }
      default: {
        if (BLI_rctf_isect_pt(&node->runtime->totr, int(mouse.x), int(mouse.y))) {
          return node;
        }
        break;
      }
    }
  }
  return nullptr;
}

static bool node_under_mouse_tweak(const bNodeTree &ntree, const float2 &mouse)
{
  LISTBASE_FOREACH_BACKWARD (const bNode *, node, &ntree.nodes) {
    switch (node->type) {
      case NODE_REROUTE: {
        const float2 location = node_to_view(*node, {node->locx, node->locy});
        if (math::distance_squared(mouse, location) < square_f(24.0f)) {
          return true;
        }
        break;
      }
      case NODE_FRAME: {
        if (node_frame_select_isect_mouse(*node, mouse)) {
          return true;
        }
        break;
      }
      default: {
        if (BLI_rctf_isect_pt(&node->runtime->totr, mouse.x, mouse.y)) {
          return true;
        }
        break;
      }
    }
  }
  return false;
}

static bool is_position_over_node_or_socket(SpaceNode &snode, const float2 &mouse)
{
  if (node_under_mouse_tweak(*snode.edittree, mouse)) {
    return true;
  }
  if (node_find_indicated_socket(snode, mouse, SOCK_IN | SOCK_OUT)) {
    return true;
  }
  return false;
}

static bool is_event_over_node_or_socket(const bContext &C, const wmEvent &event)
{
  SpaceNode &snode = *CTX_wm_space_node(&C);
  ARegion &region = *CTX_wm_region(&C);

  int2 mval;
  WM_event_drag_start_mval(&event, &region, mval);

  float2 mouse;
  UI_view2d_region_to_view(&region.v2d, mval.x, mval.y, &mouse.x, &mouse.y);
  return is_position_over_node_or_socket(snode, mouse);
}

void node_socket_select(bNode *node, bNodeSocket &sock)
{
  sock.flag |= SELECT;

  /* select node too */
  if (node) {
    node->flag |= SELECT;
  }
}

void node_socket_deselect(bNode *node, bNodeSocket &sock, const bool deselect_node)
{
  sock.flag &= ~SELECT;

  if (node && deselect_node) {
    bool sel = false;

    /* if no selected sockets remain, also deselect the node */
    LISTBASE_FOREACH (bNodeSocket *, input, &node->inputs) {
      if (input->flag & SELECT) {
        sel = true;
        break;
      }
    }
    LISTBASE_FOREACH (bNodeSocket *, output, &node->outputs) {
      if (output->flag & SELECT) {
        sel = true;
        break;
      }
    }

    if (!sel) {
      node->flag &= ~SELECT;
    }
  }
}

static void node_socket_toggle(bNode *node, bNodeSocket &sock, bool deselect_node)
{
  if (sock.flag & SELECT) {
    node_socket_deselect(node, sock, deselect_node);
  }
  else {
    node_socket_select(node, sock);
  }
}

void node_deselect_all(bNodeTree &node_tree)
{
  for (bNode *node : node_tree.all_nodes()) {
    nodeSetSelected(node, false);
  }
}

void node_deselect_all_input_sockets(bNodeTree &node_tree, const bool deselect_nodes)
{
  /* XXX not calling node_socket_deselect here each time, because this does iteration
   * over all node sockets internally to check if the node stays selected.
   * We can do that more efficiently here.
   */

  for (bNode *node : node_tree.all_nodes()) {
    bool sel = false;

    LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
      socket->flag &= ~SELECT;
    }

    /* If no selected sockets remain, also deselect the node. */
    if (deselect_nodes) {
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
        if (socket->flag & SELECT) {
          sel = true;
          break;
        }
      }

      if (!sel) {
        node->flag &= ~SELECT;
      }
    }
  }
}

void node_deselect_all_output_sockets(bNodeTree &node_tree, const bool deselect_nodes)
{
  /* XXX not calling node_socket_deselect here each time, because this does iteration
   * over all node sockets internally to check if the node stays selected.
   * We can do that more efficiently here.
   */

  for (bNode *node : node_tree.all_nodes()) {
    bool sel = false;

    LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
      socket->flag &= ~SELECT;
    }

    /* if no selected sockets remain, also deselect the node */
    if (deselect_nodes) {
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
        if (socket->flag & SELECT) {
          sel = true;
          break;
        }
      }

      if (!sel) {
        node->flag &= ~SELECT;
      }
    }
  }
}

void node_select_paired(bNodeTree &node_tree)
{
  for (bNode *input_node : node_tree.nodes_by_type("GeometryNodeSimulationInput")) {
    const auto *storage = static_cast<const NodeGeometrySimulationInput *>(input_node->storage);
    if (bNode *output_node = node_tree.node_by_id(storage->output_node_id)) {
      if (input_node->flag & NODE_SELECT) {
        output_node->flag |= NODE_SELECT;
      }
      if (output_node->flag & NODE_SELECT) {
        input_node->flag |= NODE_SELECT;
      }
    }
  }
}

VectorSet<bNode *> get_selected_nodes(bNodeTree &node_tree)
{
  VectorSet<bNode *> selected_nodes;
  for (bNode *node : node_tree.all_nodes()) {
    if (node->flag & NODE_SELECT) {
      selected_nodes.add(node);
    }
  }
  return selected_nodes;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Grouped Operator
 * \{ */

/* Return true if we need redraw, otherwise false. */

static bool node_select_grouped_type(bNodeTree &node_tree, bNode &node_act)
{
  bool changed = false;
  for (bNode *node : node_tree.all_nodes()) {
    if ((node->flag & SELECT) == 0) {
      if (node->type == node_act.type) {
        nodeSetSelected(node, true);
        changed = true;
      }
    }
  }
  return changed;
}

static bool node_select_grouped_color(bNodeTree &node_tree, bNode &node_act)
{
  bool changed = false;
  for (bNode *node : node_tree.all_nodes()) {
    if ((node->flag & SELECT) == 0) {
      if (compare_v3v3(node->color, node_act.color, 0.005f)) {
        nodeSetSelected(node, true);
        changed = true;
      }
    }
  }
  return changed;
}

static bool node_select_grouped_name(bNodeTree &node_tree, bNode &node_act, const bool from_right)
{
  bool changed = false;
  const uint delims[] = {'.', '-', '_', '\0'};
  size_t pref_len_act, pref_len_curr;
  const char *sep, *suf_act, *suf_curr;

  pref_len_act = BLI_str_partition_ex_utf8(
      node_act.name, nullptr, delims, &sep, &suf_act, from_right);

  /* NOTE: in case we are searching for suffix, and found none, use whole name as suffix. */
  if (from_right && !(sep && suf_act)) {
    pref_len_act = 0;
    suf_act = node_act.name;
  }

  for (bNode *node : node_tree.all_nodes()) {
    if (node->flag & SELECT) {
      continue;
    }
    pref_len_curr = BLI_str_partition_ex_utf8(
        node->name, nullptr, delims, &sep, &suf_curr, from_right);

    /* Same as with active node name! */
    if (from_right && !(sep && suf_curr)) {
      pref_len_curr = 0;
      suf_curr = node->name;
    }

    if ((from_right && STREQ(suf_act, suf_curr)) ||
        (!from_right && (pref_len_act == pref_len_curr) &&
         STREQLEN(node_act.name, node->name, pref_len_act)))
    {
      nodeSetSelected(node, true);
      changed = true;
    }
  }

  return changed;
}

enum {
  NODE_SELECT_GROUPED_TYPE = 0,
  NODE_SELECT_GROUPED_COLOR = 1,
  NODE_SELECT_GROUPED_PREFIX = 2,
  NODE_SELECT_GROUPED_SUFIX = 3,
};

static int node_select_grouped_exec(bContext *C, wmOperator *op)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &node_tree = *snode.edittree;
  bNode *node_act = nodeGetActive(snode.edittree);

  if (node_act == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const int type = RNA_enum_get(op->ptr, "type");

  if (!extend) {
    node_deselect_all(node_tree);
  }
  nodeSetSelected(node_act, true);

  switch (type) {
    case NODE_SELECT_GROUPED_TYPE:
      changed = node_select_grouped_type(node_tree, *node_act);
      break;
    case NODE_SELECT_GROUPED_COLOR:
      changed = node_select_grouped_color(node_tree, *node_act);
      break;
    case NODE_SELECT_GROUPED_PREFIX:
      changed = node_select_grouped_name(node_tree, *node_act, false);
      break;
    case NODE_SELECT_GROUPED_SUFIX:
      changed = node_select_grouped_name(node_tree, *node_act, true);
      break;
    default:
      break;
  }

  if (changed) {
    node_sort(node_tree);
    WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
    return OPERATOR_FINISHED;
  }

  return OPERATOR_CANCELLED;
}

void NODE_OT_select_grouped(wmOperatorType *ot)
{
  PropertyRNA *prop;
  static const EnumPropertyItem prop_select_grouped_types[] = {
      {NODE_SELECT_GROUPED_TYPE, "TYPE", 0, "Type", ""},
      {NODE_SELECT_GROUPED_COLOR, "COLOR", 0, "Color", ""},
      {NODE_SELECT_GROUPED_PREFIX, "PREFIX", 0, "Prefix", ""},
      {NODE_SELECT_GROUPED_SUFIX, "SUFFIX", 0, "Suffix", ""},
      {0, nullptr, 0, nullptr, nullptr},
  };

  /* identifiers */
  ot->name = "Select Grouped";
  ot->description = "Select nodes with similar properties";
  ot->idname = "NODE_OT_select_grouped";

  /* api callbacks */
  ot->invoke = WM_menu_invoke;
  ot->exec = node_select_grouped_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  prop = RNA_def_boolean(ot->srna,
                         "extend",
                         false,
                         "Extend",
                         "Extend selection instead of deselecting everything first");
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  ot->prop = RNA_def_enum(ot->srna, "type", prop_select_grouped_types, 0, "Type", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select (Cursor Pick) Operator
 * \{ */

void node_select_single(bContext &C, bNode &node)
{
  Main *bmain = CTX_data_main(&C);
  SpaceNode &snode = *CTX_wm_space_node(&C);
  bNodeTree &node_tree = *snode.edittree;
  const Object *ob = CTX_data_active_object(&C);
  const Scene *scene = CTX_data_scene(&C);
  const wmWindowManager *wm = CTX_wm_manager(&C);
  bool active_texture_changed = false;

  for (bNode *node_iter : node_tree.all_nodes()) {
    if (node_iter != &node) {
      nodeSetSelected(node_iter, false);
    }
  }
  nodeSetSelected(&node, true);

  ED_node_set_active(bmain, &snode, &node_tree, &node, &active_texture_changed);
  ED_node_set_active_viewer_key(&snode);

  node_sort(node_tree);
  if (active_texture_changed && has_workbench_in_texture_color(wm, scene, ob)) {
    DEG_id_tag_update(&node_tree.id, ID_RECALC_COPY_ON_WRITE);
  }

  WM_event_add_notifier(&C, NC_NODE | NA_SELECTED, nullptr);
}

static bool node_mouse_select(bContext *C,
                              wmOperator *op,
                              const int2 mval,
                              SelectPick_Params *params)
{
  Main &bmain = *CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &node_tree = *snode.edittree;
  ARegion &region = *CTX_wm_region(C);
  const Object *ob = CTX_data_active_object(C);
  const Scene *scene = CTX_data_scene(C);
  const wmWindowManager *wm = CTX_wm_manager(C);
  bNode *node = nullptr;
  bNodeSocket *sock = nullptr;

  /* Always do socket_select when extending selection. */
  const bool socket_select = (params->sel_op == SEL_OP_XOR) ||
                             RNA_boolean_get(op->ptr, "socket_select");
  bool changed = false;
  bool found = false;
  bool node_was_selected = false;

  /* Get mouse coordinates in view2d space. */
  float2 cursor;
  UI_view2d_region_to_view(&region.v2d, mval.x, mval.y, &cursor.x, &cursor.y);

  /* First do socket selection, these generally overlap with nodes. */
  if (socket_select) {
    /* NOTE: unlike nodes #SelectPick_Params isn't fully supported. */
    const bool extend = (params->sel_op == SEL_OP_XOR);
    sock = node_find_indicated_socket(snode, cursor, SOCK_IN);
    if (sock) {
      node = &sock->owner_node();
      found = true;
      node_was_selected = node->flag & SELECT;

      /* NOTE: SOCK_IN does not take into account the extend case...
       * This feature is not really used anyway currently? */
      node_socket_toggle(node, *sock, true);
      changed = true;
    }
    if (!changed) {
      sock = node_find_indicated_socket(snode, cursor, SOCK_OUT);
      if (sock) {
        node = &sock->owner_node();
        found = true;
        node_was_selected = node->flag & SELECT;

        if (sock->flag & SELECT) {
          if (extend) {
            node_socket_deselect(node, *sock, true);
            changed = true;
          }
        }
        else {
          /* Only allow one selected output per node, for sensible linking.
           * Allow selecting outputs from different nodes though, if extend is true. */
          for (bNodeSocket *tsock : node->output_sockets()) {
            if (tsock == sock) {
              continue;
            }
            node_socket_deselect(node, *tsock, true);
            changed = true;
          }
          if (!extend) {
            for (bNode *tnode : node_tree.all_nodes()) {
              if (tnode == node) {
                continue;
              }
              for (bNodeSocket *tsock : tnode->output_sockets()) {
                node_socket_deselect(tnode, *tsock, true);
                changed = true;
              }
            }
          }
          node_socket_select(node, *sock);
          changed = true;
        }
      }
    }
  }

  if (!sock) {

    /* Find the closest visible node. */
    node = node_under_mouse_select(node_tree, cursor);
    found = (node != nullptr);
    node_was_selected = node && (node->flag & SELECT);

    if (params->sel_op == SEL_OP_SET) {
      if ((found && params->select_passthrough) && (node->flag & SELECT)) {
        found = false;
      }
      else if (found || params->deselect_all) {
        /* Deselect everything. */
        node_deselect_all(node_tree);
        changed = true;
      }
    }

    if (found) {
      switch (params->sel_op) {
        case SEL_OP_ADD:
          nodeSetSelected(node, true);
          break;
        case SEL_OP_SUB:
          nodeSetSelected(node, false);
          break;
        case SEL_OP_XOR: {
          /* Check active so clicking on an inactive node activates it. */
          bool is_selected = (node->flag & NODE_SELECT) && (node->flag & NODE_ACTIVE);
          nodeSetSelected(node, !is_selected);
          break;
        }
        case SEL_OP_SET:
          nodeSetSelected(node, true);
          break;
        case SEL_OP_AND:
          /* Doesn't make sense for picking. */
          BLI_assert_unreachable();
          break;
      }

      changed = true;
    }
  }

  if (RNA_boolean_get(op->ptr, "clear_viewer")) {
    if (node == nullptr) {
      /* Disable existing active viewer. */
      WorkSpace *workspace = CTX_wm_workspace(C);
      BKE_viewer_path_clear(&workspace->viewer_path);
      WM_event_add_notifier(C, NC_VIEWER_PATH, nullptr);
    }
  }

  if (!(changed || found)) {
    return false;
  }

  bool active_texture_changed = false;
  bool viewer_node_changed = false;
  if ((node != nullptr) && (node_was_selected == false || params->select_passthrough == false)) {
    viewer_node_changed = (node->flag & NODE_DO_OUTPUT) == 0 && node->type == GEO_NODE_VIEWER;
    ED_node_set_active(&bmain, &snode, snode.edittree, node, &active_texture_changed);
  }
  else if (node != nullptr && node->type == GEO_NODE_VIEWER) {
    viewer_path::activate_geometry_node(bmain, snode, *node);
  }
  ED_node_set_active_viewer_key(&snode);
  node_sort(node_tree);
  if ((active_texture_changed && has_workbench_in_texture_color(wm, scene, ob)) ||
      viewer_node_changed)
  {
    DEG_id_tag_update(&snode.edittree->id, ID_RECALC_COPY_ON_WRITE);
  }

  WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);

  return true;
}

static int node_select_exec(bContext *C, wmOperator *op)
{
  /* Get settings from RNA properties for operator. */
  int2 mval;
  RNA_int_get_array(op->ptr, "location", mval);

  SelectPick_Params params = {};
  ED_select_pick_params_from_operator(op->ptr, &params);

  /* Perform the selection. */
  const bool changed = node_mouse_select(C, op, mval, &params);

  if (changed) {
    return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
  }
  /* Nothing selected, just pass through. */
  return OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED;
}

static int node_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_int_set_array(op->ptr, "location", event->mval);

  const int retval = node_select_exec(C, op);

  return WM_operator_flag_only_pass_through_on_press(retval, event);
}

void NODE_OT_select(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select";
  ot->idname = "NODE_OT_select";
  ot->description = "Select the node under the cursor";

  /* api callbacks */
  ot->exec = node_select_exec;
  ot->invoke = node_select_invoke;
  ot->poll = ED_operator_node_active;
  ot->get_name = ED_select_pick_get_name;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_mouse_select(ot);

  prop = RNA_def_int_vector(ot->srna,
                            "location",
                            2,
                            nullptr,
                            INT_MIN,
                            INT_MAX,
                            "Location",
                            "Mouse location",
                            INT_MIN,
                            INT_MAX);
  RNA_def_property_flag(prop, PROP_HIDDEN);

  RNA_def_boolean(ot->srna, "socket_select", false, "Socket Select", "");

  RNA_def_boolean(ot->srna,
                  "clear_viewer",
                  false,
                  "Clear Viewer",
                  "Deactivate geometry nodes viewer when clicking in empty space");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Box Select Operator
 * \{ */

static int node_box_select_exec(bContext *C, wmOperator *op)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &node_tree = *snode.edittree;
  const ARegion &region = *CTX_wm_region(C);
  rctf rectf;

  WM_operator_properties_border_to_rctf(op, &rectf);
  UI_view2d_region_to_view_rctf(&region.v2d, &rectf, &rectf);

  const eSelectOp sel_op = (eSelectOp)RNA_enum_get(op->ptr, "mode");
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    node_deselect_all(node_tree);
  }

  for (bNode *node : node_tree.all_nodes()) {
    bool is_inside = false;

    switch (node->type) {
      case NODE_FRAME: {
        /* Frame nodes are selectable by their borders (including their whole rect - as for other
         * nodes - would prevent selection of other nodes inside that frame. */
        const rctf frame_inside = node_frame_rect_inside(*node);
        if (BLI_rctf_isect(&rectf, &node->runtime->totr, nullptr) &&
            !BLI_rctf_inside_rctf(&frame_inside, &rectf))
        {
          nodeSetSelected(node, select);
          is_inside = true;
        }
        break;
      }
      default: {
        is_inside = BLI_rctf_isect(&rectf, &node->runtime->totr, nullptr);
        break;
      }
    }

    if (is_inside) {
      nodeSetSelected(node, select);
    }
  }

  node_sort(node_tree);

  WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

static int node_box_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool tweak = RNA_boolean_get(op->ptr, "tweak");

  if (tweak && is_event_over_node_or_socket(*C, *event)) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  return WM_gesture_box_invoke(C, op, event);
}

void NODE_OT_select_box(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Box Select";
  ot->idname = "NODE_OT_select_box";
  ot->description = "Use box selection to select nodes";

  /* api callbacks */
  ot->invoke = node_box_select_invoke;
  ot->exec = node_box_select_exec;
  ot->modal = WM_gesture_box_modal;
  ot->cancel = WM_gesture_box_cancel;

  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "tweak",
                  false,
                  "Tweak",
                  "Only activate when mouse is not over a node (useful for tweak gesture)");

  WM_operator_properties_gesture_box(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Circle Select Operator
 * \{ */

static int node_circleselect_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  bNodeTree &node_tree = *snode->edittree;

  int x, y, radius;
  float2 offset;

  float zoom = float(BLI_rcti_size_x(&region->winrct)) / float(BLI_rctf_size_x(&region->v2d.cur));

  const eSelectOp sel_op = ED_select_op_modal(
      (eSelectOp)RNA_enum_get(op->ptr, "mode"),
      WM_gesture_is_modal_first((const wmGesture *)op->customdata));
  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    node_deselect_all(node_tree);
  }

  /* get operator properties */
  x = RNA_int_get(op->ptr, "x");
  y = RNA_int_get(op->ptr, "y");
  radius = RNA_int_get(op->ptr, "radius");

  UI_view2d_region_to_view(&region->v2d, x, y, &offset.x, &offset.y);

  for (bNode *node : node_tree.all_nodes()) {
    switch (node->type) {
      case NODE_FRAME: {
        /* Frame nodes are selectable by their borders (including their whole rect - as for other
         * nodes - would prevent selection of _only_ other nodes inside that frame. */
        rctf frame_inside = node_frame_rect_inside(*node);
        const float radius_adjusted = float(radius) / zoom;
        BLI_rctf_pad(&frame_inside, -2.0f * radius_adjusted, -2.0f * radius_adjusted);
        if (BLI_rctf_isect_circle(&node->runtime->totr, offset, radius_adjusted) &&
            !BLI_rctf_isect_circle(&frame_inside, offset, radius_adjusted))
        {
          nodeSetSelected(node, select);
        }
        break;
      }
      default: {
        if (BLI_rctf_isect_circle(&node->runtime->totr, offset, radius / zoom)) {
          nodeSetSelected(node, select);
        }
        break;
      }
    }
  }

  WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_select_circle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Circle Select";
  ot->idname = "NODE_OT_select_circle";
  ot->description = "Use circle selection to select nodes";

  /* api callbacks */
  ot->invoke = WM_gesture_circle_invoke;
  ot->exec = node_circleselect_exec;
  ot->modal = WM_gesture_circle_modal;
  ot->poll = ED_operator_node_active;
  ot->get_name = ED_select_circle_get_name;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  /* properties */
  WM_operator_properties_gesture_circle(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Lasso Select Operator
 * \{ */

static int node_lasso_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool tweak = RNA_boolean_get(op->ptr, "tweak");

  if (tweak && is_event_over_node_or_socket(*C, *event)) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  return WM_gesture_lasso_invoke(C, op, event);
}

static bool do_lasso_select_node(bContext *C,
                                 const int mcoords[][2],
                                 const int mcoords_len,
                                 eSelectOp sel_op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree &node_tree = *snode->edittree;

  ARegion *region = CTX_wm_region(C);

  rcti rect;
  bool changed = false;

  const bool select = (sel_op != SEL_OP_SUB);
  if (SEL_OP_USE_PRE_DESELECT(sel_op)) {
    node_deselect_all(node_tree);
    changed = true;
  }

  /* Get rectangle from operator. */
  BLI_lasso_boundbox(&rect, mcoords, mcoords_len);

  for (bNode *node : node_tree.all_nodes()) {
    if (select && (node->flag & NODE_SELECT)) {
      continue;
    }

    switch (node->type) {
      case NODE_FRAME: {
        /* Frame nodes are selectable by their borders (including their whole rect - as for other
         * nodes - would prevent selection of other nodes inside that frame. */
        rctf rectf;
        BLI_rctf_rcti_copy(&rectf, &rect);
        UI_view2d_region_to_view_rctf(&region->v2d, &rectf, &rectf);
        const rctf frame_inside = node_frame_rect_inside(*node);
        if (BLI_rctf_isect(&rectf, &node->runtime->totr, nullptr) &&
            !BLI_rctf_inside_rctf(&frame_inside, &rectf))
        {
          nodeSetSelected(node, select);
          changed = true;
        }
        break;
      }
      default: {
        int2 screen_co;
        const float2 center = {BLI_rctf_cent_x(&node->runtime->totr),
                               BLI_rctf_cent_y(&node->runtime->totr)};

        /* marker in screen coords */
        if (UI_view2d_view_to_region_clip(
                &region->v2d, center.x, center.y, &screen_co.x, &screen_co.y) &&
            BLI_rcti_isect_pt(&rect, screen_co.x, screen_co.y) &&
            BLI_lasso_is_point_inside(mcoords, mcoords_len, screen_co.x, screen_co.y, INT_MAX))
        {
          nodeSetSelected(node, select);
          changed = true;
        }
        break;
      }
    }
  }

  if (changed) {
    WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
  }

  return changed;
}

static int node_lasso_select_exec(bContext *C, wmOperator *op)
{
  int mcoords_len;
  const int(*mcoords)[2] = WM_gesture_lasso_path_to_array(C, op, &mcoords_len);

  if (mcoords) {
    const eSelectOp sel_op = (eSelectOp)RNA_enum_get(op->ptr, "mode");

    do_lasso_select_node(C, mcoords, mcoords_len, sel_op);

    MEM_freeN((void *)mcoords);

    return OPERATOR_FINISHED;
  }
  return OPERATOR_PASS_THROUGH;
}

void NODE_OT_select_lasso(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Lasso Select";
  ot->description = "Select nodes using lasso selection";
  ot->idname = "NODE_OT_select_lasso";

  /* api callbacks */
  ot->invoke = node_lasso_select_invoke;
  ot->modal = WM_gesture_lasso_modal;
  ot->exec = node_lasso_select_exec;
  ot->poll = ED_operator_node_active;
  ot->cancel = WM_gesture_lasso_cancel;

  /* flags */
  ot->flag = OPTYPE_UNDO | OPTYPE_DEPENDS_ON_CURSOR;

  /* properties */
  RNA_def_boolean(ot->srna,
                  "tweak",
                  false,
                  "Tweak",
                  "Only activate when mouse is not over a node (useful for tweak gesture)");

  WM_operator_properties_gesture_lasso(ot);
  WM_operator_properties_select_operation_simple(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name (De)select All Operator
 * \{ */

static bool any_node_selected(const bNodeTree &node_tree)
{
  for (const bNode *node : node_tree.all_nodes()) {
    if (node->flag & NODE_SELECT) {
      return true;
    }
  }
  return false;
}

static int node_select_all_exec(bContext *C, wmOperator *op)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &node_tree = *snode.edittree;

  node_tree.ensure_topology_cache();

  int action = RNA_enum_get(op->ptr, "action");
  if (action == SEL_TOGGLE) {
    if (any_node_selected(node_tree)) {
      action = SEL_DESELECT;
    }
    else {
      action = SEL_SELECT;
    }
  }

  switch (action) {
    case SEL_SELECT:
      for (bNode *node : node_tree.all_nodes()) {
        nodeSetSelected(node, true);
      }
      break;
    case SEL_DESELECT:
      node_deselect_all(node_tree);
      break;
    case SEL_INVERT:
      for (bNode *node : node_tree.all_nodes()) {
        nodeSetSelected(node, !(node->flag & SELECT));
      }
      break;
  }

  node_sort(node_tree);

  WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
  return OPERATOR_FINISHED;
}

void NODE_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->description = "(De)select all nodes";
  ot->idname = "NODE_OT_select_all";

  /* api callbacks */
  ot->exec = node_select_all_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  WM_operator_properties_select_all(ot);
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked To Operator
 * \{ */

static int node_select_linked_to_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &node_tree = *snode.edittree;

  node_tree.ensure_topology_cache();

  VectorSet<bNode *> initial_selection = get_selected_nodes(node_tree);

  for (bNode *node : initial_selection) {
    for (bNodeSocket *output_socket : node->output_sockets()) {
      if (!output_socket->is_available()) {
        continue;
      }
      for (bNodeSocket *input_socket : output_socket->directly_linked_sockets()) {
        if (!input_socket->is_available()) {
          continue;
        }
        nodeSetSelected(&input_socket->owner_node(), true);
      }
    }
  }

  node_sort(node_tree);

  WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
  return OPERATOR_FINISHED;
}

void NODE_OT_select_linked_to(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked To";
  ot->description = "Select nodes linked to the selected ones";
  ot->idname = "NODE_OT_select_linked_to";

  /* api callbacks */
  ot->exec = node_select_linked_to_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked From Operator
 * \{ */

static int node_select_linked_from_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &node_tree = *snode.edittree;

  node_tree.ensure_topology_cache();

  VectorSet<bNode *> initial_selection = get_selected_nodes(node_tree);

  for (bNode *node : initial_selection) {
    for (bNodeSocket *input_socket : node->input_sockets()) {
      if (!input_socket->is_available()) {
        continue;
      }
      for (bNodeSocket *output_socket : input_socket->directly_linked_sockets()) {
        if (!output_socket->is_available()) {
          continue;
        }
        nodeSetSelected(&output_socket->owner_node(), true);
      }
    }
  }

  node_sort(node_tree);

  WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
  return OPERATOR_FINISHED;
}

void NODE_OT_select_linked_from(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked From";
  ot->description = "Select nodes linked from the selected ones";
  ot->idname = "NODE_OT_select_linked_from";

  /* api callbacks */
  ot->exec = node_select_linked_from_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Same Type Step Operator
 * \{ */

static bool nodes_are_same_type_for_select(const bNode &a, const bNode &b)
{
  return a.type == b.type;
}

static int node_select_same_type_step_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  const bool prev = RNA_boolean_get(op->ptr, "prev");
  bNode &active_node = *nodeGetActive(snode->edittree);

  bNodeTree &node_tree = *snode->edittree;
  node_tree.ensure_topology_cache();
  if (node_tree.all_nodes().size() == 1) {
    return OPERATOR_CANCELLED;
  }

  const Span<const bNode *> toposort = node_tree.toposort_left_to_right();
  const int index = toposort.first_index(&active_node);

  int new_index = index;
  while (true) {
    new_index += (prev ? -1 : 1);
    if (!toposort.index_range().contains(new_index)) {
      return OPERATOR_CANCELLED;
    }
    if (nodes_are_same_type_for_select(*toposort[new_index], active_node)) {
      break;
    }
  }

  bNode *new_active_node = node_tree.all_nodes()[toposort[new_index]->index()];
  if (new_active_node == &active_node) {
    return OPERATOR_CANCELLED;
  }

  node_select_single(*C, *new_active_node);

  if (!BLI_rctf_inside_rctf(&region->v2d.cur, &new_active_node->runtime->totr)) {
    const int smooth_viewtx = WM_operator_smooth_viewtx_get(op);
    space_node_view_flag(*C, *snode, *region, NODE_SELECT, smooth_viewtx);
  }

  return OPERATOR_FINISHED;
}

void NODE_OT_select_same_type_step(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Activate Same Type Next/Prev";
  ot->description = "Activate and view same node type, step by step";
  ot->idname = "NODE_OT_select_same_type_step";

  /* api callbacks */
  ot->exec = node_select_same_type_step_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  RNA_def_boolean(ot->srna, "prev", false, "Previous", "");
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Find Node by Name Operator
 * \{ */

static void node_find_create_label(const bNode *node, char *str, int str_maxncpy)
{
  if (node->label[0]) {
    BLI_snprintf(str, str_maxncpy, "%s (%s)", node->name, node->label);
  }
  else {
    BLI_strncpy(str, node->name, str_maxncpy);
  }
}

/* Generic search invoke. */
static void node_find_update_fn(const bContext *C,
                                void * /*arg*/,
                                const char *str,
                                uiSearchItems *items,
                                const bool /*is_first*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  StringSearch *search = BLI_string_search_new();

  for (bNode *node : snode->edittree->all_nodes()) {
    char name[256];
    node_find_create_label(node, name, ARRAY_SIZE(name));
    BLI_string_search_add(search, name, node, 0);
  }

  bNode **filtered_nodes;
  int filtered_amount = BLI_string_search_query(search, str, (void ***)&filtered_nodes);

  for (int i = 0; i < filtered_amount; i++) {
    bNode *node = filtered_nodes[i];
    char name[256];
    node_find_create_label(node, name, ARRAY_SIZE(name));
    if (!UI_search_item_add(items, name, node, ICON_NONE, 0, 0)) {
      break;
    }
  }

  MEM_freeN(filtered_nodes);
  BLI_string_search_free(search);
}

static void node_find_exec_fn(bContext *C, void * /*arg1*/, void *arg2)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNode *active = (bNode *)arg2;

  if (active) {
    ARegion *region = CTX_wm_region(C);
    node_select_single(*C, *active);

    if (!BLI_rctf_inside_rctf(&region->v2d.cur, &active->runtime->totr)) {
      space_node_view_flag(*C, *snode, *region, NODE_SELECT, U.smooth_viewtx);
    }
  }
}

static uiBlock *node_find_menu(bContext *C, ARegion *region, void *arg_op)
{
  static char search[256] = "";
  uiBlock *block;
  uiBut *but;
  wmOperator *op = (wmOperator *)arg_op;

  block = UI_block_begin(C, region, "_popup", UI_EMBOSS);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  but = uiDefSearchBut(block,
                       search,
                       0,
                       ICON_VIEWZOOM,
                       sizeof(search),
                       10,
                       10,
                       UI_searchbox_size_x(),
                       UI_UNIT_Y,
                       0,
                       0,
                       "");
  UI_but_func_search_set(
      but, nullptr, node_find_update_fn, op->type, false, nullptr, node_find_exec_fn, nullptr);
  UI_but_flag_enable(but, UI_BUT_ACTIVATE_ON_INIT);

  /* Fake button holds space for search items. */
  uiDefBut(block,
           UI_BTYPE_LABEL,
           0,
           "",
           10,
           10 - UI_searchbox_size_y(),
           UI_searchbox_size_x(),
           UI_searchbox_size_y(),
           nullptr,
           0,
           0,
           0,
           0,
           nullptr);

  /* Move it downwards, mouse over button. */
  std::array<int, 2> bounds_offset = {0, -UI_UNIT_Y};
  UI_block_bounds_set_popup(block, 0.3f * U.widget_unit, bounds_offset.data());

  return block;
}

static int node_find_node_invoke(bContext *C, wmOperator *op, const wmEvent * /*event*/)
{
  UI_popup_block_invoke(C, node_find_menu, op, nullptr);
  return OPERATOR_CANCELLED;
}

void NODE_OT_find_node(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Find Node";
  ot->description = "Search for a node by name and focus and select it";
  ot->idname = "NODE_OT_find_node";

  /* api callbacks */
  ot->invoke = node_find_node_invoke;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender::ed::space_node
