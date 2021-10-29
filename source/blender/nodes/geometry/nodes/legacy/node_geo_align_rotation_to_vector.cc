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

#include "BLI_math_rotation.h"
#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_align_rotation_to_vector_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::String>(N_("Factor"));
  b.add_input<decl::Float>(N_("Factor"), "Factor_001")
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR);
  b.add_input<decl::String>(N_("Vector"));
  b.add_input<decl::Vector>(N_("Vector"), "Vector_001")
      .default_value({0.0, 0.0, 1.0})
      .subtype(PROP_ANGLE);
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void geo_node_align_rotation_to_vector_layout(uiLayout *layout,
                                                     bContext *UNUSED(C),
                                                     PointerRNA *ptr)
{
  uiItemR(layout, ptr, "axis", UI_ITEM_R_EXPAND, nullptr, ICON_NONE);
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "pivot_axis", 0, IFACE_("Pivot"), ICON_NONE);
  uiLayout *col = uiLayoutColumn(layout, false);
  uiItemR(col, ptr, "input_type_factor", 0, IFACE_("Factor"), ICON_NONE);
  uiItemR(col, ptr, "input_type_vector", 0, IFACE_("Vector"), ICON_NONE);
}

static void geo_node_align_rotation_to_vector_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryAlignRotationToVector *node_storage = (NodeGeometryAlignRotationToVector *)
      MEM_callocN(sizeof(NodeGeometryAlignRotationToVector), __func__);

  node_storage->axis = GEO_NODE_ALIGN_ROTATION_TO_VECTOR_AXIS_X;
  node_storage->input_type_factor = GEO_NODE_ATTRIBUTE_INPUT_FLOAT;
  node_storage->input_type_vector = GEO_NODE_ATTRIBUTE_INPUT_VECTOR;

  node->storage = node_storage;
}

static void geo_node_align_rotation_to_vector_update(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometryAlignRotationToVector *node_storage = (NodeGeometryAlignRotationToVector *)
                                                        node->storage;
  update_attribute_input_socket_availabilities(
      *node, "Factor", (GeometryNodeAttributeInputMode)node_storage->input_type_factor);
  update_attribute_input_socket_availabilities(
      *node, "Vector", (GeometryNodeAttributeInputMode)node_storage->input_type_vector);
}

static void align_rotations_auto_pivot(const VArray<float3> &vectors,
                                       const VArray<float> &factors,
                                       const float3 local_main_axis,
                                       const MutableSpan<float3> rotations)
{
  threading::parallel_for(IndexRange(vectors.size()), 128, [&](IndexRange range) {
    for (const int i : range) {
      const float3 vector = vectors[i];
      if (is_zero_v3(vector)) {
        continue;
      }

      float old_rotation[3][3];
      eul_to_mat3(old_rotation, rotations[i]);
      float3 old_axis;
      mul_v3_m3v3(old_axis, old_rotation, local_main_axis);

      const float3 new_axis = vector.normalized();
      float3 rotation_axis = float3::cross_high_precision(old_axis, new_axis);
      if (is_zero_v3(rotation_axis)) {
        /* The vectors are linearly dependent, so we fall back to another axis. */
        rotation_axis = float3::cross_high_precision(old_axis, float3(1, 0, 0));
        if (is_zero_v3(rotation_axis)) {
          /* This is now guaranteed to not be zero. */
          rotation_axis = float3::cross_high_precision(old_axis, float3(0, 1, 0));
        }
      }

      const float full_angle = angle_normalized_v3v3(old_axis, new_axis);
      const float angle = factors[i] * full_angle;

      float rotation[3][3];
      axis_angle_to_mat3(rotation, rotation_axis, angle);

      float new_rotation_matrix[3][3];
      mul_m3_m3m3(new_rotation_matrix, rotation, old_rotation);

      float3 new_rotation;
      mat3_to_eul(new_rotation, new_rotation_matrix);

      rotations[i] = new_rotation;
    }
  });
}

static void align_rotations_fixed_pivot(const VArray<float3> &vectors,
                                        const VArray<float> &factors,
                                        const float3 local_main_axis,
                                        const float3 local_pivot_axis,
                                        const MutableSpan<float3> rotations)
{
  if (local_main_axis == local_pivot_axis) {
    /* Can't compute any meaningful rotation angle in this case. */
    return;
  }

  threading::parallel_for(IndexRange(vectors.size()), 128, [&](IndexRange range) {
    for (const int i : range) {
      const float3 vector = vectors[i];
      if (is_zero_v3(vector)) {
        continue;
      }

      float old_rotation[3][3];
      eul_to_mat3(old_rotation, rotations[i]);
      float3 old_axis;
      mul_v3_m3v3(old_axis, old_rotation, local_main_axis);
      float3 pivot_axis;
      mul_v3_m3v3(pivot_axis, old_rotation, local_pivot_axis);

      float full_angle = angle_signed_on_axis_v3v3_v3(vector, old_axis, pivot_axis);
      if (full_angle > M_PI) {
        /* Make sure the point is rotated as little as possible. */
        full_angle -= 2.0f * M_PI;
      }
      const float angle = factors[i] * full_angle;

      float rotation[3][3];
      axis_angle_to_mat3(rotation, pivot_axis, angle);

      float new_rotation_matrix[3][3];
      mul_m3_m3m3(new_rotation_matrix, rotation, old_rotation);

      float3 new_rotation;
      mat3_to_eul(new_rotation, new_rotation_matrix);

      rotations[i] = new_rotation;
    }
  });
}

static void align_rotations_on_component(GeometryComponent &component,
                                         const GeoNodeExecParams &params)
{
  const bNode &node = params.node();
  const NodeGeometryAlignRotationToVector &storage = *(const NodeGeometryAlignRotationToVector *)
                                                          node.storage;

  OutputAttribute_Typed<float3> rotations = component.attribute_try_get_for_output<float3>(
      "rotation", ATTR_DOMAIN_POINT, {0, 0, 0});
  if (!rotations) {
    return;
  }

  GVArray_Typed<float> factors = params.get_input_attribute<float>(
      "Factor", component, ATTR_DOMAIN_POINT, 1.0f);
  GVArray_Typed<float3> vectors = params.get_input_attribute<float3>(
      "Vector", component, ATTR_DOMAIN_POINT, {0, 0, 1});

  float3 local_main_axis{0, 0, 0};
  local_main_axis[storage.axis] = 1;
  if (storage.pivot_axis == GEO_NODE_ALIGN_ROTATION_TO_VECTOR_PIVOT_AXIS_AUTO) {
    align_rotations_auto_pivot(vectors, factors, local_main_axis, rotations.as_span());
  }
  else {
    float3 local_pivot_axis{0, 0, 0};
    local_pivot_axis[storage.pivot_axis - 1] = 1;
    align_rotations_fixed_pivot(
        vectors, factors, local_main_axis, local_pivot_axis, rotations.as_span());
  }

  rotations.save();
}

static void geo_node_align_rotation_to_vector_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (geometry_set.has<MeshComponent>()) {
    align_rotations_on_component(geometry_set.get_component_for_write<MeshComponent>(), params);
  }
  if (geometry_set.has<PointCloudComponent>()) {
    align_rotations_on_component(geometry_set.get_component_for_write<PointCloudComponent>(),
                                 params);
  }
  if (geometry_set.has<CurveComponent>()) {
    align_rotations_on_component(geometry_set.get_component_for_write<CurveComponent>(), params);
  }

  params.set_output("Geometry", geometry_set);
}

}  // namespace blender::nodes

void register_node_type_geo_align_rotation_to_vector()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype,
                     GEO_NODE_LEGACY_ALIGN_ROTATION_TO_VECTOR,
                     "Align Rotation to Vector",
                     NODE_CLASS_GEOMETRY,
                     0);
  node_type_init(&ntype, blender::nodes::geo_node_align_rotation_to_vector_init);
  node_type_update(&ntype, blender::nodes::geo_node_align_rotation_to_vector_update);
  node_type_storage(&ntype,
                    "NodeGeometryAlignRotationToVector",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  ntype.declare = blender::nodes::geo_node_align_rotation_to_vector_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_align_rotation_to_vector_exec;
  ntype.draw_buttons = blender::nodes::geo_node_align_rotation_to_vector_layout;
  nodeRegisterType(&ntype);
}
