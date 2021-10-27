/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "DNA_mesh_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_bvhutils.h"
#include "BKE_mesh_sample.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

using namespace blender::bke::mesh_surface_sample;

namespace blender::nodes {

static void geo_node_raycast_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Target Geometry")
      .only_realized_data()
      .supported_type(GEO_COMPONENT_TYPE_MESH);

  b.add_input<decl::Vector>("Attribute").hide_value().supports_field();
  b.add_input<decl::Float>("Attribute", "Attribute_001").hide_value().supports_field();
  b.add_input<decl::Color>("Attribute", "Attribute_002").hide_value().supports_field();
  b.add_input<decl::Bool>("Attribute", "Attribute_003").hide_value().supports_field();
  b.add_input<decl::Int>("Attribute", "Attribute_004").hide_value().supports_field();

  b.add_input<decl::Vector>("Source Position").implicit_field();
  b.add_input<decl::Vector>("Ray Direction").default_value({0.0f, 0.0f, -1.0f}).supports_field();
  b.add_input<decl::Float>("Ray Length")
      .default_value(100.0f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .supports_field();

  b.add_output<decl::Bool>("Is Hit").dependent_field();
  b.add_output<decl::Vector>("Hit Position").dependent_field();
  b.add_output<decl::Vector>("Hit Normal").dependent_field();
  b.add_output<decl::Float>("Hit Distance").dependent_field();

  b.add_output<decl::Vector>("Attribute").dependent_field({1, 2, 3, 4, 5, 6});
  b.add_output<decl::Float>("Attribute", "Attribute_001").dependent_field({1, 2, 3, 4, 5, 6});
  b.add_output<decl::Color>("Attribute", "Attribute_002").dependent_field({1, 2, 3, 4, 5, 6});
  b.add_output<decl::Bool>("Attribute", "Attribute_003").dependent_field({1, 2, 3, 4, 5, 6});
  b.add_output<decl::Int>("Attribute", "Attribute_004").dependent_field({1, 2, 3, 4, 5, 6});
}

static void geo_node_raycast_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "data_type", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "mapping", 0, "", ICON_NONE);
}

static void geo_node_raycast_init(bNodeTree *UNUSED(tree), bNode *node)
{
  NodeGeometryRaycast *data = (NodeGeometryRaycast *)MEM_callocN(sizeof(NodeGeometryRaycast),
                                                                 __func__);
  data->mapping = GEO_NODE_RAYCAST_INTERPOLATED;
  data->data_type = CD_PROP_FLOAT;
  node->storage = data;
}

static void geo_node_raycast_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  const NodeGeometryRaycast &data = *(const NodeGeometryRaycast *)node->storage;
  const CustomDataType data_type = static_cast<CustomDataType>(data.data_type);

  bNodeSocket *socket_vector = (bNodeSocket *)BLI_findlink(&node->inputs, 1);
  bNodeSocket *socket_float = socket_vector->next;
  bNodeSocket *socket_color4f = socket_float->next;
  bNodeSocket *socket_boolean = socket_color4f->next;
  bNodeSocket *socket_int32 = socket_boolean->next;

  nodeSetSocketAvailability(socket_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(socket_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(socket_color4f, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(socket_boolean, data_type == CD_PROP_BOOL);
  nodeSetSocketAvailability(socket_int32, data_type == CD_PROP_INT32);

  bNodeSocket *out_socket_vector = (bNodeSocket *)BLI_findlink(&node->outputs, 4);
  bNodeSocket *out_socket_float = out_socket_vector->next;
  bNodeSocket *out_socket_color4f = out_socket_float->next;
  bNodeSocket *out_socket_boolean = out_socket_color4f->next;
  bNodeSocket *out_socket_int32 = out_socket_boolean->next;

  nodeSetSocketAvailability(out_socket_vector, data_type == CD_PROP_FLOAT3);
  nodeSetSocketAvailability(out_socket_float, data_type == CD_PROP_FLOAT);
  nodeSetSocketAvailability(out_socket_color4f, data_type == CD_PROP_COLOR);
  nodeSetSocketAvailability(out_socket_boolean, data_type == CD_PROP_BOOL);
  nodeSetSocketAvailability(out_socket_int32, data_type == CD_PROP_INT32);
}

static eAttributeMapMode get_map_mode(GeometryNodeRaycastMapMode map_mode)
{
  switch (map_mode) {
    case GEO_NODE_RAYCAST_INTERPOLATED:
      return eAttributeMapMode::INTERPOLATED;
    default:
    case GEO_NODE_RAYCAST_NEAREST:
      return eAttributeMapMode::NEAREST;
  }
}

static void raycast_to_mesh(IndexMask mask,
                            const Mesh &mesh,
                            const VArray<float3> &ray_origins,
                            const VArray<float3> &ray_directions,
                            const VArray<float> &ray_lengths,
                            const MutableSpan<bool> r_hit,
                            const MutableSpan<int> r_hit_indices,
                            const MutableSpan<float3> r_hit_positions,
                            const MutableSpan<float3> r_hit_normals,
                            const MutableSpan<float> r_hit_distances,
                            int &hit_count)
{
  BVHTreeFromMesh tree_data;
  BKE_bvhtree_from_mesh_get(&tree_data, &mesh, BVHTREE_FROM_LOOPTRI, 4);
  if (tree_data.tree == nullptr) {
    free_bvhtree_from_mesh(&tree_data);
    return;
  }

  for (const int i : mask) {
    const float ray_length = ray_lengths[i];
    const float3 ray_origin = ray_origins[i];
    const float3 ray_direction = ray_directions[i].normalized();

    BVHTreeRayHit hit;
    hit.index = -1;
    hit.dist = ray_length;
    if (BLI_bvhtree_ray_cast(tree_data.tree,
                             ray_origin,
                             ray_direction,
                             0.0f,
                             &hit,
                             tree_data.raycast_callback,
                             &tree_data) != -1) {
      hit_count++;
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
  }

  /* We shouldn't be rebuilding the BVH tree when calling this function in parallel. */
  BLI_assert(tree_data.cached);
  free_bvhtree_from_mesh(&tree_data);
}

class RaycastFunction : public fn::MultiFunction {
 private:
  GeometrySet target_;
  GeometryNodeRaycastMapMode mapping_;

  /** The field for data evaluated on the target geometry. */
  std::optional<GeometryComponentFieldContext> target_context_;
  std::unique_ptr<FieldEvaluator> target_evaluator_;
  const GVArray *target_data_ = nullptr;

  /* Always evaluate the target domain data on the point domain. Eventually this could be
   * exposed as an option or determined automatically from the field inputs in order to avoid
   * losing information if the target field is on a different domain. */
  const AttributeDomain domain_ = ATTR_DOMAIN_POINT;

  fn::MFSignature signature_;

 public:
  RaycastFunction(GeometrySet target, GField src_field, GeometryNodeRaycastMapMode mapping)
      : target_(std::move(target)), mapping_((GeometryNodeRaycastMapMode)mapping)
  {
    target_.ensure_owns_direct_data();
    this->evaluate_target_field(std::move(src_field));
    signature_ = create_signature();
    this->set_signature(&signature_);
  }

  fn::MFSignature create_signature()
  {
    blender::fn::MFSignatureBuilder signature{"Geometry Proximity"};
    signature.single_input<float3>("Source Position");
    signature.single_input<float3>("Ray Direction");
    signature.single_input<float>("Ray Length");
    signature.single_output<bool>("Is Hit");
    signature.single_output<float3>("Hit Position");
    signature.single_output<float3>("Hit Normal");
    signature.single_output<float>("Distance");
    if (target_data_) {
      signature.single_output("Attribute", target_data_->type());
    }
    return signature.build();
  }

  void call(IndexMask mask, fn::MFParams params, fn::MFContext UNUSED(context)) const override
  {
    /* Hit positions are always necessary for retrieving the attribute from the target if that
     * output is required, so always retrieve a span from the evaluator in that case (it's
     * expected that the evaluator is more likely to have a spare buffer that could be used). */
    MutableSpan<float3> hit_positions =
        (target_data_) ? params.uninitialized_single_output<float3>(4, "Hit Position") :
                         params.uninitialized_single_output_if_required<float3>(4, "Hit Position");

    Array<int> hit_indices;
    if (target_data_) {
      hit_indices.reinitialize(mask.min_array_size());
    }

    BLI_assert(target_.has_mesh());
    const Mesh &mesh = *target_.get_mesh_for_read();

    int hit_count = 0;
    raycast_to_mesh(mask,
                    mesh,
                    params.readonly_single_input<float3>(0, "Source Position"),
                    params.readonly_single_input<float3>(1, "Ray Direction"),
                    params.readonly_single_input<float>(2, "Ray Length"),
                    params.uninitialized_single_output_if_required<bool>(3, "Is Hit"),
                    hit_indices,
                    hit_positions,
                    params.uninitialized_single_output_if_required<float3>(5, "Hit Normal"),
                    params.uninitialized_single_output_if_required<float>(6, "Distance"),
                    hit_count);

    if (target_data_) {
      IndexMask hit_mask;
      Vector<int64_t> hit_mask_indices;
      if (hit_count < mask.size()) {
        /* Not all rays hit the target. Create a corrected mask to avoid transferring attribute
         * data to invalid indices. An alternative would be handling -1 indices in a separate case
         * in #MeshAttributeInterpolator, but since it already has an IndexMask in its constructor,
         * it's simpler to use that. */
        hit_mask_indices.reserve(hit_count);
        for (const int64_t i : mask) {
          if (hit_indices[i] != -1) {
            hit_mask_indices.append(i);
          }
          hit_mask = IndexMask(hit_mask_indices);
        }
      }
      else {
        hit_mask = mask;
      }

      GMutableSpan result = params.uninitialized_single_output_if_required(7, "Attribute");
      if (!result.is_empty()) {
        MeshAttributeInterpolator interp(&mesh, hit_mask, hit_positions, hit_indices);
        result.type().fill_assign_indices(result.type().default_value(), result.data(), mask);
        interp.sample_data(*target_data_, domain_, get_map_mode(mapping_), result);
      }
    }
  }

 private:
  void evaluate_target_field(GField src_field)
  {
    if (!src_field) {
      return;
    }
    const MeshComponent &mesh_component = *target_.get_component_for_read<MeshComponent>();
    target_context_.emplace(GeometryComponentFieldContext{mesh_component, domain_});
    const int domain_size = mesh_component.attribute_domain_size(domain_);
    target_evaluator_ = std::make_unique<FieldEvaluator>(*target_context_, domain_size);
    target_evaluator_->add(std::move(src_field));
    target_evaluator_->evaluate();
    target_data_ = &target_evaluator_->get_evaluated(0);
  }
};

static GField get_input_attribute_field(GeoNodeExecParams &params, const CustomDataType data_type)
{
  switch (data_type) {
    case CD_PROP_FLOAT:
      if (params.output_is_required("Attribute_001")) {
        return params.extract_input<Field<float>>("Attribute_001");
      }
      break;
    case CD_PROP_FLOAT3:
      if (params.output_is_required("Attribute")) {
        return params.extract_input<Field<float3>>("Attribute");
      }
      break;
    case CD_PROP_COLOR:
      if (params.output_is_required("Attribute_002")) {
        return params.extract_input<Field<ColorGeometry4f>>("Attribute_002");
      }
      break;
    case CD_PROP_BOOL:
      if (params.output_is_required("Attribute_003")) {
        return params.extract_input<Field<bool>>("Attribute_003");
      }
      break;
    case CD_PROP_INT32:
      if (params.output_is_required("Attribute_004")) {
        return params.extract_input<Field<int>>("Attribute_004");
      }
      break;
    default:
      BLI_assert_unreachable();
  }
  return {};
}

static void output_attribute_field(GeoNodeExecParams &params, GField field)
{
  switch (bke::cpp_type_to_custom_data_type(field.cpp_type())) {
    case CD_PROP_FLOAT: {
      params.set_output("Attribute_001", Field<float>(field));
      break;
    }
    case CD_PROP_FLOAT3: {
      params.set_output("Attribute", Field<float3>(field));
      break;
    }
    case CD_PROP_COLOR: {
      params.set_output("Attribute_002", Field<ColorGeometry4f>(field));
      break;
    }
    case CD_PROP_BOOL: {
      params.set_output("Attribute_003", Field<bool>(field));
      break;
    }
    case CD_PROP_INT32: {
      params.set_output("Attribute_004", Field<int>(field));
      break;
    }
    default:
      break;
  }
}

static void geo_node_raycast_exec(GeoNodeExecParams params)
{
  GeometrySet target = params.extract_input<GeometrySet>("Target Geometry");
  const NodeGeometryRaycast &data = *(const NodeGeometryRaycast *)params.node().storage;
  const GeometryNodeRaycastMapMode mapping = static_cast<GeometryNodeRaycastMapMode>(data.mapping);
  const CustomDataType data_type = static_cast<CustomDataType>(data.data_type);

  auto return_default = [&]() {
    params.set_output("Is Hit", fn::make_constant_field<bool>(false));
    params.set_output("Hit Position", fn::make_constant_field<float3>({0.0f, 0.0f, 0.0f}));
    params.set_output("Hit Normal", fn::make_constant_field<float3>({0.0f, 0.0f, 0.0f}));
    params.set_output("Hit Distance", fn::make_constant_field<float>(0.0f));
    attribute_math::convert_to_static_type(data_type, [&](auto dummy) {
      using T = decltype(dummy);
      output_attribute_field(params, fn::make_constant_field<T>(T()));
    });
  };

  if (target.is_empty()) {
    return return_default();
  }

  if (!target.has_mesh()) {
    return return_default();
  }

  if (target.get_mesh_for_read()->totpoly == 0) {
    params.error_message_add(NodeWarningType::Error, TIP_("The target mesh must have faces"));
    return return_default();
  }

  GField field = get_input_attribute_field(params, data_type);
  const bool do_attribute_transfer = bool(field);
  Field<float3> position_field = params.extract_input<Field<float3>>("Source Position");
  Field<float3> direction_field = params.extract_input<Field<float3>>("Ray Direction");
  Field<float> length_field = params.extract_input<Field<float>>("Ray Length");

  auto fn = std::make_unique<RaycastFunction>(std::move(target), std::move(field), mapping);
  auto op = std::make_shared<FieldOperation>(FieldOperation(
      std::move(fn),
      {std::move(position_field), std::move(direction_field), std::move(length_field)}));

  params.set_output("Is Hit", Field<bool>(op, 0));
  params.set_output("Hit Position", Field<float3>(op, 1));
  params.set_output("Hit Normal", Field<float3>(op, 2));
  params.set_output("Hit Distance", Field<float>(op, 3));
  if (do_attribute_transfer) {
    output_attribute_field(params, GField(op, 4));
  }
}

}  // namespace blender::nodes

void register_node_type_geo_raycast()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_RAYCAST, "Raycast", NODE_CLASS_GEOMETRY, 0);
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_init(&ntype, blender::nodes::geo_node_raycast_init);
  node_type_update(&ntype, blender::nodes::geo_node_raycast_update);
  node_type_storage(
      &ntype, "NodeGeometryRaycast", node_free_standard_storage, node_copy_standard_storage);
  ntype.declare = blender::nodes::geo_node_raycast_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_raycast_exec;
  ntype.draw_buttons = blender::nodes::geo_node_raycast_layout;
  nodeRegisterType(&ntype);
}
