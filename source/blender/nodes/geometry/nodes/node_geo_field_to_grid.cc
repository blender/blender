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
#  include "BKE_volume_grid_fields.hh"
#  include "BKE_volume_grid_process.hh"
#endif

namespace blender::nodes::node_geo_field_to_grid_cc {

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
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(storage.data_type);

  b.add_input(data_type, "Topology").structure_type(StructureType::Grid);

  const Span<GeometryNodeFieldToGridItem> items(storage.items, storage.items_num);
  for (const int i : items.index_range()) {
    const GeometryNodeFieldToGridItem &item = items[i];
    const eNodeSocketDatatype data_type = eNodeSocketDatatype(item.data_type);
    const std::string input_identifier = ItemsAccessor::input_socket_identifier_for_item(item);
    const std::string output_identifier = ItemsAccessor::output_socket_identifier_for_item(item);

    b.add_input(data_type, item.name, input_identifier)
        .supports_field()
        .socket_name_ptr(&tree->id, FieldToGridItemsAccessor::item_srna, &item, "name");
    b.add_output(data_type, item.name, output_identifier)
        .structure_type(StructureType::Grid)
        .align_with_previous()
        .description("Output grid with evaluated field values");
  }

  b.add_input<decl::Extend>("", "__extend__").structure_type(StructureType::Field);
  b.add_output<decl::Extend>("", "__extend__")
      .structure_type(StructureType::Grid)
      .align_with_previous();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_layout_ex(uiLayout *layout, bContext *C, PointerRNA *ptr)
{
  bNodeTree &tree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *static_cast<bNode *>(ptr->data);
  if (uiLayout *panel = layout->panel(C, "field_to_grid_items", false, IFACE_("Fields"))) {
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
      bNode &node = params.add_node("GeometryNodeFieldToGrid");
      node_storage(node).data_type = *data_type;
      params.update_and_connect_available_socket(node, "Topology");
    });
    params.add_item(IFACE_("Field"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeFieldToGrid");
      socket_items::add_item_with_socket_type_and_name<ItemsAccessor>(
          params.node_tree, node, *data_type, params.socket.name);
      params.update_and_connect_available_socket(node, params.socket.name);
    });
  }
  else {
    params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeFieldToGrid");
      socket_items::add_item_with_socket_type_and_name<ItemsAccessor>(
          params.node_tree, node, *data_type, params.socket.name);
      params.update_and_connect_available_socket(node, params.socket.name);
    });
  }
}

#ifdef WITH_OPENVDB
BLI_NOINLINE static void process_leaf_node(const Span<fn::GField> fields,
                                           const openvdb::math::Transform &transform,
                                           const grid::LeafNodeMask &leaf_node_mask,
                                           const openvdb::CoordBBox &leaf_bbox,
                                           const grid::GetVoxelsFn get_voxels_fn,
                                           const Span<openvdb::GridBase::Ptr> output_grids)
{
  AlignedBuffer<8192, 8> allocation_buffer;
  ResourceScope scope;
  scope.allocator().provide_buffer(allocation_buffer);

  IndexMaskMemory memory;
  const IndexMask index_mask = IndexMask::from_predicate(
      IndexRange(grid::LeafNodeMask::SIZE),
      GrainSize(grid::LeafNodeMask::SIZE),
      memory,
      [&](const int64_t i) { return leaf_node_mask.isOn(i); });

  const openvdb::Coord any_voxel_in_leaf = leaf_bbox.min();
  MutableSpan<openvdb::Coord> voxels = scope.allocator().allocate_array<openvdb::Coord>(
      index_mask.min_array_size());
  get_voxels_fn(voxels);

  bke::VoxelFieldContext field_context{transform, voxels};
  fn::FieldEvaluator evaluator{field_context, &index_mask};

  Array<MutableSpan<bool>> boolean_outputs(fields.size());
  for (const int i : fields.index_range()) {
    const CPPType &type = fields[i].cpp_type();
    grid::to_typed_grid(*output_grids[i], [&](auto &grid) {
      using GridT = typename std::decay_t<decltype(grid)>;
      using ValueT = typename GridT::ValueType;

      auto &tree = grid.tree();
      auto *leaf_node = tree.probeLeaf(any_voxel_in_leaf);
      /* Should have been added before. */
      BLI_assert(leaf_node);

      /* Boolean grids are special because they encode the values as bitmask. */
      if constexpr (std::is_same_v<ValueT, bool>) {
        boolean_outputs[i] = scope.allocator().allocate_array<bool>(index_mask.min_array_size());
        evaluator.add_with_destination(fields[i], boolean_outputs[i]);
      }
      else {
        /* Write directly into the buffer of the output leaf node. */
        ValueT *buffer = leaf_node->buffer().data();
        evaluator.add_with_destination(fields[i],
                                       GMutableSpan(type, buffer, grid::LeafNodeMask::SIZE));
      }
    });
  }

  evaluator.evaluate();

  for (const int i : fields.index_range()) {
    if (!boolean_outputs[i].is_empty()) {
      grid::set_mask_leaf_buffer_from_bools(static_cast<openvdb::BoolGrid &>(*output_grids[i]),
                                            boolean_outputs[i],
                                            index_mask,
                                            voxels);
    }
  }
}

BLI_NOINLINE static void process_voxels(const Span<fn::GField> fields,
                                        const openvdb::math::Transform &transform,
                                        const Span<openvdb::Coord> voxels,
                                        const Span<openvdb::GridBase::Ptr> output_grids)
{
  const int64_t voxels_num = voxels.size();
  AlignedBuffer<8192, 8> allocation_buffer;
  ResourceScope scope;
  scope.allocator().provide_buffer(allocation_buffer);

  bke::VoxelFieldContext field_context{transform, voxels};
  fn::FieldEvaluator evaluator{field_context, voxels_num};

  Array<GMutableSpan> output_values(output_grids.size());
  for (const int i : fields.index_range()) {
    const CPPType &type = fields[i].cpp_type();
    output_values[i] = {type, scope.allocator().allocate_array(type, voxels_num), voxels_num};
    evaluator.add_with_destination(fields[i], output_values[i]);
  }
  evaluator.evaluate();

  for (const int i : fields.index_range()) {
    grid::set_grid_values(*output_grids[i], output_values[i], voxels);
  }
}

BLI_NOINLINE static void process_tiles(const Span<fn::GField> fields,
                                       const openvdb::math::Transform &transform,
                                       const Span<openvdb::CoordBBox> tiles,
                                       const Span<openvdb::GridBase::Ptr> output_grids)
{
  const int64_t tiles_num = tiles.size();
  AlignedBuffer<8192, 8> allocation_buffer;
  ResourceScope scope;
  scope.allocator().provide_buffer(allocation_buffer);

  bke::TilesFieldContext field_context{transform, tiles};
  fn::FieldEvaluator evaluator{field_context, tiles_num};

  Array<GMutableSpan> output_values(output_grids.size());
  for (const int i : fields.index_range()) {
    const CPPType &type = fields[i].cpp_type();
    output_values[i] = {type, scope.allocator().allocate_array(type, tiles_num), tiles_num};
    evaluator.add_with_destination(fields[i], output_values[i]);
  }
  evaluator.evaluate();

  for (const int i : fields.index_range()) {
    grid::set_tile_values(*output_grids[i], output_values[i], tiles);
  }
}

BLI_NOINLINE static void process_background(const Span<fn::GField> fields,
                                            const openvdb::math::Transform &transform,
                                            const Span<openvdb::GridBase::Ptr> output_grids)
{
  AlignedBuffer<256, 8> allocation_buffer;
  ResourceScope scope;
  scope.allocator().provide_buffer(allocation_buffer);

  static const openvdb::CoordBBox background_space = openvdb::CoordBBox::inf();
  bke::TilesFieldContext field_context(transform, Span<openvdb::CoordBBox>(&background_space, 1));
  fn::FieldEvaluator evaluator(field_context, 1);

  Array<GMutablePointer> output_values(output_grids.size());
  for (const int i : fields.index_range()) {
    const CPPType &type = fields[i].cpp_type();
    output_values[i] = {type, scope.allocator().allocate(type)};
    evaluator.add_with_destination(fields[i], GMutableSpan{type, output_values[i].get(), 1});
  }
  evaluator.evaluate();

  for (const int i : fields.index_range()) {
    grid::set_grid_background(*output_grids[i], output_values[i]);
  }
}
#endif

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const GeometryNodeFieldToGrid &storage = node_storage(params.node());
  const Span<GeometryNodeFieldToGridItem> items(storage.items, storage.items_num);
  bke::GVolumeGrid topology_grid = params.extract_input<bke::GVolumeGrid>("Topology");
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
    if (params.output_is_required(ItemsAccessor::output_socket_identifier_for_item(items[i]))) {
      required_items.append(i);
    }
  }

  Vector<fn::GField> fields(required_items.size());
  for (const int i : required_items.index_range()) {
    const int item_i = required_items[i];
    const std::string identifier = ItemsAccessor::input_socket_identifier_for_item(items[item_i]);
    fields[i] = params.extract_input<fn::GField>(identifier);
  }

  openvdb::MaskTree mask_tree;
  grid::to_typed_grid(topology_base,
                      [&](const auto &grid) { mask_tree.topologyUnion(grid.tree()); });

  Vector<openvdb::GridBase::Ptr> output_grids(required_items.size());
  for (const int i : required_items.index_range()) {
    const int item_i = required_items[i];
    const eNodeSocketDatatype socket_type = eNodeSocketDatatype(items[item_i].data_type);
    const VolumeGridType grid_type = *bke::socket_type_to_grid_type(socket_type);
    output_grids[i] = grid::create_grid_with_topology(mask_tree, transform, grid_type);
  }

  grid::parallel_grid_topology_tasks(
      mask_tree,
      [&](const grid::LeafNodeMask &leaf_node_mask,
          const openvdb::CoordBBox &leaf_bbox,
          const grid::GetVoxelsFn get_voxels_fn) {
        process_leaf_node(
            fields, transform, leaf_node_mask, leaf_bbox, get_voxels_fn, output_grids);
      },
      [&](const Span<openvdb::Coord> voxels) {
        process_voxels(fields, transform, voxels, output_grids);
      },
      [&](const Span<openvdb::CoordBBox> tiles) {
        process_tiles(fields, transform, tiles, output_grids);
      });

  process_background(fields, transform, output_grids);

  for (const int i : required_items.index_range()) {
    const int item_i = required_items[i];
    const std::string identifier = ItemsAccessor::output_socket_identifier_for_item(items[item_i]);
    params.set_output(identifier, bke::GVolumeGrid(std::move(output_grids[i])));
  }

#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  GeometryNodeFieldToGrid *data = MEM_callocN<GeometryNodeFieldToGrid>(__func__);
  data->data_type = SOCK_FLOAT;
  node->storage = data;
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<ItemsAccessor>(*node);
  MEM_freeN(node->storage);
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const GeometryNodeFieldToGrid &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_dupallocN<GeometryNodeFieldToGrid>(__func__, src_storage);
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
  return node.input_by_identifier(output_socket.identifier);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeFieldToGrid");
  ntype.ui_name = "Field to Grid";
  ntype.ui_description =
      "Create new grids by evaluating new values on an existing volume grid topology";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  blender::bke::node_type_storage(
      ntype, "GeometryNodeFieldToGrid", node_free_storage, node_copy_storage);
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
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_field_to_grid_cc

namespace blender::nodes {

StructRNA *FieldToGridItemsAccessor::item_srna = &RNA_GeometryNodeFieldToGridItem;

void FieldToGridItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  BLO_write_string(writer, item.name);
}

void FieldToGridItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace blender::nodes
