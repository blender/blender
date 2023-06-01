/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.h"
#include "BKE_mesh.hh"
#include "BKE_mesh_sample.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_sample_nearest_surface_cc {

using namespace blender::bke::mesh_surface_sample;

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh").supported_type(GEO_COMPONENT_TYPE_MESH);

  b.add_input<decl::Float>("Value", "Value_Float").hide_value().field_on_all();
  b.add_input<decl::Int>("Value", "Value_Int").hide_value().field_on_all();
  b.add_input<decl::Vector>("Value", "Value_Vector").hide_value().field_on_all();
  b.add_input<decl::Color>("Value", "Value_Color").hide_value().field_on_all();
  b.add_input<decl::Bool>("Value", "Value_Bool").hide_value().field_on_all();

  b.add_input<decl::Vector>("Sample Position").implicit_field(implicit_field_inputs::position);

  b.add_output<decl::Float>("Value", "Value_Float").dependent_field({6});
  b.add_output<decl::Int>("Value", "Value_Int").dependent_field({6});
  b.add_output<decl::Vector>("Value", "Value_Vector").dependent_field({6});
  b.add_output<decl::Color>("Value", "Value_Color").dependent_field({6});
  b.add_output<decl::Bool>("Value", "Value_Bool").dependent_field({6});
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
}

static void node_update(bNodeTree *ntree, bNode *node)
{
  const eCustomDataType data_type = eCustomDataType(node->custom1);

  bNodeSocket *in_socket_mesh = static_cast<bNodeSocket *>(node->inputs.first);
  bNodeSocket *in_socket_float = in_socket_mesh->next;
  bNodeSocket *in_socket_int32 = in_socket_float->next;
  bNodeSocket *in_socket_vector = in_socket_int32->next;
  bNodeSocket *in_socket_color4f = in_socket_vector->next;
  bNodeSocket *in_socket_bool = in_socket_color4f->next;

  bke::nodeSetSocketAvailability(ntree, in_socket_vector, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, in_socket_float, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, in_socket_color4f, data_type == CD_PROP_COLOR);
  bke::nodeSetSocketAvailability(ntree, in_socket_bool, data_type == CD_PROP_BOOL);
  bke::nodeSetSocketAvailability(ntree, in_socket_int32, data_type == CD_PROP_INT32);

  bNodeSocket *out_socket_float = static_cast<bNodeSocket *>(node->outputs.first);
  bNodeSocket *out_socket_int32 = out_socket_float->next;
  bNodeSocket *out_socket_vector = out_socket_int32->next;
  bNodeSocket *out_socket_color4f = out_socket_vector->next;
  bNodeSocket *out_socket_bool = out_socket_color4f->next;

  bke::nodeSetSocketAvailability(ntree, out_socket_vector, data_type == CD_PROP_FLOAT3);
  bke::nodeSetSocketAvailability(ntree, out_socket_float, data_type == CD_PROP_FLOAT);
  bke::nodeSetSocketAvailability(ntree, out_socket_color4f, data_type == CD_PROP_COLOR);
  bke::nodeSetSocketAvailability(ntree, out_socket_bool, data_type == CD_PROP_BOOL);
  bke::nodeSetSocketAvailability(ntree, out_socket_int32, data_type == CD_PROP_INT32);
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().fixed_declaration;
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_back(2));
  search_link_ops_for_declarations(params, declaration.inputs.as_span().take_front(1));

  const std::optional<eCustomDataType> type = node_data_type_to_custom_data_type(
      (eNodeSocketDatatype)params.other_socket().type);
  if (type && *type != CD_PROP_STRING) {
    /* The input and output sockets have the same name. */
    params.add_item(IFACE_("Value"), [type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeSampleNearestSurface");
      node.custom1 = *type;
      params.update_and_connect_available_socket(node, "Value");
    });
  }
}

static void get_closest_mesh_looptris(const Mesh &mesh,
                                      const VArray<float3> &positions,
                                      const IndexMask &mask,
                                      const MutableSpan<int> r_looptri_indices,
                                      const MutableSpan<float> r_distances_sq,
                                      const MutableSpan<float3> r_positions)
{
  BLI_assert(mesh.totpoly > 0);
  BVHTreeFromMesh tree_data;
  BKE_bvhtree_from_mesh_get(&tree_data, &mesh, BVHTREE_FROM_LOOPTRI, 2);
  get_closest_in_bvhtree(
      tree_data, positions, mask, r_looptri_indices, r_distances_sq, r_positions);
  free_bvhtree_from_mesh(&tree_data);
}

class SampleNearestSurfaceFunction : public mf::MultiFunction {
  GeometrySet source_;

 public:
  SampleNearestSurfaceFunction(GeometrySet geometry) : source_(std::move(geometry))
  {
    source_.ensure_owns_direct_data();
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Sample Nearest Surface", signature};
      builder.single_input<float3>("Position");
      builder.single_output<int>("Triangle Index");
      builder.single_output<float3>("Sample Position");
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &positions = params.readonly_single_input<float3>(0, "Position");
    MutableSpan<int> triangle_index = params.uninitialized_single_output<int>(1, "Triangle Index");
    MutableSpan<float3> sample_position = params.uninitialized_single_output<float3>(
        2, "Sample Position");
    const Mesh &mesh = *source_.get_mesh_for_read();
    get_closest_mesh_looptris(mesh, positions, mask, triangle_index, {}, sample_position);
  }
};

static GField get_input_attribute_field(GeoNodeExecParams &params, const eCustomDataType data_type)
{
  switch (data_type) {
    case CD_PROP_FLOAT:
      return params.extract_input<Field<float>>("Value_Float");
    case CD_PROP_FLOAT3:
      return params.extract_input<Field<float3>>("Value_Vector");
    case CD_PROP_COLOR:
      return params.extract_input<Field<ColorGeometry4f>>("Value_Color");
    case CD_PROP_BOOL:
      return params.extract_input<Field<bool>>("Value_Bool");
    case CD_PROP_INT32:
      return params.extract_input<Field<int>>("Value_Int");
    default:
      BLI_assert_unreachable();
  }
  return {};
}

static void output_attribute_field(GeoNodeExecParams &params, GField field)
{
  switch (bke::cpp_type_to_custom_data_type(field.cpp_type())) {
    case CD_PROP_FLOAT: {
      params.set_output("Value_Float", Field<float>(field));
      break;
    }
    case CD_PROP_FLOAT3: {
      params.set_output("Value_Vector", Field<float3>(field));
      break;
    }
    case CD_PROP_COLOR: {
      params.set_output("Value_Color", Field<ColorGeometry4f>(field));
      break;
    }
    case CD_PROP_BOOL: {
      params.set_output("Value_Bool", Field<bool>(field));
      break;
    }
    case CD_PROP_INT32: {
      params.set_output("Value_Int", Field<int>(field));
      break;
    }
    default:
      break;
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Mesh");
  const eCustomDataType data_type = eCustomDataType(params.node().custom1);
  const Mesh *mesh = geometry.get_mesh_for_read();
  if (mesh == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }
  if (mesh->totvert == 0) {
    params.set_default_remaining_outputs();
    return;
  }
  if (mesh->totpoly == 0) {
    params.error_message_add(NodeWarningType::Error, TIP_("The source mesh must have faces"));
    params.set_default_remaining_outputs();
    return;
  }

  auto nearest_op = FieldOperation::Create(
      std::make_shared<SampleNearestSurfaceFunction>(geometry),
      {params.extract_input<Field<float3>>("Sample Position")});
  Field<int> triangle_indices(nearest_op, 0);
  Field<float3> nearest_positions(nearest_op, 1);

  Field<float3> bary_weights = Field<float3>(FieldOperation::Create(
      std::make_shared<bke::mesh_surface_sample::BaryWeightFromPositionFn>(geometry),
      {nearest_positions, triangle_indices}));

  GField field = get_input_attribute_field(params, data_type);
  auto sample_op = FieldOperation::Create(
      std::make_shared<bke::mesh_surface_sample::BaryWeightSampleFn>(geometry, std::move(field)),
      {triangle_indices, bary_weights});

  output_attribute_field(params, GField(sample_op));
}

}  // namespace blender::nodes::node_geo_sample_nearest_surface_cc

void register_node_type_geo_sample_nearest_surface()
{
  namespace file_ns = blender::nodes::node_geo_sample_nearest_surface_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SAMPLE_NEAREST_SURFACE, "Sample Nearest Surface", NODE_CLASS_GEOMETRY);
  ntype.initfunc = file_ns::node_init;
  ntype.updatefunc = file_ns::node_update;
  ntype.declare = file_ns::node_declare;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  nodeRegisterType(&ntype);
}
