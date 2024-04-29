/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_math_matrix.hh"
#include "BLI_string.h"
#include "BLI_string_utf8.h"
#include "BLI_string_utils.hh"
#include "BLI_task.hh"

#include "BKE_attribute_math.hh"
#include "BKE_bake_geometry_nodes_modifier.hh"
#include "BKE_bake_items_socket.hh"
#include "BKE_compute_contexts.hh"
#include "BKE_context.hh"
#include "BKE_curves.hh"
#include "BKE_instances.hh"
#include "BKE_modifier.hh"
#include "BKE_node_socket_value.hh"
#include "BKE_node_tree_update.hh"
#include "BKE_node_tree_zones.hh"
#include "BKE_object.hh"
#include "BKE_scene.hh"
#include "BKE_screen.hh"

#include "DEG_depsgraph_query.hh"

#include "UI_interface.hh"

#include "NOD_common.h"
#include "NOD_geo_simulation.hh"
#include "NOD_geometry.hh"
#include "NOD_socket.hh"
#include "NOD_socket_items_ops.hh"

#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"
#include "DNA_modifier_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_space_types.h"

#include "ED_node.hh"

#include "RNA_access.hh"
#include "RNA_define.hh"
#include "RNA_prototypes.h"

#include "MOD_nodes.hh"

#include "BLT_translation.hh"

#include "GEO_mix_geometries.hh"

#include "WM_api.hh"

#include "BLO_read_write.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_simulation_cc {

static const CPPType &get_simulation_item_cpp_type(const eNodeSocketDatatype socket_type)
{
  const char *socket_idname = nodeStaticSocketType(socket_type, 0);
  const bNodeSocketType *typeinfo = nodeSocketTypeFind(socket_idname);
  BLI_assert(typeinfo);
  BLI_assert(typeinfo->geometry_nodes_cpp_type);
  return *typeinfo->geometry_nodes_cpp_type;
}

static const CPPType &get_simulation_item_cpp_type(const NodeSimulationItem &item)
{
  return get_simulation_item_cpp_type(eNodeSocketDatatype(item.socket_type));
}

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

static std::shared_ptr<AnonymousAttributeFieldInput> make_attribute_field(
    const Object &self_object,
    const ComputeContext &compute_context,
    const bNode &node,
    const NodeSimulationItem &item,
    const CPPType &type)
{
  AnonymousAttributeIDPtr attribute_id = AnonymousAttributeIDPtr(MEM_new<NodeAnonymousAttributeID>(
      __func__, self_object, compute_context, node, std::to_string(item.identifier), item.name));
  return std::make_shared<AnonymousAttributeFieldInput>(attribute_id, type, node.label_or_name());
}

static void move_simulation_state_to_values(const Span<NodeSimulationItem> node_simulation_items,
                                            bke::bake::BakeState zone_state,
                                            const Object &self_object,
                                            const ComputeContext &compute_context,
                                            const bNode &node,
                                            bke::bake::BakeDataBlockMap *data_block_map,
                                            Span<void *> r_output_values)
{
  const bke::bake::BakeSocketConfig config = make_bake_socket_config(node_simulation_items);
  Vector<bke::bake::BakeItem *> bake_items;
  for (const NodeSimulationItem &item : node_simulation_items) {
    std::unique_ptr<bke::bake::BakeItem> *bake_item = zone_state.items_by_id.lookup_ptr(
        item.identifier);
    bake_items.append(bake_item ? bake_item->get() : nullptr);
  }

  bke::bake::move_bake_items_to_socket_values(
      bake_items,
      config,
      data_block_map,
      [&](const int i, const CPPType &type) {
        return make_attribute_field(
            self_object, compute_context, node, node_simulation_items[i], type);
      },
      r_output_values);
}

static void copy_simulation_state_to_values(const Span<NodeSimulationItem> node_simulation_items,
                                            const bke::bake::BakeStateRef &zone_state,
                                            const Object &self_object,
                                            const ComputeContext &compute_context,
                                            const bNode &node,
                                            bke::bake::BakeDataBlockMap *data_block_map,
                                            Span<void *> r_output_values)
{
  const bke::bake::BakeSocketConfig config = make_bake_socket_config(node_simulation_items);
  Vector<const bke::bake::BakeItem *> bake_items;
  for (const NodeSimulationItem &item : node_simulation_items) {
    const bke::bake::BakeItem *const *bake_item = zone_state.items_by_id.lookup_ptr(
        item.identifier);
    bake_items.append(bake_item ? *bake_item : nullptr);
  }

  bke::bake::copy_bake_items_to_socket_values(
      bake_items,
      config,
      data_block_map,
      [&](const int i, const CPPType &type) {
        return make_attribute_field(
            self_object, compute_context, node, node_simulation_items[i], type);
      },
      r_output_values);
}

static bke::bake::BakeState move_values_to_simulation_state(
    const Span<NodeSimulationItem> node_simulation_items,
    const Span<void *> input_values,
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

static void draw_simulation_state_item(uiList * /*ui_list*/,
                                       const bContext *C,
                                       uiLayout *layout,
                                       PointerRNA * /*idataptr*/,
                                       PointerRNA *itemptr,
                                       int /*icon*/,
                                       PointerRNA * /*active_dataptr*/,
                                       const char * /*active_propname*/,
                                       int /*index*/,
                                       int /*flt_flag*/)
{
  uiLayout *row = uiLayoutRow(layout, true);
  float4 color;
  RNA_float_get_array(itemptr, "color", color);
  uiTemplateNodeSocket(row, const_cast<bContext *>(C), color);
  uiLayoutSetEmboss(row, UI_EMBOSS_NONE);
  uiItemR(row, itemptr, "name", UI_ITEM_NONE, "", ICON_NONE);
}

static void draw_simulation_state(const bContext *C,
                                  uiLayout *layout,
                                  bNodeTree &ntree,
                                  bNode &output_node)
{
  static const uiListType *state_items_list = []() {
    uiListType *list = MEM_cnew<uiListType>(__func__);
    STRNCPY(list->idname, "DATA_UL_simulation_zone_state");
    list->draw_item = draw_simulation_state_item;
    WM_uilisttype_add(list);
    return list;
  }();

  PointerRNA output_node_ptr = RNA_pointer_create(&ntree.id, &RNA_Node, &output_node);

  if (uiLayout *panel = uiLayoutPanel(
          C, layout, "simulation_state_items", false, TIP_("Simulation State")))
  {
    uiLayout *row = uiLayoutRow(panel, false);
    uiTemplateList(row,
                   C,
                   state_items_list->idname,
                   "",
                   &output_node_ptr,
                   "state_items",
                   &output_node_ptr,
                   "active_index",
                   nullptr,
                   3,
                   5,
                   UILST_LAYOUT_DEFAULT,
                   0,
                   UI_TEMPLATE_LIST_FLAG_NONE);

    {
      uiLayout *ops_col = uiLayoutColumn(row, false);
      {
        uiLayout *add_remove_col = uiLayoutColumn(ops_col, true);
        uiItemO(add_remove_col, "", ICON_ADD, "node.simulation_zone_item_add");
        uiItemO(add_remove_col, "", ICON_REMOVE, "node.simulation_zone_item_remove");
      }
      {
        uiLayout *up_down_col = uiLayoutColumn(ops_col, true);
        uiItemEnumO(
            up_down_col, "node.simulation_zone_item_move", "", ICON_TRIA_UP, "direction", 0);
        uiItemEnumO(
            up_down_col, "node.simulation_zone_item_move", "", ICON_TRIA_DOWN, "direction", 1);
      }
    }

    auto &storage = *static_cast<NodeGeometrySimulationOutput *>(output_node.storage);
    if (storage.active_index >= 0 && storage.active_index < storage.items_num) {
      NodeSimulationItem &active_item = storage.items[storage.active_index];
      PointerRNA item_ptr = RNA_pointer_create(
          output_node_ptr.owner_id, SimulationItemsAccessor::item_srna, &active_item);
      uiLayoutSetPropSep(panel, true);
      uiLayoutSetPropDecorate(panel, false);
      uiItemR(panel, &item_ptr, "socket_type", UI_ITEM_NONE, nullptr, ICON_NONE);
      if (socket_type_supports_fields(eNodeSocketDatatype(active_item.socket_type))) {
        uiItemR(panel, &item_ptr, "attribute_domain", UI_ITEM_NONE, nullptr, ICON_NONE);
      }
    }
  }
}

static void NODE_OT_simulation_zone_item_remove(wmOperatorType *ot)
{
  socket_items::ops::remove_active_item<SimulationItemsAccessor>(
      ot, "Remove Simulation Zone Item", __func__, "Remove active simulation zone item");
}

static void NODE_OT_simulation_zone_item_add(wmOperatorType *ot)
{
  socket_items::ops::add_item<SimulationItemsAccessor>(
      ot, "Add Simulation Zone Item", __func__, "Add simulation zone item");
}

static void NODE_OT_simulation_zone_item_move(wmOperatorType *ot)
{
  socket_items::ops::move_active_item<SimulationItemsAccessor>(
      ot, "Move Simulation Zone Item", __func__, "Move active simulation zone item");
}

/** Shared for simulation input and output node. */
static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *current_node_ptr)
{
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(current_node_ptr->owner_id);
  bNode *current_node = static_cast<bNode *>(current_node_ptr->data);

  Scene *scene = CTX_data_scene(C);
  SpaceNode *snode = CTX_wm_space_node(C);

  const bke::bNodeTreeZones *zones = ntree.zones();
  if (!zones) {
    return;
  }
  const bke::bNodeTreeZone *zone = zones->get_zone_by_node(current_node->identifier);
  if (!zone) {
    return;
  }
  if (!zone->output_node) {
    return;
  }
  bNode &output_node = const_cast<bNode &>(*zone->output_node);

  draw_simulation_state(C, layout, ntree, output_node);

  if (snode == nullptr) {
    return;
  }
  std::optional<ed::space_node::ObjectAndModifier> object_and_modifier =
      ed::space_node::get_modifier_for_node_editor(*snode);
  if (!object_and_modifier.has_value()) {
    return;
  }
  const Object &object = *object_and_modifier->object;
  const NodesModifierData &nmd = *object_and_modifier->nmd;
  const std::optional<int32_t> bake_id = ed::space_node::find_nested_node_id_in_root(*snode,
                                                                                     output_node);
  if (!bake_id.has_value()) {
    return;
  }
  const NodesModifierBake *bake = nullptr;
  for (const NodesModifierBake &iter_bake : Span{nmd.bakes, nmd.bakes_num}) {
    if (iter_bake.id == *bake_id) {
      bake = &iter_bake;
      break;
    }
  }
  if (bake == nullptr) {
    return;
  }

  PointerRNA bake_rna = RNA_pointer_create(
      const_cast<ID *>(&object.id), &RNA_NodesModifierBake, (void *)bake);

  const std::optional<IndexRange> simulation_range = bke::bake::get_node_bake_frame_range(
      *scene, object, nmd, *bake_id);

  std::optional<IndexRange> baked_range;
  if (nmd.runtime->cache) {
    const bke::bake::ModifierCache &cache = *nmd.runtime->cache;
    std::lock_guard lock{cache.mutex};
    if (const std::unique_ptr<bke::bake::SimulationNodeCache> *node_cache_ptr =
            cache.simulation_cache_by_id.lookup_ptr(*bake_id))
    {
      const bke::bake::SimulationNodeCache &node_cache = **node_cache_ptr;
      if (node_cache.cache_status == bke::bake::CacheStatus::Baked &&
          !node_cache.bake.frames.is_empty())
      {
        const int first_frame = node_cache.bake.frames.first()->frame.frame();
        const int last_frame = node_cache.bake.frames.last()->frame.frame();
        baked_range = IndexRange(first_frame, last_frame - first_frame + 1);
      }
    }
  }
  bool is_baked = baked_range.has_value();

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);

  uiLayoutSetEnabled(layout, !ID_IS_LINKED(&object));

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    uiLayout *row = uiLayoutRow(col, true);
    {
      char bake_label[1024] = N_("Bake");

      PointerRNA ptr;
      uiItemFullO(row,
                  "OBJECT_OT_geometry_node_bake_single",
                  bake_label,
                  ICON_NONE,
                  nullptr,
                  WM_OP_INVOKE_DEFAULT,
                  UI_ITEM_NONE,
                  &ptr);
      WM_operator_properties_id_lookup_set_from_id(&ptr, &object.id);
      RNA_string_set(&ptr, "modifier_name", nmd.modifier.name);
      RNA_int_set(&ptr, "bake_id", bake->id);
    }
    {
      PointerRNA ptr;
      uiItemFullO(row,
                  "OBJECT_OT_geometry_node_bake_delete_single",
                  "",
                  ICON_TRASH,
                  nullptr,
                  WM_OP_INVOKE_DEFAULT,
                  UI_ITEM_NONE,
                  &ptr);
      WM_operator_properties_id_lookup_set_from_id(&ptr, &object.id);
      RNA_string_set(&ptr, "modifier_name", nmd.modifier.name);
      RNA_int_set(&ptr, "bake_id", bake->id);
    }
    if (is_baked) {
      char baked_range_label[64];
      SNPRINTF(baked_range_label,
               N_("Baked %d - %d"),
               int(baked_range->first()),
               int(baked_range->last()));
      uiItemL(layout, baked_range_label, ICON_NONE);
    }
    else if (simulation_range.has_value()) {
      char simulation_range_label[64];
      SNPRINTF(simulation_range_label,
               N_("Frames %d - %d"),
               int(simulation_range->first()),
               int(simulation_range->last()));
      uiItemL(layout, simulation_range_label, ICON_NONE);
    }
  }
  {
    uiLayout *settings_col = uiLayoutColumn(layout, false);
    uiLayoutSetActive(settings_col, !is_baked);
    {
      uiLayout *col = uiLayoutColumn(settings_col, true);
      uiLayoutSetActive(col, !is_baked);
      uiItemR(col, &bake_rna, "use_custom_path", UI_ITEM_NONE, "Custom Path", ICON_NONE);
      uiLayout *subcol = uiLayoutColumn(col, true);
      uiLayoutSetActive(subcol, bake->flag & NODES_MODIFIER_BAKE_CUSTOM_PATH);
      uiItemR(subcol, &bake_rna, "directory", UI_ITEM_NONE, "Path", ICON_NONE);
    }
    {
      uiLayout *col = uiLayoutColumn(settings_col, true);
      uiItemR(col,
              &bake_rna,
              "use_custom_simulation_frame_range",
              UI_ITEM_NONE,
              "Custom Range",
              ICON_NONE);
      uiLayout *subcol = uiLayoutColumn(col, true);
      uiLayoutSetActive(subcol, bake->flag & NODES_MODIFIER_BAKE_CUSTOM_SIMULATION_FRAME_RANGE);
      uiItemR(subcol, &bake_rna, "frame_start", UI_ITEM_NONE, "Start", ICON_NONE);
      uiItemR(subcol, &bake_rna, "frame_end", UI_ITEM_NONE, "End", ICON_NONE);
    }
  }

  draw_data_blocks(C, layout, bake_rna);
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
    if (found_id->is_in_loop) {
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
                                    const GeoNodesLFUserData &user_data,
                                    bke::bake::BakeDataBlockMap *data_block_map,
                                    const bke::bake::BakeStateRef &zone_state) const
  {
    Array<void *> outputs(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      outputs[i] = params.get_output_data_ptr(i + 1);
    }
    copy_simulation_state_to_values(simulation_items_,
                                    zone_state,
                                    *user_data.call_data->self_object(),
                                    *user_data.compute_context,
                                    node_,
                                    data_block_map,
                                    outputs);
    for (const int i : simulation_items_.index_range()) {
      params.output_set(i + 1);
    }
  }

  void output_simulation_state_move(lf::Params &params,
                                    const GeoNodesLFUserData &user_data,
                                    bke::bake::BakeDataBlockMap *data_block_map,
                                    bke::bake::BakeState zone_state) const
  {
    Array<void *> outputs(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      outputs[i] = params.get_output_data_ptr(i + 1);
    }
    move_simulation_state_to_values(simulation_items_,
                                    std::move(zone_state),
                                    *user_data.call_data->self_object(),
                                    *user_data.compute_context,
                                    node_,
                                    data_block_map,
                                    outputs);
    for (const int i : simulation_items_.index_range()) {
      params.output_set(i + 1);
    }
  }

  void pass_through(lf::Params &params,
                    const GeoNodesLFUserData &user_data,
                    bke::bake::BakeDataBlockMap *data_block_map) const
  {
    Array<void *> input_values(inputs_.size());
    for (const int i : inputs_.index_range()) {
      input_values[i] = params.try_get_input_data_ptr_or_request(i);
    }
    if (input_values.as_span().contains(nullptr)) {
      /* Wait for inputs to be computed. */
      return;
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
    const StringRef name = item.name;
    const std::string identifier = SimulationItemsAccessor::socket_identifier_for_item(item);
    auto &input_decl = b.add_input(socket_type, name, identifier);
    auto &output_decl = b.add_output(socket_type, name, identifier).align_with_previous();
    if (socket_type_supports_fields(socket_type)) {
      input_decl.supports_field();
      output_decl.dependent_field({input_decl.index()});
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Extend>("", "__extend__").align_with_previous();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySimulationInput *data = MEM_cnew<NodeGeometrySimulationInput>(__func__);
  /* Needs to be initialized for the node to work. */
  data->output_node_id = 0;
  node->storage = data;
}

static void node_label(const bNodeTree * /*ntree*/,
                       const bNode * /*node*/,
                       char *label,
                       const int label_maxncpy)
{
  BLI_strncpy_utf8(label, IFACE_("Simulation"), label_maxncpy);
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  bNode *output_node = ntree->node_by_id(node_storage(*node).output_node_id);
  if (!output_node) {
    return true;
  }
  return socket_items::try_add_item_via_any_extend_socket<SimulationItemsAccessor>(
      *ntree, *node, *output_node, *link);
}

static void node_register()
{
  static bNodeType ntype;
  geo_node_type_base(&ntype, GEO_NODE_SIMULATION_INPUT, "Simulation Input", NODE_CLASS_INTERFACE);
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.labelfunc = node_label;
  ntype.insert_link = node_insert_link;
  ntype.gather_link_search_ops = nullptr;
  ntype.no_muting = true;
  ntype.draw_buttons_ex = node_layout_ex;
  node_type_storage(&ntype,
                    "NodeGeometrySimulationInput",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  nodeRegisterType(&ntype);
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
        "Skip", *skip_bsocket.typeinfo->geometry_nodes_cpp_type, lf::ValueUsage::Maybe);
    lf_index_by_bsocket[skip_bsocket.index_in_tree()] = skip_input_index_;

    skip_inputs_offset_ = inputs_.size();

    /* Add the skip inputs that are linked to the simulation input node. */
    for (const int i : simulation_items_.index_range()) {
      const NodeSimulationItem &item = simulation_items_[i];
      const CPPType &type = get_simulation_item_cpp_type(item);
      inputs_.append_as(item.name, type, lf::ValueUsage::Maybe);
    }

    solve_inputs_offset_ = inputs_.size();

    /* Add the solve inputs that correspond to the simulation state inputs in the UI. */
    for (const int i : simulation_items_.index_range()) {
      const NodeSimulationItem &item = simulation_items_[i];
      const bNodeSocket &input_bsocket = node.input_socket(i + 1);
      const bNodeSocket &output_bsocket = node.output_socket(i);

      const CPPType &type = get_simulation_item_cpp_type(item);

      lf_index_by_bsocket[input_bsocket.index_in_tree()] = inputs_.append_and_get_index_as(
          item.name, type, lf::ValueUsage::Maybe);
      lf_index_by_bsocket[output_bsocket.index_in_tree()] = outputs_.append_and_get_index_as(
          item.name, type);
    }
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const final
  {
    GeoNodesLFUserData &user_data = *static_cast<GeoNodesLFUserData *>(context.user_data);
    if (!user_data.call_data->self_object()) {
      /* The self object is currently required for generating anonymous attribute names. */
      this->set_default_outputs(params);
      return;
    }
    if (!user_data.call_data->simulation_params) {
      this->set_default_outputs(params);
      return;
    }
    std::optional<FoundNestedNodeID> found_id = find_nested_node_id(user_data, node_.identifier);
    if (!found_id) {
      this->set_default_outputs(params);
      return;
    }
    if (found_id->is_in_loop) {
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
                           GeoNodesLFUserData &user_data,
                           bke::bake::BakeDataBlockMap *data_block_map,
                           const bke::bake::BakeStateRef &state) const
  {
    Array<void *> output_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    copy_simulation_state_to_values(simulation_items_,
                                    state,
                                    *user_data.call_data->self_object(),
                                    *user_data.compute_context,
                                    node_,
                                    data_block_map,
                                    output_values);
    for (const int i : simulation_items_.index_range()) {
      params.output_set(i);
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
    Array<void *> output_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    copy_simulation_state_to_values(simulation_items_,
                                    prev_state,
                                    self_object,
                                    compute_context,
                                    node_,
                                    data_block_map,
                                    output_values);

    Array<void *> next_values(simulation_items_.size());
    LinearAllocator<> allocator;
    for (const int i : simulation_items_.index_range()) {
      const CPPType &type = *outputs_[i].type;
      next_values[i] = allocator.allocate(type.size(), type.alignment());
    }
    copy_simulation_state_to_values(simulation_items_,
                                    next_state,
                                    self_object,
                                    compute_context,
                                    node_,
                                    data_block_map,
                                    next_values);

    for (const int i : simulation_items_.index_range()) {
      mix_baked_data_item(eNodeSocketDatatype(simulation_items_[i].socket_type),
                          output_values[i],
                          next_values[i],
                          mix_factor);
    }

    for (const int i : simulation_items_.index_range()) {
      const CPPType &type = *outputs_[i].type;
      type.destruct(next_values[i]);
    }

    for (const int i : simulation_items_.index_range()) {
      params.output_set(i);
    }
  }

  void pass_through(lf::Params &params,
                    GeoNodesLFUserData &user_data,
                    bke::bake::BakeDataBlockMap *data_block_map) const
  {
    std::optional<bke::bake::BakeState> bake_state = this->get_bake_state_from_inputs(
        params, data_block_map, true);
    if (!bake_state) {
      /* Wait for inputs to be computed. */
      return;
    }

    Array<void *> output_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    move_simulation_state_to_values(simulation_items_,
                                    std::move(*bake_state),
                                    *user_data.call_data->self_object(),
                                    *user_data.compute_context,
                                    node_,
                                    data_block_map,
                                    output_values);
    for (const int i : simulation_items_.index_range()) {
      params.output_set(i);
    }
  }

  void store_new_state(lf::Params &params,
                       GeoNodesLFUserData &user_data,
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
    Array<void *> input_values(simulation_items_.size());
    for (const int i : simulation_items_.index_range()) {
      input_values[i] = params.try_get_input_data_ptr_or_request(i + params_offset);
    }
    if (input_values.as_span().contains(nullptr)) {
      /* Wait for inputs to be computed. */
      return std::nullopt;
    }

    return move_values_to_simulation_state(simulation_items_, input_values, data_block_map);
  }
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Bool>("Skip").description(
      "Forward the output of the simulation input node directly to the output node and ignore "
      "the nodes in the simulation zone");

  const bNode *node = b.node_or_null();
  if (node == nullptr) {
    return;
  }

  const NodeGeometrySimulationOutput &storage = node_storage(*node);

  for (const int i : IndexRange(storage.items_num)) {
    const NodeSimulationItem &item = storage.items[i];
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
    const StringRef name = item.name;
    const std::string identifier = SimulationItemsAccessor::socket_identifier_for_item(item);
    auto &input_decl = b.add_input(socket_type, name, identifier);
    auto &output_decl = b.add_output(socket_type, name, identifier).align_with_previous();
    if (socket_type_supports_fields(socket_type)) {
      input_decl.supports_field();
      output_decl.dependent_field({input_decl.index()});
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Extend>("", "__extend__").align_with_previous();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySimulationOutput *data = MEM_cnew<NodeGeometrySimulationOutput>(__func__);

  data->next_identifier = 0;

  data->items = MEM_cnew_array<NodeSimulationItem>(1, __func__);
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
  auto *dst_storage = MEM_new<NodeGeometrySimulationOutput>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<SimulationItemsAccessor>(*src_node, *dst_node);
}

static void node_operators()
{
  WM_operatortype_append(NODE_OT_simulation_zone_item_add);
  WM_operatortype_append(NODE_OT_simulation_zone_item_remove);
  WM_operatortype_append(NODE_OT_simulation_zone_item_move);
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  return socket_items::try_add_item_via_any_extend_socket<SimulationItemsAccessor>(
      *ntree, *node, *node, *link);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SIMULATION_OUTPUT, "Simulation Output", NODE_CLASS_INTERFACE);
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.labelfunc = sim_input_node::node_label;
  ntype.gather_link_search_ops = nullptr;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.no_muting = true;
  ntype.register_operators = node_operators;
  node_type_storage(&ntype, "NodeGeometrySimulationOutput", node_free_storage, node_copy_storage);
  nodeRegisterType(&ntype);
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
  BLI_assert(node.type == GEO_NODE_SIMULATION_INPUT);
  return std::make_unique<
      node_geo_simulation_cc::sim_input_node::LazyFunctionForSimulationInputNode>(
      node_tree, node, own_lf_graph_info);
}

std::unique_ptr<LazyFunction> get_simulation_output_lazy_function(
    const bNode &node, GeometryNodesLazyFunctionGraphInfo &own_lf_graph_info)
{
  BLI_assert(node.type == GEO_NODE_SIMULATION_OUTPUT);
  return std::make_unique<
      node_geo_simulation_cc::sim_output_node::LazyFunctionForSimulationOutputNode>(
      node, own_lf_graph_info);
}

void mix_baked_data_item(const eNodeSocketDatatype socket_type,
                         void *prev,
                         const void *next,
                         const float factor)
{
  switch (socket_type) {
    case SOCK_GEOMETRY: {
      GeometrySet &prev_geo = *static_cast<GeometrySet *>(prev);
      const GeometrySet &next_geo = *static_cast<const GeometrySet *>(next);
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
      const CPPType &type = node_geo_simulation_cc::get_simulation_item_cpp_type(socket_type);
      SocketValueVariant prev_value_variant = *static_cast<const SocketValueVariant *>(prev);
      SocketValueVariant next_value_variant = *static_cast<const SocketValueVariant *>(next);
      if (prev_value_variant.is_context_dependent_field() ||
          next_value_variant.is_context_dependent_field())
      {
        /* Fields are evaluated on geometries and are mixed there. */
        break;
      }

      prev_value_variant.convert_to_single();
      next_value_variant.convert_to_single();

      void *prev_value = prev_value_variant.get_single_ptr().get();
      const void *next_value = next_value_variant.get_single_ptr().get();

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
int SimulationItemsAccessor::node_type = GEO_NODE_SIMULATION_OUTPUT;

void SimulationItemsAccessor::blend_write(BlendWriter *writer, const bNode &node)
{
  const auto &storage = *static_cast<const NodeGeometrySimulationOutput *>(node.storage);
  BLO_write_struct_array(writer, NodeSimulationItem, storage.items_num, storage.items);
  for (const NodeSimulationItem &item : Span(storage.items, storage.items_num)) {
    BLO_write_string(writer, item.name);
  }
}

void SimulationItemsAccessor::blend_read_data(BlendDataReader *reader, bNode &node)
{
  auto &storage = *static_cast<NodeGeometrySimulationOutput *>(node.storage);
  BLO_read_struct_array(reader, NodeSimulationItem, storage.items_num, &storage.items);
  for (const NodeSimulationItem &item : Span(storage.items, storage.items_num)) {
    BLO_read_string(reader, &item.name);
  }
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
