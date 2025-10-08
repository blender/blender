/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include <algorithm>
#include <numeric>

#include "BLI_array.hh"
#include "BLI_math_base.h"
#include "BLI_ordered_edge.hh"
#include "BLI_span.hh"

#include "BKE_anonymous_attribute_id.hh"
#include "BKE_attribute.hh"
#include "BKE_attribute_math.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "BKE_geometry_compare.hh"

#include "DNA_curve_types.h"
#include "DNA_lattice_types.h"

namespace blender::bke::compare_geometry {

enum class GeoMismatch : int8_t {
  NumPoints,        /* The number of points is different. */
  NumEdges,         /* The number of edges is different. */
  NumCorners,       /* The number of corners is different. */
  NumFaces,         /* The number of faces is different. */
  NumCurves,        /* The number of curves is different. */
  PointAttributes,  /* Some values of the point attributes are different. */
  EdgeAttributes,   /* Some values of the edge attributes are different. */
  CornerAttributes, /* Some values of the corner attributes are different. */
  FaceAttributes,   /* Some values of the face attributes are different. */
  CurveAttributes,  /* Some values of the curve attributes are different. */
  EdgeTopology,     /* The edge topology is different. */
  FaceTopology,     /* The face topology is different. */
  CurveTopology,    /* The curve topology is different. */
  Attributes,       /* The sets of attribute ids are different. */
  AttributeTypes,   /* Some attributes with the same name have different types. */
  Indices,          /* The geometries are the same up to a change of indices. */
};

const char *mismatch_to_string(const GeoMismatch &mismatch)
{
  switch (mismatch) {
    case GeoMismatch::NumPoints:
      return "The number of points is different";
    case GeoMismatch::NumEdges:
      return "The number of edges is different";
    case GeoMismatch::NumCorners:
      return "The number of corners is different";
    case GeoMismatch::NumFaces:
      return "The number of faces is different";
    case GeoMismatch::NumCurves:
      return "The number of curves is different";
    case GeoMismatch::PointAttributes:
      return "Some values of the point attributes are different";
    case GeoMismatch::EdgeAttributes:
      return "Some values of the edge attributes are different";
    case GeoMismatch::CornerAttributes:
      return "Some values of the corner attributes are different";
    case GeoMismatch::FaceAttributes:
      return "Some values of the face attributes are different";
    case GeoMismatch::CurveAttributes:
      return "Some values of the curve attributes are different";
    case GeoMismatch::EdgeTopology:
      return "The edge topology is different";
    case GeoMismatch::FaceTopology:
      return "The face topology is different";
    case GeoMismatch::CurveTopology:
      return "The curve topology is different";
    case GeoMismatch::Attributes:
      return "The sets of attribute ids are different";
    case GeoMismatch::AttributeTypes:
      return "Some attributes with the same name have different types";
    case GeoMismatch::Indices:
      return "The geometries are the same up to a change of indices";
  }
  BLI_assert_unreachable();
  return "";
}

class IndexMapping {
 private:
  void calculate_inverse_map(const Span<int> map, MutableSpan<int> inverted_map)
  {
    for (const int i : map.index_range()) {
      inverted_map[map[i]] = i;
    }
  }

 public:
  Array<int> from_sorted1;
  Array<int> from_sorted2;
  Array<int> to_sorted1;
  Array<int> to_sorted2;
  Array<int> set_ids;
  Array<int> set_sizes;

  IndexMapping(const int64_t domain_size)
  {
    to_sorted1 = Array<int>(domain_size);
    to_sorted2 = Array<int>(domain_size);
    from_sorted1 = Array<int>(domain_size);
    from_sorted2 = Array<int>(domain_size);
    set_ids = Array<int>(domain_size);
    set_sizes = Array<int>(domain_size);
    std::iota(from_sorted1.begin(), from_sorted1.end(), 0);
    std::iota(from_sorted2.begin(), from_sorted2.end(), 0);
    std::iota(to_sorted1.begin(), to_sorted1.end(), 0);
    std::iota(to_sorted2.begin(), to_sorted2.end(), 0);
    set_ids.fill(0);
    set_sizes.fill(set_ids.size());
  }

  /**
   * Update the "to_sorted" maps by inverting the "from_sorted" maps.
   */
  void recalculate_inverse_maps()
  {
    calculate_inverse_map(from_sorted1, to_sorted1);
    calculate_inverse_map(from_sorted2, to_sorted2);
  }
};

/**
 * Sort the indices using the values. For vectors of floats, the sorting happens based on the given
 * component.
 */
template<typename T>
static void sort_indices(MutableSpan<int> indices, const Span<T> values, const int component_i)
{
  /* We need to have an appropriate comparison function, depending on the type. */
  std::stable_sort(indices.begin(), indices.end(), [&](int i1, int i2) {
    const T value1 = values[i1];
    const T value2 = values[i2];
    if constexpr (is_same_any_v<T, int, float, bool, int8_t, OrderedEdge>) {
      /* These types are already comparable. */
      return value1 < value2;
    }
    if constexpr (is_same_any_v<T, float2, float3, ColorGeometry4f>) {
      return value1[component_i] < value2[component_i];
    }
    if constexpr (std::is_same_v<T, math::Quaternion>) {
      const float4 value1_quat = float4(value1);
      const float4 value2_quat = float4(value2);
      return value1_quat[component_i] < value2_quat[component_i];
    }
    if constexpr (std::is_same_v<T, float4x4>) {
      return value1.base_ptr()[component_i] < value2.base_ptr()[component_i];
    }
    if constexpr (std::is_same_v<T, int2>) {
      for (int i = 0; i < 2; i++) {
        if (value1[i] != value2[i]) {
          return value1[i] < value2[i];
        }
      }
      return false;
    }
    if constexpr (std::is_same_v<T, ColorGeometry4b>) {
      for (int i = 0; i < 4; i++) {
        if (value1[i] != value2[i]) {
          return value1[i] < value2[i];
        }
      }
      return false;
    }

    BLI_assert_unreachable();
    return false;
  });
}

/**
 * Sort the indices using the set ids of the values.
 */
static void sort_indices_with_id_maps(MutableSpan<int> indices,
                                      const Span<int> values,
                                      const Span<int> values_to_sorted,
                                      const Span<int> set_ids)
{
  std::stable_sort(indices.begin(), indices.end(), [&](int i1, int i2) {
    return set_ids[values_to_sorted[values[i1]]] < set_ids[values_to_sorted[values[i2]]];
  });
}

/* Sort the elements in each set based on the attribute values. */
template<typename T>
static void sort_per_set_based_on_attributes(const Span<int> set_sizes,
                                             MutableSpan<int> sorted_to_domain1,
                                             MutableSpan<int> sorted_to_domain2,
                                             const Span<T> values1,
                                             const Span<T> values2,
                                             const int component_i)
{
  int i = 0;
  while (i < set_sizes.size()) {
    const int set_size = set_sizes[i];
    if (set_size == 1) {
      /* No need to sort anymore. */
      i += 1;
      continue;
    }

    sort_indices(sorted_to_domain1.slice(IndexRange(i, set_size)), values1, component_i);
    sort_indices(sorted_to_domain2.slice(IndexRange(i, set_size)), values2, component_i);
    i += set_size;
  }
}

/* Sort the elements in each set based on the set ids of the values. */
static void sort_per_set_with_id_maps(const Span<int> set_sizes,
                                      const Span<int> values1,
                                      const Span<int> values2,
                                      const Span<int> values1_to_sorted,
                                      const Span<int> values2_to_sorted,
                                      const Span<int> value_set_ids,
                                      MutableSpan<int> sorted_to_domain1,
                                      MutableSpan<int> sorted_to_domain2)
{
  int i = 0;
  while (i < sorted_to_domain1.size()) {
    const int set_size = set_sizes[i];
    if (set_size == 1) {
      /* No need to sort anymore. */
      i += 1;
      continue;
    }

    sort_indices_with_id_maps(sorted_to_domain1.slice(IndexRange(i, set_size)),
                              values1,
                              values1_to_sorted,
                              value_set_ids);
    sort_indices_with_id_maps(sorted_to_domain2.slice(IndexRange(i, set_size)),
                              values2,
                              values2_to_sorted,
                              value_set_ids);
    i += set_size;
  }
}

/**
 * Checks if the two values are different. For float types, the equality is checked based on a
 * threshold.
 */
template<typename T>
static bool values_different(const T value1,
                             const T value2,
                             const float threshold,
                             const int component_i)
{
  if constexpr (is_same_any_v<T, int, short2, int2, bool, int8_t, OrderedEdge, ColorGeometry4b>) {
    /* These types already have a good implementation. */
    return value1 != value2;
  }
  /* The other types are based on floats. */
  if constexpr (std::is_same_v<T, float>) {
    return compare_threshold_relative(value1, value2, threshold);
  }

  /* GCC 15.x triggers an array-bounds warning unless `component_i` is assumed to be in range. */
#if (defined(__GNUC__) && (__GNUC__ >= 15) && !defined(__clang__))
#  define ASSERT_AND_ASSUME(expr) \
    BLI_assert(expr); \
    [[assume(expr)]];
#else
#  define ASSERT_AND_ASSUME(expr) BLI_assert(expr);
#endif

  if constexpr (is_same_any_v<T, float2>) {
    ASSERT_AND_ASSUME(component_i >= 0 && component_i < 2);
    return compare_threshold_relative(value1[component_i], value2[component_i], threshold);
  }
  if constexpr (is_same_any_v<T, float3>) {
    ASSERT_AND_ASSUME(component_i >= 0 && component_i < 3);
    return compare_threshold_relative(value1[component_i], value2[component_i], threshold);
  }
  if constexpr (is_same_any_v<T, ColorGeometry4f>) {
    ASSERT_AND_ASSUME(component_i >= 0 && component_i < 4);
    return compare_threshold_relative(value1[component_i], value2[component_i], threshold);
  }
  if constexpr (std::is_same_v<T, math::Quaternion>) {
    ASSERT_AND_ASSUME(component_i >= 0 && component_i < 4);
    const float4 value1_f = float4(value1);
    const float4 value2_f = float4(value2);
    return compare_threshold_relative(value1_f[component_i], value2_f[component_i], threshold);
  }
  if constexpr (std::is_same_v<T, float4x4>) {
    ASSERT_AND_ASSUME(component_i >= 0 && component_i < 4);
    return compare_threshold_relative(
        value1.base_ptr()[component_i], value2.base_ptr()[component_i], threshold);
  }

#undef ASSERT_AND_ASSUME

  BLI_assert_unreachable();
}

/**
 * Split the sets into smaller sets based on the sorted attribute values.
 *
 * \returns false if the attributes don't line up.
 */
template<typename T>
static bool update_set_ids(MutableSpan<int> set_ids,
                           const Span<T> values1,
                           const Span<T> values2,
                           const Span<int> sorted_to_values1,
                           const Span<int> sorted_to_values2,
                           const float threshold,
                           const int component_i)
{
  /* Due to the way the sorting works, there could be a slightly bigger difference. */
  const float value_threshold = 5 * threshold;
  if (set_ids.is_empty()) {
    return true;
  }
  T previous = values1[0];
  int set_id = 0;
  for (const int i : values1.index_range()) {
    const T value1 = values1[sorted_to_values1[i]];
    const T value2 = values2[sorted_to_values2[i]];
    if (values_different(value1, value2, value_threshold, component_i)) {
      /* They should be the same after sorting. */
      return false;
    }
    if ((values_different(previous, value1, value_threshold, component_i) &&
         values_different(previous, value2, value_threshold, component_i)) ||
        set_ids[i] == i)
    {
      /* Different value, or this was already a different set. */
      set_id = i;
      previous = value1;
    }
    set_ids[i] = set_id;
  }

  return true;
}

/**
 * Split the sets into smaller sets based on the set ids of the sorted values.
 *
 * \returns false if the attributes don't line up.
 */
static bool update_set_ids_with_id_maps(MutableSpan<int> set_ids,
                                        const Span<int> domain_to_values1,
                                        const Span<int> domain_to_values2,
                                        const Span<int> values1_to_sorted,
                                        const Span<int> values2_to_sorted,
                                        const Span<int> value_set_ids,
                                        const Span<int> sorted_to_domain1,
                                        const Span<int> sorted_to_domain2)
{
  if (set_ids.is_empty()) {
    return true;
  }
  int previous = value_set_ids[values1_to_sorted[domain_to_values1[sorted_to_domain1[0]]]];
  int set_id = 0;
  for (const int i : sorted_to_domain1.index_range()) {
    const int value_id1 =
        value_set_ids[values1_to_sorted[domain_to_values1[sorted_to_domain1[i]]]];
    const int value_id2 =
        value_set_ids[values2_to_sorted[domain_to_values2[sorted_to_domain2[i]]]];
    if (value_id1 != value_id2) {
      /* They should be the same after sorting. */
      return false;
    }
    if (value_id1 != previous || set_ids[i] == i) {
      /* Different value, or this was already a different set. */
      set_id = i;
      previous = value_id1;
    }
    set_ids[i] = set_id;
  }

  return true;
}

/**
 * Update set sizes, using the updated set ids.
 */
static void update_set_sizes(const Span<int> set_ids, MutableSpan<int> set_sizes)
{
  int i = set_ids.size() - 1;
  while (i >= 0) {
    /* The id of a set is the index of its first element, so the size can be computed as the index
     * of the last element minus the id (== index of first element) + 1. */
    int set_size = i - set_ids[i] + 1;
    /* Set the set size for each element in the set. */
    for (int k = i - set_size + 1; k <= i; k++) {
      set_sizes[k] = set_size;
    }
    i -= set_size;
  }
}

static void edges_from_vert_sets(const Span<int2> edges,
                                 const Span<int> verts_to_sorted,
                                 const Span<int> vert_set_ids,
                                 MutableSpan<OrderedEdge> r_edges)
{
  for (const int i : r_edges.index_range()) {
    const int2 e = edges[i];
    r_edges[i] = OrderedEdge(vert_set_ids[verts_to_sorted[e.x]],
                             vert_set_ids[verts_to_sorted[e.y]]);
  }
}

/**
 * Sort the edges based on the sorted vertex set ids.
 */
static bool sort_edges(const Span<int2> edges1,
                       const Span<int2> edges2,
                       const IndexMapping &verts,
                       IndexMapping &edges)
{
  /* Need `NoInitialization()` because OrderedEdge is not default constructible. */
  Array<OrderedEdge> ordered_edges1(edges1.size(), NoInitialization());
  Array<OrderedEdge> ordered_edges2(edges2.size(), NoInitialization());
  edges_from_vert_sets(edges1, verts.to_sorted1, verts.set_ids, ordered_edges1);
  edges_from_vert_sets(edges2, verts.to_sorted2, verts.set_ids, ordered_edges2);
  sort_per_set_based_on_attributes(edges.set_sizes,
                                   edges.from_sorted1,
                                   edges.from_sorted2,
                                   ordered_edges1.as_span(),
                                   ordered_edges2.as_span(),
                                   0);
  const bool edges_match = update_set_ids(edges.set_ids,
                                          ordered_edges1.as_span(),
                                          ordered_edges2.as_span(),
                                          edges.from_sorted1,
                                          edges.from_sorted2,
                                          0,
                                          0);
  if (!edges_match) {
    return false;
  }
  update_set_sizes(edges.set_ids, edges.set_sizes);
  return true;
}

/**
 * Sort the corners based on the sorted vertex/edge set ids.
 */
static bool sort_corners_based_on_domain(const Span<int> corner_domain1,
                                         const Span<int> corner_domain2,
                                         const IndexMapping &domain,
                                         IndexMapping &corners)
{
  sort_per_set_with_id_maps(corners.set_sizes,
                            corner_domain1,
                            corner_domain2,
                            domain.to_sorted1,
                            domain.to_sorted2,
                            domain.set_ids,
                            corners.from_sorted1,
                            corners.from_sorted2);
  const bool corners_line_up = update_set_ids_with_id_maps(corners.set_ids,
                                                           corner_domain1,
                                                           corner_domain2,
                                                           domain.to_sorted1,
                                                           domain.to_sorted2,
                                                           domain.set_ids,
                                                           corners.from_sorted1,
                                                           corners.from_sorted2);
  if (!corners_line_up) {
    return false;
  }
  update_set_sizes(corners.set_ids, corners.set_sizes);
  return true;
}

static void calc_smallest_corner_ids(const Span<int> face_offsets,
                                     const Span<int> corners_to_sorted,
                                     const Span<int> corner_set_ids,
                                     MutableSpan<int> smallest_corner_ids)
{
  for (const int face_i : smallest_corner_ids.index_range()) {
    const int face_start = face_offsets[face_i];
    const int face_end = face_offsets[face_i + 1];
    int smallest = corner_set_ids[corners_to_sorted[face_start]];
    const IndexRange corners = IndexRange(face_start, face_end - face_start);
    for (const int corner_i : corners.drop_front(1)) {
      const int corner_id = corner_set_ids[corners_to_sorted[corner_i]];
      smallest = std::min(corner_id, smallest);
    }
    smallest_corner_ids[face_i] = smallest;
  }
}

/**
 * Sort the faces using the sorted corner set ids.
 */
static bool sort_faces_based_on_corners(const IndexMapping &corners,
                                        const Span<int> face_offsets1,
                                        const Span<int> face_offsets2,
                                        IndexMapping &faces)
{
  /* The smallest corner set id, per face. */
  Array<int> smallest_corner_ids1(faces.from_sorted1.size());
  Array<int> smallest_corner_ids2(faces.from_sorted2.size());
  calc_smallest_corner_ids(
      face_offsets1, corners.to_sorted1, corners.set_ids, smallest_corner_ids1);
  calc_smallest_corner_ids(
      face_offsets2, corners.to_sorted2, corners.set_ids, smallest_corner_ids2);
  sort_per_set_based_on_attributes(faces.set_sizes,
                                   faces.from_sorted1,
                                   faces.from_sorted2,
                                   smallest_corner_ids1.as_span(),
                                   smallest_corner_ids2.as_span(),
                                   0);
  const bool faces_line_up = update_set_ids(faces.set_ids,
                                            smallest_corner_ids1.as_span(),
                                            smallest_corner_ids2.as_span(),
                                            faces.from_sorted1,
                                            faces.from_sorted2,
                                            0,
                                            0);
  if (!faces_line_up) {
    return false;
  }
  update_set_sizes(faces.set_ids, faces.set_sizes);
  return true;
}

/**
 * The UV selection & pin layers are ignored in the comparisons because
 * the original flags they replace were ignored as well. Because of the
 * lazy creation of these layers it would need careful handling of the
 * test files to compare these layers. For now it has been decided to
 * skip them.
 */
static bool ignored_attribute(const StringRef id)
{
  return attribute_name_is_anonymous(id) || id.startswith(".pn.") ||
         ELEM(id, ".uv_select_vert", ".uv_select_edge", ".uv_select_face");
}

/**
 * Verify that both geometries have the same attributes:
 * - Same names
 * - Same domains
 * - Same types
 */
static std::optional<GeoMismatch> verify_attributes_compatible(
    const AttributeAccessor &attributes1, const AttributeAccessor &attributes2)
{
  Set<StringRefNull> attribute_ids1 = attributes1.all_ids();
  Set<StringRefNull> attribute_ids2 = attributes2.all_ids();
  attribute_ids1.remove_if(ignored_attribute);
  attribute_ids2.remove_if(ignored_attribute);

  if (attribute_ids1 != attribute_ids2) {
    /* Disabled for now due to tests not being up to date. */
    // return GeoMismatch::Attributes;
  }
  for (const StringRef id : attribute_ids1) {
    GAttributeReader reader1 = attributes1.lookup(id);
    GAttributeReader reader2 = attributes2.lookup(id);
    if (!reader1 || !reader2) {
      /* Necessary because of previous disabled return. */
      continue;
    }
    if (reader1.domain != reader2.domain || reader1.varray.type() != reader2.varray.type()) {
      return GeoMismatch::AttributeTypes;
    }
  }
  return std::nullopt;
}

/**
 * Sort the domain using all the attributes on that domain except the ones in excluded_attributes
 *
 * \returns A mismatch if one of the attributes has different values between the two geometries.
 */
static std::optional<GeoMismatch> sort_domain_using_attributes(
    const AttributeAccessor &attributes1,
    const AttributeAccessor &attributes2,
    const AttrDomain domain,
    const Span<StringRef> excluded_attributes,
    IndexMapping &maps,
    const float threshold)
{

  /* We only need the ids from one geometry, since we know they have the same attributes. */
  Set<StringRefNull> attribute_ids = attributes1.all_ids();
  for (const StringRef name : excluded_attributes) {
    attribute_ids.remove_as(name);
  }
  attribute_ids.remove_if(ignored_attribute);

  for (const StringRef id : attribute_ids) {
    if (!attributes2.contains(id)) {
      /* Only needed right now since some test meshes don't have the same attributes. */
      return GeoMismatch::Attributes;
    }
    GAttributeReader reader1 = attributes1.lookup(id);
    GAttributeReader reader2 = attributes2.lookup(id);

    if (reader1.domain != domain) {
      /* We only look at attributes of the given domain. */
      continue;
    }

    std::optional<GeoMismatch> mismatch = {};

    attribute_math::convert_to_static_type(reader1.varray.type(), [&](auto dummy) {
      using T = decltype(dummy);
      const VArraySpan<T> values1 = reader1.varray.typed<T>();
      const VArraySpan<T> values2 = reader2.varray.typed<T>();

      /* Because sorting of float vectors is not very stable, we do a separate sort per component,
       * re-computing the set ids each time. */
      int num_loops = 1;
      if constexpr (std::is_same_v<T, float2>) {
        num_loops = 2;
      }
      else if constexpr (std::is_same_v<T, float3>) {
        num_loops = 3;
      }
      else if constexpr (is_same_any_v<T, math::Quaternion, ColorGeometry4f>) {
        num_loops = 4;
      }
      else if constexpr (is_same_any_v<T, float4x4>) {
        num_loops = 16;
      }
      for (const int component_i : IndexRange(num_loops)) {
        sort_per_set_based_on_attributes(
            maps.set_sizes, maps.from_sorted1, maps.from_sorted2, values1, values2, component_i);
        const bool attributes_line_up = update_set_ids(maps.set_ids,
                                                       values1,
                                                       values2,
                                                       maps.from_sorted1,
                                                       maps.from_sorted2,
                                                       threshold,
                                                       component_i);
        if (!attributes_line_up) {
          switch (domain) {
            case AttrDomain::Point:
              mismatch = GeoMismatch::PointAttributes;
              return;
            case AttrDomain::Edge:
              mismatch = GeoMismatch::EdgeAttributes;
              return;
            case AttrDomain::Corner:
              mismatch = GeoMismatch::CornerAttributes;
              return;
            case AttrDomain::Face:
              mismatch = GeoMismatch::FaceAttributes;
              return;
            case AttrDomain::Curve:
              mismatch = GeoMismatch::CurveAttributes;
              return;
            default:
              BLI_assert_unreachable();
              break;
          }
          return;
        }
        update_set_sizes(maps.set_ids, maps.set_sizes);
      }
    });

    if (mismatch) {
      return mismatch;
    }
  }
  return std::nullopt;
}

/* When all checks are done, it's possible that some set sizes are still not one e.g, when you have
 * two loose verts at the same position they are indistinguishable. This makes all the set ID's one
 * by choosing a match. If possible, the match is chosen such that they have the same unsorted
 * index.
 */
static void make_set_sizes_one(IndexMapping &indices)
{
  for (const int sorted_i : indices.set_sizes.index_range()) {
    if (indices.set_sizes[sorted_i] == 1) {
      continue;
    }
    int match = sorted_i;
    for (const int other_index :
         IndexRange(indices.set_ids[sorted_i], indices.set_sizes[sorted_i]))
    {
      if (indices.from_sorted1[sorted_i] == indices.from_sorted2[other_index]) {
        match = other_index;
        break;
      }
    }
    std::swap(indices.from_sorted2[sorted_i], indices.from_sorted2[match]);
    for (const int other_set_i :
         IndexRange(indices.set_ids[sorted_i], indices.set_sizes[sorted_i]))
    {
      /* New first element, since this one is now in a new set. */
      indices.set_ids[other_set_i] = sorted_i + 1;
      indices.set_sizes[other_set_i] -= 1;
    }
    indices.set_ids[sorted_i] = sorted_i;
    indices.set_sizes[sorted_i] = 1;
  }
}

static bool all_set_sizes_one(const Span<int> set_sizes)
{
  for (const int size : set_sizes) {
    if (size != 1) {
      return false;
    }
  }
  return true;
}

/**
 * Tries to construct a (bijective) mapping from the vertices of the first mesh to the
 * vertices of the second mesh, such that:
 * - Edge topology is preserved under this mapping, i.e. if v_1 and v_2 are on an edge in mesh1
 *   then `f(v_1)` and `f(v_2)` are on an edge in mesh2.
 * - Face topology is preserved under this mapping, i.e. if v_1, ..., v_n form a face in mesh1,
 *   then `f(v_1)`, ..., `f(v_n)` form a face in mesh2.
 * - The mapping preserves all vertex attributes, i.e. if `attr` is some vertex attribute on mesh1,
 *   then for every vertex v of mesh1, `attr(v) = attr(f(v))`.
 *
 * \returns the type of mismatch that occurred if the mapping couldn't be constructed.
 */
static std::optional<GeoMismatch> construct_vert_mapping(const Mesh &mesh1,
                                                         const Mesh &mesh2,
                                                         IndexMapping &verts,
                                                         IndexMapping &edges)
{
  if (all_set_sizes_one(verts.set_sizes)) {
    /* The vertices are already in one-to-one correspondence. */
    return std::nullopt;
  }

  /* Since we are not yet able to distinguish all vertices based on their attributes alone, we
   * need to use the edge topology. */
  Array<int> vert_to_edge_offsets1;
  Array<int> vert_to_edge_indices1;
  const GroupedSpan<int> vert_to_edge_map1 = mesh::build_vert_to_edge_map(
      mesh1.edges(), mesh1.verts_num, vert_to_edge_offsets1, vert_to_edge_indices1);
  Array<int> vert_to_edge_offsets2;
  Array<int> vert_to_edge_indices2;
  const GroupedSpan<int> vert_to_edge_map2 = mesh::build_vert_to_edge_map(
      mesh2.edges(), mesh2.verts_num, vert_to_edge_offsets2, vert_to_edge_indices2);

  for (const int sorted_i : verts.from_sorted1.index_range()) {
    const int vert1 = verts.from_sorted1[sorted_i];
    Vector<int> matching_verts;
    const Span<int> edges1 = vert_to_edge_map1[vert1];
    /* Try to find all matching vertices. We know that it will be in the same vertex set, if it
     * exists. */
    for (const int index_in_set : IndexRange(verts.set_sizes[sorted_i])) {
      /* The set id is the index of its first element. */
      const int vert2 = verts.from_sorted2[verts.set_ids[sorted_i] + index_in_set];
      const Span<int> edges2 = vert_to_edge_map2[vert2];
      if (edges1.size() != edges2.size()) {
        continue;
      }
      bool vert_matches = true;
      for (const int edge1 : edges1) {
        bool found_matching_edge = false;
        for (const int edge2 : edges2) {
          if (edges.set_ids[edges.to_sorted1[edge1]] == edges.set_ids[edges.to_sorted2[edge2]]) {
            found_matching_edge = true;
            break;
          }
        }
        if (!found_matching_edge) {
          vert_matches = false;
          break;
        }
      }
      if (vert_matches) {
        matching_verts.append(index_in_set);
      }
    }

    if (matching_verts.is_empty()) {
      return GeoMismatch::EdgeTopology;
    }

    /* Update the maps. */

    /* In principle, we should make sure that there is exactly one matching vertex. If the mesh is
     * of good enough quality, that will always be the case. In other cases we just assume that any
     * choice will be valid. Otherwise, the logic becomes a lot more difficult. Because we want to
     * test for mesh equality as well, we try to pick the matching vert with the same index. */
    int index_in_set = matching_verts.first();
    for (const int other_index_in_set : matching_verts) {
      const int other_sorted_index = verts.set_ids[sorted_i] + other_index_in_set;
      if (verts.from_sorted1[sorted_i] == verts.from_sorted2[other_sorted_index]) {
        index_in_set = other_index_in_set;
        break;
      }
    }
    std::swap(verts.from_sorted2[sorted_i],
              verts.from_sorted2[verts.set_ids[sorted_i] + index_in_set]);
    for (const int other_set_i : IndexRange(verts.set_ids[sorted_i], verts.set_sizes[sorted_i])) {
      /* New first element, since this one is now in a new set. */
      verts.set_ids[other_set_i] = sorted_i + 1;
      verts.set_sizes[other_set_i] -= 1;
    }
    verts.set_ids[sorted_i] = sorted_i;
    verts.set_sizes[sorted_i] = 1;
  }

  BLI_assert(all_set_sizes_one(verts.set_sizes));

  verts.recalculate_inverse_maps();

  /* The bijective mapping is now given by composing `verts.to_sorted1` with `verts.from_sorted2`,
   * or vice versa. Since we don't actually need the mapping (we just care that it exists), we
   * don't construct it here. */

  return std::nullopt;
}

std::optional<GeoMismatch> compare_meshes(const Mesh &mesh1,
                                          const Mesh &mesh2,
                                          const float threshold)
{

  /* These will be assumed implicitly later on. */
  if (mesh1.verts_num != mesh2.verts_num) {
    return GeoMismatch::NumPoints;
  }
  if (mesh1.edges_num != mesh2.edges_num) {
    return GeoMismatch::NumEdges;
  }
  if (mesh1.corners_num != mesh2.corners_num) {
    return GeoMismatch::NumCorners;
  }
  if (mesh1.faces_num != mesh2.faces_num) {
    return GeoMismatch::NumFaces;
  }

  std::optional<GeoMismatch> mismatch = {};

  const AttributeAccessor mesh1_attributes = mesh1.attributes();
  const AttributeAccessor mesh2_attributes = mesh2.attributes();
  mismatch = verify_attributes_compatible(mesh1_attributes, mesh2_attributes);
  if (mismatch) {
    return mismatch;
  }

  IndexMapping verts(mesh1.verts_num);
  mismatch = sort_domain_using_attributes(
      mesh1_attributes, mesh2_attributes, AttrDomain::Point, {}, verts, threshold);
  if (mismatch) {
    return mismatch;
  }

  /* We need the maps going the other way as well. */
  verts.recalculate_inverse_maps();

  IndexMapping edges(mesh1.edges_num);
  if (!sort_edges(mesh1.edges(), mesh2.edges(), verts, edges)) {
    return GeoMismatch::EdgeTopology;
  }

  mismatch = sort_domain_using_attributes(
      mesh1_attributes, mesh2_attributes, AttrDomain::Edge, {".edge_verts"}, edges, threshold);
  if (mismatch) {
    return mismatch;
  };

  /* We need the maps going the other way as well. */
  edges.recalculate_inverse_maps();

  IndexMapping corners(mesh1.corners_num);
  if (!sort_corners_based_on_domain(mesh1.corner_verts(), mesh2.corner_verts(), verts, corners)) {
    return GeoMismatch::FaceTopology;
  }

  if (!sort_corners_based_on_domain(mesh1.corner_edges(), mesh2.corner_edges(), edges, corners)) {
    return GeoMismatch::FaceTopology;
  }

  mismatch = sort_domain_using_attributes(mesh1_attributes,
                                          mesh2_attributes,
                                          AttrDomain::Corner,
                                          {".corner_vert", ".corner_edge"},
                                          corners,
                                          threshold);
  if (mismatch) {
    return mismatch;
  };

  /* We need the maps going the other way as well. */
  corners.recalculate_inverse_maps();

  IndexMapping faces(mesh1.faces_num);
  if (!sort_faces_based_on_corners(corners, mesh1.face_offsets(), mesh2.face_offsets(), faces)) {
    return GeoMismatch::FaceTopology;
  }

  mismatch = sort_domain_using_attributes(
      mesh1_attributes, mesh2_attributes, AttrDomain::Face, {}, faces, threshold);
  if (mismatch) {
    return mismatch;
  };

  mismatch = construct_vert_mapping(mesh1, mesh2, verts, edges);
  if (mismatch) {
    return mismatch;
  }

  /* Now we double check that the other topology maps agree with this vertex mapping. */

  if (!sort_edges(mesh1.edges(), mesh2.edges(), verts, edges)) {
    return GeoMismatch::EdgeTopology;
  }

  make_set_sizes_one(edges);

  edges.recalculate_inverse_maps();

  if (!sort_corners_based_on_domain(mesh1.corner_verts(), mesh2.corner_verts(), verts, corners)) {
    return GeoMismatch::FaceTopology;
  }

  if (!sort_corners_based_on_domain(mesh1.corner_edges(), mesh2.corner_edges(), edges, corners)) {
    return GeoMismatch::FaceTopology;
  }

  make_set_sizes_one(corners);

  corners.recalculate_inverse_maps();

  if (!sort_faces_based_on_corners(corners, mesh1.face_offsets(), mesh2.face_offsets(), faces)) {
    return GeoMismatch::FaceTopology;
  }

  make_set_sizes_one(faces);

  /* The meshes are isomorphic, we now just need to determine if they are equal i.e., the indices
   * are the same. */
  for (const int sorted_i : verts.from_sorted1.index_range()) {
    if (verts.from_sorted1[sorted_i] != verts.from_sorted2[sorted_i]) {
      return GeoMismatch::Indices;
    }
  }
  /* Skip the test for edges, since a lot of tests actually have different edge indices.
   *TODO: remove this once those tests have been updated. */
  for (const int sorted_i : corners.from_sorted1.index_range()) {
    if (corners.from_sorted1[sorted_i] != corners.from_sorted2[sorted_i]) {
      return GeoMismatch::Indices;
    }
  }
  for (const int sorted_i : faces.from_sorted1.index_range()) {
    if (faces.from_sorted1[sorted_i] != faces.from_sorted2[sorted_i]) {
      return GeoMismatch::Indices;
    }
  }

  /* No mismatches found. */
  return std::nullopt;
}

/**
 * Sort curves based on their sizes.
 */
static bool sort_curves(const OffsetIndices<int> offset_indices1,
                        const OffsetIndices<int> offset_indices2,
                        IndexMapping &curves)
{
  Array<int> curve_point_counts1(offset_indices1.size());
  Array<int> curve_point_counts2(offset_indices2.size());
  offset_indices::copy_group_sizes(
      offset_indices1, offset_indices1.index_range(), curve_point_counts1.as_mutable_span());
  offset_indices::copy_group_sizes(
      offset_indices2, offset_indices2.index_range(), curve_point_counts2.as_mutable_span());
  sort_per_set_based_on_attributes(curves.set_sizes,
                                   curves.from_sorted1,
                                   curves.from_sorted2,
                                   curve_point_counts1.as_span(),
                                   curve_point_counts2.as_span(),
                                   0);
  const bool curves_sizes_match = update_set_ids(curves.set_ids,
                                                 curve_point_counts1.as_span(),
                                                 curve_point_counts2.as_span(),
                                                 curves.from_sorted1,
                                                 curves.from_sorted2,
                                                 0,
                                                 0);
  if (!curves_sizes_match) {
    return false;
  }
  update_set_sizes(curves.set_ids, curves.set_sizes);
  return true;
}

std::optional<GeoMismatch> compare_curves(const CurvesGeometry &curves1,
                                          const CurvesGeometry &curves2,
                                          const float threshold)
{
  /* These will be assumed implicitly later on. */
  if (curves1.points_num() != curves2.points_num()) {
    return GeoMismatch::NumPoints;
  }
  if (curves1.curves_num() != curves2.curves_num()) {
    return GeoMismatch::NumCurves;
  }

  std::optional<GeoMismatch> mismatch = {};

  const AttributeAccessor curves1_attributes = curves1.attributes();
  const AttributeAccessor curves2_attributes = curves2.attributes();
  mismatch = verify_attributes_compatible(curves1_attributes, curves2_attributes);
  if (mismatch) {
    return mismatch;
  }

  IndexMapping points(curves1.points_num());
  mismatch = sort_domain_using_attributes(
      curves1_attributes, curves2_attributes, AttrDomain::Point, {}, points, threshold);
  if (mismatch) {
    return mismatch;
  }

  IndexMapping curves(curves1.curves_num());
  if (!sort_curves(curves1.offsets(), curves2.offsets(), curves)) {
    return GeoMismatch::CurveTopology;
  }

  mismatch = sort_domain_using_attributes(
      curves1_attributes, curves2_attributes, AttrDomain::Curve, {}, curves, threshold);
  if (mismatch) {
    return mismatch;
  }

  for (const int sorted_i : points.from_sorted1.index_range()) {
    if (points.from_sorted1[sorted_i] != points.from_sorted2[sorted_i]) {
      return GeoMismatch::Indices;
    }
  }

  for (const int sorted_i : curves.from_sorted1.index_range()) {
    if (curves.from_sorted1[sorted_i] != curves.from_sorted2[sorted_i]) {
      return GeoMismatch::Indices;
    }
  }

  /* No mismatches found. */
  return std::nullopt;
}

std::optional<GeoMismatch> compare_lattices(const Lattice &lattice1,
                                            const Lattice &lattice2,
                                            float threshold)
{
  if (lattice1.pntsu != lattice2.pntsu) {
    return GeoMismatch::NumPoints;
  }
  if (lattice1.pntsv != lattice2.pntsv) {
    return GeoMismatch::NumPoints;
  }
  if (lattice1.pntsw != lattice2.pntsw) {
    return GeoMismatch::NumPoints;
  }

  const int num_points = lattice1.pntsu * lattice1.pntsv * lattice1.pntsw;
  const Span<BPoint> bpoints1 = {lattice1.def, num_points};
  const Span<BPoint> bpoints2 = {lattice2.def, num_points};
  for (const int i : IndexRange(num_points)) {
    const float3 co1 = bpoints1[i].vec;
    const float3 co2 = bpoints2[i].vec;
    for (const int component : IndexRange(3)) {
      if (values_different(co1, co2, threshold, component)) {
        return GeoMismatch::PointAttributes;
      }
    }
  }

  /* No mismatches found. */
  return std::nullopt;
}

}  // namespace blender::bke::compare_geometry
