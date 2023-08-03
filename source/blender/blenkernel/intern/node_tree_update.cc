/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_map.hh"
#include "BLI_multi_value_map.hh"
#include "BLI_noise.hh"
#include "BLI_rand.hh"
#include "BLI_set.hh"
#include "BLI_stack.hh"
#include "BLI_timeit.hh"
#include "BLI_vector_set.hh"

#include "PIL_time.h"

#include "DNA_anim_types.h"
#include "DNA_modifier_types.h"
#include "DNA_node_types.h"

#include "BKE_anim_data.h"
#include "BKE_image.h"
#include "BKE_main.h"
#include "BKE_node.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_anonymous_attributes.hh"
#include "BKE_node_tree_update.h"

#include "MOD_nodes.hh"

#include "NOD_geometry_nodes_lazy_function.hh"
#include "NOD_node_declaration.hh"
#include "NOD_socket.hh"
#include "NOD_texture.h"

#include "DEG_depsgraph_query.h"

using namespace blender::nodes;

/**
 * These flags are used by the `changed_flag` field in #bNodeTree, #bNode and #bNodeSocket.
 * This enum is not part of the public api. It should be used through the `BKE_ntree_update_tag_*`
 * api.
 */
enum eNodeTreeChangedFlag {
  NTREE_CHANGED_NOTHING = 0,
  NTREE_CHANGED_ANY = (1 << 1),
  NTREE_CHANGED_NODE_PROPERTY = (1 << 2),
  NTREE_CHANGED_NODE_OUTPUT = (1 << 3),
  NTREE_CHANGED_INTERFACE = (1 << 4),
  NTREE_CHANGED_LINK = (1 << 5),
  NTREE_CHANGED_REMOVED_NODE = (1 << 6),
  NTREE_CHANGED_REMOVED_SOCKET = (1 << 7),
  NTREE_CHANGED_SOCKET_PROPERTY = (1 << 8),
  NTREE_CHANGED_INTERNAL_LINK = (1 << 9),
  NTREE_CHANGED_PARENT = (1 << 10),
  NTREE_CHANGED_ALL = -1,
};

static void add_tree_tag(bNodeTree *ntree, const eNodeTreeChangedFlag flag)
{
  ntree->runtime->changed_flag |= flag;
  ntree->runtime->topology_cache_mutex.tag_dirty();
  ntree->runtime->tree_zones_cache_mutex.tag_dirty();
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
      }
      return -1;
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
      }
      return -1;
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
      }
      return -1;
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
      }
      return -1;
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
      }
      return -1;
  }

  /* The rest of the socket types only allow an internal link if both the input and output socket
   * have the same type. If the sockets are custom, we check the idname instead. */
  if (to->type == from->type && (to->type != SOCK_CUSTOM || STREQ(to->idname, from->idname))) {
    return 1;
  }

  return -1;
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
  std::optional<Map<bNodeTree *, ID *>> owner_ids_;
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
    owner_ids_.emplace();
    if (bmain_ == nullptr) {
      return;
    }

    FOREACH_NODETREE_BEGIN (bmain_, ntree, id) {
      all_trees_->append(ntree);
      if (&ntree->id != id) {
        owner_ids_->add_new(ntree, id);
      }
    }
    FOREACH_NODETREE_END;
  }

  void ensure_owner_ids()
  {
    this->ensure_all_trees();
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

  ID *get_owner_id(bNodeTree *ntree)
  {
    BLI_assert(owner_ids_.has_value());
    return owner_ids_->lookup_default(ntree, &ntree->id);
  }
};

struct TreeUpdateResult {
  bool interface_changed = false;
  bool output_changed = false;
};

class NodeTreeMainUpdater {
 private:
  Main *bmain_;
  NodeTreeUpdateExtraParams *params_;
  Map<bNodeTree *, TreeUpdateResult> update_result_by_tree_;
  NodeTreeRelations relations_;

 public:
  NodeTreeMainUpdater(Main *bmain, NodeTreeUpdateExtraParams *params)
      : bmain_(bmain), params_(params), relations_(bmain)
  {
  }

  void update()
  {
    Vector<bNodeTree *> changed_ntrees;
    FOREACH_NODETREE_BEGIN (bmain_, ntree, id) {
      if (ntree->runtime->changed_flag != NTREE_CHANGED_NOTHING) {
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
      if (ntree->runtime->changed_flag == NTREE_CHANGED_NOTHING) {
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
        if (ntree->runtime->changed_flag == NTREE_CHANGED_NOTHING) {
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
        ntree->runtime->geometry_nodes_lazy_function_graph_info.reset();
      }

      if (params_) {
        relations_.ensure_owner_ids();
        ID *id = relations_.get_owner_id(ntree);
        if (params_->tree_changed_fn) {
          params_->tree_changed_fn(id, ntree, params_->user_data);
        }
        if (params_->tree_output_changed_fn && result.output_changed) {
          params_->tree_output_changed_fn(id, ntree, params_->user_data);
        }
      }
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

    this->update_socket_link_and_use(ntree);
    this->update_individual_nodes(ntree);
    this->update_internal_links(ntree);
    this->update_generic_callback(ntree);
    this->remove_unused_previews_when_necessary(ntree);

    this->propagate_runtime_flags(ntree);
    if (ntree.type == NTREE_GEOMETRY) {
      if (node_field_inferencing::update_field_inferencing(ntree)) {
        result.interface_changed = true;
      }
      if (anonymous_attribute_inferencing::update_anonymous_attribute_relations(ntree)) {
        result.interface_changed = true;
      }
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

    if (ntree.runtime->changed_flag & NTREE_CHANGED_INTERFACE ||
        ntree.runtime->changed_flag & NTREE_CHANGED_ANY)
    {
      result.interface_changed = true;
    }

#ifdef DEBUG
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
      blender::bke::nodeDeclarationEnsure(&ntree, node);
      if (this->should_update_individual_node(ntree, *node)) {
        bNodeType &ntype = *node->typeinfo;
        if (ntype.group_update_func) {
          ntype.group_update_func(&ntree, node);
        }
        if (ntype.updatefunc) {
          ntype.updatefunc(&ntree, node);
        }
        if (ntype.declare_dynamic) {
          nodes::update_node_declaration_and_sockets(ntree, *node);
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
    if (ntree.runtime->changed_flag & NTREE_CHANGED_INTERFACE) {
      if (ELEM(node.type, NODE_GROUP_INPUT, NODE_GROUP_OUTPUT)) {
        return true;
      }
    }
    /* Check paired simulation zone nodes. */
    if (node.type == GEO_NODE_SIMULATION_INPUT) {
      const NodeGeometrySimulationInput *data = static_cast<const NodeGeometrySimulationInput *>(
          node.storage);
      if (const bNode *output_node = ntree.node_by_id(data->output_node_id)) {
        if (output_node->runtime->changed_flag & NTREE_CHANGED_NODE_PROPERTY) {
          return true;
        }
      }
    }
    if (node.type == GEO_NODE_REPEAT_INPUT) {
      const NodeGeometryRepeatInput *data = static_cast<const NodeGeometryRepeatInput *>(
          node.storage);
      if (const bNode *output_node = ntree.node_by_id(data->output_node_id)) {
        if (output_node->runtime->changed_flag & NTREE_CHANGED_NODE_PROPERTY) {
          return true;
        }
      }
    }
    return false;
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
      Vector<std::pair<bNodeSocket *, bNodeSocket *>> expected_internal_links;
      for (const bNodeSocket *output_socket : node->output_sockets()) {
        if (!output_socket->is_available()) {
          continue;
        }
        if (!output_socket->is_directly_linked()) {
          continue;
        }
        if (output_socket->flag & SOCK_NO_INTERNAL_LINK) {
          continue;
        }
        const bNodeSocket *input_socket = this->find_internally_linked_input(output_socket);
        if (input_socket != nullptr) {
          expected_internal_links.append(
              {const_cast<bNodeSocket *>(input_socket), const_cast<bNodeSocket *>(output_socket)});
        }
      }
      /* Rebuilt internal links if they have changed. */
      if (node->runtime->internal_links.size() != expected_internal_links.size()) {
        this->update_internal_links_in_node(ntree, *node, expected_internal_links);
      }
      else {
        for (auto &item : expected_internal_links) {
          const bNodeSocket *from_socket = item.first;
          const bNodeSocket *to_socket = item.second;
          bool found = false;
          for (const bNodeLink &internal_link : node->runtime->internal_links) {
            if (from_socket == internal_link.fromsock && to_socket == internal_link.tosock) {
              found = true;
            }
          }
          if (!found) {
            this->update_internal_links_in_node(ntree, *node, expected_internal_links);
            break;
          }
        }
      }
    }
  }

  const bNodeSocket *find_internally_linked_input(const bNodeSocket *output_socket)
  {
    const bNodeSocket *selected_socket = nullptr;
    int selected_priority = -1;
    bool selected_is_linked = false;
    for (const bNodeSocket *input_socket : output_socket->owner_node().input_sockets()) {
      if (!input_socket->is_available()) {
        continue;
      }
      if (input_socket->flag & SOCK_NO_INTERNAL_LINK) {
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
                                     Span<std::pair<bNodeSocket *, bNodeSocket *>> links)
  {
    node.runtime->internal_links.clear();
    node.runtime->internal_links.reserve(links.size());
    for (const auto &item : links) {
      bNodeSocket *from_socket = item.first;
      bNodeSocket *to_socket = item.second;
      bNodeLink link{};
      link.fromnode = &node;
      link.fromsock = from_socket;
      link.tonode = &node;
      link.tosock = to_socket;
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
                                   NTREE_CHANGED_NODE_PROPERTY | NTREE_CHANGED_NODE_OUTPUT |
                                   NTREE_CHANGED_INTERFACE;
    if ((ntree.runtime->changed_flag & allowed_flags) == ntree.runtime->changed_flag) {
      return;
    }
    blender::bke::node_preview_remove_unused(&ntree);
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

  void update_link_validation(bNodeTree &ntree)
  {
    const Span<const bNode *> toposort = ntree.toposort_left_to_right();

    /* Build an array of toposort indices to allow retrieving the "depth" for each node. */
    Array<int> toposort_indices(toposort.size());
    for (const int i : toposort.index_range()) {
      const bNode &node = *toposort[i];
      toposort_indices[node.index()] = i;
    }

    LISTBASE_FOREACH (bNodeLink *, link, &ntree.links) {
      link->flag |= NODE_LINK_VALID;
      if (!link->fromsock->is_available() || !link->tosock->is_available()) {
        link->flag &= ~NODE_LINK_VALID;
        continue;
      }
      const bNode &from_node = *link->fromnode;
      const bNode &to_node = *link->tonode;
      if (toposort_indices[from_node.index()] > toposort_indices[to_node.index()]) {
        link->flag &= ~NODE_LINK_VALID;
        continue;
      }
      if (ntree.typeinfo->validate_link) {
        const eNodeSocketDatatype from_type = eNodeSocketDatatype(link->fromsock->type);
        const eNodeSocketDatatype to_type = eNodeSocketDatatype(link->tosock->type);
        if (!ntree.typeinfo->validate_link(from_type, to_type)) {
          link->flag &= ~NODE_LINK_VALID;
          continue;
        }
      }
    }
  }

  bool check_if_output_changed(const bNodeTree &tree)
  {
    tree.ensure_topology_cache();

    /* Compute a hash that represents the node topology connected to the output. This always has to
     * be updated even if it is not used to detect changes right now. Otherwise
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
       * copy-on-write tag for the node tree (which happens when changing node properties). It does
       * work in a few situations like adding reroutes and duplicating nodes though. */
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

    /* The topology hash can only be used when only topology-changing operations have been done. */
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
    if (node.type == NODE_GROUP_OUTPUT) {
      return true;
    }
    /* Assume node groups without output sockets are outputs. */
    if (node.type == NODE_GROUP) {
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
   * Computes a hash that changes when the node tree topology connected to an output node changes.
   * Adding reroutes does not have an effect on the hash.
   */
  uint32_t get_combined_socket_topology_hash(const bNodeTree &tree,
                                             Span<const bNodeSocket *> sockets)
  {
    if (tree.has_available_link_cycle()) {
      /* Return dummy value when the link has any cycles. The algorithm below could be improved to
       * handle cycles more gracefully. */
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
        for (const bNodeLink *link : socket.directly_linked_links()) {
          if (link->is_muted()) {
            continue;
          }
          if (!link->is_available()) {
            continue;
          }
          const bNodeSocket &origin_socket = *link->fromsock;
          const std::optional<uint32_t> origin_hash =
              hash_by_socket_id[origin_socket.index_in_tree()];
          if (origin_hash.has_value()) {
            if (get_value_from_origin || socket.type != origin_socket.type) {
              socket_hash = noise::hash(socket_hash, *origin_hash);
            }
            else {
              /* Copy the socket hash because the link did not change it. */
              socket_hash = *origin_hash;
            }
            get_value_from_origin = true;
          }
          else {
            sockets_to_check.push(&origin_socket);
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
        if (node.type == NODE_REROUTE) {
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
          if (node.type == SH_NODE_TEX_IMAGE && socket.index() == 0) {
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
        if (!only_unused_internal_link_changed) {
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
        /* The Normal node has a special case, because the value stored in the first output socket
         * is used as input in the node. */
        if (node.type == SH_NODE_NORMAL && socket.index() == 1) {
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
        if (node->id->tag & LIB_TAG_MISSING) {
          nested_node_paths.append(path);
        }
      }
    }
    if (ntree.type == NTREE_GEOMETRY) {
      /* Create references for simulations in geometry nodes. */
      for (const bNode *node : ntree.nodes_by_type("GeometryNodeSimulationOutput")) {
        nested_node_paths.append({node->identifier, -1});
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
    RandomNumberGenerator rng(PIL_check_seconds_timer_i() & UINT_MAX);

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
    bNestedNodeRef *new_refs = static_cast<bNestedNodeRef *>(
        MEM_malloc_arrayN(new_path_by_id.size(), sizeof(bNestedNodeRef), __func__));
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

void BKE_ntree_update_tag_node_reordered(bNodeTree *ntree)
{
  /* Don't add a tree update tag to avoid reevaluations for trivial operations like selection or
   * parenting that typically influence the node order. This means the node order can be different
   * for original and evaluated trees. A different solution might avoid sorting nodes based on UI
   * states like selection, which would require not tying the node order to the drawing order. */
  ntree->runtime->topology_cache_mutex.tag_dirty();
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

void BKE_ntree_update_tag_interface(bNodeTree *ntree)
{
  add_tree_tag(ntree, NTREE_CHANGED_INTERFACE);
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
  return blender::get_default_hash_2(this->node_id, this->id_in_node);
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

void BKE_ntree_update_main(Main *bmain, NodeTreeUpdateExtraParams *params)
{
  if (is_updating) {
    return;
  }

  is_updating = true;
  blender::bke::NodeTreeMainUpdater updater{bmain, params};
  updater.update();
  is_updating = false;
}

void BKE_ntree_update_main_tree(Main *bmain, bNodeTree *ntree, NodeTreeUpdateExtraParams *params)
{
  if (ntree == nullptr) {
    BKE_ntree_update_main(bmain, params);
    return;
  }

  if (is_updating) {
    return;
  }

  is_updating = true;
  blender::bke::NodeTreeMainUpdater updater{bmain, params};
  updater.update_rooted({ntree});
  is_updating = false;
}
