/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_mesh_types.h"

#include "NOD_rna_define.hh"

#include "GEO_mesh_triangulate.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GEO_randomize.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_triangulate_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh").supported_type(GeometryComponent::Type::Mesh);
  b.add_input<decl::Bool>("Selection").default_value(true).field_on_all().hide_value();
  b.add_output<decl::Geometry>("Mesh").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "quad_method", UI_ITEM_NONE, "", ICON_NONE);
  uiItemR(layout, ptr, "ngon_method", UI_ITEM_NONE, "", ICON_NONE);
}

static void geo_triangulate_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = int(geometry::TriangulateQuadMode::ShortEdge);
  node->custom2 = int(geometry::TriangulateNGonMode::Beauty);
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  const AttributeFilter &attribute_filter = params.get_attribute_filter("Mesh");

  geometry::TriangulateNGonMode ngon_method = geometry::TriangulateNGonMode(params.node().custom2);
  geometry::TriangulateQuadMode quad_method = geometry::TriangulateQuadMode(params.node().custom1);

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    const Mesh *src_mesh = geometry_set.get_mesh();
    if (!src_mesh) {
      return;
    }
    if (src_mesh->corners_num == src_mesh->faces_num * 3) {
      /* The mesh is already completely triangulated. */
      return;
    }

    const bke::MeshFieldContext context(*src_mesh, AttrDomain::Face);
    FieldEvaluator evaluator{context, src_mesh->faces_num};
    evaluator.add(selection_field);
    evaluator.evaluate();
    const IndexMask selection = evaluator.get_evaluated_as_mask(0);
    if (selection.is_empty()) {
      return;
    }

    std::optional<Mesh *> mesh = geometry::mesh_triangulate(
        *src_mesh,
        selection,
        geometry::TriangulateNGonMode(ngon_method),
        geometry::TriangulateQuadMode(quad_method),
        attribute_filter);
    if (!mesh) {
      return;
    }

    /* Vertex order is not affected. */
    geometry::debug_randomize_edge_order(*mesh);
    geometry::debug_randomize_face_order(*mesh);

    geometry_set.replace_mesh(*mesh);
  });

  params.set_output("Mesh", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem rna_node_geometry_triangulate_quad_method_items[] = {
      {int(geometry::TriangulateQuadMode::Beauty),
       "BEAUTY",
       0,
       "Beauty",
       "Split the quads in nice triangles, slower method"},
      {int(geometry::TriangulateQuadMode::Fixed),
       "FIXED",
       0,
       "Fixed",
       "Split the quads on the first and third vertices"},
      {int(geometry::TriangulateQuadMode::Alternate),
       "FIXED_ALTERNATE",
       0,
       "Fixed Alternate",
       "Split the quads on the 2nd and 4th vertices"},
      {int(geometry::TriangulateQuadMode::ShortEdge),
       "SHORTEST_DIAGONAL",
       0,
       "Shortest Diagonal",
       "Split the quads along their shortest diagonal"},
      {int(geometry::TriangulateQuadMode::LongEdge),
       "LONGEST_DIAGONAL",
       0,
       "Longest Diagonal",
       "Split the quads along their longest diagonal"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem rna_node_geometry_triangulate_ngon_method_items[] = {
      {int(geometry::TriangulateNGonMode::Beauty),
       "BEAUTY",
       0,
       "Beauty",
       "Arrange the new triangles evenly (slow)"},
      {int(geometry::TriangulateNGonMode::EarClip),
       "CLIP",
       0,
       "Clip",
       "Split the polygons with an ear clipping algorithm"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  RNA_def_node_enum(srna,
                    "quad_method",
                    "Quad Method",
                    "Method for splitting the quads into triangles",
                    rna_node_geometry_triangulate_quad_method_items,
                    NOD_inline_enum_accessors(custom1),
                    GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT,
                    nullptr,
                    true);

  RNA_def_node_enum(srna,
                    "ngon_method",
                    "N-gon Method",
                    "Method for splitting the n-gons into triangles",
                    rna_node_geometry_triangulate_ngon_method_items,
                    NOD_inline_enum_accessors(custom2),
                    GEO_NODE_POINTS_TO_VOLUME_RESOLUTION_MODE_AMOUNT,
                    nullptr,
                    true);
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeTriangulate", GEO_NODE_TRIANGULATE);
  ntype.ui_name = "Triangulate";
  ntype.ui_description = "Convert all faces in a mesh to triangular faces";
  ntype.enum_name_legacy = "TRIANGULATE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.initfunc = geo_triangulate_init;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  blender::bke::node_register_type(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_triangulate_cc
