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

#include "DNA_collection_types.h"

#include "BLI_hash.h"
#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_instance_on_points_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Points").description("Points to instance on");
  b.add_input<decl::Bool>("Selection").default_value(true).supports_field().hide_value();
  b.add_input<decl::Geometry>("Instance").description("Geometry that is instanced on the points");
  b.add_input<decl::Bool>("Pick Instance")
      .supports_field()
      .description("Place different instances on different points");
  b.add_input<decl::Int>("Instance Index")
      .implicit_field()
      .description(
          "Index of the instance that used for each point. This is only used when Pick Instances "
          "is on. By default the point index is used");
  b.add_input<decl::Vector>("Rotation")
      .subtype(PROP_EULER)
      .supports_field()
      .description("Rotation of the instances");
  b.add_input<decl::Vector>("Scale")
      .default_value({1.0f, 1.0f, 1.0f})
      .subtype(PROP_XYZ)
      .supports_field()
      .description("Scale of the instances");

  b.add_output<decl::Geometry>("Instances");
}

static void add_instances_from_component(InstancesComponent &dst_component,
                                         const GeometryComponent &src_component,
                                         const GeometrySet &instance,
                                         const GeoNodeExecParams &params)
{
  const AttributeDomain domain = ATTR_DOMAIN_POINT;
  const int domain_size = src_component.attribute_domain_size(domain);

  GeometryComponentFieldContext field_context{src_component, domain};
  const Field<bool> selection_field = params.get_input<Field<bool>>("Selection");
  fn::FieldEvaluator selection_evaluator{field_context, domain_size};
  selection_evaluator.add(selection_field);
  selection_evaluator.evaluate();
  const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);

  /* The initial size of the component might be non-zero when this function is called for multiple
   * component types. */
  const int start_len = dst_component.instances_amount();
  const int select_len = selection.index_range().size();
  dst_component.resize(start_len + select_len);

  MutableSpan<int> dst_handles = dst_component.instance_reference_handles().slice(start_len,
                                                                                  select_len);
  MutableSpan<float4x4> dst_transforms = dst_component.instance_transforms().slice(start_len,
                                                                                   select_len);
  MutableSpan<int> dst_stable_ids = dst_component.instance_ids().slice(start_len, select_len);

  FieldEvaluator field_evaluator{field_context, domain_size};
  const VArray<bool> *pick_instance = nullptr;
  const VArray<int> *indices = nullptr;
  const VArray<float3> *rotations = nullptr;
  const VArray<float3> *scales = nullptr;
  /* The evaluator could use the component's stable IDs as a destination directly, but only the
   * selected indices should be copied. */
  GVArray_Typed<int> stable_ids = src_component.attribute_get_for_read("id", ATTR_DOMAIN_POINT, 0);
  field_evaluator.add(params.get_input<Field<bool>>("Pick Instance"), &pick_instance);
  field_evaluator.add(params.get_input<Field<int>>("Instance Index"), &indices);
  field_evaluator.add(params.get_input<Field<float3>>("Rotation"), &rotations);
  field_evaluator.add(params.get_input<Field<float3>>("Scale"), &scales);
  field_evaluator.evaluate();

  GVArray_Typed<float3> positions = src_component.attribute_get_for_read<float3>(
      "position", domain, {0, 0, 0});

  const InstancesComponent *src_instances = instance.get_component_for_read<InstancesComponent>();

  /* Maps handles from the source instances to handles on the new instance. */
  Array<int> handle_mapping;
  /* Only fill #handle_mapping when it may be used below. */
  if (src_instances != nullptr &&
      (!pick_instance->is_single() || pick_instance->get_internal_single())) {
    Span<InstanceReference> src_references = src_instances->references();
    handle_mapping.reinitialize(src_references.size());
    for (const int src_instance_handle : src_references.index_range()) {
      const InstanceReference &reference = src_references[src_instance_handle];
      const int dst_instance_handle = dst_component.add_reference(reference);
      handle_mapping[src_instance_handle] = dst_instance_handle;
    }
  }

  const int full_instance_handle = dst_component.add_reference(instance);
  /* Add this reference last, because it is the most likely one to be removed later on. */
  const int empty_reference_handle = dst_component.add_reference(InstanceReference());

  threading::parallel_for(selection.index_range(), 1024, [&](IndexRange selection_range) {
    for (const int range_i : selection_range) {
      const int64_t i = selection[range_i];
      dst_stable_ids[range_i] = (*stable_ids)[i];

      /* Compute base transform for every instances. */
      float4x4 &dst_transform = dst_transforms[range_i];
      dst_transform = float4x4::from_loc_eul_scale(
          positions[i], rotations->get(i), scales->get(i));

      /* Reference that will be used by this new instance. */
      int dst_handle = empty_reference_handle;

      const bool use_individual_instance = pick_instance->get(i);
      if (use_individual_instance) {
        if (src_instances != nullptr) {
          const int src_instances_amount = src_instances->instances_amount();
          const int original_index = indices->get(i);
          /* Use #mod_i instead of `%` to get the desirable wrap around behavior where -1
           * refers to the last element. */
          const int index = mod_i(original_index, std::max(src_instances_amount, 1));
          if (index < src_instances_amount) {
            /* Get the reference to the source instance. */
            const int src_handle = src_instances->instance_reference_handles()[index];
            dst_handle = handle_mapping[src_handle];

            /* Take transforms of the source instance into account. */
            mul_m4_m4_post(dst_transform.values,
                           src_instances->instance_transforms()[index].values);
          }
        }
      }
      else {
        /* Use entire source geometry as instance. */
        dst_handle = full_instance_handle;
      }
      /* Set properties of new instance. */
      dst_handles[range_i] = dst_handle;
    }
  });

  if (pick_instance->is_single()) {
    if (pick_instance->get_internal_single()) {
      if (instance.has_realized_data()) {
        params.error_message_add(
            NodeWarningType::Info,
            TIP_("Realized geometry is not used when pick instances is true"));
      }
    }
  }
}

static void geo_node_instance_on_points_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  GeometrySet instance = params.get_input<GeometrySet>("Instance");
  instance.ensure_owns_direct_data();

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    InstancesComponent &instances = geometry_set.get_component_for_write<InstancesComponent>();

    if (geometry_set.has<MeshComponent>()) {
      add_instances_from_component(
          instances, *geometry_set.get_component_for_read<MeshComponent>(), instance, params);
      geometry_set.remove(GEO_COMPONENT_TYPE_MESH);
    }
    if (geometry_set.has<PointCloudComponent>()) {
      add_instances_from_component(instances,
                                   *geometry_set.get_component_for_read<PointCloudComponent>(),
                                   instance,
                                   params);
      geometry_set.remove(GEO_COMPONENT_TYPE_POINT_CLOUD);
    }
    if (geometry_set.has<CurveComponent>()) {
      add_instances_from_component(
          instances, *geometry_set.get_component_for_read<CurveComponent>(), instance, params);
      geometry_set.remove(GEO_COMPONENT_TYPE_CURVE);
    }
    /* Unused references may have been added above. Remove those now so that other nodes don't
     * process them needlessly. */
    /** \note: This currently expects that all originally existing instances were used. */
    instances.remove_unused_references();
  });

  params.set_output("Instances", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_instance_on_points()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INSTANCE_ON_POINTS, "Instance on Points", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_instance_on_points_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_instance_on_points_exec;
  nodeRegisterType(&ntype);
}
