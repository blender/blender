/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_delaunay_2d.hh"
#include "BLI_index_mask.hh"

#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_grease_pencil.hh"
#include "BKE_instances.hh"
#include "BKE_mesh.hh"
#include "BKE_pointcloud.hh"

#include "FN_field.hh"

#include "GEO_foreach_geometry.hh"
#include "GEO_join_geometries.hh"
#include "GEO_mesh_selection.hh"

#include "NOD_socket_usage_inference.hh"

#include "node_geometry_util.hh"

namespace blender::nodes::node_geo_delaunay_triangulation_cc {

enum class TriangulationMode : int8_t {
  Full = 0,
  Inside = 1,
  InsideWithHoles = 2,
};

enum class TriangulationType : int8_t {
  Triangles = 0,
  NGons = 1,
};

enum class FillRule : int8_t {
  EvenOdd = 0,
  NonZero = 1,
};

static const EnumPropertyItem triangulation_mode_items[] = {
    {int(TriangulationMode::Full),
     "FULL",
     0,
     N_("Full"),
     N_("All triangles. The outer boundary is the convex hull of input points")},
    {int(TriangulationMode::Inside),
     "INSIDE",
     0,
     N_("Inside"),
     N_("All triangles fully enclosed by constraint edges or faces")},
    {int(TriangulationMode::InsideWithHoles),
     "INSIDE_WITH_HOLES",
     0,
     N_("Inside With Holes"),
     N_("Triangles fully enclosed by constraint edges or faces excluding triangles inside "
        "detected holes")},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem triangulation_type_items[] = {
    {int(TriangulationType::Triangles), "TRIANGLES", 0, N_("Triangles"), ""},
    {int(TriangulationType::NGons), "NGONS", 0, N_("N-gons"), ""},
    {0, nullptr, 0, nullptr, nullptr},
};

static const EnumPropertyItem fill_rule_items[] = {
    {int(FillRule::EvenOdd),
     "EVEN_ODD",
     0,
     N_("Even-Odd"),
     N_("Alternate inside/outside based on crossing count")},
    {int(FillRule::NonZero),
     "NON_ZERO",
     0,
     N_("Non-Zero"),
     N_("Overlapping curves with the same winding direction are filled as a union")},
    {0, nullptr, 0, nullptr, nullptr},
};

static void node_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry"_ustr)
      .supported_type({GeometryComponent::Type::Mesh,
                       GeometryComponent::Type::Curve,
                       GeometryComponent::Type::GreasePencil,
                       GeometryComponent::Type::PointCloud})
      .description(
          "The geometries that are used to constrain the triangulation using the points, edges, "
          "and faces");
  b.add_input<decl::Int>("Group ID"_ustr)
      .evaluated_geometry_field()
      .hide_value()
      .description(
          "An ID used to group points together. Triangulation is done separately for each group");
  b.add_input<decl::Menu>("Mode"_ustr)
      .static_items(triangulation_mode_items)
      .default_value(MenuValue(TriangulationMode::Full))
      .optional_label();
  b.add_input<decl::Menu>("Type"_ustr)
      .static_items(triangulation_type_items)
      .default_value(MenuValue(TriangulationType::Triangles))
      .optional_label()
      .usage_inference(
          [](const socket_usage_inference::SocketUsageParams &params) -> std::optional<bool> {
            return params.menu_input_may_be("Mode"_ustr, int(TriangulationMode::Inside)) ||
                   params.menu_input_may_be("Mode"_ustr, int(TriangulationMode::InsideWithHoles));
          });
  b.add_input<decl::Menu>("Fill Rule"_ustr)
      .static_items(fill_rule_items)
      .default_value(MenuValue(FillRule::EvenOdd))
      .optional_label()
      .usage_inference(
          [](const socket_usage_inference::SocketUsageParams &params) -> std::optional<bool> {
            return params.menu_input_may_be("Mode"_ustr, int(TriangulationMode::InsideWithHoles));
          });
  b.add_output<decl::Geometry>("Mesh"_ustr).propagate_all_geometry();
  b.add_output<decl::Bool>("Intersection Points"_ustr)
      .anonymous_attribute_output()
      .description("A selection of newly created intersection points")
      .no_muted_links();
}

static CDT_output_type get_cdt_output_type(const TriangulationMode mode,
                                           const TriangulationType output_type,
                                           const FillRule fill_rule)
{
  switch (mode) {
    case TriangulationMode::Full:
      return CDT_FULL;
    case TriangulationMode::Inside:
      if (output_type == TriangulationType::Triangles) {
        return CDT_INSIDE;
      }
      return CDT_CONSTRAINTS_VALID_BMESH;
    case TriangulationMode::InsideWithHoles:
      switch (fill_rule) {
        case FillRule::EvenOdd: {
          if (output_type == TriangulationType::Triangles) {
            return CDT_INSIDE_WITH_HOLES;
          }
          return CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES;
        }
        case FillRule::NonZero: {
          if (output_type == TriangulationType::Triangles) {
            return CDT_INSIDE_WITH_HOLES_NONZERO;
          }
          return CDT_CONSTRAINTS_VALID_BMESH_WITH_HOLES_NONZERO;
        }
      }
  }
  return CDT_FULL;
}

/* The kind of geometry a CDT point source comes from. */
enum class SourceComponent : int8_t {
  Mesh = 0,
  Curve = 1,
  PointCloud = 2,
};

/**
 * The points of all source components are concatenated into a single index space, so that
 * attribute propagation doesn't have to distinguish between them. The returned offsets give the
 * range of each #SourceComponent.
 */
static Array<int, 4> src_component_point_offsets(const Mesh *mesh,
                                                 const bke::CurvesGeometry *curves,
                                                 const PointCloud *pointcloud)
{
  Array<int, 4> offsets(4);
  offsets[int(SourceComponent::Mesh)] = mesh ? mesh->verts_num : 0;
  offsets[int(SourceComponent::Curve)] = curves ? curves->evaluated_points_num() : 0;
  offsets[int(SourceComponent::PointCloud)] = pointcloud ? pointcloud->totpoint : 0;
  offset_indices::accumulate_counts_to_offsets(offsets);
  return offsets;
}

/* The points, edges, and faces of the original geometry that make up a single group. The masks
 * index into the original geometry's domains (mesh vertices, curve evaluated points, point cloud
 * points). */
struct GroupMasks {
  IndexMask mesh_verts;
  IndexMask mesh_loose_edges;
  IndexMask mesh_faces;
  IndexMask curves;
  IndexMask curve_points;
  IndexMask points;
};

struct TriangulationResult {
  meshintersect::CDT_result<double> cdt_result;

  /* For every output vertex that isn't an intersection point, the index of its source point in
   * the concatenated points of all components (see #src_component_point_offsets). */
  Array<int> src_point_by_dst_point;

  /* Output vertices created at the intersection of two constraint edges. They are added at the
   * end. Their values are mixed from the four end points of the two edges. */
  IndexRange intersection_points;
  Array<int> intersection_src_offsets;
  Array<int> intersection_src_points;
  Array<float> intersection_src_weights;

  OffsetIndices<int> intersection_srcs() const
  {
    return intersection_src_offsets.as_span();
  }
};

static const IndexMask &group_point_mask(const GroupMasks &group, const SourceComponent domain)
{
  switch (domain) {
    case SourceComponent::Mesh:
      return group.mesh_verts;
    case SourceComponent::Curve:
      return group.curve_points;
    case SourceComponent::PointCloud:
      return group.points;
  }
  BLI_assert_unreachable();
  return group.points;
}

static void gather_2d_positions(const Span<float3> src_positions,
                                const IndexMask &mask,
                                MutableSpan<double2> dst_positions)
{
  mask.foreach_index_optimized<int>(
      [&](const int index, const int pos) {
        dst_positions[pos] = double2(src_positions[index].x, src_positions[index].y);
      },
      exec_mode::grain_size(8192));
}

static void calc_group_triangulation(const Mesh *mesh,
                                     const bke::CurvesGeometry *curves,
                                     const PointCloud *pointcloud,
                                     const OffsetIndices<int> src_points_by_component,
                                     const CDT_output_type output_type,
                                     const GroupMasks &group,
                                     IndexMaskMemory &memory,
                                     TriangulationResult &result)
{
  /* Gather the point sources (and the offsets describing their place in the CDT vertex
   * array) in a fixed order: mesh vertices, curve evaluated points, then point cloud
   * points. */
  Vector<SourceComponent, 3> point_source_domains;
  Vector<int, 4> point_offsets;
  int mesh_point_source = -1;
  int curve_point_source = -1;
  auto add_point_source = [&](const SourceComponent domain, const int64_t count, int &r_index) {
    r_index = int(point_source_domains.append_and_get_index(domain));
    point_offsets.append(int(count));
  };
  int unused_index = -1;
  if (mesh && !group.mesh_verts.is_empty()) {
    add_point_source(SourceComponent::Mesh, group.mesh_verts.size(), mesh_point_source);
  }
  if (curves && !group.curve_points.is_empty()) {
    add_point_source(SourceComponent::Curve, group.curve_points.size(), curve_point_source);
  }
  if (pointcloud && !group.points.is_empty()) {
    add_point_source(SourceComponent::PointCloud, group.points.size(), unused_index);
  }
  if (point_source_domains.is_empty()) {
    return;
  }

  /* For curves, cyclic curves (with more than 2 points) are added as face constraints, and
   * non-cyclic curves are added as edge constraints. The edges are the evaluated line segments of
   * each curve. */
  IndexMask face_curves;
  IndexMask edge_curves;
  Vector<int> curve_segment_offsets;
  if (curves && !group.curves.is_empty()) {
    const VArray<bool> cyclic = curves->cyclic();
    face_curves = IndexMask::from_bools(group.curves, cyclic, memory);
    const OffsetIndices<int> points_by_curve = curves->evaluated_points_by_curve();
    face_curves = IndexMask::from_predicate(
        face_curves, memory, [&](const int curve) { return points_by_curve[curve].size() > 2; });
    edge_curves = face_curves.complement(group.curves, memory);
    if (!edge_curves.is_empty()) {
      curve_segment_offsets.resize(edge_curves.size());
      edge_curves.foreach_index_optimized<int>(
          [&](const int curve, const int pos) {
            /* No need for #bke::curves::segments_num because these curves are known to
             * be non-cyclic. Cyclic curves are processed as faces. */
            curve_segment_offsets[pos] = points_by_curve[curve].size() - 1;
          },
          exec_mode::grain_size(4096));
    }
  }
  curve_segment_offsets.append(0);
  const OffsetIndices<int> edges_by_curve = offset_indices::accumulate_counts_to_offsets(
      curve_segment_offsets.as_mutable_span());

  /* Edge sources, in the same fixed order. */
  Vector<SourceComponent, 2> edge_source_domains;
  Vector<int, 3> edge_offsets;
  if (mesh && !group.mesh_loose_edges.is_empty()) {
    edge_source_domains.append(SourceComponent::Mesh);
    edge_offsets.append(group.mesh_loose_edges.size());
  }
  if (!edges_by_curve.is_empty()) {
    edge_source_domains.append(SourceComponent::Curve);
    edge_offsets.append(int(edges_by_curve.total_size()));
  }

  /* Face sources, in the same fixed order. */
  Vector<SourceComponent, 2> face_source_domains;
  Vector<int, 3> faces_by_source_data;
  if (mesh && !group.mesh_faces.is_empty()) {
    face_source_domains.append(SourceComponent::Mesh);
    faces_by_source_data.append(group.mesh_faces.size());
  }
  if (!face_curves.is_empty()) {
    face_source_domains.append(SourceComponent::Curve);
    faces_by_source_data.append(face_curves.size());
  }

  point_offsets.append(0);
  edge_offsets.append(0);
  faces_by_source_data.append(0);
  const OffsetIndices<int> points_by_source = offset_indices::accumulate_counts_to_offsets(
      point_offsets);
  const OffsetIndices<int> edges_by_source = offset_indices::accumulate_counts_to_offsets(
      edge_offsets);
  const OffsetIndices<int> faces_by_source = offset_indices::accumulate_counts_to_offsets(
      faces_by_source_data);

  const int total_source_points = points_by_source.total_size();

  /* Add 2D points. */
  Array<double2> cdt_verts(total_source_points);
  for (const int source_i : point_source_domains.index_range()) {
    const IndexRange dst_range = points_by_source[source_i];
    MutableSpan<double2> dst_positions_2d = cdt_verts.as_mutable_span().slice(dst_range);
    switch (point_source_domains[source_i]) {
      case SourceComponent::Mesh:
        gather_2d_positions(mesh->vert_positions(), group.mesh_verts, dst_positions_2d);
        break;
      case SourceComponent::Curve:
        gather_2d_positions(curves->evaluated_positions(), group.curve_points, dst_positions_2d);
        break;
      case SourceComponent::PointCloud:
        gather_2d_positions(pointcloud->positions(), group.points, dst_positions_2d);
        break;
    }
  }

  /* Map from original point indices to their destination indices in the CDT vertex array,
   * for remapping edge and face constraints. */
  Array<int> mesh_vert_to_dst;
  if (mesh && (!group.mesh_loose_edges.is_empty() || !group.mesh_faces.is_empty())) {
    mesh_vert_to_dst.reinitialize(mesh->verts_num);
    index_mask::build_reverse_map<int>(group.mesh_verts, mesh_vert_to_dst);
  }
  Array<int> curve_point_to_dst;
  if (curves && (!edges_by_curve.is_empty() || !face_curves.is_empty())) {
    curve_point_to_dst.reinitialize(curves->evaluated_points_num());
    index_mask::build_reverse_map<int>(group.curve_points, curve_point_to_dst);
  }

  /* Add edge constraints. */
  Array<int2> cdt_edges(edges_by_source.total_size());
  for (const int source_i : edge_source_domains.index_range()) {
    MutableSpan<int2> dst_edges = cdt_edges.as_mutable_span().slice(edges_by_source[source_i]);
    switch (edge_source_domains[source_i]) {
      case SourceComponent::Mesh: {
        const Span<int2> edges = mesh->edges();
        const int dst_point_offset = points_by_source[mesh_point_source].start();
        group.mesh_loose_edges.foreach_index_optimized<int>(
            [&](const int index, const int pos) {
              dst_edges[pos] = {mesh_vert_to_dst[edges[index][0]] + dst_point_offset,
                                mesh_vert_to_dst[edges[index][1]] + dst_point_offset};
            },
            exec_mode::grain_size(4096));
        break;
      }
      case SourceComponent::Curve: {
        const OffsetIndices<int> points_by_curve = curves->evaluated_points_by_curve();
        const int dst_point_offset = points_by_source[curve_point_source].start();
        edge_curves.foreach_index(
            [&](const int curve, const int pos) {
              const IndexRange points = points_by_curve[curve];
              MutableSpan<int2> dst_edges_by_curve = dst_edges.slice(edges_by_curve[pos]);
              for (const int point : points.index_range().drop_back(1)) {
                dst_edges_by_curve[point] = {curve_point_to_dst[points[point]] + dst_point_offset,
                                             curve_point_to_dst[points[point] + 1] +
                                                 dst_point_offset};
              }
            },
            exec_mode::grain_size(4096));
        break;
      }
      case SourceComponent::PointCloud:
        break;
    }
  }

  /* Count total face sizes. */
  Array<int> cdt_face_offsets(faces_by_source.total_size() + 1);
  for (const int source_i : face_source_domains.index_range()) {
    MutableSpan<int> dst_face_sizes = cdt_face_offsets.as_mutable_span().slice(
        faces_by_source[source_i]);
    switch (face_source_domains[source_i]) {
      case SourceComponent::Mesh:
        offset_indices::gather_group_sizes(mesh->faces(), group.mesh_faces, dst_face_sizes);
        break;
      case SourceComponent::Curve:
        offset_indices::gather_group_sizes(
            curves->evaluated_points_by_curve(), face_curves, dst_face_sizes);
        break;
      case SourceComponent::PointCloud:
        break;
    }
  }
  const OffsetIndices<int> cdt_faces = offset_indices::accumulate_counts_to_offsets(
      cdt_face_offsets);

  /* Add face constraints. */
  Array<int> cdt_face_vert_indices(cdt_faces.total_size());
  for (const int source_i : face_source_domains.index_range()) {
    const OffsetIndices<int> dst_faces = cdt_faces.slice(faces_by_source[source_i]);
    switch (face_source_domains[source_i]) {
      case SourceComponent::Mesh: {
        const int dst_point_offset = points_by_source[mesh_point_source].start();
        const OffsetIndices faces = mesh->faces();
        const Span<int> corner_verts = mesh->corner_verts();
        group.mesh_faces.foreach_index(
            [&](const int index, const int pos) {
              const IndexRange face = faces[index];
              MutableSpan<int> dst_face = cdt_face_vert_indices.as_mutable_span().slice(
                  dst_faces[pos]);
              for (const int i : face.index_range()) {
                dst_face[i] = mesh_vert_to_dst[corner_verts[face[i]]] + dst_point_offset;
              }
            },
            exec_mode::grain_size(2048));
        break;
      }
      case SourceComponent::Curve: {
        const int dst_point_offset = points_by_source[curve_point_source].start();
        const OffsetIndices<int> points_by_curve = curves->evaluated_points_by_curve();
        threading::parallel_for(face_curves.index_range(), 1024, [&](const IndexRange range) {
          for (const int i : range) {
            const int curve = face_curves[i];
            const IndexRange points = points_by_curve[curve];
            MutableSpan<int> dst_face = cdt_face_vert_indices.as_mutable_span().slice(
                dst_faces[i]);
            for (const int j : points.index_range()) {
              dst_face[j] = curve_point_to_dst[points[j]] + dst_point_offset;
            }
          }
        });
        break;
      }
      case SourceComponent::PointCloud:
        break;
    }
  }

  /* Construct CDT input and compute delaunay triangulation. */
  const meshintersect::CDT_input<double> cdt_input{.vert = cdt_verts,
                                                   .edge = cdt_edges,
                                                   .face_offsets = cdt_faces,
                                                   .face_vert_indices = cdt_face_vert_indices,
                                                   .needed_ids = CDT_ORIG_VERTS |
                                                                 CDT_INTERSECTED_EDGES};

  result.cdt_result = meshintersect::delaunay_2d_calc(cdt_input, output_type);

  /* Map every CDT input vertex to its index in the concatenated source points. */
  Array<int> src_point_by_cdt_vert(total_source_points);
  for (const int source_i : point_source_domains.index_range()) {
    const SourceComponent domain = point_source_domains[source_i];
    const int src_offset = src_points_by_component[int(domain)].start();
    MutableSpan<int> dst = src_point_by_cdt_vert.as_mutable_span().slice(
        points_by_source[source_i]);
    group_point_mask(group, domain)
        .foreach_index_optimized<int>(
            [&](const int index, const int pos) { dst[pos] = index + src_offset; },
            exec_mode::grain_size(8192));
  }

  /* Vertices created at intersections have no original vertex and are added at the end. */
  const Span<Vector<uint>> verts_orig = result.cdt_result.vert_orig.as_span();
  const int orig_verts_num = std::find_if(
                                 verts_orig.begin(),
                                 verts_orig.end(),
                                 [](const Vector<uint> &verts) { return verts.is_empty(); }) -
                             verts_orig.begin();

  result.src_point_by_dst_point.reinitialize(orig_verts_num);
  threading::parallel_for(IndexRange(orig_verts_num), 4096, [&](const IndexRange range) {
    for (const int dst_point : range) {
      /* Only use the first point and discard the rest of potentially merged vertices. */
      result.src_point_by_dst_point[dst_point] =
          src_point_by_cdt_vert[verts_orig[dst_point].first()];
    }
  });

  result.intersection_points = IndexRange::from_begin_end(orig_verts_num, verts_orig.size());
  if (result.intersection_points.is_empty()) {
    return;
  }

  const uint32_t face_edge_offset = result.cdt_result.face_edge_offset;
  auto verts_from_cdt_edge = [&](const int edge) -> std::pair<int, int> {
    if (edge < face_edge_offset) {
      return {cdt_edges[edge].x, cdt_edges[edge].y};
    }
    /* See CDT_result::edge_orig for how edge indices are encoded. */
    const int src_face = (edge / face_edge_offset) - 1;
    const int src_face_offset = edge % face_edge_offset;
    const IndexRange face_range = cdt_faces[src_face];
    const Span<int> face = cdt_face_vert_indices.as_span().slice(face_range);
    return {face[src_face_offset], face[(src_face_offset + 1) % face_range.size()]};
  };

  /* Every intersection point is mixed from the two end points of both intersecting edges. */
  const int intersections_num = result.intersection_points.size();
  result.intersection_src_offsets.reinitialize(intersections_num + 1);
  offset_indices::fill_constant_group_size(4, 0, result.intersection_src_offsets);
  result.intersection_src_points.reinitialize(intersections_num * 4);
  result.intersection_src_weights.reinitialize(intersections_num * 4);

  const Span<int2> intersected_edges_orig = result.cdt_result.intersected_edges_orig.as_span();
  threading::parallel_for(IndexRange(intersections_num), 1024, [&](const IndexRange range) {
    for (const int intersection : range) {
      const int dst_point = result.intersection_points[intersection];
      const int edge1 = intersected_edges_orig[dst_point][0];
      const int edge2 = intersected_edges_orig[dst_point][1];
      BLI_assert(edge1 != -1 && edge2 != -1 && edge1 != edge2);

      const auto [vert1, vert2] = verts_from_cdt_edge(edge1);
      const auto [vert3, vert4] = verts_from_cdt_edge(edge2);
      const float2 pos1 = float2(cdt_verts[vert1]);
      const float2 pos2 = float2(cdt_verts[vert2]);
      const float2 pos3 = float2(cdt_verts[vert3]);
      const float2 pos4 = float2(cdt_verts[vert4]);

      /* double area = cross(B - A, C - A) */
      const float d1 = math::cross(pos4 - pos3, pos1 - pos3);
      const float d2 = math::cross(pos4 - pos3, pos2 - pos3);
      const float d3 = math::cross(pos2 - pos1, pos3 - pos1);
      const float d4 = math::cross(pos2 - pos1, pos4 - pos1);

      /* Factors along each edge for intersection. */
      const float t = math::safe_divide(d1, (d1 - d2));
      const float u = math::safe_divide(d3, (d3 - d4));
      BLI_assert(t >= 0.0f && t <= 1.0f && u >= 0.0f && u <= 1.0f);

      const IndexRange srcs = result.intersection_srcs()[intersection];
      MutableSpan<int> src_points = result.intersection_src_points.as_mutable_span().slice(srcs);
      src_points[0] = src_point_by_cdt_vert[vert1];
      src_points[1] = src_point_by_cdt_vert[vert2];
      src_points[2] = src_point_by_cdt_vert[vert3];
      src_points[3] = src_point_by_cdt_vert[vert4];

      MutableSpan<float> weights = result.intersection_src_weights.as_mutable_span().slice(srcs);
      weights[0] = (1.0f - t) / 2.0f;
      weights[1] = t / 2.0f;
      weights[2] = (1.0f - u) / 2.0f;
      weights[3] = u / 2.0f;
    }
  });
}

static Array<TriangulationResult> calc_triangulations(
    const Mesh *mesh,
    const bke::CurvesGeometry *curves,
    const fn::FieldContext *curves_field_context,
    const PointCloud *pointcloud,
    const OffsetIndices<int> src_points_by_component,
    const Field<int> &group_index,
    const CDT_output_type output_type)
{
  IndexMaskMemory memory;
  Vector<IndexMask> mesh_masks;
  Vector<IndexMask> curve_masks;
  Vector<IndexMask> point_masks;
  VectorSet<int> mesh_ids;
  VectorSet<int> curve_ids;
  VectorSet<int> point_ids;

  std::optional<FieldEvaluator> mesh_evaluator;
  std::optional<FieldEvaluator> curve_evaluator;
  std::optional<FieldEvaluator> point_evaluator;

  if (mesh && mesh->verts_num > 0) {
    const bke::GeometryFieldContext context{*mesh, bke::AttrDomain::Point};
    mesh_evaluator.emplace(context, mesh->verts_num);
    mesh_evaluator->add(group_index);
    mesh_evaluator->evaluate();
    const VArray<int> groups = mesh_evaluator->get_evaluated<int>(0);
    mesh_masks = IndexMask::from_group_ids(groups, memory);
    for (const IndexMask &mask : mesh_masks) {
      mask.foreach_index([&](const int64_t i) { mesh_ids.add(groups[i]); });
    }
  }
  if (curves && !curves->is_empty()) {
    curve_evaluator.emplace(*curves_field_context, curves->curves_num());
    curve_evaluator->add(group_index);
    curve_evaluator->evaluate();
    const VArray<int> groups = curve_evaluator->get_evaluated<int>(0);
    curve_masks = IndexMask::from_group_ids(groups, memory);
    for (const IndexMask &mask : curve_masks) {
      mask.foreach_index([&](const int64_t i) { curve_ids.add(groups[i]); });
    }
  }
  if (pointcloud && pointcloud->totpoint > 0) {
    const bke::GeometryFieldContext context{*pointcloud};
    point_evaluator.emplace(context, pointcloud->totpoint);
    point_evaluator->add(group_index);
    point_evaluator->evaluate();
    const VArray<int> groups = point_evaluator->get_evaluated<int>(0);
    point_masks = IndexMask::from_group_ids(groups, memory);
    for (const IndexMask &mask : point_masks) {
      mask.foreach_index([&](const int64_t i) { point_ids.add(groups[i]); });
    }
  }

  VectorSet<int> all_group_ids = mesh_ids;
  all_group_ids.add_multiple(curve_ids);
  all_group_ids.add_multiple(point_ids);
  if (all_group_ids.is_empty()) {
    return {};
  }

  Span<int2> mesh_edges;
  OffsetIndices<int> mesh_faces;
  Span<int> mesh_corner_verts;
  if (mesh) {
    mesh_edges = mesh->edges();
    mesh_faces = mesh->faces();
    mesh_corner_verts = mesh->corner_verts();
  }

  Array<TriangulationResult> results(all_group_ids.size());
  threading::parallel_for(
      results.index_range(),
      1024,
      [&](const IndexRange range) {
        for (const int i : range) {
          const int group_id = all_group_ids[i];
          GroupMasks group;
          IndexMaskMemory memory;
          if (mesh) {
            const int index = mesh_ids.index_of_try(group_id);
            if (index != -1) {
              group.mesh_verts = mesh_masks[index];
              /* A loose edge or face belongs to the group only if all of its vertices do. */
              Array<bool> vert_in_group(mesh->verts_num, false);
              group.mesh_verts.to_bools(vert_in_group);
              group.mesh_loose_edges = IndexMask::from_predicate(
                  mesh->loose_edges(), memory, [&](const int64_t edge) {
                    return vert_in_group[mesh_edges[edge][0]] &&
                           vert_in_group[mesh_edges[edge][1]];
                  });
              /* Note: This should be optimized further. */
              group.mesh_faces = geometry::face_selection_from_vert(
                  mesh_faces, mesh_corner_verts, vert_in_group, memory);
            }
          }
          if (curves) {
            const int index = curve_ids.index_of_try(group_id);
            if (index != -1) {
              group.curves = curve_masks[index];
              group.curve_points = IndexMask::from_ranges(
                  curves->evaluated_points_by_curve(), group.curves, memory);
            }
          }
          if (pointcloud) {
            const int index = point_ids.index_of_try(group_id);
            if (index != -1) {
              group.points = point_masks[index];
            }
          }
          calc_group_triangulation(mesh,
                                   curves,
                                   pointcloud,
                                   src_points_by_component,
                                   output_type,
                                   group,
                                   memory,
                                   results[i]);
        }
      },
      threading::individual_task_sizes([&](const int i) {
        const int group_id = all_group_ids[i];
        int size = 0;
        if (mesh) {
          const int index = mesh_ids.index_of_try(group_id);
          if (index != -1) {
            size += mesh_masks[index].size();
          }
        }
        if (curves) {
          const int index = curve_ids.index_of_try(group_id);
          if (index != -1) {
            size += curve_masks[index].size();
          }
        }
        if (pointcloud) {
          const int index = point_ids.index_of_try(group_id);
          if (index != -1) {
            size += point_masks[index].size();
          }
        }
        return size;
      }));

  return results;
}

static AttributeAccessor src_component_attributes(const Mesh *mesh,
                                                  const bke::CurvesGeometry *curves,
                                                  const PointCloud *pointcloud,
                                                  const SourceComponent component)
{
  switch (component) {
    case SourceComponent::Mesh:
      return mesh->attributes();
    case SourceComponent::Curve:
      return curves->attributes();
    case SourceComponent::PointCloud:
      return pointcloud->attributes();
  }
  BLI_assert_unreachable();
  return mesh->attributes();
}

/**
 * Retrieve a point attribute from a single source component, converted to the given type. Curve
 * attributes are interpolated to the evaluated points.
 */
static GVArray src_component_attribute(const Mesh *mesh,
                                       const bke::CurvesGeometry *curves,
                                       const PointCloud *pointcloud,
                                       const SourceComponent component,
                                       const StringRef name,
                                       const bke::AttrType data_type)
{
  const GVArray src = *src_component_attributes(mesh, curves, pointcloud, component)
                           .lookup_or_default(name, AttrDomain::Point, data_type);
  if (component != SourceComponent::Curve) {
    return src;
  }
  if (src.is_single()) {
    BUFFER_FOR_CPP_TYPE_VALUE(src.type(), value);
    src.get_internal_single(value);
    return GVArray::from_single(src.type(), curves->evaluated_points_num(), value);
  }
  curves->ensure_can_interpolate_to_evaluated();
  GArray<> evaluated(src.type(), curves->evaluated_points_num());
  curves->interpolate_to_evaluated(GVArraySpan(src), evaluated.as_mutable_span());
  return GVArray::from_garray(std::move(evaluated));
}

/**
 * Retrieve a point attribute for the concatenated points of all source components (see
 * #src_point_offsets). Components without the attribute contribute default values.
 */
static GVArray src_attribute(const Mesh *mesh,
                             const bke::CurvesGeometry *curves,
                             const PointCloud *pointcloud,
                             const OffsetIndices<int> src_points_by_component,
                             const StringRef name,
                             const bke::AttrType data_type)
{
  Vector<SourceComponent, 3> components;
  for (const int i : src_points_by_component.index_range()) {
    if (!src_points_by_component[i].is_empty()) {
      components.append(SourceComponent(i));
    }
  }
  if (components.size() == 1) {
    return src_component_attribute(mesh, curves, pointcloud, components[0], name, data_type);
  }
  GArray<> src(bke::attribute_type_to_cpp_type(data_type),
               src_points_by_component.total_size(),
               NoInitialization());
  for (const SourceComponent component : components) {
    src_component_attribute(mesh, curves, pointcloud, component, name, data_type)
        .materialize_to_uninitialized(
            src.as_mutable_span().slice(src_points_by_component[int(component)]).data());
  }
  return GVArray::from_garray(std::move(src));
}

static Mesh *cdts_to_mesh(const Span<TriangulationResult> results,
                          const Mesh *mesh,
                          const bke::CurvesGeometry *curves,
                          const PointCloud *pointcloud,
                          const OffsetIndices<int> src_points_by_component,
                          const std::optional<std::string> dst_intersection_points_attribute_id,
                          const AttributeFilter &attribute_filter)
{
  Array<int> vert_groups_data(results.size() + 1);
  Array<int> edge_groups_data(results.size() + 1);
  Array<int> face_groups_data(results.size() + 1);
  Array<int> corner_groups_data(results.size() + 1);
  threading::parallel_for(results.index_range(), 1024, [&](const IndexRange results_range) {
    for (const int i_result : results_range) {
      const meshintersect::CDT_result<double> &result = results[i_result].cdt_result;
      vert_groups_data[i_result] = result.vert.size();
      edge_groups_data[i_result] = result.edge.size();
      face_groups_data[i_result] = result.face.size();
      int corners_num = 0;
      for (const Vector<int> &face : result.face) {
        corners_num += face.size();
      }
      corner_groups_data[i_result] = corners_num;
    }
  });

  const OffsetIndices vert_groups = offset_indices::accumulate_counts_to_offsets(vert_groups_data);
  const OffsetIndices edge_groups = offset_indices::accumulate_counts_to_offsets(edge_groups_data);
  const OffsetIndices face_groups = offset_indices::accumulate_counts_to_offsets(face_groups_data);
  const OffsetIndices corner_groups = offset_indices::accumulate_counts_to_offsets(
      corner_groups_data);

  Mesh *dst_mesh = BKE_mesh_new_nomain(vert_groups.total_size(),
                                       edge_groups.total_size(),
                                       face_groups.total_size(),
                                       corner_groups.total_size());

  MutableSpan<float3> all_positions = dst_mesh->vert_positions_for_write();
  MutableSpan<int2> all_edges = dst_mesh->edges_for_write();
  MutableSpan<int> all_face_offsets = dst_mesh->face_offsets_for_write();
  MutableSpan<int> all_corner_verts = dst_mesh->corner_verts_for_write();

  threading::parallel_for(results.index_range(), 1024, [&](const IndexRange results_range) {
    for (const int i_result : results_range) {
      const meshintersect::CDT_result<double> &result = results[i_result].cdt_result;
      const IndexRange verts_range = vert_groups[i_result];
      const IndexRange edges_range = edge_groups[i_result];
      const IndexRange faces_range = face_groups[i_result];
      const IndexRange loops_range = corner_groups[i_result];

      MutableSpan<float3> positions = all_positions.slice(verts_range);
      for (const int i : result.vert.index_range()) {
        positions[i] = float3(float(result.vert[i].x), float(result.vert[i].y), 0.0f);
      }

      MutableSpan<int2> edges = all_edges.slice(edges_range);
      for (const int i : result.edge.index_range()) {
        edges[i] = int2(result.edge[i][0] + verts_range.start(),
                        result.edge[i][1] + verts_range.start());
      }

      MutableSpan<int> face_offsets = all_face_offsets.slice(faces_range);
      MutableSpan<int> corner_verts = all_corner_verts.slice(loops_range);
      int i_face_corner = 0;
      for (const int i_face : result.face.index_range()) {
        face_offsets[i_face] = i_face_corner + loops_range.start();
        for (const int i_corner : result.face[i_face].index_range()) {
          corner_verts[i_face_corner] = result.face[i_face][i_corner] + verts_range.start();
          i_face_corner++;
        }
      }
    }
  });

  MutableAttributeAccessor dst_attributes = dst_mesh->attributes_for_write();

  /* Gather the point attributes to propagate from all source components, and the type to use
   * for each. */
  bke::GeometrySet::GatheredAttributes attributes_to_propagate;
  for (const int i : src_points_by_component.index_range()) {
    if (src_points_by_component[i].is_empty()) {
      continue;
    }
    const AttributeAccessor src_attributes = src_component_attributes(
        mesh, curves, pointcloud, SourceComponent(i));
    src_attributes.foreach_attribute([&](const AttributeIter &iter) {
      if (iter.domain != AttrDomain::Point || iter.data_type == bke::AttrType::String) {
        return;
      }
      if (iter.name == "position" || attribute_filter.allow_skip(iter.name)) {
        return;
      }
      if (iter.is_builtin && !dst_attributes.is_builtin(iter.name)) {
        return;
      }
      attributes_to_propagate.add(iter.name, {AttrDomain::Point, iter.data_type});
    });
  }

  /* Copy the values of original points and mix the values of intersection points. */
  for (const int attr_i : attributes_to_propagate.names.index_range()) {
    const StringRef name = attributes_to_propagate.names[attr_i];
    const bke::AttrType data_type = attributes_to_propagate.kinds[attr_i].data_type;
    GSpanAttributeWriter dst = dst_attributes.lookup_or_add_for_write_only_span(
        name, AttrDomain::Point, data_type);
    if (!dst) {
      continue;
    }
    const GVArraySpan src(
        src_attribute(mesh, curves, pointcloud, src_points_by_component, name, data_type));
    threading::parallel_for(
        results.index_range(),
        4096,
        [&](const IndexRange range) {
          for (const int result_i : range) {
            const TriangulationResult &result = results[result_i];
            GMutableSpan dst_verts = dst.span.slice(vert_groups[result_i]);
            array_utils::gather(src,
                                result.src_point_by_dst_point,
                                dst_verts.take_front(result.src_point_by_dst_point.size()));
            if (!result.intersection_points.is_empty()) {
              bke::attribute_math::mix_groups(src,
                                              result.intersection_srcs(),
                                              result.intersection_src_points,
                                              result.intersection_src_weights.as_span(),
                                              dst_verts.slice(result.intersection_points));
            }
          }
        },
        threading::accumulated_task_sizes(
            [&](const IndexRange range) { return vert_groups[range].size(); }));
    dst.finish();
  }

  if (dst_intersection_points_attribute_id) {
    if (SpanAttributeWriter dst_intersection_points =
            dst_attributes.lookup_or_add_for_write_span<bool>(
                *dst_intersection_points_attribute_id, AttrDomain::Point))
    {
      threading::parallel_for(results.index_range(), 1024, [&](const IndexRange results_range) {
        for (const int result_i : results_range) {
          const IndexRange verts_range = vert_groups[result_i];
          const IndexRange intersection_points = results[result_i].intersection_points;
          dst_intersection_points.span.slice(verts_range).slice(intersection_points).fill(true);
        }
      });
      dst_intersection_points.finish();
    }
  }

  /* The delaunay triangulation doesn't seem to return all of the necessary all_edges, even in
   * triangulation mode. */
  bke::mesh_calc_edges(*dst_mesh, true, false);
  bke::mesh_smooth_set(*dst_mesh, false);

  dst_mesh->tag_overlapping_none();

  return dst_mesh;
}

static void node_geo_exec(GeoNodeExecParams params)
{
  GeometrySet geometry = params.extract_input<GeometrySet>("Geometry"_ustr);
  Field<int> group_index = params.extract_input<Field<int>>("Group ID"_ustr);

  const TriangulationMode mode = params.extract_input<TriangulationMode>("Mode"_ustr);
  const TriangulationType type = params.extract_input<TriangulationType>("Type"_ustr);
  const FillRule fill_rule = params.extract_input<FillRule>("Fill Rule"_ustr);
  const CDT_output_type output_type = get_cdt_output_type(mode, type, fill_rule);

  const AttributeFilter &attribute_filter = params.get_attribute_filter("Mesh"_ustr);
  std::optional<std::string> dst_intersection_points_attribute_id =
      params.get_output_anonymous_attribute_id_if_needed("Intersection Points"_ustr);

  geometry::foreach_real_geometry(geometry, [&](GeometrySet &geometry) {
    if (geometry.has_mesh() || geometry.has_curves() || geometry.has_pointcloud()) {
      const Mesh *mesh = geometry.get_mesh();
      const Curves *curves_id = geometry.get_curves();
      const bke::CurvesGeometry *curves = curves_id ? &curves_id->geometry.wrap() : nullptr;
      const PointCloud *pointcloud = geometry.get_pointcloud();
      std::optional<bke::CurvesFieldContext> curves_field_context;
      if (curves) {
        curves_field_context.emplace(bke::CurvesFieldContext(*curves, bke::AttrDomain::Curve));
      }
      const Array<int, 4> src_offsets = src_component_point_offsets(mesh, curves, pointcloud);
      Array<TriangulationResult> geometry_results = calc_triangulations(
          mesh,
          curves,
          curves_field_context.has_value() ? &*curves_field_context : nullptr,
          pointcloud,
          src_offsets.as_span(),
          group_index,
          output_type);
      Mesh *result_mesh = cdts_to_mesh(geometry_results,
                                       mesh,
                                       curves,
                                       pointcloud,
                                       src_offsets.as_span(),
                                       dst_intersection_points_attribute_id,
                                       attribute_filter);
      geometry.replace_mesh(result_mesh);
    }

    if (geometry.has_grease_pencil()) {
      using namespace blender::bke::greasepencil;
      const GreasePencil &grease_pencil = *geometry.get_grease_pencil();
      Vector<Mesh *> mesh_by_layer(grease_pencil.layers().size(), nullptr);
      for (const int layer_index : grease_pencil.layers().index_range()) {
        const Drawing *drawing = grease_pencil.get_eval_drawing(grease_pencil.layer(layer_index));
        if (drawing == nullptr) {
          continue;
        }
        const bke::CurvesGeometry &curves = drawing->strokes();
        if (curves.is_empty()) {
          continue;
        }
        bke::GreasePencilLayerFieldContext layer_field_context(
            grease_pencil, bke::AttrDomain::Curve, layer_index);
        const Array<int, 4> src_offsets = src_component_point_offsets(nullptr, &curves, nullptr);
        Array<TriangulationResult> geometry_results = calc_triangulations(nullptr,
                                                                          &curves,
                                                                          &layer_field_context,
                                                                          nullptr,
                                                                          src_offsets.as_span(),
                                                                          group_index,
                                                                          output_type);
        mesh_by_layer[layer_index] = cdts_to_mesh(geometry_results,
                                                  nullptr,
                                                  &curves,
                                                  nullptr,
                                                  src_offsets.as_span(),
                                                  dst_intersection_points_attribute_id,
                                                  attribute_filter);
      }
      if (!mesh_by_layer.is_empty()) {
        auto instances = std::make_unique<bke::Instances>(mesh_by_layer.size());
        MutableSpan<int> handles = instances->reference_handles_for_write();
        MutableSpan<float4x4> transforms = instances->transforms_for_write();
        for (const int i : mesh_by_layer.index_range()) {
          Mesh *mesh = mesh_by_layer[i];
          if (!mesh) {
            /* Add an empty reference so the number of layers and instances match.
             * This makes it easy to reconstruct the layers afterwards and keep their attributes.
             * Although in this particular case we don't propagate the attributes. */
            handles[i] = instances->add_reference(bke::InstanceReference());
            transforms[i] = float4x4::identity();
            continue;
          }
          GeometrySet temp_set = GeometrySet::from_mesh(mesh);
          handles[i] = instances->add_reference(bke::InstanceReference{temp_set});
          transforms[i] = grease_pencil.layer(i).local_transform();
        }
        auto &dst_component = geometry.get_component_for_write<InstancesComponent>();
        GeometrySet new_instances = geometry::join_geometries(
            {GeometrySet::from_instances(dst_component.release()),
             GeometrySet::from_instances(std::move(instances))},
            {});
        dst_component.replace(
            new_instances.get_component_for_write<InstancesComponent>().release());
      }
    }

    geometry.keep_only({GeometryComponent::Type::Mesh, GeometryComponent::Type::Instance});
  });

  params.set_output("Mesh"_ustr, std::move(geometry));
}

static void node_register()
{
  static blender::bke::bNodeType ntype;
  geo_node_type_base(&ntype, "GeometryNodeDelaunayTriangulation"_ustr);
  ntype.ui_name = "Delaunay Triangulation";
  ntype.ui_description =
      "Generate a triangulated mesh from a set of points in the X-Y plane. Uses edges and faces "
      "as triangulation constraints";
  ntype.nclass = NODE_CLASS_GEOMETRY;
  ntype.declare = node_declare;
  ntype.geometry_node_execute = node_geo_exec;
  ntype.default_width = bke::NodeWidth::_160;
  blender::bke::node_register_type(ntype);
}
NOD_REGISTER_NODE(node_register)

}  // namespace blender::nodes::node_geo_delaunay_triangulation_cc
