/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "BLI_listbase.h"
#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_noise.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_string.h"
#include "BLI_string_utf8_symbols.h"
#include "BLI_vector_set.hh"

#include "DNA_anim_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"

#include "BKE_anim_data.hh"
#include "BKE_image.hh"
#include "BKE_lib_id.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_node.hh"
#include "BKE_node_enum.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_reference_lifetimes.hh"
#include "BKE_node_tree_update.hh"

#include "MOD_nodes.hh"

#include "NOD_geo_viewer.hh"
#include "NOD_geometry_nodes_dependencies.hh"
#include "NOD_geometry_nodes_gizmos.hh"
#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"
#include "NOD_socket_declarations.hh"
#include "NOD_sync_sockets.hh"
#include "NOD_texture.h"

#include "DEG_depsgraph_build.hh"

#include "BLT_translation.hh"

using namespace blender::nodes;

/**
 * These flags are used by the `changed_flag` field in #bNodeTree, #bNode and #bNodeSocket.
 * This enum is not part of the public API. It should be used through the `BKE_ntree_update_tag_*`
 * API.
 */
enum eNodeTreeChangedFlag {
  NTREE_CHANGED_NOTHING = 0,
  NTREE_CHANGED_ANY = (1 << 1),
  NTREE_CHANGED_NODE_PROPERTY = (1 << 2),
  NTREE_CHANGED_NODE_OUTPUT = (1 << 3),
  NTREE_CHANGED_LINK = (1 << 4),
  NTREE_CHANGED_REMOVED_NODE = (1 << 5),
  NTREE_CHANGED_REMOVED_SOCKET = (1 << 6),
  NTREE_CHANGED_SOCKET_PROPERTY = (1 << 7),
  NTREE_CHANGED_INTERNAL_LINK = (1 << 8),
  NTREE_CHANGED_PARENT = (1 << 9),
  NTREE_CHANGED_ALL = -1,
};

static void add_tree_tag(bNodeTree *ntree, const eNodeTreeChangedFlag flag)
{
  ntree->runtime->changed_flag |= flag;
  ntree->runtime->topology_cache_mutex.tag_dirty();
  ntree->runtime->tree_zones_cache_mutex.tag_dirty();
  ntree->runtime->inferenced_input_socket_usage_mutex.tag_dirty();
}

static void add_node_tag(bNodeTree *ntree, bNode *node, const eNodeTreeChangedFlag flag)
{
  add_tree_tag(ntree, flag);
  node->runtime->changed_flag |= flag;
}

static void add_socket_tag(bNodeTree *ntree, bNodeSocket *socket, const eNodeTreeChangedFlag flag)
{
  add_tree_tag(ntree, flag);
  socket->runtime->changed_flag |= flag;
}

namespace blender::bke {

/**
 * Common datatype priorities, works for compositor, shader and texture nodes alike
 * defines priority of datatype connection based on output type (to):
 * `<  0`: never connect these types.
 * `>= 0`: priority of connection (higher values chosen first).
 */
static int get_internal_link_type_priority(const bNodeSocketType *from, const bNodeSocketType *to)
{
  switch (to->type) {
    case SOCK_RGBA:
      switch (from->type) {
        case SOCK_RGBA:
          return 4;
        case SOCK_FLOAT:
          return 3;
        case SOCK_INT:
          return 2;
        case SOCK_BOOLEAN:
          return 1;
        default:
          return -1;
      }
    case SOCK_VECTOR:
      switch (from->type) {
        case SOCK_VECTOR:
          return 4;
        case SOCK_FLOAT:
          return 3;
        case SOCK_INT:
          return 2;
        case SOCK_BOOLEAN:
          return 1;
        default:
          return -1;
      }
    case SOCK_FLOAT:
      switch (from->type) {
        case SOCK_FLOAT:
          return 5;
        case SOCK_INT:
          return 4;
        case SOCK_BOOLEAN:
          return 3;
        case SOCK_RGBA:
          return 2;
        case SOCK_VECTOR:
          return 1;
        default:
          return -1;
      }
    case SOCK_INT:
      switch (from->type) {
        case SOCK_INT:
          return 5;
        case SOCK_FLOAT:
          return 4;
        case SOCK_BOOLEAN:
          return 3;
        case SOCK_RGBA:
          return 2;
        case SOCK_VECTOR:
          return 1;
        default:
          return -1;
      }
    case SOCK_BOOLEAN:
      switch (from->type) {
        case SOCK_BOOLEAN:
          return 5;
        case SOCK_INT:
          return 4;
        case SOCK_FLOAT:
          return 3;
        case SOCK_RGBA:
          return 2;
        case SOCK_VECTOR:
          return 1;
        default:
          return -1;
      }
    case SOCK_ROTATION:
      switch (from->type) {
        case SOCK_ROTATION:
          return 3;
        case SOCK_VECTOR:
          return 2;
        case SOCK_FLOAT:
          return 1;
        default:
          return -1;
      }
    default:
      break;
  }

  /* The rest of the socket types only allow an internal link if both the input and output socket
   * have the same type. If the sockets are custom, we check the idname instead. */
  if (to->type == from->type && (to->type != SOCK_CUSTOM || to->idname == from->idname)) {
    return 1;
  }

  return -1;
}

/* Check both the tree's own tags and the interface tags. */
static bool is_tree_changed(const bNodeTree &tree)
{
  return tree.runtime->changed_flag != NTREE_CHANGED_NOTHING ||
         tree.tree_interface.requires_dependent_tree_updates();
}

using TreeNodePair = std::pair<bNodeTree *, bNode *>;
using ObjectModifierPair = std::pair<Object *, ModifierData *>;
using NodeSocketPair = std::pair<bNode *, bNodeSocket *>;

/**
 * Cache common data about node trees from the #Main database that is expensive to retrieve on
 * demand every time.
 */
struct NodeTreeRelations {
 private:
  Main *bmain_;
  std::optional<Vector<bNodeTree *>> all_trees_;
  std::optional<MultiValueMap<bNodeTree *, TreeNodePair>> group_node_users_;
  std::optional<MultiValueMap<bNodeTree *, ObjectModifierPair>> modifiers_users_;

 public:
  NodeTreeRelations(Main *bmain) : bmain_(bmain) {}

  void ensure_all_trees()
  {
    if (all_trees_.has_value()) {
      return;
    }
    all_trees_.emplace();
    if (bmain_ == nullptr) {
      return;
    }

    FOREACH_NODETREE_BEGIN (bmain_, ntree, id) {
      all_trees_->append(ntree);
    }
    FOREACH_NODETREE_END;
  }

  void ensure_group_node_users()
  {
    if (group_node_users_.has_value()) {
      return;
    }
    group_node_users_.emplace();
    if (bmain_ == nullptr) {
      return;
    }

    this->ensure_all_trees();

    for (bNodeTree *ntree : *all_trees_) {
      for (bNode *node : ntree->all_nodes()) {
        if (node->id == nullptr) {
          continue;
        }
        ID *id = node->id;
        if (GS(id->name) == ID_NT) {
          bNodeTree *group = (bNodeTree *)id;
          group_node_users_->add(group, {ntree, node});
        }
      }
    }
  }

  void ensure_modifier_users()
  {
    if (modifiers_users_.has_value()) {
      return;
    }
    modifiers_users_.emplace();
    if (bmain_ == nullptr) {
      return;
    }

    LISTBASE_FOREACH (Object *, object, &bmain_->objects) {
      LISTBASE_FOREACH (ModifierData *, md, &object->modifiers) {
        if (md->type == eModifierType_Nodes) {
          NodesModifierData *nmd = (NodesModifierData *)md;
          if (nmd->node_group != nullptr) {
            modifiers_users_->add(nmd->node_group, {object, md});
          }
        }
      }
    }
  }

  Span<ObjectModifierPair> get_modifier_users(bNodeTree *ntree)
  {
    BLI_assert(modifiers_users_.has_value());
    return modifiers_users_->lookup(ntree);
  }

  Span<TreeNodePair> get_group_node_users(bNodeTree *ntree)
  {
    BLI_assert(group_node_users_.has_value());
    return group_node_users_->lookup(ntree);
  }

  Span<bNodeTree *> get_all_trees()
  {
    this->ensure_all_trees();
    return *all_trees_;
  }
};

struct TreeUpdateResult {
  bool interface_changed = false;
  bool output_changed = false;
};

class NodeTreeMainUpdater {
 private:
  Main *bmain_;
  const NodeTreeUpdateExtraParams &params_;
  Map<bNodeTree *, TreeUpdateResult> update_result_by_tree_;
  NodeTreeRelations relations_;
  bool needs_relations_update_ = false;

 public:
  NodeTreeMainUpdater(Main *bmain, const NodeTreeUpdateExtraParams &params)
      : bmain_(bmain), params_(params), relations_(bmain)
  {
  }

  void update()
  {
    Vector<bNodeTree *> changed_ntrees;
    FOREACH_NODETREE_BEGIN (bmain_, ntree, id) {
      if (is_tree_changed(*ntree)) {
        changed_ntrees.append(ntree);
      }
    }
    FOREACH_NODETREE_END;
    this->update_rooted(changed_ntrees);
  }

  void update_rooted(Span<bNodeTree *> root_ntrees)
  {
    if (root_ntrees.is_empty()) {
      return;
    }

    bool is_single_tree_update = false;

    if (root_ntrees.size() == 1) {
      bNodeTree *ntree = root_ntrees[0];
      if (!is_tree_changed(*ntree)) {
        return;
      }
      const TreeUpdateResult result = this->update_tree(*ntree);
      update_result_by_tree_.add_new(ntree, result);
      if (!result.interface_changed && !result.output_changed) {
        is_single_tree_update = true;
      }
    }

    if (!is_single_tree_update) {
      Vector<bNodeTree *> ntrees_in_order = this->get_tree_update_order(root_ntrees);
      for (bNodeTree *ntree : ntrees_in_order) {
        if (!is_tree_changed(*ntree)) {
          continue;
        }
        if (!update_result_by_tree_.contains(ntree)) {
          const TreeUpdateResult result = this->update_tree(*ntree);
          update_result_by_tree_.add_new(ntree, result);
        }
        const TreeUpdateResult result = update_result_by_tree_.lookup(ntree);
        Span<TreeNodePair> dependent_trees = relations_.get_group_node_users(ntree);
        if (result.output_changed) {
          for (const TreeNodePair &pair : dependent_trees) {
            add_node_tag(pair.first, pair.second, NTREE_CHANGED_NODE_OUTPUT);
          }
        }
        if (result.interface_changed) {
          for (const TreeNodePair &pair : dependent_trees) {
            add_node_tag(pair.first, pair.second, NTREE_CHANGED_NODE_PROPERTY);
          }
        }
      }
    }

    for (const auto item : update_result_by_tree_.items()) {
      bNodeTree *ntree = item.key;
      const TreeUpdateResult &result = item.value;

      this->reset_changed_flags(*ntree);

      if (result.interface_changed) {
        if (ntree->type == NTREE_GEOMETRY) {
          relations_.ensure_modifier_users();
          for (const ObjectModifierPair &pair : relations_.get_modifier_users(ntree)) {
            Object *object = pair.first;
            ModifierData *md = pair.second;

            if (md->type == eModifierType_Nodes) {
              MOD_nodes_update_interface(object, (NodesModifierData *)md);
            }
          }
        }
      }

      if (result.output_changed) {
        ntree->runtime->geometry_nodes_lazy_function_graph_info_mutex.tag_dirty();
      }

      ID *owner_id = BKE_id_owner_get(&ntree->id);
      ID &owner_or_self_id = owner_id ? *owner_id : ntree->id;
      if (params_.tree_changed_fn) {
        params_.tree_changed_fn(*ntree, owner_or_self_id);
      }
      if (params_.tree_output_changed_fn && result.output_changed) {
        params_.tree_output_changed_fn(*ntree, owner_or_self_id);
      }
    }

    if (needs_relations_update_) {
      if (bmain_) {
        DEG_relations_tag_update(bmain_);
      }
    }
    if (bmain_) {
      nodes::node_can_sync_cache_clear(*bmain_);
    }
  }

 private:
  enum class ToposortMark {
    None,
    Temporary,
    Permanent,
  };

  using ToposortMarkMap = Map<bNodeTree *, ToposortMark>;

  /**
   * Finds all trees that depend on the given trees (through node groups). Then those trees are
   * ordered such that all trees used by one tree come before it.
   */
  Vector<bNodeTree *> get_tree_update_order(Span<bNodeTree *> root_ntrees)
  {
    relations_.ensure_group_node_users();

    Set<bNodeTree *> trees_to_update = get_trees_to_update(root_ntrees);

    Vector<bNodeTree *> sorted_ntrees;

    ToposortMarkMap marks;
    for (bNodeTree *ntree : trees_to_update) {
      marks.add_new(ntree, ToposortMark::None);
    }
    for (bNodeTree *ntree : trees_to_update) {
      if (marks.lookup(ntree) == ToposortMark::None) {
        const bool cycle_detected = !this->get_tree_update_order__visit_recursive(
            ntree, marks, sorted_ntrees);
        /* This should be prevented by higher level operators. */
        BLI_assert(!cycle_detected);
        UNUSED_VARS_NDEBUG(cycle_detected);
      }
    }

    std::reverse(sorted_ntrees.begin(), sorted_ntrees.end());

    return sorted_ntrees;
  }

  bool get_tree_update_order__visit_recursive(bNodeTree *ntree,
                                              ToposortMarkMap &marks,
                                              Vector<bNodeTree *> &sorted_ntrees)
  {
    ToposortMark &mark = marks.lookup(ntree);
    if (mark == ToposortMark::Permanent) {
      return true;
    }
    if (mark == ToposortMark::Temporary) {
      /* There is a dependency cycle. */
      return false;
    }

    mark = ToposortMark::Temporary;

    for (const TreeNodePair &pair : relations_.get_group_node_users(ntree)) {
      this->get_tree_update_order__visit_recursive(pair.first, marks, sorted_ntrees);
    }
    sorted_ntrees.append(ntree);

    mark = ToposortMark::Permanent;
    return true;
  }

  Set<bNodeTree *> get_trees_to_update(Span<bNodeTree *> root_ntrees)
  {
    relations_.ensure_group_node_users();

    Set<bNodeTree *> reachable_trees;
    VectorSet<bNodeTree *> trees_to_check = root_ntrees;

    while (!trees_to_check.is_empty()) {
      bNodeTree *ntree = trees_to_check.pop();
      if (reachable_trees.add(ntree)) {
        for (const TreeNodePair &pair : relations_.get_group_node_users(ntree)) {
          trees_to_check.add(pair.first);
        }
      }
    }

    return reachable_trees;
  }

  TreeUpdateResult update_tree(bNodeTree &ntree)
  {
    TreeUpdateResult result;

    ntree.runtime->link_errors.clear();
    ntree.runtime->invalid_zone_output_node_ids.clear();
    ntree.runtime->shader_node_errors.clear();

    if (this->update_panel_toggle_names(ntree)) {
      result.interface_changed = true;
    }

    this->update_socket_link_and_use(ntree);
    this->update_individual_nodes(ntree);
    this->update_internal_links(ntree);
    this->update_generic_callback(ntree);
    this->remove_unused_previews_when_necessary(ntree);
    this->make_node_previews_dirty(ntree);

    this->propagate_runtime_flags(ntree);
    if (ELEM(ntree.type, NTREE_GEOMETRY, NTREE_COMPOSIT, NTREE_SHADER)) {
      if (this->propagate_enum_definitions(ntree)) {
        result.interface_changed = true;
      }
    }

    if (ntree.type == NTREE_GEOMETRY) {
      if (node_field_inferencing::update_field_inferencing(ntree)) {
        result.interface_changed = true;
      }
    }

    if (ELEM(ntree.type, NTREE_GEOMETRY, NTREE_COMPOSIT)) {
      if (node_structure_type_inferencing::update_structure_type_interface(ntree)) {
        result.interface_changed = true;
      }
    }

    if (ntree.type == NTREE_GEOMETRY) {
      this->update_from_field_inference(ntree);
      if (node_tree_reference_lifetimes::analyse_reference_lifetimes(ntree)) {
        result.interface_changed = true;
      }
      if (gizmos::update_tree_gizmo_propagation(ntree)) {
        result.interface_changed = true;
      }
    }

    if (ELEM(ntree.type, NTREE_GEOMETRY, NTREE_COMPOSIT)) {
      this->update_socket_shapes(ntree);
    }

    if (ntree.type == NTREE_GEOMETRY) {
      this->update_eval_dependencies(ntree);
    }

    result.output_changed = this->check_if_output_changed(ntree);

    this->update_socket_link_and_use(ntree);
    this->update_link_validation(ntree);

    if (this->update_nested_node_refs(ntree)) {
      result.interface_changed = true;
    }

    if (ntree.type == NTREE_TEXTURE) {
      ntreeTexCheckCyclics(&ntree);
    }

    if (ntree.tree_interface.requires_dependent_tree_updates()) {
      result.interface_changed = true;
    }

#ifndef NDEBUG
    /* Check the uniqueness of node identifiers. */
    Set<int32_t> node_identifiers;
    const Span<const bNode *> nodes = ntree.all_nodes();
    for (const int i : nodes.index_range()) {
      const bNode &node = *nodes[i];
      BLI_assert(node.identifier > 0);
      node_identifiers.add_new(node.identifier);
      BLI_assert(node.runtime->index_in_tree == i);
    }
#endif

    return result;
  }

  void update_socket_link_and_use(bNodeTree &tree)
  {
    tree.ensure_topology_cache();
    for (bNodeSocket *socket : tree.all_input_sockets()) {
      if (socket->directly_linked_links().is_empty()) {
        socket->link = nullptr;
      }
      else {
        socket->link = socket->directly_linked_links()[0];
      }
    }

    this->update_socket_used_tags(tree);
  }

  void update_socket_used_tags(bNodeTree &tree)
  {
    tree.ensure_topology_cache();
    for (bNodeSocket *socket : tree.all_sockets()) {
      const bool socket_is_linked = !socket->directly_linked_links().is_empty();
      SET_FLAG_FROM_TEST(socket->flag, socket_is_linked, SOCK_IS_LINKED);
    }
  }

  void update_individual_nodes(bNodeTree &ntree)
  {
    for (bNode *node : ntree.all_nodes()) {
      bke::node_declaration_ensure(ntree, *node);
      if (this->should_update_individual_node(ntree, *node)) {
        bke::bNodeType &ntype = *node->typeinfo;
        if (ntype.type_legacy == GEO_NODE_VIEWER) {
          this->remove_unused_geometry_nodes_viewer_sockets(ntree, *node);
        }
        if (ntype.declare) {
          /* Should have been created when the node was registered. */
          BLI_assert(ntype.static_declaration != nullptr);
          if (ntype.static_declaration->is_context_dependent) {
            nodes::update_node_declaration_and_sockets(ntree, *node);
          }
        }
        else if (node->is_undefined()) {
          /* If a node has become undefined (it generally was unregistered from Python), it does
           * not have a declaration anymore. */
          delete node->runtime->declaration;
          node->runtime->declaration = nullptr;
          LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
            socket->runtime->declaration = nullptr;
          }
          LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
            socket->runtime->declaration = nullptr;
          }
        }
        if (ntype.updatefunc) {
          ntype.updatefunc(&ntree, node);
        }
      }
    }
  }

  bool should_update_individual_node(const bNodeTree &ntree, const bNode &node)
  {
    if (ntree.runtime->changed_flag & NTREE_CHANGED_ANY) {
      return true;
    }
    if (node.runtime->changed_flag & NTREE_CHANGED_NODE_PROPERTY) {
      return true;
    }
    if (ntree.runtime->changed_flag & NTREE_CHANGED_LINK) {
      /* Currently we have no way to tell if a node needs to be updated when a link changed. */
      return true;
    }
    if (ntree.tree_interface.requires_dependent_tree_updates()) {
      if (node.is_group_input() || node.is_group_output()) {
        return true;
      }
    }
    /* Check paired simulation zone nodes. */
    if (all_zone_input_node_types().contains(node.type_legacy)) {
      const bNodeZoneType &zone_type = *zone_type_by_node_type(node.type_legacy);
      if (const bNode *output_node = zone_type.get_corresponding_output(ntree, node)) {
        if (output_node->runtime->changed_flag & NTREE_CHANGED_NODE_PROPERTY) {
          return true;
        }
      }
    }
    return false;
  }

  void remove_unused_geometry_nodes_viewer_sockets(bNodeTree &ntree, bNode &viewer_node)
  {
    ntree.ensure_topology_cache();
    Vector<int> item_indices_to_remove;
    auto &storage = *static_cast<NodeGeometryViewer *>(viewer_node.storage);
    for (const int i : IndexRange(storage.items_num)) {
      const NodeGeometryViewerItem &item = storage.items[i];
      if (!(item.flag & NODE_GEO_VIEWER_ITEM_FLAG_AUTO_REMOVE)) {
        continue;
      }
      const std::string identifier_str = GeoViewerItemsAccessor::socket_identifier_for_item(item);
      const bNodeSocket *socket = viewer_node.input_by_identifier(identifier_str.c_str());
      if (!socket) {
        continue;
      }
      if (!socket->is_directly_linked()) {
        item_indices_to_remove.append(i);
      }
    }
    std::reverse(item_indices_to_remove.begin(), item_indices_to_remove.end());
    for (const int i : item_indices_to_remove) {
      dna::array::remove_index(&storage.items,
                               &storage.items_num,
                               &storage.active_index,
                               i,
                               GeoViewerItemsAccessor::destruct_item);
    }
  }

  struct InternalLink {
    bNodeSocket *from;
    bNodeSocket *to;
    int multi_input_sort_id = 0;

    BLI_STRUCT_EQUALITY_OPERATORS_3(InternalLink, from, to, multi_input_sort_id);
  };

  const bNodeLink *first_non_dangling_link(const bNodeTree & /*ntree*/,
                                           const Span<const bNodeLink *> links) const
  {
    for (const bNodeLink *link : links) {
      if (!link->fromnode->is_dangling_reroute()) {
        return link;
      }
    }
    return nullptr;
  }

  void update_internal_links(bNodeTree &ntree)
  {
    bke::node_tree_runtime::AllowUsingOutdatedInfo allow_outdated_info{ntree};
    ntree.ensure_topology_cache();
    for (bNode *node : ntree.all_nodes()) {
      if (!this->should_update_individual_node(ntree, *node)) {
        continue;
      }
      /* Find all expected internal links. */
      Vector<InternalLink> expected_internal_links;
      for (const bNodeSocket *output_socket : node->output_sockets()) {
        if (!output_socket->is_available()) {
          continue;
        }
        if (output_socket->runtime->declaration &&
            output_socket->runtime->declaration->no_mute_links)
        {
          continue;
        }
        const bNodeSocket *input_socket = this->find_internally_linked_input(ntree, output_socket);
        if (input_socket == nullptr) {
          continue;
        }

        const Span<const bNodeLink *> connected_links = input_socket->directly_linked_links();
        const bNodeLink *connected_link = first_non_dangling_link(ntree, connected_links);

        const int index = connected_link ? connected_link->multi_input_sort_id :
                                           std::max<int>(0, connected_links.size() - 1);
        expected_internal_links.append(InternalLink{const_cast<bNodeSocket *>(input_socket),
                                                    const_cast<bNodeSocket *>(output_socket),
                                                    index});
      }

      /* Rebuilt internal links if they have changed. */
      if (node->runtime->internal_links.size() != expected_internal_links.size()) {
        this->update_internal_links_in_node(ntree, *node, expected_internal_links);
        continue;
      }

      const bool all_expected_internal_links_exist = std::all_of(
          node->runtime->internal_links.begin(),
          node->runtime->internal_links.end(),
          [&](const bNodeLink &link) {
            const InternalLink internal_link{link.fromsock, link.tosock, link.multi_input_sort_id};
            return expected_internal_links.as_span().contains(internal_link);
          });

      if (all_expected_internal_links_exist) {
        continue;
      }

      this->update_internal_links_in_node(ntree, *node, expected_internal_links);
    }
  }

  const bNodeSocket *find_internally_linked_input(const bNodeTree &ntree,
                                                  const bNodeSocket *output_socket)
  {
    const bNode &node = output_socket->owner_node();
    if (node.typeinfo->internally_linked_input) {
      return node.typeinfo->internally_linked_input(ntree, node, *output_socket);
    }

    const bNodeSocket *selected_socket = nullptr;
    int selected_priority = -1;
    bool selected_is_linked = false;
    for (const bNodeSocket *input_socket : node.input_sockets()) {
      if (!input_socket->is_available()) {
        continue;
      }
      if (input_socket->runtime->declaration && input_socket->runtime->declaration->no_mute_links)
      {
        continue;
      }
      const int priority = get_internal_link_type_priority(input_socket->typeinfo,
                                                           output_socket->typeinfo);
      if (priority < 0) {
        continue;
      }
      const bool is_linked = input_socket->is_directly_linked();
      const bool is_preferred = priority > selected_priority || (is_linked && !selected_is_linked);
      if (!is_preferred) {
        continue;
      }
      selected_socket = input_socket;
      selected_priority = priority;
      selected_is_linked = is_linked;
    }
    return selected_socket;
  }

  void update_internal_links_in_node(bNodeTree &ntree,
                                     bNode &node,
                                     Span<InternalLink> internal_links)
  {
    node.runtime->internal_links.clear();
    node.runtime->internal_links.reserve(internal_links.size());
    for (const InternalLink &internal_link : internal_links) {
      bNodeLink link{};
      link.fromnode = &node;
      link.fromsock = internal_link.from;
      link.tonode = &node;
      link.tosock = internal_link.to;
      link.multi_input_sort_id = internal_link.multi_input_sort_id;
      link.flag |= NODE_LINK_VALID;
      node.runtime->internal_links.append(link);
    }
    BKE_ntree_update_tag_node_internal_link(&ntree, &node);
  }

  void update_generic_callback(bNodeTree &ntree)
  {
    if (ntree.typeinfo->update == nullptr) {
      return;
    }
    ntree.typeinfo->update(&ntree);
  }

  void remove_unused_previews_when_necessary(bNodeTree &ntree)
  {
    /* Don't trigger preview removal when only those flags are set. */
    const uint32_t allowed_flags = NTREE_CHANGED_LINK | NTREE_CHANGED_SOCKET_PROPERTY |
                                   NTREE_CHANGED_NODE_PROPERTY | NTREE_CHANGED_NODE_OUTPUT;
    if ((ntree.runtime->changed_flag & allowed_flags) == ntree.runtime->changed_flag) {
      return;
    }
    blender::bke::node_preview_remove_unused(&ntree);
  }

  void make_node_previews_dirty(bNodeTree &ntree)
  {
    ntree.runtime->previews_refresh_state++;
    for (bNode *node : ntree.all_nodes()) {
      if (!node->is_group()) {
        continue;
      }
      if (bNodeTree *nested_tree = reinterpret_cast<bNodeTree *>(node->id)) {
        this->make_node_previews_dirty(*nested_tree);
      }
    }
  }

  void propagate_runtime_flags(const bNodeTree &ntree)
  {
    ntree.ensure_topology_cache();

    ntree.runtime->runtime_flag = 0;

    for (const bNode *group_node : ntree.group_nodes()) {
      const bNodeTree *group = reinterpret_cast<bNodeTree *>(group_node->id);
      if (group != nullptr) {
        ntree.runtime->runtime_flag |= group->runtime->runtime_flag;
      }
    }

    if (ntree.type == NTREE_SHADER) {
      /* Check if the tree itself has an animated image. */
      for (const StringRefNull idname : {"ShaderNodeTexImage", "ShaderNodeTexEnvironment"}) {
        for (const bNode *node : ntree.nodes_by_type(idname)) {
          Image *image = reinterpret_cast<Image *>(node->id);
          if (image != nullptr && BKE_image_is_animated(image)) {
            ntree.runtime->runtime_flag |= NTREE_RUNTIME_FLAG_HAS_IMAGE_ANIMATION;
            break;
          }
        }
      }
      /* Check if the tree has a material output. */
      for (const StringRefNull idname : {"ShaderNodeOutputMaterial",
                                         "ShaderNodeOutputLight",
                                         "ShaderNodeOutputWorld",
                                         "ShaderNodeOutputAOV"})
      {
        const Span<const bNode *> nodes = ntree.nodes_by_type(idname);
        if (!nodes.is_empty()) {
          ntree.runtime->runtime_flag |= NTREE_RUNTIME_FLAG_HAS_MATERIAL_OUTPUT;
          break;
        }
      }
    }
    if (ntree.type == NTREE_GEOMETRY) {
      /* Check if there is a simulation zone. */
      if (!ntree.nodes_by_type("GeometryNodeSimulationOutput").is_empty()) {
        ntree.runtime->runtime_flag |= NTREE_RUNTIME_FLAG_HAS_SIMULATION_ZONE;
      }
    }
  }

  void update_from_field_inference(bNodeTree &ntree)
  {
    /* Automatically tag a bake item as attribute when the input is a field. The flag should not be
     * removed automatically even when the field input is disconnected because the baked data may
     * still contain attribute data instead of a single value. */
    for (bNode *node : ntree.nodes_by_type("GeometryNodeBake")) {
      NodeGeometryBake &storage = *static_cast<NodeGeometryBake *>(node->storage);
      for (const int i : IndexRange(storage.items_num)) {
        const bNodeSocket &socket = node->input_socket(i);
        NodeGeometryBakeItem &item = storage.items[i];
        if (socket.may_be_field()) {
          item.flag |= GEO_NODE_BAKE_ITEM_IS_ATTRIBUTE;
        }
      }
    }
  }

  static int get_socket_shape(const bNodeSocket &socket,
                              const bool use_inferred_structure_type = false)
  {
    if (nodes::socket_type_always_single(socket.typeinfo->type)) {
      return SOCK_DISPLAY_SHAPE_LINE;
    }
    const SocketDeclaration *decl = socket.runtime->declaration;
    if (!decl) {
      return SOCK_DISPLAY_SHAPE_CIRCLE;
    }
    if (decl->identifier == "__extend__") {
      return SOCK_DISPLAY_SHAPE_CIRCLE;
    }
    const StructureType display_structure_type = use_inferred_structure_type ?
                                                     socket.runtime->inferred_structure_type :
                                                     decl->structure_type;
    switch (display_structure_type) {
      case StructureType::Single:
        return SOCK_DISPLAY_SHAPE_LINE;
      case StructureType::Dynamic:
        return SOCK_DISPLAY_SHAPE_CIRCLE;
      case StructureType::Field:
        return SOCK_DISPLAY_SHAPE_DIAMOND;
      case StructureType::Grid:
        return SOCK_DISPLAY_SHAPE_VOLUME_GRID;
      case StructureType::List:
        return SOCK_DISPLAY_SHAPE_LIST;
    }
    BLI_assert_unreachable();
    return SOCK_DISPLAY_SHAPE_CIRCLE;
  }

  void update_socket_shapes(bNodeTree &ntree)
  {
    ntree.ensure_topology_cache();
    for (bNode *node : ntree.all_nodes()) {
      if (node->is_undefined()) {
        continue;
      }
      const bke::bNodeZoneType *closure_zone_type = bke::zone_type_by_node_type(
          NODE_CLOSURE_OUTPUT);
      switch (node->type_legacy) {
        case NODE_REROUTE: {
          node->input_socket(0).display_shape = SOCK_DISPLAY_SHAPE_CIRCLE;
          node->output_socket(0).display_shape = SOCK_DISPLAY_SHAPE_CIRCLE;
          break;
        }
        case NODE_GROUP_OUTPUT:
        case NODE_GROUP_INPUT: {
          for (bNodeSocket *socket : node->input_sockets()) {
            socket->display_shape = get_socket_shape(*socket, true);
          }
          for (bNodeSocket *socket : node->output_sockets()) {
            socket->display_shape = get_socket_shape(*socket, true);
          }
          break;
        }
        case NODE_COMBINE_BUNDLE: {
          const auto &storage = *static_cast<const NodeCombineBundle *>(node->storage);
          for (const int i : IndexRange(storage.items_num)) {
            const NodeCombineBundleItem &item = storage.items[i];
            bNodeSocket &socket = node->input_socket(i);
            socket.display_shape = get_socket_shape(
                socket, item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO);
          }
          break;
        }
        case NODE_SEPARATE_BUNDLE: {
          const auto &storage = *static_cast<const NodeSeparateBundle *>(node->storage);
          for (const int i : IndexRange(storage.items_num)) {
            const NodeSeparateBundleItem &item = storage.items[i];
            bNodeSocket &socket = node->output_socket(i);
            socket.display_shape = get_socket_shape(
                socket, item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO);
          }
          break;
        }
        case NODE_CLOSURE_INPUT: {
          if (const bNode *closure_output_node = closure_zone_type->get_corresponding_output(
                  ntree, *node))
          {
            const auto &storage = *static_cast<const NodeClosureOutput *>(
                closure_output_node->storage);
            for (const int i : IndexRange(storage.input_items.items_num)) {
              const NodeClosureInputItem &item = storage.input_items.items[i];
              bNodeSocket &socket = node->output_socket(i);
              socket.display_shape = get_socket_shape(
                  socket, item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO);
            }
          }
          break;
        }
        case NODE_CLOSURE_OUTPUT: {
          const auto &storage = *static_cast<const NodeClosureOutput *>(node->storage);
          for (const int i : IndexRange(storage.output_items.items_num)) {
            const NodeClosureOutputItem &item = storage.output_items.items[i];
            bNodeSocket &socket = node->input_socket(i);
            socket.display_shape = get_socket_shape(
                socket, item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO);
          }
          break;
        }
        case NODE_EVALUATE_CLOSURE: {
          const auto &storage = *static_cast<const NodeEvaluateClosure *>(node->storage);
          for (const int i : IndexRange(storage.input_items.items_num)) {
            const NodeEvaluateClosureInputItem &item = storage.input_items.items[i];
            bNodeSocket &socket = node->input_socket(i + 1);
            socket.display_shape = get_socket_shape(
                socket, item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO);
          }
          for (const int i : IndexRange(storage.output_items.items_num)) {
            const NodeEvaluateClosureOutputItem &item = storage.output_items.items[i];
            bNodeSocket &socket = node->output_socket(i);
            socket.display_shape = get_socket_shape(
                socket, item.structure_type == NODE_INTERFACE_SOCKET_STRUCTURE_TYPE_AUTO);
          }
          break;
        }
        default: {
          /* For other nodes we just use the static structure types defined in the declaration. */
          for (bNodeSocket *socket : node->input_sockets()) {
            socket->display_shape = get_socket_shape(*socket);
          }
          for (bNodeSocket *socket : node->output_sockets()) {
            socket->display_shape = get_socket_shape(*socket);
          }
          break;
        }
      }
    }
  }

  void update_eval_dependencies(bNodeTree &ntree)
  {
    ntree.ensure_topology_cache();
    nodes::GeometryNodesEvalDependencies new_deps =
        nodes::gather_geometry_nodes_eval_dependencies_with_cache(ntree);

    /* Check if the dependencies have changed. */
    if (!ntree.runtime->geometry_nodes_eval_dependencies ||
        new_deps != *ntree.runtime->geometry_nodes_eval_dependencies)
    {
      needs_relations_update_ = true;
      ntree.runtime->geometry_nodes_eval_dependencies =
          std::make_unique<nodes::GeometryNodesEvalDependencies>(std::move(new_deps));
    }
  }

  bool propagate_enum_definitions(bNodeTree &ntree)
  {
    ntree.ensure_interface_cache();

    /* Propagation from right to left to determine which enum
     * definition to use for menu sockets. */
    for (bNode *node : ntree.toposort_right_to_left()) {
      const bool node_updated = this->should_update_individual_node(ntree, *node);

      Vector<bNodeSocket *> locally_defined_enums;
      if (node->is_type("GeometryNodeMenuSwitch")) {
        bNodeSocket &enum_input = node->input_socket(0);
        BLI_assert(enum_input.is_available() && enum_input.type == SOCK_MENU);
        /* Generate new enum items when the node has changed, otherwise keep existing items. */
        if (node_updated) {
          const NodeMenuSwitch &storage = *static_cast<NodeMenuSwitch *>(node->storage);
          const RuntimeNodeEnumItems *enum_items = this->create_runtime_enum_items(
              storage.enum_definition);

          this->set_enum_ptr(*enum_input.default_value_typed<bNodeSocketValueMenu>(), enum_items);
          /* Remove initial user. */
          enum_items->remove_user_and_delete_if_last();
        }
        locally_defined_enums.append(&enum_input);
      }
      else if (!node->is_group()) {
        /* Gather built-in menus defined by this node. */
        for (bNodeSocket *input_socket : node->input_sockets()) {
          if (!input_socket->is_available()) {
            continue;
          }
          if (input_socket->type != SOCK_MENU) {
            continue;
          }
          const auto *socket_decl = dynamic_cast<const nodes::decl::Menu *>(
              input_socket->runtime->declaration);
          if (!socket_decl) {
            continue;
          }
          this->set_enum_ptr(*input_socket->default_value_typed<bNodeSocketValueMenu>(),
                             socket_decl->items.get());
          locally_defined_enums.append(input_socket);
        }
      }

      /* Clear current enum references. */
      for (bNodeSocket *socket : node->input_sockets()) {
        if (socket->is_available() && socket->type == SOCK_MENU &&
            !locally_defined_enums.contains(socket))
        {
          clear_enum_reference(*socket);
        }
      }
      for (bNodeSocket *socket : node->output_sockets()) {
        if (socket->is_available() && socket->type == SOCK_MENU) {
          clear_enum_reference(*socket);
        }
      }

      /* Propagate enum references from output links. */
      for (bNodeSocket *output : node->output_sockets()) {
        if (!output->is_available() || output->type != SOCK_MENU) {
          continue;
        }
        for (const bNodeSocket *input : output->directly_linked_sockets()) {
          if (!input->is_available() || input->type != SOCK_MENU) {
            continue;
          }
          this->update_socket_enum_definition(*output->default_value_typed<bNodeSocketValueMenu>(),
                                              *input->default_value_typed<bNodeSocketValueMenu>());
        }
      }

      if (node->is_group()) {
        /* Node groups expose internal enum definitions. */
        if (node->id == nullptr) {
          continue;
        }
        const bNodeTree *group_tree = reinterpret_cast<bNodeTree *>(node->id);
        group_tree->ensure_interface_cache();

        for (const int socket_i : group_tree->interface_inputs().index_range()) {
          bNodeSocket &input = *node->input_sockets()[socket_i];
          const bNodeTreeInterfaceSocket &iosocket = *group_tree->interface_inputs()[socket_i];
          BLI_assert(STREQ(input.identifier, iosocket.identifier));
          if (input.is_available() && input.type == SOCK_MENU) {
            BLI_assert(STREQ(iosocket.socket_type, "NodeSocketMenu"));
            this->update_socket_enum_definition(
                *input.default_value_typed<bNodeSocketValueMenu>(),
                *static_cast<bNodeSocketValueMenu *>(iosocket.socket_data));
          }
        }
      }
      else if (node->is_type("GeometryNodeMenuSwitch")) {
        /* First input is always the node's own menu, propagate only to the enum case inputs. */
        const bNodeSocket *output = node->output_sockets().first();
        for (bNodeSocket *input : node->input_sockets().drop_front(1)) {
          if (input->is_available() && input->type == SOCK_MENU) {
            this->update_socket_enum_definition(
                *input->default_value_typed<bNodeSocketValueMenu>(),
                *output->default_value_typed<bNodeSocketValueMenu>());
          }
        }
      }
      else if (node->is_type("GeometryNodeForeachGeometryElementInput")) {
        /* Propagate menu from element inputs to field inputs. */
        BLI_assert(node->input_sockets().size() == node->output_sockets().size());
        /* Inputs Geometry, Selection and outputs Index, Element are ignored. */
        const IndexRange sockets = node->input_sockets().index_range().drop_front(2);
        for (const int socket_i : sockets) {
          bNodeSocket *input = node->input_sockets()[socket_i];
          bNodeSocket *output = node->output_sockets()[socket_i];
          if (input->is_available() && input->type == SOCK_MENU && output->is_available() &&
              output->type == SOCK_MENU)
          {
            this->update_socket_enum_definition(
                *input->default_value_typed<bNodeSocketValueMenu>(),
                *output->default_value_typed<bNodeSocketValueMenu>());
          }
        }
      }
      else {
        /* Propagate over internal relations. */
        /* XXX Placeholder implementation just propagates all outputs
         * to all inputs for built-in nodes This could perhaps use
         * input/output relations to handle propagation generically? */
        for (bNodeSocket *input : node->input_sockets()) {
          if (input->is_available() && input->type == SOCK_MENU) {
            for (const bNodeSocket *output : node->output_sockets()) {
              if (output->is_available() && output->type == SOCK_MENU) {
                this->update_socket_enum_definition(
                    *input->default_value_typed<bNodeSocketValueMenu>(),
                    *output->default_value_typed<bNodeSocketValueMenu>());
              }
            }
          }
        }
      }
    }

    /* Find conflicts between on corresponding menu sockets on different group input nodes. */
    const Span<bNode *> group_input_nodes = ntree.group_input_nodes();
    for (const int interface_input_i : ntree.interface_inputs().index_range()) {
      const bNodeTreeInterfaceSocket &interface_socket =
          *ntree.interface_inputs()[interface_input_i];
      if (interface_socket.socket_type != StringRef("NodeSocketMenu")) {
        continue;
      }
      const RuntimeNodeEnumItems *found_enum_items = nullptr;
      bool found_conflict = false;
      for (bNode *input_node : group_input_nodes) {
        const bNodeSocket &socket = input_node->output_socket(interface_input_i);
        const auto &socket_value = *socket.default_value_typed<bNodeSocketValueMenu>();
        if (socket_value.has_conflict()) {
          found_conflict = true;
          break;
        }
        if (found_enum_items == nullptr) {
          found_enum_items = socket_value.enum_items;
        }
        else if (socket_value.enum_items != nullptr) {
          if (found_enum_items != socket_value.enum_items) {
            found_conflict = true;
            break;
          }
        }
      }
      if (found_conflict) {
        /* Make sure that all group input sockets know that there is a conflict. */
        for (bNode *input_node : group_input_nodes) {
          bNodeSocket &socket = input_node->output_socket(interface_input_i);
          auto &socket_value = *socket.default_value_typed<bNodeSocketValueMenu>();
          if (socket_value.enum_items) {
            socket_value.enum_items->remove_user_and_delete_if_last();
            socket_value.enum_items = nullptr;
          }
          socket_value.runtime_flag |= NodeSocketValueMenuRuntimeFlag::NODE_MENU_ITEMS_CONFLICT;
        }
      }
      else if (found_enum_items != nullptr) {
        /* Make sure all corresponding menu sockets have the same menu reference. */
        for (bNode *input_node : group_input_nodes) {
          bNodeSocket &socket = input_node->output_socket(interface_input_i);
          auto &socket_value = *socket.default_value_typed<bNodeSocketValueMenu>();
          if (socket_value.enum_items == nullptr) {
            found_enum_items->add_user();
            socket_value.enum_items = found_enum_items;
          }
        }
      }
    }

    /* Build list of new enum items for the node tree interface. */
    Vector<bNodeSocketValueMenu> interface_enum_items(ntree.interface_inputs().size(), {0});
    for (const bNode *group_input_node : ntree.group_input_nodes()) {
      for (const int socket_i : ntree.interface_inputs().index_range()) {
        const bNodeSocket &output = *group_input_node->output_sockets()[socket_i];

        if (output.is_available() && output.type == SOCK_MENU) {
          this->update_socket_enum_definition(interface_enum_items[socket_i],
                                              *output.default_value_typed<bNodeSocketValueMenu>());
        }
      }
    }

    /* Move enum items to the interface and detect if anything changed. */
    bool changed = false;
    for (const int socket_i : ntree.interface_inputs().index_range()) {
      bNodeTreeInterfaceSocket &iosocket = *ntree.interface_inputs()[socket_i];
      if (STREQ(iosocket.socket_type, "NodeSocketMenu")) {
        bNodeSocketValueMenu &dst = *static_cast<bNodeSocketValueMenu *>(iosocket.socket_data);
        const bNodeSocketValueMenu &src = interface_enum_items[socket_i];
        if (dst.enum_items != src.enum_items || dst.has_conflict() != src.has_conflict()) {
          changed = true;
          if (dst.enum_items) {
            dst.enum_items->remove_user_and_delete_if_last();
          }
          /* Items are moved, no need to change user count. */
          dst.enum_items = src.enum_items;
          SET_FLAG_FROM_TEST(dst.runtime_flag, src.has_conflict(), NODE_MENU_ITEMS_CONFLICT);
        }
        else {
          /* If the item isn't move make sure it gets released again. */
          if (src.enum_items) {
            src.enum_items->remove_user_and_delete_if_last();
          }
        }
      }
    }

    return changed;
  }

  /**
   * Make a runtime copy of the DNA enum items.
   * The runtime items list is shared by sockets.
   */
  const RuntimeNodeEnumItems *create_runtime_enum_items(const NodeEnumDefinition &enum_def)
  {
    RuntimeNodeEnumItems *enum_items = new RuntimeNodeEnumItems();
    enum_items->items.reinitialize(enum_def.items_num);
    for (const int i : enum_def.items().index_range()) {
      const NodeEnumItem &src = enum_def.items()[i];
      RuntimeNodeEnumItem &dst = enum_items->items[i];

      dst.identifier = src.identifier;
      dst.name = src.name ? src.name : "";
      dst.description = src.description ? src.description : "";
    }
    return enum_items;
  }

  void clear_enum_reference(bNodeSocket &socket)
  {
    BLI_assert(socket.is_available() && socket.type == SOCK_MENU);
    bNodeSocketValueMenu &default_value = *socket.default_value_typed<bNodeSocketValueMenu>();
    this->reset_enum_ptr(default_value);
    default_value.runtime_flag &= ~NODE_MENU_ITEMS_CONFLICT;
  }

  void update_socket_enum_definition(bNodeSocketValueMenu &dst, const bNodeSocketValueMenu &src)
  {
    if (dst.has_conflict()) {
      /* Target enum already has a conflict. */
      BLI_assert(dst.enum_items == nullptr);
      return;
    }

    if (src.has_conflict()) {
      /* Target conflict if any source enum has a conflict. */
      this->reset_enum_ptr(dst);
      dst.runtime_flag |= NODE_MENU_ITEMS_CONFLICT;
    }
    else if (!dst.enum_items) {
      /* First connection, set the reference. */
      this->set_enum_ptr(dst, src.enum_items);
    }
    else if (src.enum_items && dst.enum_items != src.enum_items) {
      /* Error if enum ref does not match other connections. */
      this->reset_enum_ptr(dst);
      dst.runtime_flag |= NODE_MENU_ITEMS_CONFLICT;
    }
  }

  void reset_enum_ptr(bNodeSocketValueMenu &dst)
  {
    if (dst.enum_items) {
      dst.enum_items->remove_user_and_delete_if_last();
      dst.enum_items = nullptr;
    }
  }

  void set_enum_ptr(bNodeSocketValueMenu &dst, const RuntimeNodeEnumItems *enum_items)
  {
    if (dst.enum_items) {
      dst.enum_items->remove_user_and_delete_if_last();
      dst.enum_items = nullptr;
    }
    if (enum_items) {
      enum_items->add_user();
      dst.enum_items = enum_items;
    }
  }

  void update_link_validation(bNodeTree &ntree)
  {
    /* Tests if enum references are undefined. */
    const auto is_invalid_enum_ref = [](const bNodeSocket &socket) -> bool {
      if (socket.type == SOCK_MENU) {
        return socket.default_value_typed<bNodeSocketValueMenu>()->enum_items == nullptr;
      }
      return false;
    };

    const bNodeTreeZones *fallback_zones = nullptr;
    if (ELEM(ntree.type, NTREE_GEOMETRY, NTREE_SHADER) && !ntree.zones() &&
        ntree.runtime->last_valid_zones)
    {
      fallback_zones = ntree.runtime->last_valid_zones.get();
    }

    LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
      link->flag |= NODE_LINK_VALID;
      if (!link->fromsock->is_available() || !link->tosock->is_available()) {
        link->flag &= ~NODE_LINK_VALID;
        continue;
      }
      if (is_invalid_enum_ref(*link->fromsock) || is_invalid_enum_ref(*link->tosock)) {
        link->flag &= ~NODE_LINK_VALID;
        ntree.runtime->link_errors.add(
            NodeLinkKey{*link},
            NodeLinkError{TIP_("Use node groups to reuse the same menu multiple times")});
        continue;
      }
      const bNode &from_node = *link->fromnode;
      const bNode &to_node = *link->tonode;
      if (from_node.runtime->toposort_left_to_right_index >
          to_node.runtime->toposort_left_to_right_index)
      {
        link->flag &= ~NODE_LINK_VALID;
        ntree.runtime->link_errors.add(
            NodeLinkKey{*link},
            NodeLinkError{TIP_("The links form a cycle which is not supported")});
        continue;
      }
      if (ntree.typeinfo->validate_link) {
        const eNodeSocketDatatype from_type = eNodeSocketDatatype(link->fromsock->type);
        const eNodeSocketDatatype to_type = eNodeSocketDatatype(link->tosock->type);
        if (!ntree.typeinfo->validate_link(from_type, to_type)) {
          link->flag &= ~NODE_LINK_VALID;
          ntree.runtime->link_errors.add(
              NodeLinkKey{*link},
              NodeLinkError{fmt::format("{}: {} " BLI_STR_UTF8_BLACK_RIGHT_POINTING_SMALL_TRIANGLE
                                        " {}",
                                        TIP_("Conversion is not supported"),
                                        TIP_(link->fromsock->typeinfo->label),
                                        TIP_(link->tosock->typeinfo->label))});
          continue;
        }
      }
      if (fallback_zones) {
        if (!fallback_zones->link_between_sockets_is_allowed(*link->fromsock, *link->tosock)) {
          if (const bNodeTreeZone *from_zone = fallback_zones->get_zone_by_socket(*link->fromsock))
          {
            ntree.runtime->invalid_zone_output_node_ids.add(*from_zone->output_node_id);
          }

          link->flag &= ~NODE_LINK_VALID;
          ntree.runtime->link_errors.add(
              NodeLinkKey{*link},
              NodeLinkError{TIP_("Links can only go into a zone but not out")});
          continue;
        }
      }
      if (const char *error = this->get_structure_type_link_error(*link)) {
        link->flag &= ~NODE_LINK_VALID;
        ntree.runtime->link_errors.add(NodeLinkKey{*link}, NodeLinkError{error});
        continue;
      }
    }
  }

  const char *get_structure_type_link_error(const bNodeLink &link)
  {
    const nodes::StructureType from_inferred_type =
        link.fromsock->runtime->inferred_structure_type;
    if (from_inferred_type == StructureType::Dynamic) {
      /* Showing errors in this case results in many false positives in cases where Blender is not
       * sure what the actual type is. */
      return nullptr;
    }
    const int from_shape = link.fromsock->display_shape;
    const int to_shape = link.tosock->display_shape;
    switch (to_shape) {
      case SOCK_DISPLAY_SHAPE_CIRCLE: {
        return nullptr;
      }
      case SOCK_DISPLAY_SHAPE_LINE: {
        if (from_shape == SOCK_DISPLAY_SHAPE_LINE) {
          return nullptr;
        }
        if (from_inferred_type == StructureType::Single) {
          return nullptr;
        }
        return TIP_("Input expects a single value");
      }
      case SOCK_DISPLAY_SHAPE_DIAMOND: {
        if (ELEM(from_shape, SOCK_DISPLAY_SHAPE_LINE, SOCK_DISPLAY_SHAPE_DIAMOND)) {
          return nullptr;
        }
        if (ELEM(from_inferred_type, StructureType::Single, StructureType::Field)) {
          return nullptr;
        }
        return TIP_("Input expects a field or single value");
      }
      case SOCK_DISPLAY_SHAPE_VOLUME_GRID: {
        if (from_shape == SOCK_DISPLAY_SHAPE_VOLUME_GRID) {
          return nullptr;
        }
        if (from_inferred_type == StructureType::Grid) {
          return nullptr;
        }
        return TIP_("Input expects a volume grid");
      }
      case SOCK_DISPLAY_SHAPE_LIST: {
        if (from_shape == SOCK_DISPLAY_SHAPE_LIST) {
          return nullptr;
        }
        if (from_inferred_type == StructureType::List) {
          return nullptr;
        }
        return TIP_("Input expects a list");
      }
    }
    return nullptr;
  }

  bool check_if_output_changed(const bNodeTree &tree)
  {
    tree.ensure_topology_cache();

    /* Compute a hash that represents the node topology connected to the output. This always has
     * to be updated even if it is not used to detect changes right now. Otherwise
     * #btree.runtime.output_topology_hash will go out of date. */
    const Vector<const bNodeSocket *> tree_output_sockets = this->find_output_sockets(tree);
    const uint32_t old_topology_hash = tree.runtime->output_topology_hash;
    const uint32_t new_topology_hash = this->get_combined_socket_topology_hash(
        tree, tree_output_sockets);
    tree.runtime->output_topology_hash = new_topology_hash;

    if (const AnimData *adt = BKE_animdata_from_id(&tree.id)) {
      /* Drivers may copy values in the node tree around arbitrarily and may cause the output to
       * change even if it wouldn't without drivers. Only some special drivers like `frame/5` can
       * be used without causing updates all the time currently. In the future we could try to
       * handle other drivers better as well.
       * Note that this optimization only works in practice when the depsgraph didn't also get a
       * copy-on-evaluation tag for the node tree (which happens when changing node properties). It
       * does work in a few situations like adding reroutes and duplicating nodes though. */
      LISTBASE_FOREACH (const FCurve *, fcurve, &adt->drivers) {
        const ChannelDriver *driver = fcurve->driver;
        const StringRef expression = driver->expression;
        if (expression.startswith("frame")) {
          const StringRef remaining_expression = expression.drop_known_prefix("frame");
          if (remaining_expression.find_first_not_of(" */+-0123456789.") == StringRef::not_found) {
            continue;
          }
        }
        /* Unrecognized driver, assume that the output always changes. */
        return true;
      }
    }

    if (tree.runtime->changed_flag & NTREE_CHANGED_ANY) {
      return true;
    }

    if (old_topology_hash != new_topology_hash) {
      return true;
    }

    /* The topology hash can only be used when only topology-changing operations have been done.
     */
    if (tree.runtime->changed_flag ==
        (tree.runtime->changed_flag & (NTREE_CHANGED_LINK | NTREE_CHANGED_REMOVED_NODE)))
    {
      if (old_topology_hash == new_topology_hash) {
        return false;
      }
    }

    if (!this->check_if_socket_outputs_changed_based_on_flags(tree, tree_output_sockets)) {
      return false;
    }

    return true;
  }

  Vector<const bNodeSocket *> find_output_sockets(const bNodeTree &tree)
  {
    Vector<const bNodeSocket *> sockets;
    for (const bNode *node : tree.all_nodes()) {
      if (!this->is_output_node(*node)) {
        continue;
      }
      for (const bNodeSocket *socket : node->input_sockets()) {
        if (!STREQ(socket->idname, "NodeSocketVirtual")) {
          sockets.append(socket);
        }
      }
    }
    return sockets;
  }

  bool is_output_node(const bNode &node) const
  {
    if (node.typeinfo->nclass == NODE_CLASS_OUTPUT) {
      return true;
    }
    if (node.is_group_output()) {
      return true;
    }
    if (node.is_type("GeometryNodeWarning")) {
      return true;
    }
    if (nodes::gizmos::is_builtin_gizmo_node(node)) {
      return true;
    }
    /* Assume node groups without output sockets are outputs. */
    if (node.is_group()) {
      const bNodeTree *node_group = reinterpret_cast<const bNodeTree *>(node.id);
      if (node_group != nullptr &&
          node_group->runtime->runtime_flag & NTREE_RUNTIME_FLAG_HAS_MATERIAL_OUTPUT)
      {
        return true;
      }
    }
    return false;
  }

  /**
   * Computes a hash that changes when the node tree topology connected to an output node
   * changes. Adding reroutes does not have an effect on the hash.
   */
  uint32_t get_combined_socket_topology_hash(const bNodeTree &tree,
                                             Span<const bNodeSocket *> sockets)
  {
    if (tree.has_available_link_cycle()) {
      /* Return dummy value when the link has any cycles. The algorithm below could be improved
       * to handle cycles more gracefully. */
      return 0;
    }
    Array<uint32_t> hashes = this->get_socket_topology_hashes(tree, sockets);
    uint32_t combined_hash = 0;
    for (uint32_t hash : hashes) {
      combined_hash = noise::hash(combined_hash, hash);
    }
    return combined_hash;
  }

  Array<uint32_t> get_socket_topology_hashes(const bNodeTree &tree,
                                             const Span<const bNodeSocket *> sockets)
  {
    BLI_assert(!tree.has_available_link_cycle());
    Array<std::optional<uint32_t>> hash_by_socket_id(tree.all_sockets().size());
    Stack<const bNodeSocket *> sockets_to_check = sockets;

    auto get_socket_ptr_hash = [&](const bNodeSocket &socket) {
      const uint64_t socket_ptr = uintptr_t(&socket);
      return noise::hash(socket_ptr, socket_ptr >> 32);
    };
    const bNodeTreeZones *zones = tree.zones();

    while (!sockets_to_check.is_empty()) {
      const bNodeSocket &socket = *sockets_to_check.peek();
      const bNode &node = socket.owner_node();

      if (hash_by_socket_id[socket.index_in_tree()].has_value()) {
        sockets_to_check.pop();
        /* Socket is handled already. */
        continue;
      }

      uint32_t socket_hash = 0;
      if (socket.is_input()) {
        /* For input sockets, first compute the hashes of all linked sockets. */
        bool all_origins_computed = true;
        bool get_value_from_origin = false;
        Vector<const bNodeSocket *, 16> origin_sockets;
        for (const bNodeLink *link : socket.directly_linked_links()) {
          if (link->is_muted()) {
            continue;
          }
          if (!link->is_available()) {
            continue;
          }
          origin_sockets.append(link->fromsock);
        }
        if (zones) {
          if (const bNodeTreeZone *zone = zones->get_zone_by_socket(socket)) {
            if (zone->output_node_id == node.identifier) {
              if (const bNode *input_node = zone->input_node()) {
                origin_sockets.extend(input_node->input_sockets());
              }
            }
          }
        }
        for (const bNodeSocket *origin_socket : origin_sockets) {
          const std::optional<uint32_t> origin_hash =
              hash_by_socket_id[origin_socket->index_in_tree()];
          if (origin_hash.has_value()) {
            if (get_value_from_origin || socket.type != origin_socket->type) {
              socket_hash = noise::hash(socket_hash, *origin_hash);
            }
            else {
              /* Copy the socket hash because the link did not change it. */
              socket_hash = *origin_hash;
            }
            get_value_from_origin = true;
          }
          else {
            sockets_to_check.push(origin_socket);
            all_origins_computed = false;
          }
        }
        if (!all_origins_computed) {
          continue;
        }

        if (!get_value_from_origin) {
          socket_hash = get_socket_ptr_hash(socket);
        }
      }
      else {
        bool all_available_inputs_computed = true;
        for (const bNodeSocket *input_socket : node.input_sockets()) {
          if (input_socket->is_available()) {
            if (!hash_by_socket_id[input_socket->index_in_tree()].has_value()) {
              sockets_to_check.push(input_socket);
              all_available_inputs_computed = false;
            }
          }
        }
        if (!all_available_inputs_computed) {
          continue;
        }
        if (node.is_reroute()) {
          socket_hash = *hash_by_socket_id[node.input_socket(0).index_in_tree()];
        }
        else if (node.is_muted()) {
          const bNodeSocket *internal_input = socket.internal_link_input();
          if (internal_input == nullptr) {
            socket_hash = get_socket_ptr_hash(socket);
          }
          else {
            if (internal_input->type == socket.type) {
              socket_hash = *hash_by_socket_id[internal_input->index_in_tree()];
            }
            else {
              socket_hash = get_socket_ptr_hash(socket);
            }
          }
        }
        else {
          socket_hash = get_socket_ptr_hash(socket);
          for (const bNodeSocket *input_socket : node.input_sockets()) {
            if (input_socket->is_available()) {
              const uint32_t input_socket_hash = *hash_by_socket_id[input_socket->index_in_tree()];
              socket_hash = noise::hash(socket_hash, input_socket_hash);
            }
          }

          /* The Image Texture node has a special case. The behavior of the color output changes
           * depending on whether the Alpha output is linked. */
          if (node.is_type("ShaderNodeTexImage") && socket.index() == 0) {
            BLI_assert(STREQ(socket.name, "Color"));
            const bNodeSocket &alpha_socket = node.output_socket(1);
            BLI_assert(STREQ(alpha_socket.name, "Alpha"));
            if (alpha_socket.is_directly_linked()) {
              socket_hash = noise::hash(socket_hash);
            }
          }
        }
      }
      hash_by_socket_id[socket.index_in_tree()] = socket_hash;
      /* Check that nothing has been pushed in the meantime. */
      BLI_assert(sockets_to_check.peek() == &socket);
      sockets_to_check.pop();
    }

    /* Create output array. */
    Array<uint32_t> hashes(sockets.size());
    for (const int i : sockets.index_range()) {
      hashes[i] = *hash_by_socket_id[sockets[i]->index_in_tree()];
    }
    return hashes;
  }

  /**
   * Returns true when any of the provided sockets changed its values. A change is detected by
   * checking the #changed_flag on connected sockets and nodes.
   */
  bool check_if_socket_outputs_changed_based_on_flags(const bNodeTree &tree,
                                                      Span<const bNodeSocket *> sockets)
  {
    /* Avoid visiting the same socket twice when multiple links point to the same socket. */
    Array<bool> pushed_by_socket_id(tree.all_sockets().size(), false);
    Stack<const bNodeSocket *> sockets_to_check = sockets;

    for (const bNodeSocket *socket : sockets) {
      pushed_by_socket_id[socket->index_in_tree()] = true;
    }

    while (!sockets_to_check.is_empty()) {
      const bNodeSocket &socket = *sockets_to_check.pop();
      const bNode &node = socket.owner_node();
      if (socket.runtime->changed_flag != NTREE_CHANGED_NOTHING) {
        return true;
      }
      if (node.runtime->changed_flag != NTREE_CHANGED_NOTHING) {
        const bool only_unused_internal_link_changed = !node.is_muted() &&
                                                       node.runtime->changed_flag ==
                                                           NTREE_CHANGED_INTERNAL_LINK;
        const bool only_parent_changed = node.runtime->changed_flag == NTREE_CHANGED_PARENT;
        const bool change_affects_output = !(only_unused_internal_link_changed ||
                                             only_parent_changed);
        if (change_affects_output) {
          return true;
        }
      }
      if (socket.is_input()) {
        for (const bNodeSocket *origin_socket : socket.directly_linked_sockets()) {
          bool &pushed = pushed_by_socket_id[origin_socket->index_in_tree()];
          if (!pushed) {
            sockets_to_check.push(origin_socket);
            pushed = true;
          }
        }
      }
      else {
        for (const bNodeSocket *input_socket : node.input_sockets()) {
          if (input_socket->is_available()) {
            bool &pushed = pushed_by_socket_id[input_socket->index_in_tree()];
            if (!pushed) {
              sockets_to_check.push(input_socket);
              pushed = true;
            }
          }
        }
        /* Zones may propagate changes from the input node to the output node even though there is
         * no explicit link. */
        switch (node.type_legacy) {
          case GEO_NODE_REPEAT_OUTPUT:
          case GEO_NODE_SIMULATION_OUTPUT:
          case GEO_NODE_FOREACH_GEOMETRY_ELEMENT_OUTPUT: {
            const bNodeTreeZones *zones = tree.zones();
            if (!zones) {
              break;
            }
            const bNodeTreeZone *zone = zones->get_zone_by_node(node.identifier);
            if (!zone || !zone->input_node()) {
              break;
            }
            for (const bNodeSocket *input_socket : zone->input_node()->input_sockets()) {
              if (input_socket->is_available()) {
                bool &pushed = pushed_by_socket_id[input_socket->index_in_tree()];
                if (!pushed) {
                  sockets_to_check.push(input_socket);
                  pushed = true;
                }
              }
            }
            break;
          }
        }
        /* The Normal node has a special case, because the value stored in the first output
         * socket is used as input in the node. */
        if ((node.is_type("ShaderNodeNormal") || node.is_type("CompositorNodeNormal")) &&
            socket.index() == 1)
        {
          BLI_assert(STREQ(socket.name, "Dot"));
          const bNodeSocket &normal_output = node.output_socket(0);
          BLI_assert(STREQ(normal_output.name, "Normal"));
          bool &pushed = pushed_by_socket_id[normal_output.index_in_tree()];
          if (!pushed) {
            sockets_to_check.push(&normal_output);
            pushed = true;
          }
        }
      }
    }
    return false;
  }

  /**
   * Make sure that the #bNodeTree::nested_node_refs is up to date. It's supposed to contain a
   * reference to all (nested) simulation zones.
   */
  bool update_nested_node_refs(bNodeTree &ntree)
  {
    ntree.ensure_topology_cache();

    /* Simplify lookup of old ids. */
    Map<bNestedNodePath, int32_t> old_id_by_path;
    Set<int32_t> old_ids;
    for (const bNestedNodeRef &ref : ntree.nested_node_refs_span()) {
      old_id_by_path.add(ref.path, ref.id);
      old_ids.add(ref.id);
    }

    Vector<bNestedNodePath> nested_node_paths;

    /* Don't forget nested node refs just because the linked file is not available right now. */
    for (const bNestedNodePath &path : old_id_by_path.keys()) {
      const bNode *node = ntree.node_by_id(path.node_id);
      if (node && node->is_group() && node->id) {
        if (node->id->tag & ID_TAG_MISSING) {
          nested_node_paths.append(path);
        }
      }
    }
    if (ntree.type == NTREE_GEOMETRY) {
      /* Create references for simulations and bake nodes in geometry nodes.
       * Those are the nodes that we want to store settings for at a higher level. */
      for (StringRefNull idname : {"GeometryNodeSimulationOutput", "GeometryNodeBake"}) {
        for (const bNode *node : ntree.nodes_by_type(idname)) {
          nested_node_paths.append({node->identifier, -1});
        }
      }
    }
    /* Propagate references to nested nodes in group nodes. */
    for (const bNode *node : ntree.group_nodes()) {
      const bNodeTree *group = reinterpret_cast<const bNodeTree *>(node->id);
      if (group == nullptr) {
        continue;
      }
      for (const int i : group->nested_node_refs_span().index_range()) {
        const bNestedNodeRef &child_ref = group->nested_node_refs[i];
        nested_node_paths.append({node->identifier, child_ref.id});
      }
    }

    /* Used to generate new unique IDs if necessary. */
    RandomNumberGenerator rng = RandomNumberGenerator::from_random_seed();

    Map<int32_t, bNestedNodePath> new_path_by_id;
    for (const bNestedNodePath &path : nested_node_paths) {
      const int32_t old_id = old_id_by_path.lookup_default(path, -1);
      if (old_id != -1) {
        /* The same path existed before, it should keep the same ID as before. */
        new_path_by_id.add(old_id, path);
        continue;
      }
      int32_t new_id;
      while (true) {
        new_id = rng.get_int32(INT32_MAX);
        if (!old_ids.contains(new_id) && !new_path_by_id.contains(new_id)) {
          break;
        }
      }
      /* The path is new, it should get a new ID that does not collide with any existing IDs. */
      new_path_by_id.add(new_id, path);
    }

    /* Check if the old and new references are identical. */
    if (!this->nested_node_refs_changed(ntree, new_path_by_id)) {
      return false;
    }

    MEM_SAFE_FREE(ntree.nested_node_refs);
    if (new_path_by_id.is_empty()) {
      ntree.nested_node_refs_num = 0;
      return true;
    }

    /* Allocate new array for the nested node references contained in the node tree. */
    bNestedNodeRef *new_refs = MEM_malloc_arrayN<bNestedNodeRef>(size_t(new_path_by_id.size()),
                                                                 __func__);
    int index = 0;
    for (const auto item : new_path_by_id.items()) {
      bNestedNodeRef &ref = new_refs[index];
      ref.id = item.key;
      ref.path = item.value;
      index++;
    }

    ntree.nested_node_refs = new_refs;
    ntree.nested_node_refs_num = new_path_by_id.size();

    return true;
  }

  bool nested_node_refs_changed(const bNodeTree &ntree,
                                const Map<int32_t, bNestedNodePath> &new_path_by_id)
  {
    if (ntree.nested_node_refs_num != new_path_by_id.size()) {
      return true;
    }
    for (const bNestedNodeRef &ref : ntree.nested_node_refs_span()) {
      if (!new_path_by_id.contains(ref.id)) {
        return true;
      }
    }
    return false;
  }

  void reset_changed_flags(bNodeTree &ntree)
  {
    ntree.runtime->changed_flag = NTREE_CHANGED_NOTHING;
    for (bNode *node : ntree.all_nodes()) {
      node->runtime->changed_flag = NTREE_CHANGED_NOTHING;
      node->runtime->update = 0;
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->inputs) {
        socket->runtime->changed_flag = NTREE_CHANGED_NOTHING;
      }
      LISTBASE_FOREACH (bNodeSocket *, socket, &node->outputs) {
        socket->runtime->changed_flag = NTREE_CHANGED_NOTHING;
      }
    }

    ntree.tree_interface.reset_interface_changed();
  }

  /**
   * Update the panel toggle sockets to use the same name as the panel.
   */
  bool update_panel_toggle_names(bNodeTree &ntree)
  {
    bool changed = false;
    ntree.ensure_interface_cache();
    for (bNodeTreeInterfaceItem *item : ntree.interface_items()) {
      if (item->item_type != NODE_INTERFACE_PANEL) {
        continue;
      }
      bNodeTreeInterfacePanel *panel = reinterpret_cast<bNodeTreeInterfacePanel *>(item);
      if (bNodeTreeInterfaceSocket *toggle_socket = panel->header_toggle_socket()) {
        if (!STREQ(panel->name, toggle_socket->name)) {
          MEM_SAFE_FREE(toggle_socket->name);
          toggle_socket->name = BLI_strdup_null(panel->name);
          changed = true;
        }
      }
    }
    return changed;
  }
};

}  // namespace blender::bke

void BKE_ntree_update_tag_all(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_ANY);
}

void BKE_ntree_update_tag_node_property(bNodeTree *ntree, bNode *node)
{
  add_node_tag(ntree, node, NTREE_CHANGED_NODE_PROPERTY);
}

void BKE_ntree_update_tag_node_new(bNodeTree *ntree, bNode *node)
{
  add_node_tag(ntree, node, NTREE_CHANGED_NODE_PROPERTY);
}

void BKE_ntree_update_tag_node_type(bNodeTree *ntree, bNode *node)
{
  add_node_tag(ntree, node, NTREE_CHANGED_NODE_PROPERTY);
}

void BKE_ntree_update_tag_socket_property(bNodeTree *ntree, bNodeSocket *socket)
{
  add_socket_tag(ntree, socket, NTREE_CHANGED_SOCKET_PROPERTY);
}

void BKE_ntree_update_tag_socket_new(bNodeTree *ntree, bNodeSocket *socket)
{
  add_socket_tag(ntree, socket, NTREE_CHANGED_SOCKET_PROPERTY);
}

void BKE_ntree_update_tag_socket_removed(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_REMOVED_SOCKET);
}

void BKE_ntree_update_tag_socket_type(bNodeTree *ntree, bNodeSocket *socket)
{
  add_socket_tag(ntree, socket, NTREE_CHANGED_SOCKET_PROPERTY);
}

void BKE_ntree_update_tag_socket_availability(bNodeTree *ntree, bNodeSocket *socket)
{
  add_socket_tag(ntree, socket, NTREE_CHANGED_SOCKET_PROPERTY);
}

void BKE_ntree_update_tag_node_removed(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_REMOVED_NODE);
}

void BKE_ntree_update_tag_node_mute(bNodeTree *ntree, bNode *node)
{
  add_node_tag(ntree, node, NTREE_CHANGED_NODE_PROPERTY);
}

void BKE_ntree_update_tag_node_internal_link(bNodeTree *ntree, bNode *node)
{
  add_node_tag(ntree, node, NTREE_CHANGED_INTERNAL_LINK);
}

void BKE_ntree_update_tag_link_changed(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_LINK);
}

void BKE_ntree_update_tag_link_removed(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_LINK);
}

void BKE_ntree_update_tag_link_added(bNodeTree *ntree, bNodeLink * /*link*/)
{
  add_tree_tag(ntree, NTREE_CHANGED_LINK);
}

void BKE_ntree_update_tag_link_mute(bNodeTree *ntree, bNodeLink * /*link*/)
{
  add_tree_tag(ntree, NTREE_CHANGED_LINK);
}

void BKE_ntree_update_tag_active_output_changed(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_ANY);
}

void BKE_ntree_update_tag_missing_runtime_data(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_ALL);
}

void BKE_ntree_update_tag_parent_change(bNodeTree *ntree, bNode *node)
{
  add_node_tag(ntree, node, NTREE_CHANGED_PARENT);
}

void BKE_ntree_update_tag_id_changed(Main *bmain, ID *id)
{
  FOREACH_NODETREE_BEGIN (bmain, ntree, ntree_id) {
    for (bNode *node : ntree->all_nodes()) {
      if (node->id == id) {
        node->runtime->update |= NODE_UPDATE_ID;
        add_node_tag(ntree, node, NTREE_CHANGED_NODE_PROPERTY);
      }
    }
  }
  FOREACH_NODETREE_END;
}

void BKE_ntree_update_tag_image_user_changed(bNodeTree *ntree, ImageUser * /*iuser*/)
{
  /* Would have to search for the node that uses the image user for a more detailed tag. */
  add_tree_tag(ntree, NTREE_CHANGED_ANY);
}

uint64_t bNestedNodePath::hash() const
{
  return blender::get_default_hash(this->node_id, this->id_in_node);
}

bool operator==(const bNestedNodePath &a, const bNestedNodePath &b)
{
  return a.node_id == b.node_id && a.id_in_node == b.id_in_node;
}

/**
 * Protect from recursive calls into the updating function. Some node update functions might
 * trigger this from Python or in other cases.
 *
 * This could be added to #Main, but given that there is generally only one #Main, that's not
 * really worth it now.
 */
static bool is_updating = false;

void BKE_ntree_update(Main &bmain,
                      const std::optional<blender::Span<bNodeTree *>> modified_trees,
                      const NodeTreeUpdateExtraParams &params)
{
  if (is_updating) {
    return;
  }

  is_updating = true;
  blender::bke::NodeTreeMainUpdater updater{&bmain, params};
  if (modified_trees.has_value()) {
    updater.update_rooted(*modified_trees);
  }
  else {
    updater.update();
  }
  is_updating = false;
}

void BKE_ntree_update_after_single_tree_change(Main &bmain,
                                               bNodeTree &modified_tree,
                                               const NodeTreeUpdateExtraParams &params)
{
  BKE_ntree_update(bmain, blender::Span{&modified_tree}, params);
}

void BKE_ntree_update_without_main(bNodeTree &tree)
{
  BLI_assert(tree.id.tag & ID_TAG_NO_MAIN);
  if (is_updating) {
    return;
  }
  is_updating = true;
  NodeTreeUpdateExtraParams params;
  blender::bke::NodeTreeMainUpdater updater{nullptr, params};
  updater.update_rooted({&tree});
  is_updating = false;
}
