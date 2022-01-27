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
#include "BKE_material.h"
#include "BKE_mesh.h"
#include "BKE_spline.hh"

#include "BKE_curve_to_mesh.hh"

using blender::fn::GMutableSpan;
using blender::fn::GSpan;

namespace blender::bke {

/** Information about the creation of one curve spline and profile spline combination. */
struct ResultInfo {
  const Spline &spline;
  const Spline &profile;
  int vert_offset;
  int edge_offset;
  int loop_offset;
  int poly_offset;
  int spline_vert_len;
  int spline_edge_len;
  int profile_vert_len;
  int profile_edge_len;
};

static void vert_extrude_to_mesh_data(const Spline &spline,
                                      const float3 profile_vert,
                                      MutableSpan<MVert> r_verts,
                                      MutableSpan<MEdge> r_edges,
                                      const int vert_offset,
                                      const int edge_offset)
{
  const int eval_size = spline.evaluated_points_size();
  for (const int i : IndexRange(eval_size - 1)) {
    MEdge &edge = r_edges[edge_offset + i];
    edge.v1 = vert_offset + i;
    edge.v2 = vert_offset + i + 1;
    edge.flag = ME_LOOSEEDGE;
  }

  if (spline.is_cyclic() && spline.evaluated_edges_size() > 1) {
    MEdge &edge = r_edges[edge_offset + spline.evaluated_edges_size() - 1];
    edge.v1 = vert_offset + eval_size - 1;
    edge.v2 = vert_offset;
    edge.flag = ME_LOOSEEDGE;
  }

  Span<float3> positions = spline.evaluated_positions();
  Span<float3> tangents = spline.evaluated_tangents();
  Span<float3> normals = spline.evaluated_normals();
  VArray<float> radii = spline.interpolate_to_evaluated(spline.radii());
  for (const int i : IndexRange(eval_size)) {
    float4x4 point_matrix = float4x4::from_normalized_axis_data(
        positions[i], normals[i], tangents[i]);
    point_matrix.apply_scale(radii[i]);

    MVert &vert = r_verts[vert_offset + i];
    copy_v3_v3(vert.co, point_matrix * profile_vert);
  }
}

static void mark_edges_sharp(MutableSpan<MEdge> edges)
{
  for (MEdge &edge : edges) {
    edge.flag |= ME_SHARP;
  }
}

static void spline_extrude_to_mesh_data(const ResultInfo &info,
                                        const bool fill_caps,
                                        MutableSpan<MVert> r_verts,
                                        MutableSpan<MEdge> r_edges,
                                        MutableSpan<MLoop> r_loops,
                                        MutableSpan<MPoly> r_polys)
{
  const Spline &spline = info.spline;
  const Spline &profile = info.profile;
  if (info.profile_vert_len == 1) {
    vert_extrude_to_mesh_data(spline,
                              profile.evaluated_positions()[0],
                              r_verts,
                              r_edges,
                              info.vert_offset,
                              info.edge_offset);
    return;
  }

  /* Add the edges running along the length of the curve, starting at each profile vertex. */
  const int spline_edges_start = info.edge_offset;
  for (const int i_profile : IndexRange(info.profile_vert_len)) {
    const int profile_edge_offset = spline_edges_start + i_profile * info.spline_edge_len;
    for (const int i_ring : IndexRange(info.spline_edge_len)) {
      const int i_next_ring = (i_ring == info.spline_vert_len - 1) ? 0 : i_ring + 1;

      const int ring_vert_offset = info.vert_offset + info.profile_vert_len * i_ring;
      const int next_ring_vert_offset = info.vert_offset + info.profile_vert_len * i_next_ring;

      MEdge &edge = r_edges[profile_edge_offset + i_ring];
      edge.v1 = ring_vert_offset + i_profile;
      edge.v2 = next_ring_vert_offset + i_profile;
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
  }

  /* Add the edges running along each profile ring. */
  const int profile_edges_start = spline_edges_start +
                                  info.profile_vert_len * info.spline_edge_len;
  for (const int i_ring : IndexRange(info.spline_vert_len)) {
    const int ring_vert_offset = info.vert_offset + info.profile_vert_len * i_ring;

    const int ring_edge_offset = profile_edges_start + i_ring * info.profile_edge_len;
    for (const int i_profile : IndexRange(info.profile_edge_len)) {
      const int i_next_profile = (i_profile == info.profile_vert_len - 1) ? 0 : i_profile + 1;

      MEdge &edge = r_edges[ring_edge_offset + i_profile];
      edge.v1 = ring_vert_offset + i_profile;
      edge.v2 = ring_vert_offset + i_next_profile;
      edge.flag = ME_EDGEDRAW | ME_EDGERENDER;
    }
  }

  /* Calculate poly and corner indices. */
  for (const int i_ring : IndexRange(info.spline_edge_len)) {
    const int i_next_ring = (i_ring == info.spline_vert_len - 1) ? 0 : i_ring + 1;

    const int ring_vert_offset = info.vert_offset + info.profile_vert_len * i_ring;
    const int next_ring_vert_offset = info.vert_offset + info.profile_vert_len * i_next_ring;

    const int ring_edge_start = profile_edges_start + info.profile_edge_len * i_ring;
    const int next_ring_edge_offset = profile_edges_start + info.profile_edge_len * i_next_ring;

    const int ring_poly_offset = info.poly_offset + i_ring * info.profile_edge_len;
    const int ring_loop_offset = info.loop_offset + i_ring * info.profile_edge_len * 4;

    for (const int i_profile : IndexRange(info.profile_edge_len)) {
      const int ring_segment_loop_offset = ring_loop_offset + i_profile * 4;
      const int i_next_profile = (i_profile == info.profile_vert_len - 1) ? 0 : i_profile + 1;

      const int spline_edge_start = spline_edges_start + info.spline_edge_len * i_profile;
      const int next_spline_edge_start = spline_edges_start +
                                         info.spline_edge_len * i_next_profile;

      MPoly &poly = r_polys[ring_poly_offset + i_profile];
      poly.loopstart = ring_segment_loop_offset;
      poly.totloop = 4;
      poly.flag = ME_SMOOTH;

      MLoop &loop_a = r_loops[ring_segment_loop_offset];
      loop_a.v = ring_vert_offset + i_profile;
      loop_a.e = ring_edge_start + i_profile;
      MLoop &loop_b = r_loops[ring_segment_loop_offset + 1];
      loop_b.v = ring_vert_offset + i_next_profile;
      loop_b.e = next_spline_edge_start + i_ring;
      MLoop &loop_c = r_loops[ring_segment_loop_offset + 2];
      loop_c.v = next_ring_vert_offset + i_next_profile;
      loop_c.e = next_ring_edge_offset + i_profile;
      MLoop &loop_d = r_loops[ring_segment_loop_offset + 3];
      loop_d.v = next_ring_vert_offset + i_profile;
      loop_d.e = spline_edge_start + i_ring;
    }
  }

  if (fill_caps && profile.is_cyclic()) {
    const int poly_size = info.spline_edge_len * info.profile_edge_len;
    const int cap_loop_offset = info.loop_offset + poly_size * 4;
    const int cap_poly_offset = info.poly_offset + poly_size;

    MPoly &poly_start = r_polys[cap_poly_offset];
    poly_start.loopstart = cap_loop_offset;
    poly_start.totloop = info.profile_edge_len;
    MPoly &poly_end = r_polys[cap_poly_offset + 1];
    poly_end.loopstart = cap_loop_offset + info.profile_edge_len;
    poly_end.totloop = info.profile_edge_len;

    const int last_ring_index = info.spline_vert_len - 1;
    const int last_ring_vert_offset = info.vert_offset + info.profile_vert_len * last_ring_index;
    const int last_ring_edge_offset = profile_edges_start +
                                      info.profile_edge_len * last_ring_index;

    for (const int i : IndexRange(info.profile_edge_len)) {
      const int i_inv = info.profile_edge_len - i - 1;
      MLoop &loop_start = r_loops[cap_loop_offset + i];
      loop_start.v = info.vert_offset + i_inv;
      loop_start.e = profile_edges_start + ((i == (info.profile_edge_len - 1)) ?
                                                (info.profile_edge_len - 1) :
                                                (i_inv - 1));
      MLoop &loop_end = r_loops[cap_loop_offset + info.profile_edge_len + i];
      loop_end.v = last_ring_vert_offset + i;
      loop_end.e = last_ring_edge_offset + i;
    }

    mark_edges_sharp(r_edges.slice(profile_edges_start, info.profile_edge_len));
    mark_edges_sharp(r_edges.slice(last_ring_edge_offset, info.profile_edge_len));
  }

  /* Calculate the positions of each profile ring profile along the spline. */
  Span<float3> positions = spline.evaluated_positions();
  Span<float3> tangents = spline.evaluated_tangents();
  Span<float3> normals = spline.evaluated_normals();
  Span<float3> profile_positions = profile.evaluated_positions();

  VArray<float> radii = spline.interpolate_to_evaluated(spline.radii());
  for (const int i_ring : IndexRange(info.spline_vert_len)) {
    float4x4 point_matrix = float4x4::from_normalized_axis_data(
        positions[i_ring], normals[i_ring], tangents[i_ring]);
    point_matrix.apply_scale(radii[i_ring]);

    const int ring_vert_start = info.vert_offset + i_ring * info.profile_vert_len;
    for (const int i_profile : IndexRange(info.profile_vert_len)) {
      MVert &vert = r_verts[ring_vert_start + i_profile];
      copy_v3_v3(vert.co, point_matrix * profile_positions[i_profile]);
    }
  }

  /* Mark edge loops from sharp vector control points sharp. */
  if (profile.type() == Spline::Type::Bezier) {
    const BezierSpline &bezier_spline = static_cast<const BezierSpline &>(profile);
    Span<int> control_point_offsets = bezier_spline.control_point_offsets();
    for (const int i : IndexRange(bezier_spline.size())) {
      if (bezier_spline.point_is_sharp(i)) {
        mark_edges_sharp(
            r_edges.slice(spline_edges_start + info.spline_edge_len * control_point_offsets[i],
                          info.spline_edge_len));
      }
    }
  }
}

static inline int spline_extrude_vert_size(const Spline &curve, const Spline &profile)
{
  return curve.evaluated_points_size() * profile.evaluated_points_size();
}

static inline int spline_extrude_edge_size(const Spline &curve, const Spline &profile)
{
  /* Add the ring edges, with one ring for every curve vertex, and the edge loops
   * that run along the length of the curve, starting on the first profile. */
  return curve.evaluated_points_size() * profile.evaluated_edges_size() +
         curve.evaluated_edges_size() * profile.evaluated_points_size();
}

static inline int spline_extrude_loop_size(const Spline &curve,
                                           const Spline &profile,
                                           const bool fill_caps)
{
  const int tube = curve.evaluated_edges_size() * profile.evaluated_edges_size() * 4;
  const int caps = (fill_caps && profile.is_cyclic()) ? profile.evaluated_edges_size() * 2 : 0;
  return tube + caps;
}

static inline int spline_extrude_poly_size(const Spline &curve,
                                           const Spline &profile,
                                           const bool fill_caps)
{
  const int tube = curve.evaluated_edges_size() * profile.evaluated_edges_size();
  const int caps = (fill_caps && profile.is_cyclic()) ? 2 : 0;
  return tube + caps;
}

struct ResultOffsets {
  Array<int> vert;
  Array<int> edge;
  Array<int> loop;
  Array<int> poly;
};
static ResultOffsets calculate_result_offsets(Span<SplinePtr> profiles,
                                              Span<SplinePtr> curves,
                                              const bool fill_caps)
{
  const int total = profiles.size() * curves.size();
  Array<int> vert(total + 1);
  Array<int> edge(total + 1);
  Array<int> loop(total + 1);
  Array<int> poly(total + 1);

  int mesh_index = 0;
  int vert_offset = 0;
  int edge_offset = 0;
  int loop_offset = 0;
  int poly_offset = 0;
  for (const int i_spline : curves.index_range()) {
    for (const int i_profile : profiles.index_range()) {
      vert[mesh_index] = vert_offset;
      edge[mesh_index] = edge_offset;
      loop[mesh_index] = loop_offset;
      poly[mesh_index] = poly_offset;
      vert_offset += spline_extrude_vert_size(*curves[i_spline], *profiles[i_profile]);
      edge_offset += spline_extrude_edge_size(*curves[i_spline], *profiles[i_profile]);
      loop_offset += spline_extrude_loop_size(*curves[i_spline], *profiles[i_profile], fill_caps);
      poly_offset += spline_extrude_poly_size(*curves[i_spline], *profiles[i_profile], fill_caps);
      mesh_index++;
    }
  }
  vert.last() = vert_offset;
  edge.last() = edge_offset;
  loop.last() = loop_offset;
  poly.last() = poly_offset;

  return {std::move(vert), std::move(edge), std::move(loop), std::move(poly)};
}

static AttributeDomain get_result_attribute_domain(const MeshComponent &component,
                                                   const AttributeIDRef &attribute_id)
{
  /* Only use a different domain if it is builtin and must only exist on one domain. */
  if (!component.attribute_is_builtin(attribute_id)) {
    return ATTR_DOMAIN_POINT;
  }

  std::optional<AttributeMetaData> meta_data = component.attribute_get_meta_data(attribute_id);
  if (!meta_data) {
    /* This function has to return something in this case, but it shouldn't be used,
     * so return an output that will assert later if the code attempts to handle it. */
    return ATTR_DOMAIN_AUTO;
  }

  return meta_data->domain;
}

/**
 * The data stored in the attribute and its domain from #OutputAttribute, to avoid calling
 * `as_span()` for every single profile and curve spline combination, and for readability.
 */
struct ResultAttributeData {
  GMutableSpan data;
  AttributeDomain domain;
};

static std::optional<ResultAttributeData> create_attribute_and_get_span(
    MeshComponent &component,
    const AttributeIDRef &attribute_id,
    AttributeMetaData meta_data,
    Vector<OutputAttribute> &r_attributes)
{
  const AttributeDomain domain = get_result_attribute_domain(component, attribute_id);
  OutputAttribute attribute = component.attribute_try_get_for_output_only(
      attribute_id, domain, meta_data.data_type);
  if (!attribute) {
    return std::nullopt;
  }

  GMutableSpan span = attribute.as_span();
  r_attributes.append(std::move(attribute));
  return std::make_optional<ResultAttributeData>({span, domain});
}

/**
 * Store the references to the attribute data from the curve and profile inputs. Here we rely on
 * the invariants of the storage of curve attributes, that the order will be consistent between
 * splines, and all splines will have the same attributes.
 */
struct ResultAttributes {
  /**
   * Result attributes on the mesh corresponding to each attribute on the curve input, in the same
   * order. The data is optional only in case the attribute does not exist on the mesh for some
   * reason, like "shade_smooth" when the result has no faces.
   */
  Vector<std::optional<ResultAttributeData>> curve_point_attributes;
  Vector<std::optional<ResultAttributeData>> curve_spline_attributes;

  /**
   * Result attributes corresponding the attributes on the profile input, in the same order. The
   * attributes are optional in case the attribute names correspond to a names used by the curve
   * input, in which case the curve input attributes take precedence.
   */
  Vector<std::optional<ResultAttributeData>> profile_point_attributes;
  Vector<std::optional<ResultAttributeData>> profile_spline_attributes;

  /**
   * Because some builtin attributes are not stored contiguously, and the curve inputs might have
   * attributes with those names, it's necessary to keep OutputAttributes around to give access to
   * the result data in a contiguous array.
   */
  Vector<OutputAttribute> attributes;
};
static ResultAttributes create_result_attributes(const CurveEval &curve,
                                                 const CurveEval &profile,
                                                 MeshComponent &mesh_component)
{
  Set<AttributeIDRef> curve_attributes;

  /* In order to prefer attributes on the main curve input when there are name collisions, first
   * check the attributes on the curve, then add attributes on the profile that are not also on the
   * main curve input. */
  ResultAttributes result;
  curve.splines().first()->attributes.foreach_attribute(
      [&](const AttributeIDRef &id, const AttributeMetaData &meta_data) {
        curve_attributes.add_new(id);
        result.curve_point_attributes.append(
            create_attribute_and_get_span(mesh_component, id, meta_data, result.attributes));
        return true;
      },
      ATTR_DOMAIN_POINT);
  curve.attributes.foreach_attribute(
      [&](const AttributeIDRef &id, const AttributeMetaData &meta_data) {
        curve_attributes.add_new(id);
        result.curve_spline_attributes.append(
            create_attribute_and_get_span(mesh_component, id, meta_data, result.attributes));
        return true;
      },
      ATTR_DOMAIN_CURVE);
  profile.splines().first()->attributes.foreach_attribute(
      [&](const AttributeIDRef &id, const AttributeMetaData &meta_data) {
        if (curve_attributes.contains(id)) {
          result.profile_point_attributes.append({});
        }
        else {
          result.profile_point_attributes.append(
              create_attribute_and_get_span(mesh_component, id, meta_data, result.attributes));
        }
        return true;
      },
      ATTR_DOMAIN_POINT);
  profile.attributes.foreach_attribute(
      [&](const AttributeIDRef &id, const AttributeMetaData &meta_data) {
        if (curve_attributes.contains(id)) {
          result.profile_spline_attributes.append({});
        }
        else {
          result.profile_spline_attributes.append(
              create_attribute_and_get_span(mesh_component, id, meta_data, result.attributes));
        }
        return true;
      },
      ATTR_DOMAIN_CURVE);

  return result;
}

template<typename T>
static void copy_curve_point_data_to_mesh_verts(const Span<T> src,
                                                const ResultInfo &info,
                                                MutableSpan<T> dst)
{
  for (const int i_ring : IndexRange(info.spline_vert_len)) {
    const int ring_vert_start = info.vert_offset + i_ring * info.profile_vert_len;
    dst.slice(ring_vert_start, info.profile_vert_len).fill(src[i_ring]);
  }
}

template<typename T>
static void copy_curve_point_data_to_mesh_edges(const Span<T> src,
                                                const ResultInfo &info,
                                                MutableSpan<T> dst)
{
  const int edges_start = info.edge_offset + info.profile_vert_len * info.spline_edge_len;
  for (const int i_ring : IndexRange(info.spline_vert_len)) {
    const int ring_edge_start = edges_start + info.profile_edge_len * i_ring;
    dst.slice(ring_edge_start, info.profile_edge_len).fill(src[i_ring]);
  }
}

template<typename T>
static void copy_curve_point_data_to_mesh_faces(const Span<T> src,
                                                const ResultInfo &info,
                                                MutableSpan<T> dst)
{
  for (const int i_ring : IndexRange(info.spline_edge_len)) {
    const int ring_face_start = info.poly_offset + info.profile_edge_len * i_ring;
    dst.slice(ring_face_start, info.profile_edge_len).fill(src[i_ring]);
  }
}

static void copy_curve_point_attribute_to_mesh(const GSpan src,
                                               const ResultInfo &info,
                                               ResultAttributeData &dst)
{
  GVArray interpolated_gvarray = info.spline.interpolate_to_evaluated(src);
  GSpan interpolated = interpolated_gvarray.get_internal_span();

  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    switch (dst.domain) {
      case ATTR_DOMAIN_POINT:
        copy_curve_point_data_to_mesh_verts(interpolated.typed<T>(), info, dst.data.typed<T>());
        break;
      case ATTR_DOMAIN_EDGE:
        copy_curve_point_data_to_mesh_edges(interpolated.typed<T>(), info, dst.data.typed<T>());
        break;
      case ATTR_DOMAIN_FACE:
        copy_curve_point_data_to_mesh_faces(interpolated.typed<T>(), info, dst.data.typed<T>());
        break;
      case ATTR_DOMAIN_CORNER:
        /* Unsupported for now, since there are no builtin attributes to convert into. */
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  });
}

template<typename T>
static void copy_profile_point_data_to_mesh_verts(const Span<T> src,
                                                  const ResultInfo &info,
                                                  MutableSpan<T> dst)
{
  for (const int i_ring : IndexRange(info.spline_vert_len)) {
    const int profile_vert_start = info.vert_offset + i_ring * info.profile_vert_len;
    for (const int i_profile : IndexRange(info.profile_vert_len)) {
      dst[profile_vert_start + i_profile] = src[i_profile];
    }
  }
}

template<typename T>
static void copy_profile_point_data_to_mesh_edges(const Span<T> src,
                                                  const ResultInfo &info,
                                                  MutableSpan<T> dst)
{
  for (const int i_profile : IndexRange(info.profile_vert_len)) {
    const int profile_edge_offset = info.edge_offset + i_profile * info.spline_edge_len;
    dst.slice(profile_edge_offset, info.spline_edge_len).fill(src[i_profile]);
  }
}

template<typename T>
static void copy_profile_point_data_to_mesh_faces(const Span<T> src,
                                                  const ResultInfo &info,
                                                  MutableSpan<T> dst)
{
  for (const int i_ring : IndexRange(info.spline_edge_len)) {
    const int profile_face_start = info.poly_offset + i_ring * info.profile_edge_len;
    for (const int i_profile : IndexRange(info.profile_edge_len)) {
      dst[profile_face_start + i_profile] = src[i_profile];
    }
  }
}

static void copy_profile_point_attribute_to_mesh(const GSpan src,
                                                 const ResultInfo &info,
                                                 ResultAttributeData &dst)
{
  GVArray interpolated_gvarray = info.profile.interpolate_to_evaluated(src);
  GSpan interpolated = interpolated_gvarray.get_internal_span();

  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    switch (dst.domain) {
      case ATTR_DOMAIN_POINT:
        copy_profile_point_data_to_mesh_verts(interpolated.typed<T>(), info, dst.data.typed<T>());
        break;
      case ATTR_DOMAIN_EDGE:
        copy_profile_point_data_to_mesh_edges(interpolated.typed<T>(), info, dst.data.typed<T>());
        break;
      case ATTR_DOMAIN_FACE:
        copy_profile_point_data_to_mesh_faces(interpolated.typed<T>(), info, dst.data.typed<T>());
        break;
      case ATTR_DOMAIN_CORNER:
        /* Unsupported for now, since there are no builtin attributes to convert into. */
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  });
}

static void copy_point_domain_attributes_to_mesh(const ResultInfo &info,
                                                 ResultAttributes &attributes)
{
  if (!attributes.curve_point_attributes.is_empty()) {
    int i = 0;
    info.spline.attributes.foreach_attribute(
        [&](const AttributeIDRef &id, const AttributeMetaData &UNUSED(meta_data)) {
          if (attributes.curve_point_attributes[i]) {
            copy_curve_point_attribute_to_mesh(*info.spline.attributes.get_for_read(id),
                                               info,
                                               *attributes.curve_point_attributes[i]);
          }
          i++;
          return true;
        },
        ATTR_DOMAIN_POINT);
  }
  if (!attributes.profile_point_attributes.is_empty()) {
    int i = 0;
    info.profile.attributes.foreach_attribute(
        [&](const AttributeIDRef &id, const AttributeMetaData &UNUSED(meta_data)) {
          if (attributes.profile_point_attributes[i]) {
            copy_profile_point_attribute_to_mesh(*info.profile.attributes.get_for_read(id),
                                                 info,
                                                 *attributes.profile_point_attributes[i]);
          }
          i++;
          return true;
        },
        ATTR_DOMAIN_POINT);
  }
}

template<typename T>
static void copy_spline_data_to_mesh(Span<T> src, Span<int> offsets, MutableSpan<T> dst)
{
  for (const int i : IndexRange(src.size())) {
    dst.slice(offsets[i], offsets[i + 1] - offsets[i]).fill(src[i]);
  }
}

/**
 * Since the offsets for each combination of curve and profile spline are stored for every mesh
 * domain, and this just needs to fill the chunks corresponding to each combination, we can use
 * the same function for all mesh domains.
 */
static void copy_spline_attribute_to_mesh(const GSpan src,
                                          const ResultOffsets &offsets,
                                          ResultAttributeData &dst_attribute)
{
  attribute_math::convert_to_static_type(src.type(), [&](auto dummy) {
    using T = decltype(dummy);
    switch (dst_attribute.domain) {
      case ATTR_DOMAIN_POINT:
        copy_spline_data_to_mesh(src.typed<T>(), offsets.vert, dst_attribute.data.typed<T>());
        break;
      case ATTR_DOMAIN_EDGE:
        copy_spline_data_to_mesh(src.typed<T>(), offsets.edge, dst_attribute.data.typed<T>());
        break;
      case ATTR_DOMAIN_FACE:
        copy_spline_data_to_mesh(src.typed<T>(), offsets.poly, dst_attribute.data.typed<T>());
        break;
      case ATTR_DOMAIN_CORNER:
        copy_spline_data_to_mesh(src.typed<T>(), offsets.loop, dst_attribute.data.typed<T>());
        break;
      default:
        BLI_assert_unreachable();
        break;
    }
  });
}

static void copy_spline_domain_attributes_to_mesh(const CurveEval &curve,
                                                  const CurveEval &profile,
                                                  const ResultOffsets &offsets,
                                                  ResultAttributes &attributes)
{
  if (!attributes.curve_spline_attributes.is_empty()) {
    int i = 0;
    curve.attributes.foreach_attribute(
        [&](const AttributeIDRef &id, const AttributeMetaData &UNUSED(meta_data)) {
          if (attributes.curve_spline_attributes[i]) {
            copy_spline_attribute_to_mesh(*curve.attributes.get_for_read(id),
                                          offsets,
                                          *attributes.curve_spline_attributes[i]);
          }
          i++;
          return true;
        },
        ATTR_DOMAIN_CURVE);
  }
  if (!attributes.profile_spline_attributes.is_empty()) {
    int i = 0;
    profile.attributes.foreach_attribute(
        [&](const AttributeIDRef &id, const AttributeMetaData &UNUSED(meta_data)) {
          if (attributes.profile_spline_attributes[i]) {
            copy_spline_attribute_to_mesh(*profile.attributes.get_for_read(id),
                                          offsets,
                                          *attributes.profile_spline_attributes[i]);
          }
          i++;
          return true;
        },
        ATTR_DOMAIN_CURVE);
  }
}

Mesh *curve_to_mesh_sweep(const CurveEval &curve, const CurveEval &profile, const bool fill_caps)
{
  Span<SplinePtr> profiles = profile.splines();
  Span<SplinePtr> curves = curve.splines();

  const ResultOffsets offsets = calculate_result_offsets(profiles, curves, fill_caps);
  if (offsets.vert.last() == 0) {
    return nullptr;
  }

  Mesh *mesh = BKE_mesh_new_nomain(
      offsets.vert.last(), offsets.edge.last(), 0, offsets.loop.last(), offsets.poly.last());
  BKE_id_material_eval_ensure_default_slot(&mesh->id);
  mesh->flag |= ME_AUTOSMOOTH;
  mesh->smoothresh = DEG2RADF(180.0f);
  BKE_mesh_normals_tag_dirty(mesh);

  /* Create the mesh component for retrieving attributes at this scope, since output attributes
   * can keep a reference to the component for updating after retrieving write access. */
  MeshComponent mesh_component;
  mesh_component.replace(mesh, GeometryOwnershipType::Editable);
  ResultAttributes attributes = create_result_attributes(curve, profile, mesh_component);

  threading::parallel_for(curves.index_range(), 128, [&](IndexRange curves_range) {
    for (const int i_spline : curves_range) {
      const Spline &spline = *curves[i_spline];
      if (spline.evaluated_points_size() == 0) {
        continue;
      }
      const int spline_start_index = i_spline * profiles.size();
      threading::parallel_for(profiles.index_range(), 128, [&](IndexRange profiles_range) {
        for (const int i_profile : profiles_range) {
          const Spline &profile = *profiles[i_profile];
          const int i_mesh = spline_start_index + i_profile;
          ResultInfo info{
              spline,
              profile,
              offsets.vert[i_mesh],
              offsets.edge[i_mesh],
              offsets.loop[i_mesh],
              offsets.poly[i_mesh],
              spline.evaluated_points_size(),
              spline.evaluated_edges_size(),
              profile.evaluated_points_size(),
              profile.evaluated_edges_size(),
          };

          spline_extrude_to_mesh_data(info,
                                      fill_caps,
                                      {mesh->mvert, mesh->totvert},
                                      {mesh->medge, mesh->totedge},
                                      {mesh->mloop, mesh->totloop},
                                      {mesh->mpoly, mesh->totpoly});

          copy_point_domain_attributes_to_mesh(info, attributes);
        }
      });
    }
  });

  copy_spline_domain_attributes_to_mesh(curve, profile, offsets, attributes);

  for (OutputAttribute &output_attribute : attributes.attributes) {
    output_attribute.save();
  }

  return mesh;
}

static CurveEval get_curve_single_vert()
{
  CurveEval curve;
  std::unique_ptr<PolySpline> spline = std::make_unique<PolySpline>();
  spline->resize(1.0f);
  spline->positions().fill(float3(0));
  spline->radii().fill(1.0f);
  spline->tilts().fill(0.0f);
  curve.add_spline(std::move(spline));

  return curve;
}

Mesh *curve_to_wire_mesh(const CurveEval &curve)
{
  static const CurveEval vert_curve = get_curve_single_vert();
  return curve_to_mesh_sweep(curve, vert_curve, false);
}

}  // namespace blender::bke
