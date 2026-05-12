/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "BKE_mesh_sample.hh"
#include "BKE_type_conversions.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "GEO_reverse_uv_sampler.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_sample_uv_surface_cc {

using geometry::ReverseUVSampler;

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();

  b.add_input<decl::Geometry>("Mesh"_ustr)
      .supported_type(GeometryComponent::Type::Mesh)
      .description("Mesh whose UV map is used");
  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node->custom1);
    b.add_input(data_type, "Value"_ustr).hide_value().field_on_all();
  }
  b.add_input<decl::Vector>("UV Map"_ustr, "Source UV Map"_ustr)
      .hide_value()
      .field_on_all()
      .description("The mesh UV map to sample. Should not have overlapping faces");
  b.add_input<decl::Vector>("Sample UV"_ustr)
      .supports_field()
      .description("The coordinates to sample within the UV map")
      .structure_type(StructureType::Dynamic);

  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node->custom1);
    b.add_output(data_type, "Value"_ustr).dependent_field({3});
  }
  b.add_output<decl::Bool>("Is Valid"_ustr)
      .dependent_field({3})
      .description("Whether the node could find a single face to sample at the UV coordinate");
}

static void node_layout(ui::Layout &layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout.prop(ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().static_declaration;
  search_link_ops_for_declarations(params, declaration.inputs);
  search_link_ops_for_declarations(params, declaration.outputs);

  const std::optional<eCustomDataType> type = bke::socket_type_to_custom_data_type(
      eNodeSocketDatatype(params.other_socket().type));
  if (type && *type != CD_PROP_STRING) {
    /* The input and output sockets have the same name. */
    params.add_item(IFACE_("Value"), [type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeSampleUVSurface"_ustr);
      node.custom1 = *type;
      params.update_and_connect_available_socket(node, "Value"_ustr);
    });
  }
}

class ReverseUVSampleFunction : public mf::MultiFunction {
  GeometrySet source_;
  Field<float2> src_uv_map_field_;

  mutable CacheMutex mutex_;
  mutable std::optional<bke::MeshFieldContext> source_context_;
  mutable std::unique_ptr<FieldEvaluator> source_evaluator_;
  mutable VArraySpan<float2> source_uv_map_;
  mutable std::optional<ReverseUVSampler> reverse_uv_sampler_;

 public:
  ReverseUVSampleFunction(GeometrySet geometry, Field<float2> src_uv_map_field)
      : source_(std::move(geometry)), src_uv_map_field_(std::move(src_uv_map_field))
  {
    source_.ensure_owns_direct_data();

    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Sample UV Surface", signature};
      builder.single_input<float2>("Sample UV");
      builder.single_output<bool>("Is Valid", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<int>("Triangle Index", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float3>("Barycentric Weights", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArraySpan<float2> sample_uvs = params.readonly_single_input<float2>(0, "Sample UV");
    MutableSpan<bool> is_valid = params.uninitialized_single_output_if_required<bool>(1,
                                                                                      "Is Valid");
    MutableSpan<int> tri_index = params.uninitialized_single_output_if_required<int>(
        2, "Triangle Index");
    MutableSpan<float3> bary_weights = params.uninitialized_single_output_if_required<float3>(
        3, "Barycentric Weights");

    mask.foreach_index([&](const int i) {
      const ReverseUVSampler::Result result = reverse_uv_sampler_->sample(sample_uvs[i]);
      if (!is_valid.is_empty()) {
        is_valid[i] = result.type == ReverseUVSampler::ResultType::Ok;
      }
      if (!tri_index.is_empty()) {
        tri_index[i] = result.tri_index;
      }
      if (!bary_weights.is_empty()) {
        bary_weights[i] = result.bary_weights;
      }
    });
  }

  void hash_unique(UniqueHashBytes &hash) const override
  {
    static constexpr int8_t id = 0;
    hash.add(&id);
    hash.add(source_.get_mesh());
    fn::FieldHashDeep field_hash;
    hash.add(field_hash.ensure(src_uv_map_field_));
  }

  void prepare_for_execution() const override
  {
    mutex_.ensure([&]() {
      const Mesh &mesh = *source_.get_mesh();
      source_context_.emplace(bke::MeshFieldContext{mesh, AttrDomain::Corner});
      source_evaluator_ = std::make_unique<FieldEvaluator>(*source_context_, mesh.corners_num);
      source_evaluator_->add(src_uv_map_field_);
      source_evaluator_->evaluate();
      source_uv_map_ = source_evaluator_->get_evaluated<float2>(0);

      reverse_uv_sampler_.emplace(source_uv_map_, mesh.corner_tris());
    });
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Mesh"_ustr);
  const Mesh *mesh = geometry.get_mesh();
  if (mesh == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }
  if (mesh->faces_num == 0 && mesh->verts_num != 0) {
    params.error_message_add(NodeWarningType::Error, TIP_("The source mesh must have faces"));
    params.set_default_remaining_outputs();
    return;
  }

  /* Do reverse sampling of the UV map first. */
  const bke::DataTypeConversions &conversions = bke::get_implicit_type_conversions();
  Field<float2> source_uv_map = *conversions.try_convert<float2>(
      params.extract_input<Field<float3>>("Source UV Map"_ustr));

  auto sample_uv_value = params.extract_input<bke::SocketValueVariant>("Sample UV"_ustr);
  if (sample_uv_value.is_list()) {
    params.error_message_add(NodeWarningType::Error,
                             "Lists are not supported for \"Sample UV\" input");
  }
  if (sample_uv_value.is_volume_grid()) {
    params.error_message_add(NodeWarningType::Error,
                             "Volume grids are not supported for \"Sample UV\" input");
  }
  Field<float2> sample_uvs = *conversions.try_convert<float2>(
      sample_uv_value.extract<Field<float3>>());

  auto uv_op = FieldOperation::from(
      std::make_shared<ReverseUVSampleFunction>(geometry, std::move(source_uv_map)),
      {std::move(sample_uvs)});
  params.set_output("Is Valid"_ustr, Field<bool>(uv_op, 0));

  /* Use the output of the UV sampling to interpolate the mesh attribute. */
  GField field = params.extract_input<GField>("Value"_ustr);

  auto sample_op = FieldOperation::from(
      std::make_shared<bke::mesh_surface_sample::BaryWeightSampleFn>(std::move(geometry),
                                                                     std::move(field)),
      {Field<int>(uv_op, 1), Field<float3>(uv_op, 2)});
  params.set_output("Value"_ustr, GField(sample_op, 0));
}

static void node_rna(StructRNA *srna)
{
  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "",
                    rna_enum_attribute_type_items,
                    NOD_inline_enum_accessors(custom1),
                    CD_PROP_FLOAT,
                    enums::attribute_type_type_with_socket_fn);
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSampleUVSurface"_ustr, GEO_NODE_SAMPLE_UV_SURFACE);
  ntype.ui_name = "Sample UV Surface";
  ntype.ui_description =
      "Calculate the interpolated values of a mesh attribute at a UV coordinate";
  ntype.enum_name_legacy = "SAMPLE_UV_SURFACE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = node_gather_link_searches;
  bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_sample_uv_surface_cc
