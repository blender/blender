/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ED_viewer_path.hh"
#include "ED_node.hh"
#include "ED_screen.hh"

#include "BKE_compute_context_cache.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_context.hh"
#include "BKE_lib_id.hh"
#include "BKE_main.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_zones.hh"
#include "BKE_viewer_path.hh"
#include "BKE_workspace.hh"

#include "BLI_listbase.h"
#include "BLI_string.h"
#include "BLI_vector.hh"

#include "DNA_modifier_types.h"
#include "DNA_node_types.h"
#include "DNA_windowmanager_types.h"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_query.hh"

#include "WM_api.hh"

namespace blender::ed::viewer_path {

using bke::bNodeTreeZone;
using bke::bNodeTreeZones;

ViewerPathElem *viewer_path_elem_for_compute_context(const ComputeContext &compute_context)
{
  if (const auto *context = dynamic_cast<const bke::ModifierComputeContext *>(&compute_context)) {
    ModifierViewerPathElem *elem = BKE_viewer_path_elem_new_modifier();
    elem->modifier_uid = context->modifier_uid();
    if (const NodesModifierData *nmd = context->nmd()) {
      elem->base.ui_name = BLI_strdup(nmd->modifier.name);
    }
    return &elem->base;
  }
  if (const auto *context = dynamic_cast<const bke::GroupNodeComputeContext *>(&compute_context)) {
    GroupNodeViewerPathElem *elem = BKE_viewer_path_elem_new_group_node();
    elem->node_id = context->node_id();
    if (const bNode *caller_node = context->node()) {
      if (const bNodeTree *group = reinterpret_cast<const bNodeTree *>(caller_node->id)) {
        elem->base.ui_name = BLI_strdup(BKE_id_name(group->id));
      }
    }
    return &elem->base;
  }
  if (const auto *context = dynamic_cast<const bke::SimulationZoneComputeContext *>(
          &compute_context))
  {
    SimulationZoneViewerPathElem *elem = BKE_viewer_path_elem_new_simulation_zone();
    elem->sim_output_node_id = context->output_node_id();
    return &elem->base;
  }
  if (const auto *context = dynamic_cast<const bke::RepeatZoneComputeContext *>(&compute_context))
  {
    RepeatZoneViewerPathElem *elem = BKE_viewer_path_elem_new_repeat_zone();
    elem->repeat_output_node_id = context->output_node_id();
    elem->iteration = context->iteration();
    return &elem->base;
  }
  if (const auto *context = dynamic_cast<const bke::ForeachGeometryElementZoneComputeContext *>(
          &compute_context))
  {
    ForeachGeometryElementZoneViewerPathElem *elem =
        BKE_viewer_path_elem_new_foreach_geometry_element_zone();
    elem->zone_output_node_id = context->output_node_id();
    elem->index = context->index();
    return &elem->base;
  }
  if (const auto *context = dynamic_cast<const bke::EvaluateClosureComputeContext *>(
          &compute_context))
  {
    EvaluateClosureNodeViewerPathElem *elem = BKE_viewer_path_elem_new_evaluate_closure();
    elem->evaluate_node_id = context->node_id();
    if (const std::optional<nodes::ClosureSourceLocation> &source =
            context->closure_source_location())
    {
      elem->source_output_node_id = source->closure_output_node_id;
      BLI_assert(DEG_is_original(source->tree));
      elem->source_node_tree = const_cast<bNodeTree *>(source->tree);
    }
    return &elem->base;
  }
  return nullptr;
}

static void viewer_path_for_geometry_node(const SpaceNode &snode,
                                          const bNode &node,
                                          ViewerPath &r_dst)
{
  /* Only valid if the node space has a context object. */
  BLI_assert(snode.id != nullptr && GS(snode.id->name) == ID_OB);
  snode.edittree->ensure_topology_cache();

  BKE_viewer_path_init(&r_dst);

  bke::ComputeContextCache compute_context_cache;
  const ComputeContext *socket_context = space_node::compute_context_for_edittree_socket(
      snode, compute_context_cache, node.input_socket(0));
  if (!socket_context) {
    return;
  }

  Object *ob = reinterpret_cast<Object *>(snode.id);
  IDViewerPathElem *id_elem = BKE_viewer_path_elem_new_id();
  id_elem->id = &ob->id;
  BLI_addhead(&r_dst.path, id_elem);

  for (const ComputeContext *context = socket_context; context; context = context->parent()) {
    ViewerPathElem *elem = viewer_path_elem_for_compute_context(*context);
    if (!elem) {
      BKE_viewer_path_clear(&r_dst);
      return;
    }
    BLI_insertlinkafter(&r_dst.path, id_elem, elem);
  }

  ViewerNodeViewerPathElem *viewer_node_elem = BKE_viewer_path_elem_new_viewer_node();
  viewer_node_elem->node_id = node.identifier;
  viewer_node_elem->base.ui_name = BLI_strdup(bke::node_label(*snode.edittree, node).c_str());
  BLI_addtail(&r_dst.path, viewer_node_elem);
}

void activate_geometry_node(Main &bmain,
                            SpaceNode &snode,
                            bNode &node,
                            std::optional<int> item_identifier)
{
  wmWindowManager *wm = (wmWindowManager *)bmain.wm.first;
  if (wm == nullptr) {
    return;
  }
  for (bNode *iter_node : snode.edittree->all_nodes()) {
    if (iter_node->type_legacy == GEO_NODE_VIEWER) {
      SET_FLAG_FROM_TEST(iter_node->flag, iter_node == &node, NODE_DO_OUTPUT);
    }
  }
  ViewerPath new_viewer_path{};
  BLI_SCOPED_DEFER([&]() { BKE_viewer_path_clear(&new_viewer_path); });
  if (snode.id != nullptr && GS(snode.id->name) == ID_OB) {
    viewer_path_for_geometry_node(snode, node, new_viewer_path);
  }

  bool found_view3d_with_enabled_viewer = false;
  View3D *any_view3d_without_viewer = nullptr;
  LISTBASE_FOREACH (wmWindow *, window, &wm->windows) {
    WorkSpace *workspace = BKE_workspace_active_get(window->workspace_hook);
    bScreen *screen = BKE_workspace_active_screen_get(window->workspace_hook);
    LISTBASE_FOREACH (ScrArea *, area, &screen->areabase) {
      SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
      if (sl->spacetype == SPACE_SPREADSHEET) {
        SpaceSpreadsheet &sspreadsheet = *reinterpret_cast<SpaceSpreadsheet *>(sl);
        if (!(sspreadsheet.flag & SPREADSHEET_FLAG_PINNED)) {
          SpreadsheetTableIDGeometry &table_id = sspreadsheet.geometry_id;
          table_id.object_eval_state = SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE;
          if (item_identifier) {
            table_id.viewer_item_identifier = *item_identifier;
          }
          MEM_SAFE_FREE(table_id.bundle_path);
          table_id.bundle_path_num = 0;
          table_id.closure_input_output = SPREADSHEET_CLOSURE_NONE;
        }
      }
      else if (sl->spacetype == SPACE_VIEW3D) {
        View3D &v3d = *reinterpret_cast<View3D *>(sl);
        if (v3d.flag2 & V3D_SHOW_VIEWER) {
          found_view3d_with_enabled_viewer = true;
        }
        else {
          any_view3d_without_viewer = &v3d;
        }
      }
    }

    /* Enable viewer in one viewport if it is disabled in all of them. */
    if (!found_view3d_with_enabled_viewer && any_view3d_without_viewer != nullptr) {
      any_view3d_without_viewer->flag2 |= V3D_SHOW_VIEWER;
    }

    BKE_viewer_path_clear(&workspace->viewer_path);
    BKE_viewer_path_copy(&workspace->viewer_path, &new_viewer_path);

    /* Make sure the viewed data becomes available. */
    DEG_id_tag_update(snode.id, ID_RECALC_GEOMETRY);
    WM_main_add_notifier(NC_VIEWER_PATH, nullptr);
  }
}

Object *parse_object_only(const ViewerPath &viewer_path)
{
  if (BLI_listbase_count(&viewer_path.path) != 1) {
    return nullptr;
  }
  const ViewerPathElem *elem = static_cast<ViewerPathElem *>(viewer_path.path.first);
  if (elem->type != VIEWER_PATH_ELEM_TYPE_ID) {
    return nullptr;
  }
  ID *id = reinterpret_cast<const IDViewerPathElem *>(elem)->id;
  if (id == nullptr) {
    return nullptr;
  }
  if (GS(id->name) != ID_OB) {
    return nullptr;
  }
  return reinterpret_cast<Object *>(id);
}

std::optional<ViewerPathForGeometryNodesViewer> parse_geometry_nodes_viewer(
    const ViewerPath &viewer_path)
{
  Vector<const ViewerPathElem *, 16> elems_vec;
  LISTBASE_FOREACH (const ViewerPathElem *, item, &viewer_path.path) {
    elems_vec.append(item);
  }

  if (elems_vec.size() < 3) {
    /* Need at least the object, modifier and viewer node name. */
    return std::nullopt;
  }
  Span<const ViewerPathElem *> remaining_elems = elems_vec;
  const ViewerPathElem &id_elem = *remaining_elems[0];
  if (id_elem.type != VIEWER_PATH_ELEM_TYPE_ID) {
    return std::nullopt;
  }
  ID *root_id = reinterpret_cast<const IDViewerPathElem &>(id_elem).id;
  if (root_id == nullptr) {
    return std::nullopt;
  }
  if (GS(root_id->name) != ID_OB) {
    return std::nullopt;
  }
  Object *root_ob = reinterpret_cast<Object *>(root_id);
  remaining_elems = remaining_elems.drop_front(1);
  const ViewerPathElem &modifier_elem = *remaining_elems[0];
  if (modifier_elem.type != VIEWER_PATH_ELEM_TYPE_MODIFIER) {
    return std::nullopt;
  }
  const int modifier_uid =
      reinterpret_cast<const ModifierViewerPathElem &>(modifier_elem).modifier_uid;

  remaining_elems = remaining_elems.drop_front(1);
  Vector<const ViewerPathElem *> node_path;
  for (const ViewerPathElem *elem : remaining_elems.drop_back(1)) {
    if (!ELEM(elem->type,
              VIEWER_PATH_ELEM_TYPE_GROUP_NODE,
              VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE,
              VIEWER_PATH_ELEM_TYPE_REPEAT_ZONE,
              VIEWER_PATH_ELEM_TYPE_FOREACH_GEOMETRY_ELEMENT_ZONE,
              VIEWER_PATH_ELEM_TYPE_EVALUATE_CLOSURE))
    {
      return std::nullopt;
    }
    node_path.append(elem);
  }
  const ViewerPathElem *last_elem = remaining_elems.last();
  if (last_elem->type != VIEWER_PATH_ELEM_TYPE_VIEWER_NODE) {
    return std::nullopt;
  }
  const int32_t viewer_node_id =
      reinterpret_cast<const ViewerNodeViewerPathElem *>(last_elem)->node_id;
  return ViewerPathForGeometryNodesViewer{root_ob, modifier_uid, node_path, viewer_node_id};
}

bool exists_geometry_nodes_viewer(const ViewerPathForGeometryNodesViewer &parsed_viewer_path)
{
  const NodesModifierData *modifier = nullptr;
  LISTBASE_FOREACH (const ModifierData *, md, &parsed_viewer_path.object->modifiers) {
    if (md->type != eModifierType_Nodes) {
      continue;
    }
    if (md->persistent_uid != parsed_viewer_path.modifier_uid) {
      continue;
    }
    modifier = reinterpret_cast<const NodesModifierData *>(md);
    break;
  }
  if (modifier == nullptr) {
    return false;
  }
  if (modifier->node_group == nullptr) {
    return false;
  }
  const bNodeTree *ngroup = modifier->node_group;
  const bNodeTreeZone *zone = nullptr;
  for (const ViewerPathElem *path_elem : parsed_viewer_path.node_path) {
    ngroup->ensure_topology_cache();
    const bNodeTreeZones *tree_zones = ngroup->zones();
    switch (path_elem->type) {
      case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE: {
        const auto &typed_elem = *reinterpret_cast<const SimulationZoneViewerPathElem *>(
            path_elem);
        const bNodeTreeZone *next_zone = tree_zones->get_zone_by_node(
            typed_elem.sim_output_node_id);
        if (next_zone == nullptr) {
          return false;
        }
        if (next_zone->parent_zone != zone) {
          return false;
        }
        zone = next_zone;
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_REPEAT_ZONE: {
        const auto &typed_elem = *reinterpret_cast<const RepeatZoneViewerPathElem *>(path_elem);
        const bNodeTreeZone *next_zone = tree_zones->get_zone_by_node(
            typed_elem.repeat_output_node_id);
        if (next_zone == nullptr) {
          return false;
        }
        if (next_zone->parent_zone != zone) {
          return false;
        }
        zone = next_zone;
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_FOREACH_GEOMETRY_ELEMENT_ZONE: {
        const auto &typed_elem =
            *reinterpret_cast<const ForeachGeometryElementZoneViewerPathElem *>(path_elem);
        const bNodeTreeZone *next_zone = tree_zones->get_zone_by_node(
            typed_elem.zone_output_node_id);
        if (next_zone == nullptr) {
          return false;
        }
        if (next_zone->parent_zone != zone) {
          return false;
        }
        zone = next_zone;
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_GROUP_NODE: {
        const auto &typed_elem = *reinterpret_cast<const GroupNodeViewerPathElem *>(path_elem);
        const bNode *group_node = ngroup->node_by_id(typed_elem.node_id);
        if (group_node == nullptr) {
          return false;
        }
        const bNodeTreeZone *parent_zone = tree_zones->get_zone_by_node(typed_elem.node_id);
        if (parent_zone != zone) {
          return false;
        }
        if (group_node->id == nullptr) {
          return false;
        }
        ngroup = reinterpret_cast<const bNodeTree *>(group_node->id);
        zone = nullptr;
        break;
      }
      case VIEWER_PATH_ELEM_TYPE_EVALUATE_CLOSURE: {
        const auto &typed_elem = *reinterpret_cast<const EvaluateClosureNodeViewerPathElem *>(
            path_elem);
        const bNode *evaluate_node = ngroup->node_by_id(typed_elem.evaluate_node_id);
        if (evaluate_node == nullptr) {
          return false;
        }
        const bNodeTreeZone *parent_zone = tree_zones->get_zone_by_node(
            typed_elem.evaluate_node_id);
        if (parent_zone != zone) {
          return false;
        }
        if (!typed_elem.source_node_tree) {
          return false;
        }
        const bNode *closure_output_node = typed_elem.source_node_tree->node_by_id(
            typed_elem.source_output_node_id);
        if (!closure_output_node) {
          return false;
        }
        ngroup = typed_elem.source_node_tree;
        const bNodeTreeZones *closure_tree_zones = typed_elem.source_node_tree->zones();
        if (!closure_tree_zones) {
          return false;
        }
        zone = closure_tree_zones->get_zone_by_node(typed_elem.source_output_node_id);
        break;
      }
      default: {
        BLI_assert_unreachable();
      }
    }
  }

  const bNode *viewer_node = ngroup->node_by_id(parsed_viewer_path.viewer_node_id);
  if (viewer_node == nullptr) {
    return false;
  }
  const bNodeTreeZones *tree_zones = ngroup->zones();
  if (tree_zones == nullptr) {
    return false;
  }
  if (tree_zones->get_zone_by_node(viewer_node->identifier) != zone) {
    return false;
  }
  return true;
}

UpdateActiveGeometryNodesViewerResult update_active_geometry_nodes_viewer(const bContext &C,
                                                                          ViewerPath &viewer_path)
{
  if (BLI_listbase_is_empty(&viewer_path.path)) {
    return UpdateActiveGeometryNodesViewerResult::NotActive;
  }
  const ViewerPathElem *last_elem = static_cast<ViewerPathElem *>(viewer_path.path.last);
  if (last_elem->type != VIEWER_PATH_ELEM_TYPE_VIEWER_NODE) {
    return UpdateActiveGeometryNodesViewerResult::NotActive;
  }
  const int32_t viewer_node_id =
      reinterpret_cast<const ViewerNodeViewerPathElem *>(last_elem)->node_id;

  const Main *bmain = CTX_data_main(&C);
  const wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  if (wm == nullptr) {
    return UpdateActiveGeometryNodesViewerResult::NotActive;
  }
  LISTBASE_FOREACH (const wmWindow *, window, &wm->windows) {
    const bScreen *active_screen = BKE_workspace_active_screen_get(window->workspace_hook);
    Vector<const bScreen *> screens = {active_screen};
    if (ELEM(active_screen->state, SCREENMAXIMIZED, SCREENFULL)) {
      const ScrArea *area = static_cast<ScrArea *>(active_screen->areabase.first);
      screens.append(area->full);
    }
    for (const bScreen *screen : screens) {
      LISTBASE_FOREACH (const ScrArea *, area, &screen->areabase) {
        const SpaceLink *sl = static_cast<SpaceLink *>(area->spacedata.first);
        if (sl == nullptr) {
          continue;
        }
        if (sl->spacetype != SPACE_NODE) {
          continue;
        }
        const SpaceNode &snode = *reinterpret_cast<const SpaceNode *>(sl);
        if (snode.edittree == nullptr) {
          continue;
        }
        if (snode.edittree->type != NTREE_GEOMETRY) {
          continue;
        }
        if (!snode.id) {
          continue;
        }
        snode.edittree->ensure_topology_cache();
        const bNode *viewer_node = snode.edittree->node_by_id(viewer_node_id);
        if (viewer_node == nullptr) {
          continue;
        }
        if (!(viewer_node->flag & NODE_DO_OUTPUT)) {
          continue;
        }
        ViewerPath tmp_viewer_path{};
        BLI_SCOPED_DEFER([&]() { BKE_viewer_path_clear(&tmp_viewer_path); });
        viewer_path_for_geometry_node(snode, *viewer_node, tmp_viewer_path);
        if (!BKE_viewer_path_equal(
                &viewer_path, &tmp_viewer_path, VIEWER_PATH_EQUAL_FLAG_IGNORE_ITERATION))
        {
          continue;
        }
        if (!BKE_viewer_path_equal(&viewer_path, &tmp_viewer_path)) {
          std::swap(viewer_path, tmp_viewer_path);
          /* Make sure the viewed data becomes available. */
          DEG_id_tag_update(snode.id, ID_RECALC_GEOMETRY);
          return UpdateActiveGeometryNodesViewerResult::Updated;
        }
        if (!BKE_viewer_path_equal(
                &viewer_path, &tmp_viewer_path, VIEWER_PATH_EQUAL_FLAG_CONSIDER_UI_NAME))
        {
          /* Only swap, without triggering a depsgraph update. */
          std::swap(viewer_path, tmp_viewer_path);
          return UpdateActiveGeometryNodesViewerResult::Updated;
        }
        return UpdateActiveGeometryNodesViewerResult::StillActive;
      }
    }
  }
  return UpdateActiveGeometryNodesViewerResult::NotActive;
}

bNode *find_geometry_nodes_viewer(const ViewerPath &viewer_path, SpaceNode &snode)
{
  /* Viewer path is only valid if the context object is set. */
  if (snode.id == nullptr || GS(snode.id->name) != ID_OB) {
    return nullptr;
  }

  const std::optional<ViewerPathForGeometryNodesViewer> parsed_viewer_path =
      parse_geometry_nodes_viewer(viewer_path);
  if (!parsed_viewer_path.has_value()) {
    return nullptr;
  }

  snode.edittree->ensure_topology_cache();
  bNode *possible_viewer = snode.edittree->node_by_id(parsed_viewer_path->viewer_node_id);
  if (possible_viewer == nullptr) {
    return nullptr;
  }
  ViewerPath tmp_viewer_path;
  BLI_SCOPED_DEFER([&]() { BKE_viewer_path_clear(&tmp_viewer_path); });
  viewer_path_for_geometry_node(snode, *possible_viewer, tmp_viewer_path);

  if (BKE_viewer_path_equal(&viewer_path, &tmp_viewer_path)) {
    return possible_viewer;
  }
  return nullptr;
}

[[nodiscard]] const ComputeContext *compute_context_for_viewer_path_elem(
    const ViewerPathElem &elem_generic,
    bke::ComputeContextCache &compute_context_cache,
    const ComputeContext *parent_compute_context)
{
  switch (ViewerPathElemType(elem_generic.type)) {
    case VIEWER_PATH_ELEM_TYPE_VIEWER_NODE:
    case VIEWER_PATH_ELEM_TYPE_ID: {
      return nullptr;
    }
    case VIEWER_PATH_ELEM_TYPE_MODIFIER: {
      const auto &elem = reinterpret_cast<const ModifierViewerPathElem &>(elem_generic);
      return &compute_context_cache.for_modifier(parent_compute_context, elem.modifier_uid);
    }
    case VIEWER_PATH_ELEM_TYPE_GROUP_NODE: {
      const auto &elem = reinterpret_cast<const GroupNodeViewerPathElem &>(elem_generic);
      return &compute_context_cache.for_group_node(parent_compute_context, elem.node_id);
    }
    case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE: {
      const auto &elem = reinterpret_cast<const SimulationZoneViewerPathElem &>(elem_generic);
      return &compute_context_cache.for_simulation_zone(parent_compute_context,
                                                        elem.sim_output_node_id);
    }
    case VIEWER_PATH_ELEM_TYPE_REPEAT_ZONE: {
      const auto &elem = reinterpret_cast<const RepeatZoneViewerPathElem &>(elem_generic);
      return &compute_context_cache.for_repeat_zone(
          parent_compute_context, elem.repeat_output_node_id, elem.iteration);
    }
    case VIEWER_PATH_ELEM_TYPE_FOREACH_GEOMETRY_ELEMENT_ZONE: {
      const auto &elem = reinterpret_cast<const ForeachGeometryElementZoneViewerPathElem &>(
          elem_generic);
      return &compute_context_cache.for_foreach_geometry_element_zone(
          parent_compute_context, elem.zone_output_node_id, elem.index);
    }
    case VIEWER_PATH_ELEM_TYPE_EVALUATE_CLOSURE: {
      const auto &elem = reinterpret_cast<const EvaluateClosureNodeViewerPathElem &>(elem_generic);
      std::optional<nodes::ClosureSourceLocation> source_location;
      if (elem.source_node_tree) {
        source_location = nodes::ClosureSourceLocation{
            elem.source_node_tree,
            elem.source_output_node_id,
            parent_compute_context ? parent_compute_context->hash() : ComputeContextHash{}};
      }
      return &compute_context_cache.for_evaluate_closure(
          parent_compute_context, elem.evaluate_node_id, nullptr, source_location);
    }
  }
  return nullptr;
}

}  // namespace blender::ed::viewer_path
