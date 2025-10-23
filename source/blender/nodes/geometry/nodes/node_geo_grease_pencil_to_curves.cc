/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"

#include "GEO_realize_instances.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_grease_pencil_to_curves_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Grease Pencil")
      .supported_type(bke::GeometryComponent::Type::GreasePencil)
      .description("Grease Pencil data to convert to curves");
  b.add_input<decl::Bool>("Selection")
      .default_value(true)
      .hide_value()
      .field_on_all()
      .description("Select the layers to convert");
  b.add_input<decl::Bool>("Layers as Instances")
      .default_value(true)
      .description("Create a separate curve instance for every layer");
  b.add_output<decl::Geometry>("Curves").propagate_all();
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet grease_pencil_geometry = params.extract_input<GeometrySet>("Grease Pencil");
  const GreasePencil *grease_pencil = grease_pencil_geometry.get_grease_pencil();
  if (!grease_pencil) {
    params.set_default_remaining_outputs();
    return;
  }

  const Span<const bke::greasepencil::Layer *> layers = grease_pencil->layers();
  const int layers_num = layers.size();

  const bke::GreasePencilFieldContext field_context{*grease_pencil};
  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  FieldEvaluator evaluator{field_context, layers_num};
  evaluator.set_selection(selection_field);
  evaluator.evaluate();
  const IndexMask layer_selection = evaluator.get_evaluated_selection_as_mask();

  const int instances_num = layer_selection.size();
  if (instances_num == 0) {
    params.set_default_remaining_outputs();
    return;
  }

  bke::Instances *instances = new bke::Instances();
  std::optional<int> empty_geometry_handle;

  layer_selection.foreach_index([&](const int layer_i) {
    const bke::greasepencil::Layer &layer = *layers[layer_i];
    const bke::greasepencil::Drawing *drawing = grease_pencil->get_eval_drawing(layer);
    const float4x4 transform = layer.local_transform();
    if (!drawing) {
      if (!empty_geometry_handle.has_value()) {
        empty_geometry_handle = instances->add_reference(bke::InstanceReference());
      }
      instances->add_instance(*empty_geometry_handle, transform);
      return;
    }
    const bke::CurvesGeometry &layer_strokes = drawing->strokes();
    Curves *curves_id = bke::curves_new_nomain(layer_strokes);
    curves_id->mat = static_cast<Material **>(MEM_dupallocN(grease_pencil->material_array));
    curves_id->totcol = grease_pencil->material_array_num;
    GeometrySet curves_geometry = GeometrySet::from_curves(curves_id);
    curves_geometry.name = layer.name();
    const int handle = instances->add_reference(std::move(curves_geometry));
    instances->add_instance(handle, transform);
  });

  const bke::AttributeAccessor grease_pencil_attributes = grease_pencil->attributes();
  bke::MutableAttributeAccessor instances_attributes = instances->attributes_for_write();
  grease_pencil_attributes.foreach_attribute([&](const bke::AttributeIter &iter) {
    if (ELEM(iter.name, "opacity")) {
      return;
    }
    if (iter.data_type == bke::AttrType::String) {
      return;
    }
    const GAttributeReader src_attribute = iter.get();
    if (!src_attribute) {
      return;
    }
    if (src_attribute.varray.is_span() && src_attribute.sharing_info) {
      /* Try reusing existing attribute array. */
      instances_attributes.add(
          iter.name,
          AttrDomain::Instance,
          iter.data_type,
          bke::AttributeInitShared{src_attribute.varray.get_internal_span().data(),
                                   *src_attribute.sharing_info});
      return;
    }
    if (!instances_attributes.add(
            iter.name, AttrDomain::Instance, iter.data_type, bke::AttributeInitConstruct()))
    {
      return;
    }
    bke::GSpanAttributeWriter dst_attribute = instances_attributes.lookup_for_write_span(
        iter.name);
    array_utils::gather(src_attribute.varray, layer_selection, dst_attribute.span);
    dst_attribute.finish();
  });

  {
    /* Manually propagate "opacity" data, because it's not a layer attribute on grease pencil
     * yet. */
    if (SpanAttributeWriter<float> opacity_attribute =
            instances_attributes.lookup_or_add_for_write_only_span<float>("opacity",
                                                                          AttrDomain::Instance))
    {
      layer_selection.foreach_index([&](const int layer_i, const int instance_i) {
        opacity_attribute.span[instance_i] = grease_pencil->layer(layer_i).opacity;
      });
      opacity_attribute.finish();
    }
  }

  GeometrySet curves_geometry = GeometrySet::from_instances(instances);
  curves_geometry.name = std::move(grease_pencil_geometry.name);

  const bool layers_as_instances = params.extract_input<bool>("Layers as Instances");
  if (!layers_as_instances) {
    geometry::RealizeInstancesOptions options;
    const NodeAttributeFilter attribute_filter = params.get_attribute_filter("Curves");
    options.attribute_filter = attribute_filter;
    curves_geometry = geometry::realize_instances(curves_geometry, options).geometry;
  }

  params.set_output("Curves", std::move(curves_geometry));
}

static void node_register()
{
  static bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeGreasePencilToCurves", GEO_NODE_GREASE_PENCIL_TO_CURVES);
  ntype.ui_name = "Grease Pencil to Curves";
  ntype.ui_description = "Convert Grease Pencil layers into curve instances";
  ntype.enum_name_legacy = "GREASE_PENCIL_TO_CURVES";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  bke::node_type_size(ntype, 160, 100, 320);

  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_grease_pencil_to_curves_cc
