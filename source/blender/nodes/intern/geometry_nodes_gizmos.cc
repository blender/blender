/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"

#include "BKE_compute_context_cache.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_context.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_zones.hh"
#include "BKE_object.hh"
#include "BKE_workspace.hh"

#include "NOD_geometry_nodes_gizmos.hh"
#include "NOD_inverse_eval_path.hh"
#include "NOD_partial_eval.hh"
#include "NOD_socket_usage_inference.hh"

#include "DNA_modifier_types.h"
#include "DNA_space_types.h"
#include "DNA_windowmanager_types.h"

#include "ED_node.hh"

namespace blender::nodes::gizmos {

bool is_builtin_gizmo_node(const bNode &node)
{
  return ELEM(
      node.type_legacy, GEO_NODE_GIZMO_LINEAR, GEO_NODE_GIZMO_DIAL, GEO_NODE_GIZMO_TRANSFORM);
}

/**
 * Get the part of a socket value that may be edited with gizmos.
 */
static ie::ElemVariant get_gizmo_socket_elem(const bNode &node, const bNodeSocket &socket)
{
  switch (node.type_legacy) {
    case GEO_NODE_GIZMO_LINEAR: {
      return {ie::FloatElem::all()};
    }
    case GEO_NODE_GIZMO_DIAL: {
      return {ie::FloatElem::all()};
    }
    case GEO_NODE_GIZMO_TRANSFORM: {
      const auto &storage = *static_cast<const NodeGeometryTransformGizmo *>(node.storage);
      ie::MatrixElem elem;
      if (storage.flag & GEO_NODE_TRANSFORM_GIZMO_USE_TRANSLATION_ALL) {
        elem.translation = ie::VectorElem::all();
      }
      if (storage.flag &
          (GEO_NODE_TRANSFORM_GIZMO_USE_ROTATION_ALL | GEO_NODE_TRANSFORM_GIZMO_USE_SCALE_ALL))
      {
        elem.rotation = ie::RotationElem::all();
        elem.scale = ie::VectorElem::all();
      }
      return {elem};
    }
  }
  const eNodeSocketDatatype socket_type = eNodeSocketDatatype(socket.type);
  if (std::optional<ie::ElemVariant> elem = ie::get_elem_variant_for_socket_type(socket_type)) {
    elem->set_all();
    return *elem;
  }
  BLI_assert_unreachable();
  return {};
}

static TreeGizmoPropagation build_tree_gizmo_propagation(bNodeTree &tree)
{
  BLI_assert(!tree.has_available_link_cycle());

  TreeGizmoPropagation gizmo_propagation;

  struct GizmoInput {
    const bNodeSocket *gizmo_socket;
    /* For multi-input sockets we start propagation at the origin socket. */
    const bNodeSocket *propagation_start_socket;
    ie::ElemVariant elem;
  };

  /* Gather all gizmo inputs so that we can find their inverse evaluation targets afterwards. */
  Vector<GizmoInput> all_gizmo_inputs;
  for (const bNode *node : tree.all_nodes()) {
    if (node->is_muted()) {
      continue;
    }
    if (node->is_group()) {
      if (!node->id) {
        continue;
      }
      const bNodeTree &group = *reinterpret_cast<const bNodeTree *>(node->id);
      if (!group.runtime->gizmo_propagation) {
        continue;
      }
      const TreeGizmoPropagation &group_gizmo_propagation = *group.runtime->gizmo_propagation;
      for (const ie::GroupInputElem &group_input_elem :
           group_gizmo_propagation.gizmo_inputs_by_group_inputs.keys())
      {
        const bNodeSocket &input_socket = node->input_socket(group_input_elem.group_input_index);
        all_gizmo_inputs.append({&input_socket, &input_socket, group_input_elem.elem});
      }
    }
    if (is_builtin_gizmo_node(*node)) {
      gizmo_propagation.gizmo_nodes.append(node);
      const bNodeSocket &gizmo_input_socket = node->input_socket(0);
      gizmo_propagation.gizmo_endpoint_sockets.add(&gizmo_input_socket);
      const ie::ElemVariant elem = get_gizmo_socket_elem(*node, gizmo_input_socket);
      for (const bNodeLink *link : gizmo_input_socket.directly_linked_links()) {
        if (!link->is_used()) {
          continue;
        }
        all_gizmo_inputs.append({&gizmo_input_socket, link->fromsock, elem});
      }
    }
  }

  /* Find the local gizmo targets for all gizmo inputs. */
  for (const GizmoInput &gizmo_input : all_gizmo_inputs) {
    gizmo_propagation.gizmo_endpoint_sockets.add(gizmo_input.gizmo_socket);
    const ie::SocketElem gizmo_input_socket_elem{gizmo_input.gizmo_socket, gizmo_input.elem};
    /* The conversion is necessary when e.g. connecting a Rotation directly to the matrix input of
     * the Transform Gizmo node. */
    const std::optional<ie::ElemVariant> converted_elem = ie::convert_socket_elem(
        *gizmo_input.gizmo_socket, *gizmo_input.propagation_start_socket, gizmo_input.elem);
    if (!converted_elem) {
      continue;
    }
    const ie::LocalInverseEvalTargets targets = ie::find_local_inverse_eval_targets(
        tree, {gizmo_input.propagation_start_socket, *converted_elem});
    const bool has_target = !targets.input_sockets.is_empty() ||
                            !targets.group_inputs.is_empty() || !targets.value_nodes.is_empty();
    if (!has_target) {
      continue;
    }
    /* Remember all the gizmo targets for quick lookup later on. */
    for (const ie::SocketElem &input_socket : targets.input_sockets) {
      gizmo_propagation.gizmo_inputs_by_node_inputs.add(input_socket, gizmo_input_socket_elem);
      gizmo_propagation.gizmo_endpoint_sockets.add(input_socket.socket);
    }
    for (const ie::ValueNodeElem &value_node : targets.value_nodes) {
      gizmo_propagation.gizmo_inputs_by_value_nodes.add(value_node, gizmo_input_socket_elem);
      gizmo_propagation.gizmo_endpoint_sockets.add(&value_node.node->output_socket(0));
    }
    for (const ie::GroupInputElem &group_input : targets.group_inputs) {
      gizmo_propagation.gizmo_inputs_by_group_inputs.add(group_input, gizmo_input_socket_elem);
      for (const bNode *group_input_node : tree.group_input_nodes()) {
        gizmo_propagation.gizmo_endpoint_sockets.add(
            &group_input_node->output_socket(group_input.group_input_index));
      }
    }
  }

  return gizmo_propagation;
}

bool update_tree_gizmo_propagation(bNodeTree &tree)
{
  tree.ensure_topology_cache();

  if (tree.has_available_link_cycle()) {
    const bool changed = tree.runtime->gizmo_propagation != nullptr;
    tree.runtime->gizmo_propagation.reset();
    return changed;
  }

  TreeGizmoPropagation new_gizmo_propagation = build_tree_gizmo_propagation(tree);
  const bool changed = tree.runtime->gizmo_propagation ?
                           *tree.runtime->gizmo_propagation != new_gizmo_propagation :
                           true;
  tree.runtime->gizmo_propagation = std::make_unique<TreeGizmoPropagation>(
      std::move(new_gizmo_propagation));
  return changed;
}

static void foreach_gizmo_for_input(const ie::SocketElem &input_socket,
                                    bke::ComputeContextCache &compute_context_cache,
                                    const ComputeContext *compute_context,
                                    const bNodeTree &tree,
                                    const ForeachGizmoInModifierFn fn);

static void foreach_gizmo_for_group_input(const bNodeTree &tree,
                                          const ie::GroupInputElem &group_input,
                                          bke::ComputeContextCache &compute_context_cache,
                                          const ComputeContext *compute_context,
                                          const ForeachGizmoInModifierFn fn)
{
  const TreeGizmoPropagation &gizmo_propagation = *tree.runtime->gizmo_propagation;
  for (const ie::SocketElem &gizmo_input :
       gizmo_propagation.gizmo_inputs_by_group_inputs.lookup(group_input))
  {
    foreach_gizmo_for_input(gizmo_input, compute_context_cache, compute_context, tree, fn);
  }
}

static void foreach_gizmo_for_input(const ie::SocketElem &input_socket,
                                    bke::ComputeContextCache &compute_context_cache,
                                    const ComputeContext *compute_context,
                                    const bNodeTree &tree,
                                    const ForeachGizmoInModifierFn fn)
{
  const bke::bNodeTreeZones *zones = tree.zones();
  if (!zones) {
    /* There are invalid zones. */
    return;
  }
  const bNode &node = input_socket.socket->owner_node();
  if (zones->get_zone_by_node(node.identifier) != nullptr) {
    /* Gizmos in zones are not supported yet. */
    return;
  }
  if (is_builtin_gizmo_node(node)) {
    if (node.is_muted()) {
      return;
    }
    /* Found an actual built-in gizmo node. */
    fn(*compute_context, node, *input_socket.socket);
    return;
  }
  if (node.is_group()) {
    const bNodeTree &group = *reinterpret_cast<const bNodeTree *>(node.id);
    group.ensure_topology_cache();
    const ComputeContext &group_compute_context = compute_context_cache.for_group_node(
        compute_context, node.identifier, &tree);
    foreach_gizmo_for_group_input(
        group,
        ie::GroupInputElem{input_socket.socket->index(), input_socket.elem},
        compute_context_cache,
        &group_compute_context,
        fn);
  }
}

static void foreach_active_gizmo_in_open_node_editor(
    const SpaceNode &snode,
    const Object *object_filter,
    const NodesModifierData *nmd_filter,
    bke::ComputeContextCache &compute_context_cache,
    const ForeachGizmoFn fn)
{
  if (snode.nodetree == nullptr) {
    return;
  }
  if (snode.edittree == nullptr || !snode.edittree->runtime->gizmo_propagation) {
    return;
  }
  const std::optional<ed::space_node::ObjectAndModifier> object_and_modifier =
      ed::space_node::get_modifier_for_node_editor(snode);
  if (!object_and_modifier) {
    return;
  }
  if (object_filter) {
    if (object_and_modifier->object != object_filter) {
      return;
    }
  }
  if (nmd_filter) {
    if (object_and_modifier->nmd != nmd_filter) {
      return;
    }
  }

  const Object &object = *object_and_modifier->object;
  const NodesModifierData &nmd = *object_and_modifier->nmd;

  if (!(nmd.modifier.mode & eModifierMode_Realtime)) {
    /* Disabled modifiers can't have gizmos currently. */
    return;
  }

  const ComputeContext *current_compute_context = ed::space_node::compute_context_for_edittree(
      snode, compute_context_cache);
  if (!current_compute_context) {
    return;
  }

  snode.edittree->ensure_topology_cache();
  const TreeGizmoPropagation &gizmo_propagation = *snode.edittree->runtime->gizmo_propagation;
  Set<ie::SocketElem> used_gizmo_inputs;

  /* Check gizmos on value nodes. */
  for (auto &&item : gizmo_propagation.gizmo_inputs_by_value_nodes.items()) {
    const bNode &node = *item.key.node;
    const bNodeSocket &output_socket = node.output_socket(0);
    if ((node.flag & NODE_SELECT) || (output_socket.flag & SOCK_GIZMO_PIN)) {
      used_gizmo_inputs.add_multiple(item.value);
      continue;
    }
    for (const ie::SocketElem &socket_elem : item.value) {
      if (socket_elem.socket->owner_node().flag & NODE_SELECT) {
        used_gizmo_inputs.add(socket_elem);
      }
    }
  }
  /* Check gizmos on input sockets. */
  for (auto &&item : gizmo_propagation.gizmo_inputs_by_node_inputs.items()) {
    const bNodeSocket &socket = *item.key.socket;
    if (socket.is_inactive()) {
      continue;
    }
    const bNode &node = socket.owner_node();
    if ((node.flag & NODE_SELECT) || (socket.flag & SOCK_GIZMO_PIN)) {
      used_gizmo_inputs.add_multiple(item.value);
      continue;
    }
    for (const ie::SocketElem &socket_elem : item.value) {
      if (socket_elem.socket->owner_node().flag & NODE_SELECT) {
        used_gizmo_inputs.add(socket_elem);
      }
    }
  }
  /* Check built-in gizmo nodes. */
  for (const bNode *gizmo_node : gizmo_propagation.gizmo_nodes) {
    if (gizmo_node->is_muted()) {
      continue;
    }
    const bNodeSocket &gizmo_input_socket = gizmo_node->input_socket(0);
    if ((gizmo_node->flag & NODE_SELECT) || (gizmo_input_socket.flag & SOCK_GIZMO_PIN)) {
      used_gizmo_inputs.add(
          {&gizmo_input_socket,
           *ie::get_elem_variant_for_socket_type(eNodeSocketDatatype(gizmo_input_socket.type))});
    }
  }
  for (const ie::SocketElem &gizmo_input : used_gizmo_inputs) {
    foreach_gizmo_for_input(gizmo_input,
                            compute_context_cache,
                            current_compute_context,
                            *snode.edittree,
                            [&](const ComputeContext &compute_context,
                                const bNode &gizmo_node,
                                const bNodeSocket &gizmo_socket) {
                              fn(object, nmd, compute_context, gizmo_node, gizmo_socket);
                            });
  }
}

static void foreach_active_gizmo_in_open_editors(const wmWindowManager &wm,
                                                 const Object *object_filter,
                                                 const NodesModifierData *nmd_filter,
                                                 bke::ComputeContextCache &compute_context_cache,
                                                 const ForeachGizmoFn fn)
{
  LISTBASE_FOREACH (const wmWindow *, window, &wm.windows) {
    const bScreen *active_screen = BKE_workspace_active_screen_get(window->workspace_hook);
    Vector<const bScreen *> screens = {active_screen};
    if (ELEM(active_screen->state, SCREENMAXIMIZED, SCREENFULL)) {
      const ScrArea *area = static_cast<const ScrArea *>(active_screen->areabase.first);
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
        foreach_active_gizmo_in_open_node_editor(
            snode, object_filter, nmd_filter, compute_context_cache, fn);
      }
    }
  }
}

static void foreach_active_gizmo_exposed_to_modifier(
    const NodesModifierData &nmd,
    bke::ComputeContextCache &compute_context_cache,
    const ForeachGizmoInModifierFn fn)
{
  if (!nmd.node_group) {
    return;
  }
  const bNodeTree &tree = *nmd.node_group;
  if (!tree.runtime->gizmo_propagation) {
    return;
  }

  tree.ensure_interface_cache();

  ResourceScope scope;
  const Vector<InferenceValue> input_values = get_geometry_nodes_input_inference_values(
      *nmd.node_group, nmd.settings.properties, scope);

  const auto get_input_value = [&](const int group_input_i) {
    return input_values[group_input_i];
  };
  SocketValueInferencer value_inferencer{
      *nmd.node_group, scope, compute_context_cache, get_input_value};
  socket_usage_inference::SocketUsageInferencer usage_inferencer(
      *nmd.node_group, scope, value_inferencer, compute_context_cache);

  const ComputeContext &root_compute_context = compute_context_cache.for_modifier(nullptr, nmd);
  for (auto &&item : tree.runtime->gizmo_propagation->gizmo_inputs_by_group_inputs.items()) {
    const ie::GroupInputElem &group_input_elem = item.key;
    if (item.value.is_empty()) {
      continue;
    }
    if (!usage_inferencer.is_group_input_used(group_input_elem.group_input_index)) {
      continue;
    }
    for (const ie::SocketElem &socket_elem : item.value) {
      foreach_gizmo_for_input(socket_elem, compute_context_cache, &root_compute_context, tree, fn);
    }
  }
}

void foreach_active_gizmo_in_modifier(const Object &object,
                                      const NodesModifierData &nmd,
                                      const wmWindowManager &wm,
                                      bke::ComputeContextCache &compute_context_cache,
                                      const ForeachGizmoInModifierFn fn)
{
  if (!nmd.node_group) {
    return;
  }

  foreach_active_gizmo_in_open_editors(wm,
                                       &object,
                                       &nmd,
                                       compute_context_cache,
                                       [&](const Object &object_with_gizmo,
                                           const NodesModifierData &nmd_with_gizmo,
                                           const ComputeContext &compute_context,
                                           const bNode &gizmo_node,
                                           const bNodeSocket &gizmo_socket) {
                                         BLI_assert(&object == &object_with_gizmo);
                                         BLI_assert(&nmd == &nmd_with_gizmo);
                                         UNUSED_VARS_NDEBUG(object_with_gizmo, nmd_with_gizmo);
                                         fn(compute_context, gizmo_node, gizmo_socket);
                                       });

  foreach_active_gizmo_exposed_to_modifier(nmd, compute_context_cache, fn);
}

void foreach_active_gizmo(const bContext &C,
                          bke::ComputeContextCache &compute_context_cache,
                          const ForeachGizmoFn fn)
{
  const wmWindowManager *wm = CTX_wm_manager(&C);
  if (!wm) {
    return;
  }
  foreach_active_gizmo_in_open_editors(*wm, nullptr, nullptr, compute_context_cache, fn);

  if (const Base *active_base = CTX_data_active_base(&C)) {
    if (!(active_base->flag & BASE_SELECTED)) {
      return;
    }
    Object *active_object = active_base->object;

    if (const ModifierData *md = BKE_object_active_modifier(active_object)) {
      if (!(md->mode & eModifierMode_Realtime)) {
        return;
      }
      if (md->type == eModifierType_Nodes) {
        const NodesModifierData &nmd = *reinterpret_cast<const NodesModifierData *>(md);
        foreach_active_gizmo_exposed_to_modifier(
            nmd,
            compute_context_cache,
            [&](const ComputeContext &compute_context,
                const bNode &gizmo_node,
                const bNodeSocket &gizmo_socket) {
              fn(*active_object, nmd, compute_context, gizmo_node, gizmo_socket);
            });
      }
    }
  }
}

void foreach_compute_context_on_gizmo_path(const ComputeContext &gizmo_context,
                                           const bNode &gizmo_node,
                                           const bNodeSocket &gizmo_socket,
                                           FunctionRef<void(const ComputeContext &context)> fn)
{
  ie::foreach_element_on_inverse_eval_path(
      gizmo_context, {&gizmo_socket, get_gizmo_socket_elem(gizmo_node, gizmo_socket)}, fn, {});
}

void foreach_socket_on_gizmo_path(
    const ComputeContext &gizmo_context,
    const bNode &gizmo_node,
    const bNodeSocket &gizmo_socket,
    FunctionRef<void(
        const ComputeContext &context, const bNodeSocket &socket, const ie::ElemVariant &elem)> fn)
{
  ie::foreach_element_on_inverse_eval_path(
      gizmo_context, {&gizmo_socket, get_gizmo_socket_elem(gizmo_node, gizmo_socket)}, {}, fn);
}

ie::ElemVariant get_editable_gizmo_elem(const ComputeContext &gizmo_context,
                                        const bNode &gizmo_node,
                                        const bNodeSocket &gizmo_socket)
{
  std::optional<ie::ElemVariant> found_elem = ie::get_elem_variant_for_socket_type(
      eNodeSocketDatatype(gizmo_socket.type));
  BLI_assert(found_elem.has_value());

  ie::foreach_element_on_inverse_eval_path(
      gizmo_context,
      {&gizmo_socket, get_gizmo_socket_elem(gizmo_node, gizmo_socket)},
      {},
      [&](const ComputeContext &context, const bNodeSocket &socket, const ie::ElemVariant &elem) {
        if (context.hash() == gizmo_context.hash() && &socket == &gizmo_socket) {
          found_elem->merge(elem);
        }
      });

  return *found_elem;
}

void apply_gizmo_change(
    bContext &C,
    Object &object,
    NodesModifierData &nmd,
    geo_eval_log::GeoNodesLog &eval_log,
    const ComputeContext &gizmo_context,
    const bNodeSocket &gizmo_socket,
    const FunctionRef<void(bke::SocketValueVariant &value)> apply_on_gizmo_value_fn)
{
  Vector<ie::SocketToUpdate> sockets_to_update;

  const bNodeTree &gizmo_node_tree = gizmo_socket.owner_tree();
  geo_eval_log::GeoTreeLog &gizmo_tree_log = eval_log.get_tree_log(gizmo_context.hash());

  /* Gather all sockets to update together with their new values. */
  for (const bNodeLink *link : gizmo_socket.directly_linked_links()) {
    gizmo_node_tree.ensure_topology_cache();
    if (!link->is_used()) {
      continue;
    }
    if (link->fromnode->is_dangling_reroute()) {
      continue;
    }
    const std::optional<bke::SocketValueVariant> old_value = ie::get_logged_socket_value(
        gizmo_tree_log, *link->fromsock);
    if (!old_value) {
      continue;
    }
    const std::optional<bke::SocketValueVariant> old_value_converted =
        ie::convert_single_socket_value(*link->fromsock, *link->tosock, *old_value);
    if (!old_value_converted) {
      continue;
    }
    bke::SocketValueVariant new_value = *old_value_converted;
    apply_on_gizmo_value_fn(new_value);

    sockets_to_update.append({&gizmo_context, &gizmo_socket, link, new_value});
  }

  /* Actually backpropagate the socket values. */
  ie::backpropagate_socket_values(C, object, nmd, eval_log, sockets_to_update);
}

bool value_node_has_gizmo(const bNodeTree &tree, const bNode &node)
{
  BLI_assert(partial_eval::is_supported_value_node(node));
  if (!tree.runtime->gizmo_propagation) {
    return false;
  }
  return tree.runtime->gizmo_propagation->gizmo_endpoint_sockets.contains(&node.output_socket(0));
}

}  // namespace blender::nodes::gizmos
