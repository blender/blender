/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "node_geometry_util.hh"

#include "BLI_math_matrix.hh"

#include "BKE_attribute_math.hh"
#include "BKE_pointcloud.hh"
#include "BKE_volume_grid.hh"
#include "BKE_volume_openvdb.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface.hh"
#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

#include "GEO_randomize.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/tree/NodeManager.h>
#endif

namespace blender::nodes::node_geo_grid_to_points_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }

  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_default_layout();

  b.add_output<decl::Geometry>("Points").description(
      "A point for each active voxel or tile in the grid");
  b.add_output(data_type, "Value").field_on_all().description("The grid's value at each voxel");

  auto &panel = b.add_panel("Voxel Index").default_closed(true);
  panel.add_output<decl::Int>("X").field_on_all().description(
      "X coordinate of the voxel in index space, or the minimum X coordinate of a tile");
  panel.add_output<decl::Int>("Y").field_on_all().description(
      "Y coordinate of the voxel in index space, or the minimum Y coordinate of a tile");
  panel.add_output<decl::Int>("Z").field_on_all().description(
      "Z coordinate of the voxel in index space, or the minimum Z coordinate of a tile");
  panel.add_output<decl::Bool>("Is Tile").field_on_all().description(
      "The point represents a tile (multiple voxels) rather than a single voxel");
  panel.add_output<decl::Int>("Extent").field_on_all().description(
      "The size of the tile or voxel. For individual voxels this is 1, for tiles this represents "
      "the cubic size of the tile");

  b.add_input(data_type, "Grid").hide_value().structure_type(StructureType::Grid);
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.use_property_split_set(true);
  layout.use_property_decorate_set(false);
  layout.prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
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
  const bNodeSocket &other_socket = params.other_socket();
  const StructureType structure_type = other_socket.runtime->inferred_structure_type;
  const eNodeSocketDatatype other_type = eNodeSocketDatatype(other_socket.type);

  if (params.in_out() == SOCK_IN) {
    if (ELEM(structure_type, StructureType::Grid, StructureType::Dynamic)) {
      const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(other_socket);
      if (data_type) {
        params.add_item(IFACE_("Grid"), [data_type](LinkSearchOpParams &params) {
          bNode &node = params.add_node("GeometryNodeGridToPoints");
          node.custom1 = *data_type;
          params.update_and_connect_available_socket(node, "Grid");
        });
      }
    }
  }
  else {
    if (params.node_tree().typeinfo->validate_link(SOCK_GEOMETRY, other_type)) {
      params.add_item(IFACE_("Points"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeGridToPoints");
        params.update_and_connect_available_socket(node, "Points");
      });
    }
    const std::optional<eNodeSocketDatatype> data_type = node_type_for_socket_type(other_socket);
    if (data_type) {
      params.add_item(IFACE_("Value"), [data_type](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeGridToPoints");
        node.custom1 = *data_type;
        params.update_and_connect_available_socket(node, "Value");
      });
    }
  }
}

#ifdef WITH_OPENVDB

template<typename LeafNodeT>
static void process_leaf_node(const LeafNodeT &leaf_node,
                              const float4x4 &grid_transform,
                              MutableSpan<float3> r_position,
                              MutableSpan<bool> r_is_tile,
                              MutableSpan<int> r_extent,
                              MutableSpan<int> r_coord_x,
                              MutableSpan<int> r_coord_y,
                              MutableSpan<int> r_coord_z,
                              MutableSpan<typename LeafNodeT::ValueType> r_value)
{
  using MaskT = LeafNodeT::NodeMaskType;

  r_is_tile.fill(false);
  r_extent.fill(1);

  const MaskT &mask = leaf_node.getValueMask();
  int iter_i = 0;
  for (auto iter = mask.beginOn(); iter; ++iter, ++iter_i) {
    const int i_in_node = iter.pos();
    const openvdb::Coord ijk = leaf_node.offsetToGlobalCoord(i_in_node);
    const float3 object_pos = math::transform_point(grid_transform,
                                                    float3(ijk.x(), ijk.y(), ijk.z()));
    r_position[iter_i] = object_pos;
    if (!r_coord_x.is_empty()) {
      r_coord_x[iter_i] = ijk.x();
    }
    if (!r_coord_y.is_empty()) {
      r_coord_y[iter_i] = ijk.y();
    }
    if (!r_coord_z.is_empty()) {
      r_coord_z[iter_i] = ijk.z();
    }
    if (!r_value.is_empty()) {
      r_value[iter_i] = leaf_node.getValue(i_in_node);
    }
  }
}

template<typename InternalNodeT>
static void process_internal_node(const InternalNodeT &internal_node,
                                  const float4x4 &grid_transform,
                                  MutableSpan<float3> r_position,
                                  MutableSpan<bool> r_is_tile,
                                  MutableSpan<int> r_extent,
                                  MutableSpan<int> r_coord_x,
                                  MutableSpan<int> r_coord_y,
                                  MutableSpan<int> r_coord_z,
                                  MutableSpan<typename InternalNodeT::ValueType> r_value)
{
  using MaskT = InternalNodeT::NodeMaskType;
  using UnionT = InternalNodeT::UnionType;

  r_is_tile.fill(true);
  r_extent.fill(InternalNodeT::ChildNodeType::DIM);

  int iter_i = 0;
  const UnionT *table = internal_node.getTable();
  const MaskT &mask = internal_node.getValueMask();
  for (auto iter = mask.beginOn(); iter; ++iter, ++iter_i) {
    const int i_in_node = iter.pos();
    const openvdb::Coord ijk = internal_node.offsetToGlobalCoord(i_in_node);
    const float3 object_pos = math::transform_point(grid_transform,
                                                    float3(ijk.x(), ijk.y(), ijk.z()));
    r_position[iter_i] = object_pos;
    if (!r_coord_x.is_empty()) {
      r_coord_x[iter_i] = ijk.x();
    }
    if (!r_coord_y.is_empty()) {
      r_coord_y[iter_i] = ijk.y();
    }
    if (!r_coord_z.is_empty()) {
      r_coord_z[iter_i] = ijk.z();
    }
    if (!r_value.is_empty()) {
      r_value[iter_i] = table[i_in_node].getValue();
    }
  }
}

template<typename TreeT>
static void process_tree(const TreeT &tree,
                         const float4x4 &grid_transform,
                         Array<float3> &r_position,
                         std::optional<Array<bool>> &r_is_tile,
                         std::optional<Array<int>> &r_extent,
                         std::optional<Array<int>> &r_coord_x,
                         std::optional<Array<int>> &r_coord_y,
                         std::optional<Array<int>> &r_coord_z,
                         std::optional<GArray<>> &r_value)
{
  using ValueT = TreeT::ValueType;
  using RootNodeT = TreeT::RootNodeType;
  using LeafNodeT = TreeT::LeafNodeType;

  openvdb::tree::NodeManager<const TreeT> node_manager(tree);

  /* Iterate over all nodes sequentially to figure out how many points need to be created. Also
   * compute an #IndexRange for each node indicating where the points for that node will be put in
   * the output. */
  int current_offset = 0;
  Map<const void *, IndexRange> slice_by_node;
  node_manager.foreachTopDown(
      [&]<typename NodeT>(const NodeT &node) {
        if constexpr (!std::is_same_v<NodeT, RootNodeT>) {
          using MaskT = NodeT::NodeMaskType;
          const MaskT &value_mask = node.getValueMask();
          const int values_num = value_mask.countOn();
          slice_by_node.add_new(&node, IndexRange(current_offset, values_num));
          current_offset += values_num;
        }
      },
      false);
  const int active_value_count = current_offset;

  /* Initialize all the required output arrays.  */
  r_position.reinitialize(active_value_count);
  if (r_is_tile.has_value()) {
    r_is_tile->reinitialize(active_value_count);
  }
  if (r_extent.has_value()) {
    r_extent->reinitialize(active_value_count);
  }
  if (r_coord_x.has_value()) {
    r_coord_x->reinitialize(active_value_count);
  }
  if (r_coord_y.has_value()) {
    r_coord_y->reinitialize(active_value_count);
  }
  if (r_coord_z.has_value()) {
    r_coord_z->reinitialize(active_value_count);
  }
  if (r_value.has_value()) {
    r_value->reinitialize(active_value_count);
  }

  /* Iterate over all grid nodes in parallel to compute all the required point attributes. */
  node_manager.foreachTopDown([&]<typename NodeT>(const NodeT &node) {
    if constexpr (std::is_same_v<NodeT, RootNodeT>) {
      /* Ignore. */
    }
    else {
      const IndexRange slice = slice_by_node.lookup(&node);
      if (slice.is_empty()) {
        return;
      }
      const MutableSpan<float3> r_position_slice = r_position.as_mutable_span().slice(slice);
      const MutableSpan<bool> r_is_tile_slice = r_is_tile.has_value() ?
                                                    r_is_tile->as_mutable_span().slice(slice) :
                                                    MutableSpan<bool>();
      const MutableSpan<int> r_extent_slice = r_extent.has_value() ?
                                                  r_extent->as_mutable_span().slice(slice) :
                                                  MutableSpan<int>();
      const MutableSpan<int> r_coord_x_slice = r_coord_x.has_value() ?
                                                   r_coord_x->as_mutable_span().slice(slice) :
                                                   MutableSpan<int>();
      const MutableSpan<int> r_coord_y_slice = r_coord_y.has_value() ?
                                                   r_coord_y->as_mutable_span().slice(slice) :
                                                   MutableSpan<int>();
      const MutableSpan<int> r_coord_z_slice = r_coord_z.has_value() ?
                                                   r_coord_z->as_mutable_span().slice(slice) :
                                                   MutableSpan<int>();
      const MutableSpan<ValueT> r_value_slice =
          r_value.has_value() ?
              MutableSpan<ValueT>(
                  static_cast<ValueT *>(r_value->as_mutable_span().slice(slice).data()),
                  slice.size()) :
              MutableSpan<ValueT>();
      if constexpr (std::is_same_v<NodeT, LeafNodeT>) {
        process_leaf_node<LeafNodeT>(node,
                                     grid_transform,
                                     r_position_slice,
                                     r_is_tile_slice,
                                     r_extent_slice,
                                     r_coord_x_slice,
                                     r_coord_y_slice,
                                     r_coord_z_slice,
                                     r_value_slice);
      }
      else {
        process_internal_node<NodeT>(node,
                                     grid_transform,
                                     r_position_slice,
                                     r_is_tile_slice,
                                     r_extent_slice,
                                     r_coord_x_slice,
                                     r_coord_y_slice,
                                     r_coord_z_slice,
                                     r_value_slice);
      }
    }
  });
}

#endif

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const bke::GVolumeGrid grid = params.extract_input<bke::GVolumeGrid>("Grid");
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }
  const eNodeSocketDatatype socket_type = eNodeSocketDatatype(params.node().custom1);
  const CPPType *cpp_type = bke::socket_type_to_geo_nodes_base_cpp_type(socket_type);
  if (!cpp_type) {
    params.set_default_remaining_outputs();
    return;
  }

  bke::VolumeTreeAccessToken tree_token;
  const openvdb::GridBase &grid_base = grid->grid(tree_token);
  const openvdb::TreeBase &tree_base = grid_base.baseTree();

  const float4x4 grid_transform = BKE_volume_transform_to_blender(grid_base.transform());

  std::optional<std::string> coord_x_id = params.get_output_anonymous_attribute_id_if_needed("X");
  std::optional<std::string> coord_y_id = params.get_output_anonymous_attribute_id_if_needed("Y");
  std::optional<std::string> coord_z_id = params.get_output_anonymous_attribute_id_if_needed("Z");
  std::optional<std::string> is_tile_id = params.get_output_anonymous_attribute_id_if_needed(
      "Is Tile");
  std::optional<std::string> extent_id = params.get_output_anonymous_attribute_id_if_needed(
      "Extent");
  std::optional<std::string> value_id = params.get_output_anonymous_attribute_id_if_needed(
      "Value");

  Array<float3> position_array;
  std::optional<Array<bool>> is_tile_array;
  std::optional<Array<int>> extent_array;
  std::optional<Array<int>> coord_x_array;
  std::optional<Array<int>> coord_y_array;
  std::optional<Array<int>> coord_z_array;
  std::optional<GArray<>> value_array;

  if (is_tile_id.has_value()) {
    is_tile_array.emplace();
  }
  if (extent_id.has_value()) {
    extent_array.emplace();
  }
  if (coord_x_id.has_value()) {
    coord_x_array.emplace();
  }
  if (coord_y_id.has_value()) {
    coord_y_array.emplace();
  }
  if (coord_z_id.has_value()) {
    coord_z_array.emplace();
  }
  if (value_id.has_value()) {
    value_array.emplace(*cpp_type);
  }

  bool valid_grid_type = false;
  bke::attribute_math::to_static_type(*cpp_type, [&]<typename ValueT>() {
    using type_traits = typename bke::VolumeGridTraits<ValueT>;
    using TreeT = typename type_traits::TreeType;

    if constexpr (!std::is_same_v<typename type_traits::BlenderType, void>) {
      valid_grid_type = true;
      const TreeT &tree = static_cast<const TreeT &>(tree_base);
      process_tree<TreeT>(tree,
                          grid_transform,
                          position_array,
                          is_tile_array,
                          extent_array,
                          coord_x_array,
                          coord_y_array,
                          coord_z_array,
                          value_array);

      BLI_assert(position_array.size() ==
                 tree_base.activeLeafVoxelCount() + tree_base.activeTileCount());
    }
  });

  if (!valid_grid_type) {
    params.set_default_remaining_outputs();
    return;
  }

  const int points_num = position_array.size();
  PointCloud *pointcloud = bke::pointcloud_new_no_attributes(points_num);
  MutableAttributeAccessor attributes = pointcloud->attributes_for_write();

  auto *position_attr = new ImplicitSharedValue<Array<float3>>(std::move(position_array));
  attributes.add<float3>("position",
                         AttrDomain::Point,
                         bke::AttributeInitShared(position_attr->data.data(), *position_attr));
  position_attr->remove_user_and_delete_if_last();
  if (coord_x_id.has_value()) {
    auto *coord_x_attr = new ImplicitSharedValue<Array<int>>(std::move(*coord_x_array));
    attributes.add<int>(*coord_x_id,
                        AttrDomain::Point,
                        bke::AttributeInitShared(coord_x_attr->data.data(), *coord_x_attr));
    coord_x_attr->remove_user_and_delete_if_last();
  }
  if (coord_y_id.has_value()) {
    auto *coord_y_attr = new ImplicitSharedValue<Array<int>>(std::move(*coord_y_array));
    attributes.add<int>(*coord_y_id,
                        AttrDomain::Point,
                        bke::AttributeInitShared(coord_y_attr->data.data(), *coord_y_attr));
    coord_y_attr->remove_user_and_delete_if_last();
  }
  if (coord_z_id.has_value()) {
    auto *coord_z_attr = new ImplicitSharedValue<Array<int>>(std::move(*coord_z_array));
    attributes.add<int>(*coord_z_id,
                        AttrDomain::Point,
                        bke::AttributeInitShared(coord_z_attr->data.data(), *coord_z_attr));
    coord_z_attr->remove_user_and_delete_if_last();
  }
  if (is_tile_id.has_value()) {
    auto *is_tile_attr = new ImplicitSharedValue<Array<bool>>(std::move(*is_tile_array));
    attributes.add<bool>(*is_tile_id,
                         AttrDomain::Point,
                         bke::AttributeInitShared(is_tile_attr->data.data(), *is_tile_attr));
    is_tile_attr->remove_user_and_delete_if_last();
  }
  if (extent_id.has_value()) {
    auto *extent_attr = new ImplicitSharedValue<Array<int>>(std::move(*extent_array));
    attributes.add<int>(*extent_id,
                        AttrDomain::Point,
                        bke::AttributeInitShared(extent_attr->data.data(), *extent_attr));
    extent_attr->remove_user_and_delete_if_last();
  }
  if (value_id.has_value()) {
    auto *value_attr = new ImplicitSharedValue<GArray<>>(std::move(*value_array));
    attributes.add(*value_id,
                   AttrDomain::Point,
                   bke::cpp_type_to_attribute_type(*cpp_type),
                   bke::AttributeInitShared(value_attr->data.data(), *value_attr));
    value_attr->remove_user_and_delete_if_last();
  }

  geometry::debug_randomize_point_order(pointcloud);
  params.set_output("Points", GeometrySet::from_pointcloud(pointcloud));

#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = SOCK_FLOAT;
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "Node socket data type",
                    rna_enum_node_socket_data_type_items,
                    NOD_inline_enum_accessors(custom1),
                    SOCK_FLOAT,
                    grid_socket_type_items_filter_fn);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeGridToPoints");
  ntype.ui_name = "Grid to Points";
  ntype.ui_description = "Generate a point cloud from a volume grid's active voxels";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.initfunc = node_init;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.declare = node_declare;
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grid_to_points_cc
