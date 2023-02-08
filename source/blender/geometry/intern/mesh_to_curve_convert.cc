/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_array_utils.hh"
#include "BLI_set.hh"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"
#include "BKE_mesh.h"

#include "GEO_mesh_to_curve.hh"

namespace blender::geometry {

bke::CurvesGeometry create_curve_from_vert_indices(
    const Mesh &mesh,
    const Span<int> vert_indices,
    const Span<int> curve_offsets,
    const IndexRange cyclic_curves,
    const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  bke::CurvesGeometry curves(vert_indices.size(), curve_offsets.size());
  curves.offsets_for_write().drop_back(1).copy_from(curve_offsets);
  curves.offsets_for_write().last() = vert_indices.size();
  curves.fill_curve_types(CURVE_TYPE_POLY);

  const bke::AttributeAccessor mesh_attributes = mesh.attributes();
  bke::MutableAttributeAccessor curves_attributes = curves.attributes_for_write();

  if (!cyclic_curves.is_empty()) {
    bke::SpanAttributeWriter cyclic = curves_attributes.lookup_or_add_for_write_span<bool>(
        "cyclic", ATTR_DOMAIN_CURVE);
    cyclic.span.slice(cyclic_curves).fill(true);
    cyclic.finish();
  }

  Set<bke::AttributeIDRef> source_attribute_ids = mesh_attributes.all_ids();

  for (const bke::AttributeIDRef &attribute_id : source_attribute_ids) {
    if (mesh_attributes.is_builtin(attribute_id) && !curves_attributes.is_builtin(attribute_id)) {
      /* Don't copy attributes that are built-in on meshes but not on curves. */
      continue;
    }

    if (attribute_id.is_anonymous() && !propagation_info.propagate(attribute_id.anonymous_id())) {
      continue;
    }

    const GVArray mesh_attribute = mesh_attributes.lookup(attribute_id, ATTR_DOMAIN_POINT);
    /* Some attributes might not exist if they were builtin attribute on domains that don't
     * have any elements, i.e. a face attribute on the output of the line primitive node. */
    if (!mesh_attribute) {
      continue;
    }

    /* Copy attribute based on the map for this curve. */
    attribute_math::convert_to_static_type(mesh_attribute.type(), [&](auto dummy) {
      using T = decltype(dummy);
      bke::SpanAttributeWriter<T> attribute =
          curves_attributes.lookup_or_add_for_write_only_span<T>(attribute_id, ATTR_DOMAIN_POINT);
      array_utils::gather<T>(mesh_attribute.typed<T>(), vert_indices, attribute.span);
      attribute.finish();
    });
  }

  return curves;
}

struct CurveFromEdgesOutput {
  /** The indices in the mesh for each control point of each result curves. */
  Vector<int> vert_indices;
  /** The first index of each curve in the result. */
  Vector<int> curve_offsets;
  /** A subset of curves that should be set cyclic. */
  IndexRange cyclic_curves;
};

static CurveFromEdgesOutput edges_to_curve_point_indices(const int verts_num,
                                                         Span<std::pair<int, int>> edges)
{
  Vector<int> vert_indices;
  vert_indices.reserve(edges.size());
  Vector<int> curve_offsets;

  /* Compute the number of edges connecting to each vertex. */
  Array<int> neighbor_count(verts_num, 0);
  for (const std::pair<int, int> &edge : edges) {
    neighbor_count[edge.first]++;
    neighbor_count[edge.second]++;
  }

  /* Compute an offset into the array of neighbor edges based on the counts. */
  Array<int> neighbor_offsets(verts_num);
  int start = 0;
  for (const int i : IndexRange(verts_num)) {
    neighbor_offsets[i] = start;
    start += neighbor_count[i];
  }

  /* Use as an index into the "neighbor group" for each vertex. */
  Array<int> used_slots(verts_num, 0);
  /* Calculate the indices of each vertex's neighboring edges. */
  Array<int> neighbors(edges.size() * 2);
  for (const int i : edges.index_range()) {
    const int v1 = edges[i].first;
    const int v2 = edges[i].second;
    neighbors[neighbor_offsets[v1] + used_slots[v1]] = v2;
    neighbors[neighbor_offsets[v2] + used_slots[v2]] = v1;
    used_slots[v1]++;
    used_slots[v2]++;
  }

  /* Now use the neighbor group offsets calculated above as a count used edges at each vertex. */
  Array<int> unused_edges = std::move(used_slots);

  for (const int start_vert : IndexRange(verts_num)) {
    /* The vertex will be part of a cyclic curve. */
    if (neighbor_count[start_vert] == 2) {
      continue;
    }

    /* The vertex has no connected edges, or they were already used. */
    if (unused_edges[start_vert] == 0) {
      continue;
    }

    for (const int i : IndexRange(neighbor_count[start_vert])) {
      int current_vert = start_vert;
      int next_vert = neighbors[neighbor_offsets[current_vert] + i];

      if (unused_edges[next_vert] == 0) {
        continue;
      }

      /* Start a new curve in the output. */
      curve_offsets.append(vert_indices.size());
      vert_indices.append(current_vert);

      /* Follow connected edges until we read a vertex with more than two connected edges. */
      while (true) {
        int last_vert = current_vert;
        current_vert = next_vert;

        vert_indices.append(current_vert);
        unused_edges[current_vert]--;
        unused_edges[last_vert]--;

        if (neighbor_count[current_vert] != 2) {
          break;
        }

        const int offset = neighbor_offsets[current_vert];
        const int next_a = neighbors[offset];
        const int next_b = neighbors[offset + 1];
        next_vert = (last_vert == next_a) ? next_b : next_a;
      }
    }
  }

  /* All curves added after this are cyclic. */
  const int cyclic_start = curve_offsets.size();

  /* All remaining edges are part of cyclic curves (we skipped vertices with two edges before). */
  for (const int start_vert : IndexRange(verts_num)) {
    if (unused_edges[start_vert] != 2) {
      continue;
    }

    int current_vert = start_vert;
    int next_vert = neighbors[neighbor_offsets[current_vert]];

    curve_offsets.append(vert_indices.size());
    vert_indices.append(current_vert);

    /* Follow connected edges until we loop back to the start vertex. */
    while (next_vert != start_vert) {
      const int last_vert = current_vert;
      current_vert = next_vert;

      vert_indices.append(current_vert);
      unused_edges[current_vert]--;
      unused_edges[last_vert]--;

      const int offset = neighbor_offsets[current_vert];
      const int next_a = neighbors[offset];
      const int next_b = neighbors[offset + 1];
      next_vert = (last_vert == next_a) ? next_b : next_a;
    }
  }

  const IndexRange cyclic_curves = curve_offsets.index_range().drop_front(cyclic_start);

  return {std::move(vert_indices), std::move(curve_offsets), cyclic_curves};
}

/**
 * Get a separate array of the indices for edges in a selection (a boolean attribute).
 * This helps to make the above algorithm simpler by removing the need to check for selection
 * in many places.
 */
static Vector<std::pair<int, int>> get_selected_edges(const Mesh &mesh, const IndexMask selection)
{
  Vector<std::pair<int, int>> selected_edges;
  const Span<MEdge> edges = mesh.edges();
  for (const int i : selection) {
    selected_edges.append({edges[i].v1, edges[i].v2});
  }
  return selected_edges;
}

bke::CurvesGeometry mesh_to_curve_convert(
    const Mesh &mesh,
    const IndexMask selection,
    const bke::AnonymousAttributePropagationInfo &propagation_info)
{
  Vector<std::pair<int, int>> selected_edges = get_selected_edges(mesh, selection);
  CurveFromEdgesOutput output = edges_to_curve_point_indices(mesh.totvert, selected_edges);

  return create_curve_from_vert_indices(
      mesh, output.vert_indices, output.curve_offsets, output.cyclic_curves, propagation_info);
}

}  // namespace blender::geometry
