/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "DNA_curves_types.h"
#include "DNA_mesh_types.h"

#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"

#include "NOD_rna_define.hh"

#include "GEO_foreach_geometry.hh"
#include "GEO_mesh_bevel.hh"

#include "UI_interface.hh"
#include "UI_resources.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_bevel_cc {

static const EnumPropertyItem affect_items[] = {
    {int(geometry::BevelAffect::Vertices), "VERTICES", 0, "Vertices", "Bevel affects vertices"},
    {int(geometry::BevelAffect::Edges), "EDGES", 0, "Edges", "Bevel affects edges"},
    {0, nullptr, 0, nullptr, nullptr}};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.use_custom_socket_order();
  b.allow_any_socket_order();

  b.add_input<decl::Geometry>("Mesh"_ustr).supported_type(GeometryComponent::Type::Mesh);
  b.add_output<decl::Geometry>("Mesh"_ustr).propagate_all_geometry().align_with_previous();
  b.add_input<decl::Bool>("Selection"_ustr)
      .default_value(true)
      .hide_value()
      .evaluated_geometry_field()
      .description("Selects elements of 'Affect Kind' for beveling");
  b.add_input<decl::Menu>("Affect Kind"_ustr)
      .default_value(geometry::BevelAffect::Edges)
      .static_items(affect_items)
      .optional_label();
  b.add_input<decl::Float>("Start Left Offset"_ustr)
      .default_value(0.1f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .evaluated_geometry_field()
      .usage_by_single_menu(int(geometry::BevelAffect::Edges))
      .description("Offset for left side of source of edge, viewed from source");
  b.add_input<decl::Float>("Start Right Offset"_ustr)
      .default_value(0.1f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .evaluated_geometry_field()
      .usage_by_single_menu(int(geometry::BevelAffect::Edges))
      .description("Offset for right side of source of edge, viewed from source");
  b.add_input<decl::Float>("End Left Offset"_ustr)
      .default_value(0.1f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .evaluated_geometry_field()
      .usage_by_single_menu(int(geometry::BevelAffect::Edges))
      .description("Offset for left side of destination of edge, viewed from source");
  b.add_input<decl::Float>("End Right Offset"_ustr)
      .default_value(0.1f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .evaluated_geometry_field()
      .usage_by_single_menu(int(geometry::BevelAffect::Edges))
      .description("Offset for right side of destination of edge, viewed from source");
  b.add_input<decl::Float>("Offset"_ustr)
      .default_value(0.1f)
      .min(0.0f)
      .subtype(PROP_DISTANCE)
      .evaluated_geometry_field()
      .usage_by_single_menu(int(geometry::BevelAffect::Vertices))
      .description("Offset per vertex for vertex bevel (all edges)");
  b.add_input<decl::Bool>("Miter"_ustr)
      .default_value(false)
      .evaluated_geometry_field()
      .description("Use a miter for corner");
  b.add_input<decl::Float>("Spread"_ustr)
      .min(0.0f)
      .default_value(0.1f)
      .subtype(PROP_DISTANCE)
      .evaluated_geometry_field()
      .description("Per corner specification of 'spread' for arc miters")
      .usage_by_bool("Miter"_ustr, true);
  b.add_input<decl::Int>("Segments"_ustr)
      .min(1)
      .default_value(1)
      .description(
          "How many pieces is an edge beveled into, "
          "or, for vertex bevels, the how many segments on the arcs between the edges.");
  b.add_input<decl::Float>("Shape"_ustr)
      .default_value(0.5f)
      .min(0.0f)
      .max(1.0f)
      .subtype(PROP_FACTOR)
      .description(
          "Superellipse shape parameter, used when there is no Profile, "
          " and also used for Arc and Patch miters");
  b.add_input<decl::Geometry>("Profile"_ustr)
      .supported_type(GeometryComponent::Type::Curve)
      .description(
          "If present, the first curve will be sampled to give a custom profile on edges."
          " The curve should be in the XY plane, going from (0,1,0) to (1,0,0)");

  PanelDeclarationBuilder &selections_panel = b.add_panel("Selections"_ustr);
  selections_panel.default_closed(true);
  selections_panel.add_output<decl::Bool>("Vertex Face"_ustr)
      .anonymous_attribute_output()
      .no_muted_links()
      .description("Identifies output faces that are in the new mesh parts for vertices");
  selections_panel.add_output<decl::Bool>("Edge Face"_ustr)
      .anonymous_attribute_output()
      .no_muted_links()
      .description("Identifies output faces that are in the new mesh parts for edges");
  selections_panel.add_output<decl::Bool>("Outer Edge"_ustr)
      .anonymous_attribute_output()
      .no_muted_links()
      .description("Identifies output edges that are on the outsides of new mesh parts for edges");
  selections_panel.add_output<decl::Bool>("Mid Edge"_ustr)
      .anonymous_attribute_output()
      .no_muted_links()
      .description(
          "Identifies output edges that are in the middle of new mesh parts of edges "
          " and continued through vertices (round down if odd number of segments)");
}

/* -------------------------------------------------------------------- */
/** \name Profile Curve Sampling
 *
 * Samples the first spline of a #bke::CurvesGeometry into a flat array of (x, y) pairs
 * for use as `BevelParameters::custom_profile_samples`.
 *
 * The input curve is expected to start near (0, 1, z) and end near (1, 0, z) in its local
 * XY plane (Z is ignored).  The evaluated positions are used directly, so Bezier, poly,
 * Catmull-Rom, and NURBS curves all work without any special-casing.
 * \{ */

/**
 * Returns the evaluated (x, y) positions of the first spline of `curves`.
 * Returns an empty array when the geometry has no splines or fewer than 2 evaluated points.
 */
static Array<float2> sample_profile_curve(const bke::CurvesGeometry &curves)
{
  if (curves.curves_num() == 0) {
    return {};
  }

  /* Use the evaluator to get dense positions that correctly represent the curve shape
   * regardless of type (Bezier, Catmull-Rom, poly, NURBS). */
  const OffsetIndices<int> eval_by_curve = curves.evaluated_points_by_curve();
  const Span<float3> eval_positions = curves.evaluated_positions();
  const IndexRange eval_pts = eval_by_curve[0];

  if (eval_pts.size() < 2) {
    return {};
  }

  Array<float2> samples(eval_pts.size());
  for (const int i : eval_pts.index_range()) {
    const float3 &p = eval_positions[eval_pts[i]];
    samples[i] = float2(p.x, p.y);
  }
  return samples;
}

/** \} */

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh"_ustr);
  const Field<bool> selection_field = params.extract_input<Field<bool>>("Selection"_ustr);
  const AttributeFilter &attribute_filter = params.get_attribute_filter("Mesh"_ustr);
  const int segments = params.extract_input<int>("Segments"_ustr);
  if (segments < 1) {
    params.error_message_add(NodeWarningType::Error, "Segments must be at least 1");
    params.set_output("Mesh"_ustr, std::move(geometry_set));
    params.set_default_remaining_outputs();
    return;
  }
  const auto affect = params.extract_input<geometry::BevelAffect>("Affect Kind"_ustr);

  const Field<float> offset0_field = affect == geometry::BevelAffect::Edges ?
                                         params.extract_input<Field<float>>(
                                             "Start Left Offset"_ustr) :
                                         params.extract_input<Field<float>>("Offset"_ustr);
  const Field<float> offset1_field = params.extract_input<Field<float>>("Start Right Offset"_ustr);
  const Field<float> offset2_field = params.extract_input<Field<float>>("End Left Offset"_ustr);
  const Field<float> offset3_field = params.extract_input<Field<float>>("End Right Offset"_ustr);

  const Field<bool> miter_field = params.extract_input<Field<bool>>("Miter"_ustr);
  const Field<float> spread_field = params.extract_input<Field<float>>("Spread"_ustr);
  const float shape = params.extract_input<float>("Shape"_ustr);

  /* Sample the Profile input curve into a flat float2 array.
   * The array is built once here and shared across all instances in the geometry loop;
   * BevelParameters holds it by const reference (non-owning Span). */
  const GeometrySet profile_geometry = params.extract_input<GeometrySet>("Profile"_ustr);
  Array<float2> profile_samples;
  if (const Curves *profile_curves_id = profile_geometry.get_curves()) {
    const bke::CurvesGeometry &profile_geom = profile_curves_id->geometry.wrap();
    profile_samples = sample_profile_curve(profile_geom);
  }

  geometry::foreach_real_geometry(geometry_set, [&](GeometrySet &geometry_set) {
    const Mesh *src_mesh = geometry_set.get_mesh();
    if (!src_mesh) {
      return;
    }
    geometry::BevelParameters bevel_params;
    bevel_params.affect_type = affect;
    bevel_params.segments = segments;
    bevel_params.shape = shape;
    /* Move the samples into bevel_params (zero-copy; profile_samples stays valid
     * through the geometry loop because it is declared in this outer scope). */
    bevel_params.custom_profile_samples = profile_samples;
    const int ne = src_mesh->edges_num;

    /* Shared logic executed after the selection/offset evaluator (and its lifetime) is set up.
     * The `selection` reference must not outlive `evaluator`. */
    auto run_bevel = [&](const IndexMask &selection) {
      if (selection.is_empty()) {
        return;
      }

      const bke::MeshFieldContext corner_context(*src_mesh, AttrDomain::Corner);
      FieldEvaluator corner_evaluator{corner_context, src_mesh->corners_num};
      /* TODO: make this more efficient in usual case of no miters. */
      bevel_params.miter = Array<bool>(src_mesh->corners_num);
      bevel_params.spread = Array<float>(src_mesh->corners_num);
      corner_evaluator.add_with_destination(miter_field, bevel_params.miter.as_mutable_span());
      corner_evaluator.add_with_destination(spread_field, bevel_params.spread.as_mutable_span());
      corner_evaluator.evaluate();

      bevel_params.attribute_outputs.vertex_face_id =
          params.get_output_anonymous_attribute_id_if_needed("Vertex Face"_ustr);
      bevel_params.attribute_outputs.edge_face_id =
          params.get_output_anonymous_attribute_id_if_needed("Edge Face"_ustr);
      bevel_params.attribute_outputs.outer_edge_id =
          params.get_output_anonymous_attribute_id_if_needed("Outer Edge"_ustr);
      bevel_params.attribute_outputs.mid_edge_id =
          params.get_output_anonymous_attribute_id_if_needed("Mid Edge"_ustr);

      std::optional<Mesh *> mesh = geometry::mesh_bevel(
          *src_mesh, selection, bevel_params, attribute_filter);
      if (!mesh) {
        return;
      }
      geometry_set.replace_mesh(*mesh);
    };

    if (affect == geometry::BevelAffect::Vertices) {
      /* Vertex bevel: selection and offset0 are per-vertex.
       * offsets[0][v] is the slide distance at vertex v; offsets[1..3] are unused. */
      const int nv = src_mesh->verts_num;
      bevel_params.offsets = {Array<float>(nv), Array<float>(0), Array<float>(0), Array<float>(0)};

      const bke::MeshFieldContext vert_context(*src_mesh, AttrDomain::Point);
      FieldEvaluator vert_evaluator{vert_context, nv};
      vert_evaluator.add(selection_field);
      vert_evaluator.add_with_destination(offset0_field,
                                          bevel_params.offsets[0].as_mutable_span());
      vert_evaluator.evaluate();

      /* Pass to run_bevel while vert_evaluator is still alive. */
      run_bevel(vert_evaluator.get_evaluated_as_mask(0));
    }
    else {
      /* Edge bevel: selection and all four offsets are per-edge. */
      bevel_params.offsets = {
          Array<float>(ne), Array<float>(ne), Array<float>(ne), Array<float>(ne)};

      const bke::MeshFieldContext edge_context(*src_mesh, AttrDomain::Edge);
      FieldEvaluator edge_evaluator{edge_context, ne};
      edge_evaluator.add(selection_field);
      edge_evaluator.add_with_destination(offset0_field,
                                          bevel_params.offsets[0].as_mutable_span());
      edge_evaluator.add_with_destination(offset1_field,
                                          bevel_params.offsets[1].as_mutable_span());
      edge_evaluator.add_with_destination(offset2_field,
                                          bevel_params.offsets[2].as_mutable_span());
      edge_evaluator.add_with_destination(offset3_field,
                                          bevel_params.offsets[3].as_mutable_span());
      edge_evaluator.evaluate();

      /* Pass to run_bevel while edge_evaluator is still alive. */
      run_bevel(edge_evaluator.get_evaluated_as_mask(0));
    }
  });

  params.set_output("Mesh"_ustr, std::move(geometry_set));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;

  geo_node_type_base(&ntype, "GeometryNodeMeshBevel"_ustr, std::nullopt);
  ntype.ui_name = "Mesh Bevel";
  ntype.ui_description = "Bevel selected edges or vertices";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.default_width = bke::NodeWidth::_180;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_bevel_cc
