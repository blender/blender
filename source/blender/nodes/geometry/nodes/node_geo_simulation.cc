/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.hh"
#include "BLI_string_utf8.hh"

#include "BKE_anonymous_attribute_make.hh"
#include "BKE_attribute_math.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_instances.hh"
#include "BKE_modifier.hh"
#include "BKE_node_tree_zones.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph_query.hh"

#include "UI_interface_layout.hh"

#include "NOD_common.hh"
#include "NOD_geo_bake.hh"
#include "NOD_geo_simulation.hh"
#include "NOD_node_extra_info.hh"
#include "NOD_socket.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"

#include "DNA_mesh_types.h"
#include "DNA_pointcloud_types.h"

#include "ED_node.hh"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "BLT_translation.hh"

#include "GEO_mix_geometries.hh"

#include "WM_api.hh"

#include "BLO_read_write.hh"

#include "node_geometry_util.hh"

namespace blender {

namespace nodes::node_geo_simulation_cc {

static Vector<SocketValueVariant> get_output_values_from_bake_values(
    const Span<NodeSimulationItem> simulation_items,
    const ComputeContext &compute_context,
    bke::bake::BakeDataBlockMap *data_block_map,
    bke::bake::BakeValues &&bake_values)
{
  Vector<bke::bake::BakeValues::OutputKey> keys;
  for (const NodeSimulationItem &item : simulation_items) {
    keys.append({item.identifier, item.socket_type});
  }
  Vector<SocketValueVariant> output_values = bake_values.to_runtime_values(
      keys, compute_context, data_block_map);
  bake_values.clear();
  return output_values;
}

static void draw_simulation_state(const bContext *C,
                                  ui::Layout &layout,
                                  bNodeTree &ntree,
                                  bNode &output_node)
{
  if (ui::Layout *panel = layout.panel(
          C, "simulation_state_items", false, IFACE_("Simulation State")))
  {
    socket_items::ui::draw_items_list_with_operators<SimulationItemsAccessor>(
        C, panel, ntree, output_node);
    auto &storage = *static_cast<NodeGeometrySimulationOutput *>(output_node.storage);
    socket_items::ui::draw_active_item_props<SimulationItemsAccessor>(
        ntree, output_node, [&](PointerRNA *item_ptr) {
          NodeSimulationItem &active_item = storage.items[storage.active_index];
          const eNodeSocketDatatype socket_type = active_item.socket_type;
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          if (socket_type_supports_attributes(socket_type)) {
            panel->prop(item_ptr, "attribute_domain", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          }
        });
  }
}

/** Shared for simulation input and output node. */
static void node_layout_ex(ui::Layout &layout, bContext *C, PointerRNA *current_node_ptr)
{
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(current_node_ptr->owner_id);
  bNode *current_node = static_cast<bNode *>(current_node_ptr->data);

  const bke::bNodeTreeZones *zones = ntree.zones();
  if (!zones) {
    return;
  }
  const bke::bNodeTreeZone *zone = zones->get_zone_by_node(current_node->identifier);
  if (!zone) {
    return;
  }
  if (!zone->output_node_id) {
    return;
  }
  bNode &output_node = const_cast<bNode &>(*zone->output_node());

  BakeDrawContext ctx;
  if (!get_bake_draw_context(C, output_node, ctx)) {
    return;
  }
  layout.active_set(ctx.is_bakeable_in_current_context);

  draw_simulation_state(C, layout, ntree, output_node);

  layout.use_property_split_set(true);
  layout.use_property_decorate_set(false);

  layout.enabled_set(ID_IS_EDITABLE(ctx.object));

  {
    ui::Layout &col = layout.column(false);
    draw_bake_button_row(ctx, col, true);
    if (const std::optional<std::string> bake_state_str = get_bake_state_string(ctx)) {
      ui::Layout &row = col.row(true);
      row.label(*bake_state_str, ICON_NONE);
    }
  }
  draw_common_bake_settings(C, ctx, layout);
  draw_data_blocks(C, layout, ctx.bake_rna);
}

namespace sim_input_node {

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
        "Delta Time", CPPType::get<SocketValueVariant>());

    for (const int i : simulation_items_.index_range()) {
      const NodeSimulationItem &item = simulation_items_[i];
      const bNodeSocket &input_bsocket = node.input_socket(i);
      const bNodeSocket &output_bsocket = node.output_socket(i + 1);

      lf_index_by_bsocket[input_bsocket.index_in_tree()] = inputs_.append_and_get_index_as(
          item.name, CPPType::get<SocketValueVariant>(), lf::ValueUsage::Maybe);
      lf_index_by_bsocket[output_bsocket.index_in_tree()] = outputs_.append_and_get_index_as(
          item.name, CPPType::get<SocketValueVariant>());
    }
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const final
  {
    const GeoNodesUserData &user_data = *static_cast<const GeoNodesUserData *>(context.user_data);
    if (!user_data.call_data->simulation_params) {
      this->set_default_outputs(params);
      return;
    }
    if (!user_data.call_data->self_object()) {
      /* Self object is currently required for creating anonymous attribute names. */
      this->set_default_outputs(params);
      return;
    }
    std::optional<FoundNestedNodeID> found_id = find_nested_node_id(user_data, output_node_id_);
    if (!found_id) {
      this->set_default_outputs(params);
      return;
    }
    if (found_id->is_in_loop || found_id->is_in_closure) {
      this->set_default_outputs(params);
      return;
    }
    SimulationZoneBehavior *zone_behavior = user_data.call_data->simulation_params->get(
        found_id->id);
    if (!zone_behavior) {
      this->set_default_outputs(params);
      return;
    }
    sim_input::Behavior &input_behavior = zone_behavior->input;
    float delta_time = 0.0f;
    if (auto *info = std::get_if<sim_input::UseCache>(&input_behavior)) {
      delta_time = info->delta_time;
      this->output_simulation_state(
          params, user_data, zone_behavior->data_block_map, std::move(info->values));
    }
    else if (std::get_if<sim_input::PassThrough>(&input_behavior)) {
      delta_time = 0.0f;
      this->pass_through(params, user_data, zone_behavior->data_block_map);
    }
    else {
      BLI_assert_unreachable();
    }
    if (!params.output_was_set(0)) {
      params.set_output(0, SocketValueVariant(delta_time));
    }
  }

  void set_default_outputs(lf::Params &params) const
  {
    set_default_remaining_node_outputs(params, node_);
  }

  void output_simulation_state(lf::Params &params,
                               const GeoNodesUserData &user_data,
                               bke::bake::BakeDataBlockMap *data_block_map,
                               bke::bake::BakeValues &&bake_values) const
  {
    Vector<SocketValueVariant> output_values = get_output_values_from_bake_values(
        simulation_items_, *user_data.compute_context, data_block_map, std::move(bake_values));
    for (const int i : simulation_items_.index_range()) {
      params.set_output(i + 1, std::move(output_values[i]));
    }
  }

  void pass_through(lf::Params &params,
                    const GeoNodesUserData &user_data,
                    bke::bake::BakeDataBlockMap *data_block_map) const
  {
    /* Instead of outputting the initial values directly, convert them to a simulation state and
     * then back. This ensures that some geometry processing happens on the data consistently (e.g.
     * removing anonymous attributes). */
    std::optional<bke::bake::BakeValues> bake_values = this->get_bake_values_from_inputs(
        params, data_block_map);
    if (!bake_values) {
      /* Wait for inputs to be computed. */
      return;
    }
    this->output_simulation_state(params, user_data, data_block_map, std::move(*bake_values));
  }

  std::optional<bke::bake::BakeValues> get_bake_values_from_inputs(
      lf::Params &params, bke::bake::BakeDataBlockMap *data_block_map) const
  {
    Array<SocketValueVariant *> input_value_pointers(inputs_.size());
    for (const int i : inputs_.index_range()) {
      input_value_pointers[i] = params.try_get_input_data_ptr_or_request<SocketValueVariant>(i);
    }
    if (input_value_pointers.as_span().contains(nullptr)) {
      /* Wait for inputs to be computed. */
      return std::nullopt;
    }
    Vector<bke::bake::BakeValues::InputValue> input_values(simulation_items_.size());
    for (const int i : inputs_.index_range()) {
      const NodeSimulationItem &item = simulation_items_[i];
      bke::bake::BakeValues::InputValue &input_value = input_values[i];
      input_value.id = item.identifier;
      input_value.name = item.name;
      input_value.field_domain = AttrDomain(item.attribute_domain);
      input_value.value = std::move(*input_value_pointers[i]);
    }
    return bke::bake::BakeValues::from_runtime_values(std::move(input_values), data_block_map);
  }
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_output<decl::Float>("Delta Time"_ustr);

  const bNode *node = b.node_or_null();
  const bNodeTree *node_tree = b.tree_or_null();
  if (ELEM(nullptr, node, node_tree)) {
    return;
  }

  const bNode *output_node = node_tree->node_by_id(node_storage(*node).output_node_id);
  if (!output_node) {
    return;
  }
  const auto &output_storage = *static_cast<const NodeGeometrySimulationOutput *>(
      output_node->storage);

  for (const int i : IndexRange(output_storage.items_num)) {
    const NodeSimulationItem &item = output_storage.items[i];
    const eNodeSocketDatatype socket_type = item.socket_type;
    if (socket_type == SOCK_GEOMETRY && i > 0) {
      b.add_separator();
    }
    const UString name(item.name);
    const UString identifier(SimulationItemsAccessor::socket_identifier_for_item(item));
    auto &input_decl = b.add_input(socket_type, name, identifier)
                           .socket_name_ptr(
                               &node_tree->id, *SimulationItemsAccessor::item_srna, &item, "name")
                           .structure_type(StructureType::Dynamic);
    auto &output_decl = b.add_output(socket_type, name, identifier)
                            .align_with_previous()
                            .propagate_all({input_decl.index()})
                            .inferred_structure_type({input_decl.index()})
                            .structure_type(StructureType::Dynamic);
    if (socket_type == SOCK_BUNDLE) {
      dynamic_cast<decl::BundleBuilder &>(output_decl)
          .pass_through_input_index(input_decl.index());
    }
  }
  b.add_input<decl::Extend>(""_ustr, "__extend__"_ustr)
      .structure_type(StructureType::Dynamic)
      .custom_draw(socket_items::ui::draw_extend_socket_fn<SimulationItemsAccessor>());
  b.add_output<decl::Extend>(""_ustr, "__extend__"_ustr)
      .structure_type(StructureType::Dynamic)
      .align_with_previous();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySimulationInput *data = MEM_new<NodeGeometrySimulationInput>(__func__);
  /* Needs to be initialized for the node to work. */
  data->output_node_id = 0;
  node->storage = data;
}

static void node_label(const bNodeTree * /*ntree*/,
                       const bNode * /*node*/,
                       char *label,
                       const int label_maxncpy)
{
  BLI_strncpy_utf8(label, CTX_IFACE_(BLT_I18NCONTEXT_ID_NODETREE, "Simulation"), label_maxncpy);
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  bNode *output_node = params.ntree.node_by_id(node_storage(params.node).output_node_id);
  if (!output_node) {
    return true;
  }
  return socket_items::try_add_item_via_any_extend_socket<SimulationItemsAccessor>(
      params.ntree, params.node, *output_node, params.link);
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeSimulationInput"_ustr, GEO_NODE_SIMULATION_INPUT);
  ntype.ui_name = "Simulation Input";
  ntype.ui_description = "Input data for the simulation zone";
  ntype.enum_name_legacy = "SIMULATION_INPUT";
  ntype.nclass = NODE_CLASS_INTERFACE;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.labelfunc = node_label;
  ntype.insert_link = node_insert_link;
  ntype.gather_link_search_ops = nullptr;
  ntype.no_muting = true;
  ntype.draw_buttons_ex = node_layout_ex;
  bke::node_type_storage(ntype,
                         "NodeGeometrySimulationInput",
                         node_free_standard_storage,
                         node_copy_standard_storage);
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace sim_input_node

namespace sim_output_node {

NODE_STORAGE_FUNCS(NodeGeometrySimulationOutput);

class LazyFunctionForSimulationOutputNode final : public LazyFunction {
  const bNode &node_;
  Span<NodeSimulationItem> simulation_items_;
  int skip_input_index_;
  /**
   * Start index of the simulation state inputs that are used when the simulation is skipped.
   * Those inputs are linked directly to the simulation input node. Those inputs only exist
   * internally, but not in the UI.
   */
  int skip_inputs_offset_;
  /**
   * Start index of the simulation state inputs that are used when the simulation is actually
   * computed. Those correspond to the sockets that are visible in the UI.
   */
  int solve_inputs_offset_;

 public:
  LazyFunctionForSimulationOutputNode(const bNode &node,
                                      GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info)
      : node_(node)
  {
    debug_name_ = "Simulation Output";
    const NodeGeometrySimulationOutput &storage = node_storage(node);
    simulation_items_ = {storage.items, storage.items_num};

    MutableSpan<int> lf_index_by_bsocket = own_lf_graph_info.mapping.lf_index_by_bsocket;

    const bNodeSocket &skip_bsocket = node.input_socket(0);
    skip_input_index_ = inputs_.append_and_get_index_as(
        "Skip", CPPType::get<SocketValueVariant>(), lf::ValueUsage::Maybe);
    lf_index_by_bsocket[skip_bsocket.index_in_tree()] = skip_input_index_;

    skip_inputs_offset_ = inputs_.size();

    /* Add the skip inputs that are linked to the simulation input node. */
    for (const int i : simulation_items_.index_range()) {
      const NodeSimulationItem &item = simulation_items_[i];
      inputs_.append_as(item.name, CPPType::get<SocketValueVariant>(), lf::ValueUsage::Maybe);
    }

    solve_inputs_offset_ = inputs_.size();

    /* Add the solve inputs that correspond to the simulation state inputs in the UI. */
    for (const int i : simulation_items_.index_range()) {
      const NodeSimulationItem &item = simulation_items_[i];
      const bNodeSocket &input_bsocket = node.input_socket(i + 1);
      const bNodeSocket &output_bsocket = node.output_socket(i);

      lf_index_by_bsocket[input_bsocket.index_in_tree()] = inputs_.append_and_get_index_as(
          item.name, CPPType::get<SocketValueVariant>(), lf::ValueUsage::Maybe);
      lf_index_by_bsocket[output_bsocket.index_in_tree()] = outputs_.append_and_get_index_as(
          item.name, CPPType::get<SocketValueVariant>());
    }
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const final
  {
    GeoNodesUserData &user_data = *static_cast<GeoNodesUserData *>(context.user_data);
    GeoNodesLocalUserData &local_user_data = *static_cast<GeoNodesLocalUserData *>(
        context.local_user_data);
    if (!user_data.call_data->self_object()) {
      /* The self object is currently required for generating anonymous attribute names. */
      this->set_default_outputs(params);
      return;
    }
    if (!user_data.call_data->simulation_params) {
      if (eval_log::NodeTreeLogger *tree_logger = local_user_data.try_get_tree_logger(user_data)) {
        tree_logger->node_warnings.append(
            *tree_logger->allocator,
            {node_.identifier,
             {NodeWarningType::Error, TIP_("Simulation zone is not supported")}});
      }
      this->set_default_outputs(params);
      return;
    }
    std::optional<FoundNestedNodeID> found_id = find_nested_node_id(user_data, node_.identifier);
    if (!found_id) {
      this->set_default_outputs(params);
      return;
    }
    if (found_id->is_in_loop || found_id->is_in_closure) {
      if (eval_log::NodeTreeLogger *tree_logger = local_user_data.try_get_tree_logger(user_data)) {
        const StringRefNull message = TIP_("Simulation must not be in a loop or closure");
        tree_logger->node_warnings.append(*tree_logger->allocator,
                                          {node_.identifier, {NodeWarningType::Error, message}});
      }
      this->set_default_outputs(params);
      return;
    }
    SimulationZoneBehavior *zone_behavior = user_data.call_data->simulation_params->get(
        found_id->id);
    if (!zone_behavior) {
      this->set_default_outputs(params);
      return;
    }
    sim_output::Behavior &output_behavior = zone_behavior->output;
    if (auto *info = std::get_if<sim_output::ReadSingle>(&output_behavior)) {
      this->output_cached_state(
          params, user_data, zone_behavior->data_block_map, std::move(info->values));
    }
    else if (auto *info = std::get_if<sim_output::ReadInterpolated>(&output_behavior)) {
      this->output_mixed_cached_state(params,
                                      zone_behavior->data_block_map,
                                      *user_data.compute_context,
                                      std::move(info->prev_values),
                                      std::move(info->next_values),
                                      info->mix_factor);
    }
    else if (std::get_if<sim_output::PassThrough>(&output_behavior)) {
      this->pass_through(params, user_data, zone_behavior->data_block_map);
    }
    else if (auto *info = std::get_if<sim_output::StoreNewState>(&output_behavior)) {
      this->store_new_state(params, user_data, zone_behavior->data_block_map, *info);
    }
    else {
      BLI_assert_unreachable();
    }
  }

  void set_default_outputs(lf::Params &params) const
  {
    set_default_remaining_node_outputs(params, node_);
  }

  void output_cached_state(lf::Params &params,
                           GeoNodesUserData &user_data,
                           bke::bake::BakeDataBlockMap *data_block_map,
                           bke::bake::BakeValues &&bake_values) const
  {
    Vector<bke::bake::BakeValues::OutputKey> keys;
    for (const NodeSimulationItem &item : simulation_items_) {
      keys.append({item.identifier, item.socket_type});
    }
    Vector<SocketValueVariant> output_values = bake_values.to_runtime_values(
        keys, *user_data.compute_context, data_block_map);
    bake_values.clear();
    for (const int i : simulation_items_.index_range()) {
      params.set_output(i, std::move(output_values[i]));
    }
  }

  void output_mixed_cached_state(lf::Params &params,
                                 bke::bake::BakeDataBlockMap *data_block_map,
                                 const ComputeContext &compute_context,
                                 bke::bake::BakeValues &&prev_bake_values,
                                 bke::bake::BakeValues &&next_bake_values,
                                 const float mix_factor) const
  {
    Vector<SocketValueVariant> output_values = get_output_values_from_bake_values(
        simulation_items_, compute_context, data_block_map, std::move(prev_bake_values));

    Vector<SocketValueVariant> next_values = get_output_values_from_bake_values(
        simulation_items_, compute_context, data_block_map, std::move(next_bake_values));
    for (const int i : simulation_items_.index_range()) {
      geometry::mix_socket_values(output_values[i], next_values[i], mix_factor);
    }
    for (const int i : simulation_items_.index_range()) {
      params.set_output(i, std::move(output_values[i]));
    }
  }

  void pass_through(lf::Params &params,
                    GeoNodesUserData &user_data,
                    bke::bake::BakeDataBlockMap *data_block_map) const
  {
    std::optional<bke::bake::BakeValues> bake_values = this->get_bake_values_from_inputs(
        params, data_block_map, true);
    if (!bake_values) {
      /* Wait for inputs to be computed. */
      return;
    }
    Vector<SocketValueVariant> output_values = get_output_values_from_bake_values(
        simulation_items_, *user_data.compute_context, data_block_map, std::move(*bake_values));
    for (const int i : simulation_items_.index_range()) {
      params.set_output(i, std::move(output_values[i]));
    }
  }

  void store_new_state(lf::Params &params,
                       GeoNodesUserData &user_data,
                       bke::bake::BakeDataBlockMap *data_block_map,
                       const sim_output::StoreNewState &info) const
  {
    const SocketValueVariant *skip_variant =
        params.try_get_input_data_ptr_or_request<SocketValueVariant>(skip_input_index_);
    if (skip_variant == nullptr) {
      /* Wait for skip input to be computed. */
      return;
    }
    const bool skip = skip_variant->get<bool>();

    /* Instead of outputting the values directly, convert them to a bake state and then back.
     * This ensures that some geometry processing happens on the data consistently (e.g. removing
     * anonymous attributes). */
    std::optional<bke::bake::BakeValues> bake_values = this->get_bake_values_from_inputs(
        params, data_block_map, skip);
    if (!bake_values) {
      /* Wait for inputs to be computed. */
      return;
    }
    info.store_fn(*bake_values);
    this->output_cached_state(params, user_data, data_block_map, std::move(*bake_values));
  }

  std::optional<bke::bake::BakeValues> get_bake_values_from_inputs(
      lf::Params &params, bke::bake::BakeDataBlockMap *data_block_map, const bool skip) const
  {
    /* Choose which set of input parameters to use. The others are ignored. */
    const int params_offset = skip ? skip_inputs_offset_ : solve_inputs_offset_;
    Array<SocketValueVariant *> input_value_pointers(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      input_value_pointers[i] = params.try_get_input_data_ptr_or_request<SocketValueVariant>(
          i + params_offset);
    }
    if (input_value_pointers.as_span().contains(nullptr)) {
      /* Wait for inputs to be computed. */
      return std::nullopt;
    }

    Vector<bke::bake::BakeValues::InputValue> input_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      const NodeSimulationItem &item = simulation_items_[i];
      bke::bake::BakeValues::InputValue &input_value = input_values[i];
      input_value.id = item.identifier;
      input_value.name = item.name;
      input_value.field_domain = AttrDomain(item.attribute_domain);
      input_value.value = std::move(*input_value_pointers[i]);
    }
    return bke::bake::BakeValues::from_runtime_values(std::move(input_values), data_block_map);
  }
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Bool>("Skip"_ustr)
      .hide_value()
      .description(
          "Forward the output of the simulation input node directly to the output node and ignore "
          "the nodes in the simulation zone");

  const bNodeTree *tree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (node == nullptr) {
    return;
  }

  const NodeGeometrySimulationOutput &storage = node_storage(*node);

  for (const int i : IndexRange(storage.items_num)) {
    const NodeSimulationItem &item = storage.items[i];
    const eNodeSocketDatatype socket_type = item.socket_type;
    if (socket_type == SOCK_GEOMETRY && i > 0) {
      b.add_separator();
    }
    const UString name(item.name);
    const UString identifier(SimulationItemsAccessor::socket_identifier_for_item(item));
    auto &input_decl = b.add_input(socket_type, name, identifier)
                           .socket_name_ptr(
                               &tree->id, *SimulationItemsAccessor::item_srna, &item, "name")
                           .structure_type(StructureType::Dynamic);
    auto &output_decl = b.add_output(socket_type, name, identifier)
                            .align_with_previous()
                            .propagate_all({input_decl.index()})
                            .inferred_structure_type({input_decl.index()})
                            .structure_type(StructureType::Dynamic);
    if (socket_type == SOCK_BUNDLE) {
      dynamic_cast<decl::BundleBuilder &>(output_decl)
          .pass_through_input_index(input_decl.index());
    }
  }
  b.add_input<decl::Extend>(""_ustr, "__extend__"_ustr)
      .structure_type(StructureType::Dynamic)
      .custom_draw(socket_items::ui::draw_extend_socket_fn<SimulationItemsAccessor>());
  b.add_output<decl::Extend>(""_ustr, "__extend__"_ustr)
      .structure_type(StructureType::Dynamic)
      .align_with_previous();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySimulationOutput *data = MEM_new<NodeGeometrySimulationOutput>(__func__);

  data->next_identifier = 0;

  data->items = MEM_new_array<NodeSimulationItem>(1, __func__);
  data->items[0].name = BLI_strdup(DATA_("Geometry"));
  data->items[0].socket_type = SOCK_GEOMETRY;
  data->items[0].identifier = data->next_identifier++;
  data->items_num = 1;

  node->storage = data;
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<SimulationItemsAccessor>(*node);
  MEM_delete(reinterpret_cast<NodeGeometrySimulationOutput *>(node->storage));
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometrySimulationOutput &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_new<NodeGeometrySimulationOutput>(__func__,
                                                            dna::shallow_copy(src_storage));
  dst_node->storage = dst_storage;

  socket_items::copy_array<SimulationItemsAccessor>(*src_node, *dst_node);
}

static void node_operators()
{
  socket_items::ops::make_common_operators<SimulationItemsAccessor>();
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  return socket_items::try_add_item_via_any_extend_socket<SimulationItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_extra_info(NodeExtraInfoParams &params)
{
  BakeDrawContext ctx;
  if (!get_bake_draw_context(&params.C, params.node, ctx)) {
    return;
  }
  if (!ctx.is_bakeable_in_current_context) {
    NodeExtraInfoRow row;
    row.text = TIP_("Cannot bake in zone");
    row.icon = ICON_ERROR;
    params.rows.append(std::move(row));
  }
  if (ctx.is_baked) {
    NodeExtraInfoRow row;
    row.text = get_baked_string(ctx);
    params.rows.append(std::move(row));
  }
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const bNodeSocket &other_socket = params.other_socket();
  if (!SimulationItemsAccessor::supports_socket_type(other_socket.type, params.node_tree().type)) {
    return;
  }
  params.add_item_full_name(IFACE_("Simulation"), [](LinkSearchOpParams &params) {
    bNode &input_node = params.add_node("GeometryNodeSimulationInput"_ustr);
    bNode &output_node = params.add_node("GeometryNodeSimulationOutput"_ustr);
    output_node.location[0] = 300;

    auto &input_storage = *static_cast<NodeGeometrySimulationInput *>(input_node.storage);
    input_storage.output_node_id = output_node.identifier;

    socket_items::clear<SimulationItemsAccessor>(output_node);
    const UString name(params.socket.name);
    socket_items::add_item_with_socket_type_and_name<SimulationItemsAccessor>(
        params.node_tree, output_node, params.socket.type, name.c_str());
    update_node_declaration_and_sockets(params.node_tree, input_node);
    update_node_declaration_and_sockets(params.node_tree, output_node);
    if (params.socket.in_out == SOCK_IN) {
      params.connect_available_socket(output_node, name);
    }
    else {
      params.connect_available_socket(input_node, name);
    }
    params.node_tree.ensure_topology_cache();
    bke::node_add_link(params.node_tree,
                       input_node,
                       input_node.output_socket(1),
                       output_node,
                       output_node.input_socket(1));
  });
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<SimulationItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<SimulationItemsAccessor>(&reader, node);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSimulationOutput"_ustr, GEO_NODE_SIMULATION_OUTPUT);
  ntype.ui_name = "Simulation Output";
  ntype.ui_description = "Output data from the simulation zone";
  ntype.enum_name_legacy = "SIMULATION_OUTPUT";
  ntype.nclass = NODE_CLASS_INTERFACE;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.labelfunc = sim_input_node::node_label;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.no_muting = true;
  ntype.register_operators = node_operators;
  ntype.get_extra_info = node_extra_info;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  bke::node_type_storage(
      ntype, "NodeGeometrySimulationOutput", node_free_storage, node_copy_storage);
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace sim_output_node

}  // namespace nodes::node_geo_simulation_cc

namespace nodes {

std::unique_ptr<LazyFunction> get_simulation_input_lazy_function(
    const bNodeTree &node_tree,
    const bNode &node,
    GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info)
{
  BLI_assert(node.type_legacy == GEO_NODE_SIMULATION_INPUT);
  return std::make_unique<
      node_geo_simulation_cc::sim_input_node::LazyFunctionForSimulationInputNode>(
      node_tree, node, own_lf_graph_info);
}

std::unique_ptr<LazyFunction> get_simulation_output_lazy_function(
    const bNode &node, GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info)
{
  BLI_assert(node.type_legacy == GEO_NODE_SIMULATION_OUTPUT);
  return std::make_unique<
      node_geo_simulation_cc::sim_output_node::LazyFunctionForSimulationOutputNode>(
      node, own_lf_graph_info);
}

StructRNA **SimulationItemsAccessor::item_srna = &RNA_SimulationStateItem;

void SimulationItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  writer->write_string(item.name);
}

void SimulationItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace nodes

Span<NodeSimulationItem> NodeGeometrySimulationOutput::items_span() const
{
  return Span<NodeSimulationItem>(items, items_num);
}

MutableSpan<NodeSimulationItem> NodeGeometrySimulationOutput::items_span()
{
  return MutableSpan<NodeSimulationItem>(items, items_num);
}

}  // namespace blender
