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
#include "BKE_subdiv.h"
#include "BKE_subdiv_mesh.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_mesh_subdivide_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Int>(N_("Level")).default_value(1).min(0).max(6);
  b.add_output<decl::Geometry>(N_("Mesh"));
}

static void geometry_set_mesh_subdivide(GeometrySet &geometry_set, const int level)
{
  if (!geometry_set.has_mesh()) {
    return;
  }

  const Mesh *mesh_in = geometry_set.get_mesh_for_read();

  /* Initialize mesh settings. */
  SubdivToMeshSettings mesh_settings;
  mesh_settings.resolution = (1 << level) + 1;
  mesh_settings.use_optimal_display = false;

  /* Initialize subdivision settings. */
  SubdivSettings subdiv_settings;
  subdiv_settings.is_simple = true;
  subdiv_settings.is_adaptive = false;
  subdiv_settings.use_creases = false;
  subdiv_settings.level = 1;
  subdiv_settings.vtx_boundary_interpolation = BKE_subdiv_vtx_boundary_interpolation_from_subsurf(
      0);
  subdiv_settings.fvar_linear_interpolation = BKE_subdiv_fvar_interpolation_from_uv_smooth(0);

  /* Apply subdivision from mesh. */
  Subdiv *subdiv = BKE_subdiv_update_from_mesh(nullptr, &subdiv_settings, mesh_in);

  /* In case of bad topology, skip to input mesh. */
  if (subdiv == nullptr) {
    return;
  }

  Mesh *mesh_out = BKE_subdiv_to_mesh(subdiv, &mesh_settings, mesh_in);
  BKE_mesh_normals_tag_dirty(mesh_out);

  MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
  mesh_component.replace(mesh_out);

  BKE_subdiv_free(subdiv);
}

static void geo_node_mesh_subdivide_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

#ifndef WITH_OPENSUBDIV
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenSubdiv"));
  params.set_output("Mesh", std::move(geometry_set));
  return;
#endif

  /* See CCGSUBSURF_LEVEL_MAX for max limit. */
  const int subdiv_level = clamp_i(params.extract_input<int>("Level"), 0, 11);

  if (subdiv_level == 0) {
    params.set_output("Mesh", std::move(geometry_set));
    return;
  }

  geometry_set.modify_geometry_sets(
      [&](GeometrySet &geometry_set) { geometry_set_mesh_subdivide(geometry_set, subdiv_level); });

  params.set_output("Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_subdivide()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SUBDIVIDE_MESH, "Subdivide Mesh", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_mesh_subdivide_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_mesh_subdivide_exec;
  nodeRegisterType(&ntype);
}
