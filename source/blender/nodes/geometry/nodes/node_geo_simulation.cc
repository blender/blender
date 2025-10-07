/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_string.h"
#include "BLI_string_utf8.h"

#include "BKE_anonymous_attribute_make.hh"
#include "BKE_attribute_math.hh"
#include "BKE_bake_items_socket.hh"
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

namespace blender::nodes::node_geo_simulation_cc {

static bke::bake::BakeSocketConfig make_bake_socket_config(
    const Span<NodeSimulationItem> node_simulation_items)
{
  bke::bake::BakeSocketConfig config;
  const int items_num = node_simulation_items.size();
  config.domains.resize(items_num);
  config.names.resize(items_num);
  config.types.resize(items_num);
  config.geometries_by_attribute.resize(items_num);

  int last_geometry_index = -1;
  for (const int item_i : node_simulation_items.index_range()) {
    const NodeSimulationItem &item = node_simulation_items[item_i];
    config.types[item_i] = eNodeSocketDatatype(item.socket_type);
    config.names[item_i] = item.name;
    config.domains[item_i] = AttrDomain(item.attribute_domain);
    if (item.socket_type == SOCK_GEOMETRY) {
      last_geometry_index = item_i;
    }
    else if (last_geometry_index != -1) {
      config.geometries_by_attribute[item_i].append(last_geometry_index);
    }
  }
  return config;
}

static std::shared_ptr<AttributeFieldInput> make_attribute_field(
    const Object &self_object,
    const ComputeContext &compute_context,
    const bNode &node,
    const NodeSimulationItem &item,
    const CPPType &type)
{
  std::string attribute_name = bke::hash_to_anonymous_attribute_name(
      self_object.id.name, compute_context.hash(), node.identifier, item.identifier);
  std::string socket_inspection_name = make_anonymous_attribute_socket_inspection_string(
      node.label_or_name(), item.name);
  return std::make_shared<AttributeFieldInput>(
      std::move(attribute_name), type, std::move(socket_inspection_name));
}

static Vector<SocketValueVariant> move_simulation_state_to_values(
    const Span<NodeSimulationItem> node_simulation_items,
    bke::bake::BakeState zone_state,
    const Object &self_object,
    const ComputeContext &compute_context,
    const bNode &node,
    bke::bake::BakeDataBlockMap *data_block_map)
{
  const bke::bake::BakeSocketConfig config = make_bake_socket_config(node_simulation_items);
  Vector<bke::bake::BakeItem *> bake_items;
  for (const NodeSimulationItem &item : node_simulation_items) {
    std::unique_ptr<bke::bake::BakeItem> *bake_item = zone_state.items_by_id.lookup_ptr(
        item.identifier);
    bake_items.append(bake_item ? bake_item->get() : nullptr);
  }

  return bke::bake::move_bake_items_to_socket_values(
      bake_items, config, data_block_map, [&](const int i, const CPPType &type) {
        return make_attribute_field(
            self_object, compute_context, node, node_simulation_items[i], type);
      });
}

static Vector<SocketValueVariant> copy_simulation_state_to_values(
    const Span<NodeSimulationItem> node_simulation_items,
    const bke::bake::BakeStateRef &zone_state,
    const Object &self_object,
    const ComputeContext &compute_context,
    const bNode &node,
    bke::bake::BakeDataBlockMap *data_block_map)
{
  const bke::bake::BakeSocketConfig config = make_bake_socket_config(node_simulation_items);
  Vector<const bke::bake::BakeItem *> bake_items;
  for (const NodeSimulationItem &item : node_simulation_items) {
    const bke::bake::BakeItem *const *bake_item = zone_state.items_by_id.lookup_ptr(
        item.identifier);
    bake_items.append(bake_item ? *bake_item : nullptr);
  }

  return bke::bake::copy_bake_items_to_socket_values(
      bake_items, config, data_block_map, [&](const int i, const CPPType &type) {
        return make_attribute_field(
            self_object, compute_context, node, node_simulation_items[i], type);
      });
}

static bke::bake::BakeState move_values_to_simulation_state(
    const Span<NodeSimulationItem> node_simulation_items,
    MutableSpan<SocketValueVariant> input_values,
    bke::bake::BakeDataBlockMap *data_block_map)
{
  const bke::bake::BakeSocketConfig config = make_bake_socket_config(node_simulation_items);

  Array<std::unique_ptr<bke::bake::BakeItem>> bake_items =
      bke::bake::move_socket_values_to_bake_items(input_values, config, data_block_map);

  bke::bake::BakeState bake_state;
  for (const int i : node_simulation_items.index_range()) {
    const NodeSimulationItem &item = node_simulation_items[i];
    std::unique_ptr<bke::bake::BakeItem> &bake_item = bake_items[i];
    if (bake_item) {
      bake_state.items_by_id.add_new(item.identifier, std::move(bake_item));
    }
  }
  return bake_state;
}

static void draw_simulation_state(const bContext *C,
                                  uiLayout *layout,
                                  bNodeTree &ntree,
                                  bNode &output_node)
{
  if (uiLayout *panel = layout->panel(
          C, "simulation_state_items", false, IFACE_("Simulation State")))
  {
    socket_items::ui::draw_items_list_with_operators<SimulationItemsAccessor>(
        C, panel, ntree, output_node);
    auto &storage = *static_cast<NodeGeometrySimulationOutput *>(output_node.storage);
    socket_items::ui::draw_active_item_props<SimulationItemsAccessor>(
        ntree, output_node, [&](PointerRNA *item_ptr) {
          NodeSimulationItem &active_item = storage.items[storage.active_index];
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          if (socket_type_supports_fields(eNodeSocketDatatype(active_item.socket_type))) {
            panel->prop(item_ptr, "attribute_domain", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          }
        });
  }
}

/** Shared for simulation input and output node. */
static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *current_node_ptr)
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
  layout->active_set(ctx.is_bakeable_in_current_context);

  draw_simulation_state(C, layout, ntree, output_node);

  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  layout->enabled_set(ID_IS_EDITABLE(ctx.object));

  {
    uiLayout *col = &layout->column(false);
    draw_bake_button_row(ctx, col, true);
    if (const std::optional<std::string> bake_state_str = get_bake_state_string(ctx)) {
      uiLayout *row = &col->row(true);
      row->label(*bake_state_str, ICON_NONE);
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
    if (auto *info = std::get_if<sim_input::OutputCopy>(&input_behavior)) {
      delta_time = info->delta_time;
      this->output_simulation_state_copy(
          params, user_data, zone_behavior->data_block_map, info->state);
    }
    else if (auto *info = std::get_if<sim_input::OutputMove>(&input_behavior)) {
      delta_time = info->delta_time;
      this->output_simulation_state_move(
          params, user_data, zone_behavior->data_block_map, std::move(info->state));
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

  void output_simulation_state_copy(lf::Params &params,
                                    const GeoNodesUserData &user_data,
                                    bke::bake::BakeDataBlockMap *data_block_map,
                                    const bke::bake::BakeStateRef &zone_state) const
  {
    Vector<SocketValueVariant> output_values = copy_simulation_state_to_values(
        simulation_items_,
        zone_state,
        *user_data.call_data->self_object(),
        *user_data.compute_context,
        node_,
        data_block_map);
    for (const int i : simulation_items_.index_range()) {
      params.set_output(i + 1, std::move(output_values[i]));
    }
  }

  void output_simulation_state_move(lf::Params &params,
                                    const GeoNodesUserData &user_data,
                                    bke::bake::BakeDataBlockMap *data_block_map,
                                    bke::bake::BakeState zone_state) const
  {
    Vector<SocketValueVariant> output_values = move_simulation_state_to_values(
        simulation_items_,
        std::move(zone_state),
        *user_data.call_data->self_object(),
        *user_data.compute_context,
        node_,
        data_block_map);
    for (const int i : simulation_items_.index_range()) {
      params.set_output(i + 1, std::move(output_values[i]));
    }
  }

  void pass_through(lf::Params &params,
                    const GeoNodesUserData &user_data,
                    bke::bake::BakeDataBlockMap *data_block_map) const
  {
    Array<SocketValueVariant *> input_value_pointers(inputs_.size());
    for (const int i : inputs_.index_range()) {
      input_value_pointers[i] = params.try_get_input_data_ptr_or_request<SocketValueVariant>(i);
    }
    if (input_value_pointers.as_span().contains(nullptr)) {
      /* Wait for inputs to be computed. */
      return;
    }
    Array<SocketValueVariant> input_values(inputs_.size());
    for (const int i : inputs_.index_range()) {
      input_values[i] = std::move(*input_value_pointers[i]);
    }

    /* Instead of outputting the initial values directly, convert them to a simulation state and
     * then back. This ensures that some geometry processing happens on the data consistently (e.g.
     * removing anonymous attributes). */
    bke::bake::BakeState bake_state = move_values_to_simulation_state(
        simulation_items_, input_values, data_block_map);
    this->output_simulation_state_move(params, user_data, data_block_map, std::move(bake_state));
  }
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_output<decl::Float>("Delta Time");

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
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
    if (socket_type == SOCK_GEOMETRY && i > 0) {
      b.add_separator();
    }
    const StringRef name = item.name;
    const std::string identifier = SimulationItemsAccessor::socket_identifier_for_item(item);
    auto &input_decl = b.add_input(socket_type, name, identifier)
                           .socket_name_ptr(
                               &node_tree->id, SimulationItemsAccessor::item_srna, &item, "name");
    auto &output_decl = b.add_output(socket_type, name, identifier).align_with_previous();
    if (socket_type_supports_fields(socket_type)) {
      /* If it's below a geometry input it may be a field evaluated on that geometry. */
      input_decl.supports_field().structure_type(StructureType::Dynamic);
      output_decl.dependent_field({input_decl.index()});
    }
  }
  b.add_input<decl::Extend>("", "__extend__").structure_type(StructureType::Dynamic);
  b.add_output<decl::Extend>("", "__extend__")
      .structure_type(StructureType::Dynamic)
      .align_with_previous();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySimulationInput *data = MEM_callocN<NodeGeometrySimulationInput>(__func__);
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
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeSimulationInput", GEO_NODE_SIMULATION_INPUT);
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
  blender::bke::node_type_storage(ntype,
                                  "NodeGeometrySimulationInput",
                                  node_free_standard_storage,
                                  node_copy_standard_storage);
  blender::bke::node_register_type(ntype);
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
      if (geo_eval_log::GeoTreeLogger *tree_logger = local_user_data.try_get_tree_logger(
              user_data))
      {
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
      if (geo_eval_log::GeoTreeLogger *tree_logger = local_user_data.try_get_tree_logger(
              user_data))
      {
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
      this->output_cached_state(params, user_data, zone_behavior->data_block_map, info->state);
    }
    else if (auto *info = std::get_if<sim_output::ReadInterpolated>(&output_behavior)) {
      this->output_mixed_cached_state(params,
                                      zone_behavior->data_block_map,
                                      *user_data.call_data->self_object(),
                                      *user_data.compute_context,
                                      info->prev_state,
                                      info->next_state,
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
                           const bke::bake::BakeStateRef &state) const
  {
    Vector<SocketValueVariant> output_values = copy_simulation_state_to_values(
        simulation_items_,
        state,
        *user_data.call_data->self_object(),
        *user_data.compute_context,
        node_,
        data_block_map);
    for (const int i : simulation_items_.index_range()) {
      params.set_output(i, std::move(output_values[i]));
    }
  }

  void output_mixed_cached_state(lf::Params &params,
                                 bke::bake::BakeDataBlockMap *data_block_map,
                                 const Object &self_object,
                                 const ComputeContext &compute_context,
                                 const bke::bake::BakeStateRef &prev_state,
                                 const bke::bake::BakeStateRef &next_state,
                                 const float mix_factor) const
  {
    Vector<SocketValueVariant> output_values = copy_simulation_state_to_values(
        simulation_items_, prev_state, self_object, compute_context, node_, data_block_map);

    Vector<SocketValueVariant> next_values = copy_simulation_state_to_values(
        simulation_items_, next_state, self_object, compute_context, node_, data_block_map);
    for (const int i : simulation_items_.index_range()) {
      mix_baked_data_item(eNodeSocketDatatype(simulation_items_[i].socket_type),
                          output_values[i],
                          next_values[i],
                          mix_factor);
    }
    for (const int i : simulation_items_.index_range()) {
      params.set_output(i, std::move(output_values[i]));
    }
  }

  void pass_through(lf::Params &params,
                    GeoNodesUserData &user_data,
                    bke::bake::BakeDataBlockMap *data_block_map) const
  {
    std::optional<bke::bake::BakeState> bake_state = this->get_bake_state_from_inputs(
        params, data_block_map, true);
    if (!bake_state) {
      /* Wait for inputs to be computed. */
      return;
    }
    Vector<SocketValueVariant> output_values = move_simulation_state_to_values(
        simulation_items_,
        std::move(*bake_state),
        *user_data.call_data->self_object(),
        *user_data.compute_context,
        node_,
        data_block_map);
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
    std::optional<bke::bake::BakeState> bake_state = this->get_bake_state_from_inputs(
        params, data_block_map, skip);
    if (!bake_state) {
      /* Wait for inputs to be computed. */
      return;
    }
    this->output_cached_state(params, user_data, data_block_map, *bake_state);
    info.store_fn(std::move(*bake_state));
  }

  std::optional<bke::bake::BakeState> get_bake_state_from_inputs(
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

    Array<SocketValueVariant> input_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      input_values[i] = std::move(*input_value_pointers[i]);
    }

    return move_values_to_simulation_state(simulation_items_, input_values, data_block_map);
  }
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Bool>("Skip").hide_value().description(
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
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
    if (socket_type == SOCK_GEOMETRY && i > 0) {
      b.add_separator();
    }
    const StringRef name = item.name;
    const std::string identifier = SimulationItemsAccessor::socket_identifier_for_item(item);
    auto &input_decl = b.add_input(socket_type, name, identifier)
                           .socket_name_ptr(
                               &tree->id, SimulationItemsAccessor::item_srna, &item, "name");
    auto &output_decl = b.add_output(socket_type, name, identifier).align_with_previous();
    if (socket_type_supports_fields(socket_type)) {
      /* If it's below a geometry input it may be a field evaluated on that geometry. */
      input_decl.supports_field().structure_type(StructureType::Dynamic);
      output_decl.dependent_field({input_decl.index()});
    }
  }
  b.add_input<decl::Extend>("", "__extend__").structure_type(StructureType::Dynamic);
  b.add_output<decl::Extend>("", "__extend__")
      .structure_type(StructureType::Dynamic)
      .align_with_previous();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySimulationOutput *data = MEM_callocN<NodeGeometrySimulationOutput>(__func__);

  data->next_identifier = 0;

  data->items = MEM_calloc_arrayN<NodeSimulationItem>(1, __func__);
  data->items[0].name = BLI_strdup(DATA_("Geometry"));
  data->items[0].socket_type = SOCK_GEOMETRY;
  data->items[0].identifier = data->next_identifier++;
  data->items_num = 1;

  node->storage = data;
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<SimulationItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometrySimulationOutput &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<NodeGeometrySimulationOutput>(__func__, src_storage);
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
  if (!SimulationItemsAccessor::supports_socket_type(eNodeSocketDatatype(other_socket.type),
                                                     params.node_tree().type))
  {
    return;
  }
  params.add_item_full_name(IFACE_("Simulation"), [](LinkSearchOpParams &params) {
    bNode &input_node = params.add_node("GeometryNodeSimulationInput");
    bNode &output_node = params.add_node("GeometryNodeSimulationOutput");
    output_node.location[0] = 300;

    auto &input_storage = *static_cast<NodeGeometrySimulationInput *>(input_node.storage);
    input_storage.output_node_id = output_node.identifier;

    socket_items::clear<SimulationItemsAccessor>(output_node);
    socket_items::add_item_with_socket_type_and_name<SimulationItemsAccessor>(
        params.node_tree,
        output_node,
        eNodeSocketDatatype(params.socket.type),
        params.socket.name);
    update_node_declaration_and_sockets(params.node_tree, input_node);
    update_node_declaration_and_sockets(params.node_tree, output_node);
    if (params.socket.in_out == SOCK_IN) {
      params.connect_available_socket(output_node, params.socket.name);
    }
    else {
      params.connect_available_socket(input_node, params.socket.name);
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
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSimulationOutput", GEO_NODE_SIMULATION_OUTPUT);
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
  blender::bke::node_type_storage(
      ntype, "NodeGeometrySimulationOutput", node_free_storage, node_copy_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace sim_output_node

}  // namespace blender::nodes::node_geo_simulation_cc

namespace blender::nodes {

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

void mix_baked_data_item(const eNodeSocketDatatype socket_type,
                         SocketValueVariant &prev,
                         const SocketValueVariant &next,
                         const float factor)
{
  switch (socket_type) {
    case SOCK_GEOMETRY: {
      GeometrySet &prev_geo = *prev.get_single_ptr().get<GeometrySet>();
      const GeometrySet &next_geo = *next.get_single_ptr().get<GeometrySet>();
      prev_geo = geometry::mix_geometries(std::move(prev_geo), next_geo, factor);
      break;
    }
    case SOCK_FLOAT:
    case SOCK_VECTOR:
    case SOCK_INT:
    case SOCK_BOOLEAN:
    case SOCK_ROTATION:
    case SOCK_RGBA:
    case SOCK_MATRIX: {
      const CPPType &type = *bke::socket_type_to_geo_nodes_base_cpp_type(socket_type);
      if (!prev.is_single() || !next.is_single()) {
        /* Fields are evaluated on geometries and are mixed there. */
        break;
      }

      prev.convert_to_single();

      SocketValueVariant next_copy = next;
      next_copy.convert_to_single();

      void *prev_value = prev.get_single_ptr().get();
      const void *next_value = next_copy.get_single_ptr().get();

      bke::attribute_math::convert_to_static_type(type, [&](auto dummy) {
        using T = decltype(dummy);
        *static_cast<T *>(prev_value) = bke::attribute_math::mix2(
            factor, *static_cast<T *>(prev_value), *static_cast<const T *>(next_value));
      });
      break;
    }
    default:
      break;
  }
}

StructRNA *SimulationItemsAccessor::item_srna = &RNA_SimulationStateItem;

void SimulationItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void SimulationItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace blender::nodes

blender::Span<NodeSimulationItem> NodeGeometrySimulationOutput::items_span() const
{
  return blender::Span<NodeSimulationItem>(items, items_num);
}

blender::MutableSpan<NodeSimulationItem> NodeGeometrySimulationOutput::items_span()
{
  return blender::MutableSpan<NodeSimulationItem>(items, items_num);
}
