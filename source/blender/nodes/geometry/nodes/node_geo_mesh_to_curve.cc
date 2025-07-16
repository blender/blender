/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "NOD_rna_define.hh"

#include "GEO_mesh_to_curve.hh"

#include "UI_interface_c.hh"
#include "UI_interface_layout.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_to_curve_cc {

enum class Mode : int8_t {
  Edges = 0,
  Faces = 1,
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh")
      .supported_type(GeometryComponent::Type::Mesh)
      .description("Mesh to convert to curves");
  b.add_input<decl::Bool>("Selection").default_value(true).hide_value().field_on_all();
  b.add_output<decl::Geometry>("Curve").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  layout->prop(ptr, "mode", UI_ITEM_R_EXPAND, std::nullopt, 0);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  const Mode mode = Mode(params.node().custom1);
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    const Mesh *mesh = geometry_set.get_mesh();
    if (mesh == nullptr) {
      geometry_set.remove_geometry_during_modify();
      return;
    }

    switch (mode) {
      case Mode::Edges: {
        const bke::MeshFieldContext context{*mesh, AttrDomain::Edge};
        fn::FieldEvaluator evaluator{context, mesh->edges_num};
        evaluator.add(params.get_input<Field<bool>>("Selection"));
        evaluator.evaluate();
        const IndexMask selection = evaluator.get_evaluated_as_mask(0);
        if (selection.is_empty()) {
          geometry_set.remove_geometry_during_modify();
          return;
        }

        bke::CurvesGeometry curves = geometry::mesh_edges_to_curves_convert(
            *mesh, selection, params.get_attribute_filter("Curve"));
        geometry_set.replace_curves(bke::curves_new_nomain(std::move(curves)));
        break;
      }
      case Mode::Faces: {
        const bke::MeshFieldContext context{*mesh, AttrDomain::Face};
        fn::FieldEvaluator evaluator{context, mesh->faces_num};
        evaluator.add(params.get_input<Field<bool>>("Selection"));
        evaluator.evaluate();
        const IndexMask selection = evaluator.get_evaluated_as_mask(0);
        if (selection.is_empty()) {
          geometry_set.remove_geometry_during_modify();
          return;
        }

        bke::CurvesGeometry curves = geometry::mesh_faces_to_curves_convert(
            *mesh, selection, params.get_attribute_filter("Curve"));
        geometry_set.replace_curves(bke::curves_new_nomain(std::move(curves)));
        break;
      }
    }
    geometry_set.keep_only_during_modify({GeometryComponent::Type::Curve});
  });

  params.set_output("Curve", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem mode_items[] = {
      {int(Mode::Edges),
       "EDGES",
       0,
       "Edges",
       "Convert mesh edges to curve segments. Attributes are propagated to curve points."},
      {int(Mode::Faces),
       "FACES",
       0,
       "Faces",
       "Convert each mesh face to a cyclic curve. Face attributes are propagated to curves."},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(
      srna, "mode", "Mode", "", mode_items, NOD_inline_enum_accessors(custom1), int(Mode::Edges));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeMeshToCurve", GEO_NODE_MESH_TO_CURVE);
  ntype.ui_name = "Mesh to Curve";
  ntype.ui_description = "Generate a curve from a mesh";
  ntype.enum_name_legacy = "MESH_TO_CURVE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.draw_buttons = node_layout;
  ntype.geometry_node_execute = node_geo_exec;
  blender::bke::node_register_type(ntype);
  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_to_curve_cc
