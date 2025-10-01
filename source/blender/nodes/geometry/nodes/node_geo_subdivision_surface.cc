/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_modifier_types.h"

#include "BKE_attribute.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_subdiv.hh"
#include "BKE_subdiv_mesh.hh"

#include "RNA_enum_types.hh"

#include "GEO_foreach_geometry.hh"
#include "GEO_randomize.hh"

#include "FN_multi_function_builder.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_subdivision_surface_cc {

NODE_STORAGE_FUNCS(NodeGeometrySubdivisionSurface)

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();
  b.add_input<decl::Geometry>("Mesh")
      .supported_type(GeometryComponent::Type::Mesh)
      .description("Mesh to subdivide");
  b.add_output<decl::Geometry>("Mesh").propagate_all().align_with_previous();
  b.add_input<decl::Int>("Level").default_value(1).min(0).max(6);
  b.add_input<decl::Float>("Edge Crease")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .field_on_all();
  b.add_input<decl::Float>("Vertex Crease")
      .default_value(0.0f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .field_on_all();
  b.add_input<decl::Bool>("Limit Surface")
      .default_value(true)
      .description(
          "Place vertices at the surface that would be produced with infinite "
          "levels of subdivision (smoothest possible shape)");
  b.add_input<decl::Menu>("UV Smooth")
      .static_items(rna_enum_subdivision_uv_smooth_items)
      .default_value(SUBSURF_UV_SMOOTH_PRESERVE_BOUNDARIES)
      .optional_label()
      .description("Controls how smoothing is applied to UVs");
  b.add_input<decl::Menu>("Boundary Smooth")
      .static_items(rna_enum_subdivision_boundary_smooth_items)
      .default_value(SUBSURF_BOUNDARY_SMOOTH_ALL)
      .optional_label()
      .description("Controls how open boundaries are smoothed");
}

static void node_init(bNodeTree * /*tree*/, bNode *node)
{
  /* Still used for forward compatibility. */
  node->storage = MEM_callocN<NodeGeometrySubdivisionSurface>(__func__);
}

#ifdef WITH_OPENSUBDIV

static void write_vert_creases(Mesh &mesh, const VArray<float> &creases)
{
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  attributes.remove("crease_vert");
  attributes.add<float>("crease_vert", AttrDomain::Point, bke::AttributeInitVArray(creases));
}

static void write_edge_creases(Mesh &mesh, const VArray<float> &creases)
{
  bke::MutableAttributeAccessor attributes = mesh.attributes_for_write();
  attributes.remove("crease_edge");
  attributes.add<float>("crease_edge", AttrDomain::Edge, bke::AttributeInitVArray(creases));
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
  return fn::Field<float>(fn::FieldOperation::from(clamp_fn, {std::move(crease_field)}));
}

static Mesh *mesh_subsurf_calc(const Mesh *mesh,
                               const int level,
                               const Field<float> &vert_crease_field,
                               const Field<float> &edge_crease_field,
                               const int boundary_smooth,
                               const int uv_smooth,
                               const bool use_limit_surface)
{
  const bke::MeshFieldContext point_context{*mesh, AttrDomain::Point};
  FieldEvaluator point_evaluator(point_context, mesh->verts_num);
  point_evaluator.add(clamp_crease(vert_crease_field));
  point_evaluator.evaluate();

  const bke::MeshFieldContext edge_context{*mesh, AttrDomain::Edge};
  FieldEvaluator edge_evaluator(edge_context, mesh->edges_num);
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
    mesh_copy = BKE_mesh_copy_for_eval(*mesh);
    write_vert_creases(*mesh_copy, vert_creases);
    write_edge_creases(*mesh_copy, edge_creases);
    mesh = mesh_copy;
  }

  bke::subdiv::ToMeshSettings mesh_settings;
  mesh_settings.resolution = (1 << level) + 1;
  mesh_settings.use_optimal_display = false;

  bke::subdiv::Settings subdiv_settings;
  subdiv_settings.is_simple = false;
  subdiv_settings.is_adaptive = use_limit_surface;
  subdiv_settings.use_creases = use_creases;
  subdiv_settings.level = level;
  subdiv_settings.vtx_boundary_interpolation =
      bke::subdiv::vtx_boundary_interpolation_from_subsurf(boundary_smooth);
  subdiv_settings.fvar_linear_interpolation = bke::subdiv::fvar_interpolation_from_uv_smooth(
      uv_smooth);

  bke::subdiv::Subdiv *subdiv = bke::subdiv::new_from_mesh(&subdiv_settings, mesh);
  if (!subdiv) {
    return nullptr;
  }

  Mesh *result = bke::subdiv::subdiv_to_mesh(subdiv, &mesh_settings, mesh);
  bke::subdiv::free(subdiv);

  if (use_creases) {
    /* Remove the layer in case it was created by the node from the field input. The fact
     * that this node uses attributes to input creases to the subdivision code is meant to be
     * an implementation detail ideally. */
    result->attributes_for_write().remove("crease_vert");
    result->attributes_for_write().remove("crease_edge");
  }

  if (mesh_copy) {
    BKE_id_free(nullptr, mesh_copy);
  }

  geometry::debug_randomize_mesh_order(result);

  return result;
}

#endif

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");
#ifdef WITH_OPENSUBDIV
  const Field<float> vert_crease = params.extract_input<Field<float>>("Vertex Crease");
  const Field<float> edge_crease = params.extract_input<Field<float>>("Edge Crease");

  const int uv_smooth = params.get_input<eSubsurfUVSmooth>("UV Smooth");
  const int boundary_smooth = params.get_input<eSubsurfBoundarySmooth>("Boundary Smooth");
  const int level = std::max(params.extract_input<int>("Level"), 0);
  const bool use_limit_surface = params.extract_input<bool>("Limit Surface");
  if (level == 0) {
    params.set_output("Mesh", std::move(geometry_set));
    return;
  }
  /* At this limit, a subdivided single triangle would be too large to be stored in #Mesh. */
  if (level >= 16) {
    params.error_message_add(NodeWarningType::Error, TIP_("The subdivision level is too large"));
    params.set_default_remaining_outputs();
    return;
  }

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    if (const Mesh *mesh = geometry_set.get_mesh()) {
      geometry_set.replace_mesh(mesh_subsurf_calc(
          mesh, level, vert_crease, edge_crease, boundary_smooth, uv_smooth, use_limit_surface));
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
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeSubdivisionSurface", GEO_NODE_SUBDIVISION_SURFACE);
  ntype.ui_name = "Subdivision Surface";
  ntype.ui_description =
      "Divide mesh faces to form a smooth surface, using the Catmull-Clark subdivision method";
  ntype.enum_name_legacy = "SUBDIVISION_SURFACE";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.initfunc = node_init;
  bke::node_type_size_preset(ntype, bke::eNodeSizePreset::Middle);
  blender::bke::node_type_storage(ntype,
                                  "NodeGeometrySubdivisionSurface",
                                  node_free_standard_storage,
                                  node_copy_standard_storage);
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_subdivision_surface_cc
