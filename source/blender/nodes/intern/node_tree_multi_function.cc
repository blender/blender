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

template<typename From, typename To>
static void add_implicit_conversion(DataTypeConversions &conversions)
{
  static fn::CustomMF_Convert<From, To> function;
  conversions.add(fn::MFDataType::ForSingle<From>(), fn::MFDataType::ForSingle<To>(), function);
}

template<typename From, typename To, typename ConversionF>
static void add_implicit_conversion(DataTypeConversions &conversions,
                                    StringRef name,
                                    ConversionF conversion)
{
  static fn::CustomMF_SI_SO<From, To> function{name, conversion};
  conversions.add(fn::MFDataType::ForSingle<From>(), fn::MFDataType::ForSingle<To>(), function);
}

static DataTypeConversions create_implicit_conversions()
{
  DataTypeConversions conversions;
  add_implicit_conversion<float, float2>(conversions);
  add_implicit_conversion<float, float3>(conversions);
  add_implicit_conversion<float, int32_t>(conversions);
  add_implicit_conversion<float, bool>(conversions);
  add_implicit_conversion<float, Color4f>(
      conversions, "float to Color4f", [](float a) { return Color4f(a, a, a, 1.0f); });

  add_implicit_conversion<float2, float3>(
      conversions, "float2 to float3", [](float2 a) { return float3(a.x, a.y, 0.0f); });
  add_implicit_conversion<float2, float>(
      conversions, "float2 to float", [](float2 a) { return a.length(); });
  add_implicit_conversion<float2, int32_t>(
      conversions, "float2 to int32_t", [](float2 a) { return (int32_t)a.length(); });
  add_implicit_conversion<float2, bool>(
      conversions, "float2 to bool", [](float2 a) { return a.length_squared() == 0.0f; });
  add_implicit_conversion<float2, Color4f>(
      conversions, "float2 to Color4f", [](float2 a) { return Color4f(a.x, a.y, 0.0f, 1.0f); });

  add_implicit_conversion<float3, bool>(
      conversions, "float3 to boolean", [](float3 a) { return a.length_squared() == 0.0f; });
  add_implicit_conversion<float3, float>(
      conversions, "Vector Length", [](float3 a) { return a.length(); });
  add_implicit_conversion<float3, int32_t>(
      conversions, "float3 to int32_t", [](float3 a) { return (int)a.length(); });
  add_implicit_conversion<float3, float2>(conversions);
  add_implicit_conversion<float3, Color4f>(
      conversions, "float3 to Color4f", [](float3 a) { return Color4f(a.x, a.y, a.z, 1.0f); });

  add_implicit_conversion<int32_t, bool>(conversions);
  add_implicit_conversion<int32_t, float>(conversions);
  add_implicit_conversion<int32_t, float2>(
      conversions, "int32 to float2", [](int32_t a) { return float2((float)a); });
  add_implicit_conversion<int32_t, float3>(
      conversions, "int32 to float3", [](int32_t a) { return float3((float)a); });

  add_implicit_conversion<bool, float>(conversions);
  add_implicit_conversion<bool, int32_t>(conversions);
  add_implicit_conversion<bool, float2>(
      conversions, "boolean to float2", [](bool a) { return (a) ? float2(1.0f) : float2(0.0f); });
  add_implicit_conversion<bool, float3>(
      conversions, "boolean to float3", [](bool a) { return (a) ? float3(1.0f) : float3(0.0f); });
  add_implicit_conversion<bool, Color4f>(conversions, "boolean to Color4f", [](bool a) {
    return (a) ? Color4f(1.0f, 1.0f, 1.0f, 1.0f) : Color4f(0.0f, 0.0f, 0.0f, 1.0f);
  });

  add_implicit_conversion<Color4f, bool>(conversions, "Color4f to boolean", [](Color4f a) {
    return a.r == 0.0f && a.g == 0.0f && a.b == 0.0f;
  });
  add_implicit_conversion<Color4f, float>(
      conversions, "Color4f to float", [](Color4f a) { return rgb_to_grayscale(a); });
  add_implicit_conversion<Color4f, float2>(
      conversions, "Color4f to float2", [](Color4f a) { return float2(a.r, a.g); });
  add_implicit_conversion<Color4f, float3>(
      conversions, "Color4f to float3", [](Color4f a) { return float3(a.r, a.g, a.b); });

  return conversions;
}

const DataTypeConversions &get_implicit_type_conversions()
{
  static const DataTypeConversions conversions = create_implicit_conversions();
  return conversions;
}

void DataTypeConversions::convert(const CPPType &from_type,
                                  const CPPType &to_type,
                                  const void *from_value,
                                  void *to_value) const
{
  const fn::MultiFunction *fn = this->get_conversion(MFDataType::ForSingle(from_type),
                                                     MFDataType::ForSingle(to_type));
  BLI_assert(fn != nullptr);

  fn::MFContextBuilder context;
  fn::MFParamsBuilder params{*fn, 1};
  params.add_readonly_single_input(fn::GSpan(from_type, from_value, 1));
  params.add_uninitialized_single_output(fn::GMutableSpan(to_type, to_value, 1));
  fn->call({0}, params, context);
}

static fn::MFOutputSocket &insert_default_value_for_type(CommonMFNetworkBuilderData &common,
                                                         fn::MFDataType type)
{
  const fn::MultiFunction *default_fn;
  if (type.is_single()) {
    default_fn = &common.resources.construct<fn::CustomMF_GenericConstant>(
        AT, type.single_type(), type.single_type().default_value());
  }
  else {
    default_fn = &common.resources.construct<fn::CustomMF_GenericConstantArray>(
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
        const fn::MultiFunction *conversion_fn = get_implicit_type_conversions().get_conversion(
            from_type, to_type);
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
                                                  ResourceCollector &resources)
{
  MFNetworkTreeMap network_map{tree, network};

  CommonMFNetworkBuilderData common{resources, network, network_map, tree};

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
    ResourceCollector &resources)
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

  fn::MFNetworkEvaluator &fn_evaluator = resources.construct<fn::MFNetworkEvaluator>(
      __func__, std::move(dummy_fn_inputs), std::move(dummy_fn_outputs));
  return fn_evaluator;
}

/**
 * Returns a single multi-function for every node that supports it. This makes it easier to reuse
 * the multi-function implementation of nodes in different contexts.
 */
MultiFunctionByNode get_multi_function_per_node(const DerivedNodeTree &tree,
                                                ResourceCollector &resources)
{
  /* Build a network that nodes can insert themselves into. However, the individual nodes are not
   * connected. */
  fn::MFNetwork &network = resources.construct<fn::MFNetwork>(__func__);
  MFNetworkTreeMap network_map{tree, network};
  MultiFunctionByNode functions_by_node;

  CommonMFNetworkBuilderData common{resources, network, network_map, tree};

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
            dnode, network, network_map, resources);
        functions_by_node.add_new(dnode, &fn);
        break;
      }
    }
  });

  return functions_by_node;
}

}  // namespace blender::nodes
