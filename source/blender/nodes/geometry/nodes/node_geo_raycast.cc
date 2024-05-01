/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.hh"
#include "BKE_mesh_sample.hh"

#include "NOD_rna_define.hh"
#include "NOD_socket_search_link.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_raycast_cc {

using namespace blender::bke::mesh_surface_sample;

NODE_STORAGE_FUNCS(NodeGeometryRaycast)

static void node_declare(NodeDeclarationBuilder &b)
{
  const bNode *node = b.node_or_null();

  b.add_input<decl::Geometry>("Target Geometry")
      .only_realized_data()
      .supported_type(GeometryComponent::Type::Mesh);
  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node_storage(*node).data_type);
    /* TODO: Field interfacing depends on the offset of the next declarations! */
    b.add_input(data_type, "Attribute").hide_value().field_on_all();
  }

  b.add_input<decl::Vector>("Source Position").implicit_field(implicit_field_inputs::position);
  b.add_input<decl::Vector>("Ray Direction").default_value({0.0f, 0.0f, -1.0f}).supports_field();
  b.add_input<decl::Float>("Ray Length")
      .default_value(100.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .supports_field();

  b.add_output<decl::Bool>("Is Hit").dependent_field({2, 3, 4});
  b.add_output<decl::Vector>("Hit Position").dependent_field({2, 3, 4});
  b.add_output<decl::Vector>("Hit Normal").dependent_field({2, 3, 4});
  b.add_output<decl::Float>("Hit Distance").dependent_field({2, 3, 4});

  if (node != nullptr) {
    const eCustomDataType data_type = eCustomDataType(node_storage(*node).data_type);
    b.add_output(data_type, "Attribute").dependent_field({2, 3, 4});
  }
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", UI_ITEM_NONE, "", ICON_NONE);
  uiItemR(layout, ptr, "mapping", UI_ITEM_NONE, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometryRaycast *data = MEM_cnew<NodeGeometryRaycast>(__func__);
  data->mapping = GEO_NODE_RAYCAST_INTERPOLATED;
  data->data_type = CD_PROP_FLOAT;
  node->storage = data;
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
    params.add_item(IFACE_("Attribute"), [type](LinkSearchOpParams &params) {
      bNode &node = params.add_node("GeometryNodeRaycast");
      node_storage(node).data_type = *type;
      params.update_and_connect_available_socket(node, "Attribute");
    });
  }
}

static void raycast_to_mesh(const IndexMask &mask,
                            const Mesh &mesh,
                            const VArray<float3> &ray_origins,
                            const VArray<float3> &ray_directions,
                            const VArray<float> &ray_lengths,
                            const MutableSpan<bool> r_hit,
                            const MutableSpan<int> r_hit_indices,
                            const MutableSpan<float3> r_hit_positions,
                            const MutableSpan<float3> r_hit_normals,
                            const MutableSpan<float> r_hit_distances)
{
  BVHTreeFromMesh tree_data;
  BKE_bvhtree_from_mesh_get(&tree_data, &mesh, BVHTREE_FROM_CORNER_TRIS, 4);
  BLI_SCOPED_DEFER([&]() { free_bvhtree_from_mesh(&tree_data); });

  if (tree_data.tree == nullptr) {
    return;
  }
  /* We shouldn't be rebuilding the BVH tree when calling this function in parallel. */
  BLI_assert(tree_data.cached);

  mask.foreach_index([&](const int i) {
    const float ray_length = ray_lengths[i];
    const float3 ray_origin = ray_origins[i];
    const float3 ray_direction = ray_directions[i];

    BVHTreeRayHit hit;
    hit.index = -1;
    hit.dist = ray_length;
    if (BLI_bvhtree_ray_cast(tree_data.tree,
                             ray_origin,
                             ray_direction,
                             0.0f,
                             &hit,
                             tree_data.raycast_callback,
                             &tree_data) != -1)
    {
      if (!r_hit.is_empty()) {
        r_hit[i] = hit.index >= 0;
      }
      if (!r_hit_indices.is_empty()) {
        /* The caller must be able to handle invalid indices anyway, so don't clamp this value. */
        r_hit_indices[i] = hit.index;
      }
      if (!r_hit_positions.is_empty()) {
        r_hit_positions[i] = hit.co;
      }
      if (!r_hit_normals.is_empty()) {
        r_hit_normals[i] = hit.no;
      }
      if (!r_hit_distances.is_empty()) {
        r_hit_distances[i] = hit.dist;
      }
    }
    else {
      if (!r_hit.is_empty()) {
        r_hit[i] = false;
      }
      if (!r_hit_indices.is_empty()) {
        r_hit_indices[i] = -1;
      }
      if (!r_hit_positions.is_empty()) {
        r_hit_positions[i] = float3(0.0f, 0.0f, 0.0f);
      }
      if (!r_hit_normals.is_empty()) {
        r_hit_normals[i] = float3(0.0f, 0.0f, 0.0f);
      }
      if (!r_hit_distances.is_empty()) {
        r_hit_distances[i] = ray_length;
      }
    }
  });
}

class RaycastFunction : public mf::MultiFunction {
 private:
  GeometrySet target_;

 public:
  RaycastFunction(GeometrySet target) : target_(std::move(target))
  {
    target_.ensure_owns_direct_data();
    static const mf::Signature signature = []() {
      mf::Signature signature;
      mf::SignatureBuilder builder{"Raycast", signature};
      builder.single_input<float3>("Source Position");
      builder.single_input<float3>("Ray Direction");
      builder.single_input<float>("Ray Length");
      builder.single_output<bool>("Is Hit", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float3>("Hit Position", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float3>("Hit Normal", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<float>("Distance", mf::ParamFlag::SupportsUnusedOutput);
      builder.single_output<int>("Triangle Index", mf::ParamFlag::SupportsUnusedOutput);
      return signature;
    }();
    this->set_signature(&signature);
  }

  void call(const IndexMask &mask, mf::Params params, mf::Context /*context*/) const override
  {
    BLI_assert(target_.has_mesh());
    const Mesh &mesh = *target_.get_mesh();

    raycast_to_mesh(mask,
                    mesh,
                    params.readonly_single_input<float3>(0, "Source Position"),
                    params.readonly_single_input<float3>(1, "Ray Direction"),
                    params.readonly_single_input<float>(2, "Ray Length"),
                    params.uninitialized_single_output_if_required<bool>(3, "Is Hit"),
                    params.uninitialized_single_output_if_required<int>(7, "Triangle Index"),
                    params.uninitialized_single_output_if_required<float3>(4, "Hit Position"),
                    params.uninitialized_single_output_if_required<float3>(5, "Hit Normal"),
                    params.uninitialized_single_output_if_required<float>(6, "Distance"));
  }
};

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet target = params.extract_input<GeometrySet>("Target Geometry");
  const NodeGeometryRaycast &storage = node_storage(params.node());
  const GeometryNodeRaycastMapMode mapping = GeometryNodeRaycastMapMode(storage.mapping);

  if (target.is_empty()) {
    params.set_default_remaining_outputs();
    return;
  }

  if (!target.has_mesh()) {
    params.set_default_remaining_outputs();
    return;
  }

  if (target.get_mesh()->faces_num == 0) {
    params.error_message_add(NodeWarningType::Error, TIP_("The target mesh must have faces"));
    params.set_default_remaining_outputs();
    return;
  }

  static auto normalize_fn = mf::build::SI1_SO<float3, float3>(
      "Normalize",
      [](const float3 &v) { return math::normalize(v); },
      mf::build::exec_presets::AllSpanOrSingle());
  auto direction_op = FieldOperation::Create(
      normalize_fn, {params.extract_input<Field<float3>>("Ray Direction")});

  auto op = FieldOperation::Create(std::make_unique<RaycastFunction>(target),
                                   {params.extract_input<Field<float3>>("Source Position"),
                                    Field<float3>(direction_op),
                                    params.extract_input<Field<float>>("Ray Length")});

  Field<float3> hit_position(op, 1);
  params.set_output("Is Hit", Field<bool>(op, 0));
  params.set_output("Hit Position", hit_position);
  params.set_output("Hit Normal", Field<float3>(op, 2));
  params.set_output("Hit Distance", Field<float>(op, 3));

  if (!params.output_is_required("Attribute")) {
    return;
  }

  GField field = params.extract_input<GField>("Attribute");
  Field<int> triangle_index(op, 4);
  Field<float3> bary_weights;
  switch (mapping) {
    case GEO_NODE_RAYCAST_INTERPOLATED:
      bary_weights = Field<float3>(FieldOperation::Create(
          std::make_shared<bke::mesh_surface_sample::BaryWeightFromPositionFn>(target),
          {hit_position, triangle_index}));
      break;
    case GEO_NODE_RAYCAST_NEAREST:
      bary_weights = Field<float3>(FieldOperation::Create(
          std::make_shared<bke::mesh_surface_sample::CornerBaryWeightFromPositionFn>(target),
          {hit_position, triangle_index}));
      break;
  }
  auto sample_op = FieldOperation::Create(
      std::make_shared<bke::mesh_surface_sample::BaryWeightSampleFn>(std::move(target),
                                                                     std::move(field)),
      {triangle_index, bary_weights});
  params.set_output("Attribute", GField(sample_op));
}

static void node_rna(StructRNA *srna)
{
  static EnumPropertyItem mapping_items[] = {
      {GEO_NODE_RAYCAST_INTERPOLATED,
       "INTERPOLATED",
       0,
       "Interpolated",
       "Interpolate the attribute from the corners of the hit face"},
      {GEO_NODE_RAYCAST_NEAREST,
       "NEAREST",
       0,
       "Nearest",
       "Use the attribute value of the closest mesh element"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "mapping",
                    "Mapping",
                    "Mapping from the target geometry to hit points",
                    mapping_items,
                    NOD_storage_enum_accessors(mapping),
                    GEO_NODE_RAYCAST_INTERPOLATED);
  RNA_def_node_enum(srna,
                    "data_type",
                    "Data Type",
                    "Type of data stored in attribute",
                    rna_enum_attribute_type_items,
                    NOD_storage_enum_accessors(data_type),
                    CD_PROP_FLOAT,
                    enums::attribute_type_type_with_socket_fn);
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_RAYCAST, "Raycast", NODE_CLASS_GEOMETRY);
  bke::node_type_size_preset(&ntype, bke::eNodeSizePreset::Middle);
  ntype.initfunc = node_init;
  node_type_storage(
      &ntype, "NodeGeometryRaycast", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  ntype.gather_link_search_ops = node_gather_link_searches;
  nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_raycast_cc
