/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <fmt/format.h>

#include "NOD_geo_bake.hh"
#include "NOD_node_extra_info.hh"
#include "NOD_rna_define.hh"
#include "NOD_socket_items_ops.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BLI_string.h"

#include "BKE_bake_geometry_nodes_modifier.hh"
#include "BKE_bake_items_socket.hh"
#include "BKE_context.hh"
#include "BKE_screen.hh"

#include "ED_node.hh"

#include "DNA_modifier_types.h"

#include "RNA_access.hh"
#include "RNA_prototypes.h"

#include "MOD_nodes.hh"

#include "WM_api.hh"

#include "BLO_read_write.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_bake_cc {

namespace bake = bke::bake;

NODE_STORAGE_FUNCS(NodeGeometryBake)

static void node_declare(NodeDeclarationBuilder &b)
{
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
    auto &input_decl = b.add_input(socket_type, name, identifier);
    auto &output_decl = b.add_output(socket_type, name, identifier);
    if (socket_type_supports_fields(socket_type)) {
      input_decl.supports_field();
      if (item.flag & GEO_NODE_BAKE_ITEM_IS_ATTRIBUTE) {
        output_decl.field_source();
      }
      else {
        output_decl.dependent_field({input_decl.index()});
      }
    }
  }
  b.add_input<decl::Extend>("", "__extend__");
  b.add_output<decl::Extend>("", "__extend__");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryBake *data = MEM_cnew<NodeGeometryBake>(__func__);

  data->items = MEM_cnew_array<NodeGeometryBakeItem>(1, __func__);
  data->items_num = 1;

  NodeGeometryBakeItem &item = data->items[0];
  item.name = BLI_strdup("Geometry");
  item.identifier = data->next_identifier++;
  item.attribute_domain = int16_t(AttrDomain::Point);
  item.socket_type = SOCK_GEOMETRY;

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
  auto *dst_storage = MEM_new<NodeGeometryBake>(__func__, src_storage);
  dst_node->storage = dst_storage;

  socket_items::copy_array<BakeItemsAccessor>(*src_node, *dst_node);
}

static bool node_insert_link(bNodeTree *ntree, bNode *node, bNodeLink *link)
{
  return socket_items::try_add_item_via_any_extend_socket<BakeItemsAccessor>(
      *ntree, *node, *node, *link);
}

static const CPPType &get_item_cpp_type(const eNodeSocketDatatype socket_type)
{
  const char *socket_idname = nodeStaticSocketType(socket_type, 0);
  const bNodeSocketType *typeinfo = nodeSocketTypeFind(socket_idname);
  BLI_assert(typeinfo);
  BLI_assert(typeinfo->geometry_nodes_cpp_type);
  return *typeinfo->geometry_nodes_cpp_type;
}

static void draw_bake_item(uiList * /*ui_list*/,
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

static void draw_bake_items(const bContext *C, uiLayout *layout, PointerRNA node_ptr)
{
  static const uiListType *bake_items_list = []() {
    uiListType *list = MEM_cnew<uiListType>(__func__);
    STRNCPY(list->idname, "DATA_UL_bake_node_items");
    list->draw_item = draw_bake_item;
    WM_uilisttype_add(list);
    return list;
  }();

  bNode &node = *static_cast<bNode *>(node_ptr.data);

  if (uiLayout *panel = uiLayoutPanel(C, layout, "bake_items", false, TIP_("Bake Items"))) {
    uiLayout *row = uiLayoutRow(panel, false);
    uiTemplateList(row,
                   C,
                   bake_items_list->idname,
                   "",
                   &node_ptr,
                   "bake_items",
                   &node_ptr,
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
        uiItemO(add_remove_col, "", ICON_ADD, "node.bake_node_item_add");
        uiItemO(add_remove_col, "", ICON_REMOVE, "node.bake_node_item_remove");
      }
      {
        uiLayout *up_down_col = uiLayoutColumn(ops_col, true);
        uiItemEnumO(up_down_col, "node.bake_node_item_move", "", ICON_TRIA_UP, "direction", 0);
        uiItemEnumO(up_down_col, "node.bake_node_item_move", "", ICON_TRIA_DOWN, "direction", 1);
      }
    }

    NodeGeometryBake &storage = node_storage(node);
    if (storage.active_index >= 0 && storage.active_index < storage.items_num) {
      NodeGeometryBakeItem &active_item = storage.items[storage.active_index];
      PointerRNA item_ptr = RNA_pointer_create(
          node_ptr.owner_id, BakeItemsAccessor::item_srna, &active_item);
      uiLayoutSetPropSep(panel, true);
      uiLayoutSetPropDecorate(panel, false);
      uiItemR(panel, &item_ptr, "socket_type", UI_ITEM_NONE, nullptr, ICON_NONE);
      if (socket_type_supports_fields(eNodeSocketDatatype(active_item.socket_type))) {
        uiItemR(panel, &item_ptr, "attribute_domain", UI_ITEM_NONE, nullptr, ICON_NONE);
        uiItemR(panel, &item_ptr, "is_attribute", UI_ITEM_NONE, nullptr, ICON_NONE);
      }
    }
  }
}

static void NODE_OT_bake_node_item_remove(wmOperatorType *ot)
{
  socket_items::ops::remove_active_item<BakeItemsAccessor>(
      ot, "Remove Bake Item", __func__, "Remove active bake item");
}

static void NODE_OT_bake_node_item_add(wmOperatorType *ot)
{
  socket_items::ops::add_item<BakeItemsAccessor>(ot, "Add Bake Item", __func__, "Add bake item");
}

static void NODE_OT_bake_node_item_move(wmOperatorType *ot)
{
  socket_items::ops::move_active_item<BakeItemsAccessor>(
      ot, "Move Bake Item", __func__, "Move active bake item");
}

static void node_operators()
{
  WM_operatortype_append(NODE_OT_bake_node_item_add);
  WM_operatortype_append(NODE_OT_bake_node_item_remove);
  WM_operatortype_append(NODE_OT_bake_node_item_move);
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
      const CPPType &type = get_item_cpp_type(eNodeSocketDatatype(item.socket_type));
      lf_index_by_bsocket[input_bsocket.index_in_tree()] = inputs_.append_and_get_index_as(
          item.name, type, lf::ValueUsage::Maybe);
      lf_index_by_bsocket[output_bsocket.index_in_tree()] = outputs_.append_and_get_index_as(
          item.name, type);
    }

    bake_socket_config_ = make_bake_socket_config(bake_items_);
  }

  void execute_impl(lf::Params &params, const lf::Context &context) const final
  {
    GeoNodesLFUserData &user_data = *static_cast<GeoNodesLFUserData *>(context.user_data);
    GeoNodesLFLocalUserData &local_user_data = *static_cast<GeoNodesLFLocalUserData *>(
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
    if (found_id->is_in_loop) {
      this->set_default_outputs(params);
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
                    GeoNodesLFUserData &user_data,
                    bke::bake::BakeDataBlockMap *data_block_map) const
  {
    std::optional<bake::BakeState> bake_state = this->get_bake_state_from_inputs(params,
                                                                                 data_block_map);
    if (!bake_state) {
      /* Wait for inputs to be computed. */
      return;
    }
    Array<void *> output_values(bake_items_.size());
    for (const int i : bake_items_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    this->move_bake_state_to_values(std::move(*bake_state),
                                    data_block_map,
                                    *user_data.call_data->self_object(),
                                    *user_data.compute_context,
                                    output_values);
    for (const int i : bake_items_.index_range()) {
      params.output_set(i);
    }
  }

  void store(lf::Params &params,
             GeoNodesLFUserData &user_data,
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
                           GeoNodesLFUserData &user_data,
                           bke::bake::BakeDataBlockMap *data_block_map,
                           const bake::BakeStateRef &bake_state) const
  {
    Array<void *> output_values(bake_items_.size());
    for (const int i : bake_items_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    this->copy_bake_state_to_values(bake_state,
                                    data_block_map,
                                    *user_data.call_data->self_object(),
                                    *user_data.compute_context,
                                    output_values);
    for (const int i : bake_items_.index_range()) {
      params.output_set(i);
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
    Array<void *> output_values(bake_items_.size());
    for (const int i : bake_items_.index_range()) {
      output_values[i] = params.get_output_data_ptr(i);
    }
    this->copy_bake_state_to_values(
        prev_state, data_block_map, self_object, compute_context, output_values);

    Array<void *> next_values(bake_items_.size());
    LinearAllocator<> allocator;
    for (const int i : bake_items_.index_range()) {
      const CPPType &type = *outputs_[i].type;
      next_values[i] = allocator.allocate(type.size(), type.alignment());
    }
    this->copy_bake_state_to_values(
        next_state, data_block_map, self_object, compute_context, next_values);

    for (const int i : bake_items_.index_range()) {
      mix_baked_data_item(eNodeSocketDatatype(bake_items_[i].socket_type),
                          output_values[i],
                          next_values[i],
                          mix_factor);
    }

    for (const int i : bake_items_.index_range()) {
      const CPPType &type = *outputs_[i].type;
      type.destruct(next_values[i]);
    }

    for (const int i : bake_items_.index_range()) {
      params.output_set(i);
    }
  }

  std::optional<bake::BakeState> get_bake_state_from_inputs(
      lf::Params &params, bke::bake::BakeDataBlockMap *data_block_map) const
  {
    Array<void *> input_values(bake_items_.size());
    for (const int i : bake_items_.index_range()) {
      input_values[i] = params.try_get_input_data_ptr_or_request(i);
    }
    if (input_values.as_span().contains(nullptr)) {
      /* Wait for inputs to be computed. */
      return std::nullopt;
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

  void move_bake_state_to_values(bake::BakeState bake_state,
                                 bke::bake::BakeDataBlockMap *data_block_map,
                                 const Object &self_object,
                                 const ComputeContext &compute_context,
                                 Span<void *> r_output_values) const
  {
    Vector<bake::BakeItem *> bake_items;
    for (const NodeGeometryBakeItem &item : bake_items_) {
      std::unique_ptr<bake::BakeItem> *bake_item = bake_state.items_by_id.lookup_ptr(
          item.identifier);
      bake_items.append(bake_item ? bake_item->get() : nullptr);
    }
    bake::move_bake_items_to_socket_values(
        bake_items,
        bake_socket_config_,
        data_block_map,
        [&](const int i, const CPPType &type) {
          return this->make_attribute_field(self_object, compute_context, bake_items_[i], type);
        },
        r_output_values);
  }

  void copy_bake_state_to_values(const bake::BakeStateRef &bake_state,
                                 bke::bake::BakeDataBlockMap *data_block_map,
                                 const Object &self_object,
                                 const ComputeContext &compute_context,
                                 Span<void *> r_output_values) const
  {
    Vector<const bake::BakeItem *> bake_items;
    for (const NodeGeometryBakeItem &item : bake_items_) {
      const bake::BakeItem *const *bake_item = bake_state.items_by_id.lookup_ptr(item.identifier);
      bake_items.append(bake_item ? *bake_item : nullptr);
    }
    bake::copy_bake_items_to_socket_values(
        bake_items,
        bake_socket_config_,
        data_block_map,
        [&](const int i, const CPPType &type) {
          return this->make_attribute_field(self_object, compute_context, bake_items_[i], type);
        },
        r_output_values);
  }

  std::shared_ptr<AnonymousAttributeFieldInput> make_attribute_field(
      const Object &self_object,
      const ComputeContext &compute_context,
      const NodeGeometryBakeItem &item,
      const CPPType &type) const
  {
    AnonymousAttributeIDPtr attribute_id = AnonymousAttributeIDPtr(
        MEM_new<NodeAnonymousAttributeID>(__func__,
                                          self_object,
                                          compute_context,
                                          node_,
                                          std::to_string(item.identifier),
                                          item.name));
    return std::make_shared<AnonymousAttributeFieldInput>(
        attribute_id, type, node_.label_or_name());
  }
};

struct BakeDrawContext {
  const bNode *node;
  SpaceNode *snode;
  const Object *object;
  const NodesModifierData *nmd;
  const NodesModifierBake *bake;
  PointerRNA bake_rna;
  std::optional<IndexRange> baked_range;
  bool bake_still;
  bool is_baked;
};

[[nodiscard]] static bool get_bake_draw_context(const bContext *C,
                                                const bNode &node,
                                                BakeDrawContext &r_ctx)
{
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
  const std::optional<int32_t> bake_id = ed::space_node::find_nested_node_id_in_root(*r_ctx.snode,
                                                                                     *r_ctx.node);
  if (!bake_id) {
    return false;
  }
  r_ctx.bake = nullptr;
  for (const NodesModifierBake &iter_bake : Span(r_ctx.nmd->bakes, r_ctx.nmd->bakes_num)) {
    if (iter_bake.id == *bake_id) {
      r_ctx.bake = &iter_bake;
      break;
    }
  }
  if (!r_ctx.bake) {
    return false;
  }

  r_ctx.bake_rna = RNA_pointer_create(
      const_cast<ID *>(&r_ctx.object->id), &RNA_NodesModifierBake, (void *)r_ctx.bake);
  if (r_ctx.nmd->runtime->cache) {
    const bake::ModifierCache &cache = *r_ctx.nmd->runtime->cache;
    std::lock_guard lock{cache.mutex};
    if (const std::unique_ptr<bake::BakeNodeCache> *node_cache_ptr =
            cache.bake_cache_by_id.lookup_ptr(*bake_id))
    {
      const bake::BakeNodeCache &node_cache = **node_cache_ptr;
      if (!node_cache.bake.frames.is_empty()) {
        const int first_frame = node_cache.bake.frames.first()->frame.frame();
        const int last_frame = node_cache.bake.frames.last()->frame.frame();
        r_ctx.baked_range = IndexRange(first_frame, last_frame - first_frame + 1);
      }
    }
  }

  r_ctx.bake_still = r_ctx.bake->bake_mode == NODES_MODIFIER_BAKE_MODE_STILL;
  r_ctx.is_baked = r_ctx.baked_range.has_value();

  return true;
}

static std::string get_baked_string(const BakeDrawContext &ctx)
{
  if (ctx.bake_still && ctx.baked_range->size() == 1) {
    return fmt::format(RPT_("Baked Frame {}"), ctx.baked_range->first());
  }
  return fmt::format(RPT_("Baked {} - {}"), ctx.baked_range->first(), ctx.baked_range->last());
}

static void draw_bake_button(uiLayout *layout, const BakeDrawContext &ctx)
{
  uiLayout *col = uiLayoutColumn(layout, true);
  uiLayout *row = uiLayoutRow(col, true);
  {
    PointerRNA ptr;
    uiItemFullO(row,
                "OBJECT_OT_geometry_node_bake_single",
                IFACE_("Bake"),
                ICON_NONE,
                nullptr,
                WM_OP_INVOKE_DEFAULT,
                UI_ITEM_NONE,
                &ptr);
    WM_operator_properties_id_lookup_set_from_id(&ptr, &ctx.object->id);
    RNA_string_set(&ptr, "modifier_name", ctx.nmd->modifier.name);
    RNA_int_set(&ptr, "bake_id", ctx.bake->id);
  }
  {
    uiLayout *subrow = uiLayoutRow(row, true);
    uiLayoutSetActive(subrow, ctx.is_baked);
    PointerRNA ptr;
    uiItemFullO(subrow,
                "OBJECT_OT_geometry_node_bake_delete_single",
                "",
                ICON_TRASH,
                nullptr,
                WM_OP_INVOKE_DEFAULT,
                UI_ITEM_NONE,
                &ptr);
    WM_operator_properties_id_lookup_set_from_id(&ptr, &ctx.object->id);
    RNA_string_set(&ptr, "modifier_name", ctx.nmd->modifier.name);
    RNA_int_set(&ptr, "bake_id", ctx.bake->id);
  }
}

static void node_extra_info(NodeExtraInfoParams &params)
{
  BakeDrawContext ctx;
  if (!get_bake_draw_context(&params.C, params.node, ctx)) {
    return;
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

  uiLayoutSetEnabled(layout, !ID_IS_LINKED(ctx.object));
  uiLayout *col = uiLayoutColumn(layout, false);
  {
    uiLayout *row = uiLayoutRow(col, true);
    uiLayoutSetEnabled(row, !ctx.is_baked);
    uiItemR(row, &ctx.bake_rna, "bake_mode", UI_ITEM_R_EXPAND, "Mode", ICON_NONE);
  }
  draw_bake_button(col, ctx);
}

static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  draw_bake_items(C, layout, *ptr);

  BakeDrawContext ctx;
  const bNode &node = *static_cast<const bNode *>(ptr->data);
  if (!get_bake_draw_context(C, node, ctx)) {
    return;
  }

  uiLayoutSetEnabled(layout, !ID_IS_LINKED(ctx.object));

  {
    uiLayout *col = uiLayoutColumn(layout, false);
    {
      uiLayout *row = uiLayoutRow(col, true);
      uiLayoutSetEnabled(row, !ctx.is_baked);
      uiItemR(row, &ctx.bake_rna, "bake_mode", UI_ITEM_R_EXPAND, "Mode", ICON_NONE);
    }

    draw_bake_button(col, ctx);
    if (ctx.is_baked) {
      const std::string label = get_baked_string(ctx);
      uiItemL(col, label.c_str(), ICON_NONE);
    }
  }

  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  {
    uiLayout *settings_col = uiLayoutColumn(layout, false);
    uiLayoutSetEnabled(settings_col, !ctx.is_baked);
    {
      uiLayout *col = uiLayoutColumn(settings_col, true);
      uiItemR(col, &ctx.bake_rna, "use_custom_path", UI_ITEM_NONE, "Custom Path", ICON_NONE);
      uiLayout *subcol = uiLayoutColumn(col, true);
      uiLayoutSetActive(subcol, ctx.bake->flag & NODES_MODIFIER_BAKE_CUSTOM_PATH);
      uiItemR(subcol, &ctx.bake_rna, "directory", UI_ITEM_NONE, "Path", ICON_NONE);
    }
    if (!ctx.bake_still) {
      uiLayout *col = uiLayoutColumn(settings_col, true);
      uiItemR(col,
              &ctx.bake_rna,
              "use_custom_simulation_frame_range",
              UI_ITEM_NONE,
              "Custom Range",
              ICON_NONE);
      uiLayout *subcol = uiLayoutColumn(col, true);
      uiLayoutSetActive(subcol,
                        ctx.bake->flag & NODES_MODIFIER_BAKE_CUSTOM_SIMULATION_FRAME_RANGE);
      uiItemR(subcol, &ctx.bake_rna, "frame_start", UI_ITEM_NONE, "Start", ICON_NONE);
      uiItemR(subcol, &ctx.bake_rna, "frame_end", UI_ITEM_NONE, "End", ICON_NONE);
    }
  }

  draw_data_blocks(C, layout, ctx.bake_rna);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_BAKE, "Bake", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.initfunc = node_init;
  ntype.insert_link = node_insert_link;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.get_extra_info = node_extra_info;
  ntype.register_operators = node_operators;
  node_type_storage(&ntype, "NodeGeometryBake", node_free_storage, node_copy_storage);
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_bake_cc

namespace blender::nodes {

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
  uiLayout *row = uiLayoutRow(layout, true);

  std::string name;
  if (StringRef(data_block.lib_name).is_empty()) {
    name = data_block.id_name;
  }
  else {
    name = fmt::format("{} [{}]", data_block.id_name, data_block.lib_name);
  }

  uiItemR(row, itemptr, "id", UI_ITEM_NONE, name.c_str(), ICON_NONE);
}

void draw_data_blocks(const bContext *C, uiLayout *layout, PointerRNA &bake_rna)
{
  static const uiListType *data_block_list = []() {
    uiListType *list = MEM_cnew<uiListType>(__func__);
    STRNCPY(list->idname, "DATA_UL_nodes_modifier_data_blocks");
    list->draw_item = draw_bake_data_block_list_item;
    WM_uilisttype_add(list);
    return list;
  }();

  PointerRNA data_blocks_ptr = RNA_pointer_create(
      bake_rna.owner_id, &RNA_NodesModifierBakeDataBlocks, bake_rna.data);

  if (uiLayout *panel = uiLayoutPanel(
          C, layout, "data_block_references", true, TIP_("Data-Block References")))
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
  BLI_assert(node.type == GEO_NODE_BAKE);
  return std::make_unique<file_ns::LazyFunctionForBakeNode>(node, lf_graph_info);
}

StructRNA *BakeItemsAccessor::item_srna = &RNA_NodeGeometryBakeItem;
int BakeItemsAccessor::node_type = GEO_NODE_BAKE;

void BakeItemsAccessor::blend_write(BlendWriter *writer, const bNode &node)
{
  const auto &storage = *static_cast<const NodeGeometryBake *>(node.storage);
  BLO_write_struct_array(writer, NodeGeometryBakeItem, storage.items_num, storage.items);
  for (const NodeGeometryBakeItem &item : Span(storage.items, storage.items_num)) {
    BLO_write_string(writer, item.name);
  }
}

void BakeItemsAccessor::blend_read_data(BlendDataReader *reader, bNode &node)
{
  auto &storage = *static_cast<NodeGeometryBake *>(node.storage);
  BLO_read_struct_array(reader, NodeGeometryBakeItem, storage.items_num, &storage.items);
  for (const NodeGeometryBakeItem &item : Span(storage.items, storage.items_num)) {
    BLO_read_string(reader, &item.name);
  }
}

};  // namespace blender::nodes
