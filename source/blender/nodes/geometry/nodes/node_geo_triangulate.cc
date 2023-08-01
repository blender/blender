/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_customdata.h"
#include "BKE_mesh.hh"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DNA_mesh_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

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
    const Mesh &mesh_in = *geometry_set.get_mesh_for_read();

    const bke::MeshFieldContext context{mesh_in, ATTR_DOMAIN_FACE};
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
}  // namespace blender::nodes::node_geo_triangulate_cc

void register_node_type_geo_triangulate()
{
  namespace file_ns = blender::nodes::node_geo_triangulate_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_TRIANGULATE, "Triangulate", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.initfunc = file_ns::geo_triangulate_init;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
