/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "NOD_node_tree_multi_function.hh"
#include "NOD_type_conversions.hh"

#include "FN_multi_function_network_evaluation.hh"

#include "BLI_color.hh"
#include "BLI_float2.hh"
#include "BLI_float3.hh"

namespace blender::nodes {

const fn::MultiFunction &NodeMFNetworkBuilder::get_default_fn(StringRef name)
{
  Vector<fn::MFDataType, 10> input_types;
  Vector<fn::MFDataType, 10> output_types;

  for (const InputSocketRef *dsocket : dnode_->inputs()) {
    if (dsocket->is_available()) {
      std::optional<fn::MFDataType> data_type = socket_mf_type_get(*dsocket->typeinfo());
      if (data_type.has_value()) {
        input_types.append(*data_type);
      }
    }
  }
  for (const OutputSocketRef *dsocket : dnode_->outputs()) {
    if (dsocket->is_available()) {
      std::optional<fn::MFDataType> data_type = socket_mf_type_get(*dsocket->typeinfo());
      if (data_type.has_value()) {
        output_types.append(*data_type);
      }
    }
  }

  const fn::MultiFunction &fn = this->construct_fn<fn::CustomMF_DefaultOutput>(
      name, input_types, output_types);
  return fn;
}

static void insert_dummy_node(CommonMFNetworkBuilderData &common, const DNode &dnode)
{
  constexpr int stack_capacity = 10;

  Vector<fn::MFDataType, stack_capacity> input_types;
  Vector<StringRef, stack_capacity> input_names;
  Vector<const InputSocketRef *, stack_capacity> input_dsockets;

  for (const InputSocketRef *dsocket : dnode->inputs()) {
    if (dsocket->is_available()) {
      std::optional<fn::MFDataType> data_type = socket_mf_type_get(*dsocket->bsocket()->typeinfo);
      if (data_type.has_value()) {
        input_types.append(*data_type);
        input_names.append(dsocket->name());
        input_dsockets.append(dsocket);
      }
    }
  }

  Vector<fn::MFDataType, stack_capacity> output_types;
  Vector<StringRef, stack_capacity> output_names;
  Vector<const OutputSocketRef *, stack_capacity> output_dsockets;

  for (const OutputSocketRef *dsocket : dnode->outputs()) {
    if (dsocket->is_available()) {
      std::optional<fn::MFDataType> data_type = socket_mf_type_get(*dsocket->bsocket()->typeinfo);
      if (data_type.has_value()) {
        output_types.append(*data_type);
        output_names.append(dsocket->name());
        output_dsockets.append(dsocket);
      }
    }
  }

  fn::MFDummyNode &dummy_node = common.network.add_dummy(
      dnode->name(), input_types, output_types, input_names, output_names);

  common.network_map.add(*dnode.context(), input_dsockets, dummy_node.inputs());
  common.network_map.add(*dnode.context(), output_dsockets, dummy_node.outputs());
}

static bool has_data_sockets(const DNode &dnode)
{
  for (const InputSocketRef *socket : dnode->inputs()) {
    if (socket_is_mf_data_socket(*socket->bsocket()->typeinfo)) {
      return true;
    }
  }
  for (const OutputSocketRef *socket : dnode->outputs()) {
    if (socket_is_mf_data_socket(*socket->bsocket()->typeinfo)) {
      return true;
    }
  }
  return false;
}

static void foreach_node_to_insert(CommonMFNetworkBuilderData &common,
                                   FunctionRef<void(DNode)> callback)
{
  common.tree.foreach_node([&](const DNode dnode) {
    if (dnode->is_group_node()) {
      return;
    }
    /* Don't insert non-root group input/output nodes, because they will be inlined. */
    if (!dnode.context()->is_root()) {
      if (dnode->is_group_input_node() || dnode->is_group_output_node()) {
        return;
      }
    }
    callback(dnode);
  });
}

/**
 * Expands all function nodes in the multi-function network. Nodes that don't have an expand
 * function, but do have data sockets, will get corresponding dummy nodes.
 */
static void insert_nodes(CommonMFNetworkBuilderData &common)
{
  foreach_node_to_insert(common, [&](const DNode dnode) {
    const bNodeType *node_type = dnode->typeinfo();
    if (node_type->expand_in_mf_network != nullptr) {
      NodeMFNetworkBuilder builder{common, dnode};
      node_type->expand_in_mf_network(builder);
    }
    else if (has_data_sockets(dnode)) {
      insert_dummy_node(common, dnode);
    }
  });
}

static fn::MFOutputSocket &insert_default_value_for_type(CommonMFNetworkBuilderData &common,
                                                         fn::MFDataType type)
{
  const fn::MultiFunction *default_fn;
  if (type.is_single()) {
    default_fn = &common.scope.construct<fn::CustomMF_GenericConstant>(
        AT, type.single_type(), type.single_type().default_value());
  }
  else {
    default_fn = &common.scope.construct<fn::CustomMF_GenericConstantArray>(
        AT, fn::GSpan(type.vector_base_type()));
  }

  fn::MFNode &node = common.network.add_function(*default_fn);
  return node.output(0);
}

static fn::MFOutputSocket *insert_unlinked_input(CommonMFNetworkBuilderData &common,
                                                 const DInputSocket &dsocket)
{
  BLI_assert(socket_is_mf_data_socket(*dsocket->typeinfo()));

  SocketMFNetworkBuilder builder{common, dsocket};
  socket_expand_in_mf_network(builder);

  fn::MFOutputSocket *built_socket = builder.built_socket();
  BLI_assert(built_socket != nullptr);
  return built_socket;
}

static void insert_links_and_unlinked_inputs(CommonMFNetworkBuilderData &common)
{
  foreach_node_to_insert(common, [&](const DNode dnode) {
    for (const InputSocketRef *socket_ref : dnode->inputs()) {
      const DInputSocket to_dsocket{dnode.context(), socket_ref};
      if (!to_dsocket->is_available()) {
        continue;
      }
      if (!socket_is_mf_data_socket(*to_dsocket->typeinfo())) {
        continue;
      }

      Span<fn::MFInputSocket *> to_sockets = common.network_map.lookup(to_dsocket);
      BLI_assert(to_sockets.size() >= 1);
      const fn::MFDataType to_type = to_sockets[0]->data_type();

      Vector<DSocket> from_dsockets;
      to_dsocket.foreach_origin_socket([&](DSocket socket) { from_dsockets.append(socket); });
      if (from_dsockets.size() > 1) {
        fn::MFOutputSocket &from_socket = insert_default_value_for_type(common, to_type);
        for (fn::MFInputSocket *to_socket : to_sockets) {
          common.network.add_link(from_socket, *to_socket);
        }
        continue;
      }
      if (from_dsockets.is_empty()) {
        /* The socket is not linked. Need to use the value of the socket itself. */
        fn::MFOutputSocket *built_socket = insert_unlinked_input(common, to_dsocket);
        for (fn::MFInputSocket *to_socket : to_sockets) {
          common.network.add_link(*built_socket, *to_socket);
        }
        continue;
      }
      if (from_dsockets[0]->is_input()) {
        DInputSocket from_dsocket{from_dsockets[0]};
        fn::MFOutputSocket *built_socket = insert_unlinked_input(common, from_dsocket);
        for (fn::MFInputSocket *to_socket : to_sockets) {
          common.network.add_link(*built_socket, *to_socket);
        }
        continue;
      }
      DOutputSocket from_dsocket{from_dsockets[0]};
      fn::MFOutputSocket *from_socket = &common.network_map.lookup(from_dsocket);
      const fn::MFDataType from_type = from_socket->data_type();

      if (from_type != to_type) {
        const fn::MultiFunction *conversion_fn =
            get_implicit_type_conversions().get_conversion_multi_function(from_type, to_type);
        if (conversion_fn != nullptr) {
          fn::MFNode &node = common.network.add_function(*conversion_fn);
          common.network.add_link(*from_socket, node.input(0));
          from_socket = &node.output(0);
        }
        else {
          from_socket = &insert_default_value_for_type(common, to_type);
        }
      }

      for (fn::MFInputSocket *to_socket : to_sockets) {
        common.network.add_link(*from_socket, *to_socket);
      }
    }
  });
}

/**
 * Expands all function nodes contained in the given node tree within the given multi-function
 * network.
 *
 * Returns a mapping between the original node tree and the generated nodes/sockets for further
 * processing.
 */
MFNetworkTreeMap insert_node_tree_into_mf_network(fn::MFNetwork &network,
                                                  const DerivedNodeTree &tree,
                                                  ResourceScope &scope)
{
  MFNetworkTreeMap network_map{tree, network};

  CommonMFNetworkBuilderData common{scope, network, network_map, tree};

  insert_nodes(common);
  insert_links_and_unlinked_inputs(common);

  return network_map;
}

/**
 * A single node is allowed to expand into multiple nodes before evaluation. Depending on what
 * nodes it expands to, it belongs a different type of the ones below.
 */
enum class NodeExpandType {
  SingleFunctionNode,
  MultipleFunctionNodes,
  HasDummyNodes,
};

/**
 * Checks how the given node expanded in the multi-function network. If it is only a single
 * function node, the corresponding function is returned as well.
 */
static NodeExpandType get_node_expand_type(MFNetworkTreeMap &network_map,
                                           const DNode &dnode,
                                           const fn::MultiFunction **r_single_function)
{
  const fn::MFFunctionNode *single_function_node = nullptr;
  bool has_multiple_nodes = false;
  bool has_dummy_nodes = false;

  auto check_mf_node = [&](fn::MFNode &mf_node) {
    if (mf_node.is_function()) {
      if (single_function_node == nullptr) {
        single_function_node = &mf_node.as_function();
      }
      if (&mf_node != single_function_node) {
        has_multiple_nodes = true;
      }
    }
    else {
      BLI_assert(mf_node.is_dummy());
      has_dummy_nodes = true;
    }
  };

  for (const InputSocketRef *dsocket : dnode->inputs()) {
    if (dsocket->is_available()) {
      for (fn::MFInputSocket *mf_input :
           network_map.lookup(DInputSocket(dnode.context(), dsocket))) {
        check_mf_node(mf_input->node());
      }
    }
  }
  for (const OutputSocketRef *dsocket : dnode->outputs()) {
    if (dsocket->is_available()) {
      fn::MFOutputSocket &mf_output = network_map.lookup(DOutputSocket(dnode.context(), dsocket));
      check_mf_node(mf_output.node());
    }
  }

  if (has_dummy_nodes) {
    return NodeExpandType::HasDummyNodes;
  }
  if (has_multiple_nodes) {
    return NodeExpandType::MultipleFunctionNodes;
  }
  *r_single_function = &single_function_node->function();
  return NodeExpandType::SingleFunctionNode;
}

static const fn::MultiFunction &create_function_for_node_that_expands_into_multiple(
    const DNode &dnode,
    fn::MFNetwork &network,
    MFNetworkTreeMap &network_map,
    ResourceScope &scope)
{
  Vector<const fn::MFOutputSocket *> dummy_fn_inputs;
  for (const InputSocketRef *dsocket : dnode->inputs()) {
    if (dsocket->is_available()) {
      MFDataType data_type = *socket_mf_type_get(*dsocket->typeinfo());
      fn::MFOutputSocket &fn_input = network.add_input(data_type.to_string(), data_type);
      for (fn::MFInputSocket *mf_input :
           network_map.lookup(DInputSocket(dnode.context(), dsocket))) {
        network.add_link(fn_input, *mf_input);
        dummy_fn_inputs.append(&fn_input);
      }
    }
  }
  Vector<const fn::MFInputSocket *> dummy_fn_outputs;
  for (const OutputSocketRef *dsocket : dnode->outputs()) {
    if (dsocket->is_available()) {
      fn::MFOutputSocket &mf_output = network_map.lookup(DOutputSocket(dnode.context(), dsocket));
      MFDataType data_type = mf_output.data_type();
      fn::MFInputSocket &fn_output = network.add_output(data_type.to_string(), data_type);
      network.add_link(mf_output, fn_output);
      dummy_fn_outputs.append(&fn_output);
    }
  }

  fn::MFNetworkEvaluator &fn_evaluator = scope.construct<fn::MFNetworkEvaluator>(
      __func__, std::move(dummy_fn_inputs), std::move(dummy_fn_outputs));
  return fn_evaluator;
}

/**
 * Returns a single multi-function for every node that supports it. This makes it easier to reuse
 * the multi-function implementation of nodes in different contexts.
 */
MultiFunctionByNode get_multi_function_per_node(const DerivedNodeTree &tree, ResourceScope &scope)
{
  /* Build a network that nodes can insert themselves into. However, the individual nodes are not
   * connected. */
  fn::MFNetwork &network = scope.construct<fn::MFNetwork>(__func__);
  MFNetworkTreeMap network_map{tree, network};
  MultiFunctionByNode functions_by_node;

  CommonMFNetworkBuilderData common{scope, network, network_map, tree};

  tree.foreach_node([&](DNode dnode) {
    const bNodeType *node_type = dnode->typeinfo();
    if (node_type->expand_in_mf_network == nullptr) {
      /* This node does not have a multi-function implementation. */
      return;
    }

    NodeMFNetworkBuilder builder{common, dnode};
    node_type->expand_in_mf_network(builder);

    const fn::MultiFunction *single_function = nullptr;
    const NodeExpandType expand_type = get_node_expand_type(network_map, dnode, &single_function);

    switch (expand_type) {
      case NodeExpandType::HasDummyNodes: {
        /* Dummy nodes cannot be executed, so skip them. */
        break;
      }
      case NodeExpandType::SingleFunctionNode: {
        /* This is the common case. Most nodes just expand to a single function. */
        functions_by_node.add_new(dnode, single_function);
        break;
      }
      case NodeExpandType::MultipleFunctionNodes: {
        /* If a node expanded into multiple functions, a new function has to be created that
         * combines those. */
        const fn::MultiFunction &fn = create_function_for_node_that_expands_into_multiple(
            dnode, network, network_map, scope);
        functions_by_node.add_new(dnode, &fn);
        break;
      }
    }
  });

  return functions_by_node;
}

}  // namespace blender::nodes
