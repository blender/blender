/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BKE_mesh.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_mesh.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "GEO_randomize.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_mesh_subdivide_cc {

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Mesh").supported_type(GeometryComponent::Type::Mesh);
  b.add_input<decl::Int>("Level").default_value(1).min(0).max(6);
  b.add_output<decl::Geometry>("Mesh").propagate_all();
}

#ifdef WITH_OPENSUBDIV
static Mesh *simple_subdivide_mesh(const Mesh &mesh, const int level)
{
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
  Subdiv *subdiv = BKE_subdiv_new_from_mesh(&subdiv_settings, &mesh);
  if (!subdiv) {
    return nullptr;
  }

  Mesh *result = BKE_subdiv_to_mesh(subdiv, &mesh_settings, &mesh);

  BKE_subdiv_free(subdiv);

  geometry::debug_randomize_mesh_order(result);
  return result;
}
#endif /* WITH_OPENSUBDIV */

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
#ifdef WITH_OPENSUBDIV
  /* See CCGSUBSURF_LEVEL_MAX for max limit. */
  const int level = clamp_i(params.extract_input<int>("Level"), 0, 11);
  if (level == 0) {
    params.set_output("Mesh", std::move(geometry_set));
    return;
  }

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (const Mesh *mesh = geometry_set.get_mesh()) {
      geometry_set.replace_mesh(simple_subdivide_mesh(*mesh, level));
    }
  });
#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenSubdiv"));
#endif
  params.set_output("Mesh", std::move(geometry_set));
}

static void node_register()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_SUBDIVIDE_MESH, "Subdivide Mesh", NODE_CLASS_GEOMETRY);
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  nodeRegisterType(&ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_mesh_subdivide_cc
