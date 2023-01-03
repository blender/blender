/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_attribute.hh"
#include "BKE_mesh.h"
#include "BKE_subdiv.h"
#include "BKE_subdiv_mesh.h"

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
static void materialize_and_clamp_creases(const VArray<float> &crease_varray,
                                          MutableSpan<float> creases)
{
  threading::parallel_for(creases.index_range(), 1024, [&](IndexRange range) {
    crease_varray.materialize(range, creases);
    for (const int i : range) {
      creases[i] = std::clamp(creases[i], 0.0f, 1.0f);
    }
  });
}

static void write_vertex_creases(Mesh &mesh, const VArray<float> &crease_varray)
{
  float *crease;
  if (CustomData_has_layer(&mesh.vdata, CD_CREASE)) {
    crease = static_cast<float *>(CustomData_get_layer(&mesh.vdata, CD_CREASE));
  }
  else {
    crease = static_cast<float *>(
        CustomData_add_layer(&mesh.vdata, CD_CREASE, CD_CONSTRUCT, nullptr, mesh.totvert));
  }
  materialize_and_clamp_creases(crease_varray, {crease, mesh.totvert});
}

static void write_edge_creases(MeshComponent &mesh, const VArray<float> &crease_varray)
{
  bke::SpanAttributeWriter<float> attribute =
      mesh.attributes_for_write()->lookup_or_add_for_write_only_span<float>("crease",
                                                                            ATTR_DOMAIN_EDGE);
  materialize_and_clamp_creases(crease_varray, attribute.span);
  attribute.finish();
}

static bool varray_is_nonzero(const VArray<float> &varray)
{
  return !(varray.is_single() && varray.get_internal_single() == 0.0f);
}
#endif

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
#ifndef WITH_OPENSUBDIV
  params.error_message_add(NodeWarningType::Error,
                           TIP_("Disabled, Blender was compiled without OpenSubdiv"));
#else
  Field<float> edge_crease_field = params.extract_input<Field<float>>("Edge Crease");
  Field<float> vertex_crease_field = params.extract_input<Field<float>>("Vertex Crease");

  const NodeGeometrySubdivisionSurface &storage = node_storage(params.node());
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

    const Mesh &mesh = *geometry_set.get_mesh_for_read();
    if (mesh.totvert == 0 || mesh.totedge == 0) {
      return;
    }

    bke::MeshFieldContext point_context{mesh, ATTR_DOMAIN_POINT};
    FieldEvaluator point_evaluator(point_context, mesh.totvert);
    point_evaluator.add(vertex_crease_field);
    point_evaluator.evaluate();
    const VArray<float> vertex_creases = point_evaluator.get_evaluated<float>(0);

    bke::MeshFieldContext edge_context{mesh, ATTR_DOMAIN_EDGE};
    FieldEvaluator edge_evaluator(edge_context, mesh.totedge);
    edge_evaluator.add(edge_crease_field);
    edge_evaluator.evaluate();
    const VArray<float> edge_creases = edge_evaluator.get_evaluated<float>(0);

    const bool use_creases = varray_is_nonzero(vertex_creases) || varray_is_nonzero(edge_creases);

    if (use_creases) {
      write_vertex_creases(*geometry_set.get_mesh_for_write(), vertex_creases);
      write_edge_creases(geometry_set.get_component_for_write<MeshComponent>(), edge_creases);
    }

    /* Initialize mesh settings. */
    SubdivToMeshSettings mesh_settings;
    mesh_settings.resolution = (1 << subdiv_level) + 1;
    mesh_settings.use_optimal_display = false;

    /* Initialize subdivision settings. */
    SubdivSettings subdiv_settings;
    subdiv_settings.is_simple = false;
    subdiv_settings.is_adaptive = false;
    subdiv_settings.use_creases = use_creases;
    subdiv_settings.level = subdiv_level;

    subdiv_settings.vtx_boundary_interpolation =
        BKE_subdiv_vtx_boundary_interpolation_from_subsurf(boundary_smooth);
    subdiv_settings.fvar_linear_interpolation = BKE_subdiv_fvar_interpolation_from_uv_smooth(
        uv_smooth);

    /* Apply subdivision to mesh. */
    Subdiv *subdiv = BKE_subdiv_update_from_mesh(nullptr, &subdiv_settings, &mesh);

    /* In case of bad topology, skip to input mesh. */
    if (subdiv == nullptr) {
      return;
    }

    Mesh *mesh_out = BKE_subdiv_to_mesh(subdiv, &mesh_settings, &mesh);

    if (use_creases) {
      /* Remove the layer in case it was created by the node from the field input. The fact
       * that this node uses #CD_CREASE to input creases to the subdivision code is meant to be
       * an implementation detail ideally. */
      CustomData_free_layers(&mesh_out->edata, CD_CREASE, mesh_out->totedge);
    }

    geometry_set.replace_mesh(mesh_out);

    BKE_subdiv_free(subdiv);
  });
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
