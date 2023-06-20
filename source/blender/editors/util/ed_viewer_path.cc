/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "ED_viewer_path.hh"
#include "ED_screen.h"

#include "BKE_context.h"
#include "BKE_main.h"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_zones.hh"
#include "BKE_workspace.h"

#include "BLI_listbase.h"
#include "BLI_vector.hh"

#include "DNA_modifier_types.h"
#include "DNA_windowmanager_types.h"

#include "DEG_depsgraph.h"

#include "WM_api.h"

namespace blender::ed::viewer_path {

using namespace bke::node_tree_zones;

static void viewer_path_for_geometry_node(const SpaceNode &snode,
                                          const bNode &node,
                                          ViewerPath &r_dst)
{
  /* Only valid if the node space has a context object. */
  BLI_assert(snode.id != nullptr && GS(snode.id->name) == ID_OB);

  BKE_viewer_path_init(&r_dst);

  Object *ob = reinterpret_cast<Object *>(snode.id);
  IDViewerPathElem *id_elem = BKE_viewer_path_elem_new_id();
  id_elem->id = &ob->id;
  BLI_addtail(&r_dst.path, id_elem);

  NodesModifierData *modifier = nullptr;
  LISTBASE_FOREACH (ModifierData *, md, &ob->modifiers) {
    if (md->type != eModifierType_Nodes) {
      continue;
    }
    NodesModifierData *nmd = reinterpret_cast<NodesModifierData *>(md);
    if (nmd->node_group != snode.nodetree) {
      continue;
    }
    if (snode.flag & SNODE_PIN) {
      /* If the node group is pinned, use the first matching modifier. This can be improved by
       * storing the modifier name in the node editor when the context is pinned. */
      modifier = nmd;
      break;
    }
    if (md->flag & eModifierFlag_Active) {
      modifier = nmd;
    }
  }
  if (modifier == nullptr) {
    return;
  }
  ModifierViewerPathElem *modifier_elem = BKE_viewer_path_elem_new_modifier();
  modifier_elem->modifier_name = BLI_strdup(modifier->modifier.name);
  BLI_addtail(&r_dst.path, modifier_elem);

  Vector<const bNodeTreePath *, 16> tree_path = snode.treepath;
  for (const int i : tree_path.index_range().drop_back(1)) {
    bNodeTree *tree = tree_path[i]->nodetree;
    /* The tree path contains the name of the node but not its ID. */
    const char *node_name = tree_path[i + 1]->node_name;
    const bNode *node = nodeFindNodebyName(tree, node_name);
    /* The name in the tree path should match a group node in the tree. */
    BLI_assert(node != nullptr);

    tree->ensure_topology_cache();
    const TreeZones *tree_zones = tree->zones();
    if (!tree_zones) {
      return;
    }
    const Vector<const TreeZone *> zone_stack = tree_zones->get_zone_stack_for_node(
        node->identifier);
    for (const TreeZone *zone : zone_stack) {
      SimulationZoneViewerPathElem *node_elem = BKE_viewer_path_elem_new_simulation_zone();
      node_elem->sim_output_node_id = zone->output_node->identifier;
      BLI_addtail(&r_dst.path, node_elem);
    }

    GroupNodeViewerPathElem *node_elem = BKE_viewer_path_elem_new_group_node();
    node_elem->node_id = node->identifier;
    node_elem->base.ui_name = BLI_strdup(node->name);
    BLI_addtail(&r_dst.path, node_elem);
  }

  snode.edittree->ensure_topology_cache();
  const TreeZones *tree_zones = snode.edittree->zones();
  if (!tree_zones) {
    return;
  }
  const Vector<const TreeZone *> zone_stack = tree_zones->get_zone_stack_for_node(node.identifier);
  for (const TreeZone *zone : zone_stack) {
    SimulationZoneViewerPathElem *node_elem = BKE_viewer_path_elem_new_simulation_zone();
    node_elem->sim_output_node_id = zone->output_node->identifier;
    BLI_addtail(&r_dst.path, node_elem);
  }

  ViewerNodeViewerPathElem *viewer_node_elem = BKE_viewer_path_elem_new_viewer_node();
  viewer_node_elem->node_id = node.identifier;
  viewer_node_elem->base.ui_name = BLI_strdup(node.name);
  BLI_addtail(&r_dst.path, viewer_node_elem);
}

void activate_geometry_node(Main &bmain, SpaceNode &snode, bNode &node)
{
  wmWindowManager *wm = (wmWindowManager *)bmain.wm.first;
  if (wm == nullptr) {
    return;
  }
  for (bNode *iter_node : snode.edittree->all_nodes()) {
    if (iter_node->type == GEO_NODE_VIEWER) {
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
          sspreadsheet.object_eval_state = SPREADSHEET_OBJECT_EVAL_STATE_VIEWER_NODE;
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
  const Vector<const ViewerPathElem *, 16> elems_vec = viewer_path.path;
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
  const char *modifier_name =
      reinterpret_cast<const ModifierViewerPathElem &>(modifier_elem).modifier_name;
  if (modifier_name == nullptr) {
    return std::nullopt;
  }
  remaining_elems = remaining_elems.drop_front(1);
  Vector<const ViewerPathElem *> node_path;
  for (const ViewerPathElem *elem : remaining_elems.drop_back(1)) {
    if (!ELEM(elem->type, VIEWER_PATH_ELEM_TYPE_GROUP_NODE, VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE))
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
  return ViewerPathForGeometryNodesViewer{root_ob, modifier_name, node_path, viewer_node_id};
}

bool exists_geometry_nodes_viewer(const ViewerPathForGeometryNodesViewer &parsed_viewer_path)
{
  const NodesModifierData *modifier = nullptr;
  LISTBASE_FOREACH (const ModifierData *, md, &parsed_viewer_path.object->modifiers) {
    if (md->type != eModifierType_Nodes) {
      continue;
    }
    if (md->name != parsed_viewer_path.modifier_name) {
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
  const TreeZone *zone = nullptr;
  for (const ViewerPathElem *path_elem : parsed_viewer_path.node_path) {
    ngroup->ensure_topology_cache();
    const TreeZones *tree_zones = ngroup->zones();
    switch (path_elem->type) {
      case VIEWER_PATH_ELEM_TYPE_SIMULATION_ZONE: {
        const auto &typed_elem = *reinterpret_cast<const SimulationZoneViewerPathElem *>(
            path_elem);
        const TreeZone *next_zone = tree_zones->get_zone_by_node(typed_elem.sim_output_node_id);
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
        const TreeZone *parent_zone = tree_zones->get_zone_by_node(typed_elem.node_id);
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
      default: {
        BLI_assert_unreachable();
      }
    }
  }

  const bNode *viewer_node = ngroup->node_by_id(parsed_viewer_path.viewer_node_id);
  if (viewer_node == nullptr) {
    return false;
  }
  const TreeZones *tree_zones = ngroup->zones();
  if (tree_zones == nullptr) {
    return false;
  }
  if (tree_zones->get_zone_by_node(viewer_node->identifier) != zone) {
    return false;
  }
  return true;
}

bool is_active_geometry_nodes_viewer(const bContext &C, const ViewerPath &viewer_path)
{
  if (BLI_listbase_is_empty(&viewer_path.path)) {
    return false;
  }
  const ViewerPathElem *last_elem = static_cast<ViewerPathElem *>(viewer_path.path.last);
  if (last_elem->type != VIEWER_PATH_ELEM_TYPE_VIEWER_NODE) {
    return false;
  }
  const int32_t viewer_node_id =
      reinterpret_cast<const ViewerNodeViewerPathElem *>(last_elem)->node_id;

  const Main *bmain = CTX_data_main(&C);
  const wmWindowManager *wm = static_cast<wmWindowManager *>(bmain->wm.first);
  if (wm == nullptr) {
    return false;
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
        if (!BKE_viewer_path_equal(&viewer_path, &tmp_viewer_path)) {
          continue;
        }
        return true;
      }
    }
  }
  return false;
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

}  // namespace blender::ed::viewer_path
