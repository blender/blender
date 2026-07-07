/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_space_types.h"

#include "BLI_listbase.h"

#include "BKE_appdir.hh"
#include "BKE_blendfile.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_lib_id.hh"
#include "BKE_lib_query.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_main_idmap.hh"
#include "BKE_main_invariants.hh"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_report.hh"

#include "BLO_readfile.hh"

#include "BLI_path_utils.hh"

#include "ED_node.hh"
#include "ED_render.hh"
#include "ED_screen.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"

#include "DEG_depsgraph_build.hh"

#include "node_intern.hh"

namespace blender {

namespace ed::space_node {

/* -------------------------------------------------------------------- */
/** \name Local Utilities
 * \{ */

static void node_copybuffer_filepath_get(char filepath[FILE_MAX], size_t filepath_maxncpy)
{
  BLI_path_join(filepath, filepath_maxncpy, BKE_tempdir_base(), "copybuffer_nodes.blend");
}

/** Returns the number of nodes copied. */
static int node_copy_local(bNodeTree &from_tree,
                           bNodeTree &to_tree,
                           const bool allow_duplicate_names,
                           const bool do_user_count,
                           const float2 offset,
                           const bool snap_to_grid,
                           ReportList *reports)
{
  node_select_paired(from_tree);

  Map<const bNode *, bNode *> node_map;
  Map<const bNodeSocket *, bNodeSocket *> socket_map;
  const char *disabled_hint = nullptr;

  for (bNode *node : get_selected_nodes(from_tree)) {
    if (!node->typeinfo->poll_instance ||
        node->typeinfo->poll_instance(node, &to_tree, &disabled_hint))
    {
      int flags = LIB_ID_COPY_DEFAULT;
      if (!do_user_count) {
        flags |= LIB_ID_CREATE_NO_USER_REFCOUNT;
      }

      bNode *new_node = bke::node_copy_with_mapping(
          &to_tree, *node, flags, std::nullopt, std::nullopt, socket_map, allow_duplicate_names);
      node_map.add_new(node, new_node);
      new_node->location[0] += offset.x;
      new_node->location[1] += offset.y;
      if (snap_to_grid) {
        new_node->location[0] = nearest_node_grid_coord(new_node->location[0]);
        new_node->location[1] = nearest_node_grid_coord(new_node->location[1]);
      }
    }
    else {
      if (disabled_hint) {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Cannot add node %s into node tree %s: %s",
                    node->name,
                    to_tree.id.name + 2,
                    disabled_hint);
      }
      else {
        BKE_reportf(reports,
                    RPT_ERROR,
                    "Cannot add node %s into node tree %s",
                    node->name,
                    to_tree.id.name + 2);
      }
    }
  }

  if (node_map.is_empty()) {
    return false;
  }

  for (bNode *new_node : node_map.values()) {
    /* Parent pointer must be redirected to new node or detached if parent is not copied. */
    if (new_node->parent) {
      if (node_map.contains(new_node->parent)) {
        new_node->parent = node_map.lookup(new_node->parent);
      }
      else {
        bke::node_detach_node(to_tree, *new_node);
      }
    }
  }

  remap_node_pairing(to_tree, node_map);

  /* Copy links between selected nodes. */
  for (bNodeLink &link : from_tree.links) {
    BLI_assert(link.tonode);
    BLI_assert(link.fromnode);
    if (link.tonode->flag & NODE_SELECT && link.fromnode->flag & NODE_SELECT) {
      if (!node_map.contains(link.tonode) || !node_map.contains(link.fromnode)) {
        /* If copying a node fails, skip copying their links. */
        continue;
      };
      bNode *from_node = node_map.lookup(link.fromnode);
      bNode *to_node = node_map.lookup(link.tonode);

      bNodeSocket *from = bke::node_find_socket(*from_node, SOCK_OUT, link.fromsock->identifier);
      bNodeSocket *to = bke::node_find_socket(*to_node, SOCK_IN, link.tosock->identifier);
      if (!from || !to) {
        continue;
      }

      bNodeLink &new_link = bke::node_add_link(to_tree, *from_node, *from, *to_node, *to);
      new_link.multi_input_sort_id = link.multi_input_sort_id;
    }
  }

  to_tree.ensure_topology_cache();
  for (bNode *new_node : node_map.values()) {
    /* Update multi input socket indices in case all connected nodes weren't copied. */
    update_multi_input_indices_for_removed_links(*new_node);
  }

  return node_map.size();
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Copy
 * \{ */

static wmOperatorStatus node_clipboard_copy_exec(bContext *C, wmOperator *op)
{
  using namespace blender::bke::blendfile;

  Main *bmain = CTX_data_main(C);
  SpaceNode *snode = CTX_wm_space_node(C);
  bNodeTree *node_tree = snode->edittree;

  PartialWriteContext copy_buffer{*bmain};
  bNodeTree *copy_tree = reinterpret_cast<bNodeTree *>(
      copy_buffer.id_create(ID_NT,
                            "Copy Tree",
                            nullptr,
                            {(PartialWriteContext::IDAddOperations::SET_FAKE_USER |
                              PartialWriteContext::IDAddOperations::SET_CLIPBOARD_MARK)}));

  strcpy(copy_tree->idname, node_tree->typeinfo->idname.c_str());
  bke::node_tree_set_type(*copy_tree);

  /* Copy node interface to avoid losing links to Group Input and Group Output nodes.
   * Note: this doesn't create new interface items if they don't exist. */
  /* #bNodeTreeInterface::copy_data() allocates runtime memory, so we need to free it to avoid
   * memory leak. */
  copy_tree->tree_interface.free_data();
  copy_tree->tree_interface.copy_data(node_tree->tree_interface, LIB_ID_COPY_DEFAULT);

  const int num_copied = node_copy_local(
      *node_tree, *copy_tree, true, false, float2(0), false, op->reports);
  if (num_copied == 0) {
    return OPERATOR_CANCELLED;
  }

  auto add_tree_ids_dependencies_cb = [&copy_buffer,
                                       copy_tree](LibraryIDLinkCallbackData *cb_data) -> int {
    /* Embedded or null IDs usages can be ignored here. */
    if (cb_data->cb_flag & (IDWALK_CB_EMBEDDED | IDWALK_CB_EMBEDDED_NOT_OWNING)) {
      return IDWALK_RET_NOP;
    }
    ID *id_src = *cb_data->id_pointer;
    if (!id_src) {
      return IDWALK_RET_NOP;
    }

    ID *id_dst = nullptr;

    if (id_src == &copy_tree->id) {
      /* #copy_tree is just a container for the copied nodes, so it can be safely ignored. */
      return IDWALK_RET_NOP;
    }

    auto partial_write_dependencies_filter_cb = [](LibraryIDLinkCallbackData *cb_deps_data,
                                                   PartialWriteContext::IDAddOptions /*options*/) {
      ID *id_deps_src = *cb_deps_data->id_pointer;
      const ID_Type id_type = GS((id_deps_src)->name);
      if (id_type == ID_SCE) {
        /* Note: Scenes referenced in the Render Layers node are cleared. At this stage, we
         * don't know if the target blender instance will have a scene with identical name, so
         * they are saved in the copy buffer as empty scenes. The pasting code deletes the
         * extra scenes, see #node_clipboard_paste_exec(). */
        return PartialWriteContext::IDAddOperations::CLEAR_DEPENDENCIES;
      }
      /* All ID datablocks exposed through nodes are added here. */
      return PartialWriteContext::IDAddOperations::ADD_DEPENDENCIES;
    };

    id_dst = copy_buffer.id_add(
        id_src, {PartialWriteContext::IDAddOperations::NOP}, partial_write_dependencies_filter_cb);

    *cb_data->id_pointer = id_dst;
    return IDWALK_RET_NOP;
  };

  BKE_library_foreach_ID_link(
      nullptr, &copy_tree->id, add_tree_ids_dependencies_cb, nullptr, IDWALK_NOP);

  char filepath[FILE_MAX];
  node_copybuffer_filepath_get(filepath, sizeof(filepath));
  if (!copy_buffer.write(filepath, *op->reports)) {
    BLI_assert_unreachable();
    BKE_report(op->reports, RPT_ERROR, "Unable to write to copy buffer on disk.");
    return OPERATOR_CANCELLED;
  };

  BKE_reportf(op->reports, RPT_INFO, "Copied %d selected node(s)", num_copied);
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

static StringRef scene_lib_filepath(const Scene &scene)
{
  if (scene.id.lib && scene.id.lib->runtime) {
    return scene.id.lib->runtime->filepath_abs;
  }
  return "";
}

static wmOperatorStatus node_clipboard_paste_exec(bContext *C, wmOperator *op)
{
  SpaceNode *snode = CTX_wm_space_node(C);

  char filepath[FILE_MAX];
  node_copybuffer_filepath_get(filepath, sizeof(filepath));

  const BlendFileReadParams params{};
  BlendFileReadReport bf_reports{};
  BlendFileData *bfd = BKE_blendfile_read(filepath, &params, &bf_reports);

  if (bfd == nullptr) {
    BKE_report(op->reports, RPT_INFO, "No data to paste");
    return OPERATOR_CANCELLED;
  }

  ED_preview_kill_jobs(CTX_wm_manager(C), CTX_data_main(C));

  Main *bmain_src = bfd->main;
  bfd->main = nullptr;
  BLO_blendfiledata_free(bfd);

  /* We don't want to paste scenes referenced by the Render Layers node if they don't exist in the
   * destination bmain. */
  Main *bmain_dst = CTX_data_main(C);
  Set<std::pair<StringRef, StringRef>> dst_scenes;
  for (Scene &scene : bmain_dst->scenes) {
    /* Packed scenes are currently not needed so they are skipped.
     * TODO: Support packed scenes. */
    if (ID_IS_PACKED(&scene.id)) {
      continue;
    }
    dst_scenes.add({scene.id.name, scene_lib_filepath(scene)});
  }

  for (Scene &scene : bmain_src->scenes.items_mutable()) {
    if (ID_IS_PACKED(&scene.id)) {
      continue;
    }
    /* All scenes that will be added through merging the two bmains are removed. */
    if (!dst_scenes.contains({scene.id.name, scene_lib_filepath(scene)})) {
      BKE_id_delete(bmain_src, &scene.id);
    }
  }

  MainMergeReport merge_reports = {};
  /* Frees bmain_src. */
  BKE_main_merge(bmain_dst, &bmain_src, merge_reports);

  bNodeTree *from_tree = nullptr;
  FOREACH_NODETREE_BEGIN (bmain_dst, node_tree, id) {
    if (node_tree->id.flag & ID_FLAG_CLIPBOARD_MARK) {
      from_tree = node_tree;
      break;
    }
  }
  FOREACH_NODETREE_END;
  BLI_assert(from_tree != nullptr);

  bNodeTree *to_tree = snode->edittree;
  node_deselect_all(*to_tree);

  float2 offset(0);
  PropertyRNA *offset_prop = RNA_struct_find_property(op->ptr, "offset");
  if (RNA_property_is_set(op->ptr, offset_prop)) {
    rctf bbox;
    float2 center;
    BLI_rctf_init_minmax(&bbox);
    for (bNode *node : from_tree->all_nodes()) {
      bbox.xmin = math::min(node->location[0], bbox.xmin);
      bbox.ymin = math::min(node->location[1], bbox.ymin);

      bbox.xmax = math::max(node->location[0] + node->width, bbox.xmax);
      bbox.ymax = math::max(node->location[1] - node->height / 2, bbox.ymax);
    }
    center.x = BLI_rctf_cent_x(&bbox);
    center.y = BLI_rctf_cent_y(&bbox);

    float2 mouse_location;
    RNA_property_float_get_array(op->ptr, offset_prop, mouse_location);
    offset = mouse_location / UI_SCALE_FAC - center;
  }

  const bool snap_to_grid = CTX_data_scene(C)->toolsettings->snap_flag_node & SCE_SNAP;
  const int num_copied = node_copy_local(
      *from_tree, *snode->edittree, false, true, offset, snap_to_grid, op->reports);
  if (num_copied == 0) {
    /* Note: we don't return OPERATOR_CANCELLED here although the copy fails to avoid corrupting
     * the undo stack after merging two bmains. */
  };
  BKE_id_delete(bmain_dst, &from_tree->id);

  BKE_main_ensure_invariants(*bmain_dst);
  /* Pasting nodes can create arbitrary new relations because nodes can reference IDs. */
  DEG_relations_tag_update(bmain_dst);

  return OPERATOR_FINISHED;
}

static wmOperatorStatus node_clipboard_paste_invoke(bContext *C,
                                                    wmOperator *op,
                                                    const wmEvent *event)
{
  const ARegion *region = CTX_wm_region(C);
  float2 cursor;
  ui::view2d_region_to_view(&region->v2d, event->mval[0], event->mval[1], &cursor.x, &cursor.y);
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

}  // namespace ed::space_node
}  // namespace blender
