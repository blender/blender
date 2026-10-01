/* SPDX-FileCopyrightText: 2025 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"

#include "UI_interface_layout.hh"
#include "UI_resources.hh"

#include "NOD_rna_define.hh"

#include "GEO_foreach_geometry.hh"

#include "RNA_enum_types.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_set_grease_pencil_color_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Grease Pencil"_ustr)
      .supported_type(GeometryComponent::Type::GreasePencil)
      .description("Grease Pencil to change the color of");
  b.add_output<decl::Geometry>("Grease Pencil"_ustr)
      .propagate_all_geometry()
      .align_with_previous();
  b.add_input<decl::Bool>("Selection"_ustr)
      .default_value(true)
      .hide_value()
      .evaluated_geometry_field();
  b.add_input<decl::Menu>("Mode"_ustr)
      .static_items(rna_enum_node_grease_pencil_stroke_type_items)
      .optional_label();
  b.add_input<decl::Color>("Color"_ustr)
      .default_value(ColorGeometry4f(1.0f, 1.0f, 1.0f, 1.0f))
      .evaluated_geometry_field()
      .optional_label();
  b.add_input<decl::Float>("Opacity"_ustr)
      .default_value(1.0f)
      .min(0.0f)
      .max(1.0f)
      .evaluated_geometry_field();
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int(GEO_NODE_GREASE_PENCIL_STROKE);
}

static void set_grease_pencil_color(GeometrySet &geometry_set,
                                    const AttrDomain domain,
                                    const StringRef color_attr,
                                    const StringRef opacity_attr,
                                    const Field<bool> &selection,
                                    const Field<ColorGeometry4f> &color_field,
                                    const Field<float> &opacity_field)
{
  using namespace bke::greasepencil;

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry) {
    GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write();
    if (!grease_pencil) {
      return;
    }

    for (const int layer_index : grease_pencil->layers().index_range()) {
      Drawing *drawing = grease_pencil->get_eval_drawing(grease_pencil->layer(layer_index));
      if (!drawing) {
        continue;
      }

      bke::CurvesGeometry &curves = drawing->strokes_for_write();
      bke::MutableAttributeAccessor attributes = curves.attributes_for_write();

      if (!attributes.contains(opacity_attr)) {
        attributes.add<float>(opacity_attr, domain, bke::AttributeInitValue(1.0f));
      }

      const bke::GreasePencilLayerFieldContext field_context(*grease_pencil, domain, layer_index);

      bke::try_capture_fields_on_geometry(attributes,
                                          field_context,
                                          {color_attr, opacity_attr},
                                          domain,
                                          selection,
                                          {color_field, opacity_field});
    }
  });
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const auto mode = params.get_input<NodeGeometryGreasePencilStrokeType>("Mode"_ustr);

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Grease Pencil"_ustr);
  const Field<bool> selection = params.extract_input<Field<bool>>("Selection"_ustr);
  const Field<ColorGeometry4f> color_field = params.extract_input<Field<ColorGeometry4f>>(
      "Color"_ustr);
  const Field<float> opacity_field = params.extract_input<Field<float>>("Opacity"_ustr);

  if (ELEM(mode, GEO_NODE_GREASE_PENCIL_STROKE, GEO_NODE_GREASE_PENCIL_BOTH)) {
    set_grease_pencil_color(geometry_set,
                            AttrDomain::Point,
                            "vertex_color",
                            "opacity",
                            selection,
                            color_field,
                            opacity_field);
  }
  if (ELEM(mode, GEO_NODE_GREASE_PENCIL_FILL, GEO_NODE_GREASE_PENCIL_BOTH)) {
    set_grease_pencil_color(geometry_set,
                            AttrDomain::Curve,
                            "fill_color",
                            "fill_opacity",
                            selection,
                            color_field,
                            opacity_field);
  }

  params.set_output("Grease Pencil"_ustr, std::move(geometry_set));
}

static void node_register()
{
  static bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSetGreasePencilColor"_ustr);
  ntype.ui_name = "Set Grease Pencil Color";
  ntype.ui_description = "Set color and opacity attributes on Grease Pencil geometry";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.default_width = bke::NodeWidth::_180;
  bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_grease_pencil_color_cc
