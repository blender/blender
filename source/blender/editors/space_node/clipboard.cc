/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_space_types.h"

#include "BKE_context.h"
#include "BKE_global.h"
#include "BKE_lib_id.h"
#include "BKE_main.h"
#include "BKE_node.h"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.h"
#include "BKE_report.h"

#include "ED_node.h"
#include "ED_node.hh"
#include "ED_render.h"
#include "ED_screen.h"

#include "NOD_socket.h"

#include "RNA_access.h"
#include "RNA_define.h"

#include "DEG_depsgraph_build.h"

#include "node_intern.hh"

namespace blender::ed::space_node {

struct NodeClipboardItem {
  bNode *node;
  /**
   * The offset and size of the node from when it was drawn. Stored here since it doesn't remain
   * valid for the nodes in the clipboard.
   */
  rctf draw_rect;

  /* Extra info to validate the node on creation. Otherwise we may reference missing data. */
  ID *id;
  std::string id_name;
  std::string library_name;
};

struct NodeClipboard {
  Vector<NodeClipboardItem> nodes;
  Vector<bNodeLink> links;

  void clear()
  {
    for (NodeClipboardItem &item : this->nodes) {
      bke::node_free_node(nullptr, item.node);
    }
    this->nodes.clear_and_shrink();
    this->links.clear_and_shrink();
  }

  /**
   * Replace node IDs that are no longer available in the current file. Return false when one or
   * more IDs are lost.
   */
  bool validate()
  {
    bool ok = true;

    for (NodeClipboardItem &item : this->nodes) {
      bNode &node = *item.node;
      /* Reassign each loop since we may clear, open a new file where the ID is valid, and paste
       * again. */
      node.id = item.id;

      if (node.id) {
        const ListBase *lb = which_libbase(G_MAIN, GS(item.id_name.c_str()));
        if (BLI_findindex(lb, item.id) == -1) {
          /* May assign null. */
          node.id = static_cast<ID *>(
              BLI_findstring(lb, item.id_name.c_str() + 2, offsetof(ID, name) + 2));
          if (!node.id) {
            ok = false;
          }
        }
      }
    }

    return ok;
  }

  void add_node(const bNode &node,
                Map<const bNode *, bNode *> &node_map,
                Map<const bNodeSocket *, bNodeSocket *> &socket_map)
  {
    /* No ID reference-counting, this node is virtual,
     * detached from any actual Blender data currently. */
    bNode *new_node = bke::node_copy_with_mapping(
        nullptr, node, LIB_ID_CREATE_NO_USER_REFCOUNT | LIB_ID_CREATE_NO_MAIN, false, socket_map);
    node_map.add_new(&node, new_node);

    NodeClipboardItem item;
    item.draw_rect = node.runtime->totr;
    item.node = new_node;
    item.id = new_node->id;
    if (item.id) {
      item.id_name = new_node->id->name;
      if (ID_IS_LINKED(new_node->id)) {
        item.library_name = new_node->id->lib->filepath_abs;
      }
    }
    this->nodes.append(std::move(item));
  }
};

static NodeClipboard &get_node_clipboard()
{
  static NodeClipboard clipboard;
  return clipboard;
}

/* -------------------------------------------------------------------- */
/** \name Copy
 * \{ */

static int node_clipboard_copy_exec(bContext *C, wmOperator * /*op*/)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &tree = *snode.edittree;
  NodeClipboard &clipboard = get_node_clipboard();

  clipboard.clear();

  Map<const bNode *, bNode *> node_map;
  Map<const bNodeSocket *, bNodeSocket *> socket_map;

  for (const bNode *node : tree.all_nodes()) {
    if (node->flag & SELECT) {
      clipboard.add_node(*node, node_map, socket_map);
    }
  }

  for (bNode *new_node : node_map.values()) {
    /* Parent pointer must be redirected to new node or detached if parent is not copied. */
    if (new_node->parent) {
      if (node_map.contains(new_node->parent)) {
        new_node->parent = node_map.lookup(new_node->parent);
      }
      else {
        nodeDetachNode(&tree, new_node);
      }
    }
  }

  /* Copy links between selected nodes. */
  LISTBASE_FOREACH (bNodeLink *, link, &tree.links) {
    BLI_assert(link->tonode);
    BLI_assert(link->fromnode);
    if (link->tonode->flag & NODE_SELECT && link->fromnode->flag & NODE_SELECT) {
      bNodeLink new_link{};
      new_link.flag = link->flag;
      new_link.tonode = node_map.lookup(link->tonode);
      new_link.tosock = socket_map.lookup(link->tosock);
      new_link.fromnode = node_map.lookup(link->fromnode);
      new_link.fromsock = socket_map.lookup(link->fromsock);
      new_link.multi_input_socket_index = link->multi_input_socket_index;
      clipboard.links.append(new_link);
    }
  }

  return OPERATOR_FINISHED;
}

void NODE_OT_clipboard_copy(wmOperatorType *ot)
{
  ot->name = "Copy to Clipboard";
  ot->description = "Copy the selected nodes to the internal clipboard";
  ot->idname = "NODE_OT_clipboard_copy";

  ot->exec = node_clipboard_copy_exec;
  ot->poll = ED_operator_node_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paste
 * \{ */

static void remap_pairing(bNodeTree &dst_tree, const Map<const bNode *, bNode *> &node_map)
{
  /* We don't have the old tree for looking up output nodes by ID,
   * so we have to build a map first to find copied output nodes in the new tree. */
  Map<int32_t, bNode *> dst_output_node_map;
  for (const auto &item : node_map.items()) {
    if (item.key->type == GEO_NODE_SIMULATION_OUTPUT) {
      dst_output_node_map.add_new(item.key->identifier, item.value);
    }
  }

  for (bNode *dst_node : node_map.values()) {
    if (dst_node->type == GEO_NODE_SIMULATION_INPUT) {
      NodeGeometrySimulationInput &data = *static_cast<NodeGeometrySimulationInput *>(
          dst_node->storage);
      if (const bNode *output_node = dst_output_node_map.lookup_default(data.output_node_id,
                                                                        nullptr)) {
        data.output_node_id = output_node->identifier;
      }
      else {
        data.output_node_id = 0;
        blender::nodes::update_node_declaration_and_sockets(dst_tree, *dst_node);
      }
    }
  }
}

static int node_clipboard_paste_exec(bContext *C, wmOperator *op)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &tree = *snode.edittree;
  NodeClipboard &clipboard = get_node_clipboard();

  const bool is_valid = clipboard.validate();

  if (clipboard.nodes.is_empty()) {
    BKE_report(op->reports, RPT_ERROR, "The internal clipboard is empty");
    return OPERATOR_CANCELLED;
  }

  if (!is_valid) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Some nodes references could not be restored, will be left empty");
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  node_deselect_all(tree);

  Map<const bNode *, bNode *> node_map;
  Map<const bNodeSocket *, bNodeSocket *> socket_map;

  /* copy valid nodes from clipboard */
  for (NodeClipboardItem &item : clipboard.nodes) {
    const bNode &node = *item.node;
    const char *disabled_hint = nullptr;
    if (node.typeinfo->poll_instance && node.typeinfo->poll_instance(&node, &tree, &disabled_hint))
    {
      bNode *new_node = bke::node_copy_with_mapping(
          &tree, node, LIB_ID_COPY_DEFAULT, true, socket_map);
      node_map.add_new(&node, new_node);
    }
    else {
      if (disabled_hint) {
        BKE_reportf(op->reports,
                    RPT_ERROR,
                    "Cannot add node %s into node tree %s: %s",
                    node.name,
                    tree.id.name + 2,
                    disabled_hint);
      }
      else {
        BKE_reportf(op->reports,
                    RPT_ERROR,
                    "Cannot add node %s into node tree %s",
                    node.name,
                    tree.id.name + 2);
      }
    }
  }

  for (bNode *new_node : node_map.values()) {
    nodeSetSelected(new_node, true);

    /* The parent pointer must be redirected to new node. */
    if (new_node->parent) {
      if (node_map.contains(new_node->parent)) {
        new_node->parent = node_map.lookup(new_node->parent);
      }
    }
  }

  PropertyRNA *offset_prop = RNA_struct_find_property(op->ptr, "offset");
  if (RNA_property_is_set(op->ptr, offset_prop)) {
    float2 center(0);
    for (NodeClipboardItem &item : clipboard.nodes) {
      center.x += BLI_rctf_cent_x(&item.draw_rect);
      center.y += BLI_rctf_cent_y(&item.draw_rect);
    }
    /* DPI factor needs to be removed when computing a View2D offset from drawing rects. */
    center /= clipboard.nodes.size();

    float2 mouse_location;
    RNA_property_float_get_array(op->ptr, offset_prop, mouse_location);
    const float2 offset = (mouse_location - center) / UI_SCALE_FAC;

    for (bNode *new_node : node_map.values()) {
      /* Skip the offset for parented nodes since the location is in parent space. */
      if (new_node->parent == nullptr) {
        new_node->locx += offset.x;
        new_node->locy += offset.y;
      }
    }
  }

  /* Add links between existing nodes. */
  for (const bNodeLink &link : clipboard.links) {
    const bNode *fromnode = link.fromnode;
    const bNode *tonode = link.tonode;
    if (node_map.lookup_key_ptr(fromnode) && node_map.lookup_key_ptr(tonode)) {
      bNodeLink *new_link = nodeAddLink(&tree,
                                        node_map.lookup(fromnode),
                                        socket_map.lookup(link.fromsock),
                                        node_map.lookup(tonode),
                                        socket_map.lookup(link.tosock));
      new_link->multi_input_socket_index = link.multi_input_socket_index;
    }
  }

  for (bNode *new_node : node_map.values()) {
    nodeDeclarationEnsure(&tree, new_node);
  }

  remap_pairing(tree, node_map);

  tree.ensure_topology_cache();
  for (bNode *new_node : node_map.values()) {
    /* Update multi input socket indices in case all connected nodes weren't copied. */
    update_multi_input_indices_for_removed_links(*new_node);
  }

  Main *bmain = CTX_data_main(C);
  ED_node_tree_propagate_change(C, bmain, &tree);
  /* Pasting nodes can create arbitrary new relations because nodes can reference IDs. */
  DEG_relations_tag_update(bmain);

  return OPERATOR_FINISHED;
}

static int node_clipboard_paste_invoke(bContext *C, wmOperator *op, const wmEvent *event)
{
  const ARegion *region = CTX_wm_region(C);
  float2 cursor;
  UI_view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &cursor.x, &cursor.y);
  RNA_float_set_array(op->ptr, "offset", cursor);
  return node_clipboard_paste_exec(C, op);
}

void NODE_OT_clipboard_paste(wmOperatorType *ot)
{
  ot->name = "Paste from Clipboard";
  ot->description = "Paste nodes from the internal clipboard to the active node tree";
  ot->idname = "NODE_OT_clipboard_paste";

  ot->invoke = node_clipboard_paste_invoke;
  ot->exec = node_clipboard_paste_exec;
  ot->poll = ED_operator_node_editable;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;

  PropertyRNA *prop = RNA_def_float_array(
      ot->srna,
      "offset",
      2,
      nullptr,
      -FLT_MAX,
      FLT_MAX,
      "Location",
      "The 2D view location for the center of the new nodes, or unchanged if not set",
      -FLT_MAX,
      FLT_MAX);
  RNA_def_property_flag(prop, PROP_SKIP_SAVE);
  RNA_def_property_flag(prop, PROP_HIDDEN);
}

/** \} */

}  // namespace blender::ed::space_node

void ED_node_clipboard_free()
{
  using namespace blender::ed::space_node;
  NodeClipboard &clipboard = get_node_clipboard();
  clipboard.validate();
  clipboard.clear();
}
