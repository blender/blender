/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_type_conversions.hh"
#include "BKE_volume_grid.hh"
#include "BKE_volume_openvdb.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_sample_grid_index_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);

  b.add_input(data_type, "Grid").hide_value();
  b.add_input<decl::Int>("X").supports_field();
  b.add_input<decl::Int>("Y").supports_field();
  b.add_input<decl::Int>("Z").supports_field();

  b.add_output(data_type, "Value").dependent_field({1, 2, 3});
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
  if (!U.experimental.use_new_volume_nodes) {
    return;
  }
  const std::optional<eNodeSocketDatatype> node_type = node_type_for_socket_type(
      params.other_socket());
  if (!node_type) {
    return;
  }
  if (params.in_out() == SOCK_IN) {
    params.add_item(IFACE_("Grid"), [node_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeSampleGridIndex");
      node.custom1 = *node_type;
      params.update_and_connect_available_socket(node, "Grid");
    });
    const eNodeSocketDatatype other_type = eNodeSocketDatatype(params.other_socket().type);
    if (params.node_tree().typeinfo->validate_link(other_type, SOCK_INT)) {
      params.add_item(IFACE_("X"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeSampleGridIndex");
        params.update_and_connect_available_socket(node, "X");
      });
      params.add_item(IFACE_("Y"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeSampleGridIndex");
        params.update_and_connect_available_socket(node, "Y");
      });
      params.add_item(IFACE_("Z"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeSampleGridIndex");
        params.update_and_connect_available_socket(node, "Z");
      });
    }
  }
  else {
    params.add_item(IFACE_("Value"), [node_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeSampleGridIndex");
      node.custom1 = *node_type;
      params.update_and_connect_available_socket(node, "Value");
    });
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = SOCK_FLOAT;
}

#ifdef WITH_OPENVDB

template<typename T>
void sample_grid(const bke::OpenvdbGridType<T> &grid,
                 const Span<int> x,
                 const Span<int> y,
                 const Span<int> z,
                 const IndexMask &mask,
                 MutableSpan<T> dst)
{
  using GridType = bke::OpenvdbGridType<T>;
  using GridValueT = typename GridType::ValueType;
  using AccessorT = typename GridType::ConstAccessor;
  using TraitsT = typename bke::VolumeGridTraits<T>;
  AccessorT accessor = grid.getConstAccessor();

  mask.foreach_index([&](const int64_t i) {
    GridValueT value = accessor.getValue(openvdb::Coord(x[i], y[i], z[i]));
    dst[i] = TraitsT::to_blender(value);
  });
}

template<typename Fn> void convert_to_static_type(const VolumeGridType type, const Fn &fn)
{
  switch (type) {
    case VOLUME_GRID_BOOLEAN:
      fn(bool());
      break;
    case VOLUME_GRID_FLOAT:
      fn(float());
      break;
    case VOLUME_GRID_INT:
      fn(int());
      break;
    case VOLUME_GRID_MASK:
      fn(bool());
      break;
    case VOLUME_GRID_VECTOR_FLOAT:
      fn(float3());
      break;
    default:
      break;
  }
}

class SampleGridIndexFunction : public mf::MultiFunction {
  bke::GVolumeGrid grid_;
  mf::Signature signature_;

 public:
  SampleGridIndexFunction(bke::GVolumeGrid grid) : grid_(std::move(grid))
  {
    BLI_assert(grid_);

    const std::optional<eNodeSocketDatatype> data_type = bke::grid_type_to_socket_type(
        grid_->grid_type());
    const CPPType *cpp_type = bke::socket_type_to_geo_nodes_base_cpp_type(*data_type);
    mf::SignatureBuilder builder{"Sample Grid Index", signature_};
    builder.single_input<int>("X");
    builder.single_input<int>("Y");
    builder.single_input<int>("Z");
    builder.single_output("Value", *cpp_type);
    this->set_signature(&signature_);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const final
  {
    const VArraySpan<int> x = params.readonly_single_input<int>(0, "X");
    const VArraySpan<int> y = params.readonly_single_input<int>(1, "Y");
    const VArraySpan<int> z = params.readonly_single_input<int>(2, "Z");
    GMutableSpan dst = params.uninitialized_single_output(3, "Value");

    bke::VolumeTreeAccessToken tree_token;
    convert_to_static_type(grid_->grid_type(), [&](auto dummy) {
      using T = decltype(dummy);
      sample_grid<T>(grid_.typed<T>().grid(tree_token), x, y, z, mask, dst.typed<T>());
    });
  }
};

#endif /* WITH_OPENVDB */

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  const bNode &node = params.node();
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node.custom1);

  bke::GVolumeGrid grid = params.extract_input<bke::GVolumeGrid>("Grid");
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }

  auto fn = std::make_shared<SampleGridIndexFunction>(std::move(grid));
  auto op = FieldOperation::Create(std::move(fn),
                                   {params.extract_input<Field<int>>("X"),
                                    params.extract_input<Field<int>>("Y"),
                                    params.extract_input<Field<int>>("Z")});

  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
  const CPPType &output_type = *bke::socket_type_to_geo_nodes_base_cpp_type(data_type);
  const GField output_field = conversions.try_convert(fn::GField(std::move(op)), output_type);
  params.set_output("Value", std::move(output_field));

#else
  node_geo_exec_with_missing_openvdb(params);
#endif
}

static const EnumPropertyItem *data_type_filter_fn(bContext * /*C*/,
                                                   PointerRNA * /*ptr*/,
                                                   PropertyRNA * /*prop*/,
                                                   bool *r_free)
{
  *r_free = true;
  return enum_items_filter(
      rna_enum_node_socket_data_type_items, [](const EnumPropertyItem &item) -> bool {
        return ELEM(item.value, SOCK_FLOAT, SOCK_INT, SOCK_BOOLEAN, SOCK_VECTOR);
      });
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
                    data_type_filter_fn);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SAMPLE_GRID_INDEX, "Sample Grid Index", NODE_CLASS_CONVERTER);
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_sample_grid_index_cc
