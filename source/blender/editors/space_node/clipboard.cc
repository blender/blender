/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_space_types.h"

#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_main.hh"
#include "BKE_main_idmap.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_report.hh"

#include "ED_node.hh"
#include "ED_render.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "DEG_depsgraph_build.hh"

#include "node_intern.hh"

namespace blender::ed::space_node {

struct NodeClipboardItemIDInfo {
  /** Name of the referenced ID. */
  std::string id_name = "";
  /**
   * Library filepath of the referenced ID, together with its name it forms a unique identifier.
   *
   * Note: Library reference is stored as an absolute path. Since the Node clipboard is runtime
   * data, persistent over new blend-files opening, this should guarantee that identical IDs from
   * identical libraries can be matched accordingly, even across several blend-files.
   */
  std::string library_path = "";

  /** The validated ID pointer (may be the same as the original one, or a new one). */
  std::optional<ID *> new_id = {};
};

struct NodeClipboardItem {
  bNode *node;
  /**
   * The offset and size of the node from when it was drawn. Stored here since it doesn't remain
   * valid for the nodes in the clipboard.
   */
  rctf draw_rect;

  /* Extra info to validate the IDs used by the node on creation. Otherwise it may reference
   * missing data. */

  ID *id;
  std::string id_name;
  std::string library_name;
};

struct NodeClipboard {
  Vector<NodeClipboardItem> nodes;
  Vector<bNodeLink> links;

  /* A mapping of all ID references from nodes in the clipboard, to information allowing to find
   * their valid matching counterpart in current Main data when pasting the nodes back. Entries are
   * added when adding nodes to the clipboard, and they are updated when pasting the nodes back
   * into current Main. */
  Map<ID *, NodeClipboardItemIDInfo> old_ids_to_idinfo;

  /** Completely empty the current clipboard content. */
  void clear()
  {
    for (NodeClipboardItem &item : this->nodes) {
      bke::node_free_node(nullptr, item.node);
    }
    this->nodes.clear_and_shrink();
    this->links.clear_and_shrink();
    this->old_ids_to_idinfo.clear_and_shrink();
  }

  /**
   * Find valid current pointers for all IDs used by the nodes in the clipboard.
   *
   * DO NOT update the nodes' pointers here though, as this would affect the clipboard content,
   * which is no desired here. It should remain as in original state, such that e.g. one can copy
   * nodes in file `A.blend`, open file `B.blend`, paste nodes (and lose some of the invalid ID
   * references in file `B.blend`), and then open again file `A.blend`, paste nodes, and lose no ID
   * references.
   */
  bool paste_validate_id_references(Main &bmain)
  {
    bool is_valid = true;
    IDNameLib_Map *bmain_id_map = nullptr;

    /* Clear any potentially previously found `new_id` valid pointers in #old_ids_to_idinfo values,
     * and populate a temporary mapping from absolute library paths to existing Library IDs in
     * given Main. */
    Map<std::string, Library *> libraries_path_to_id;
    for (NodeClipboardItemIDInfo &id_info : this->old_ids_to_idinfo.values()) {
      id_info.new_id.reset();
      if (!id_info.library_path.empty() && !libraries_path_to_id.contains(id_info.library_path)) {
        libraries_path_to_id.add(
            id_info.library_path,
            static_cast<Library *>(BLI_findstring(&bmain.libraries,
                                                  id_info.library_path.c_str(),
                                                  offsetof(Library, runtime.filepath_abs))));
      }
    }

    /* Find a new valid ID pointer for all ID usages in given node.
     *
     * NOTE: Due to the fact that the clipboard survives file loading, only name (including IDType)
     * and library-path pairs can be used here.
     *   - UID cannot be trusted across file load.
     *   - ID pointer itself cannot be trusted across undo/redo and file-load. */
    auto validate_id_fn = [this, &is_valid, &bmain, &bmain_id_map, &libraries_path_to_id](
                              LibraryIDLinkCallbackData *cb_data) -> int {
      ID *old_id = *(cb_data->id_pointer);
      if (!old_id) {
        return IDWALK_RET_NOP;
      }
      if (!this->old_ids_to_idinfo.contains(old_id)) {
        BLI_assert_msg(
            0, "Missing entry in the old ID data of the node clipboard, should not happen");
        is_valid = false;
        return IDWALK_RET_NOP;
      }

      NodeClipboardItemIDInfo &id_info = this->old_ids_to_idinfo.lookup(old_id);
      if (!id_info.new_id) {
        if (!bmain_id_map) {
          bmain_id_map = BKE_main_idmap_create(&bmain, false, nullptr, MAIN_IDMAP_TYPE_NAME);
        }
        Library *new_id_lib = libraries_path_to_id.lookup_default(id_info.library_path, nullptr);
        if (id_info.library_path.empty() || new_id_lib) {
          id_info.new_id = BKE_main_idmap_lookup_name(
              bmain_id_map, GS(id_info.id_name.c_str()), id_info.id_name.c_str() + 2, new_id_lib);
        }
        else {
          /* No matching library found, so there is no possible matching ID either. */
          id_info.new_id = nullptr;
        }
      }
      if (*(id_info.new_id) == nullptr) {
        is_valid = false;
      }
      return IDWALK_RET_NOP;
    };
    for (NodeClipboardItem &item : this->nodes) {
      BKE_library_foreach_subdata_id(
          &bmain,
          nullptr,
          nullptr,
          [&item](LibraryForeachIDData *data) { bke::node_node_foreach_id(item.node, data); },
          validate_id_fn,
          nullptr,
          IDWALK_READONLY);
    }

    if (bmain_id_map) {
      BKE_main_idmap_destroy(bmain_id_map);
      bmain_id_map = nullptr;
    }
    return is_valid;
  }

  /**
   * Ensure that a newly pasted copy of a node from the clipboard has valid ID references, as
   * ensured by #paste_validate_id_references.
   */
  void paste_update_node_id_references(bNode &node)
  {
    /* Update all old ID pointers in given node by new, valid ones. */
    auto update_id_fn = [this](LibraryIDLinkCallbackData *cb_data) -> int {
      ID *old_id = *(cb_data->id_pointer);
      if (!old_id) {
        return IDWALK_RET_NOP;
      }
      if (!this->old_ids_to_idinfo.contains(old_id)) {
        BLI_assert_msg(
            0, "Missing entry in the old ID data of the node clipboard, should not happen");
        *(cb_data->id_pointer) = nullptr;
        return IDWALK_RET_NOP;
      }

      NodeClipboardItemIDInfo &id_info = this->old_ids_to_idinfo.lookup(old_id);
      if (!id_info.new_id) {
        BLI_assert_msg(
            0,
            "Unset new ID value for an old ID reference in the node clipboard, should not happen");
        *(cb_data->id_pointer) = nullptr;
        return IDWALK_RET_NOP;
      }
      *(cb_data->id_pointer) = *(id_info.new_id);
      if (cb_data->cb_flag & IDWALK_CB_USER) {
        id_us_plus(*(cb_data->id_pointer));
      }
      return IDWALK_RET_NOP;
    };
    BKE_library_foreach_subdata_id(
        nullptr,
        nullptr,
        nullptr,
        [&node](LibraryForeachIDData *data) { bke::node_node_foreach_id(&node, data); },
        update_id_fn,
        nullptr,
        IDWALK_NOP);
  }

  /** Add a new node to the clipboard. */
  void copy_add_node(const bNode &node,
                     Map<const bNode *, bNode *> &node_map,
                     Map<const bNodeSocket *, bNodeSocket *> &socket_map)
  {
    /* No ID reference-counting, this node is virtual,
     * detached from any actual Blender data currently. */
    bNode *new_node = bke::node_copy_with_mapping(
        nullptr, node, LIB_ID_CREATE_NO_USER_REFCOUNT | LIB_ID_CREATE_NO_MAIN, false, socket_map);
    node_map.add_new(&node, new_node);

    /* Find a new valid ID pointer for all ID usages in given node. */
    auto ensure_id_info_fn = [this](LibraryIDLinkCallbackData *cb_data) -> int {
      ID *old_id = *(cb_data->id_pointer);
      if (!old_id) {
      }
      if (this->old_ids_to_idinfo.contains(old_id)) {
        return IDWALK_RET_NOP;
      }

      NodeClipboardItemIDInfo id_info;
      if (old_id) {
        id_info.id_name = old_id->name;
        if (ID_IS_LINKED(old_id)) {
          id_info.library_path = old_id->lib->runtime.filepath_abs;
        }
      }
      this->old_ids_to_idinfo.add(old_id, std::move(id_info));
      return IDWALK_RET_NOP;
    };
    BKE_library_foreach_subdata_id(
        nullptr,
        nullptr,
        nullptr,
        [&node](LibraryForeachIDData *data) {
          bke::node_node_foreach_id(const_cast<bNode *>(&node), data);
        },
        ensure_id_info_fn,
        nullptr,
        IDWALK_READONLY);

    NodeClipboardItem item;
    item.draw_rect = node.runtime->totr;
    item.node = new_node;
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
      clipboard.copy_add_node(*node, node_map, socket_map);
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
      new_link.multi_input_sort_id = link->multi_input_sort_id;
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

static int node_clipboard_paste_exec(bContext *C, wmOperator *op)
{
  Main *bmain = CTX_data_main(C);
  SpaceNode &snode = *CTX_wm_space_node(C);
  bNodeTree &tree = *snode.edittree;
  NodeClipboard &clipboard = get_node_clipboard();

  if (clipboard.nodes.is_empty()) {
    BKE_report(op->reports, RPT_ERROR, "The internal clipboard is empty");
    return OPERATOR_CANCELLED;
  }

  if (!clipboard.paste_validate_id_references(*bmain)) {
    BKE_report(op->reports,
               RPT_WARNING,
               "Some nodes references to other IDs could not be restored, will be left empty");
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
      /* Do not access referenced ID pointers here, as they are still the old ones, which may be
       * invalid. */
      bNode *new_node = bke::node_copy_with_mapping(
          &tree, node, LIB_ID_CREATE_NO_USER_REFCOUNT, true, socket_map);
      /* Update the newly copied node's ID references. */
      clipboard.paste_update_node_id_references(*new_node);
      /* Reset socket shape in case a node is copied to a different tree type. */
      LISTBASE_FOREACH (bNodeSocket *, socket, &new_node->inputs) {
        socket->display_shape = SOCK_DISPLAY_SHAPE_CIRCLE;
      }
      LISTBASE_FOREACH (bNodeSocket *, socket, &new_node->outputs) {
        socket->display_shape = SOCK_DISPLAY_SHAPE_CIRCLE;
      }
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

    new_node->flag &= ~NODE_ACTIVE;

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
      new_link->multi_input_sort_id = link.multi_input_sort_id;
    }
  }

  for (bNode *new_node : node_map.values()) {
    bke::nodeDeclarationEnsure(&tree, new_node);
  }

  remap_node_pairing(tree, node_map);

  tree.ensure_topology_cache();
  for (bNode *new_node : node_map.values()) {
    /* Update multi input socket indices in case all connected nodes weren't copied. */
    update_multi_input_indices_for_removed_links(*new_node);
  }

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
  clipboard.clear();
}
