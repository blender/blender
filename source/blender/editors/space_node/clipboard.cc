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

#include "DEG_depsgraph_build.h"

#include "node_intern.hh"

namespace blender::ed::space_node {

struct NodeClipboardItem {
  bNode *node;

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

  void add_node(bNode *node)
  {
    NodeClipboardItem item;
    item.node = node;
    item.id = node->id;
    if (item.id) {
      item.id_name = node->id->name;
      if (ID_IS_LINKED(node->id)) {
        item.library_name = node->id->lib->filepath_abs;
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

  for (bNode *node : tree.all_nodes()) {
    if (node->flag & SELECT) {
      /* No ID reference counting, this node is virtual, detached from any actual Blender data. */
      bNode *new_node = bke::node_copy_with_mapping(nullptr,
                                                    *node,
                                                    LIB_ID_CREATE_NO_USER_REFCOUNT |
                                                        LIB_ID_CREATE_NO_MAIN,
                                                    false,
                                                    socket_map);
      node_map.add_new(node, new_node);
    }
  }

  for (bNode *new_node : node_map.values()) {
    clipboard.add_node(new_node);

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
  ot->description = "Copies selected nodes to the clipboard";
  ot->idname = "NODE_OT_clipboard_copy";

  ot->exec = node_clipboard_copy_exec;
  ot->poll = ED_operator_node_active;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Paste
 * \{ */

static int node_clipboard_paste_exec(bContext *C, wmOperator *op)
{
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &tree = *snode.edittree;
  NodeClipboard &clipboard = get_node_clipboard();

  const bool is_valid = clipboard.validate();

  if (clipboard.nodes.is_empty()) {
    BKE_report(op->reports, RPT_ERROR, "Clipboard is empty");
    return OPERATOR_CANCELLED;
  }

  if (!is_valid) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Some nodes references could not be restored, will be left empty");
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  node_deselect_all(tree);

  /* calculate "barycenter" for placing on mouse cursor */
  float2 center = {0.0f, 0.0f};
  for (const NodeClipboardItem &item : clipboard.nodes) {
    center.x += BLI_rctf_cent_x(&item.node->runtime->totr);
    center.y += BLI_rctf_cent_y(&item.node->runtime->totr);
  }
  center /= clipboard.nodes.size();

  Map<const bNode *, bNode *> node_map;
  Map<const bNodeSocket *, bNodeSocket *> socket_map;

  /* copy valid nodes from clipboard */
  for (NodeClipboardItem &item : clipboard.nodes) {
    const bNode &node = *item.node;
    const char *disabled_hint = nullptr;
    if (node.typeinfo->poll_instance &&
        node.typeinfo->poll_instance(&node, &tree, &disabled_hint)) {
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

void NODE_OT_clipboard_paste(wmOperatorType *ot)
{
  ot->name = "Paste from Clipboard";
  ot->description = "Pastes nodes from the clipboard to the active node tree";
  ot->idname = "NODE_OT_clipboard_paste";

  ot->exec = node_clipboard_paste_exec;
  ot->poll = ED_operator_node_editable;

  ot->flag = OPTYPE_REGISTER | OPTYPE_UNDO;
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
