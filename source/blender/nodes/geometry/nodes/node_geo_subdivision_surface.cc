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

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_mesh.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_mesh.h"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes {

static void geo_node_subdivision_surface_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Int>(N_("Level")).default_value(1).min(0).max(6);
  b.add_input<decl::Float>(N_("Crease"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .supports_field()
      .subtype(PROP_FACTOR);
  b.add_output<decl::Geometry>(N_("Mesh"));
}

static void geo_node_subdivision_surface_layout(uiLayout *layout,
                                                bContext *UNUSED(C),
                                                PointerRNA *ptr)
{
  uiItemR(layout, ptr, "uv_smooth", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "boundary_smooth", 0, "", ICON_NONE);
}

static void geo_node_subdivision_surface_init(bNodeTree *UNUSED(ntree), bNode *node)
{
  NodeGeometrySubdivisionSurface *data = (NodeGeometrySubdivisionSurface *)MEM_callocN(
      sizeof(NodeGeometrySubdivisionSurface), __func__);
  data->uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES;
  data->boundary_smooth = SUBSURF_BOUNDARY_SMOOTH_ALL;
  node->storage = data;
}

static void geo_node_subdivision_surface_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
#ifndef WITH_OPENSUBDIV
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenSubdiv"));
#else
  Field<float> crease_field = params.extract_input<Field<float>>("Crease");

  const NodeGeometrySubdivisionSurface &storage =
      *(const NodeGeometrySubdivisionSurface *)params.node().storage;
  const int uv_smooth = storage.uv_smooth;
  const int boundary_smooth = storage.boundary_smooth;
  const int subdiv_level = clamp_i(params.extract_input<int>("Level"), 0, 30);

  /* Only process subdivision if level is greater than 0. */
  if (subdiv_level == 0) {
    params.set_output("Mesh", std::move(geometry_set));
    return;
  }

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (!geometry_set.has_mesh()) {
      return;
    }

    MeshComponent &mesh_component = geometry_set.get_component_for_write<MeshComponent>();
    AttributeDomain domain = ATTR_DOMAIN_EDGE;
    GeometryComponentFieldContext field_context{mesh_component, domain};
    const int domain_size = mesh_component.attribute_domain_size(domain);

    if (domain_size == 0) {
      return;
    }

    FieldEvaluator evaluator(field_context, domain_size);
    evaluator.add(crease_field);
    evaluator.evaluate();
    const VArray<float> &creases = evaluator.get_evaluated<float>(0);

    OutputAttribute_Typed<float> crease = mesh_component.attribute_try_get_for_output_only<float>(
        "crease", domain);
    MutableSpan<float> crease_span = crease.as_span();
    for (auto i : creases.index_range()) {
      crease_span[i] = std::clamp(creases[i], 0.0f, 1.0f);
    }
    crease.save();

    /* Initialize mesh settings. */
    SubdivToMeshSettings mesh_settings;
    mesh_settings.resolution = (1 << subdiv_level) + 1;
    mesh_settings.use_optimal_display = false;

    /* Initialize subdivision settings. */
    SubdivSettings subdiv_settings;
    subdiv_settings.is_simple = false;
    subdiv_settings.is_adaptive = false;
    subdiv_settings.use_creases = !(creases.is_single() && creases.get_internal_single() == 0.0f);
    subdiv_settings.level = subdiv_level;

    subdiv_settings.vtx_boundary_interpolation =
        BKE_subdiv_vtx_boundary_interpolation_from_subsurf(boundary_smooth);
    subdiv_settings.fvar_linear_interpolation = BKE_subdiv_fvar_interpolation_from_uv_smooth(
        uv_smooth);

    Mesh *mesh_in = mesh_component.get_for_write();

    /* Apply subdivision to mesh. */
    Subdiv *subdiv = BKE_subdiv_update_from_mesh(nullptr, &subdiv_settings, mesh_in);

    /* In case of bad topology, skip to input mesh. */
    if (subdiv == nullptr) {
      return;
    }

    Mesh *mesh_out = BKE_subdiv_to_mesh(subdiv, &mesh_settings, mesh_in);
    BKE_mesh_normals_tag_dirty(mesh_out);

    mesh_component.replace(mesh_out);

    BKE_subdiv_free(subdiv);
  });
#endif
  params.set_output("Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes

void register_node_type_geo_subdivision_surface()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SUBDIVISION_SURFACE, "Subdivision Surface", NODE_CLASS_GEOMETRY, 0);
  ntype.declare = blender::nodes::geo_node_subdivision_surface_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_subdivision_surface_exec;
  ntype.draw_buttons = blender::nodes::geo_node_subdivision_surface_layout;
  node_type_init(&ntype, blender::nodes::geo_node_subdivision_surface_init);
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_storage(&ntype,
                    "NodeGeometrySubdivisionSurface",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
