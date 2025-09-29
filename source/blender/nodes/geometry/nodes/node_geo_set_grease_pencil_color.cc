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

enum class Mode : int8_t {
  Stroke = 0,
  Fill = 1,
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_default_layout();
  b.add_input<decl::Geometry>("Grease Pencil")
      .supported_type(GeometryComponent::Type::GreasePencil)
      .align_with_previous()
      .description("Grease Pencil to change the color of");
  b.add_output<decl::Geometry>("Grease Pencil").propagate_all().align_with_previous();
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_input<decl::Color>("Color")
      .default_value(ColorGeometry4f(1.0f, 1.0f, 1.0f, 1.0f))
      .field_on_all()
      .optional_label();
  b.add_input<decl::Float>("Opacity").default_value(1.0f).min(0.0f).max(1.0f).field_on_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "mode", UI_ITEM_NONE, "", ICON_NONE);
}
static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int(Mode::Stroke);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const bNode &node = params.node();
  const AttrDomain domain = Mode(node.custom1) == Mode::Stroke ? AttrDomain::Point :
                                                                 AttrDomain::Curve;

  GeometrySet geometry_set = params.extract_input<GeometrySet>("Grease Pencil");
  const Field<bool> selection = params.extract_input<Field<bool>>("Selection");
  const Field<ColorGeometry4f> color_field = params.extract_input<Field<ColorGeometry4f>>("Color");
  const Field<float> opacity_field = params.extract_input<Field<float>>("Opacity");

  const StringRef color_attr_name = domain == AttrDomain::Point ? "vertex_color" : "fill_color";
  const StringRef opacity_attr_name = domain == AttrDomain::Point ? "opacity" : "fill_opacity";

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry) {
    if (GreasePencil *grease_pencil = geometry.get_grease_pencil_for_write()) {
      using namespace bke::greasepencil;
      for (const int layer_index : grease_pencil->layers().index_range()) {
        Drawing *drawing = grease_pencil->get_eval_drawing(grease_pencil->layer(layer_index));
        if (drawing == nullptr) {
          continue;
        }
        bke::CurvesGeometry &curves = drawing->strokes_for_write();
        const int64_t domain_size = curves.attributes().domain_size(domain);

        const bke::GreasePencilLayerFieldContext layer_field_context(
            *grease_pencil, domain, layer_index);

        /* FIXME: The default float value is 0, while the default opacity should be 1. So we have
         * to initialize the attribute manually.
         * TODO: Avoid doing this if the selection is false. */
        if (!curves.attributes().contains(opacity_attr_name)) {
          curves.attributes_for_write().add<float>(
              opacity_attr_name,
              domain,
              bke::AttributeInitVArray(VArray<float>::from_single(1.0f, domain_size)));
        }
        bke::try_capture_fields_on_geometry(curves.attributes_for_write(),
                                            layer_field_context,
                                            {color_attr_name, opacity_attr_name},
                                            domain,
                                            selection,
                                            {color_field, opacity_field});
      }
    }
  });

  params.set_output("Grease Pencil", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem mode_items[] = {
      {int(Mode::Stroke),
       "STROKE",
       ICON_NONE,
       "Stroke",
       "Set the color and opacity for the points of the stroke"},
      {int(Mode::Fill),
       "FILL",
       ICON_NONE,
       "Fill",
       "Set the color and opacity for the stroke fills"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna, "mode", "Mode", "", mode_items, NOD_inline_enum_accessors(custom1));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSetGreasePencilColor");
  ntype.ui_name = "Set Grease Pencil Color";
  ntype.ui_description = "Set color and opacity attributes on Grease Pencil geometry";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.declare = node_declare;
  ntype.initfunc = node_init;
  ntype.draw_buttons = node_layout;
  bke::node_type_size(ntype, 170, 120, NODE_DEFAULT_MAX_WIDTH);
  blender::bke::node_register_type(ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_set_grease_pencil_color_cc
