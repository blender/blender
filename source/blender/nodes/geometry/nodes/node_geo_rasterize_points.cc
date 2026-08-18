/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_volume.hh"
#include "BKE_volume_grid.hh"

#include "BLI_array_utils.hh"
#include "BLI_generic_array.hh"
#include "BLI_math_matrix.hh"

#include "BLO_read_write.hh"

#include "GEO_points_to_volume.hh"

#include "NOD_geo_rasterize_points.hh"
#include "NOD_socket.hh"
#include "NOD_socket_items_blend.hh"
#include "NOD_socket_items_ops.hh"
#include "NOD_socket_items_ui.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"
#include "RNA_prototypes.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_rasterize_points {

NODE_STORAGE_FUNCS(NodeGeometryRasterizePoints)

enum class GridTransformMode {
  /* Uniform voxel scale with no translation and rotation. */
  VoxelSize,
  /* Full grid matrix input. */
  Matrix,
};

static EnumPropertyItem grid_transform_mode_items[] = {
    {int(GridTransformMode::VoxelSize),
     "VOXEL_SIZE",
     0,
     N_("Voxel Size"),
     N_("Use uniform voxel scale with no translation or rotation")},
    {int(GridTransformMode::Matrix),
     "MATRIX",
     0,
     N_("Matrix"),
     N_("Define grid transform with a matrix")},
    {0, nullptr, 0, nullptr, nullptr},
};

static EnumPropertyItem kernel_type_items[] = {
    {int(geometry::KernelType::NearestPoint),
     "CONSTANT",
     0,
     N_("Constant"),
     N_("Assign points to the closest voxel")},
    {int(geometry::KernelType::Linear),
     "LINEAR",
     0,
     N_("Linear"),
     N_("Linear falloff over the voxel range")},
    {int(geometry::KernelType::Quadratic),
     "QUADRATIC",
     0,
     N_("Quadratic B-Spline"),
     N_("Quadratic b-spline kernel over 1.5 voxels")},
    {int(geometry::KernelType::Cubic),
     "CONSTANT",
     0,
     N_("Cubic B-Spline"),
     N_("Cube b-spline kernel over 2 voxels")},
    {0, nullptr, 0, nullptr, nullptr},
};

static geometry::PointRasterizeType get_rasterize_item_type(
    const NodeGeometryRasterizePointsItemType type)
{
  switch (type) {
    case GEO_NODE_RASTERIZE_POINTS_ITEM_TYPE_SCALAR:
      return geometry::PointRasterizeType::Scalar;
    case GEO_NODE_RASTERIZE_POINTS_ITEM_TYPE_VECTOR:
      return geometry::PointRasterizeType::Vector;
  }
  BLI_assert_unreachable();
  return geometry::PointRasterizeType::Scalar;
}

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Geometry>("Points"_ustr);
  b.add_input<decl::Menu>("Grid Transform Mode"_ustr)
      .static_items(grid_transform_mode_items)
      .default_value(GridTransformMode::VoxelSize)
      .expanded()
      .optional_label()
      .description("Method of defining the grid transform");
  b.add_input<decl::Float>("Voxel Size"_ustr)
      .default_value(0.3f)
      .min(0.01f)
      .subtype(PROP_DISTANCE)
      .usage_by_menu("Grid Transform Mode"_ustr, int(GridTransformMode::VoxelSize));
  b.add_input<decl::Matrix>("Matrix"_ustr)
      .usage_by_menu("Grid Transform Mode"_ustr, int(GridTransformMode::Matrix));
  b.add_input<decl::Menu>("Kernel Type"_ustr)
      .static_items(kernel_type_items)
      .default_value(geometry::KernelType::Linear)
      .optional_label()
      .description("Kernel function for computing weights at each voxel");

  b.add_input<decl::Vector>("Position"_ustr)
      .default_input_type(NODE_DEFAULT_INPUT_POSITION_FIELD)
      .structure_type(StructureType::Field);

  b.add_separator();

  const bNode *node = b.node_or_null();
  const bNodeTree *tree = b.tree_or_null();
  if (node && tree) {
    const NodeGeometryRasterizePoints &storage = node_storage(*node);
    for (const int i : IndexRange(storage.items_num)) {
      const NodeGeometryRasterizePointsItem &item = storage.items[i];
      const StringRef name = item.name ? item.name : "";
      const std::string identifier = RasterizePointsItemsAccessor::socket_identifier_for_item(
          item);
      const geometry::PointRasterizeType rasterize_type = get_rasterize_item_type(
          NodeGeometryRasterizePointsItemType(item.type));
      const CPPType &attribute_type = geometry::points_rasterize_attribute_type(rasterize_type);
      const CPPType &grid_type = geometry::points_rasterize_grid_type(rasterize_type);
      const eNodeSocketDatatype input_type = *bke::geo_nodes_base_cpp_type_to_socket_type(
          attribute_type);
      const eNodeSocketDatatype output_type = *bke::geo_nodes_base_cpp_type_to_socket_type(
          grid_type);

      auto &input_decl = b.add_input(input_type, UString(name), UString(identifier));
      input_decl.socket_name_ptr(
          &tree->id, *RasterizePointsItemsAccessor::item_srna, &item, "name");
      input_decl.evaluated_geometry_field();

      b.add_output(output_type, UString(name), UString(identifier))
          .structure_type(StructureType::Grid)
          .align_with_previous();
    }
  }
  b.add_input<decl::Extend>(""_ustr, "__extend__"_ustr).structure_type(StructureType::Field);
  b.add_output<decl::Extend>(""_ustr, "__extend__"_ustr)
      .structure_type(StructureType::Field)
      .align_with_previous();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryRasterizePoints *data = MEM_new<NodeGeometryRasterizePoints>(__func__);
  data->next_identifier = 0;

  data->items = nullptr;
  data->items_num = 0;

  node->storage = data;
}

static void node_free_storage(bNode *node)
{
  socket_items::destruct_array<RasterizePointsItemsAccessor>(*node);
  MEM_delete(reinterpret_cast<NodeGeometryRasterizePoints *>(node->storage));
}

static void node_copy_storage(bNodeTree * /*dst_tree*/, bNode *dst_node, const bNode *src_node)
{
  const NodeGeometryRasterizePoints &src_storage = node_storage(*src_node);
  auto *dst_storage = MEM_new<NodeGeometryRasterizePoints>(__func__,
                                                           dna::shallow_copy(src_storage));
  dst_node->storage = dst_storage;

  socket_items::copy_array<RasterizePointsItemsAccessor>(*src_node, *dst_node);
}

static bool node_insert_link(bke::NodeInsertLinkParams &params)
{
  return socket_items::try_add_item_via_any_extend_socket<RasterizePointsItemsAccessor>(
      params.ntree, params.node, params.node, params.link);
}

static void node_operators()
{
  socket_items::ops::make_common_operators<RasterizePointsItemsAccessor>();
}

static void node_layout_ex(ui::Layout &layout, bContext *C, PointerRNA *ptr)
{
  bNodeTree &ntree = *reinterpret_cast<bNodeTree *>(ptr->owner_id);
  bNode &node = *static_cast<bNode *>(ptr->data);

  if (ui::Layout *panel = layout.panel(C, "rasterize_items", false, IFACE_("Items"))) {
    socket_items::ui::draw_items_list_with_operators<RasterizePointsItemsAccessor>(
        C, panel, ntree, node);
    socket_items::ui::draw_active_item_props<RasterizePointsItemsAccessor>(
        ntree, node, [&](PointerRNA *item_ptr) {
          panel->use_property_split_set(true);
          panel->use_property_decorate_set(false);
          panel->prop(item_ptr, "type", UI_ITEM_NONE, std::nullopt, ICON_NONE);
        });
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const NodeGeometryRasterizePoints &storage = node_storage(params.node());

  /* Same transform is used for the intermediate point data grid and all output grids. */
  const GridTransformMode grid_transform_mode = params.extract_input<GridTransformMode>(
      "Grid Transform Mode"_ustr);
  float4x4 grid_transform;
  switch (grid_transform_mode) {
    case GridTransformMode::VoxelSize:
      grid_transform = math::from_scale<float4x4>(
          float3(params.extract_input<float>("Voxel Size"_ustr)));
      break;
    case GridTransformMode::Matrix:
      grid_transform = params.extract_input<float4x4>("Matrix"_ustr);
      break;
  }
  const double determinant = math::determinant(grid_transform);
  if (!BKE_volume_grid_determinant_valid(determinant)) {
    params.set_default_remaining_outputs();
    return;
  }

  const geometry::KernelType kernel_type = params.get_input<geometry::KernelType>(
      "Kernel Type"_ustr);
  const GeometrySet geometry_set = params.extract_input<GeometrySet>("Points"_ustr);
  const Field<float3> position_field = params.extract_input<Field<float3>>("Position"_ustr);
  Vector<GField> fields_by_item;
  for (const int i : IndexRange(storage.items_num)) {
    const NodeGeometryRasterizePointsItem &item = storage.items[i];
    const std::string identifier = RasterizePointsItemsAccessor::socket_identifier_for_item(item);

    fields_by_item.append(params.extract_input<GField>(UString(identifier)));
  }

  const Array<GeometryComponent::Type> component_types = {GeometryComponent::Type::Mesh,
                                                          GeometryComponent::Type::PointCloud,
                                                          GeometryComponent::Type::Curve};
  Vector<int> points_offsets;
  for (const GeometryComponent::Type type : component_types) {
    const GeometryComponent *component = geometry_set.get_component(type);
    points_offsets.append(component ? component->attribute_domain_size(AttrDomain::Point) : 0);
  }
  points_offsets.append(0);
  const OffsetIndices points_by_component = offset_indices::accumulate_counts_to_offsets(
      points_offsets);
  const int points_num = points_by_component.total_size();
  if (points_num == 0) {
    params.set_default_remaining_outputs();
    return;
  }

  Array<float3> positions(points_num);
  Array<GArray<>> value_buffers(storage.items_num);
  Vector<geometry::PointDataGridAttributeInfo> point_data_grid_attributes;
  Vector<geometry::PointRasterizeAttributeInfo> point_rasterize_attributes;
  for (const int i : IndexRange(storage.items_num)) {
    const NodeGeometryRasterizePointsItem &item = storage.items[i];
    const geometry::PointRasterizeType rasterize_type = get_rasterize_item_type(
        NodeGeometryRasterizePointsItemType(item.type));
    const CPPType &attribute_type = geometry::points_rasterize_attribute_type(rasterize_type);

    value_buffers[i] = GArray<>(attribute_type, points_num);
    /* Note: Item name is unique and can be used as an attribute identifier. */
    point_data_grid_attributes.append({item.name, value_buffers[i].as_span()});
    point_rasterize_attributes.append({item.name, rasterize_type});
  }

  for (const int component_i : component_types.index_range()) {
    const GeometryComponent *component = geometry_set.get_component(component_types[component_i]);
    const IndexRange points = points_by_component[component_i];
    if (points.is_empty()) {
      continue;
    }
    BLI_assert(component != nullptr);

    const bke::GeometryFieldContext field_context{*component, AttrDomain::Point};
    fn::FieldEvaluator evaluator{field_context, points.size()};
    evaluator.add_with_destination(position_field, positions.as_mutable_span().slice(points));
    for (const int i : IndexRange(storage.items_num)) {
      const GField &field = fields_by_item[i];
      evaluator.add_with_destination(field, value_buffers[i].as_mutable_span().slice(points));
    }
    evaluator.evaluate();
  }

  geometry::MappedPointDataGrid point_data_grid = geometry::points_to_point_data_grid(
      positions, point_data_grid_attributes, grid_transform);

  // TODO only generate output grids that are actually needed.
  Array<bke::GVolumeGrid> output_attribute_grids(storage.items_num);
  geometry::points_rasterize(point_data_grid,
                             kernel_type,
                             point_rasterize_attributes,
                             grid_transform,
                             output_attribute_grids);
  for (const int i : IndexRange(storage.items_num)) {
    const NodeGeometryRasterizePointsItem &item = storage.items[i];
    const std::string identifier = RasterizePointsItemsAccessor::socket_identifier_for_item(item);
    if (output_attribute_grids[i]) {
      params.set_output(UString(identifier), std::move(output_attribute_grids[i]));
    }
    else {
      const std::string message = fmt::format(
          fmt::runtime(TIP_("Could not generate grid for \"{}\"")), item.name);
      params.error_message_add(NodeWarningType::Warning, message);
    }
  }
  params.set_default_remaining_outputs();

#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_blend_write(const bNodeTree & /*tree*/, const bNode &node, BlendWriter &writer)
{
  socket_items::blend_write<RasterizePointsItemsAccessor>(&writer, node);
}

static void node_blend_read(bNodeTree & /*tree*/, bNode &node, BlendDataReader &reader)
{
  socket_items::blend_read_data<RasterizePointsItemsAccessor>(&reader, node);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeRasterizePoints"_ustr);
  ntype.ui_name = "Rasterize Points";
  ntype.ui_description = "Create volume grids from points with a weighted sum";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.default_width = bke::NodeWidth::_200;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.blend_write_storage_content = node_blend_write;
  ntype.blend_data_read_storage_content = node_blend_read;
  blender::bke::node_type_storage(
      ntype, "NodeGeometryRasterizePoints", node_free_storage, node_copy_storage);
  ntype.insert_link = node_insert_link;
  // ntype.gather_link_search_ops = search_link_ops_for_volume_grid_node;
  ntype.draw_buttons_ex = node_layout_ex;
  ntype.register_operators = node_operators;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_rasterize_points

namespace blender::nodes {

StructRNA **RasterizePointsItemsAccessor::item_srna = &RNA_NodeGeometryRasterizePointsItem;

void RasterizePointsItemsAccessor::blend_write_item(BlendWriter *writer, const ItemT &item)
{
  writer->write_string(item.name);
}

void RasterizePointsItemsAccessor::blend_read_data_item(BlendDataReader *reader, ItemT &item)
{
  BLO_read_string(reader, &item.name);
}

}  // namespace blender::nodes

namespace blender {

blender::Span<NodeGeometryRasterizePointsItem> NodeGeometryRasterizePoints::items_span() const
{
  return blender::Span<NodeGeometryRasterizePointsItem>(items, items_num);
}

blender::MutableSpan<NodeGeometryRasterizePointsItem> NodeGeometryRasterizePoints::items_span()
{
  return blender::MutableSpan<NodeGeometryRasterizePointsItem>(items, items_num);
}

}  // namespace blender
