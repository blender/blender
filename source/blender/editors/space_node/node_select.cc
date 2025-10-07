/* SPDX-FileCopyrightText: 2008 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spnode
 */

#include <array>
#include <cstdlib>
#include <fmt/format.h>

#include "DNA_collection_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_node_types.h"
#include "DNA_object_types.h"
#include "DNA_windowmanager_types.h"

#include "BLI_lasso_2d.hh"
#include "BLI_listbase.h"
#include "BLI_math_vector.h"
#include "BLI_rect.h"
#include "BLI_resource_scope.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_utildefines.h"

#include "BKE_context.hh"
#include "BKE_idtype.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_screen.hh"
#include "BKE_viewer_path.hh"
#include "BKE_workspace.hh"

#include "ED_node.hh" /* own include */
#include "ED_screen.hh"
#include "ED_select_utils.hh"
#include "ED_view3d.hh"
#include "ED_viewer_path.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "WM_api.hh"
#include "WM_types.hh"

#include "UI_resources.hh"
#include "UI_string_search.hh"
#include "UI_view2d.hh"

#include "DEG_depsgraph.hh"

#include "BLT_translation.hh"

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

rctf node_frame_rect_inside(const SpaceNode &snode, const bNode &node)
{
  const float margin = 4.0f * NODE_RESIZE_MARGIN * math::max(snode.runtime->aspect, 1.0f);
  rctf frame_inside = {
      node.runtime->draw_bounds.xmin,
      node.runtime->draw_bounds.xmax,
      node.runtime->draw_bounds.ymin,
      node.runtime->draw_bounds.ymax,
  };

  BLI_rctf_pad(&frame_inside, -margin, -margin);

  return frame_inside;
}

bool node_or_socket_isect_event(const bContext &C, const wmEvent &event)
{
  return is_event_over_node_or_socket(C, event);
}

static bool node_frame_select_isect_mouse(const SpaceNode &snode,
                                          const bNode &node,
                                          const float2 &mouse)
{
  /* Frame nodes are selectable by their borders (including their whole rect - as for other nodes -
   * would prevent e.g. box selection of nodes inside that frame). */
  const rctf frame_inside = node_frame_rect_inside(snode, node);
  if (BLI_rctf_isect_pt(&node.runtime->draw_bounds, mouse.x, mouse.y) &&
      !BLI_rctf_isect_pt(&frame_inside, mouse.x, mouse.y))
  {
    return true;
  }

  return false;
}

bNode *node_under_mouse_get(const SpaceNode &snode, const float2 mouse)
{
  for (bNode *node : tree_draw_order_calc_nodes_reversed(*snode.edittree)) {
    switch (node->type_legacy) {
      case NODE_FRAME: {
        if (node_frame_select_isect_mouse(snode, *node, mouse)) {
          return node;
        }
        break;
      }
      default: {
        if (BLI_rctf_isect_pt(&node->runtime->draw_bounds, int(mouse.x), int(mouse.y))) {
          return node;
        }
        break;
      }
    }
  }
  return nullptr;
}

static bool is_position_over_node_or_socket(SpaceNode &snode, ARegion &region, const float2 &mouse)
{
  if (node_under_mouse_get(snode, mouse)) {
    return true;
  }
  if (node_find_indicated_socket(snode, region, mouse, SOCK_IN | SOCK_OUT)) {
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
  return is_position_over_node_or_socket(snode, region, mouse);
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

bool node_deselect_all(bNodeTree &node_tree)
{
  bool changed = false;
  for (bNode *node : node_tree.all_nodes()) {
    changed |= bke::node_set_selected(*node, false);
  }
  return changed;
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
  node_tree.ensure_topology_cache();
  for (const bke::bNodeZoneType *zone_type : bke::all_zone_types()) {
    for (bNode *input_node : node_tree.nodes_by_type(zone_type->input_idname)) {
      if (bNode *output_node = zone_type->get_corresponding_output(node_tree, *input_node)) {
        if (input_node->flag & NODE_SELECT) {
          output_node->flag |= NODE_SELECT;
        }
        if (output_node->flag & NODE_SELECT) {
          input_node->flag |= NODE_SELECT;
        }
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
      if (node->type_legacy == node_act.type_legacy) {
        bke::node_set_selected(*node, true);
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
        bke::node_set_selected(*node, true);
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
      bke::node_set_selected(*node, true);
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

static wmOperatorStatus node_select_grouped_exec(bContext *C, wmOperator *op)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &node_tree = *snode.edittree;
  bNode *node_act = bke::node_get_active(*snode.edittree);

  if (node_act == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bool changed = false;
  const bool extend = RNA_boolean_get(op->ptr, "extend");
  const int type = RNA_enum_get(op->ptr, "type");

  if (!extend) {
    node_deselect_all(node_tree);
  }
  bke::node_set_selected(*node_act, true);

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
    tree_draw_order_update(node_tree);
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

  /* API callbacks. */
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
      bke::node_set_selected(*node_iter, false);
    }
  }
  bke::node_set_selected(node, true);

  ED_node_set_active(bmain, &snode, &node_tree, &node, &active_texture_changed);
  ED_node_set_active_viewer_key(&snode);

  tree_draw_order_update(node_tree);
  if (active_texture_changed && has_workbench_in_texture_color(wm, scene, ob)) {
    DEG_id_tag_update(&node_tree.id, ID_RECALC_SYNC_TO_EVAL);
  }

  WM_event_add_notifier(&C, NC_NODE | NA_SELECTED, nullptr);
}

static const bNodeSocket *find_socket_at_mouse_y(const Span<const bNodeSocket *> sockets,
                                                 const float view_y)
{
  const bNodeSocket *best_socket = nullptr;
  float best_distance = FLT_MAX;
  for (const bNodeSocket *socket : sockets) {
    if (!socket->is_icon_visible()) {
      continue;
    }
    const float socket_y = socket->runtime->location.y;
    const float distance = math::distance(socket_y, view_y);
    if (distance < best_distance) {
      best_distance = distance;
      best_socket = socket;
    }
  }
  return best_socket;
}

static void activate_interface_socket(bNodeTree &tree, bNodeTreeInterfaceSocket &io_socket)
{
  bNodeTreeInterfacePanel &io_panel = *tree.tree_interface.find_item_parent(io_socket.item, true);
  bNodeTreeInterfaceItem *item_to_activate = nullptr;
  if (io_panel.header_toggle_socket() == &io_socket) {
    item_to_activate = &io_panel.item;
  }
  else {
    item_to_activate = &io_socket.item;
  }
  tree.tree_interface.active_item_set(item_to_activate);
}

static void handle_group_input_node_selection(bNodeTree &tree,
                                              const bNode &group_input_node,
                                              const float2 &cursor)
{

  tree.ensure_topology_cache();
  tree.ensure_interface_cache();
  const bNodeSocket *indicated_socket = find_socket_at_mouse_y(
      group_input_node.output_sockets().drop_back(1), cursor.y);
  if (!indicated_socket) {
    return;
  }
  const int group_input_i = indicated_socket->index();
  bNodeTreeInterfaceSocket &io_socket = *tree.interface_inputs()[group_input_i];
  activate_interface_socket(tree, io_socket);
}

static void handle_group_output_node_selection(bNodeTree &tree,
                                               const bNode &group_output_node,
                                               const float2 &cursor)
{
  tree.ensure_topology_cache();
  tree.ensure_interface_cache();
  const bNodeSocket *indicated_socket = find_socket_at_mouse_y(
      group_output_node.input_sockets().drop_back(1), cursor.y);
  if (!indicated_socket) {
    return;
  }
  const int group_output_i = indicated_socket->index();
  bNodeTreeInterfaceSocket &io_socket = *tree.interface_outputs()[group_output_i];
  activate_interface_socket(tree, io_socket);
}

static bool node_mouse_select(bContext *C,
                              wmOperator *op,
                              const int2 mval,
                              const SelectPick_Params &params)
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
  const bool socket_select = (params.sel_op == SEL_OP_XOR) ||
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
    const bool extend = (params.sel_op == SEL_OP_XOR);
    sock = node_find_indicated_socket(snode, region, cursor, SOCK_IN);
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
      sock = node_find_indicated_socket(snode, region, cursor, SOCK_OUT);
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
    node = node_under_mouse_get(snode, cursor);
    found = (node != nullptr);
    node_was_selected = node && (node->flag & SELECT);

    if (params.sel_op == SEL_OP_SET) {
      if ((found && params.select_passthrough) && (node->flag & SELECT)) {
        found = false;
      }
      else if (found || params.deselect_all) {
        /* Deselect everything. */
        changed = node_deselect_all(node_tree);
      }
    }

    if (found) {
      switch (params.sel_op) {
        case SEL_OP_ADD:
          bke::node_set_selected(*node, true);
          break;
        case SEL_OP_SUB:
          bke::node_set_selected(*node, false);
          break;
        case SEL_OP_XOR: {
          /* Check active so clicking on an inactive node activates it. */
          bool is_selected = (node->flag & NODE_SELECT) && (node->flag & NODE_ACTIVE);
          bke::node_set_selected(*node, !is_selected);
          break;
        }
        case SEL_OP_SET:
          bke::node_set_selected(*node, true);
          break;
        case SEL_OP_AND:
          /* Doesn't make sense for picking. */
          BLI_assert_unreachable();
          break;
      }

      if (node->is_group_input()) {
        handle_group_input_node_selection(node_tree, *node, cursor);
      }
      if (node->is_group_output()) {
        handle_group_output_node_selection(node_tree, *node, cursor);
      }

      changed = true;
    }
  }

  if (RNA_boolean_get(op->ptr, "clear_viewer")) {
    if (node == nullptr) {
      /* Disable existing active viewer. */
      WorkSpace *workspace = CTX_wm_workspace(C);
      if (const std::optional<viewer_path::ViewerPathForGeometryNodesViewer> parsed_path =
              viewer_path::parse_geometry_nodes_viewer(workspace->viewer_path))
      {
        /* The object needs to be reevaluated, because the viewer path is changed which means that
         * the object may generate different viewer geometry as a side effect. */
        Depsgraph *depsgraph = CTX_data_depsgraph_pointer(C);
        DEG_id_tag_update_for_side_effect_request(
            depsgraph, &parsed_path->object->id, ID_RECALC_GEOMETRY);
      }
      BKE_viewer_path_clear(&workspace->viewer_path);
      WM_event_add_notifier(C, NC_VIEWER_PATH, nullptr);
    }
  }

  if (!(changed || found)) {
    return false;
  }

  bool active_texture_changed = false;
  bool viewer_node_changed = false;
  if ((node != nullptr) && (node_was_selected == false || params.select_passthrough == false)) {
    viewer_node_changed = (node->flag & NODE_DO_OUTPUT) == 0 &&
                          node->type_legacy == GEO_NODE_VIEWER;
    ED_node_set_active(&bmain, &snode, snode.edittree, node, &active_texture_changed);
  }
  else if (node != nullptr && node->type_legacy == GEO_NODE_VIEWER) {
    viewer_path::activate_geometry_node(bmain, snode, *node);
  }
  ED_node_set_active_viewer_key(&snode);
  tree_draw_order_update(node_tree);
  if ((active_texture_changed && has_workbench_in_texture_color(wm, scene, ob)) ||
      viewer_node_changed)
  {
    DEG_id_tag_update(&snode.edittree->id, ID_RECALC_SYNC_TO_EVAL);
  }

  WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
  WM_event_add_notifier(C, NC_NODE | ND_NODE_GIZMO, nullptr);

  BKE_main_ensure_invariants(bmain, node_tree.id);

  return true;
}

static wmOperatorStatus node_select_exec(bContext *C, wmOperator *op)
{
  /* Get settings from RNA properties for operator. */
  int2 mval;
  RNA_int_get_array(op->ptr, "location", mval);

  const SelectPick_Params params = ED_select_pick_params_from_operator(op->ptr);

  /* Perform the selection. */
  const bool changed = node_mouse_select(C, op, mval, params);

  if (changed) {
    return OPERATOR_PASS_THROUGH | OPERATOR_FINISHED;
  }
  /* Nothing selected, just pass through. */
  return OPERATOR_PASS_THROUGH | OPERATOR_CANCELLED;
}

static wmOperatorStatus node_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  RNA_int_set_array(op->ptr, "location", event->mval);

  const wmOperatorStatus retval = node_select_exec(C, op);

  return WM_operator_flag_only_pass_through_on_press(retval, event);
}

void NODE_OT_select(wmOperatorType *ot)
{
  PropertyRNA *prop;

  /* identifiers */
  ot->name = "Select";
  ot->idname = "NODE_OT_select";
  ot->description = "Select the node under the cursor";

  /* API callbacks. */
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

static wmOperatorStatus node_box_select_exec(bContext *C, wmOperator *op)
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

    switch (node->type_legacy) {
      case NODE_FRAME: {
        /* Frame nodes are selectable by their borders (including their whole rect - as for other
         * nodes - would prevent selection of other nodes inside that frame. */
        const rctf frame_inside = node_frame_rect_inside(snode, *node);
        if (BLI_rctf_isect(&rectf, &node->runtime->draw_bounds, nullptr) &&
            !BLI_rctf_inside_rctf(&frame_inside, &rectf))
        {
          bke::node_set_selected(*node, select);
          is_inside = true;
        }
        break;
      }
      default: {
        is_inside = BLI_rctf_isect(&rectf, &node->runtime->draw_bounds, nullptr);
        break;
      }
    }

    if (is_inside) {
      bke::node_set_selected(*node, select);
    }
  }

  tree_draw_order_update(node_tree);

  WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
  WM_event_add_notifier(C, NC_NODE | ND_NODE_GIZMO, nullptr);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus node_box_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
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

  /* API callbacks. */
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

static wmOperatorStatus node_circleselect_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  bNodeTree &node_tree = *snode->edittree;

  int x, y, radius;
  float2 offset;

  float zoom = float(BLI_rcti_size_x(&region->winrct)) / BLI_rctf_size_x(&region->v2d.cur);

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
    switch (node->type_legacy) {
      case NODE_FRAME: {
        /* Frame nodes are selectable by their borders (including their whole rect - as for other
         * nodes - would prevent selection of _only_ other nodes inside that frame. */
        rctf frame_inside = node_frame_rect_inside(*snode, *node);
        const float radius_adjusted = float(radius) / zoom;
        BLI_rctf_pad(&frame_inside, -2.0f * radius_adjusted, -2.0f * radius_adjusted);
        if (BLI_rctf_isect_circle(&node->runtime->draw_bounds, offset, radius_adjusted) &&
            !BLI_rctf_isect_circle(&frame_inside, offset, radius_adjusted))
        {
          bke::node_set_selected(*node, select);
        }
        break;
      }
      default: {
        if (BLI_rctf_isect_circle(&node->runtime->draw_bounds, offset, radius / zoom)) {
          bke::node_set_selected(*node, select);
        }
        break;
      }
    }
  }

  WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
  WM_event_add_notifier(C, NC_NODE | ND_NODE_GIZMO, nullptr);

  return OPERATOR_FINISHED;
}

void NODE_OT_select_circle(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Circle Select";
  ot->idname = "NODE_OT_select_circle";
  ot->description = "Use circle selection to select nodes";

  /* API callbacks. */
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

static wmOperatorStatus node_lasso_select_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const bool tweak = RNA_boolean_get(op->ptr, "tweak");

  if (tweak && is_event_over_node_or_socket(*C, *event)) {
    return OPERATOR_CANCELLED | OPERATOR_PASS_THROUGH;
  }

  return WM_gesture_lasso_invoke(C, op, event);
}

static bool do_lasso_select_node(bContext *C, const Span<int2> mcoords, eSelectOp sel_op)
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
  BLI_lasso_boundbox(&rect, mcoords);

  for (bNode *node : node_tree.all_nodes()) {
    if (select && (node->flag & NODE_SELECT)) {
      continue;
    }

    switch (node->type_legacy) {
      case NODE_FRAME: {
        /* Frame nodes are selectable by their borders (including their whole rect - as for other
         * nodes - would prevent selection of other nodes inside that frame. */
        rctf rectf;
        BLI_rctf_rcti_copy(&rectf, &rect);
        UI_view2d_region_to_view_rctf(&region->v2d, &rectf, &rectf);
        const rctf frame_inside = node_frame_rect_inside(*snode, *node);
        if (BLI_rctf_isect(&rectf, &node->runtime->draw_bounds, nullptr) &&
            !BLI_rctf_inside_rctf(&frame_inside, &rectf))
        {
          bke::node_set_selected(*node, select);
          changed = true;
        }
        break;
      }
      default: {
        int2 screen_co;
        const float2 center = {BLI_rctf_cent_x(&node->runtime->draw_bounds),
                               BLI_rctf_cent_y(&node->runtime->draw_bounds)};

        /* marker in screen coords */
        if (UI_view2d_view_to_region_clip(
                &region->v2d, center.x, center.y, &screen_co.x, &screen_co.y) &&
            BLI_rcti_isect_pt(&rect, screen_co.x, screen_co.y) &&
            BLI_lasso_is_point_inside(mcoords, screen_co.x, screen_co.y, INT_MAX))
        {
          bke::node_set_selected(*node, select);
          changed = true;
        }
        break;
      }
    }
  }

  if (changed) {
    WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
    WM_event_add_notifier(C, NC_NODE | ND_NODE_GIZMO, nullptr);
  }

  return changed;
}

static wmOperatorStatus node_lasso_select_exec(bContext *C, wmOperator *op)
{
  const Array<int2> mcoords = WM_gesture_lasso_path_to_array(C, op);

  if (mcoords.is_empty()) {
    return OPERATOR_PASS_THROUGH;
  }

  const eSelectOp sel_op = (eSelectOp)RNA_enum_get(op->ptr, "mode");

  do_lasso_select_node(C, mcoords, sel_op);

  return OPERATOR_FINISHED;
}

void NODE_OT_select_lasso(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Lasso Select";
  ot->description = "Select nodes using lasso selection";
  ot->idname = "NODE_OT_select_lasso";

  /* API callbacks. */
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

static wmOperatorStatus node_select_all_exec(bContext *C, wmOperator *op)
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
        bke::node_set_selected(*node, true);
      }
      break;
    case SEL_DESELECT:
      node_deselect_all(node_tree);
      break;
    case SEL_INVERT:
      for (bNode *node : node_tree.all_nodes()) {
        bke::node_set_selected(*node, !(node->flag & SELECT));
      }
      break;
  }

  tree_draw_order_update(node_tree);

  WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
  WM_event_add_notifier(C, NC_NODE | ND_NODE_GIZMO, nullptr);
  return OPERATOR_FINISHED;
}

void NODE_OT_select_all(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "(De)select All";
  ot->description = "(De)select all nodes";
  ot->idname = "NODE_OT_select_all";

  /* API callbacks. */
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

static wmOperatorStatus node_select_linked_to_exec(bContext *C, wmOperator * /*op*/)
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
        bke::node_set_selected(input_socket->owner_node(), true);
      }
    }
  }

  tree_draw_order_update(node_tree);

  WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
  return OPERATOR_FINISHED;
}

void NODE_OT_select_linked_to(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked To";
  ot->description = "Select nodes linked to the selected ones";
  ot->idname = "NODE_OT_select_linked_to";

  /* API callbacks. */
  ot->exec = node_select_linked_to_exec;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Select Linked From Operator
 * \{ */

static wmOperatorStatus node_select_linked_from_exec(bContext *C, wmOperator * /*op*/)
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
        bke::node_set_selected(output_socket->owner_node(), true);
      }
    }
  }

  tree_draw_order_update(node_tree);

  WM_event_add_notifier(C, NC_NODE | NA_SELECTED, nullptr);
  return OPERATOR_FINISHED;
}

void NODE_OT_select_linked_from(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Select Linked From";
  ot->description = "Select nodes linked from the selected ones";
  ot->idname = "NODE_OT_select_linked_from";

  /* API callbacks. */
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
  return a.type_legacy == b.type_legacy;
}

static wmOperatorStatus node_select_same_type_step_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  ARegion *region = CTX_wm_region(C);
  const bool prev = RNA_boolean_get(op->ptr, "prev");
  bNode *active_node = bke::node_get_active(*snode->edittree);

  if (active_node == nullptr) {
    return OPERATOR_CANCELLED;
  }

  bNodeTree &node_tree = *snode->edittree;
  node_tree.ensure_topology_cache();
  if (node_tree.all_nodes().size() == 1) {
    return OPERATOR_CANCELLED;
  }

  const Span<const bNode *> toposort = node_tree.toposort_left_to_right();
  const int index = toposort.first_index(active_node);

  int new_index = index;
  while (true) {
    new_index += (prev ? -1 : 1);
    if (!toposort.index_range().contains(new_index)) {
      return OPERATOR_CANCELLED;
    }
    if (nodes_are_same_type_for_select(*toposort[new_index], *active_node)) {
      break;
    }
  }

  bNode *new_active_node = node_tree.all_nodes()[toposort[new_index]->index()];
  if (new_active_node == active_node) {
    return OPERATOR_CANCELLED;
  }

  node_select_single(*C, *new_active_node);

  if (!BLI_rctf_inside_rctf(&region->v2d.cur, &new_active_node->runtime->draw_bounds)) {
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

  /* API callbacks. */
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

static std::string node_find_create_node_label(const bNodeTree &ntree, const bNode &node)
{
  std::string label = bke::node_label(ntree, node);
  if (label == node.name) {
    return label;
  }
  return fmt::format("{} ({})", label, node.name);
}

static std::string node_find_create_group_input_label(const bNode &node, const bNodeSocket &socket)
{
  return fmt::format("{}: \"{}\" ({})", TIP_("Input"), socket.name, node.name);
}

static std::string node_find_create_string_value(const bNode &node, const StringRef str)
{
  return fmt::format("{}: \"{}\" ({})", TIP_("String"), str, node.name);
}

static std::string node_find_create_data_block_value(const bNode &node, const ID &id)
{
  const IDTypeInfo *type = BKE_idtype_get_info_from_id(&id);
  BLI_assert(type);
  StringRef type_name = TIP_(type->name);
  if (GS(id.name) == ID_NT) {
    type_name = TIP_("Node Group");
  }
  return fmt::format("{}: \"{}\" ({})", type_name, BKE_id_name(id), node.name);
}

/* Generic search invoke. */
static void node_find_update_fn(const bContext *C,
                                void * /*arg*/,
                                const char *str,
                                uiSearchItems *items,
                                const bool /*is_first*/)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  struct Item {
    bNode *node;
    std::string search_str;
  };

  ui::string_search::StringSearch<Item> search;
  blender::ResourceScope scope;

  auto add_data_block_item = [&](bNode &node, const ID *id) {
    if (!id) {
      return;
    }
    const StringRef search_str = scope.add_value(node_find_create_data_block_value(node, *id));
    search.add(search_str, &scope.construct<Item>(Item{&node, search_str}));
  };

  const bNodeTree &ntree = *snode->edittree;
  ntree.ensure_topology_cache();
  for (bNode *node : snode->edittree->all_nodes()) {
    const StringRef name = scope.add_value(node_find_create_node_label(ntree, *node));
    search.add(name, &scope.construct<Item>(Item{node, name}));

    if (node->is_type("FunctionNodeInputString")) {
      const auto *storage = static_cast<const NodeInputString *>(node->storage);
      const StringRef value_str = storage->string;
      if (!value_str.is_empty()) {
        const StringRef search_str = scope.add_value(
            node_find_create_string_value(*node, value_str));
        search.add(search_str, &scope.construct<Item>(Item{node, search_str}));
      }
    }
    if (node->is_group_input()) {
      for (const bNodeSocket *socket : node->output_sockets().drop_back(1)) {
        if (!socket->is_directly_linked()) {
          continue;
        }
        const StringRef search_str = scope.add_value(
            node_find_create_group_input_label(*node, *socket));
        search.add(search_str, &scope.construct<Item>(Item{node, search_str}));
      }
    }
    if (node->id) {
      /* Avoid showing referenced node group data-blocks twice. */
      const bool skip_data_block =
          node->is_group() &&
          StringRef(bke::node_label(ntree, *node)).find(BKE_id_name(*node->id)) !=
              StringRef::not_found;
      if (!skip_data_block) {
        add_data_block_item(*node, node->id);
      }
    }

    for (const bNodeSocket *socket : node->input_sockets()) {
      switch (socket->type) {
        case SOCK_STRING: {
          if (socket->is_logically_linked()) {
            continue;
          }
          const bNodeSocketValueString *value =
              socket->default_value_typed<bNodeSocketValueString>();
          const StringRef value_str = value->value;
          if (!value_str.is_empty()) {
            const StringRef search_str = scope.add_value(
                node_find_create_string_value(*node, value_str));
            search.add(search_str, &scope.construct<Item>(Item{node, search_str}));
          }
          break;
        }
        case SOCK_OBJECT: {
          add_data_block_item(
              *node, id_cast<ID *>(socket->default_value_typed<bNodeSocketValueObject>()->value));
          break;
        }
        case SOCK_MATERIAL: {
          add_data_block_item(
              *node,
              id_cast<ID *>(socket->default_value_typed<bNodeSocketValueMaterial>()->value));
          break;
        }
        case SOCK_COLLECTION: {
          add_data_block_item(
              *node,
              id_cast<ID *>(socket->default_value_typed<bNodeSocketValueCollection>()->value));
          break;
        }
        case SOCK_IMAGE: {
          add_data_block_item(
              *node, id_cast<ID *>(socket->default_value_typed<bNodeSocketValueImage>()->value));
          break;
        }
      }
    }
  }

  const Vector<Item *> filtered_items = search.query(str);
  for (const Item *item : filtered_items) {
    if (!UI_search_item_add(items, item->search_str, item->node, ICON_NONE, 0, 0)) {
      break;
    }
  }
}

static void node_find_exec_fn(bContext *C, void * /*arg1*/, void *arg2)
{
  SpaceNode *snode = CTX_wm_space_node(C);
  bNode *active = (bNode *)arg2;

  if (active) {
    ARegion *region = CTX_wm_region(C);
    node_select_single(*C, *active);

    if (!BLI_rctf_inside_rctf(&region->v2d.cur, &active->runtime->draw_bounds)) {
      space_node_view_flag(*C, *snode, *region, NODE_SELECT, U.smooth_viewtx);
    }
  }
}

static uiBlock *node_find_menu(bContext *C, ARegion *region, void *arg_optype)
{
  static char search[256] = "";
  uiBlock *block;
  uiBut *but;
  wmOperatorType *optype = (wmOperatorType *)arg_optype;

  block = UI_block_begin(C, region, "_popup", ui::EmbossType::Emboss);
  UI_block_flag_enable(block, UI_BLOCK_LOOP | UI_BLOCK_MOVEMOUSE_QUIT | UI_BLOCK_SEARCH_MENU);
  UI_block_theme_style_set(block, UI_BLOCK_THEME_STYLE_POPUP);

  const int box_width = UI_searchbox_size_x_guess(C, node_find_update_fn, nullptr);

  but = uiDefSearchBut(
      block, search, 0, ICON_VIEWZOOM, sizeof(search), 0, 0, box_width, UI_UNIT_Y, "");
  UI_but_func_search_set(
      but, nullptr, node_find_update_fn, optype, false, nullptr, node_find_exec_fn, nullptr);
  UI_but_flag_enable(but, UI_BUT_ACTIVATE_ON_INIT);

  /* Fake button holds space for search items. */
  const int height = UI_searchbox_size_y() - UI_SEARCHBOX_BOUNDS;
  uiDefBut(
      block, ButType::Label, 0, "", 0, -height, box_width, height, nullptr, 0, 0, std::nullopt);

  /* Move it downwards, mouse over button. */
  std::array<int, 2> bounds_offset = {0, -UI_UNIT_Y};
  UI_block_bounds_set_popup(block, UI_SEARCHBOX_BOUNDS, bounds_offset.data());

  return block;
}

static wmOperatorStatus node_find_node_invoke(bContext *C,
                                              wmOperator *op,
                                              const wmEvent * /*event*/)
{
  UI_popup_block_invoke(C, node_find_menu, op->type, nullptr);
  return OPERATOR_CANCELLED;
}

void NODE_OT_find_node(wmOperatorType *ot)
{
  /* identifiers */
  ot->name = "Find Node";
  ot->description = "Search for a node by name and focus and select it";
  ot->idname = "NODE_OT_find_node";

  /* API callbacks. */
  ot->invoke = node_find_node_invoke;
  ot->poll = ED_operator_node_active;

  /* flags */
  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

}  // namespace blender::ed::space_node
