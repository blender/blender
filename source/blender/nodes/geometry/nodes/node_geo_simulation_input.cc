/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_compute_contexts.hh"
#include "BKE_scene.h"

#include "DEG_depsgraph_query.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_geometry.h"
#include "NOD_socket.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_simulation_input_cc {

NODE_STORAGE_FUNCS(NodeGeometrySimulationInput);

class LazyFunctionForSimulationInputNode final : public LazyFunction {
  const bNode &node_;
  int32_t output_node_id_;
  Span<NodeSimulationItem> simulation_items_;

 public:
  LazyFunctionForSimulationInputNode(const bNodeTree &node_tree,
                                     const bNode &node,
                                     GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info)
      : node_(node)
  {
    debug_name_ = "Simulation Input";
    output_node_id_ = node_storage(node).output_node_id;
    const bNode &output_node = *node_tree.node_by_id(output_node_id_);
    const NodeGeometrySimulationOutput &storage = *static_cast<NodeGeometrySimulationOutput *>(
        output_node.storage);
    simulation_items_ = {storage.items, storage.items_num};

    MutableSpan<int> lf_index_by_bsocket = own_lf_graph_info.mapping.lf_index_by_bsocket;
    lf_index_by_bsocket[node.output_socket(0).index_in_tree()] = outputs_.append_and_get_index_as(
        "Delta Time", CPPType::get<ValueOrField<float>>());

    for (const int i : simulation_items_.index_range()) {
      const NodeSimulationItem &item = simulation_items_[i];
      const bNodeSocket &input_bsocket = node.input_socket(i);
      const bNodeSocket &output_bsocket = node.output_socket(i + 1);

      const CPPType &type = get_simulation_item_cpp_type(item);

      lf_index_by_bsocket[input_bsocket.index_in_tree()] = inputs_.append_and_get_index_as(
          item.name, type, lf::ValueUsage::Maybe);
      lf_index_by_bsocket[output_bsocket.index_in_tree()] = outputs_.append_and_get_index_as(
          item.name, type);
    }
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const final
  {
    const GeoNodesLFUserData &user_data = *static_cast<const GeoNodesLFUserData *>(
        context.user_data);
    const GeoNodesModifierData &modifier_data = *user_data.modifier_data;
    if (modifier_data.current_simulation_state == nullptr) {
      params.set_default_remaining_outputs();
      return;
    }

    if (!params.output_was_set(0)) {
      const float delta_time = modifier_data.simulation_time_delta;
      params.set_output(0, fn::ValueOrField<float>(delta_time));
    }

    const bke::sim::SimulationZoneID zone_id = get_simulation_zone_id(*user_data.compute_context,
                                                                      output_node_id_);

    const bke::sim::SimulationZoneState *prev_zone_state =
        modifier_data.prev_simulation_state == nullptr ?
            nullptr :
            modifier_data.prev_simulation_state->get_zone_state(zone_id);

    std::optional<bke::sim::SimulationZoneState> initial_prev_zone_state;
    if (prev_zone_state == nullptr) {
      Array<void *> input_values(simulation_items_.size(), nullptr);
      for (const int i : simulation_items_.index_range()) {
        input_values[i] = params.try_get_input_data_ptr_or_request(i);
      }
      if (input_values.as_span().contains(nullptr)) {
        /* Wait until all inputs are available. */
        return;
      }

      initial_prev_zone_state.emplace();
      values_to_simulation_state(simulation_items_, input_values, *initial_prev_zone_state);
      prev_zone_state = &*initial_prev_zone_state;
    }

    Array<void *> output_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i + 1);
    }
    simulation_state_to_values(simulation_items_,
                               *prev_zone_state,
                               *modifier_data.self_object,
                               *user_data.compute_context,
                               node_,
                               output_values);
    for (const int i : simulation_items_.index_range()) {
      params.output_set(i + 1);
    }
  }
};

}  // namespace blender::nodes::node_geo_simulation_input_cc

namespace blender::nodes {

std::unique_ptr<LazyFunction> get_simulation_input_lazy_function(
    const bNodeTree &node_tree,
    const bNode &node,
    GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info)
{
  namespace file_ns = blender::nodes::node_geo_simulation_input_cc;
  BLI_assert(node.type == GEO_NODE_SIMULATION_INPUT);
  return std::make_unique<file_ns::LazyFunctionForSimulationInputNode>(
      node_tree, node, own_lf_graph_info);
}

}  // namespace blender::nodes

namespace blender::nodes::node_geo_simulation_input_cc {

static void node_declare_dynamic(const bNodeTree &node_tree,
                                 const bNode &node,
                                 NodeDeclaration &r_declaration)
{
  const bNode *output_node = node_tree.node_by_id(node_storage(node).output_node_id);
  if (!output_node) {
    return;
  }

  std::unique_ptr<decl::Float> delta_time = std::make_unique<decl::Float>();
  delta_time->identifier = "Delta Time";
  delta_time->name = DATA_("Delta Time");
  delta_time->in_out = SOCK_OUT;
  r_declaration.outputs.append(std::move(delta_time));

  const NodeGeometrySimulationOutput &storage = *static_cast<const NodeGeometrySimulationOutput *>(
      output_node->storage);
  socket_declarations_for_simulation_items({storage.items, storage.items_num}, r_declaration);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySimulationInput *data = MEM_cnew<NodeGeometrySimulationInput>(__func__);
  /* Needs to be initialized for the node to work. */
  data->output_node_id = 0;
  node->storage = data;
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  const bNode *output_node = ntree->node_by_id(node_storage(*node).output_node_id);
  if (!output_node) {
    return true;
  }

  NodeGeometrySimulationOutput &storage = *static_cast<NodeGeometrySimulationOutput *>(
      output_node->storage);

  if (link->tonode == node) {
    if (link->tosock->identifier == StringRef("__extend__")) {
      if (const NodeSimulationItem *item = NOD_geometry_simulation_output_add_item_from_socket(
              &storage, link->fromnode, link->fromsock))
      {
        update_node_declaration_and_sockets(*ntree, *node);
        link->tosock = nodeFindSocket(
            node, SOCK_IN, socket_identifier_for_simulation_item(*item).c_str());
      }
      else {
        return false;
      }
    }
  }
  else {
    BLI_assert(link->fromnode == node);
    if (link->fromsock->identifier == StringRef("__extend__")) {
      if (const NodeSimulationItem *item = NOD_geometry_simulation_output_add_item_from_socket(
              &storage, link->tonode, link->tosock))
      {
        update_node_declaration_and_sockets(*ntree, *node);
        link->fromsock = nodeFindSocket(
            node, SOCK_OUT, socket_identifier_for_simulation_item(*item).c_str());
      }
      else {
        return false;
      }
    }
  }
  return true;
}

}  // namespace blender::nodes::node_geo_simulation_input_cc

void register_node_type_geo_simulation_input()
{
  namespace file_ns = blender::nodes::node_geo_simulation_input_cc;

  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_SIMULATION_INPUT, "Simulation Input", NODE_CLASS_INTERFACE);
  ntype.initfunc = file_ns::node_init;
  ntype.declare_dynamic = file_ns::node_declare_dynamic;
  ntype.insert_link = file_ns::node_insert_link;
  ntype.gather_add_node_search_ops = nullptr;
  ntype.gather_link_search_ops = nullptr;
  node_type_storage(&ntype,
                    "NodeGeometrySimulationInput",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  nodeRegisterType(&ntype);
}

bNode *NOD_geometry_simulation_input_get_paired_output(bNodeTree *node_tree,
                                                       const bNode *simulation_input_node)
{
  namespace file_ns = blender::nodes::node_geo_simulation_input_cc;

  const NodeGeometrySimulationInput &data = file_ns::node_storage(*simulation_input_node);
  return node_tree->node_by_id(data.output_node_id);
}

bool NOD_geometry_simulation_input_pair_with_output(const bNodeTree *node_tree,
                                                    bNode *sim_input_node,
                                                    const bNode *sim_output_node)
{
  namespace file_ns = blender::nodes::node_geo_simulation_input_cc;

  BLI_assert(sim_input_node->type == GEO_NODE_SIMULATION_INPUT);
  if (sim_output_node->type != GEO_NODE_SIMULATION_OUTPUT) {
    return false;
  }

  /* Allow only one input paired to an output. */
  for (const bNode *other_input_node : node_tree->nodes_by_type("GeometryNodeSimulationInput")) {
    if (other_input_node != sim_input_node) {
      const NodeGeometrySimulationInput &other_storage = file_ns::node_storage(*other_input_node);
      if (other_storage.output_node_id == sim_output_node->identifier) {
        return false;
      }
    }
  }

  NodeGeometrySimulationInput &storage = file_ns::node_storage(*sim_input_node);
  storage.output_node_id = sim_output_node->identifier;
  return true;
}
