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
#include "BLI_float3.hh"

namespace blender::nodes {

const fn::MultiFunction &NodeMFNetworkBuilder::get_default_fn(StringRef name)
{
  Vector<fn::MFDataType, 10> input_types;
  Vector<fn::MFDataType, 10> output_types;

  for (const DInputSocket *dsocket : dnode_.inputs()) {
    if (dsocket->is_available()) {
      std::optional<fn::MFDataType> data_type = socket_mf_type_get(*dsocket->bsocket()->typeinfo);
      if (data_type.has_value()) {
        input_types.append(*data_type);
      }
    }
  }
  for (const DOutputSocket *dsocket : dnode_.outputs()) {
    if (dsocket->is_available()) {
      std::optional<fn::MFDataType> data_type = socket_mf_type_get(*dsocket->bsocket()->typeinfo);
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
  Vector<const DInputSocket *, stack_capacity> input_dsockets;

  for (const DInputSocket *dsocket : dnode.inputs()) {
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
  Vector<const DOutputSocket *, stack_capacity> output_dsockets;

  for (const DOutputSocket *dsocket : dnode.outputs()) {
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
      dnode.name(), input_types, output_types, input_names, output_names);

  common.network_map.add(input_dsockets, dummy_node.inputs());
  common.network_map.add(output_dsockets, dummy_node.outputs());
}

static bool has_data_sockets(const DNode &dnode)
{
  for (const DInputSocket *socket : dnode.inputs()) {
    if (socket_is_mf_data_socket(*socket->bsocket()->typeinfo)) {
      return true;
    }
  }
  for (const DOutputSocket *socket : dnode.outputs()) {
    if (socket_is_mf_data_socket(*socket->bsocket()->typeinfo)) {
      return true;
    }
  }
  return false;
}

/**
 * Expands all function nodes in the multi-function network. Nodes that don't have an expand
 * function, but do have data sockets, will get corresponding dummy nodes.
 */
static void insert_nodes(CommonMFNetworkBuilderData &common)
{
  for (const DNode *dnode : common.tree.nodes()) {
    const bNodeType *node_type = dnode->node_ref().bnode()->typeinfo;
    if (node_type->expand_in_mf_network != nullptr) {
      NodeMFNetworkBuilder builder{common, *dnode};
      node_type->expand_in_mf_network(builder);
    }
    else if (has_data_sockets(*dnode)) {
      insert_dummy_node(common, *dnode);
    }
  }
}

static void insert_group_inputs(CommonMFNetworkBuilderData &common)
{
  for (const DGroupInput *group_input : common.tree.group_inputs()) {
    bNodeSocket *bsocket = group_input->bsocket();
    if (socket_is_mf_data_socket(*bsocket->typeinfo)) {
      bNodeSocketType *socktype = bsocket->typeinfo;
      BLI_assert(socktype->expand_in_mf_network != nullptr);

      SocketMFNetworkBuilder builder{common, *group_input};
      socktype->expand_in_mf_network(builder);

      fn::MFOutputSocket *from_socket = builder.built_socket();
      BLI_assert(from_socket != nullptr);
      common.network_map.add(*group_input, *from_socket);
    }
  }
}

static fn::MFOutputSocket *try_find_origin(CommonMFNetworkBuilderData &common,
                                           const DInputSocket &to_dsocket)
{
  Span<const DOutputSocket *> from_dsockets = to_dsocket.linked_sockets();
  Span<const DGroupInput *> from_group_inputs = to_dsocket.linked_group_inputs();
  int total_linked_amount = from_dsockets.size() + from_group_inputs.size();
  BLI_assert(total_linked_amount <= 1);

  if (total_linked_amount == 0) {
    return nullptr;
  }

  if (from_dsockets.size() == 1) {
    const DOutputSocket &from_dsocket = *from_dsockets[0];
    if (!from_dsocket.is_available()) {
      return nullptr;
    }
    if (socket_is_mf_data_socket(*from_dsocket.bsocket()->typeinfo)) {
      return &common.network_map.lookup(from_dsocket);
    }
    return nullptr;
  }

  const DGroupInput &from_group_input = *from_group_inputs[0];
  if (socket_is_mf_data_socket(*from_group_input.bsocket()->typeinfo)) {
    return &common.network_map.lookup(from_group_input);
  }
  return nullptr;
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
  add_implicit_conversion<float, int32_t>(conversions);
  add_implicit_conversion<float, float3>(conversions);
  add_implicit_conversion<int32_t, float>(conversions);
  add_implicit_conversion<float, bool>(conversions);
  add_implicit_conversion<bool, float>(conversions);
  add_implicit_conversion<float3, float>(
      conversions, "Vector Length", [](float3 a) { return a.length(); });
  add_implicit_conversion<int32_t, float3>(
      conversions, "int32 to float3", [](int32_t a) { return float3((float)a); });
  add_implicit_conversion<float3, Color4f>(
      conversions, "float3 to Color4f", [](float3 a) { return Color4f(a.x, a.y, a.z, 1.0f); });
  add_implicit_conversion<Color4f, float3>(
      conversions, "Color4f to float3", [](Color4f a) { return float3(a.r, a.g, a.b); });
  add_implicit_conversion<float, Color4f>(
      conversions, "float to Color4f", [](float a) { return Color4f(a, a, a, 1.0f); });
  add_implicit_conversion<Color4f, float>(
      conversions, "Color4f to float", [](Color4f a) { return rgb_to_grayscale(a); });
  add_implicit_conversion<float3, bool>(
      conversions, "float3 to boolean", [](float3 a) { return a.length_squared() == 0.0f; });
  add_implicit_conversion<bool, float3>(
      conversions, "boolean to float3", [](bool a) { return (a) ? float3(1.0f) : float3(0.0f); });
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

static void insert_links(CommonMFNetworkBuilderData &common)
{
  for (const DInputSocket *to_dsocket : common.tree.input_sockets()) {
    if (!to_dsocket->is_available()) {
      continue;
    }
    if (!to_dsocket->is_linked()) {
      continue;
    }
    if (!socket_is_mf_data_socket(*to_dsocket->bsocket()->typeinfo)) {
      continue;
    }

    Span<fn::MFInputSocket *> to_sockets = common.network_map.lookup(*to_dsocket);
    BLI_assert(to_sockets.size() >= 1);
    fn::MFDataType to_type = to_sockets[0]->data_type();

    fn::MFOutputSocket *from_socket = try_find_origin(common, *to_dsocket);
    if (from_socket == nullptr) {
      from_socket = &insert_default_value_for_type(common, to_type);
    }

    fn::MFDataType from_type = from_socket->data_type();

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
}

static void insert_unlinked_input(CommonMFNetworkBuilderData &common, const DInputSocket &dsocket)
{
  bNodeSocket *bsocket = dsocket.bsocket();
  bNodeSocketType *socktype = bsocket->typeinfo;
  BLI_assert(socktype->expand_in_mf_network != nullptr);

  SocketMFNetworkBuilder builder{common, dsocket};
  socktype->expand_in_mf_network(builder);

  fn::MFOutputSocket *from_socket = builder.built_socket();
  BLI_assert(from_socket != nullptr);

  for (fn::MFInputSocket *to_socket : common.network_map.lookup(dsocket)) {
    common.network.add_link(*from_socket, *to_socket);
  }
}

static void insert_unlinked_inputs(CommonMFNetworkBuilderData &common)
{
  Vector<const DInputSocket *> unlinked_data_inputs;
  for (const DInputSocket *dsocket : common.tree.input_sockets()) {
    if (dsocket->is_available()) {
      if (socket_is_mf_data_socket(*dsocket->bsocket()->typeinfo)) {
        if (!dsocket->is_linked()) {
          insert_unlinked_input(common, *dsocket);
        }
      }
    }
  }
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
  insert_group_inputs(common);
  insert_links(common);
  insert_unlinked_inputs(common);

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

  for (const DInputSocket *dsocket : dnode.inputs()) {
    if (dsocket->is_available()) {
      for (fn::MFInputSocket *mf_input : network_map.lookup(*dsocket)) {
        check_mf_node(mf_input->node());
      }
    }
  }
  for (const DOutputSocket *dsocket : dnode.outputs()) {
    if (dsocket->is_available()) {
      fn::MFOutputSocket &mf_output = network_map.lookup(*dsocket);
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
  for (const DInputSocket *dsocket : dnode.inputs()) {
    if (dsocket->is_available()) {
      MFDataType data_type = *socket_mf_type_get(*dsocket->typeinfo());
      fn::MFOutputSocket &fn_input = network.add_input(data_type.to_string(), data_type);
      for (fn::MFInputSocket *mf_input : network_map.lookup(*dsocket)) {
        network.add_link(fn_input, *mf_input);
        dummy_fn_inputs.append(&fn_input);
      }
    }
  }
  Vector<const fn::MFInputSocket *> dummy_fn_outputs;
  for (const DOutputSocket *dsocket : dnode.outputs()) {
    if (dsocket->is_available()) {
      fn::MFOutputSocket &mf_output = network_map.lookup(*dsocket);
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

  for (const DNode *dnode : tree.nodes()) {
    const bNodeType *node_type = dnode->typeinfo();
    if (node_type->expand_in_mf_network == nullptr) {
      /* This node does not have a multi-function implementation. */
      continue;
    }

    NodeMFNetworkBuilder builder{common, *dnode};
    node_type->expand_in_mf_network(builder);

    const fn::MultiFunction *single_function = nullptr;
    const NodeExpandType expand_type = get_node_expand_type(network_map, *dnode, &single_function);

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
            *dnode, network, network_map, resources);
        functions_by_node.add_new(dnode, &fn);
        break;
      }
    }
  }

  return functions_by_node;
}

}  // namespace blender::nodes
