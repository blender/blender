/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_customdata.hh"
#include "BKE_mesh.hh"

#include "bmesh.hh"
#include "bmesh_tools.hh"

#include "DNA_mesh_types.h"

#include "NOD_rna_define.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GEO_randomize.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_triangulate_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh").supported_type(GeometryComponent::Type::Mesh);
  b.add_input<decl::Bool>("Selection").default_value(true).field_on_all().hide_value();
  b.add_input<decl::Int>("Minimum Vertices").default_value(4).min(4).max(10000);
  b.add_output<decl::Geometry>("Mesh").propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "quad_method", UI_ITEM_NONE, "", ICON_NONE);
  uiItemR(layout, ptr, "ngon_method", UI_ITEM_NONE, "", ICON_NONE);
}

static void geo_triangulate_init(bNodeTree * /*tree*/, bNode *node)
{
  node->custom1 = GEO_NODE_TRIANGULATE_QUAD_SHORTEDGE;
  node->custom2 = GEO_NODE_TRIANGULATE_NGON_BEAUTY;
}

static Mesh *triangulate_mesh_selection(const Mesh &mesh,
                                        const int quad_method,
                                        const int ngon_method,
                                        const IndexMask &selection,
                                        const int min_vertices)
{
  CustomData_MeshMasks cd_mask_extra = {
      CD_MASK_ORIGINDEX, CD_MASK_ORIGINDEX, 0, CD_MASK_ORIGINDEX};
  BMeshCreateParams create_params{false};
  BMeshFromMeshParams from_mesh_params{};
  from_mesh_params.calc_face_normal = true;
  from_mesh_params.calc_vert_normal = true;
  from_mesh_params.cd_mask_extra = cd_mask_extra;
  BMesh *bm = BKE_mesh_to_bmesh_ex(&mesh, &create_params, &from_mesh_params);

  /* Tag faces to be triangulated from the selection mask. */
  BM_mesh_elem_table_ensure(bm, BM_FACE);
  selection.foreach_index([&](const int i_face) {
    BM_elem_flag_set(BM_face_at_index(bm, i_face), BM_ELEM_TAG, true);
  });

  BM_mesh_triangulate(bm, quad_method, ngon_method, min_vertices, true, nullptr, nullptr, nullptr);
  Mesh *result = BKE_mesh_from_bmesh_for_eval_nomain(bm, &cd_mask_extra, &mesh);
  BM_mesh_free(bm);

  /* Positions are not changed by the triangulation operation, so the bounds are the same. */
  result->runtime->bounds_cache = mesh.runtime->bounds_cache;

  /* Vertex order is not affected. */
  geometry::debug_randomize_edge_order(result);
  geometry::debug_randomize_face_order(result);

  return result;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  const int min_vertices = std::max(params.extract_input<int>("Minimum Vertices"), 4);

  GeometryNodeTriangulateQuads quad_method = GeometryNodeTriangulateQuads(params.node().custom1);
  GeometryNodeTriangulateNGons ngon_method = GeometryNodeTriangulateNGons(params.node().custom2);

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_mesh()) {
      return;
    }
    const Mesh &mesh_in = *geometry_set.get_mesh();

    const bke::MeshFieldContext context{mesh_in, AttrDomain::Face};
    FieldEvaluator evaluator{context, mesh_in.faces_num};
    evaluator.add(selection_field);
    evaluator.evaluate();
    const IndexMask selection = evaluator.get_evaluated_as_mask(0);

    Mesh *mesh_out = triangulate_mesh_selection(
        mesh_in, quad_method, ngon_method, selection, min_vertices);
    geometry_set.replace_mesh(mesh_out);
  });

  params.set_output("Mesh", std::move(geometry_set));
}

static void node_rna(StructRNA *srna)
{
  static const EnumPropertyItem rna_node_geometry_triangulate_quad_method_items[] = {
      {GEO_NODE_TRIANGULATE_QUAD_BEAUTY,
       "BEAUTY",
       0,
       "Beauty",
       "Split the quads in nice triangles, slower method"},
      {GEO_NODE_TRIANGULATE_QUAD_FIXED,
       "FIXED",
       0,
       "Fixed",
       "Split the quads on the first and third vertices"},
      {GEO_NODE_TRIANGULATE_QUAD_ALTERNATE,
       "FIXED_ALTERNATE",
       0,
       "Fixed Alternate",
       "Split the quads on the 2nd and 4th vertices"},
      {GEO_NODE_TRIANGULATE_QUAD_SHORTEDGE,
       "SHORTEST_DIAGONAL",
       0,
       "Shortest Diagonal",
       "Split the quads along their shortest diagonal"},
      {GEO_NODE_TRIANGULATE_QUAD_LONGEDGE,
       "LONGEST_DIAGONAL",
       0,
       "Longest Diagonal",
       "Split the quads along their longest diagonal"},
      {0, nullptr, 0, nullptr, nullptr},
  };

  static const EnumPropertyItem rna_node_geometry_triangulate_ngon_method_items[] = {
      {GEO_NODE_TRIANGULATE_NGON_BEAUTY,
       "BEAUTY",
       0,
       "Beauty",
       "Arrange the new triangles evenly (slow)"},
      {GEO_NODE_TRIANGULATE_NGON_EARCLIP,
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

  geo_node_type_base(&ntype, GEO_NODE_TRIANGULATE, "Triangulate", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.initfunc = geo_triangulate_init;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.draw_buttons = node_layout;
  blender::bke::nodeRegisterType(&ntype);

  node_rna(ntype.rna_ext.srna);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_triangulate_cc
