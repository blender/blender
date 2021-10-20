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

#include "BLI_array.hh"
#include "BLI_set.hh"
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute_access.hh"
#include "BKE_attribute_math.hh"
#include "BKE_geometry_set.hh"
#include "BKE_spline.hh"

#include "GEO_mesh_to_curve.hh"

namespace blender::geometry {

template<typename T>
static void copy_attribute_to_points(const VArray<T> &source_data,
                                     Span<int> map,
                                     MutableSpan<T> dest_data)
{
  for (const int point_index : map.index_range()) {
    const int vert_index = map[point_index];
    dest_data[point_index] = source_data[vert_index];
  }
}

static void copy_attributes_to_points(CurveEval &curve,
                                      const MeshComponent &mesh_component,
                                      Span<Vector<int>> point_to_vert_maps)
{
  MutableSpan<SplinePtr> splines = curve.splines();
  Set<bke::AttributeIDRef> source_attribute_ids = mesh_component.attribute_ids();

  /* Copy builtin control point attributes. */
  if (source_attribute_ids.contains("tilt")) {
    const fn::GVArray_Typed<float> tilt_attribute = mesh_component.attribute_get_for_read<float>(
        "tilt", ATTR_DOMAIN_POINT, 0.0f);
    threading::parallel_for(splines.index_range(), 256, [&](IndexRange range) {
      for (const int i : range) {
        copy_attribute_to_points<float>(
            *tilt_attribute, point_to_vert_maps[i], splines[i]->tilts());
      }
    });
    source_attribute_ids.remove_contained("tilt");
  }
  if (source_attribute_ids.contains("radius")) {
    const fn::GVArray_Typed<float> radius_attribute = mesh_component.attribute_get_for_read<float>(
        "radius", ATTR_DOMAIN_POINT, 1.0f);
    threading::parallel_for(splines.index_range(), 256, [&](IndexRange range) {
      for (const int i : range) {
        copy_attribute_to_points<float>(
            *radius_attribute, point_to_vert_maps[i], splines[i]->radii());
      }
    });
    source_attribute_ids.remove_contained("radius");
  }

  for (const bke::AttributeIDRef &attribute_id : source_attribute_ids) {
    /* Don't copy attributes that are built-in on meshes but not on curves. */
    if (mesh_component.attribute_is_builtin(attribute_id)) {
      continue;
    }

    /* Don't copy anonymous attributes with no references anymore. */
    if (attribute_id.is_anonymous()) {
      const AnonymousAttributeID &anonymous_id = attribute_id.anonymous_id();
      if (!BKE_anonymous_attribute_id_has_strong_references(&anonymous_id)) {
        continue;
      }
    }

    const fn::GVArrayPtr mesh_attribute = mesh_component.attribute_try_get_for_read(
        attribute_id, ATTR_DOMAIN_POINT);
    /* Some attributes might not exist if they were builtin attribute on domains that don't
     * have any elements, i.e. a face attribute on the output of the line primitive node. */
    if (!mesh_attribute) {
      continue;
    }

    const CustomDataType data_type = bke::cpp_type_to_custom_data_type(mesh_attribute->type());

    threading::parallel_for(splines.index_range(), 128, [&](IndexRange range) {
      for (const int i : range) {
        /* Create attribute on the spline points. */
        splines[i]->attributes.create(attribute_id, data_type);
        std::optional<fn::GMutableSpan> spline_attribute = splines[i]->attributes.get_for_write(
            attribute_id);
        BLI_assert(spline_attribute);

        /* Copy attribute based on the map for this spline. */
        attribute_math::convert_to_static_type(mesh_attribute->type(), [&](auto dummy) {
          using T = decltype(dummy);
          copy_attribute_to_points<T>(
              mesh_attribute->typed<T>(), point_to_vert_maps[i], spline_attribute->typed<T>());
        });
      }
    });
  }

  curve.assert_valid_point_attributes();
}

struct CurveFromEdgesOutput {
  std::unique_ptr<CurveEval> curve;
  Vector<Vector<int>> point_to_vert_maps;
};

static CurveFromEdgesOutput edges_to_curve(Span<MVert> verts, Span<std::pair<int, int>> edges)
{
  std::unique_ptr<CurveEval> curve = std::make_unique<CurveEval>();
  Vector<Vector<int>> point_to_vert_maps;

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
    /* The vertex will be part of a cyclic spline. */
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

      std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();
      Vector<int> point_to_vert_map;

      spline->add_point(verts[current_vert].co, 1.0f, 0.0f);
      point_to_vert_map.append(current_vert);

      /* Follow connected edges until we read a vertex with more than two connected edges. */
      while (true) {
        int last_vert = current_vert;
        current_vert = next_vert;

        spline->add_point(verts[current_vert].co, 1.0f, 0.0f);
        point_to_vert_map.append(current_vert);
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

      spline->attributes.reallocate(spline->size());
      curve->add_spline(std::move(spline));
      point_to_vert_maps.append(std::move(point_to_vert_map));
    }
  }

  /* All remaining edges are part of cyclic splines (we skipped vertices with two edges before). */
  for (const int start_vert : verts.index_range()) {
    if (unused_edges[start_vert] != 2) {
      continue;
    }

    int current_vert = start_vert;
    int next_vert = neighbors[neighbor_offsets[current_vert]];

    std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();
    Vector<int> point_to_vert_map;
    spline->set_cyclic(true);

    spline->add_point(verts[current_vert].co, 1.0f, 0.0f);
    point_to_vert_map.append(current_vert);

    /* Follow connected edges until we loop back to the start vertex. */
    while (next_vert != start_vert) {
      const int last_vert = current_vert;
      current_vert = next_vert;

      spline->add_point(verts[current_vert].co, 1.0f, 0.0f);
      point_to_vert_map.append(current_vert);
      unused_edges[current_vert]--;
      unused_edges[last_vert]--;

      const int offset = neighbor_offsets[current_vert];
      const int next_a = neighbors[offset];
      const int next_b = neighbors[offset + 1];
      next_vert = (last_vert == next_a) ? next_b : next_a;
    }

    spline->attributes.reallocate(spline->size());
    curve->add_spline(std::move(spline));
    point_to_vert_maps.append(std::move(point_to_vert_map));
  }

  curve->attributes.reallocate(curve->splines().size());
  return {std::move(curve), std::move(point_to_vert_maps)};
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

/**
 * Convert the mesh into one or many poly splines. Since splines cannot have branches,
 * intersections of more than three edges will become breaks in splines. Attributes that
 * are not built-in on meshes and not curves are transferred to the result curve.
 */
std::unique_ptr<CurveEval> mesh_to_curve_convert(const MeshComponent &mesh_component,
                                                 const IndexMask selection)
{
  const Mesh &mesh = *mesh_component.get_for_read();
  Vector<std::pair<int, int>> selected_edges = get_selected_edges(*mesh_component.get_for_read(),
                                                                  selection);
  CurveFromEdgesOutput output = edges_to_curve({mesh.mvert, mesh.totvert}, selected_edges);
  copy_attributes_to_points(*output.curve, mesh_component, output.point_to_vert_maps);
  return std::move(output.curve);
}

}  // namespace blender::geometry
