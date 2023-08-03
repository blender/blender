/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DEG_depsgraph_query.h"

#include "BKE_type_conversions.hh"
#include "BKE_volume.h"
#include "BKE_volume_openvdb.hh"

#include "BLI_virtual_array.hh"

#include "NOD_add_node_search.hh"
#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#ifdef WITH_OPENVDB
#  include <openvdb/openvdb.h>
#  include <openvdb/tools/Interpolation.h>
#endif

namespace blender::nodes::node_geo_sample_volume_cc {

NODE_STORAGE_FUNCS(NodeGeometrySampleVolume)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(CTX_N_(BLT_I18NCONTEXT_ID_ID, "Volume"))
      .translation_context(BLT_I18NCONTEXT_ID_ID)
      .supported_type(GeometryComponent::Type::Volume);

  std::string grid_socket_description = N_(
      "Expects a Named Attribute with the name of a Grid in the Volume");

  b.add_input<decl::Vector>(N_("Grid"), "Grid_Vector")
      .field_on_all()
      .hide_value()
      .description(grid_socket_description);
  b.add_input<decl::Float>(N_("Grid"), "Grid_Float")
      .field_on_all()
      .hide_value()
      .description(grid_socket_description);
  b.add_input<decl::Bool>(N_("Grid"), "Grid_Bool")
      .field_on_all()
      .hide_value()
      .description(grid_socket_description);
  b.add_input<decl::Int>(N_("Grid"), "Grid_Int")
      .field_on_all()
      .hide_value()
      .description(grid_socket_description);

  b.add_input<decl::Vector>(N_("Position")).implicit_field(implicit_field_inputs::position);

  b.add_output<decl::Vector>(N_("Value"), "Value_Vector").dependent_field({5});
  b.add_output<decl::Float>(N_("Value"), "Value_Float").dependent_field({5});
  b.add_output<decl::Bool>(N_("Value"), "Value_Bool").dependent_field({5});
  b.add_output<decl::Int>(N_("Value"), "Value_Int").dependent_field({5});
}

static void search_node_add_ops(GatherAddNodeSearchParams &params)
{
  if (!U.experimental.use_new_volume_nodes) {
    return;
  }
  blender::nodes::search_node_add_ops_for_basic_node(params);
}

static std::optional<eCustomDataType> other_socket_type_to_grid_type(
    const eNodeSocketDatatype type)
{
  switch (type) {
    case SOCK_FLOAT:
      return CD_PROP_FLOAT;
    case SOCK_VECTOR:
    case SOCK_RGBA:
      return CD_PROP_FLOAT3;
    case SOCK_BOOLEAN:
      return CD_PROP_BOOL;
    case SOCK_INT:
      return CD_PROP_INT32;
    default:
      return std::nullopt;
  }
}

static void search_link_ops(GatherLinkSearchOpParams &params)
{
  if (!U.experimental.use_new_volume_nodes) {
    return;
  }
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_back(1));
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_front(1));

  const std::optional<eCustomDataType> type = other_socket_type_to_grid_type(
      eNodeSocketDatatype(params.other_socket().type));
  if (!type) {
    return;
  }
  /* The input and output sockets have the same name. */
  params.add_item(IFACE_("Grid"), [type](LinkSearchOpParams &params) {
    bNode &node = params.add_node("GeometryNodeSampleVolume");
    node_storage(node).grid_type = *type;
    params.update_and_connect_available_socket(node, "Grid");
  });
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "grid_type", UI_ITEM_NONE, "", ICON_NONE);
  uiItemR(layout, ptr, "interpolation_mode", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySampleVolume *data = MEM_cnew<NodeGeometrySampleVolume>(__func__);
  data->grid_type = CD_PROP_FLOAT;
  data->interpolation_mode = GEO_NODE_SAMPLE_VOLUME_INTERPOLATION_MODE_TRILINEAR;
  node->storage = data;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const NodeGeometrySampleVolume &storage = node_storage(*node);
  const eCustomDataType grid_type = eCustomDataType(storage.grid_type);

  bNodeSocket *socket_value_geometry = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *socket_value_vector = socket_value_geometry->next;
  bNodeSocket *socket_value_float = socket_value_vector->next;
  bNodeSocket *socket_value_boolean = socket_value_float->next;
  bNodeSocket *socket_value_int32 = socket_value_boolean->next;

  bke::nodeSetSocketAvailability(ntree, socket_value_vector, grid_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, socket_value_float, grid_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, socket_value_boolean, grid_type == CD_PROP_BOOL);
  bke::nodeSetSocketAvailability(ntree, socket_value_int32, grid_type == CD_PROP_INT32);

  bNodeSocket *out_socket_value_vector = static_cast<bNodeSocket *>(node->outputs.first);
  bNodeSocket *out_socket_value_float = out_socket_value_vector->next;
  bNodeSocket *out_socket_value_boolean = out_socket_value_float->next;
  bNodeSocket *out_socket_value_int32 = out_socket_value_boolean->next;

  bke::nodeSetSocketAvailability(ntree, out_socket_value_vector, grid_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, out_socket_value_float, grid_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, out_socket_value_boolean, grid_type == CD_PROP_BOOL);
  bke::nodeSetSocketAvailability(ntree, out_socket_value_int32, grid_type == CD_PROP_INT32);
}

#ifdef WITH_OPENVDB

static const StringRefNull get_grid_name(GField &field)
{
  if (const auto *attribute_field_input = dynamic_cast<const AttributeFieldInput *>(&field.node()))
  {
    return attribute_field_input->attribute_name();
  }
  return "";
}

static const blender::CPPType *vdb_grid_type_to_cpp_type(const VolumeGridType grid_type)
{
  switch (grid_type) {
    case VOLUME_GRID_FLOAT:
      return &CPPType::get<float>();
    case VOLUME_GRID_VECTOR_FLOAT:
      return &CPPType::get<float3>();
    case VOLUME_GRID_INT:
      return &CPPType::get<int>();
    case VOLUME_GRID_BOOLEAN:
      return &CPPType::get<bool>();
    default:
      break;
  }
  return nullptr;
}

template<typename GridT>
void sample_grid(openvdb::GridBase::ConstPtr base_grid,
                 const Span<float3> positions,
                 const IndexMask &mask,
                 GMutableSpan dst,
                 const GeometryNodeSampleVolumeInterpolationMode interpolation_mode)
{
  using ValueT = typename GridT::ValueType;
  using AccessorT = typename GridT::ConstAccessor;
  const typename GridT::ConstPtr grid = openvdb::gridConstPtrCast<GridT>(base_grid);
  AccessorT accessor = grid->getConstAccessor();

  auto sample_data = [&](auto sampler) {
    mask.foreach_index([&](const int64_t i) {
      const float3 &pos = positions[i];
      ValueT value = sampler.wsSample(openvdb::Vec3R(pos.x, pos.y, pos.z));

      /* Special case for vector. */
      if constexpr (std::is_same_v<GridT, openvdb::VectorGrid>) {
        openvdb::Vec3f vec = static_cast<openvdb::Vec3f>(value);
        dst.typed<float3>()[i] = float3(vec.asV());
      }
      else {
        dst.typed<ValueT>()[i] = value;
      }
    });
  };

  switch (interpolation_mode) {
    case GEO_NODE_SAMPLE_VOLUME_INTERPOLATION_MODE_TRILINEAR: {
      openvdb::tools::GridSampler<AccessorT, openvdb::tools::BoxSampler> sampler(
          accessor, grid->transform());
      sample_data(sampler);
      break;
    }
    case GEO_NODE_SAMPLE_VOLUME_INTERPOLATION_MODE_TRIQUADRATIC: {
      openvdb::tools::GridSampler<AccessorT, openvdb::tools::QuadraticSampler> sampler(
          accessor, grid->transform());
      sample_data(sampler);
      break;
    }
    case GEO_NODE_SAMPLE_VOLUME_INTERPOLATION_MODE_NEAREST:
    default: {
      openvdb::tools::GridSampler<AccessorT, openvdb::tools::PointSampler> sampler(
          accessor, grid->transform());
      sample_data(sampler);
      break;
    }
  }
}

class SampleVolumeFunction : public mf::MultiFunction {
  openvdb::GridBase::ConstPtr base_grid_;
  VolumeGridType grid_type_;
  GeometryNodeSampleVolumeInterpolationMode interpolation_mode_;
  mf::Signature signature_;

 public:
  SampleVolumeFunction(openvdb::GridBase::ConstPtr base_grid,
                       GeometryNodeSampleVolumeInterpolationMode interpolation_mode)
      : base_grid_(std::move(base_grid)), interpolation_mode_(interpolation_mode)
  {
    grid_type_ = BKE_volume_grid_type_openvdb(*base_grid_);
    const CPPType *grid_cpp_type = vdb_grid_type_to_cpp_type(grid_type_);
    BLI_assert(grid_cpp_type != nullptr);
    mf::SignatureBuilder builder{"Sample Volume", signature_};
    builder.single_input<float3>("Position");
    builder.single_output("Value", *grid_cpp_type);
    this->set_signature(&signature_);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArraySpan<float3> positions = params.readonly_single_input<float3>(0, "Position");
    GMutableSpan dst = params.uninitialized_single_output(1, "Value");

    switch (grid_type_) {
      case VOLUME_GRID_FLOAT:
        sample_grid<openvdb::FloatGrid>(base_grid_, positions, mask, dst, interpolation_mode_);
        break;
      case VOLUME_GRID_INT:
        sample_grid<openvdb::Int32Grid>(base_grid_, positions, mask, dst, interpolation_mode_);
        break;
      case VOLUME_GRID_BOOLEAN:
        sample_grid<openvdb::BoolGrid>(base_grid_, positions, mask, dst, interpolation_mode_);
        break;
      case VOLUME_GRID_VECTOR_FLOAT:
        sample_grid<openvdb::VectorGrid>(base_grid_, positions, mask, dst, interpolation_mode_);
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  }
};

static GField get_input_attribute_field(GeoNodeExecParams &params, const eCustomDataType data_type)
{
  switch (data_type) {
    case CD_PROP_FLOAT:
      return params.extract_input<Field<float>>("Grid_Float");
    case CD_PROP_FLOAT3:
      return params.extract_input<Field<float3>>("Grid_Vector");
    case CD_PROP_BOOL:
      return params.extract_input<Field<bool>>("Grid_Bool");
    case CD_PROP_INT32:
      return params.extract_input<Field<int>>("Grid_Int");
    default:
      BLI_assert_unreachable();
  }
  return {};
}

static void output_attribute_field(GeoNodeExecParams &params, GField field)
{
  switch (bke::cpp_type_to_custom_data_type(field.cpp_type())) {
    case CD_PROP_FLOAT:
      params.set_output("Value_Float", Field<float>(field));
      break;
    case CD_PROP_FLOAT3:
      params.set_output("Value_Vector", Field<float3>(field));
      break;
    case CD_PROP_BOOL:
      params.set_output("Value_Bool", Field<bool>(field));
      break;
    case CD_PROP_INT32:
      params.set_output("Value_Int", Field<int>(field));
      break;
    default:
      break;
  }
}

#endif /* WITH_OPENVDB */

static void node_geo_exec(GeoNodeExecParams params)
{
#ifdef WITH_OPENVDB
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Volume");
  if (!geometry_set.has_volume()) {
    params.set_default_remaining_outputs();
    return;
  }
  const NodeGeometrySampleVolume &storage = node_storage(params.node());
  const eCustomDataType output_field_type = eCustomDataType(storage.grid_type);
  auto interpolation_mode = GeometryNodeSampleVolumeInterpolationMode(storage.interpolation_mode);

  GField grid_field = get_input_attribute_field(params, output_field_type);
  const StringRefNull grid_name = get_grid_name(grid_field);
  if (grid_name == "") {
    params.error_message_add(NodeWarningType::Error, TIP_("Grid name needs to be specified"));
    params.set_default_remaining_outputs();
    return;
  }

  const VolumeComponent *component = geometry_set.get_component<VolumeComponent>();
  const Volume *volume = component->get();
  BKE_volume_load(volume, DEG_get_bmain(params.depsgraph()));
  const VolumeGrid *volume_grid = BKE_volume_grid_find_for_read(volume, grid_name.c_str());
  if (volume_grid == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }
  openvdb::GridBase::ConstPtr base_grid = BKE_volume_grid_openvdb_for_read(volume, volume_grid);
  const VolumeGridType grid_type = BKE_volume_grid_type_openvdb(*base_grid);

  /* Check that the grid type is supported. */
  const CPPType *grid_cpp_type = vdb_grid_type_to_cpp_type(grid_type);
  if (grid_cpp_type == nullptr) {
    params.set_default_remaining_outputs();
    params.error_message_add(NodeWarningType::Error, TIP_("The grid type is unsupported"));
    return;
  }

  /* Use to the Nearest Neighbor sampler for Bool grids (no interpolation). */
  if (grid_type == VOLUME_GRID_BOOLEAN &&
      interpolation_mode != GEO_NODE_SAMPLE_VOLUME_INTERPOLATION_MODE_NEAREST)
  {
    interpolation_mode = GEO_NODE_SAMPLE_VOLUME_INTERPOLATION_MODE_NEAREST;
  }

  Field<float3> position_field = params.extract_input<Field<float3>>("Position");
  auto fn = std::make_shared<SampleVolumeFunction>(std::move(base_grid), interpolation_mode);
  auto op = FieldOperation::Create(std::move(fn), {position_field});
  GField output_field = GField(std::move(op));

  output_field = bke::get_implicit_type_conversions().try_convert(
      output_field, *bke::custom_data_type_to_cpp_type(output_field_type));

  output_attribute_field(params, std::move(output_field));
#else
  params.set_default_remaining_outputs();
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenVDB"));
#endif
}

}  // namespace blender::nodes::node_geo_sample_volume_cc

void register_node_type_geo_sample_volume()
{
  namespace file_ns = blender::nodes::node_geo_sample_volume_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SAMPLE_VOLUME, "Sample Volume", NODE_CLASS_CONVERTER);
  node_type_storage(
      &ntype, "NodeGeometrySampleVolume", node_free_standard_storage, node_copy_standard_storage);
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_add_node_search_ops = file_ns::search_node_add_ops;
  ntype.gather_link_search_ops = file_ns::search_link_ops;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
