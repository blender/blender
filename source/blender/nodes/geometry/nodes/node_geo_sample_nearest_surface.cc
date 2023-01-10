/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.h"
#include "BKE_mesh.h"
#include "BKE_mesh_sample.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "NOD_socket_search_link.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_sample_nearest_surface_cc {

using namespace blender::bke::mesh_surface_sample;

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);

  b.add_input<decl::Float>(N_("Value"), "Value_Float").hide_value().field_on_all();
  b.add_input<decl::Int>(N_("Value"), "Value_Int").hide_value().field_on_all();
  b.add_input<decl::Vector>(N_("Value"), "Value_Vector").hide_value().field_on_all();
  b.add_input<decl::Color>(N_("Value"), "Value_Color").hide_value().field_on_all();
  b.add_input<decl::Bool>(N_("Value"), "Value_Bool").hide_value().field_on_all();

  b.add_input<decl::Vector>(N_("Sample Position")).implicit_field(implicit_field_inputs::position);

  b.add_output<decl::Float>(N_("Value"), "Value_Float").dependent_field({6});
  b.add_output<decl::Int>(N_("Value"), "Value_Int").dependent_field({6});
  b.add_output<decl::Vector>(N_("Value"), "Value_Vector").dependent_field({6});
  b.add_output<decl::Color>(N_("Value"), "Value_Color").dependent_field({6});
  b.add_output<decl::Bool>(N_("Value"), "Value_Bool").dependent_field({6});
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

  nodeSetSocketAvailability(ntree, in_socket_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(ntree, in_socket_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(ntree, in_socket_color4f, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(ntree, in_socket_bool, data_type == CD_PROP_BOOL);
  nodeSetSocketAvailability(ntree, in_socket_int32, data_type == CD_PROP_INT32);

  bNodeSocket *out_socket_float = static_cast<bNodeSocket *>(node->outputs.first);
  bNodeSocket *out_socket_int32 = out_socket_float->next;
  bNodeSocket *out_socket_vector = out_socket_int32->next;
  bNodeSocket *out_socket_color4f = out_socket_vector->next;
  bNodeSocket *out_socket_bool = out_socket_color4f->next;

  nodeSetSocketAvailability(ntree, out_socket_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(ntree, out_socket_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(ntree, out_socket_color4f, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(ntree, out_socket_bool, data_type == CD_PROP_BOOL);
  nodeSetSocketAvailability(ntree, out_socket_int32, data_type == CD_PROP_INT32);
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
                                      const IndexMask mask,
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

/**
 * \note Multi-threading for this function is provided by the field evaluator. Since the #call
 * function could be called many times, calculate the data from the source geometry once and store
 * it for later.
 */
class SampleNearestSurfaceFunction : public mf::MultiFunction {
  GeometrySet source_;
  GField src_field_;

  /**
   * This function is meant to sample the surface of a mesh rather than take the value from
   * individual elements, so use the most complex domain, ensuring no information is lost. In the
   * future, it should be possible to use the most complex domain required by the field inputs, to
   * simplify sampling and avoid domain conversions.
   */
  eAttrDomain domain_ = ATTR_DOMAIN_CORNER;

  mf::Signature signature_;

  std::optional<bke::MeshFieldContext> source_context_;
  std::unique_ptr<FieldEvaluator> source_evaluator_;
  const GVArray *source_data_;

 public:
  SampleNearestSurfaceFunction(GeometrySet geometry, GField src_field)
      : source_(std::move(geometry)), src_field_(std::move(src_field))
  {
    source_.ensure_owns_direct_data();
    this->evaluate_source_field();

    mf::SignatureBuilder builder{"Sample Nearest Surface", signature_};
    builder.single_input<float3>("Position");
    builder.single_output("Value", src_field_.cpp_type());
    this->set_signature(&signature_);
  }

  void call(IndexMask mask, mf::MFParams params, mf::Context /*context*/) const override
  {
    const VArray<float3> &positions = params.readonly_single_input<float3>(0, "Position");
    GMutableSpan dst = params.uninitialized_single_output_if_required(1, "Value");

    const MeshComponent &mesh_component = *source_.get_component_for_read<MeshComponent>();
    BLI_assert(mesh_component.has_mesh());
    const Mesh &mesh = *mesh_component.get_for_read();
    BLI_assert(mesh.totpoly > 0);

    /* Find closest points on the mesh surface. */
    Array<int> looptri_indices(mask.min_array_size());
    Array<float3> sampled_positions(mask.min_array_size());
    get_closest_mesh_looptris(mesh, positions, mask, looptri_indices, {}, sampled_positions);

    MeshAttributeInterpolator interp(&mesh, mask, sampled_positions, looptri_indices);
    interp.sample_data(*source_data_, domain_, eAttributeMapMode::INTERPOLATED, dst);
  }

 private:
  void evaluate_source_field()
  {
    const Mesh &mesh = *source_.get_mesh_for_read();
    source_context_.emplace(bke::MeshFieldContext{mesh, domain_});
    const int domain_size = mesh.attributes().domain_size(domain_);
    source_evaluator_ = std::make_unique<FieldEvaluator>(*source_context_, domain_size);
    source_evaluator_->add(src_field_);
    source_evaluator_->evaluate();
    source_data_ = &source_evaluator_->get_evaluated(0);
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

  Field<float3> positions = params.extract_input<Field<float3>>("Sample Position");
  GField field = get_input_attribute_field(params, data_type);
  auto fn = std::make_shared<SampleNearestSurfaceFunction>(std::move(geometry), std::move(field));
  auto op = FieldOperation::Create(std::move(fn), {std::move(positions)});
  output_attribute_field(params, GField(std::move(op)));
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
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.gather_link_search_ops = file_ns::node_gather_link_searches;
  nodeRegisterType(&ntype);
}
