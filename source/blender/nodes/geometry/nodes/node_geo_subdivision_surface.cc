/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array_utils.hh"
#include "BLI_task.hh"

#include "DNA_modifier_types.h"

#include "BKE_attribute.hh"
#include "BKE_lib_id.h"
#include "BKE_mesh.hh"
#include "BKE_subdiv.h"
#include "BKE_subdiv_mesh.hh"

#include "UI_interface.h"
#include "UI_resources.h"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_subdivision_surface_cc {

NODE_STORAGE_FUNCS(NodeGeometrySubdivisionSurface)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>(N_("Mesh")).supported_type(GEO_COMPONENT_TYPE_MESH);
  b.add_input<decl::Int>(N_("Level")).default_value(1).min(0).max(6);
  b.add_input<decl::Float>(N_("Edge Crease"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .field_on_all()
      .subtype(PROP_FACTOR);
  b.add_input<decl::Float>(N_("Vertex Crease"))
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .field_on_all()
      .subtype(PROP_FACTOR);
  b.add_output<decl::Geometry>(N_("Mesh")).propagate_all();
}

static void node_layout(uiLayout *layout, bContext * /*C*/, PointerRNA *ptr)
{
  uiItemR(layout, ptr, "uv_smooth", 0, "", ICON_NONE);
  uiItemR(layout, ptr, "boundary_smooth", 0, "", ICON_NONE);
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  NodeGeometrySubdivisionSurface *data = MEM_cnew<NodeGeometrySubdivisionSurface>(__func__);
  data->uv_smooth = SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES;
  data->boundary_smooth = SUBSURF_BOUNDARY_SMOOTH_ALL;
  node->storage = data;
}

#ifdef WITH_OPENSUBDIV

static void write_vert_creases(Mesh &mesh, const VArray<float> &creases)
{
  CustomData_free_layers(&mesh.vdata, CD_CREASE, mesh.totvert);
  float *layer = static_cast<float *>(
      CustomData_add_layer(&mesh.vdata, CD_CREASE, CD_CONSTRUCT, mesh.totvert));
  array_utils::copy(creases, {layer, mesh.totvert});
}

static void write_edge_creases(Mesh &mesh, const VArray<float> &creases)
{
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  attributes.remove("crease");
  attributes.add<float>("crease", ATTR_DOMAIN_EDGE, bke::AttributeInitVArray(creases));
}

static bool varray_is_single_zero(const VArray<float> &varray)
{
  if (const std::optional<float> value = varray.get_if_single()) {
    return *value == 0.0f;
  }
  return false;
}

static fn::Field<float> clamp_crease(fn::Field<float> crease_field)
{
  static auto clamp_fn = mf::build::SI1_SO<float, float>(
      "Clamp",
      [](float value) { return std::clamp(value, 0.0f, 1.0f); },
      mf::build::exec_presets::AllSpanOrSingle());
  return fn::Field<float>(fn::FieldOperation::Create(clamp_fn, {std::move(crease_field)}));
}

static Mesh *mesh_subsurf_calc(const Mesh *mesh,
                               const int level,
                               const Field<float> &vert_crease_field,
                               const Field<float> &edge_crease_field,
                               const int boundary_smooth,
                               const int uv_smooth)
{
  const bke::MeshFieldContext point_context{*mesh, ATTR_DOMAIN_POINT};
  FieldEvaluator point_evaluator(point_context, mesh->totvert);
  point_evaluator.add(clamp_crease(vert_crease_field));
  point_evaluator.evaluate();

  const bke::MeshFieldContext edge_context{*mesh, ATTR_DOMAIN_EDGE};
  FieldEvaluator edge_evaluator(edge_context, mesh->totedge);
  edge_evaluator.add(clamp_crease(edge_crease_field));
  edge_evaluator.evaluate();

  const VArray<float> vert_creases = point_evaluator.get_evaluated<float>(0);
  const VArray<float> edge_creases = edge_evaluator.get_evaluated<float>(0);
  const bool use_creases = !varray_is_single_zero(vert_creases) ||
                           !varray_is_single_zero(edge_creases);

  Mesh *mesh_copy = nullptr;
  if (use_creases) {
    /* Due to the "BKE_subdiv" API, the crease layers must be on the input mesh. But in this node
     * they are provided as separate inputs, not as custom data layers. When needed, retrieve the
     * mesh with write access and store the new crease values there. */
    mesh_copy = BKE_mesh_copy_for_eval(mesh);
    write_vert_creases(*mesh_copy, vert_creases);
    write_edge_creases(*mesh_copy, edge_creases);
    mesh = mesh_copy;
  }

  SubdivToMeshSettings mesh_settings;
  mesh_settings.resolution = (1 << level) + 1;
  mesh_settings.use_optimal_display = false;

  SubdivSettings subdiv_settings;
  subdiv_settings.is_simple = false;
  subdiv_settings.is_adaptive = false;
  subdiv_settings.use_creases = use_creases;
  subdiv_settings.level = level;
  subdiv_settings.vtx_boundary_interpolation = BKE_subdiv_vtx_boundary_interpolation_from_subsurf(
      boundary_smooth);
  subdiv_settings.fvar_linear_interpolation = BKE_subdiv_fvar_interpolation_from_uv_smooth(
      uv_smooth);

  Subdiv *subdiv = BKE_subdiv_new_from_mesh(&subdiv_settings, mesh);
  if (!subdiv) {
    return nullptr;
  }

  Mesh *result = BKE_subdiv_to_mesh(subdiv, &mesh_settings, mesh);
  BKE_subdiv_free(subdiv);

  if (use_creases) {
    /* Remove the layer in case it was created by the node from the field input. The fact
     * that this node uses #CD_CREASE to input creases to the subdivision code is meant to be
     * an implementation detail ideally. */
    CustomData_free_layers(&result->edata, CD_CREASE, result->totedge);
  }

  if (mesh_copy) {
    BKE_id_free(nullptr, mesh_copy);
  }

  return result;
}

#endif

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
#ifdef WITH_OPENSUBDIV
  const Field<float> vert_crease = params.extract_input<Field<float>>("Vertex Crease");
  const Field<float> edge_crease = params.extract_input<Field<float>>("Edge Crease");

  const NodeGeometrySubdivisionSurface &storage = node_storage(params.node());
  const int uv_smooth = storage.uv_smooth;
  const int boundary_smooth = storage.boundary_smooth;
  const int level = std::clamp(params.extract_input<int>("Level"), 0, 11);
  if (level == 0) {
    params.set_output("Mesh", std::move(geometry_set));
    return;
  }

  geometry_set.modify_geometry_sets([&](GeometrySet &geometry_set) {
    if (const Mesh *mesh = geometry_set.get_mesh_for_read()) {
      geometry_set.replace_mesh(
          mesh_subsurf_calc(mesh, level, vert_crease, edge_crease, boundary_smooth, uv_smooth));
    }
  });
#else
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenSubdiv"));

#endif
  params.set_output("Mesh", std::move(geometry_set));
}

}  // namespace blender::nodes::node_geo_subdivision_surface_cc

void register_node_type_geo_subdivision_surface()
{
  namespace file_ns = blender::nodes::node_geo_subdivision_surface_cc;

  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_SUBDIVISION_SURFACE, "Subdivision Surface", NODE_CLASS_GEOMETRY);
  ntype.declare = file_ns::node_declare;
  ntype.geometry_node_execute = file_ns::node_geo_exec;
  ntype.draw_buttons = file_ns::node_layout;
  ntype.initfunc = file_ns::node_init;
  node_type_size_preset(&ntype, NODE_SIZE_MIDDLE);
  node_type_storage(&ntype,
                    "NodeGeometrySubdivisionSurface",
                    node_free_standard_storage,
                    node_copy_standard_storage);
  nodeRegisterType(&ntype);
}
