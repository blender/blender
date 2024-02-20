/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_bvhutils.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_sample.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

#include "BLI_task.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_sample_nearest_surface_cc {

using namespace blender::bke::mesh_surface_sample;

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();

  b.add_input<decl::Geometry>("Mesh").supported_type(GeometryComponent::Type::Mesh);
  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node->custom1);
    b.add_input(data_type, "Value").hide_value().field_on_all();
  }
  b.add_input<decl::Int>("Group ID")
      .hide_value()
      .field_on_all()
      .description(
          "Splits the faces of the input mesh into groups which can be sampled individually");
  b.add_input<decl::Vector>("Sample Position").implicit_field(implicit_field_inputs::position);
  b.add_input<decl::Int>("Sample Group ID").hide_value().supports_field();

  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node->custom1);
    b.add_output(data_type, "Value").dependent_field({3, 4});
  }
  b.add_output<decl::Bool>("Is Valid")
      .dependent_field({3, 4})
      .description(
          "Whether the sampling was successfull. It can fail when the sampled group is empty");
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = CD_PROP_FLOAT;
}

static void node_gather_link_searches(GatherLinkSearchOpParams &params)
{
  const NodeDeclaration &declaration = *params.node_type().static_declaration;
  search_link_ops_for_declarations(params, declaration.inputs);

  const std::optional<eCustomDataType> type = bke::socket_type_to_custom_data_type(
      eNodeSocketDatatype(params.other_socket().type));
  if (type && *type != CD_PROP_STRING) {
    /* The input and output sockets have the same name. */
    params.add_item(IFACE_("Value"), [type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeSampleNearestSurface");
      node.custom1 = *type;
      params.update_and_connect_available_socket(node, "Value");
    });
  }
}

class SampleNearestSurfaceFunction : public mf::MultiFunction {
 private:
  GeometrySet source_;
  Array<BVHTreeFromMesh> bvh_trees_;
  VectorSet<int> group_indices_;

 public:
  SampleNearestSurfaceFunction(GeometrySet geometry, const Field<int> &group_id_field)
      : source_(std::move(geometry))
  {
    source_.ensure_owns_direct_data();
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Sample Nearest Surface", signature};
      builder.single_input<float3>("Position");
      builder.single_input<int>("Sample ID");
      builder.single_output<int>("Triangle Index");
      builder.single_output<float3>("Sample Position");
      builder.single_output<bool>("Is Valid", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);

    const Mesh &mesh = *source_.get_mesh();

    /* Compute group ids on mesh. */
    bke::MeshFieldContext field_context{mesh, bke::AttrDomain::Face};
    FieldEvaluator field_evaluator{field_context, mesh.faces_num};
    field_evaluator.add(group_id_field);
    field_evaluator.evaluate();
    const VArray<int> group_ids = field_evaluator.get_evaluated<int>(0);

    /* Compute index masks for groups. */
    IndexMaskMemory memory;
    const Vector<IndexMask> group_masks = IndexMask::from_group_ids(
        group_ids, memory, group_indices_);
    const int groups_num = group_masks.size();

    /* Construct BVH tree for each group. */
    bvh_trees_.reinitialize(groups_num);
    threading::parallel_for(IndexRange(groups_num), 16, [&](const IndexRange range) {
      for (const int group_i : range) {
        const IndexMask &group_mask = group_masks[group_i];
        BVHTreeFromMesh &bvh = bvh_trees_[group_i];
        if (group_mask.size() == mesh.faces_num) {
          BKE_bvhtree_from_mesh_get(&bvh, &mesh, BVHTREE_FROM_CORNER_TRIS, 2);
        }
        else {
          BKE_bvhtree_from_mesh_tris_init(mesh, group_mask, bvh);
        }
      }
    });
  }

  ~SampleNearestSurfaceFunction()
  {
    for (BVHTreeFromMesh &tree : bvh_trees_) {
      free_bvhtree_from_mesh(&tree);
    }
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    const VArray<float3> &positions = params.readonly_single_input<float3>(0, "Position");
    const VArray<int> &sample_ids = params.readonly_single_input<int>(1, "Sample ID");
    MutableSpan<int> triangle_index = params.uninitialized_single_output<int>(2, "Triangle Index");
    MutableSpan<float3> sample_position = params.uninitialized_single_output<float3>(
        3, "Sample Position");
    MutableSpan<bool> is_valid_span = params.uninitialized_single_output_if_required<bool>(
        4, "Is Valid");

    mask.foreach_index([&](const int i) {
      const float3 position = positions[i];
      const int sample_id = sample_ids[i];
      const int group_index = group_indices_.index_of_try(sample_id);
      if (group_index == -1) {
        triangle_index[i] = -1;
        sample_position[i] = float3(0, 0, 0);
        if (!is_valid_span.is_empty()) {
          is_valid_span[i] = false;
        }
        return;
      }
      const BVHTreeFromMesh &bvh = bvh_trees_[group_index];
      BVHTreeNearest nearest;
      nearest.dist_sq = FLT_MAX;
      BLI_bvhtree_find_nearest(
          bvh.tree, position, &nearest, bvh.nearest_callback, const_cast<BVHTreeFromMesh *>(&bvh));
      triangle_index[i] = nearest.index;
      sample_position[i] = nearest.co;
      if (!is_valid_span.is_empty()) {
        is_valid_span[i] = true;
      }
    });
  }

  ExecutionHints get_execution_hints() const override
  {
    ExecutionHints hints;
    hints.min_grain_size = 512;
    return hints;
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Mesh");
  const Mesh *mesh = geometry.get_mesh();
  if (mesh == nullptr) {
    params.set_default_remaining_outputs();
    return;
  }
  if (mesh->verts_num == 0) {
    params.set_default_remaining_outputs();
    return;
  }
  if (mesh->faces_num == 0) {
    params.error_message_add(NodeWarningType::Error, TIP_("The source mesh must have faces"));
    params.set_default_remaining_outputs();
    return;
  }

  auto nearest_op = FieldOperation::Create(
      std::make_shared<SampleNearestSurfaceFunction>(geometry,
                                                     params.extract_input<Field<int>>("Group ID")),
      {params.extract_input<Field<float3>>("Sample Position"),
       params.extract_input<Field<int>>("Sample Group ID")});
  Field<int> triangle_indices(nearest_op, 0);
  Field<float3> nearest_positions(nearest_op, 1);
  Field<bool> is_valid(nearest_op, 2);

  Field<float3> bary_weights = Field<float3>(FieldOperation::Create(
      std::make_shared<bke::mesh_surface_sample::BaryWeightFromPositionFn>(geometry),
      {nearest_positions, triangle_indices}));

  GField field = params.extract_input<GField>("Value");
  auto sample_op = FieldOperation::Create(
      std::make_shared<bke::mesh_surface_sample::BaryWeightSampleFn>(geometry, std::move(field)),
      {triangle_indices, bary_weights});

  params.set_output("Value", GField(sample_op));
  params.set_output("Is Valid", is_valid);
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
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SAMPLE_NEAREST_SURFACE, "Sample Nearest Surface", NODE_CLASS_GEOMETRY);
  ntype.initfunc = node_init;
  ntype.declare = node_declare;
  blender::bke::node_type_size_preset(&ntype, blender::bke::eNodeSizePreset::MIDDLE);
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = node_gather_link_searches;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_sample_nearest_surface_cc
