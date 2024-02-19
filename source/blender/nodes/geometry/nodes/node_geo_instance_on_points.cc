/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_collection_types.h"

#include "BLI_array_utils.hh"
#include "BLI_hash.h"
#include "BLI_math_matrix.h"
#include "BLI_math_matrix.hh"
#include "BLI_task.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_instance_on_points_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Points").description("Points to instance on");
  b.add_input<decl::Bool>("Selection").default_value(true).field_on({0}).hide_value();
  b.add_input<decl::Geometry>("Instance").description("Geometry that is instanced on the points");
  b.add_input<decl::Bool>("Pick Instance")
      .field_on({0})
      .description(
          "Choose instances from the \"Instance\" input at each point instead of instancing the "
          "entire geometry");
  b.add_input<decl::Int>("Instance Index")
      .implicit_field_on(implicit_field_inputs::id_or_index, {0})
      .description(
          "Index of the instance used for each point. This is only used when Pick Instances "
          "is on. By default the point index is used");
  b.add_input<decl::Rotation>("Rotation").field_on({0}).description("Rotation of the instances");
  b.add_input<decl::Vector>("Scale")
      .default_value({1.0f, 1.0f, 1.0f})
      .subtype(PROP_XYZ)
      .field_on({0})
      .description("Scale of the instances");

  b.add_output<decl::Geometry>("Instances").propagate_all();
}

static void add_instances_from_component(
    bke::Instances &dst_component,
    const AttributeAccessor &src_attributes,
    const GeometrySet &instance,
    const fn::FieldContext &field_context,
    const GeoNodeExecParams &params,
    const Map<AttributeIDRef, AttributeKind> &attributes_to_propagate)
{
  const AttrDomain domain = AttrDomain::Point;
  const int domain_num = src_attributes.domain_size(domain);

  VArray<bool> pick_instance;
  VArray<int> indices;
  VArray<math::Quaternion> rotations;
  VArray<float3> scales;

  const Field<bool> selection_field = params.get_input<Field<bool>>("Selection");
  fn::FieldEvaluator evaluator{field_context, domain_num};
  evaluator.set_selection(selection_field);
  /* The evaluator could use the component's stable IDs as a destination directly, but only the
   * selected indices should be copied. */
  evaluator.add(params.get_input<Field<bool>>("Pick Instance"), &pick_instance);
  evaluator.add(params.get_input<Field<int>>("Instance Index"), &indices);
  evaluator.add(params.get_input<Field<math::Quaternion>>("Rotation"), &rotations);
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

  MutableSpan<int> dst_handles = dst_component.reference_handles_for_write().slice(start_len,
                                                                                   select_len);
  MutableSpan<float4x4> dst_transforms = dst_component.transforms().slice(start_len, select_len);

  const VArraySpan positions = *src_attributes.lookup<float3>("position");

  const bke::Instances *src_instances = instance.get_instances();

  /* Maps handles from the source instances to handles on the new instance. */
  Array<int> handle_mapping;
  /* Only fill #handle_mapping when it may be used below. */
  if (src_instances != nullptr &&
      (!pick_instance.is_single() || pick_instance.get_internal_single()))
  {
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

  selection.foreach_index(GrainSize(1024), [&](const int64_t i, const int64_t range_i) {
    /* Compute base transform for every instances. */
    float4x4 &dst_transform = dst_transforms[range_i];
    dst_transform = math::from_loc_rot_scale<float4x4>(positions[i], rotations[i], scales[i]);

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
          mul_m4_m4_post(dst_transform.ptr(), src_instances->transforms()[index].ptr());
        }
      }
    }
    else {
      /* Use entire source geometry as instance. */
      dst_handle = full_instance_handle;
    }
    /* Set properties of new instance. */
    dst_handles[range_i] = dst_handle;
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

  bke::MutableAttributeAccessor dst_attributes = dst_component.attributes_for_write();
  for (const auto item : attributes_to_propagate.items()) {
    const AttributeIDRef &id = item.key;
    const eCustomDataType data_type = item.value.data_type;
    const bke::GAttributeReader src = src_attributes.lookup(id, AttrDomain::Point, data_type);
    if (!src) {
      /* Domain interpolation can fail if the source domain is empty. */
      continue;
    }

    if (!dst_attributes.contains(id)) {
      if (src.varray.size() == dst_component.instances_num() && src.sharing_info &&
          src.varray.is_span())
      {
        const bke::AttributeInitShared init(src.varray.get_internal_span().data(),
                                            *src.sharing_info);
        dst_attributes.add(id, AttrDomain::Instance, data_type, init);
        continue;
      }
      dst_attributes.add(id, AttrDomain::Instance, data_type, bke::AttributeInitConstruct());
    }

    GSpanAttributeWriter dst = dst_attributes.lookup_for_write_span(id);
    array_utils::gather(src.varray, selection, dst.span.slice(start_len, select_len));
    dst.finish();
  }
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Points");
  GeometrySet instance = params.get_input<GeometrySet>("Instance");
  instance.ensure_owns_direct_data();
  const AnonymousAttributePropagationInfo &propagation_info = params.get_output_propagation_info(
      "Instances");

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

    const Array<GeometryComponent::Type> types{GeometryComponent::Type::Mesh,
                                               GeometryComponent::Type::PointCloud,
                                               GeometryComponent::Type::Curve};

    Map<AttributeIDRef, AttributeKind> attributes_to_propagate;
    geometry_set.gather_attributes_for_propagation(types,
                                                   GeometryComponent::Type::Instance,
                                                   false,
                                                   propagation_info,
                                                   attributes_to_propagate);
    attributes_to_propagate.remove("position");
    attributes_to_propagate.remove(".reference_index");

    for (const GeometryComponent::Type type : types) {
      if (geometry_set.has(type)) {
        const GeometryComponent &component = *geometry_set.get_component(type);
        const bke::GeometryFieldContext field_context{component, AttrDomain::Point};
        add_instances_from_component(*dst_instances,
                                     *component.attributes(),
                                     instance,
                                     field_context,
                                     params,
                                     attributes_to_propagate);
      }
    }
    if (geometry_set.has_grease_pencil()) {
      using namespace bke::greasepencil;
      const GreasePencil &grease_pencil = *geometry_set.get_grease_pencil();
      for (const int layer_index : grease_pencil.layers().index_range()) {
        const Drawing *drawing = get_eval_grease_pencil_layer_drawing(grease_pencil, layer_index);
        if (drawing == nullptr) {
          continue;
        }
        const bke::CurvesGeometry &src_curves = drawing->strokes();
        if (src_curves.curves_num() == 0) {
          /* Add an empty reference so the number of layers and instances match.
           * This makes it easy to reconstruct the layers afterwards and keep their attributes.
           * Although in this particular case we don't propagate the attributes. */
          const int handle = dst_instances->add_reference(bke::InstanceReference());
          dst_instances->add_instance(handle, float4x4::identity());
          continue;
        }
        /* TODO: Attributes are not propagating from the curves or the points. */
        bke::Instances *instances = new bke::Instances();
        const bke::GreasePencilLayerFieldContext field_context(
            grease_pencil, AttrDomain::Point, layer_index);
        add_instances_from_component(*instances,
                                     src_curves.attributes(),
                                     instance,
                                     field_context,
                                     params,
                                     attributes_to_propagate);
        GeometrySet temp_set = GeometrySet::from_instances(instances);
        const int handle = dst_instances->add_reference(bke::InstanceReference{temp_set});
        dst_instances->add_instance(handle, float4x4::identity());
      }
      GeometrySet::propagate_attributes_from_layer_to_instances(
          geometry_set.get_grease_pencil()->attributes(),
          geometry_set.get_instances_for_write()->attributes_for_write(),
          propagation_info);
      geometry_set.replace_grease_pencil(nullptr);
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

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_INSTANCE_ON_POINTS, "Instance on Points", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_instance_on_points_cc
