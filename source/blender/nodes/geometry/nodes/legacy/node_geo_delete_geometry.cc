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

#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

#include "BKE_customdata.h"
#include "BKE_mesh.h"
#include "BKE_pointcloud.h"
#include "BKE_spline.hh"

#include "node_geometry_util.hh"

using blender::bke::CustomDataAttributes;

/* Code from the mask modifier in MOD_mask.cc. */
extern void copy_masked_vertices_to_new_mesh(const Mesh &src_mesh,
                                             Mesh &dst_mesh,
                                             blender::Span<int> vertex_map);
extern void copy_masked_edges_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          blender::Span<int> vertex_map,
                                          blender::Span<int> edge_map);
extern void copy_masked_polys_to_new_mesh(const Mesh &src_mesh,
                                          Mesh &dst_mesh,
                                          blender::Span<int> vertex_map,
                                          blender::Span<int> edge_map,
                                          blender::Span<int> masked_poly_indices,
                                          blender::Span<int> new_loop_starts);

namespace blender::nodes {

static void geo_node_delete_geometry_declare(NodeDeclarationBuilder &b)
{
  b.add_input<decl::Geometry>("Geometry");
  b.add_input<decl::String>("Selection");
  b.add_input<decl::Bool>("Invert");
  b.add_output<decl::Geometry>("Geometry");
}

template<typename T> static void copy_data(Span<T> data, MutableSpan<T> r_data, IndexMask mask)
{
  for (const int i_out : mask.index_range()) {
    r_data[i_out] = data[mask[i_out]];
  }
}

static void spline_copy_builtin_attributes(const Spline &spline,
                                           Spline &r_spline,
                                           const IndexMask mask)
{
  copy_data(spline.positions(), r_spline.positions(), mask);
  copy_data(spline.radii(), r_spline.radii(), mask);
  copy_data(spline.tilts(), r_spline.tilts(), mask);
  switch (spline.type()) {
    case Spline::Type::Poly:
      break;
    case Spline::Type::Bezier: {
      const BezierSpline &src = static_cast<const BezierSpline &>(spline);
      BezierSpline &dst = static_cast<BezierSpline &>(r_spline);
      copy_data(src.handle_positions_left(), dst.handle_positions_left(), mask);
      copy_data(src.handle_positions_right(), dst.handle_positions_right(), mask);
      copy_data(src.handle_types_left(), dst.handle_types_left(), mask);
      copy_data(src.handle_types_right(), dst.handle_types_right(), mask);
      break;
    }
    case Spline::Type::NURBS: {
      const NURBSpline &src = static_cast<const NURBSpline &>(spline);
      NURBSpline &dst = static_cast<NURBSpline &>(r_spline);
      copy_data(src.weights(), dst.weights(), mask);
      break;
    }
  }
}

static void copy_dynamic_attributes(const CustomDataAttributes &src,
                                    CustomDataAttributes &dst,
                                    const IndexMask mask)
{
  src.foreach_attribute(
      [&](const AttributeIDRef &attribute_id, const AttributeMetaData &meta_data) {
        std::optional<GSpan> src_attribute = src.get_for_read(attribute_id);
        BLI_assert(src_attribute);

        if (!dst.create(attribute_id, meta_data.data_type)) {
          /* Since the source spline of the same type had the attribute, adding it should work.
           */
          BLI_assert_unreachable();
        }

        std::optional<GMutableSpan> new_attribute = dst.get_for_write(attribute_id);
        BLI_assert(new_attribute);

        attribute_math::convert_to_static_type(new_attribute->type(), [&](auto dummy) {
          using T = decltype(dummy);
          copy_data(src_attribute->typed<T>(), new_attribute->typed<T>(), mask);
        });
        return true;
      },
      ATTR_DOMAIN_POINT);
}

static SplinePtr spline_delete(const Spline &spline, const IndexMask mask)
{
  SplinePtr new_spline = spline.copy_only_settings();
  new_spline->resize(mask.size());

  spline_copy_builtin_attributes(spline, *new_spline, mask);
  copy_dynamic_attributes(spline.attributes, new_spline->attributes, mask);

  return new_spline;
}

static std::unique_ptr<CurveEval> curve_delete(const CurveEval &input_curve,
                                               const StringRef name,
                                               const bool invert)
{
  Span<SplinePtr> input_splines = input_curve.splines();
  std::unique_ptr<CurveEval> output_curve = std::make_unique<CurveEval>();

  /* Keep track of which splines were copied to the result to copy spline domain attributes. */
  Vector<int64_t> copied_splines;

  if (input_curve.attributes.get_for_read(name)) {
    GVArray_Typed<bool> selection = input_curve.attributes.get_for_read<bool>(name, false);
    for (const int i : input_splines.index_range()) {
      if (selection[i] == invert) {
        output_curve->add_spline(input_splines[i]->copy());
        copied_splines.append(i);
      }
    }
  }
  else {
    /* Reuse index vector for each spline. */
    Vector<int64_t> indices_to_copy;

    for (const int i : input_splines.index_range()) {
      const Spline &spline = *input_splines[i];
      GVArray_Typed<bool> selection = spline.attributes.get_for_read<bool>(name, false);

      indices_to_copy.clear();
      for (const int i_point : IndexRange(spline.size())) {
        if (selection[i_point] == invert) {
          indices_to_copy.append(i_point);
        }
      }

      /* Avoid creating an empty spline. */
      if (indices_to_copy.is_empty()) {
        continue;
      }

      SplinePtr new_spline = spline_delete(spline, IndexMask(indices_to_copy));
      output_curve->add_spline(std::move(new_spline));
      copied_splines.append(i);
    }
  }

  if (copied_splines.is_empty()) {
    return {};
  }

  output_curve->attributes.reallocate(output_curve->splines().size());
  copy_dynamic_attributes(
      input_curve.attributes, output_curve->attributes, IndexMask(copied_splines));

  return output_curve;
}

static void delete_curve_selection(const CurveComponent &in_component,
                                   CurveComponent &r_component,
                                   const StringRef selection_name,
                                   const bool invert)
{
  std::unique_ptr<CurveEval> r_curve = curve_delete(
      *in_component.get_for_read(), selection_name, invert);
  if (r_curve) {
    r_component.replace(r_curve.release());
  }
  else {
    r_component.clear();
  }
}

static void delete_point_cloud_selection(const PointCloudComponent &in_component,
                                         PointCloudComponent &out_component,
                                         const StringRef selection_name,
                                         const bool invert)
{
  const GVArray_Typed<bool> selection_attribute = in_component.attribute_get_for_read<bool>(
      selection_name, ATTR_DOMAIN_POINT, false);
  VArray_Span<bool> selection{selection_attribute};

  const int total = selection.count(invert);
  if (total == 0) {
    out_component.clear();
    return;
  }
  out_component.replace(BKE_pointcloud_new_nomain(total));

  /* Invert the inversion, because this deletes the selected points instead of keeping them. */
  copy_point_attributes_based_on_mask(in_component, out_component, selection, !invert);
}

static void compute_selected_vertices_from_vertex_selection(const VArray<bool> &vertex_selection,
                                                            const bool invert,
                                                            MutableSpan<int> r_vertex_map,
                                                            uint *r_num_selected_vertices)
{
  BLI_assert(vertex_selection.size() == r_vertex_map.size());

  uint num_selected_vertices = 0;
  for (const int i : r_vertex_map.index_range()) {
    if (vertex_selection[i] != invert) {
      r_vertex_map[i] = num_selected_vertices;
      num_selected_vertices++;
    }
    else {
      r_vertex_map[i] = -1;
    }
  }

  *r_num_selected_vertices = num_selected_vertices;
}

static void compute_selected_edges_from_vertex_selection(const Mesh &mesh,
                                                         const VArray<bool> &vertex_selection,
                                                         const bool invert,
                                                         MutableSpan<int> r_edge_map,
                                                         uint *r_num_selected_edges)
{
  BLI_assert(mesh.totedge == r_edge_map.size());

  uint num_selected_edges = 0;
  for (const int i : IndexRange(mesh.totedge)) {
    const MEdge &edge = mesh.medge[i];

    /* Only add the edge if both vertices will be in the new mesh. */
    if (vertex_selection[edge.v1] != invert && vertex_selection[edge.v2] != invert) {
      r_edge_map[i] = num_selected_edges;
      num_selected_edges++;
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  *r_num_selected_edges = num_selected_edges;
}

static void compute_selected_polygons_from_vertex_selection(const Mesh &mesh,
                                                            const VArray<bool> &vertex_selection,
                                                            const bool invert,
                                                            Vector<int> &r_selected_poly_indices,
                                                            Vector<int> &r_loop_starts,
                                                            uint *r_num_selected_polys,
                                                            uint *r_num_selected_loops)
{
  BLI_assert(mesh.totvert == vertex_selection.size());

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  uint num_selected_loops = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];

    bool all_verts_in_selection = true;
    Span<MLoop> loops_src(&mesh.mloop[poly_src.loopstart], poly_src.totloop);
    for (const MLoop &loop : loops_src) {
      if (vertex_selection[loop.v] == invert) {
        all_verts_in_selection = false;
        break;
      }
    }

    if (all_verts_in_selection) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(num_selected_loops);
      num_selected_loops += poly_src.totloop;
    }
  }

  *r_num_selected_polys = r_selected_poly_indices.size();
  *r_num_selected_loops = num_selected_loops;
}

/**
 * Checks for every edge if it is in `edge_selection`. If it is, then the two vertices of the
 * edge are kept along with the edge.
 */
static void compute_selected_vertices_and_edges_from_edge_selection(
    const Mesh &mesh,
    const VArray<bool> &edge_selection,
    const bool invert,
    MutableSpan<int> r_vertex_map,
    MutableSpan<int> r_edge_map,
    uint *r_num_selected_vertices,
    uint *r_num_selected_edges)
{
  BLI_assert(mesh.totedge == edge_selection.size());

  uint num_selected_edges = 0;
  uint num_selected_vertices = 0;
  for (const int i : IndexRange(mesh.totedge)) {
    const MEdge &edge = mesh.medge[i];
    if (edge_selection[i] != invert) {
      r_edge_map[i] = num_selected_edges;
      num_selected_edges++;
      if (r_vertex_map[edge.v1] == -1) {
        r_vertex_map[edge.v1] = num_selected_vertices;
        num_selected_vertices++;
      }
      if (r_vertex_map[edge.v2] == -1) {
        r_vertex_map[edge.v2] = num_selected_vertices;
        num_selected_vertices++;
      }
    }
    else {
      r_edge_map[i] = -1;
    }
  }

  *r_num_selected_vertices = num_selected_vertices;
  *r_num_selected_edges = num_selected_edges;
}

/**
 * Checks for every polygon if all the edges are in `edge_selection`. If they are, then that
 * polygon is kept.
 */
static void compute_selected_polygons_from_edge_selection(const Mesh &mesh,
                                                          const VArray<bool> &edge_selection,
                                                          const bool invert,
                                                          Vector<int> &r_selected_poly_indices,
                                                          Vector<int> &r_loop_starts,
                                                          uint *r_num_selected_polys,
                                                          uint *r_num_selected_loops)
{
  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  uint num_selected_loops = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];

    bool all_edges_in_selection = true;
    Span<MLoop> loops_src(&mesh.mloop[poly_src.loopstart], poly_src.totloop);
    for (const MLoop &loop : loops_src) {
      if (edge_selection[loop.e] == invert) {
        all_edges_in_selection = false;
        break;
      }
    }

    if (all_edges_in_selection) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(num_selected_loops);
      num_selected_loops += poly_src.totloop;
    }
  }

  *r_num_selected_polys = r_selected_poly_indices.size();
  *r_num_selected_loops = num_selected_loops;
}

/**
 * Checks for every vertex if it is in `vertex_selection`. The polygons and edges are kept if all
 * vertices of that polygon or edge are in the selection.
 */
static void compute_selected_mesh_data_from_vertex_selection(const Mesh &mesh,
                                                             const VArray<bool> &vertex_selection,
                                                             const bool invert,
                                                             MutableSpan<int> r_vertex_map,
                                                             MutableSpan<int> r_edge_map,
                                                             Vector<int> &r_selected_poly_indices,
                                                             Vector<int> &r_loop_starts,
                                                             uint *r_num_selected_vertices,
                                                             uint *r_num_selected_edges,
                                                             uint *r_num_selected_polys,
                                                             uint *r_num_selected_loops)
{
  compute_selected_vertices_from_vertex_selection(
      vertex_selection, invert, r_vertex_map, r_num_selected_vertices);

  compute_selected_edges_from_vertex_selection(
      mesh, vertex_selection, invert, r_edge_map, r_num_selected_edges);

  compute_selected_polygons_from_vertex_selection(mesh,
                                                  vertex_selection,
                                                  invert,
                                                  r_selected_poly_indices,
                                                  r_loop_starts,
                                                  r_num_selected_polys,
                                                  r_num_selected_loops);
}

/**
 * Checks for every edge if it is in `edge_selection`. If it is, the vertices belonging to
 * that edge are kept as well. The polygons are kept if all edges are in the selection.
 */
static void compute_selected_mesh_data_from_edge_selection(const Mesh &mesh,
                                                           const VArray<bool> &edge_selection,
                                                           const bool invert,
                                                           MutableSpan<int> r_vertex_map,
                                                           MutableSpan<int> r_edge_map,
                                                           Vector<int> &r_selected_poly_indices,
                                                           Vector<int> &r_loop_starts,
                                                           uint *r_num_selected_vertices,
                                                           uint *r_num_selected_edges,
                                                           uint *r_num_selected_polys,
                                                           uint *r_num_selected_loops)
{
  r_vertex_map.fill(-1);
  compute_selected_vertices_and_edges_from_edge_selection(mesh,
                                                          edge_selection,
                                                          invert,
                                                          r_vertex_map,
                                                          r_edge_map,
                                                          r_num_selected_vertices,
                                                          r_num_selected_edges);
  compute_selected_polygons_from_edge_selection(mesh,
                                                edge_selection,
                                                invert,
                                                r_selected_poly_indices,
                                                r_loop_starts,
                                                r_num_selected_polys,
                                                r_num_selected_loops);
}

/**
 * Checks for every polygon if it is in `poly_selection`. If it is, the edges and vertices
 * belonging to that polygon are kept as well.
 */
static void compute_selected_mesh_data_from_poly_selection(const Mesh &mesh,
                                                           const VArray<bool> &poly_selection,
                                                           const bool invert,
                                                           MutableSpan<int> r_vertex_map,
                                                           MutableSpan<int> r_edge_map,
                                                           Vector<int> &r_selected_poly_indices,
                                                           Vector<int> &r_loop_starts,
                                                           uint *r_num_selected_vertices,
                                                           uint *r_num_selected_edges,
                                                           uint *r_num_selected_polys,
                                                           uint *r_num_selected_loops)
{
  BLI_assert(mesh.totpoly == poly_selection.size());
  BLI_assert(mesh.totedge == r_edge_map.size());
  r_vertex_map.fill(-1);
  r_edge_map.fill(-1);

  r_selected_poly_indices.reserve(mesh.totpoly);
  r_loop_starts.reserve(mesh.totloop);

  uint num_selected_loops = 0;
  uint num_selected_vertices = 0;
  uint num_selected_edges = 0;
  for (const int i : IndexRange(mesh.totpoly)) {
    const MPoly &poly_src = mesh.mpoly[i];
    /* We keep this one. */
    if (poly_selection[i] != invert) {
      r_selected_poly_indices.append_unchecked(i);
      r_loop_starts.append_unchecked(num_selected_loops);
      num_selected_loops += poly_src.totloop;

      /* Add the vertices and the edges. */
      Span<MLoop> loops_src(&mesh.mloop[poly_src.loopstart], poly_src.totloop);
      for (const MLoop &loop : loops_src) {
        /* Check first if it has not yet been added. */
        if (r_vertex_map[loop.v] == -1) {
          r_vertex_map[loop.v] = num_selected_vertices;
          num_selected_vertices++;
        }
        if (r_edge_map[loop.e] == -1) {
          r_edge_map[loop.e] = num_selected_edges;
          num_selected_edges++;
        }
      }
    }
  }
  *r_num_selected_vertices = num_selected_vertices;
  *r_num_selected_edges = num_selected_edges;
  *r_num_selected_polys = r_selected_poly_indices.size();
  *r_num_selected_loops = num_selected_loops;
}

using FillMapsFunction = void (*)(const Mesh &mesh,
                                  const VArray<bool> &selection,
                                  const bool invert,
                                  MutableSpan<int> r_vertex_map,
                                  MutableSpan<int> r_edge_map,
                                  Vector<int> &r_selected_poly_indices,
                                  Vector<int> &r_loop_starts,
                                  uint *r_num_selected_vertices,
                                  uint *r_num_selected_edges,
                                  uint *r_num_selected_polys,
                                  uint *r_num_selected_loops);

/**
 * Delete the parts of the mesh that are in the selection. The `fill_maps_function`
 * depends on the selection type: vertices, edges or faces.
 */
static Mesh *delete_mesh_selection(const Mesh &mesh_in,
                                   const VArray<bool> &selection,
                                   const bool invert,
                                   FillMapsFunction fill_maps_function)
{
  Array<int> vertex_map(mesh_in.totvert);
  uint num_selected_vertices;

  Array<int> edge_map(mesh_in.totedge);
  uint num_selected_edges;

  Vector<int> selected_poly_indices;
  Vector<int> new_loop_starts;
  uint num_selected_polys;
  uint num_selected_loops;

  /* Fill all the maps based on the selection. We delete everything
   * in the selection instead of keeping it, so we need to invert it. */
  fill_maps_function(mesh_in,
                     selection,
                     !invert,
                     vertex_map,
                     edge_map,
                     selected_poly_indices,
                     new_loop_starts,
                     &num_selected_vertices,
                     &num_selected_edges,
                     &num_selected_polys,
                     &num_selected_loops);

  Mesh *result = BKE_mesh_new_nomain_from_template(&mesh_in,
                                                   num_selected_vertices,
                                                   num_selected_edges,
                                                   0,
                                                   num_selected_loops,
                                                   num_selected_polys);

  /* Copy the selected parts of the mesh over to the new mesh. */
  copy_masked_vertices_to_new_mesh(mesh_in, *result, vertex_map);
  copy_masked_edges_to_new_mesh(mesh_in, *result, vertex_map, edge_map);
  copy_masked_polys_to_new_mesh(
      mesh_in, *result, vertex_map, edge_map, selected_poly_indices, new_loop_starts);
  BKE_mesh_calc_edges_loose(result);
  /* Tag to recalculate normals later. */
  BKE_mesh_normals_tag_dirty(result);

  return result;
}

static AttributeDomain get_mesh_selection_domain(MeshComponent &component, const StringRef name)
{
  std::optional<AttributeMetaData> selection_attribute = component.attribute_get_meta_data(name);
  if (!selection_attribute) {
    /* The node will not do anything in this case, but this function must return something. */
    return ATTR_DOMAIN_POINT;
  }

  /* Corners can't be deleted separately, so interpolate corner attributes
   * to the face domain. Note that this choice is somewhat arbitrary. */
  if (selection_attribute->domain == ATTR_DOMAIN_CORNER) {
    return ATTR_DOMAIN_FACE;
  }

  return selection_attribute->domain;
}

static void delete_mesh_selection(MeshComponent &component,
                                  const Mesh &mesh_in,
                                  const StringRef selection_name,
                                  const bool invert)
{
  /* Figure out the best domain to use. */
  const AttributeDomain selection_domain = get_mesh_selection_domain(component, selection_name);

  /* This already checks if the attribute exists, and displays a warning in that case. */
  GVArray_Typed<bool> selection = component.attribute_get_for_read<bool>(
      selection_name, selection_domain, false);

  /* Check if there is anything to delete. */
  bool delete_nothing = true;
  for (const int i : selection.index_range()) {
    if (selection[i] != invert) {
      delete_nothing = false;
      break;
    }
  }
  if (delete_nothing) {
    return;
  }

  Mesh *mesh_out;
  switch (selection_domain) {
    case ATTR_DOMAIN_POINT:
      mesh_out = delete_mesh_selection(
          mesh_in, selection, invert, compute_selected_mesh_data_from_vertex_selection);
      break;
    case ATTR_DOMAIN_EDGE:
      mesh_out = delete_mesh_selection(
          mesh_in, selection, invert, compute_selected_mesh_data_from_edge_selection);
      break;
    case ATTR_DOMAIN_FACE:
      mesh_out = delete_mesh_selection(
          mesh_in, selection, invert, compute_selected_mesh_data_from_poly_selection);
      break;
    default:
      BLI_assert_unreachable();
      mesh_out = nullptr;
      break;
  }
  component.replace(mesh_out);
}

static void geo_node_delete_geometry_exec(GeoNodeExecParams params)
{
  GeometrySet geometry_set = params.extract_input<GeometrySet>("Geometry");
  geometry_set = bke::geometry_set_realize_instances(geometry_set);

  const bool invert = params.extract_input<bool>("Invert");
  const std::string selection_name = params.extract_input<std::string>("Selection");
  if (selection_name.empty()) {
    params.set_output("Geometry", std::move(geometry_set));
    return;
  }

  GeometrySet out_set(geometry_set);
  if (geometry_set.has<PointCloudComponent>()) {
    delete_point_cloud_selection(*geometry_set.get_component_for_read<PointCloudComponent>(),
                                 out_set.get_component_for_write<PointCloudComponent>(),
                                 selection_name,
                                 invert);
  }
  if (geometry_set.has<MeshComponent>()) {
    delete_mesh_selection(out_set.get_component_for_write<MeshComponent>(),
                          *geometry_set.get_mesh_for_read(),
                          selection_name,
                          invert);
  }
  if (geometry_set.has<CurveComponent>()) {
    delete_curve_selection(*geometry_set.get_component_for_read<CurveComponent>(),
                           out_set.get_component_for_write<CurveComponent>(),
                           selection_name,
                           invert);
  }

  params.set_output("Geometry", std::move(out_set));
}

}  // namespace blender::nodes

void register_node_type_geo_legacy_delete_geometry()
{
  static bNodeType ntype;

  geo_node_type_base(
      &ntype, GEO_NODE_LEGACY_DELETE_GEOMETRY, "Delete Geometry", NODE_CLASS_GEOMETRY, 0);

  ntype.declare = blender::nodes::geo_node_delete_geometry_declare;
  ntype.geometry_node_execute = blender::nodes::geo_node_delete_geometry_exec;
  nodeRegisterType(&ntype);
}
