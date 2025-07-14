/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "NOD_inverse_eval_params.hh"
#include "NOD_inverse_eval_path.hh"
#include "NOD_inverse_eval_run.hh"
#include "NOD_node_in_compute_context.hh"
#include "NOD_partial_eval.hh"
#include "NOD_value_elem_eval.hh"

#include "BKE_compute_contexts.hh"
#include "BKE_context.hh"
#include "BKE_library.hh"
#include "BKE_modifier.hh"
#include "BKE_node.hh"
#include "BKE_node_legacy_types.hh"
#include "BKE_node_runtime.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_type_conversions.hh"

#include "BLI_map.hh"
#include "BLI_math_euler.hh"
#include "BLI_set.hh"
#include "BLI_string.h"

#include "DEG_depsgraph.hh"

#include "ED_node.hh"

#include "RNA_access.hh"
#include "RNA_path.hh"

#include "MOD_nodes.hh"

#include "ANIM_keyframing.hh"

namespace blender::nodes::inverse_eval {

using namespace value_elem;

std::optional<SocketValueVariant> convert_single_socket_value(const bNodeSocket &old_socket,
                                                              const bNodeSocket &new_socket,
                                                              const SocketValueVariant &old_value)
{
  const eNodeSocketDatatype old_type = eNodeSocketDatatype(old_socket.type);
  const eNodeSocketDatatype new_type = eNodeSocketDatatype(new_socket.type);
  if (old_type == new_type) {
    return old_value;
  }
  const CPPType *old_cpp_type = old_socket.typeinfo->base_cpp_type;
  const CPPType *new_cpp_type = new_socket.typeinfo->base_cpp_type;
  if (!old_cpp_type || !new_cpp_type) {
    return std::nullopt;
  }
  const bke::DataTypeConversions &type_conversions = bke::get_implicit_type_conversions();
  if (type_conversions.is_convertible(*old_cpp_type, *new_cpp_type)) {
    const void *old_value_ptr = old_value.get_single_ptr_raw();
    SocketValueVariant new_value;
    void *new_value_ptr = new_value.allocate_single(new_type);
    type_conversions.convert_to_uninitialized(
        *old_cpp_type, *new_cpp_type, old_value_ptr, new_value_ptr);
    return new_value;
  }
  return std::nullopt;
}

static void evaluate_node_elem_upstream(const NodeInContext &ctx_node,
                                        Vector<const bNodeSocket *> &r_modified_inputs,
                                        Map<SocketInContext, ElemVariant> &elem_by_socket)
{
  const bNode &node = *ctx_node.node;
  const bke::bNodeType &ntype = *node.typeinfo;
  if (!ntype.eval_inverse_elem) {
    /* Node does not support inverse evaluation. */
    return;
  }
  /* Build temporary map to be used by node evaluation function. */
  Map<const bNodeSocket *, ElemVariant> elem_by_local_socket;
  for (const bNodeSocket *output_socket : node.output_sockets()) {
    if (const ElemVariant *elem = elem_by_socket.lookup_ptr({ctx_node.context, output_socket})) {
      elem_by_local_socket.add(output_socket, *elem);
    }
  }
  Vector<SocketElem> input_elems;
  InverseElemEvalParams params{node, elem_by_local_socket, input_elems};
  ntype.eval_inverse_elem(params);
  /* Write back changed socket values to the map. */
  for (const SocketElem &input_elem : input_elems) {
    if (input_elem.elem) {
      elem_by_socket.add({ctx_node.context, input_elem.socket}, input_elem.elem);
      r_modified_inputs.append(input_elem.socket);
    }
  }
}

static bool propagate_socket_elem(const SocketInContext &ctx_from,
                                  const SocketInContext &ctx_to,
                                  Map<SocketInContext, ElemVariant> &elem_by_socket)
{
  const ElemVariant *from_elem = elem_by_socket.lookup_ptr(ctx_from);
  if (!from_elem) {
    return false;
  }
  /* Perform implicit conversion if necessary. */
  const std::optional<ElemVariant> to_elem = convert_socket_elem(
      *ctx_from.socket, *ctx_to.socket, *from_elem);
  if (!to_elem || !*to_elem) {
    return false;
  }
  elem_by_socket.lookup_or_add(ctx_to, *to_elem).merge(*to_elem);
  return true;
}

static void get_input_elems_to_propagate(const NodeInContext &ctx_node,
                                         Vector<const bNodeSocket *> &r_sockets,
                                         Map<SocketInContext, ElemVariant> &elem_by_socket)
{
  for (const bNodeSocket *socket : ctx_node.node->input_sockets()) {
    if (elem_by_socket.contains({ctx_node.context, socket})) {
      r_sockets.append(socket);
    }
  }
}

LocalInverseEvalTargets find_local_inverse_eval_targets(const bNodeTree &tree,
                                                        const SocketElem &initial_socket_elem)
{
  BLI_assert(!tree.has_available_link_cycle());

  tree.ensure_topology_cache();

  bke::ComputeContextCache compute_context_cache;
  Map<SocketInContext, ElemVariant> elem_by_socket;
  elem_by_socket.add({nullptr, initial_socket_elem.socket}, initial_socket_elem.elem);

  const partial_eval::UpstreamEvalTargets upstream_eval_targets = partial_eval::eval_upstream(
      {{nullptr, initial_socket_elem.socket}},
      compute_context_cache,
      /* Evaluate node. */
      [&](const NodeInContext &ctx_node, Vector<const bNodeSocket *> &r_modified_inputs) {
        evaluate_node_elem_upstream(ctx_node, r_modified_inputs, elem_by_socket);
      },
      /* Propagate value. */
      [&](const SocketInContext &ctx_from, const SocketInContext &ctx_to) {
        return propagate_socket_elem(ctx_from, ctx_to, elem_by_socket);
      },
      /* Get input sockets to propagate. */
      [&](const NodeInContext &ctx_node, Vector<const bNodeSocket *> &r_sockets) {
        get_input_elems_to_propagate(ctx_node, r_sockets, elem_by_socket);
      });

  LocalInverseEvalTargets targets;

  for (const SocketInContext &ctx_socket : upstream_eval_targets.sockets) {
    if (ctx_socket.context) {
      /* Context should be empty because we only handle top-level sockets here. */
      continue;
    }
    const ElemVariant *elem = elem_by_socket.lookup_ptr(ctx_socket);
    if (!elem || !*elem) {
      continue;
    }
    targets.input_sockets.append({ctx_socket.socket, *elem});
  }

  for (const NodeInContext ctx_node : upstream_eval_targets.value_nodes) {
    if (ctx_node.context) {
      /* Context should be empty because we only handle top-level nodes here. */
      continue;
    }
    const bNodeSocket &socket = ctx_node.node->output_socket(0);
    const ElemVariant *elem = elem_by_socket.lookup_ptr({nullptr, &socket});
    if (!elem || !*elem) {
      continue;
    }
    targets.value_nodes.append({ctx_node.node, *elem});
  }

  for (const int group_input_index : tree.interface_inputs().index_range()) {
    const eNodeSocketDatatype type = eNodeSocketDatatype(
        tree.interface_inputs()[group_input_index]->socket_typeinfo()->type);
    std::optional<ElemVariant> elem = get_elem_variant_for_socket_type(type);
    if (!elem) {
      continue;
    }
    /* Combine the elems from each group input node. */
    for (const bNode *node : tree.group_input_nodes()) {
      const bNodeSocket &socket = node->output_socket(group_input_index);
      if (const ElemVariant *socket_elem = elem_by_socket.lookup_ptr({nullptr, &socket})) {
        elem->merge(*socket_elem);
      }
    }
    if (!*elem) {
      continue;
    }
    targets.group_inputs.append({group_input_index, *elem});
  }

  return targets;
}

static void evaluate_node_elem_downstream_filtered(
    const NodeInContext &ctx_node,
    const Map<SocketInContext, ElemVariant> &elem_by_socket_filter,
    Map<SocketInContext, ElemVariant> &elem_by_socket,
    Vector<const bNodeSocket *> &r_outputs_to_propagate)
{
  const bNode &node = *ctx_node.node;
  const bke::bNodeType &ntype = *node.typeinfo;
  if (!ntype.eval_elem) {
    return;
  }
  /* Build temporary map used by the node evaluation. */
  Map<const bNodeSocket *, ElemVariant> elem_by_local_socket;
  for (const bNodeSocket *input_socket : node.input_sockets()) {
    if (const ElemVariant *elem = elem_by_socket.lookup_ptr({ctx_node.context, input_socket})) {
      elem_by_local_socket.add(input_socket, *elem);
    }
  }
  Vector<SocketElem> output_elems;
  ElemEvalParams params{node, elem_by_local_socket, output_elems};
  ntype.eval_elem(params);
  /* Filter and store the outputs generated by the node evaluation. */
  for (const SocketElem &output_elem : output_elems) {
    if (output_elem.elem) {
      if (const ElemVariant *elem_filter = elem_by_socket_filter.lookup_ptr(
              {ctx_node.context, output_elem.socket}))
      {
        ElemVariant new_elem = *elem_filter;
        new_elem.intersect(output_elem.elem);
        elem_by_socket.add({ctx_node.context, output_elem.socket}, new_elem);
        if (new_elem) {
          r_outputs_to_propagate.append(output_elem.socket);
        }
      }
    }
  }
}

static bool propagate_value_elem_filtered(
    const SocketInContext &ctx_from,
    const SocketInContext &ctx_to,
    const Map<SocketInContext, ElemVariant> &elem_by_socket_filter,
    Map<SocketInContext, ElemVariant> &elem_by_socket)
{
  const ElemVariant *from_elem = elem_by_socket.lookup_ptr(ctx_from);
  if (!from_elem) {
    return false;
  }
  const ElemVariant *to_elem_filter = elem_by_socket_filter.lookup_ptr(ctx_to);
  if (!to_elem_filter) {
    return false;
  }
  const std::optional<ElemVariant> converted_elem = convert_socket_elem(
      *ctx_from.socket, *ctx_to.socket, *from_elem);
  if (!converted_elem) {
    return false;
  }
  if (ctx_to.socket->is_multi_input()) {
    ElemVariant added_elem = *converted_elem;
    added_elem.intersect(*to_elem_filter);
    elem_by_socket.lookup_or_add(ctx_to, added_elem).merge(added_elem);
    return true;
  }
  ElemVariant to_elem = *to_elem_filter;
  to_elem.intersect(*converted_elem);
  elem_by_socket.add(ctx_to, to_elem);
  return true;
}

void foreach_element_on_inverse_eval_path(
    const ComputeContext &initial_context,
    const SocketElem &initial_socket_elem,
    FunctionRef<void(const ComputeContext &context)> foreach_context_fn,
    FunctionRef<void(const ComputeContext &context,
                     const bNodeSocket &socket,
                     const ElemVariant &elem)> foreach_socket_fn)
{
  BLI_assert(initial_socket_elem.socket->is_input());
  if (!initial_socket_elem.elem) {
    return;
  }
  bke::ComputeContextCache compute_context_cache;
  Map<SocketInContext, ElemVariant> upstream_elem_by_socket;
  upstream_elem_by_socket.add({&initial_context, initial_socket_elem.socket},
                              initial_socket_elem.elem);

  /* In a first pass, propagate upstream to find the upstream targets. */
  const partial_eval::UpstreamEvalTargets upstream_eval_targets = partial_eval::eval_upstream(
      {{&initial_context, initial_socket_elem.socket}},
      compute_context_cache,
      /* Evaluate node. */
      [&](const NodeInContext &ctx_node, Vector<const bNodeSocket *> &r_modified_inputs) {
        evaluate_node_elem_upstream(ctx_node, r_modified_inputs, upstream_elem_by_socket);
      },
      /* Propagate value. */
      [&](const SocketInContext &ctx_from, const SocketInContext &ctx_to) {
        return propagate_socket_elem(ctx_from, ctx_to, upstream_elem_by_socket);
      },
      /* Get input sockets to propagate. */
      [&](const NodeInContext &ctx_node, Vector<const bNodeSocket *> &r_sockets) {
        get_input_elems_to_propagate(ctx_node, r_sockets, upstream_elem_by_socket);
      });

  /* The upstream propagation may also follow node paths that don't end up in upstream targets.
   * That can happen if there is a node on the path that does not support inverse evaluation. In
   * this case, parts of the evaluation path has to be discarded again. This is done using a second
   * pass. Now we start the evaluation at the discovered upstream targets and propagate the changed
   * socket elements downstream. We only care about the sockets that have already been used by
   * upstream evaluation, therefor the downstream evaluation is filtered. */

  /* Gather all upstream evaluation targets to start downstream evaluation there. */
  Vector<SocketInContext> initial_downstream_evaluation_sockets;
  initial_downstream_evaluation_sockets.extend(upstream_eval_targets.sockets.begin(),
                                               upstream_eval_targets.sockets.end());
  initial_downstream_evaluation_sockets.extend(upstream_eval_targets.group_inputs.begin(),
                                               upstream_eval_targets.group_inputs.end());
  for (const NodeInContext &ctx_node : upstream_eval_targets.value_nodes) {
    initial_downstream_evaluation_sockets.append(
        {ctx_node.context, &ctx_node.node->output_socket(0)});
  }

  Map<SocketInContext, ElemVariant> final_elem_by_socket;
  for (const SocketInContext &ctx_socket : initial_downstream_evaluation_sockets) {
    final_elem_by_socket.add(ctx_socket, upstream_elem_by_socket.lookup(ctx_socket));
  }

  partial_eval::eval_downstream(
      initial_downstream_evaluation_sockets,
      compute_context_cache,
      /* Evaluate node. */
      [&](const NodeInContext &ctx_node, Vector<const bNodeSocket *> &r_outputs_to_propagate) {
        evaluate_node_elem_downstream_filtered(
            ctx_node, upstream_elem_by_socket, final_elem_by_socket, r_outputs_to_propagate);
      },
      /* Propagate value. */
      [&](const SocketInContext &ctx_from, const SocketInContext &ctx_to) {
        return propagate_value_elem_filtered(
            ctx_from, ctx_to, upstream_elem_by_socket, final_elem_by_socket);
      });

  if (foreach_context_fn) {
    Set<ComputeContextHash> handled_hashes;
    for (const SocketInContext &ctx_socket : final_elem_by_socket.keys()) {
      if (handled_hashes.add(ctx_socket.context->hash())) {
        foreach_context_fn(*ctx_socket.context);
      }
    }
  }
  if (foreach_socket_fn) {
    for (auto &&item : final_elem_by_socket.items()) {
      foreach_socket_fn(*item.key.context, *item.key.socket, item.value);
    }
  }
}

using RNAValueVariant = std::variant<float, int, bool>;

static bool set_rna_property(bContext &C,
                             ID &id,
                             const StringRefNull rna_path,
                             const RNAValueVariant &value_variant)
{
  if (!ID_IS_EDITABLE(&id)) {
    return false;
  }

  PointerRNA id_ptr = RNA_id_pointer_create(&id);
  PointerRNA value_ptr;
  PropertyRNA *prop;
  int index;
  if (!RNA_path_resolve_property_full(&id_ptr, rna_path.c_str(), &value_ptr, &prop, &index)) {
    return false;
  }

  /* In the future, we could check if there is a driver on the property and propagate the change
   * backwards through the driver. */

  const PropertyType dst_type = RNA_property_type(prop);
  const int array_len = RNA_property_array_length(&value_ptr, prop);

  Scene *scene = CTX_data_scene(&C);
  const bool only_when_keyed = blender::animrig::is_keying_flag(scene,
                                                                AUTOKEY_FLAG_INSERTAVAILABLE);

  switch (dst_type) {
    case PROP_FLOAT: {
      float value = std::visit([](auto v) { return float(v); }, value_variant);
      float soft_min, soft_max, step, precision;
      RNA_property_float_ui_range(&value_ptr, prop, &soft_min, &soft_max, &step, &precision);
      value = std::clamp(value, soft_min, soft_max);
      if (array_len == 0) {
        RNA_property_float_set(&value_ptr, prop, value);
        RNA_property_update(&C, &value_ptr, prop);
        animrig::autokeyframe_property(
            &C, scene, &value_ptr, prop, 0, scene->r.cfra, only_when_keyed);
        return true;
      }
      if (index >= 0 && index < array_len) {
        RNA_property_float_set_index(&value_ptr, prop, index, value);
        RNA_property_update(&C, &value_ptr, prop);
        animrig::autokeyframe_property(
            &C, scene, &value_ptr, prop, index, scene->r.cfra, only_when_keyed);
        return true;
      }
      break;
    }
    case PROP_INT: {
      int value = std::visit([](auto v) { return int(v); }, value_variant);
      int soft_min, soft_max, step;
      RNA_property_int_ui_range(&value_ptr, prop, &soft_min, &soft_max, &step);
      value = std::clamp(value, soft_min, soft_max);
      if (array_len == 0) {
        RNA_property_int_set(&value_ptr, prop, value);
        RNA_property_update(&C, &value_ptr, prop);
        animrig::autokeyframe_property(
            &C, scene, &value_ptr, prop, 0, scene->r.cfra, only_when_keyed);
        return true;
      }
      if (index >= 0 && index < array_len) {
        RNA_property_int_set_index(&value_ptr, prop, index, value);
        RNA_property_update(&C, &value_ptr, prop);
        animrig::autokeyframe_property(
            &C, scene, &value_ptr, prop, index, scene->r.cfra, only_when_keyed);
        return true;
      }
      break;
    }
    case PROP_BOOLEAN: {
      const bool value = std::visit([](auto v) { return bool(v); }, value_variant);
      if (array_len == 0) {
        RNA_property_boolean_set(&value_ptr, prop, value);
        RNA_property_update(&C, &value_ptr, prop);
        animrig::autokeyframe_property(
            &C, scene, &value_ptr, prop, 0, scene->r.cfra, only_when_keyed);
        return true;
      }
      if (index >= 0 && index < array_len) {
        RNA_property_boolean_set_index(&value_ptr, prop, index, value);
        RNA_property_update(&C, &value_ptr, prop);
        animrig::autokeyframe_property(
            &C, scene, &value_ptr, prop, index, scene->r.cfra, only_when_keyed);
        return true;
      }
      break;
    }
    default:
      break;
  };

  return false;
}

static bool set_rna_property_float3(bContext &C,
                                    ID &id,
                                    const StringRefNull rna_path,
                                    const float3 &value)
{
  bool any_success = false;
  for (const int i : IndexRange(3)) {
    const std::string rna_path_for_index = fmt::format("{}[{}]", rna_path, i);
    any_success |= set_rna_property(C, id, rna_path_for_index, value[i]);
  }
  return any_success;
}

static bool set_socket_value(bContext &C,
                             bNodeSocket &socket,
                             const SocketValueVariant &value_variant)
{
  bNode &node = socket.owner_node();
  bNodeTree &tree = socket.owner_tree();

  const std::string default_value_rna_path = fmt::format(
      "nodes[\"{}\"].inputs[{}].default_value", BLI_str_escape(node.name), socket.index());

  switch (socket.type) {
    case SOCK_FLOAT: {
      const float value = value_variant.get<float>();
      return set_rna_property(C, tree.id, default_value_rna_path, value);
    }
    case SOCK_INT: {
      const int value = value_variant.get<int>();
      return set_rna_property(C, tree.id, default_value_rna_path, value);
    }
    case SOCK_BOOLEAN: {
      const bool value = value_variant.get<bool>();
      return set_rna_property(C, tree.id, default_value_rna_path, value);
    }
    case SOCK_VECTOR: {
      const float3 value = value_variant.get<float3>();
      return set_rna_property_float3(C, tree.id, default_value_rna_path, value);
    }
    case SOCK_ROTATION: {
      const math::Quaternion rotation = value_variant.get<math::Quaternion>();
      const float3 euler = float3(math::to_euler(rotation));
      return set_rna_property_float3(C, tree.id, default_value_rna_path, euler);
    }
  }
  return false;
}

static bool set_value_node_value(bContext &C, bNode &node, const SocketValueVariant &value_variant)
{
  bNodeTree &tree = node.owner_tree();

  switch (node.type_legacy) {
    case SH_NODE_VALUE: {
      const float value = value_variant.get<float>();
      const std::string rna_path = fmt::format("nodes[\"{}\"].outputs[0].default_value",
                                               BLI_str_escape(node.name));
      return set_rna_property(C, tree.id, rna_path, value);
    }
    case FN_NODE_INPUT_INT: {
      const int value = value_variant.get<int>();
      const std::string rna_path = fmt::format("nodes[\"{}\"].integer", BLI_str_escape(node.name));
      return set_rna_property(C, tree.id, rna_path, value);
    }
    case FN_NODE_INPUT_BOOL: {
      const bool value = value_variant.get<bool>();
      const std::string rna_path = fmt::format("nodes[\"{}\"].boolean", BLI_str_escape(node.name));
      return set_rna_property(C, tree.id, rna_path, value);
    }
    case FN_NODE_INPUT_VECTOR: {
      const float3 value = value_variant.get<float3>();
      const std::string rna_path = fmt::format("nodes[\"{}\"].vector", BLI_str_escape(node.name));
      return set_rna_property_float3(C, tree.id, rna_path, value);
    }
    case FN_NODE_INPUT_ROTATION: {
      const math::Quaternion rotation = value_variant.get<math::Quaternion>();
      const float3 euler = float3(math::to_euler(rotation));
      const std::string rna_path = fmt::format("nodes[\"{}\"].rotation_euler",
                                               BLI_str_escape(node.name));
      return set_rna_property_float3(C, tree.id, rna_path, euler);
    }
  }
  return false;
}

static bool set_modifier_value(bContext &C,
                               Object &object,
                               NodesModifierData &nmd,
                               const bNodeTreeInterfaceSocket &interface_socket,
                               const SocketValueVariant &value_variant)
{
  DEG_id_tag_update(&object.id, ID_RECALC_GEOMETRY);

  const std::string main_prop_rna_path = fmt::format(
      "modifiers[\"{}\"][\"{}\"]", BLI_str_escape(nmd.modifier.name), interface_socket.identifier);

  switch (interface_socket.socket_typeinfo()->type) {
    case SOCK_FLOAT: {
      const float value = value_variant.get<float>();
      return set_rna_property(C, object.id, main_prop_rna_path, value);
    }
    case SOCK_INT: {
      const int value = value_variant.get<int>();
      return set_rna_property(C, object.id, main_prop_rna_path, value);
    }
    case SOCK_BOOLEAN: {
      const bool value = value_variant.get<bool>();
      return set_rna_property(C, object.id, main_prop_rna_path, value);
    }
    case SOCK_VECTOR: {
      const float3 value = value_variant.get<float3>();
      return set_rna_property_float3(C, object.id, main_prop_rna_path, value);
    }
    case SOCK_ROTATION: {
      const math::Quaternion rotation = value_variant.get<math::Quaternion>();
      const float3 euler = float3(math::to_euler(rotation));
      return set_rna_property_float3(C, object.id, main_prop_rna_path, euler);
    }
    default:
      return false;
  }
}

std::optional<SocketValueVariant> get_logged_socket_value(geo_eval_log::GeoTreeLog &tree_log,
                                                          const bNodeSocket &socket)
{
  switch (socket.type) {
    case SOCK_FLOAT: {
      if (const std::optional<float> value = tree_log.find_primitive_socket_value<float>(socket)) {
        return SocketValueVariant{*value};
      }
      break;
    }
    case SOCK_INT: {
      if (const std::optional<int> value = tree_log.find_primitive_socket_value<int>(socket)) {
        return SocketValueVariant{*value};
      }
      break;
    }
    case SOCK_BOOLEAN: {
      if (const std::optional<bool> value = tree_log.find_primitive_socket_value<bool>(socket)) {
        return SocketValueVariant{*value};
      }
      break;
    }
    case SOCK_VECTOR: {
      if (const std::optional<float3> value = tree_log.find_primitive_socket_value<float3>(socket))
      {
        return SocketValueVariant{*value};
      }
      break;
    }
    case SOCK_ROTATION: {
      if (const std::optional<math::Quaternion> value =
              tree_log.find_primitive_socket_value<math::Quaternion>(socket))
      {
        return SocketValueVariant{*value};
      }
      break;
    }
    case SOCK_MATRIX: {
      if (const std::optional<float4x4> value = tree_log.find_primitive_socket_value<float4x4>(
              socket))
      {
        return SocketValueVariant{*value};
      }
      break;
    }
  }
  return std::nullopt;
}

static void backpropagate_socket_values_through_node(
    const NodeInContext &ctx_node,
    geo_eval_log::GeoNodesLog &eval_log,
    Map<SocketInContext, SocketValueVariant> &value_by_socket,
    Vector<const bNodeSocket *> &r_modified_inputs)
{
  const bNode &node = *ctx_node.node;
  const ComputeContext *context = ctx_node.context;
  const bke::bNodeType &ntype = *node.typeinfo;
  if (!ntype.eval_inverse) {
    /* Node does not support inverse evaluation. */
    return;
  }
  if (!context) {
    /* We need a context here to access the tree log. */
    return;
  }
  geo_eval_log::GeoTreeLog &tree_log = eval_log.get_tree_log(context->hash());
  tree_log.ensure_socket_values();

  /* Build a temporary map of old socket values for the node evaluation. */
  Map<const bNodeSocket *, SocketValueVariant> old_socket_values;
  for (const bNodeSocket *socket : node.input_sockets()) {
    if (!socket->is_available()) {
      continue;
    }
    /* Retrieve input socket values from the log. */
    if (const std::optional<SocketValueVariant> value = get_logged_socket_value(tree_log, *socket))
    {
      old_socket_values.add(socket, *value);
    }
  }
  for (const bNodeSocket *socket : node.output_sockets()) {
    if (!socket->is_available()) {
      continue;
    }
    /* First check if there is an updated socket value for an output socket. */
    if (const SocketValueVariant *value = value_by_socket.lookup_ptr({context, socket})) {
      old_socket_values.add(socket, *value);
    }
    /* If not, retrieve the output socket value from the log. */
    else if (const std::optional<SocketValueVariant> value = get_logged_socket_value(tree_log,
                                                                                     *socket))
    {
      old_socket_values.add(socket, *value);
    }
  }

  Map<const bNodeSocket *, SocketValueVariant> updated_socket_values;
  InverseEvalParams params{node, old_socket_values, updated_socket_values};
  ntype.eval_inverse(params);
  /* Write back new socket values. */
  for (auto &&item : updated_socket_values.items()) {
    const bNodeSocket &socket = *item.key;
    value_by_socket.add({context, &socket}, std::move(item.value));
    r_modified_inputs.append(&socket);
  }
}

bool backpropagate_socket_values(bContext &C,
                                 Object &object,
                                 NodesModifierData &nmd,
                                 geo_eval_log::GeoNodesLog &eval_log,
                                 const Span<SocketToUpdate> sockets_to_update)
{
  nmd.node_group->ensure_topology_cache();

  bke::ComputeContextCache compute_context_cache;
  Map<SocketInContext, SocketValueVariant> value_by_socket;

  Vector<SocketInContext> initial_sockets;

  /* Gather starting values for the backpropagation. */
  for (const SocketToUpdate &socket_to_update : sockets_to_update) {
    if (socket_to_update.multi_input_link) {
      BLI_assert(socket_to_update.multi_input_link->tosock == socket_to_update.socket);
      const std::optional<SocketValueVariant> converted_value = convert_single_socket_value(
          *socket_to_update.socket,
          *socket_to_update.multi_input_link->fromsock,
          socket_to_update.new_value);
      if (!converted_value) {
        continue;
      }
      value_by_socket.add({socket_to_update.context, socket_to_update.multi_input_link->fromsock},
                          *converted_value);
    }
    else {
      value_by_socket.add({socket_to_update.context, socket_to_update.socket},
                          socket_to_update.new_value);
    }
  }

  if (value_by_socket.is_empty()) {
    return false;
  }

  for (const SocketInContext &ctx_socket : value_by_socket.keys()) {
    initial_sockets.append(ctx_socket);
  }

  /* Actually backpropagate the socket values as far as possible in the node tree. */
  const partial_eval::UpstreamEvalTargets upstream_eval_targets = partial_eval::eval_upstream(
      initial_sockets,
      compute_context_cache,
      /* Evaluate node. */
      [&](const NodeInContext &ctx_node, Vector<const bNodeSocket *> &r_modified_inputs) {
        backpropagate_socket_values_through_node(
            ctx_node, eval_log, value_by_socket, r_modified_inputs);
      },
      /* Propagate value. */
      [&](const SocketInContext &ctx_from, const SocketInContext &ctx_to) {
        const SocketValueVariant *from_value = value_by_socket.lookup_ptr(ctx_from);
        if (!from_value) {
          return false;
        }
        const std::optional<SocketValueVariant> converted_value = convert_single_socket_value(
            *ctx_from.socket, *ctx_to.socket, *from_value);
        if (!converted_value) {
          return false;
        }
        value_by_socket.add(ctx_to, std::move(*converted_value));
        return true;
      },
      /* Get input sockets to propagate. */
      [&](const NodeInContext &ctx_node, Vector<const bNodeSocket *> &r_sockets) {
        for (const bNodeSocket *socket : ctx_node.node->input_sockets()) {
          if (value_by_socket.contains({ctx_node.context, socket})) {
            r_sockets.append(socket);
          }
        }
      });

  bool any_success = false;
  /* Set new values for sockets. */
  for (const SocketInContext &ctx_socket : upstream_eval_targets.sockets) {
    if (const SocketValueVariant *value = value_by_socket.lookup_ptr(ctx_socket)) {
      bNodeSocket &socket_mutable = const_cast<bNodeSocket &>(*ctx_socket.socket);
      any_success |= set_socket_value(C, socket_mutable, *value);
    }
  }
  /* Set new values for value nodes. */
  for (const NodeInContext &ctx_node : upstream_eval_targets.value_nodes) {
    if (const SocketValueVariant *value = value_by_socket.lookup_ptr(
            {ctx_node.context, &ctx_node.node->output_socket(0)}))
    {
      bNode &node_mutable = const_cast<bNode &>(*ctx_node.node);
      any_success |= set_value_node_value(C, node_mutable, *value);
    }
  }
  /* Set new values for modifier inputs. */
  const bke::ModifierComputeContext modifier_context{nullptr, nmd};
  for (const bNode *group_input_node : nmd.node_group->group_input_nodes()) {
    for (const bNodeSocket *socket : group_input_node->output_sockets().drop_back(1)) {
      if (const SocketValueVariant *value = value_by_socket.lookup_ptr(
              {&modifier_context, socket}))
      {
        any_success |= set_modifier_value(
            C, object, nmd, *nmd.node_group->interface_inputs()[socket->index()], *value);
      }
    }
  }

  return any_success;
}

InverseEvalParams::InverseEvalParams(
    const bNode &node,
    const Map<const bNodeSocket *, bke::SocketValueVariant> &socket_values,
    Map<const bNodeSocket *, bke::SocketValueVariant> &updated_socket_values)
    : socket_values_(socket_values), updated_socket_values_(updated_socket_values), node(node)
{
}

}  // namespace blender::nodes::inverse_eval
