/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_userdef_types.h"

#include "BKE_type_conversions.hh"
#include "BKE_volume_grid.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

#ifdef WITH_OPENVDB
#  include <openvdb/tools/Interpolation.h>
#endif

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_sample_grid_cc {

enum class InterpolationMode {
  Nearest = 0,
  TriLinear = 1,
  TriQuadratic = 2,
};

static const EnumPropertyItem interpolation_mode_items[] = {
    {int(InterpolationMode::Nearest), "NEAREST", 0, N_("Nearest Neighbor"), ""},
    {int(InterpolationMode::TriLinear), "TRILINEAR", 0, N_("Trilinear"), ""},
    {int(InterpolationMode::TriQuadratic), "TRIQUADRATIC", 0, N_("Triquadratic"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();
  if (!node) {
    return;
  }
  const eNodeSocketDatatype data_type = eNodeSocketDatatype(node->custom1);

  b.add_input(data_type, "Grid").hide_value().structure_type(StructureType::Grid);
  b.add_input<decl::Vector>("Position").implicit_field(NODE_DEFAULT_INPUT_POSITION_FIELD);
  b.add_input<decl::Menu>("Interpolation")
      .static_items(interpolation_mode_items)
      .default_value(InterpolationMode::TriLinear)
      .optional_label()
      .description("How to interpolate the values between neighboring voxels");

  b.add_output(data_type, "Value").dependent_field({1});
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
  const std::optional<eNodeSocketDatatype> node_type = node_type_for_socket_type(
      params.other_socket());
  if (!node_type) {
    return;
  }
  if (params.in_out() == SOCK_IN) {
    params.add_item(IFACE_("Grid"), [node_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeSampleGrid");
      node.custom1 = *node_type;
      params.update_and_connect_available_socket(node, "Grid");
    });
    const eNodeSocketDatatype other_type = eNodeSocketDatatype(params.other_socket().type);
    if (params.node_tree().typeinfo->validate_link(other_type, SOCK_VECTOR)) {
      params.add_item(IFACE_("Position"), [](LinkSearchOpParams &params) {
        bNode &node = params.add_node("GeometryNodeSampleGrid");
        params.update_and_connect_available_socket(node, "Position");
      });
    }
  }
  else {
    params.add_item(IFACE_("Value"), [node_type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeSampleGrid");
      node.custom1 = *node_type;
      params.update_and_connect_available_socket(node, "Value");
    });
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

#ifdef WITH_OPENVDB

template<typename T>
void sample_grid(const bke::OpenvdbGridType<T> &grid,
                 const InterpolationMode interpolation,
                 const Span<float3> positions,
                 const IndexMask &mask,
                 MutableSpan<T> dst)
{
  using GridType = bke::OpenvdbGridType<T>;
  using GridValueT = typename GridType::ValueType;
  using AccessorT = typename GridType::ConstUnsafeAccessor;
  using TraitsT = typename bke::VolumeGridTraits<T>;
  AccessorT accessor = grid.getConstUnsafeAccessor();

  auto sample_data = [&](auto sampler_type_tag) {
    using Sampler = typename decltype(sampler_type_tag)::type;
    mask.foreach_index([&](const int64_t i) {
      const float3 &pos = positions[i];
      const openvdb::Vec3R world_pos(pos.x, pos.y, pos.z);
      const openvdb::Vec3R index_pos = grid.transform().worldToIndex(world_pos);
      GridValueT value;
      Sampler::sample(accessor, index_pos, value);
      dst[i] = TraitsT::to_blender(value);
    });
  };

  /* Use to the Nearest Neighbor sampler for Bool grids (no interpolation). */
  InterpolationMode real_interpolation = interpolation;
  if constexpr (std::is_same_v<T, bool>) {
    real_interpolation = InterpolationMode::Nearest;
  }
  switch (real_interpolation) {
    case InterpolationMode::TriLinear: {
      sample_data(TypeTag<openvdb::tools::BoxSampler>{});
      break;
    }
    case InterpolationMode::TriQuadratic: {
      sample_data(TypeTag<openvdb::tools::QuadraticSampler>{});
      break;
    }
    case InterpolationMode::Nearest: {
      sample_data(TypeTag<openvdb::tools::PointSampler>{});
      break;
    }
  }
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

class SampleGridFunction : public mf::MultiFunction {
  bke::GVolumeGrid grid_;
  InterpolationMode interpolation_;
  mf::Signature signature_;
  VolumeGridType grid_type_;
  /** Avoid accessing grid in #call function to avoid overhead for each multi-function call. */
  bke::VolumeTreeAccessToken tree_token_;
  const openvdb::GridBase *grid_base_ = nullptr;

 public:
  SampleGridFunction(bke::GVolumeGrid grid, InterpolationMode interpolation)
      : grid_(std::move(grid)), interpolation_(interpolation)
  {
    BLI_assert(grid_);

    const std::optional<eNodeSocketDatatype> data_type = bke::grid_type_to_socket_type(
        grid_->grid_type());
    const CPPType *cpp_type = bke::socket_type_to_geo_nodes_base_cpp_type(*data_type);
    mf::SignatureBuilder builder{"Sample Grid", signature_};
    builder.single_input<float3>("Position");
    builder.single_output("Value", *cpp_type);
    this->set_signature(&signature_);

    grid_base_ = &grid_->grid(tree_token_);
    grid_type_ = grid_->grid_type();
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArraySpan<float3> positions = params.readonly_single_input<float3>(0, "Position");
    GMutableSpan dst = params.uninitialized_single_output(1, "Value");

    bke::VolumeTreeAccessToken tree_token;
    convert_to_static_type(grid_type_, [&](auto dummy) {
      using T = decltype(dummy);
      sample_grid<T>(static_cast<const bke::OpenvdbGridType<T> &>(*grid_base_),
                     interpolation_,
                     positions,
                     mask,
                     dst.typed<T>());
    });
  }
};

#endif /* WITH_OPENVDB */

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  bke::GVolumeGrid grid = params.extract_input<bke::GVolumeGrid>("Grid");
  if (!grid) {
    params.set_default_remaining_outputs();
    return;
  }

  const auto interpolation = params.get_input<InterpolationMode>("Interpolation");
  bke::SocketValueVariant position = params.extract_input<bke::SocketValueVariant>("Position");

  std::string error_message;
  bke::SocketValueVariant output_value;
  if (!execute_multi_function_on_value_variant(
          std::make_shared<SampleGridFunction>(std::move(grid), interpolation),
          {&position},
          {&output_value},
          params.user_data(),
          error_message))
  {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, std::move(error_message));
    return;
  }

  params.set_output("Value", std::move(output_value));
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
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSampleGrid", GEO_NODE_SAMPLE_GRID);
  ntype.ui_name = "Sample Grid";
  ntype.ui_description = "Retrieve values from the specified volume grid";
  ntype.enum_name_legacy = "SAMPLE_GRID";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.gather_link_search_ops = node_gather_link_search_ops;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_sample_grid_cc
