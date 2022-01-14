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

#include "DNA_modifier_types.h"
#include "UI_interface.h"
#include "UI_resources.h"
#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_legacy_subdivision_surface_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Geometry"));
  b.add_input<decl::Int>(N_("Level")).default_value(1).min(0).max(6);
  b.add_input<decl::Bool>(N_("Use Creases"));
  b.add_output<decl::Geometry>(N_("Geometry"));
}

static void node_layout(uiLayout *layout, bContext *UNUSED(C), PointerRNA *ptr)
{
#ifdef WITH_OPENSUBDIV
  uiLayoutSetPropSep(layout, true);
  uiLayoutSetPropDecorate(layout, false);
  uiItemR(layout, ptr, "uv_smooth", 0, nullptr, ICON_NONE);
  uiItemR(layout, ptr, "boundary_smooth", 0, nullptr, ICON_NONE);
#else
  UNUSED_VARS(layout, ptr);
#endif
}

static void node_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometrySubdivisionSurface *data = MEM_cnew<NodeGeometrySubdivisionSurface>(__func__);
  data->uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES;
  data->boundary_smooth = SUBSURF_BOUNDARY_SMOOTH_ALL;
  node->storage = data;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");

  geometry_set = geometry::realize_instances_legacy(geometry_set);

  if (!geometry_set.has_mesh()) {
    params.set_output("Geometry", geometry_set);
    return;
  }

#ifndef WITH_OPENSUBDIV
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenSubdiv"));
#else
  const NodeGeometrySubdivisionSurface &storage =
      *(const NodeGeometrySubdivisionSurface *)params.node().storage;
  const int uv_smooth = storage.uv_smooth;
  const int boundary_smooth = storage.boundary_smooth;
  const int subdiv_level = clamp_i(params.extract_input<int>("Level"), 0, 30);

  /* Only process subdivision if level is greater than 0. */
  if (subdiv_level == 0) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  const bool use_crease = params.extract_input<bool>("Use Creases");
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
      boundary_smooth);
  subdiv_settings.fvar_linear_interpolation = BKE_subdiv_fvar_interpolation_from_uv_smooth(
      uv_smooth);

  /* Apply subdivision to mesh. */
  Subdiv *subdiv = BKE_subdiv_update_from_mesh(nullptr, &subdiv_settings, mesh_in);

  /* In case of bad topology, skip to input mesh. */
  if (subdiv == nullptr) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  Mesh *mesh_out = BKE_subdiv_to_mesh(subdiv, &mesh_settings, mesh_in);
  BKE_mesh_normals_tag_dirty(mesh_out);

  MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
  mesh_component.replace(mesh_out);

  // BKE_subdiv_stats_print(&subdiv->stats);
  BKE_subdiv_free(subdiv);

#endif

  params.set_output("Geometry", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_legacy_subdivision_surface_cc

void register_node_type_geo_legacy_subdivision_surface()
{
  namespace file_ns = blender::nodes::node_geo_legacy_subdivision_surface_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_SUBDIVISION_SURFACE, "Subdivision Surface", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  node_type_init(&ntype, file_ns::node_init);
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_storage(&ntype,
                    "NodeGeometrySubdivisionSurface",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
