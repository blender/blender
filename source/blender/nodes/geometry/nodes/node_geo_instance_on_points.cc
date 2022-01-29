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

#include "BKE_attribute_math.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_instance_on_points_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Points")).description(N_("Points to instance on"));
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).supports_field().hide_value();
  b.add_input<decl::Geometry>(N_("Instance"))
      .description(N_("Geometry that is instanced on the points"));
  b.add_input<decl::Bool>(N_("Pick Instance"))
      .supports_field()
      .description(N_("Choose instances from the \"Instance\" input at each point instead of "
                      "instancing the entire geometry"));
  b.add_input<decl::Int>(N_("Instance Index"))
      .implicit_field()
      .description(N_(
          "Index of the instance that used for each point. This is only used when Pick Instances "
          "is on. By default the point index is used"));
  b.add_input<decl::Vector>(N_("Rotation"))
      .subtype(PROP_EULER)
      .supports_field()
      .description(N_("Rotation of the instances"));
  b.add_input<decl::Vector>(N_("Scale"))
      .default_value({1.0f, 1.0f, 1.0f})
      .subtype(PROP_XYZ)
      .supports_field()
      .description(N_("Scale of the instances"));

  b.add_output<decl::Geometry>(N_("Instances"));
}

static void add_instances_from_component(
    InstancesComponent &dst_component,
    const GeometryComponent &src_component,
    const GeometrySet &instance,
    const GeoNodeExecParams &params,
    const Map<AttributeIDRef, AttributeKind> &attributes_to_propagate)
{
  const AttributeDomain domain = ATTR_DOMAIN_POINT;
  const int domain_size = src_component.attribute_domain_size(domain);

  VArray<bool> pick_instance;
  VArray<int> indices;
  VArray<float3> rotations;
  VArray<float3> scales;

  GeometryComponentFieldContext field_context{src_component, domain};
  const Field<bool> selection_field = params.get_input<Field<bool>>("Selection");
  fn::FieldEvaluator evaluator{field_context, domain_size};
  evaluator.set_selection(selection_field);
  /* The evaluator could use the component's stable IDs as a destination directly, but only the
   * selected indices should be copied. */
  evaluator.add(params.get_input<Field<bool>>("Pick Instance"), &pick_instance);
  evaluator.add(params.get_input<Field<int>>("Instance Index"), &indices);
  evaluator.add(params.get_input<Field<float3>>("Rotation"), &rotations);
  evaluator.add(params.get_input<Field<float3>>("Scale"), &scales);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();

  /* The initial size of the component might be non-zero when this function is called for multiple
   * component types. */
  const int start_len = dst_component.instances_amount();
  const int select_len = selection.index_range().size();
  dst_component.resize(start_len + select_len);

  MutableSpan<int> dst_handles = dst_component.instance_reference_handles().slice(start_len,
                                                                                  select_len);
  MutableSpan<float4x4> dst_transforms = dst_component.instance_transforms().slice(start_len,
                                                                                   select_len);

  VArray<float3> positions = src_component.attribute_get_for_read<float3>(
      "position", domain, {0, 0, 0});

  const InstancesComponent *src_instances = instance.get_component_for_read<InstancesComponent>();

  /* Maps handles from the source instances to handles on the new instance. */
  Array<int> handle_mapping;
  /* Only fill #handle_mapping when it may be used below. */
  if (src_instances != nullptr &&
      (!pick_instance.is_single() || pick_instance.get_internal_single())) {
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

      /* Compute base transform for every instances. */
      float4x4 &dst_transform = dst_transforms[range_i];
      dst_transform = float4x4::from_loc_eul_scale(positions[i], rotations[i], scales[i]);

      /* Reference that will be used by this new instance. */
      int dst_handle = empty_reference_handle;

      const bool use_individual_instance = pick_instance[i];
      if (use_individual_instance) {
        if (src_instances != nullptr) {
          const int src_instances_amount = src_instances->instances_amount();
          const int original_index = indices[i];
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

  if (pick_instance.is_single()) {
    if (pick_instance.get_internal_single()) {
      if (instance.has_realized_data()) {
        params.error_message_add(
            NodeWarningType::Info,
            TIP_("Realized geometry is not used when pick instances is true"));
      }
    }
  }

  bke::CustomDataAttributes &instance_attributes = dst_component.attributes();
  for (const auto item : attributes_to_propagate.items()) {
    const AttributeIDRef &attribute_id = item.key;
    const AttributeKind attribute_kind = item.value;

    const GVArray src_attribute = src_component.attribute_get_for_read(
        attribute_id, ATTR_DOMAIN_POINT, attribute_kind.data_type);
    BLI_assert(src_attribute);
    std::optional<GMutableSpan> dst_attribute_opt = instance_attributes.get_for_write(
        attribute_id);
    if (!dst_attribute_opt) {
      if (!instance_attributes.create(attribute_id, attribute_kind.data_type)) {
        continue;
      }
      dst_attribute_opt = instance_attributes.get_for_write(attribute_id);
    }
    BLI_assert(dst_attribute_opt);
    const GMutableSpan dst_attribute = dst_attribute_opt->slice(start_len, select_len);
    threading::parallel_for(selection.index_range(), 1024, [&](IndexRange selection_range) {
      attribute_math::convert_to_static_type(attribute_kind.data_type, [&](auto dummy) {
        using T = decltype(dummy);
        VArray<T> src = src_attribute.typed<T>();
        MutableSpan<T> dst = dst_attribute.typed<T>();
        for (const int range_i : selection_range) {
          const int i = selection[range_i];
          dst[range_i] = src[i];
        }
      });
    });
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  GeometrySet instance = params.get_input<GeometrySet>("Instance");
  instance.ensure_owns_direct_data();

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    InstancesComponent &instances = geometry_set.get_component_for_write<InstancesComponent>();

    Map<AttributeIDRef, AttributeKind> attributes_to_propagate;
    geometry_set.gather_attributes_for_propagation(
        {GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_CURVE},
        GEO_COMPONENT_TYPE_INSTANCES,
        false,
        attributes_to_propagate);
    attributes_to_propagate.remove("position");

    if (geometry_set.has<MeshComponent>()) {
      add_instances_from_component(instances,
                                   *geometry_set.get_component_for_read<MeshComponent>(),
                                   instance,
                                   params,
                                   attributes_to_propagate);
    }
    if (geometry_set.has<PointCloudComponent>()) {
      add_instances_from_component(instances,
                                   *geometry_set.get_component_for_read<PointCloudComponent>(),
                                   instance,
                                   params,
                                   attributes_to_propagate);
    }
    if (geometry_set.has<CurveComponent>()) {
      add_instances_from_component(instances,
                                   *geometry_set.get_component_for_read<CurveComponent>(),
                                   instance,
                                   params,
                                   attributes_to_propagate);
    }
    geometry_set.keep_only({GEO_COMPONENT_TYPE_INSTANCES});
  });

  /* Unused references may have been added above. Remove those now so that other nodes don't
   * process them needlessly.
   * This should eventually be moved into the loop above, but currently this is quite tricky
   * because it might remove references that the loop still wants to iterate over. */
  InstancesComponent &instances = geometry_set.get_component_for_write<InstancesComponent>();
  instances.remove_unused_references();

  params.set_output("Instances", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_instance_on_points_cc

void register_node_type_geo_instance_on_points()
{
  namespace file_ns = blender::nodes::node_geo_instance_on_points_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INSTANCE_ON_POINTS, "Instance on Points", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  nodeRegisterType(&ntype);
}
