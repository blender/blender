/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_collection_types.h"

#include "BLI_array_utils.hh"
#include "BLI_hash.h"
#include "BLI_task.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "BKE_attribute_math.hh"
#include "BKE_instances.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_instance_on_points_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Points")).description(N_("Points to instance on"));
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).field_on({0}).hide_value();
  b.add_input<decl::Geometry>(N_("Instance"))
      .description(N_("Geometry that is instanced on the points"));
  b.add_input<decl::Bool>(N_("Pick Instance"))
      .field_on({0})
      .description(N_("Choose instances from the \"Instance\" input at each point instead of "
                      "instancing the entire geometry"));
  b.add_input<decl::Int>(N_("Instance Index"))
      .implicit_field_on(implicit_field_inputs::id_or_index, {0})
      .description(
          N_("Index of the instance used for each point. This is only used when Pick Instances "
             "is on. By default the point index is used"));
  b.add_input<decl::Vector>(N_("Rotation"))
      .subtype(PROP_EULER)
      .field_on({0})
      .description(N_("Rotation of the instances"));
  b.add_input<decl::Vector>(N_("Scale"))
      .default_value({1.0f, 1.0f, 1.0f})
      .subtype(PROP_XYZ)
      .field_on({0})
      .description(N_("Scale of the instances"));

  b.add_output<decl::Geometry>(N_("Instances")).propagate_all();
}

static void add_instances_from_component(
    bke::Instances &dst_component,
    const GeometryComponent &src_component,
    const GeometrySet &instance,
    const GeoNodeExecParams &params,
    const Map<AttributeIDRef, AttributeKind> &attributes_to_propagate)
{
  const eAttrDomain domain = ATTR_DOMAIN_POINT;
  const int domain_num = src_component.attribute_domain_size(domain);

  VArray<bool> pick_instance;
  VArray<int> indices;
  VArray<float3> rotations;
  VArray<float3> scales;

  bke::GeometryFieldContext field_context{src_component, domain};
  const Field<bool> selection_field = params.get_input<Field<bool>>("Selection");
  fn::FieldEvaluator evaluator{field_context, domain_num};
  evaluator.set_selection(selection_field);
  /* The evaluator could use the component's stable IDs as a destination directly, but only the
   * selected indices should be copied. */
  evaluator.add(params.get_input<Field<bool>>("Pick Instance"), &pick_instance);
  evaluator.add(params.get_input<Field<int>>("Instance Index"), &indices);
  evaluator.add(params.get_input<Field<float3>>("Rotation"), &rotations);
  evaluator.add(params.get_input<Field<float3>>("Scale"), &scales);
  evaluator.evaluate();

  const IndexMask selection = evaluator.get_evaluated_selection_as_mask();
  if (selection.is_empty()) {
    return;
  }

  /* The initial size of the component might be non-zero when this function is called for multiple
   * component types. */
  const int start_len = dst_component.instances_num();
  const int select_len = selection.index_range().size();
  dst_component.resize(start_len + select_len);

  MutableSpan<int> dst_handles = dst_component.reference_handles().slice(start_len, select_len);
  MutableSpan<float4x4> dst_transforms = dst_component.transforms().slice(start_len, select_len);

  VArray<float3> positions = src_component.attributes()->lookup_or_default<float3>(
      "position", domain, {0, 0, 0});

  const bke::Instances *src_instances = instance.get_instances_for_read();

  /* Maps handles from the source instances to handles on the new instance. */
  Array<int> handle_mapping;
  /* Only fill #handle_mapping when it may be used below. */
  if (src_instances != nullptr &&
      (!pick_instance.is_single() || pick_instance.get_internal_single())) {
    Span<bke::InstanceReference> src_references = src_instances->references();
    handle_mapping.reinitialize(src_references.size());
    for (const int src_instance_handle : src_references.index_range()) {
      const bke::InstanceReference &reference = src_references[src_instance_handle];
      const int dst_instance_handle = dst_component.add_reference(reference);
      handle_mapping[src_instance_handle] = dst_instance_handle;
    }
  }

  const int full_instance_handle = dst_component.add_reference(instance);
  /* Add this reference last, because it is the most likely one to be removed later on. */
  const int empty_reference_handle = dst_component.add_reference(bke::InstanceReference());

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
          const int src_instances_num = src_instances->instances_num();
          const int original_index = indices[i];
          /* Use #mod_i instead of `%` to get the desirable wrap around behavior where -1
           * refers to the last element. */
          const int index = mod_i(original_index, std::max(src_instances_num, 1));
          if (index < src_instances_num) {
            /* Get the reference to the source instance. */
            const int src_handle = src_instances->reference_handles()[index];
            dst_handle = handle_mapping[src_handle];

            /* Take transforms of the source instance into account. */
            mul_m4_m4_post(dst_transform.values, src_instances->transforms()[index].values);
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

  bke::CustomDataAttributes &instance_attributes = dst_component.custom_data_attributes();
  for (const auto item : attributes_to_propagate.items()) {
    const AttributeIDRef &attribute_id = item.key;
    const AttributeKind attribute_kind = item.value;

    const GVArray src_attribute = src_component.attributes()->lookup_or_default(
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
    array_utils::gather(src_attribute, selection, dst_attribute_opt->slice(start_len, select_len));
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  GeometrySet instance = params.get_input<GeometrySet>("Instance");
  instance.ensure_owns_direct_data();

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    /* It's important not to invalidate the existing #InstancesComponent because it owns references
     * to other geometry sets that are processed by this node. */
    InstancesComponent &instances_component =
        geometry_set.get_component_for_write<InstancesComponent>();
    bke::Instances *dst_instances = instances_component.get_for_write();
    if (dst_instances == nullptr) {
      dst_instances = new bke::Instances();
      instances_component.replace(dst_instances);
    }

    const Array<GeometryComponentType> types{
        GEO_COMPONENT_TYPE_MESH, GEO_COMPONENT_TYPE_POINT_CLOUD, GEO_COMPONENT_TYPE_CURVE};

    Map<AttributeIDRef, AttributeKind> attributes_to_propagate;
    geometry_set.gather_attributes_for_propagation(types,
                                                   GEO_COMPONENT_TYPE_INSTANCES,
                                                   false,
                                                   params.get_output_propagation_info("Instances"),
                                                   attributes_to_propagate);
    attributes_to_propagate.remove("position");

    for (const GeometryComponentType type : types) {
      if (geometry_set.has(type)) {
        add_instances_from_component(*dst_instances,
                                     *geometry_set.get_component_for_read(type),
                                     instance,
                                     params,
                                     attributes_to_propagate);
      }
    }
    geometry_set.remove_geometry_during_modify();
  });

  /* Unused references may have been added above. Remove those now so that other nodes don't
   * process them needlessly.
   * This should eventually be moved into the loop above, but currently this is quite tricky
   * because it might remove references that the loop still wants to iterate over. */
  if (bke::Instances *instances = geometry_set.get_instances_for_write()) {
    instances->remove_unused_references();
  }

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
