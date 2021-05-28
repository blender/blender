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
#include "BLI_task.hh"

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_spline.hh"

#include "node_geometry_util.hh"

using blender::Array;

static bNodeSocketTemplate geo_node_mesh_to_curve_in[] = {
    {SOCK_GEOMETRY, N_("Mesh")},
    {SOCK_STRING, N_("Selection")},
    {-1, ""},
};

static bNodeSocketTemplate geo_node_mesh_to_curve_out[] = {
    {SOCK_GEOMETRY, N_("Curve")},
    {-1, ""},
};

namespace blender::nodes {

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
  Set<std::string> source_attribute_names = mesh_component.attribute_names();

  /* Copy builtin control point attributes. */
  if (source_attribute_names.contains_as("tilt")) {
    const GVArray_Typed<float> tilt_attribute = mesh_component.attribute_get_for_read<float>(
        "tilt", ATTR_DOMAIN_POINT, 0.0f);
    parallel_for(splines.index_range(), 256, [&](IndexRange range) {
      for (const int i : range) {
        copy_attribute_to_points<float>(
            *tilt_attribute, point_to_vert_maps[i], splines[i]->tilts());
      }
    });
    source_attribute_names.remove_contained_as("tilt");
  }
  if (source_attribute_names.contains_as("radius")) {
    const GVArray_Typed<float> radius_attribute = mesh_component.attribute_get_for_read<float>(
        "radius", ATTR_DOMAIN_POINT, 1.0f);
    parallel_for(splines.index_range(), 256, [&](IndexRange range) {
      for (const int i : range) {
        copy_attribute_to_points<float>(
            *radius_attribute, point_to_vert_maps[i], splines[i]->radii());
      }
    });
    source_attribute_names.remove_contained_as("radius");
  }

  /* Don't copy other builtin control point attributes. */
  source_attribute_names.remove_as("position");

  /* Copy dynamic control point attributes. */
  for (const StringRef name : source_attribute_names) {
    const GVArrayPtr mesh_attribute = mesh_component.attribute_try_get_for_read(name,
                                                                                ATTR_DOMAIN_POINT);
    /* Some attributes might not exist if they were builtin attribute on domains that don't
     * have any elements, i.e. a face attribute on the output of the line primitive node. */
    if (!mesh_attribute) {
      continue;
    }

    const CustomDataType data_type = bke::cpp_type_to_custom_data_type(mesh_attribute->type());

    parallel_for(splines.index_range(), 128, [&](IndexRange range) {
      for (const int i : range) {
        /* Create attribute on the spline points. */
        splines[i]->attributes.create(name, data_type);
        std::optional<GMutableSpan> spline_attribute = splines[i]->attributes.get_for_write(name);
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

static CurveFromEdgesOutput mesh_to_curve(Span<MVert> verts, Span<std::pair<int, int>> edges)
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
static Vector<std::pair<int, int>> get_selected_edges(GeoNodeExecParams params,
                                                      const MeshComponent &component)
{
  const Mesh &mesh = *component.get_for_read();
  const std::string selection_name = params.extract_input<std::string>("Selection");
  if (!selection_name.empty() && !component.attribute_exists(selection_name)) {
    params.error_message_add(NodeWarningType::Error,
                             TIP_("No attribute with name \"") + selection_name + "\"");
  }
  GVArray_Typed<bool> selection = component.attribute_get_for_read<bool>(
      selection_name, ATTR_DOMAIN_EDGE, true);

  Vector<std::pair<int, int>> selected_edges;
  for (const int i : IndexRange(mesh.totedge)) {
    if (selection[i]) {
      selected_edges.append({mesh.medge[i].v1, mesh.medge[i].v2});
    }
  }

  return selected_edges;
}

static void geo_node_mesh_to_curve_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Mesh");

  geometry_set = bke::geometry_set_realize_instances(geometry_set);

  if (!geometry_set.has_mesh()) {
    params.set_output("Curve", GeometrySet());
    return;
  }

  const MeshComponent &component = *geometry_set.get_component_for_read<MeshComponent>();
  const Mesh &mesh = *component.get_for_read();
  Span<MVert> verts = Span{mesh.mvert, mesh.totvert};
  Span<MEdge> edges = Span{mesh.medge, mesh.totedge};
  if (edges.size() == 0) {
    params.set_output("Curve", GeometrySet());
    return;
  }

  Vector<std::pair<int, int>> selected_edges = get_selected_edges(params, component);

  CurveFromEdgesOutput output = mesh_to_curve(verts, selected_edges);
  copy_attributes_to_points(*output.curve, component, output.point_to_vert_maps);

  params.set_output("Curve", GeometrySet::create_with_curve(output.curve.release()));
}

}  // namespace blender::nodes

void register_node_type_geo_mesh_to_curve()
{
  static bNodeType ntype;

  geo_node_type_base(&ntype, GEO_NODE_MESH_TO_CURVE, "Mesh to Curve", NODE_CLASS_GEOMETRY, 0);
  node_type_socket_templates(&ntype, geo_node_mesh_to_curve_in, geo_node_mesh_to_curve_out);
  ntype.geometry_node_execute = blender::nodes::geo_node_mesh_to_curve_exec;
  nodeRegisterType(&ntype);
}
