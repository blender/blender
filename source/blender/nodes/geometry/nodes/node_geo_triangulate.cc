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

#include "BKE_customdata.h"
#include "BKE_mesh.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DNA_mesh_types.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_triangulate_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).supports_field().hide_value();
  b.add_input<decl::Int>(N_("Minimum Vertices")).default_value(4).min(4).max(10000);
  b.add_output<decl::Geometry>(N_("Mesh"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
  uiItemR(layout, ptr, "quad_method", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "ngon_method", 0, "", ICON_NONE);
}

static void geo_triangulate_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  node->custom1 = GEO_NODE_TRIANGULATE_QUAD_SHORTEDGE;
  node->custom2 = GEO_NODE_TRIANGULATE_NGON_BEAUTY;
}

static Mesh *triangulate_mesh_selection(const Mesh &mesh,
                                        const int quad_method,
                                        const int ngon_method,
                                        const IndexMask selection,
                                        const int min_vertices)
{
  CustomData_MeshMasks cd_mask_extra = {
      CD_MASK_ORIGINDEX, CD_MASK_ORIGINDEX, 0, CD_MASK_ORIGINDEX};
  BMeshCreateParams create_params{0};
  BMeshFromMeshParams from_mesh_params{true, 1, 1, 1, cd_mask_extra};
  BMesh *bm = BKE_mesh_to_bmesh_ex(&mesh, &create_params, &from_mesh_params);

  /* Tag faces to be triangulated from the selection mask. */
  BM_mesh_elem_table_ensure(bm, BM_FACE);
  for (int i_face : selection) {
    BM_elem_flag_set(BM_face_at_index(bm, i_face), BM_ELEM_TAG, true);
  }

  BM_mesh_triangulate(bm, quad_method, ngon_method, min_vertices, true, NULL, NULL, NULL);
  Mesh *result = BKE_mesh_from_bmesh_for_eval_nomain(bm, &cd_mask_extra, &mesh);
  BM_mesh_free(bm);
  BKE_mesh_normals_tag_dirty(result);
  return result;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
  Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");
  const int min_vertices = std::max(params.extract_input<int>("Minimum Vertices"), 4);

  GeometryNodeTriangulateQuads quad_method = static_cast<GeometryNodeTriangulateQuads>(
      params.node().custom1);
  GeometryNodeTriangulateNGons ngon_method = static_cast<GeometryNodeTriangulateNGons>(
      params.node().custom2);

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_mesh()) {
      return;
    }
    GeometryComponent &component = geometry_set.get_component_for_write<MeshComponent>();
    const Mesh &mesh_in = *geometry_set.get_mesh_for_read();

    const int domain_size = component.attribute_domain_size(ATTR_DOMAIN_FACE);
    GeometryComponentFieldContext context{component, ATTR_DOMAIN_FACE};
    FieldEvaluator evaluator{context, domain_size};
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
  node_type_init(&ntype, file_ns::geo_triangulate_init);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  nodeRegisterType(&ntype);
}
