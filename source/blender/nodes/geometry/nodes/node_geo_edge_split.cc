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

#include "BKE_mesh.h"
#include "BKE_mesh_runtime.h"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_edge_split_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Bool>(N_("Selection")).default_value(true).hide_value().supports_field();
  b.add_output<decl::Geometry>(N_("Mesh"));
}

static Mesh *mesh_edge_split(const Mesh &mesh, const IndexMask selection)
{
  BMeshCreateParams bmesh_create_params{};
  bmesh_create_params.use_toolflags = true;
  const BMAllocTemplate allocsize = {0, 0, 0, 0};
  BMesh *bm = BM_mesh_create(&allocsize, &bmesh_create_params);

  BMeshFromMeshParams bmesh_from_mesh_params{};
  bmesh_from_mesh_params.cd_mask_extra.vmask = CD_MASK_ORIGINDEX;
  bmesh_from_mesh_params.cd_mask_extra.emask = CD_MASK_ORIGINDEX;
  bmesh_from_mesh_params.cd_mask_extra.pmask = CD_MASK_ORIGINDEX;
  BM_mesh_bm_from_me(bm, &mesh, &bmesh_from_mesh_params);

  BM_mesh_elem_table_ensure(bm, BM_EDGE);
  for (const int i : selection) {
    BMEdge *edge = BM_edge_at_index(bm, i);
    BM_elem_flag_enable(edge, BM_ELEM_TAG);
  }

  BM_mesh_edgesplit(bm, false, true, false);

  Mesh *result = BKE_mesh_from_bmesh_for_eval_nomain(bm, nullptr, &mesh);
  BM_mesh_free(bm);

  BKE_mesh_normals_tag_dirty(result);

  return result;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection");

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_mesh()) {
      return;
    }

    const MeshComponent &mesh_component = *geometry_set.get_component_for_read<MeshComponent>();
    GeometryComponentFieldContext field_context{mesh_component, ATTR_DOMAIN_EDGE};
    const int domain_size = mesh_component.attribute_domain_size(ATTR_DOMAIN_EDGE);
    fn::FieldEvaluator selection_evaluator{field_context, domain_size};
    selection_evaluator.add(selection_field);
    selection_evaluator.evaluate();
    const IndexMask selection = selection_evaluator.get_evaluated_as_mask(0);

    geometry_set.replace_mesh(mesh_edge_split(*mesh_component.get_for_read(), selection));
  });

  params.set_output("Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_edge_split_cc

void register_node_type_geo_edge_split()
{
  namespace file_ns = blender::nodes::node_geo_edge_split_cc;

  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SPLIT_EDGES, "Split Edges", NODE_CLASS_GEOMETRY);
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.declare = file_ns::node_declare;
  nodeRegisterType(&ntype);
}
