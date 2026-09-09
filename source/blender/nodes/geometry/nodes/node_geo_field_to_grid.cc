/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "NOD_socket_search_link.hh"
#include "node_geometry_util.hh"

#include "NOD_geo_field_to_grid.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "BLO_read_write.hh"

#ifdef WITH_OPENVDB
#  include "BKE_volume_grid_process.hh"
#endif

namespace blender {

namespace nodes::node_geo_field_to_grid_cc {

NODE_STORAGE_FUNCS(GeometryNodeFieldToGrid)
using ItemsAccessor = FieldToGridItemsAccessor;

namespace grid = bke::volume_grid;

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();

  const bNodeTree *tree = b.tree_or_null();
  const bNode *node = b.node_or_null();
  if (!node || !tree) {
    return;
  }
  const GeometryNodeFieldToGrid &storage = node_storage(*node);
  const eNodeSocketDatatype data_type = storage.data_type;

  b.add_input(data_type, "Topology"_ustr).structure_type(StructureType::Grid);

  const Span<GeometryNodeFieldToGridItem> items(storage.items, storage.items_num);
  for (const int i : items.index_range()) {
    const GeometryNodeFieldToGridItem &item = items[i];
    const eNodeSocketDatatype data_type = item.data_type;
    const UString name(item.name);
    const UString input_identifier(ItemsAccessor::input_socket_identifier_for_item(item));
    const UString output_identifier(ItemsAccessor::output_socket_identifier_for_item(item));

    b.add_input(data_type, name, input_identifier)
        .structure_type(StructureType::Field)
        .socket_name_ptr(&tree->id, *FieldToGridItemsAccessor::item_srna, &item, "name");
    b.add_output(data_type, name, output_identifier)
        .structure_type(StructureType::Grid)
        .align_with_previous()
        .description("Output grid with evaluated field values");
  }

  b.add_input<decl::Extend>(""_ustr, "__extend__"_ustr)
      .structure_type(StructureType::Field)
      .custom_draw(socket_items::ui::draw_extend_socket_fn<FieldToGridItemsAccessor>());
  b.add_output<decl::Extend>(""_ustr, "__extend__"_ustr)
      .structure_type(StructureType::Grid)
      .align_with_previous();
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_layout_ex(ui::Layout &layout, bContext *C, PointerRNA *ptr)
{
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *static_cast<bNode *>(ptr->data);
  if (ui::Layout *panel = layout.panel(C, "field_to_grid_items", false, IFACE_("Fields"))) {
    socket_items::ui::draw_items_list_with_operators<ItemsAccessor>(C, panel, tree, node);
    socket_items::ui::draw_active_item_props<ItemsAccessor>(tree, node, [&](PointerRNA *item_ptr) {
      panel->use_property_split_set(true);
      panel->use_property_decorate_set(false);
      panel->prop(item_ptr, "data_type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
    });
  }
}

static std::optional<eNodeSocketDatatype> node_type_for_socket_type(const bNodeSocket &socket)
{
  switch (socket.type) {
    case SOCK_FLOAT:
      return SOCK_FLOAT;
    case SOCK_BOOLEAN:
      return SOCK_BOOLEAN;
    case SOCK_INT:
      return SOCK_INT;
    case SOCK_VECTOR:
    case SOCK_RGBA:
      return SOCK_VECTOR;
    default:
      return std::nullopt;
  }
}

static void node_gather_link_search_ops(GatherLinkSearchOpParams &params)
{
  const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(
      params.other_socket());
  if (!data_type) {
    return;
  }
  if (params.in_out() == SOCK_IN) {
    params.add_item(IFACE_("Topology"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeFieldToGrid"_ustr);
      node_storage(node).data_type = *data_type;
      params.update_and_connect_available_socket(node, "Topology"_ustr);
    });
    params.add_item(IFACE_("Field"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeFieldToGrid"_ustr);
      const auto *item = socket_items::add_item_with_socket_type_and_name<ItemsAccessor>(
          params.node_tree, node, *data_type, params.socket.name);
      params.update_and_connect_available_socket_by_identifier(
          node, UString(FieldToGridItemsAccessor::input_socket_identifier_for_item(*item)));
    });
  }
  else {
    params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeFieldToGrid"_ustr);
      socket_items::add_item_with_socket_type_and_name<ItemsAccessor>(
          params.node_tree, node, *data_type, params.socket.name);
      params.update_and_connect_available_socket(node, UString(params.socket.name));
    });
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const GeometryNodeFieldToGrid &storage = node_storage(params.node());
  const Span<GeometryNodeFieldToGridItem> items(storage.items, storage.items_num);
  bke::GVolumeGrid topology_grid = params.extract_input<bke::GVolumeGrid>("Topology"_ustr);
  if (!topology_grid) {
    params.error_message_add(NodeWarningType::Error, "The topology grid input is required");
    params.set_default_remaining_outputs();
    return;
  }

  bke::VolumeTreeAccessToken tree_token;
  const openvdb::GridBase &topology_base = topology_grid->grid(tree_token);
  const openvdb::math::Transform &transform = topology_base.transform();

  Vector<int> required_items;
  for (const int i : items.index_range()) {
    if (params.output_is_required(
            UString(ItemsAccessor::output_socket_identifier_for_item(items[i]))))
    {
      required_items.append(i);
    }
  }

  Vector<fn::GField> fields;
  fields.reserve(required_items.size());
  for (const int i : required_items.index_range()) {
    const int item_i = required_items[i];
    const std::string identifier = ItemsAccessor::input_socket_identifier_for_item(items[item_i]);
    fields.append(params.extract_input<fn::GField>(UString(identifier)));
  }

  openvdb::MaskTree mask_tree;
  grid::to_typed_grid(topology_base,
                      [&](const auto &grid) { mask_tree.topologyUnion(grid.tree()); });

  Vector<bke::GVolumeGrid> output_grids(fields.size());
  evaluate_fields_to_grid(mask_tree, transform, fields, output_grids);

  for (const int i : required_items.index_range()) {
    const int item_i = required_items[i];
    const std::string identifier = ItemsAccessor::output_socket_identifier_for_item(items[item_i]);
    params.set_output(UString(identifier), std::move(output_grids[i]));
  }

#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  GeometryNodeFieldToGrid *data = MEM_new<GeometryNodeFieldToGrid>(__func__);
  data->data_type = SOCK_FLOAT;
  node->storage = data;
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<ItemsAccessor>(*node);
  MEM_delete(static_cast<GeometryNodeFieldToGrid *>(node->storage));
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const GeometryNodeFieldToGrid &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_new<GeometryNodeFieldToGrid>(__func__, dna::shallow_copy(src_storage));
  dst_node->storage = dst_storage;

  socket_items::copy_array<ItemsAccessor>(*src_node, *dst_node);
}

static void node_operators()
{
  socket_items::ops::make_common_operators<ItemsAccessor>();
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  return socket_items::try_add_item_via_any_extend_socket<ItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<ItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<ItemsAccessor>(&reader, node);
}

static const bNodeSocket *node_internally_linked_input(const bNodeTree & /*tree*/,
                                                       const bNode &node,
                                                       const bNodeSocket &output_socket)
{
  return node.input_by_identifier(output_socket.identifier_ustr());
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeFieldToGrid"_ustr);
  ntype.ui_name = "Field to Grid";
  ntype.ui_description =
      "Create new grids by evaluating new values on an existing volume grid topology";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  bke::node_type_storage(ntype, "GeometryNodeFieldToGrid", node_free_storage, node_copy_storage);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.register_operators = node_operators;
  ntype.insert_link = node_insert_link;
  ntype.ignore_inferred_input_socket_visibility = true;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.internally_linked_input = node_internally_linked_input;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace nodes::node_geo_field_to_grid_cc

namespace nodes {

StructRNA **FieldToGridItemsAccessor::item_srna = &RNA_GeometryNodeFieldToGridItem;

void FieldToGridItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  writer->write_string(item.name);
}

void FieldToGridItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace nodes
}  // namespace blender
