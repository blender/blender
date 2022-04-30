/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_array.hh"
#include "BLI_devirtualize_parameters.hh"
#include "BLI_set.hh"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute_access.hh"
#include "BKE_attribute_math.hh"
#include "BKE_curves.hh"
#include "BKE_geometry_set.hh"

#include "GEO_mesh_to_curve.hh"

namespace blender::geometry {

template<typename T>
static void copy_with_map(const VArray<T> &src, Span<int> map, MutableSpan<T> dst)
{
  devirtualize_varray(src, [&](const auto &src) {
    threading::parallel_for(map.index_range(), 1024, [&](const IndexRange range) {
      for (const int i : range) {
        const int vert_index = map[i];
        dst[i] = src[vert_index];
      }
    });
  });
}

static Curves *create_curve_from_vert_indices(const MeshComponent &mesh_component,
                                              const Span<int> vert_indices,
                                              const Span<int> curve_offsets,
                                              const IndexRange cyclic_curves)
{
  Curves *curves_id = bke::curves_new_nomain(vert_indices.size(), curve_offsets.size());
  bke::CurvesGeometry &curves = bke::CurvesGeometry::wrap(curves_id->geometry);
  curves.offsets_for_write().drop_back(1).copy_from(curve_offsets);
  curves.offsets_for_write().last() = vert_indices.size();
  curves.fill_curve_types(CURVE_TYPE_POLY);

  curves.cyclic_for_write().fill(false);
  curves.cyclic_for_write().slice(cyclic_curves).fill(true);

  Set<bke::AttributeIDRef> source_attribute_ids = mesh_component.attribute_ids();

  CurveComponent curves_component;
  curves_component.replace(curves_id, GeometryOwnershipType::Editable);

  for (const bke::AttributeIDRef &attribute_id : source_attribute_ids) {
    if (mesh_component.attribute_is_builtin(attribute_id) &&
        !curves_component.attribute_is_builtin(attribute_id)) {
      /* Don't copy attributes that are built-in on meshes but not on curves. */
      continue;
    }

    if (!attribute_id.should_be_kept()) {
      continue;
    }

    const GVArray mesh_attribute = mesh_component.attribute_try_get_for_read(attribute_id,
                                                                             ATTR_DOMAIN_POINT);
    /* Some attributes might not exist if they were builtin attribute on domains that don't
     * have any elements, i.e. a face attribute on the output of the line primitive node. */
    if (!mesh_attribute) {
      continue;
    }

    /* Copy attribute based on the map for this curve. */
    attribute_math::convert_to_static_type(mesh_attribute.type(), [&](auto dummy) {
      using T = decltype(dummy);
      bke::OutputAttribute_Typed<T> attribute =
          curves_component.attribute_try_get_for_output_only<T>(attribute_id, ATTR_DOMAIN_POINT);
      copy_with_map<T>(mesh_attribute.typed<T>(), vert_indices, attribute.as_span());
      attribute.save();
    });
  }

  return curves_id;
}

struct CurveFromEdgesOutput {
  /** The indices in the mesh for each control point of each result curves. */
  Vector<int> vert_indices;
  /** The first index of each curve in the result. */
  Vector<int> curve_offsets;
  /** A subset of curves that should be set cyclic. */
  IndexRange cyclic_curves;
};

static CurveFromEdgesOutput edges_to_curve_point_indices(Span<MVert> verts,
                                                         Span<std::pair<int, int>> edges)
{
  Vector<int> vert_indices;
  vert_indices.reserve(edges.size());
  Vector<int> curve_offsets;

  /* Compute the number of edges connecting to each vertex. */
  Array<int> neighbor_count(verts.size(), 0);
  for (const std::pair<int, int> &edge : edges) {
    neighbor_count[edge.first]++;
    neighbor_count[edge.second]++;
  }

  /* Compute an offset into the array of neighbor edges based on the counts. */
  Array<int> neighbor_offsets(verts.size());
  int start = 0;
  for (const int i : verts.index_range()) {
    neighbor_offsets[i] = start;
    start += neighbor_count[i];
  }

  /* Use as an index into the "neighbor group" for each vertex. */
  Array<int> used_slots(verts.size(), 0);
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

  for (const int start_vert : verts.index_range()) {
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
  for (const int start_vert : verts.index_range()) {
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
  for (const int i : selection) {
    selected_edges.append({mesh.medge[i].v1, mesh.medge[i].v2});
  }
  return selected_edges;
}

Curves *mesh_to_curve_convert(const MeshComponent &mesh_component, const IndexMask selection)
{
  const Mesh &mesh = *mesh_component.get_for_read();
  Vector<std::pair<int, int>> selected_edges = get_selected_edges(*mesh_component.get_for_read(),
                                                                  selection);
  CurveFromEdgesOutput output = edges_to_curve_point_indices({mesh.mvert, mesh.totvert},
                                                             selected_edges);

  return create_curve_from_vert_indices(
      mesh_component, output.vert_indices, output.curve_offsets, output.cyclic_curves);
}

}  // namespace blender::geometry
