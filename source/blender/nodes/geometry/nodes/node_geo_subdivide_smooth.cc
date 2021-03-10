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

#include "MEM_guardedalloc.h"

#include "BKE_mesh.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_mesh.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

static bNodeSocketTemplate geo_node_subdivide_smooth_in[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {SOCK_INT, N_("Level"), 1, 0, 0, 0, 0, 6},
    {SOCK_BOOLEAN, N_("Use Creases")},
    {SOCK_BOOLEAN, N_("Boundary Smooth"), true},
    {SOCK_BOOLEAN, N_("Smooth UVs")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_subdivide_smooth_out[] = {
    {SOCK_GEOMETRY, N_("Geometry")},
    {-1, ""},
};

static void geo_node_subdivide_smooth_layout(uiLayout *layout,
                                             bContext *UNUSED(C),
                                             PointerRNA *UNUSED(ptr))
{
#ifndef WITH_OPENSUBDIV
  uiItemL(layout, IFACE_("Disabled, built without OpenSubdiv"), ICON_ERROR);
#else
  UNUSED_VARS(layout);
#endif
}

namespace blender::nodes {
static void geo_node_subdivide_smooth_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry_set_realize_instances(geometry_set);

  if (!geometry_set.has_mesh()) {
    params.set_output("Geometry", geometry_set);
    return;
  }

#ifndef WITH_OPENSUBDIV
  /* Return input geometry if Blender is built without OpenSubdiv. */
  params.set_output("Geometry", std::move(geometry_set));
  return;
#else
  const int subdiv_level = clamp_i(params.extract_input<int>("Level"), 0, 30);

  /* Only process subdivision if level is greater than 0. */
  if (subdiv_level == 0) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  const bool use_crease = params.extract_input<bool>("Use Creases");
  const bool boundary_smooth = params.extract_input<bool>("Boundary Smooth");
  const bool smooth_uvs = params.extract_input<bool>("Smooth UVs");
  const Mesh *mesh_in = geometry_set.get_mesh_for_read();

  /* Initialize mesh settings. */
  SubdivToMeshSettings mesh_settings;
  mesh_settings.resolution = (1 << subdiv_level) + 1;
  mesh_settings.use_optimal_display = false;

  /* Initialize subdivision settings. */
  SubdivSettings subdiv_settings;
  subdiv_settings.is_simple = false;
  subdiv_settings.is_adaptive = false;
  subdiv_settings.use_creases = use_crease;
  subdiv_settings.level = subdiv_level;

  subdiv_settings.vtx_boundary_interpolation = BKE_subdiv_vtx_boundary_interpolation_from_subsurf(
      !boundary_smooth);
  subdiv_settings.fvar_linear_interpolation = BKE_subdiv_fvar_interpolation_from_uv_smooth(
      smooth_uvs);

  /* Apply subdivision to mesh. */
  Subdiv *subdiv = BKE_subdiv_update_from_mesh(nullptr, &subdiv_settings, mesh_in);

  /* In case of bad topology, skip to input mesh. */
  if (subdiv == nullptr) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  Mesh *mesh_out = BKE_subdiv_to_mesh(subdiv, &mesh_settings, mesh_in);
  BKE_mesh_calc_normals(mesh_out);

  MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
  mesh_component.replace_mesh_but_keep_vertex_group_names(mesh_out);

  // BKE_subdiv_stats_print(&subdiv->stats);
  BKE_subdiv_free(subdiv);

  params.set_output("Geometry", std::move(geometry_set));
#endif
}
}  // namespace blender::nodes

void register_node_type_geo_subdivide_smooth()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SUBDIVIDE_SMOOTH, "Subdivide Smooth", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_subdivide_smooth_in, geo_node_subdivide_smooth_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_subdivide_smooth_exec;
  ntype.draw_buttons = geo_node_subdivide_smooth_layout;
  nodeRegisterType(&ntype);
}
