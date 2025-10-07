/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "NOD_geo_bake.hh"
#include "NOD_node_extra_info.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "BLI_path_utils.hh"
#include "BLI_string_utf8.h"

#include "BKE_anonymous_attribute_make.hh"
#include "BKE_bake_geometry_nodes_modifier.hh"
#include "BKE_bake_items_socket.hh"
#include "BKE_context.hh"
#include "BKE_global.hh"
#include "BKE_library.hh"
#include "BKE_main.hh"
#include "BKE_screen.hh"

#include "ED_node.hh"

#include "DNA_modifier_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.hh"

#include "MOD_nodes.hh"

#include "WM_api.hh"

#include "BLO_read_write.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_bake_cc {

namespace bake = bke::bake;

NODE_STORAGE_FUNCS(NodeGeometryBake)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_default_layout();

  const bNodeTree *ntree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const NodeGeometryBake &storage = node_storage(*node);

  for (const int i : IndexRange(storage.items_num)) {
    const NodeGeometryBakeItem &item = storage.items[i];
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(item.socket_type);
    const StringRef name = item.name;
    const std::string identifier = BakeItemsAccessor::socket_identifier_for_item(item);
    auto &input_decl = b.add_input(socket_type, name, identifier)
                           .socket_name_ptr(
                               &ntree->id, BakeItemsAccessor::item_srna, &item, "name");
    auto &output_decl = b.add_output(socket_type, name, identifier).align_with_previous();
    if (socket_type_supports_fields(socket_type)) {
      input_decl.supports_field();
      if (item.flag & GEO_NODE_BAKE_ITEM_IS_ATTRIBUTE) {
        output_decl.field_source();
      }
      else {
        output_decl.dependent_field({input_decl.index()});
      }
    }
    input_decl.structure_type(StructureType::Dynamic);
    output_decl.structure_type(StructureType::Dynamic);
    if (socket_type == SOCK_BUNDLE) {
      dynamic_cast<decl::BundleBuilder &>(output_decl)
          .pass_through_input_index(input_decl.index());
    }
  }
  b.add_input<decl::Extend>("", "__extend__").structure_type(StructureType::Dynamic);
  b.add_output<decl::Extend>("", "__extend__")
      .structure_type(StructureType::Dynamic)
      .align_with_previous();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryBake *data = MEM_callocN<NodeGeometryBake>(__func__);
  node->storage = data;
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<BakeItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometryBake &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<NodeGeometryBake>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<BakeItemsAccessor>(*src_node, *dst_node);
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  return socket_items::try_add_item_via_any_extend_socket<BakeItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void draw_bake_items(const bContext *C, uiLayout *layout, PointerRNA node_ptr)
{
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(node_ptr.owner_id);
  bNode &node = *static_cast<bNode *>(node_ptr.data);
  NodeGeometryBake &storage = node_storage(node);

  if (uiLayout *panel = layout->panel(C, "bake_items", false, IFACE_("Bake Items"))) {
    socket_items::ui::draw_items_list_with_operators<BakeItemsAccessor>(C, panel, tree, node);
    socket_items::ui::draw_active_item_props<BakeItemsAccessor>(
        tree, node, [&](PointerRNA *item_ptr) {
          const NodeGeometryBakeItem &active_item = storage.items[storage.active_index];
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "socket_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          if (socket_type_supports_fields(eNodeSocketDatatype(active_item.socket_type))) {
            panel->prop(item_ptr, "attribute_domain", UI_ITEM_NONE, std::nullopt, ICON_NONE);
            panel->prop(item_ptr, "is_attribute", UI_ITEM_NONE, std::nullopt, ICON_NONE);
          }
        });
  }
}

static void node_operators()
{
  socket_items::ops::make_common_operators<BakeItemsAccessor>();
}

static bake::BakeSocketConfig make_bake_socket_config(const Span<NodeGeometryBakeItem> bake_items)
{
  bake::BakeSocketConfig config;
  const int items_num = bake_items.size();
  config.domains.resize(items_num);
  config.names.resize(items_num);
  config.types.resize(items_num);
  config.geometries_by_attribute.resize(items_num);

  int last_geometry_index = -1;
  for (const int item_i : bake_items.index_range()) {
    const NodeGeometryBakeItem &item = bake_items[item_i];
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

/**
 * This is used when the bake node should just pass-through the data and the caller of geometry
 * nodes should not have to care about this.
 */
struct DummyDataBlockMap : public bake::BakeDataBlockMap {
 private:
  Mutex mutex_;
  Map<bake::BakeDataBlockID, ID *> map_;

 public:
  ID *lookup_or_remember_missing(const bake::BakeDataBlockID &key) override
  {
    std::lock_guard lock{mutex_};
    return map_.lookup_default(key, nullptr);
  }

  void try_add(ID &id) override
  {
    std::lock_guard lock{mutex_};
    map_.add(bake::BakeDataBlockID(id), &id);
  }
};

class LazyFunctionForBakeNode final : public LazyFunction {
  const bNode &node_;
  Span<NodeGeometryBakeItem> bake_items_;
  bake::BakeSocketConfig bake_socket_config_;

 public:
  LazyFunctionForBakeNode(const bNode &node, GeometryNodesLazyFunctionGraphInfo &lf_graph_info)
      : node_(node)
  {
    debug_name_ = "Bake";
    const NodeGeometryBake &storage = node_storage(node);
    bake_items_ = {storage.items, storage.items_num};

    MutableSpan<int> lf_index_by_bsocket = lf_graph_info.mapping.lf_index_by_bsocket;

    for (const int i : bake_items_.index_range()) {
      const NodeGeometryBakeItem &item = bake_items_[i];
      const bNodeSocket &input_bsocket = node.input_socket(i);
      const bNodeSocket &output_bsocket = node.output_socket(i);
      lf_index_by_bsocket[input_bsocket.index_in_tree()] = inputs_.append_and_get_index_as(
          item.name, CPPType::get<SocketValueVariant>(), lf::ValueUsage::Maybe);
      lf_index_by_bsocket[output_bsocket.index_in_tree()] = outputs_.append_and_get_index_as(
          item.name, CPPType::get<SocketValueVariant>());
    }

    bake_socket_config_ = make_bake_socket_config(bake_items_);
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
    if (!user_data.call_data->bake_params) {
      this->set_default_outputs(params);
      return;
    }
    std::optional<FoundNestedNodeID> found_id = find_nested_node_id(user_data, node_.identifier);
    if (!found_id) {
      this->set_default_outputs(params);
      return;
    }
    if (found_id->is_in_loop || found_id->is_in_closure) {
      DummyDataBlockMap data_block_map;
      this->pass_through(params, user_data, &data_block_map);
      return;
    }
    BakeNodeBehavior *behavior = user_data.call_data->bake_params->get(found_id->id);
    if (!behavior) {
      this->set_default_outputs(params);
      return;
    }
    if (auto *info = std::get_if<sim_output::ReadSingle>(&behavior->behavior)) {
      this->output_cached_state(params, user_data, behavior->data_block_map, info->state);
    }
    else if (auto *info = std::get_if<sim_output::ReadInterpolated>(&behavior->behavior)) {
      this->output_mixed_cached_state(params,
                                      behavior->data_block_map,
                                      *user_data.call_data->self_object(),
                                      *user_data.compute_context,
                                      info->prev_state,
                                      info->next_state,
                                      info->mix_factor);
    }
    else if (std::get_if<sim_output::PassThrough>(&behavior->behavior)) {
      this->pass_through(params, user_data, behavior->data_block_map);
    }
    else if (auto *info = std::get_if<sim_output::StoreNewState>(&behavior->behavior)) {
      this->store(params, user_data, behavior->data_block_map, *info);
    }
    else if (auto *info = std::get_if<sim_output::ReadError>(&behavior->behavior)) {
      if (geo_eval_log::GeoTreeLogger *tree_logger = local_user_data.try_get_tree_logger(
              user_data))
      {
        tree_logger->node_warnings.append(
            *tree_logger->allocator, {node_.identifier, {NodeWarningType::Error, info->message}});
      }
      this->set_default_outputs(params);
    }
    else {
      BLI_assert_unreachable();
    }
  }

  void set_default_outputs(lf::Params &params) const
  {
    set_default_remaining_node_outputs(params, node_);
  }

  void pass_through(lf::Params &params,
                    GeoNodesUserData &user_data,
                    bke::bake::BakeDataBlockMap *data_block_map) const
  {
    std::optional<bake::BakeState> bake_state = this->get_bake_state_from_inputs(params,
                                                                                 data_block_map);
    if (!bake_state) {
      /* Wait for inputs to be computed. */
      return;
    }
    Vector<SocketValueVariant> output_values = this->move_bake_state_to_values(
        std::move(*bake_state),
        data_block_map,
        *user_data.call_data->self_object(),
        *user_data.compute_context);
    for (const int i : bake_items_.index_range()) {
      params.set_output(i, std::move(output_values[i]));
    }
  }

  void store(lf::Params &params,
             GeoNodesUserData &user_data,
             bke::bake::BakeDataBlockMap *data_block_map,
             const sim_output::StoreNewState &info) const
  {
    std::optional<bake::BakeState> bake_state = this->get_bake_state_from_inputs(params,
                                                                                 data_block_map);
    if (!bake_state) {
      /* Wait for inputs to be computed. */
      return;
    }
    this->output_cached_state(params, user_data, data_block_map, *bake_state);
    info.store_fn(std::move(*bake_state));
  }

  void output_cached_state(lf::Params &params,
                           GeoNodesUserData &user_data,
                           bke::bake::BakeDataBlockMap *data_block_map,
                           const bake::BakeStateRef &bake_state) const
  {
    Vector<SocketValueVariant> values = this->copy_bake_state_to_values(
        bake_state,
        data_block_map,
        *user_data.call_data->self_object(),
        *user_data.compute_context);
    for (const int i : bake_items_.index_range()) {
      params.set_output(i, std::move(values[i]));
    }
  }

  void output_mixed_cached_state(lf::Params &params,
                                 bke::bake::BakeDataBlockMap *data_block_map,
                                 const Object &self_object,
                                 const ComputeContext &compute_context,
                                 const bake::BakeStateRef &prev_state,
                                 const bake::BakeStateRef &next_state,
                                 const float mix_factor) const
  {
    Vector<SocketValueVariant> output_values = this->copy_bake_state_to_values(
        prev_state, data_block_map, self_object, compute_context);
    Vector<SocketValueVariant> next_values = this->copy_bake_state_to_values(
        next_state, data_block_map, self_object, compute_context);
    for (const int i : bake_items_.index_range()) {
      mix_baked_data_item(eNodeSocketDatatype(bake_items_[i].socket_type),
                          output_values[i],
                          next_values[i],
                          mix_factor);
    }
    for (const int i : bake_items_.index_range()) {
      params.set_output(i, std::move(output_values[i]));
    }
  }

  std::optional<bake::BakeState> get_bake_state_from_inputs(
      lf::Params &params, bke::bake::BakeDataBlockMap *data_block_map) const
  {
    Array<bke::SocketValueVariant *> input_value_pointers(bake_items_.size());
    for (const int i : bake_items_.index_range()) {
      input_value_pointers[i] = params.try_get_input_data_ptr_or_request<bke::SocketValueVariant>(
          i);
    }
    if (input_value_pointers.as_span().contains(nullptr)) {
      /* Wait for inputs to be computed. */
      return std::nullopt;
    }

    Array<bke::SocketValueVariant> input_values(bake_items_.size());
    for (const int i : bake_items_.index_range()) {
      input_values[i] = std::move(*input_value_pointers[i]);
    }

    Array<std::unique_ptr<bake::BakeItem>> bake_items = bake::move_socket_values_to_bake_items(
        input_values, bake_socket_config_, data_block_map);

    bake::BakeState bake_state;
    for (const int i : bake_items_.index_range()) {
      const NodeGeometryBakeItem &item = bake_items_[i];
      std::unique_ptr<bake::BakeItem> &bake_item = bake_items[i];
      if (bake_item) {
        bake_state.items_by_id.add_new(item.identifier, std::move(bake_item));
      }
    }
    return bake_state;
  }

  Vector<SocketValueVariant> move_bake_state_to_values(bake::BakeState bake_state,
                                                       bke::bake::BakeDataBlockMap *data_block_map,
                                                       const Object &self_object,
                                                       const ComputeContext &compute_context) const
  {
    Vector<bake::BakeItem *> bake_items;
    for (const NodeGeometryBakeItem &item : bake_items_) {
      std::unique_ptr<bake::BakeItem> *bake_item = bake_state.items_by_id.lookup_ptr(
          item.identifier);
      bake_items.append(bake_item ? bake_item->get() : nullptr);
    }
    return bake::move_bake_items_to_socket_values(
        bake_items, bake_socket_config_, data_block_map, [&](const int i, const CPPType &type) {
          return this->make_attribute_field(self_object, compute_context, bake_items_[i], type);
        });
  }

  Vector<SocketValueVariant> copy_bake_state_to_values(const bake::BakeStateRef &bake_state,
                                                       bke::bake::BakeDataBlockMap *data_block_map,
                                                       const Object &self_object,
                                                       const ComputeContext &compute_context) const
  {
    Vector<const bake::BakeItem *> bake_items;
    for (const NodeGeometryBakeItem &item : bake_items_) {
      const bake::BakeItem *const *bake_item = bake_state.items_by_id.lookup_ptr(item.identifier);
      bake_items.append(bake_item ? *bake_item : nullptr);
    }
    return bake::copy_bake_items_to_socket_values(
        bake_items, bake_socket_config_, data_block_map, [&](const int i, const CPPType &type) {
          return this->make_attribute_field(self_object, compute_context, bake_items_[i], type);
        });
  }

  std::shared_ptr<AttributeFieldInput> make_attribute_field(const Object &self_object,
                                                            const ComputeContext &compute_context,
                                                            const NodeGeometryBakeItem &item,
                                                            const CPPType &type) const
  {
    std::string attribute_name = bke::hash_to_anonymous_attribute_name(
        compute_context.hash(), self_object.id.name, node_.identifier, item.identifier);
    std::string socket_inspection_name = make_anonymous_attribute_socket_inspection_string(
        node_.label_or_name(), item.name);
    return std::make_shared<AttributeFieldInput>(
        std::move(attribute_name), type, std::move(socket_inspection_name));
  }
};

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

static void node_layout(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  BakeDrawContext ctx;
  const bNode &node = *static_cast<const bNode *>(ptr->data);
  if (!get_bake_draw_context(C, node, ctx)) {
    return;
  }
  layout->active_set(ctx.is_bakeable_in_current_context);
  layout->enabled_set(ID_IS_EDITABLE(ctx.object));
  uiLayout *col = &layout->column(false);
  {
    uiLayout *row = &col->row(true);
    row->enabled_set(!ctx.is_baked);
    row->prop(&ctx.bake_rna, "bake_mode", UI_ITEM_R_EXPAND, IFACE_("Mode"), ICON_NONE);
  }
  draw_bake_button_row(ctx, col);
}

static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  draw_bake_items(C, layout, *ptr);

  BakeDrawContext ctx;
  const bNode &node = *static_cast<const bNode *>(ptr->data);
  if (!get_bake_draw_context(C, node, ctx)) {
    return;
  }

  layout->active_set(ctx.is_bakeable_in_current_context);
  layout->enabled_set(ID_IS_EDITABLE(ctx.object));

  {
    uiLayout *col = &layout->column(false);
    {
      uiLayout *row = &col->row(true);
      row->enabled_set(!ctx.is_baked);
      row->prop(&ctx.bake_rna, "bake_mode", UI_ITEM_R_EXPAND, IFACE_("Mode"), ICON_NONE);
    }

    draw_bake_button_row(ctx, col, true);
    if (const std::optional<std::string> bake_state_str = get_bake_state_string(ctx)) {
      uiLayout *row = &col->row(true);
      row->label(*bake_state_str, ICON_NONE);
    }
  }

  draw_common_bake_settings(C, ctx, layout);
  draw_data_blocks(C, layout, ctx.bake_rna);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const eNodeSocketDatatype type = eNodeSocketDatatype(params.other_socket().type);
  if (!BakeItemsAccessor::supports_socket_type(type, params.node_tree().type)) {
    return;
  }
  params.add_item(
      IFACE_("Value"),
      [type](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeBake");
        socket_items::add_item_with_socket_type_and_name<BakeItemsAccessor>(
            params.node_tree, node, type, params.socket.name);
        params.update_and_connect_available_socket(node, params.socket.name);
      },
      -1);
}

static const bNodeSocket *node_internally_linked_input(const bNodeTree & /*tree*/,
                                                       const bNode &node,
                                                       const bNodeSocket &output_socket)
{
  /* Internal links should always map corresponding input and output sockets. */
  return node.input_by_identifier(output_socket.identifier);
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<BakeItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<BakeItemsAccessor>(&reader, node);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeBake", GEO_NODE_BAKE);
  ntype.ui_name = "Bake";
  ntype.ui_description = "Cache the incoming data so that it can be used without recomputation";
  ntype.enum_name_legacy = "BAKE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.get_extra_info = node_extra_info;
  ntype.register_operators = node_operators;
  ntype.gather_link_search_ops = node_gather_link_searches;
  ntype.internally_linked_input = node_internally_linked_input;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  blender::bke::node_type_storage(ntype, "NodeGeometryBake", node_free_storage, node_copy_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_bake_cc

namespace blender::nodes {

bool get_bake_draw_context(const bContext *C, const bNode &node, BakeDrawContext &r_ctx)
{
  BLI_assert(ELEM(node.type_legacy, GEO_NODE_BAKE, GEO_NODE_SIMULATION_OUTPUT));
  r_ctx.node = &node;
  r_ctx.snode = CTX_wm_space_node(C);
  if (!r_ctx.snode) {
    return false;
  }
  std::optional<ed::space_node::ObjectAndModifier> object_and_modifier =
      ed::space_node::get_modifier_for_node_editor(*r_ctx.snode);
  if (!object_and_modifier) {
    return false;
  }
  r_ctx.object = object_and_modifier->object;
  r_ctx.nmd = object_and_modifier->nmd;
  const std::optional<FoundNestedNodeID> bake_id = ed::space_node::find_nested_node_id_in_root(
      *r_ctx.snode, *r_ctx.node);
  if (!bake_id) {
    return false;
  }
  r_ctx.is_bakeable_in_current_context = !bake_id->is_in_loop && !bake_id->is_in_closure;
  r_ctx.bake = nullptr;
  for (const NodesModifierBake &iter_bake : Span(r_ctx.nmd->bakes, r_ctx.nmd->bakes_num)) {
    if (iter_bake.id == bake_id->id) {
      r_ctx.bake = &iter_bake;
      break;
    }
  }
  if (!r_ctx.bake) {
    return false;
  }

  r_ctx.bake_rna = RNA_pointer_create_discrete(
      const_cast<ID *>(&r_ctx.object->id), &RNA_NodesModifierBake, (void *)r_ctx.bake);
  if (r_ctx.nmd->runtime->cache) {
    const bke::bake::ModifierCache &cache = *r_ctx.nmd->runtime->cache;
    std::lock_guard lock{cache.mutex};
    if (const std::unique_ptr<bke::bake::BakeNodeCache> *node_cache_ptr =
            cache.bake_cache_by_id.lookup_ptr(bake_id->id))
    {
      const bke::bake::BakeNodeCache &node_cache = **node_cache_ptr;
      if (!node_cache.bake.frames.is_empty()) {
        const int first_frame = node_cache.bake.frames.first()->frame.frame();
        const int last_frame = node_cache.bake.frames.last()->frame.frame();
        r_ctx.baked_range = IndexRange(first_frame, last_frame - first_frame + 1);
      }
    }
    else if (const std::unique_ptr<bke::bake::SimulationNodeCache> *node_cache_ptr =
                 cache.simulation_cache_by_id.lookup_ptr(bake_id->id))
    {
      const bke::bake::SimulationNodeCache &node_cache = **node_cache_ptr;
      if (!node_cache.bake.frames.is_empty() &&
          node_cache.cache_status == bke::bake::CacheStatus::Baked)
      {
        const int first_frame = node_cache.bake.frames.first()->frame.frame();
        const int last_frame = node_cache.bake.frames.last()->frame.frame();
        r_ctx.baked_range = IndexRange(first_frame, last_frame - first_frame + 1);
      }
    }
  }
  const Scene *scene = CTX_data_scene(C);
  r_ctx.frame_range = bke::bake::get_node_bake_frame_range(
      *scene, *r_ctx.object, *r_ctx.nmd, r_ctx.bake->id);
  r_ctx.bake_still = node.type_legacy == GEO_NODE_BAKE &&
                     r_ctx.bake->bake_mode == NODES_MODIFIER_BAKE_MODE_STILL;
  r_ctx.is_baked = r_ctx.baked_range.has_value();
  r_ctx.bake_target = bke::bake::get_node_bake_target(*r_ctx.object, *r_ctx.nmd, r_ctx.bake->id);

  return true;
}

std::string get_baked_string(const BakeDrawContext &ctx)
{
  if (ctx.bake_still && ctx.baked_range->size() == 1) {
    return fmt::format(fmt::runtime(RPT_("Baked Frame {}")), ctx.baked_range->first());
  }
  return fmt::format(
      fmt::runtime(RPT_("Baked {} - {}")), ctx.baked_range->first(), ctx.baked_range->last());
}

std::optional<std::string> get_bake_state_string(const BakeDrawContext &ctx)
{
  if (G.is_rendering) {
    /* Avoid accessing data that is generated while baking. */
    return std::nullopt;
  }
  if (ctx.is_baked) {
    const std::string baked_str = get_baked_string(ctx);
    char size_str[BLI_STR_FORMAT_INT64_BYTE_UNIT_SIZE];
    BLI_str_format_byte_unit(size_str, ctx.bake->bake_size, true);
    if (ctx.bake->packed) {
      return fmt::format(fmt::runtime(RPT_("{} ({} packed)")), baked_str, size_str);
    }
    return fmt::format(fmt::runtime(RPT_("{} ({} on disk)")), baked_str, size_str);
  }
  if (ctx.frame_range.has_value()) {
    if (!ctx.bake_still) {
      return fmt::format(
          fmt::runtime(RPT_("Frames {} - {}")), ctx.frame_range->first(), ctx.frame_range->last());
    }
  }
  return std::nullopt;
}

void draw_bake_button_row(const BakeDrawContext &ctx, uiLayout *layout, const bool is_in_sidebar)
{
  uiLayout *col = &layout->column(true);
  uiLayout *row = &col->row(true);
  {
    const char *bake_label = IFACE_("Bake");
    if (is_in_sidebar) {
      bake_label = ctx.bake_target == NODES_MODIFIER_BAKE_TARGET_DISK ? IFACE_("Bake to Disk") :
                                                                        IFACE_("Bake Packed");
    }

    PointerRNA ptr = row->op("OBJECT_OT_geometry_node_bake_single",
                             bake_label,
                             ICON_NONE,
                             wm::OpCallContext::InvokeDefault,
                             UI_ITEM_NONE);
    WM_operator_properties_id_lookup_set_from_id(&ptr, &ctx.object->id);
    RNA_string_set(&ptr, "modifier_name", ctx.nmd->modifier.name);
    RNA_int_set(&ptr, "bake_id", ctx.bake->id);
  }
  {
    uiLayout *subrow = &row->row(true);
    subrow->active_set(ctx.is_baked);
    if (is_in_sidebar) {
      if (ctx.is_baked && !G.is_rendering) {
        if (ctx.bake->packed) {
          PointerRNA ptr = subrow->op("OBJECT_OT_geometry_node_bake_unpack_single",
                                      "",
                                      ICON_PACKAGE,
                                      wm::OpCallContext::InvokeDefault,
                                      UI_ITEM_NONE);
          WM_operator_properties_id_lookup_set_from_id(&ptr, &ctx.object->id);
          RNA_string_set(&ptr, "modifier_name", ctx.nmd->modifier.name);
          RNA_int_set(&ptr, "bake_id", ctx.bake->id);
        }
        else {
          PointerRNA ptr = subrow->op("OBJECT_OT_geometry_node_bake_pack_single",
                                      "",
                                      ICON_UGLYPACKAGE,
                                      wm::OpCallContext::InvokeDefault,
                                      UI_ITEM_NONE);
          WM_operator_properties_id_lookup_set_from_id(&ptr, &ctx.object->id);
          RNA_string_set(&ptr, "modifier_name", ctx.nmd->modifier.name);
          RNA_int_set(&ptr, "bake_id", ctx.bake->id);
        }
      }
      else {
        /* If the data is not yet baked, still show the icon based on the derived bake target. */
        const int icon = ctx.bake_target == NODES_MODIFIER_BAKE_TARGET_DISK ? ICON_UGLYPACKAGE :
                                                                              ICON_PACKAGE;
        PointerRNA ptr = subrow->op("OBJECT_OT_geometry_node_bake_pack_single",
                                    "",
                                    icon,
                                    wm::OpCallContext::InvokeDefault,
                                    UI_ITEM_NONE);
      }
    }
    {
      PointerRNA ptr = subrow->op("OBJECT_OT_geometry_node_bake_delete_single",
                                  "",
                                  ICON_TRASH,
                                  wm::OpCallContext::InvokeDefault,
                                  UI_ITEM_NONE);
      WM_operator_properties_id_lookup_set_from_id(&ptr, &ctx.object->id);
      RNA_string_set(&ptr, "modifier_name", ctx.nmd->modifier.name);
      RNA_int_set(&ptr, "bake_id", ctx.bake->id);
    }
  }
}

void draw_common_bake_settings(bContext *C, BakeDrawContext &ctx, uiLayout *layout)
{
  layout->use_property_split_set(true);
  layout->use_property_decorate_set(false);

  uiLayout *settings_col = &layout->column(false);
  settings_col->active_set(!ctx.is_baked);
  {
    uiLayout *col = &settings_col->column(true);
    col->prop(&ctx.bake_rna, "bake_target", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    uiLayout *subcol = &col->column(true);
    subcol->active_set(ctx.bake_target == NODES_MODIFIER_BAKE_TARGET_DISK);
    subcol->prop(&ctx.bake_rna, "use_custom_path", UI_ITEM_NONE, IFACE_("Custom Path"), ICON_NONE);
    uiLayout *subsubcol = &subcol->column(true);
    const bool use_custom_path = ctx.bake->flag & NODES_MODIFIER_BAKE_CUSTOM_PATH;
    subsubcol->active_set(use_custom_path);
    Main *bmain = CTX_data_main(C);
    auto bake_path = bke::bake::get_node_bake_path(*bmain, *ctx.object, *ctx.nmd, ctx.bake->id);

    char placeholder_path[FILE_MAX] = "";
    if (StringRef(ctx.bake->directory).is_empty() &&
        !(ctx.bake->flag & NODES_MODIFIER_BAKE_CUSTOM_PATH) && bake_path.has_value() &&
        bake_path->bake_dir.has_value())
    {
      STRNCPY(placeholder_path, bake_path->bake_dir->c_str());
      if (BLI_path_is_rel(ctx.nmd->bake_directory)) {
        BLI_path_rel(placeholder_path, BKE_main_blendfile_path(bmain));
      }
    }

    subsubcol->prop(&ctx.bake_rna,
                    RNA_struct_find_property(&ctx.bake_rna, "directory"),
                    -1,
                    0,
                    UI_ITEM_NONE,
                    IFACE_("Path"),
                    ICON_NONE,
                    placeholder_path);
  }
  {
    uiLayout *col = &settings_col->column(true);
    col->prop(&ctx.bake_rna,
              "use_custom_simulation_frame_range",
              UI_ITEM_NONE,
              IFACE_("Custom Range"),
              ICON_NONE);
    uiLayout *subcol = &col->column(true);
    subcol->active_set(ctx.bake->flag & NODES_MODIFIER_BAKE_CUSTOM_SIMULATION_FRAME_RANGE);
    subcol->prop(&ctx.bake_rna, "frame_start", UI_ITEM_NONE, IFACE_("Start"), ICON_NONE);
    subcol->prop(&ctx.bake_rna, "frame_end", UI_ITEM_NONE, IFACE_("End"), ICON_NONE);
  }
}

static void draw_bake_data_block_list_item(uiList * /*ui_list*/,
                                           const bContext * /*C*/,
                                           uiLayout *layout,
                                           PointerRNA * /*idataptr*/,
                                           PointerRNA *itemptr,
                                           int /*icon*/,
                                           PointerRNA * /*active_dataptr*/,
                                           const char * /*active_propname*/,
                                           int /*index*/,
                                           int /*flt_flag*/)
{
  auto &data_block = *static_cast<NodesModifierDataBlock *>(itemptr->data);
  uiLayout *row = &layout->row(true);

  std::string name;
  if (StringRef(data_block.lib_name).is_empty()) {
    name = data_block.id_name;
  }
  else {
    name = fmt::format("{} [{}]", data_block.id_name, data_block.lib_name);
  }

  row->prop(itemptr, "id", UI_ITEM_NONE, name, ICON_NONE);
}

void draw_data_blocks(const bContext *C, uiLayout *layout, PointerRNA &bake_rna)
{
  static const uiListType *data_block_list = []() {
    uiListType *list = MEM_callocN<uiListType>(__func__);
    STRNCPY_UTF8(list->idname, "DATA_UL_nodes_modifier_data_blocks");
    list->draw_item = draw_bake_data_block_list_item;
    WM_uilisttype_add(list);
    return list;
  }();

  PointerRNA data_blocks_ptr = RNA_pointer_create_discrete(
      bake_rna.owner_id, &RNA_NodesModifierBakeDataBlocks, bake_rna.data);

  if (uiLayout *panel = layout->panel(
          C, "data_block_references", true, IFACE_("Data-Block References")))
  {
    uiTemplateList(panel,
                   C,
                   data_block_list->idname,
                   "",
                   &bake_rna,
                   "data_blocks",
                   &data_blocks_ptr,
                   "active_index",
                   nullptr,
                   3,
                   5,
                   UILST_LAYOUT_DEFAULT,
                   0,
                   UI_TEMPLATE_LIST_FLAG_NONE);
  }
}

std::unique_ptr<LazyFunction> get_bake_lazy_function(
    const bNode &node, GeometryNodesLazyFunctionGraphInfo &lf_graph_info)
{
  namespace file_ns = blender::nodes::node_geo_bake_cc;
  BLI_assert(node.type_legacy == GEO_NODE_BAKE);
  return std::make_unique<file_ns::LazyFunctionForBakeNode>(node, lf_graph_info);
}

StructRNA *BakeItemsAccessor::item_srna = &RNA_NodeGeometryBakeItem;

void BakeItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void BakeItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

};  // namespace blender::nodes
