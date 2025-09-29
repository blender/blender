/* SPDX-FileCopyrightText: 2024 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_generic_virtual_array.hh"
#include "BLI_math_quaternion.hh"
#include "BLI_virtual_array.hh"

#include "BKE_attribute_math.hh"
#include "BKE_deform.hh"
#include "BKE_mesh.hh"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BLI_listbase.h"

#include "FN_multi_function_builder.hh"

#include "attribute_access_intern.hh"

namespace blender::bke {

template<typename T>
static void adapt_mesh_domain_corner_to_point_impl(const Mesh &mesh,
                                                   const VArray<T> &src,
                                                   MutableSpan<T> r_dst)
{
  BLI_assert(r_dst.size() == mesh.verts_num);
  const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
  const Span<int> corner_verts = mesh.corner_verts();
  const OffsetIndices<int> faces = mesh.faces();

  threading::parallel_for(vert_to_face_map.index_range(), 2048, [&](const IndexRange range) {
    for (const int64_t vert : range) {
      const Span<int> vert_faces = vert_to_face_map[vert];

      attribute_math::DefaultMixer<T> mixer({&r_dst[vert], 1});
      for (const int face : vert_faces) {
        const int corner = mesh::face_find_corner_from_vert(faces[face], corner_verts, int(vert));
        mixer.mix_in(0, src[corner]);
      }
      mixer.finalize();
    }
  });
}

/* A vertex is selected if all connected face corners were selected and it is not loose. */
template<>
void adapt_mesh_domain_corner_to_point_impl(const Mesh &mesh,
                                            const VArray<bool> &src,
                                            MutableSpan<bool> r_dst)
{
  BLI_assert(r_dst.size() == mesh.verts_num);
  const Span<int> corner_verts = mesh.corner_verts();

  r_dst.fill(true);
  threading::parallel_for(IndexRange(mesh.corners_num), 4096, [&](const IndexRange range) {
    for (const int corner : range) {
      const int vert = corner_verts[corner];
      if (!src[corner]) {
        r_dst[vert] = false;
      }
    }
  });

  /* Deselect loose vertices without corners that are still selected from the 'true' default. */
  const LooseVertCache &loose_verts = mesh.verts_no_face();
  if (loose_verts.count > 0) {
    const BitSpan bits = loose_verts.is_loose_bits;
    threading::parallel_for(bits.index_range(), 2048, [&](const IndexRange range) {
      for (const int vert_index : range) {
        if (bits[vert_index]) {
          r_dst[vert_index] = false;
        }
      }
    });
  }
}

static GVArray adapt_mesh_domain_corner_to_point(const Mesh &mesh, const GVArray &varray)
{
  GArray<> values(varray.type(), mesh.verts_num);
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      /* We compute all interpolated values at once, because for this interpolation, one has to
       * iterate over all loops anyway. */
      adapt_mesh_domain_corner_to_point_impl<T>(
          mesh, varray.typed<T>(), values.as_mutable_span().typed<T>());
    }
  });
  return GVArray::from_garray(std::move(values));
}

/**
 * Each corner's value is simply a copy of the value at its vertex.
 */
static GVArray adapt_mesh_domain_point_to_corner(const Mesh &mesh, const GVArray &varray)
{
  const Span<int> corner_verts = mesh.corner_verts();

  GVArray new_varray;
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    new_varray = VArray<T>::from_func(
        mesh.corners_num, [corner_verts, varray = varray.typed<T>()](const int64_t corner) {
          return varray[corner_verts[corner]];
        });
  });
  return new_varray;
}

static GVArray adapt_mesh_domain_corner_to_face(const Mesh &mesh, const GVArray &varray)
{
  const OffsetIndices faces = mesh.faces();

  GVArray new_varray;
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      if constexpr (std::is_same_v<T, bool>) {
        new_varray = VArray<T>::from_func(
            faces.size(), [faces, varray = varray.typed<bool>()](const int face_index) {
              /* A face is selected if all of its corners were selected. */
              for (const int corner : faces[face_index]) {
                if (!varray[corner]) {
                  return false;
                }
              }
              return true;
            });
      }
      else {
        new_varray = VArray<T>::from_func(
            faces.size(), [faces, varray = varray.typed<T>()](const int face_index) {
              T return_value;
              attribute_math::DefaultMixer<T> mixer({&return_value, 1});
              for (const int corner : faces[face_index]) {
                const T value = varray[corner];
                mixer.mix_in(0, value);
              }
              mixer.finalize();
              return return_value;
            });
      }
    }
  });
  return new_varray;
}

template<typename T>
static void adapt_mesh_domain_corner_to_edge_impl(const Mesh &mesh,
                                                  const VArray<T> &old_values,
                                                  MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.edges_num);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_edges = mesh.corner_edges();

  attribute_math::DefaultMixer<T> mixer(r_values);

  for (const int face_index : faces.index_range()) {
    const IndexRange face = faces[face_index];

    /* For every edge, mix values from the two adjacent corners (the current and next corner). */
    for (const int corner : face) {
      const int next_corner = mesh::face_corner_next(face, corner);
      const int edge_index = corner_edges[corner];
      mixer.mix_in(edge_index, old_values[corner]);
      mixer.mix_in(edge_index, old_values[next_corner]);
    }
  }

  mixer.finalize();
}

/* An edge is selected if all corners on adjacent faces were selected. */
template<>
void adapt_mesh_domain_corner_to_edge_impl(const Mesh &mesh,
                                           const VArray<bool> &old_values,
                                           MutableSpan<bool> r_values)
{
  BLI_assert(r_values.size() == mesh.edges_num);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_edges = mesh.corner_edges();

  r_values.fill(true);
  for (const int face_index : faces.index_range()) {
    const IndexRange face = faces[face_index];

    for (const int corner : face) {
      const int next_corner = mesh::face_corner_next(face, corner);
      const int edge_index = corner_edges[corner];
      if (!old_values[corner] || !old_values[next_corner]) {
        r_values[edge_index] = false;
      }
    }
  }

  const LooseEdgeCache &loose_edges = mesh.loose_edges();
  if (loose_edges.count > 0) {
    /* Deselect loose edges without corners that are still selected from the 'true' default. */
    threading::parallel_for(IndexRange(mesh.edges_num), 2048, [&](const IndexRange range) {
      for (const int edge_index : range) {
        if (loose_edges.is_loose_bits[edge_index]) {
          r_values[edge_index] = false;
        }
      }
    });
  }
}

static GVArray adapt_mesh_domain_corner_to_edge(const Mesh &mesh, const GVArray &varray)
{
  GArray<> values(varray.type(), mesh.edges_num);
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      adapt_mesh_domain_corner_to_edge_impl<T>(
          mesh, varray.typed<T>(), values.as_mutable_span().typed<T>());
    }
  });
  return GVArray::from_garray(std::move(values));
}

static GVArray adapt_mesh_domain_face_to_point(const Mesh &mesh, const GVArray &varray)
{
  GVArray new_varray;
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      VArray<T> src = varray.typed<T>();
      const GroupedSpan<int> vert_to_face_map = mesh.vert_to_face_map();
      if constexpr (std::is_same_v<T, bool>) {
        new_varray = VArray<T>::from_func(
            mesh.verts_num, [vert_to_face_map, src](const int point_i) {
              const Span<int> vert_faces = vert_to_face_map[point_i];
              /* A vertex is selected if any of the connected faces were selected. */
              return std::any_of(
                  vert_faces.begin(), vert_faces.end(), [&](const int face) { return src[face]; });
            });
      }
      else {
        new_varray = VArray<T>::from_func(
            mesh.verts_num, [vert_to_face_map, src](const int point_i) {
              const Span<int> vert_faces = vert_to_face_map[point_i];
              T return_value;
              attribute_math::DefaultMixer<T> mixer({&return_value, 1});
              for (const int face : vert_faces) {
                mixer.mix_in(0, src[face]);
              }
              mixer.finalize();
              return return_value;
            });
      }
    }
  });
  return new_varray;
}

/* Each corner's value is simply a copy of the value at its face. */
template<typename T>
void adapt_mesh_domain_face_to_corner_impl(const Mesh &mesh,
                                           const VArray<T> &old_values,
                                           MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.corners_num);
  const OffsetIndices faces = mesh.faces();

  threading::parallel_for(faces.index_range(), 1024, [&](const IndexRange range) {
    for (const int face_index : range) {
      MutableSpan<T> face_corner_values = r_values.slice(faces[face_index]);
      face_corner_values.fill(old_values[face_index]);
    }
  });
}

static GVArray adapt_mesh_domain_face_to_corner(const Mesh &mesh, const GVArray &varray)
{
  GArray<> values(varray.type(), mesh.corners_num);
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      adapt_mesh_domain_face_to_corner_impl<T>(
          mesh, varray.typed<T>(), values.as_mutable_span().typed<T>());
    }
  });
  return GVArray::from_garray(std::move(values));
}

template<typename T>
void adapt_mesh_domain_face_to_edge_impl(const Mesh &mesh,
                                         const VArray<T> &old_values,
                                         MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.edges_num);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_edges = mesh.corner_edges();

  attribute_math::DefaultMixer<T> mixer(r_values);

  for (const int face_index : faces.index_range()) {
    const T value = old_values[face_index];
    for (const int edge : corner_edges.slice(faces[face_index])) {
      mixer.mix_in(edge, value);
    }
  }
  mixer.finalize();
}

/* An edge is selected if any connected face was selected. */
template<>
void adapt_mesh_domain_face_to_edge_impl(const Mesh &mesh,
                                         const VArray<bool> &old_values,
                                         MutableSpan<bool> r_values)
{
  BLI_assert(r_values.size() == mesh.edges_num);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_edges = mesh.corner_edges();

  r_values.fill(false);
  threading::parallel_for(faces.index_range(), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      if (old_values[face_index]) {
        for (const int edge : corner_edges.slice(faces[face_index])) {
          r_values[edge] = true;
        }
      }
    }
  });
}

static GVArray adapt_mesh_domain_face_to_edge(const Mesh &mesh, const GVArray &varray)
{
  GArray<> values(varray.type(), mesh.edges_num);
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      adapt_mesh_domain_face_to_edge_impl<T>(
          mesh, varray.typed<T>(), values.as_mutable_span().typed<T>());
    }
  });
  return GVArray::from_garray(std::move(values));
}

static GVArray adapt_mesh_domain_point_to_face(const Mesh &mesh, const GVArray &varray)
{
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  GVArray new_varray;
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      if constexpr (std::is_same_v<T, bool>) {
        new_varray = VArray<T>::from_func(
            mesh.faces_num,
            [corner_verts, faces, varray = varray.typed<bool>()](const int face_index) {
              /* A face is selected if all of its vertices were selected. */
              for (const int vert : corner_verts.slice(faces[face_index])) {
                if (!varray[vert]) {
                  return false;
                }
              }
              return true;
            });
      }
      else {
        new_varray = VArray<T>::from_func(
            mesh.faces_num,
            [corner_verts, faces, varray = varray.typed<T>()](const int face_index) {
              T return_value;
              attribute_math::DefaultMixer<T> mixer({&return_value, 1});
              for (const int vert : corner_verts.slice(faces[face_index])) {
                mixer.mix_in(0, varray[vert]);
              }
              mixer.finalize();
              return return_value;
            });
      }
    }
  });
  return new_varray;
}

static GVArray adapt_mesh_domain_point_to_edge(const Mesh &mesh, const GVArray &varray)
{
  const Span<int2> edges = mesh.edges();

  GVArray new_varray;
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      if constexpr (std::is_same_v<T, bool>) {
        /* An edge is selected if both of its vertices were selected. */
        new_varray = VArray<bool>::from_func(
            edges.size(), [edges, varray = varray.typed<bool>()](const int edge_index) {
              const int2 &edge = edges[edge_index];
              return varray[edge[0]] && varray[edge[1]];
            });
      }
      else {
        new_varray = VArray<T>::from_func(
            edges.size(), [edges, varray = varray.typed<T>()](const int edge_index) {
              const int2 &edge = edges[edge_index];
              return attribute_math::mix2(0.5f, varray[edge[0]], varray[edge[1]]);
            });
      }
    }
  });
  return new_varray;
}

template<typename T>
void adapt_mesh_domain_edge_to_corner_impl(const Mesh &mesh,
                                           const VArray<T> &old_values,
                                           MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.corners_num);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_edges = mesh.corner_edges();

  attribute_math::DefaultMixer<T> mixer(r_values);

  for (const int face_index : faces.index_range()) {
    const IndexRange face = faces[face_index];

    /* For every corner, mix the values from the adjacent edges on the face. */
    for (const int corner : face) {
      const int corner_prev = mesh::face_corner_prev(face, corner);
      const int edge = corner_edges[corner];
      const int edge_prev = corner_edges[corner_prev];
      mixer.mix_in(corner, old_values[edge]);
      mixer.mix_in(corner, old_values[edge_prev]);
    }
  }

  mixer.finalize();
}

/* A corner is selected if its two adjacent edges were selected. */
template<>
void adapt_mesh_domain_edge_to_corner_impl(const Mesh &mesh,
                                           const VArray<bool> &old_values,
                                           MutableSpan<bool> r_values)
{
  BLI_assert(r_values.size() == mesh.corners_num);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_edges = mesh.corner_edges();

  r_values.fill(false);

  threading::parallel_for(faces.index_range(), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      const IndexRange face = faces[face_index];
      for (const int corner : face) {
        const int corner_prev = mesh::face_corner_prev(face, corner);
        const int edge = corner_edges[corner];
        const int edge_prev = corner_edges[corner_prev];
        if (old_values[edge] && old_values[edge_prev]) {
          r_values[corner] = true;
        }
      }
    }
  });
}

static GVArray adapt_mesh_domain_edge_to_corner(const Mesh &mesh, const GVArray &varray)
{
  GArray<> values(varray.type(), mesh.corners_num);
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      adapt_mesh_domain_edge_to_corner_impl<T>(
          mesh, varray.typed<T>(), values.as_mutable_span().typed<T>());
    }
  });
  return GVArray::from_garray(std::move(values));
}

template<typename T>
static void adapt_mesh_domain_edge_to_point_impl(const Mesh &mesh,
                                                 const VArray<T> &old_values,
                                                 MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.verts_num);
  const Span<int2> edges = mesh.edges();

  attribute_math::DefaultMixer<T> mixer(r_values);

  for (const int edge_index : IndexRange(mesh.edges_num)) {
    const int2 &edge = edges[edge_index];
    const T value = old_values[edge_index];
    mixer.mix_in(edge[0], value);
    mixer.mix_in(edge[1], value);
  }

  mixer.finalize();
}

/* A vertex is selected if any connected edge was selected. */
template<>
void adapt_mesh_domain_edge_to_point_impl(const Mesh &mesh,
                                          const VArray<bool> &old_values,
                                          MutableSpan<bool> r_values)
{
  BLI_assert(r_values.size() == mesh.verts_num);
  const Span<int2> edges = mesh.edges();

  /* Multiple threads can write to the same index here, but they are only
   * writing true, and writing to single bytes is expected to be threadsafe. */
  r_values.fill(false);
  threading::parallel_for(edges.index_range(), 4096, [&](const IndexRange range) {
    for (const int edge_index : range) {
      if (old_values[edge_index]) {
        const int2 &edge = edges[edge_index];
        r_values[edge[0]] = true;
        r_values[edge[1]] = true;
      }
    }
  });
}

static GVArray adapt_mesh_domain_edge_to_point(const Mesh &mesh, const GVArray &varray)
{
  GArray<> values(varray.type(), mesh.verts_num);
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      adapt_mesh_domain_edge_to_point_impl<T>(
          mesh, varray.typed<T>(), values.as_mutable_span().typed<T>());
    }
  });
  return GVArray::from_garray(std::move(values));
}

static GVArray adapt_mesh_domain_edge_to_face(const Mesh &mesh, const GVArray &varray)
{
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_edges = mesh.corner_edges();

  GVArray new_varray;
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      if constexpr (std::is_same_v<T, bool>) {
        /* A face is selected if all of its edges are selected. */
        new_varray = VArray<bool>::from_func(
            faces.size(), [corner_edges, faces, varray = varray.typed<T>()](const int face_index) {
              for (const int edge : corner_edges.slice(faces[face_index])) {
                if (!varray[edge]) {
                  return false;
                }
              }
              return true;
            });
      }
      else {
        new_varray = VArray<T>::from_func(
            faces.size(), [corner_edges, faces, varray = varray.typed<T>()](const int face_index) {
              T return_value;
              attribute_math::DefaultMixer<T> mixer({&return_value, 1});
              for (const int edge : corner_edges.slice(faces[face_index])) {
                mixer.mix_in(0, varray[edge]);
              }
              mixer.finalize();
              return return_value;
            });
      }
    }
  });
  return new_varray;
}

static bool can_simple_adapt_for_single(const Mesh &mesh,
                                        const AttrDomain from_domain,
                                        const AttrDomain to_domain)
{
  /* For some domain combinations, a single value will always map directly. For others, there may
   * be loose elements on the result domain that should have the default value rather than the
   * single value from the source. */
  switch (from_domain) {
    case AttrDomain::Point:
      /* All other domains are always connected to points. */
      return true;
    case AttrDomain::Edge:
      if (to_domain == AttrDomain::Point) {
        return mesh.loose_verts().count == 0;
      }
      return true;
    case AttrDomain::Face:
      if (to_domain == AttrDomain::Point) {
        return mesh.verts_no_face().count == 0;
      }
      if (to_domain == AttrDomain::Edge) {
        return mesh.loose_edges().count == 0;
      }
      return true;
    case AttrDomain::Corner:
      if (to_domain == AttrDomain::Point) {
        return mesh.verts_no_face().count == 0;
      }
      if (to_domain == AttrDomain::Edge) {
        return mesh.loose_edges().count == 0;
      }
      return true;
    default:
      BLI_assert_unreachable();
      return false;
  }
}

static GVArray adapt_mesh_attribute_domain(const Mesh &mesh,
                                           const GVArray &varray,
                                           const AttrDomain from_domain,
                                           const AttrDomain to_domain)
{
  if (!varray) {
    return {};
  }
  if (varray.is_empty()) {
    return {};
  }
  if (from_domain == to_domain) {
    return varray;
  }
  if (varray.is_single()) {
    if (can_simple_adapt_for_single(mesh, from_domain, to_domain)) {
      BUFFER_FOR_CPP_TYPE_VALUE(varray.type(), value);
      varray.get_internal_single(value);
      return GVArray::from_single(varray.type(), mesh.attributes().domain_size(to_domain), value);
    }
  }

  switch (from_domain) {
    case AttrDomain::Corner: {
      switch (to_domain) {
        case AttrDomain::Point:
          return adapt_mesh_domain_corner_to_point(mesh, varray);
        case AttrDomain::Face:
          return adapt_mesh_domain_corner_to_face(mesh, varray);
        case AttrDomain::Edge:
          return adapt_mesh_domain_corner_to_edge(mesh, varray);
        default:
          break;
      }
      break;
    }
    case AttrDomain::Point: {
      switch (to_domain) {
        case AttrDomain::Corner:
          return adapt_mesh_domain_point_to_corner(mesh, varray);
        case AttrDomain::Face:
          return adapt_mesh_domain_point_to_face(mesh, varray);
        case AttrDomain::Edge:
          return adapt_mesh_domain_point_to_edge(mesh, varray);
        default:
          break;
      }
      break;
    }
    case AttrDomain::Face: {
      switch (to_domain) {
        case AttrDomain::Point:
          return adapt_mesh_domain_face_to_point(mesh, varray);
        case AttrDomain::Corner:
          return adapt_mesh_domain_face_to_corner(mesh, varray);
        case AttrDomain::Edge:
          return adapt_mesh_domain_face_to_edge(mesh, varray);
        default:
          break;
      }
      break;
    }
    case AttrDomain::Edge: {
      switch (to_domain) {
        case AttrDomain::Corner:
          return adapt_mesh_domain_edge_to_corner(mesh, varray);
        case AttrDomain::Point:
          return adapt_mesh_domain_edge_to_point(mesh, varray);
        case AttrDomain::Face:
          return adapt_mesh_domain_edge_to_face(mesh, varray);
        default:
          break;
      }
      break;
    }
    default:
      break;
  }

  return {};
}

static void tag_component_positions_changed(void *owner)
{
  Mesh *mesh = static_cast<Mesh *>(owner);
  if (mesh != nullptr) {
    mesh->tag_positions_changed();
  }
}

static void tag_component_sharpness_changed(void *owner)
{
  if (Mesh *mesh = static_cast<Mesh *>(owner)) {
    mesh->tag_sharpness_changed();
  }
}

static void tag_material_index_changed(void *owner)
{
  if (Mesh *mesh = static_cast<Mesh *>(owner)) {
    mesh->tag_material_index_changed();
  }
}

/**
 * This provider makes vertex groups available as float attributes.
 */
class MeshVertexGroupsAttributeProvider final : public DynamicAttributesProvider {
 public:
  GAttributeReader try_get_for_read(const void *owner, const StringRef attribute_id) const final
  {
    const Mesh *mesh = static_cast<const Mesh *>(owner);
    if (mesh == nullptr) {
      return {};
    }
    const int vertex_group_index = BKE_defgroup_name_index(&mesh->vertex_group_names,
                                                           attribute_id);
    if (vertex_group_index < 0) {
      return {};
    }
    const Span<MDeformVert> dverts = mesh->deform_verts();
    return this->get_for_vertex_group_index(*mesh, dverts, vertex_group_index);
  }

  GAttributeReader get_for_vertex_group_index(const Mesh &mesh,
                                              const Span<MDeformVert> dverts,
                                              const int vertex_group_index) const
  {
    BLI_assert(vertex_group_index >= 0);
    if (dverts.is_empty()) {
      return {VArray<float>::from_single(0.0f, mesh.verts_num), AttrDomain::Point};
    }
    return {varray_for_deform_verts(dverts, vertex_group_index), AttrDomain::Point};
  }

  GAttributeWriter try_get_for_write(void *owner, const StringRef attribute_id) const final
  {
    Mesh *mesh = static_cast<Mesh *>(owner);
    if (mesh == nullptr) {
      return {};
    }

    const int vertex_group_index = BKE_defgroup_name_index(&mesh->vertex_group_names,
                                                           attribute_id);
    if (vertex_group_index < 0) {
      return {};
    }
    MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();
    return {varray_for_mutable_deform_verts(dverts, vertex_group_index), AttrDomain::Point};
  }

  bool try_delete(void *owner, const StringRef name) const final
  {
    Mesh *mesh = static_cast<Mesh *>(owner);
    if (mesh == nullptr) {
      return true;
    }

    int index;
    bDeformGroup *group;
    if (!BKE_id_defgroup_name_find(&mesh->id, name, &index, &group)) {
      return false;
    }
    BLI_remlink(&mesh->vertex_group_names, group);
    MEM_freeN(group);
    if (mesh->deform_verts().is_empty()) {
      return true;
    }

    MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();
    remove_defgroup_index(dverts, index);
    return true;
  }

  bool foreach_attribute(const void *owner,
                         const FunctionRef<void(const AttributeIter &)> fn) const final
  {
    const Mesh *mesh = static_cast<const Mesh *>(owner);
    if (mesh == nullptr) {
      return true;
    }
    const AttributeAccessor accessor = mesh->attributes();
    const Span<MDeformVert> dverts = mesh->deform_verts();

    int group_index = 0;
    LISTBASE_FOREACH_INDEX (const bDeformGroup *, group, &mesh->vertex_group_names, group_index) {
      const auto get_fn = [&]() {
        return this->get_for_vertex_group_index(*mesh, dverts, group_index);
      };

      AttributeIter iter{group->name, AttrDomain::Point, bke::AttrType::Float, get_fn};
      iter.is_builtin = false;
      iter.accessor = &accessor;
      fn(iter);
      if (iter.is_stopped()) {
        return false;
      }
    }
    return true;
  }

  void foreach_domain(const FunctionRef<void(AttrDomain)> callback) const final
  {
    callback(AttrDomain::Point);
  }
};

static std::function<void()> get_tag_modified_function(void *owner, const StringRef name)
{
  if (name.startswith(".hide")) {
    return [owner]() { (static_cast<Mesh *>(owner))->tag_visibility_changed(); };
  }
  if (name == "custom_normal") {
    return [owner]() { (static_cast<Mesh *>(owner))->tag_custom_normals_changed(); };
  }
  return {};
}

/**
 * In this function all the attribute providers for a mesh component are created. Most data in this
 * function is statically allocated, because it does not change over time.
 */
static GeometryAttributeProviders create_attribute_providers_for_mesh()
{
#define MAKE_MUTABLE_CUSTOM_DATA_GETTER(NAME) \
  [](void *owner) -> CustomData * { \
    Mesh *mesh = static_cast<Mesh *>(owner); \
    return &mesh->NAME; \
  }
#define MAKE_CONST_CUSTOM_DATA_GETTER(NAME) \
  [](const void *owner) -> const CustomData * { \
    const Mesh *mesh = static_cast<const Mesh *>(owner); \
    return &mesh->NAME; \
  }
#define MAKE_GET_ELEMENT_NUM_GETTER(NAME) \
  [](const void *owner) -> int { \
    const Mesh *mesh = static_cast<const Mesh *>(owner); \
    return mesh->NAME; \
  }

  static CustomDataAccessInfo corner_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(corner_data),
                                               MAKE_CONST_CUSTOM_DATA_GETTER(corner_data),
                                               MAKE_GET_ELEMENT_NUM_GETTER(corners_num),
                                               get_tag_modified_function};
  static CustomDataAccessInfo point_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(vert_data),
                                              MAKE_CONST_CUSTOM_DATA_GETTER(vert_data),
                                              MAKE_GET_ELEMENT_NUM_GETTER(verts_num),
                                              get_tag_modified_function};
  static CustomDataAccessInfo edge_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(edge_data),
                                             MAKE_CONST_CUSTOM_DATA_GETTER(edge_data),
                                             MAKE_GET_ELEMENT_NUM_GETTER(edges_num),
                                             get_tag_modified_function};
  static CustomDataAccessInfo face_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(face_data),
                                             MAKE_CONST_CUSTOM_DATA_GETTER(face_data),
                                             MAKE_GET_ELEMENT_NUM_GETTER(faces_num),
                                             get_tag_modified_function};

#undef MAKE_CONST_CUSTOM_DATA_GETTER
#undef MAKE_MUTABLE_CUSTOM_DATA_GETTER

  static BuiltinCustomDataLayerProvider position("position",
                                                 AttrDomain::Point,
                                                 CD_PROP_FLOAT3,
                                                 BuiltinAttributeProvider::NonDeletable,
                                                 point_access,
                                                 tag_component_positions_changed);

  static const auto material_index_clamp = mf::build::SI1_SO<int, int>(
      "Material Index Validate",
      [](int value) {
        /* Use #short for the maximum since many areas still use that type for indices. */
        return std::clamp<int>(value, 0, std::numeric_limits<short>::max());
      },
      mf::build::exec_presets::AllSpanOrSingle());
  static BuiltinCustomDataLayerProvider material_index("material_index",
                                                       AttrDomain::Face,
                                                       CD_PROP_INT32,
                                                       BuiltinAttributeProvider::Deletable,
                                                       face_access,
                                                       tag_material_index_changed,
                                                       AttributeValidator{&material_index_clamp});

  static const auto int2_index_clamp = mf::build::SI1_SO<int2, int2>(
      "Index Validate",
      [](int2 value) { return math::max(value, int2(0)); },
      mf::build::exec_presets::AllSpanOrSingle());
  static BuiltinCustomDataLayerProvider edge_verts(".edge_verts",
                                                   AttrDomain::Edge,
                                                   CD_PROP_INT32_2D,
                                                   BuiltinAttributeProvider::NonDeletable,
                                                   edge_access,
                                                   nullptr,
                                                   AttributeValidator{&int2_index_clamp});

  /* NOTE: This clamping is more of a last resort, since it's quite easy to make an
   * invalid mesh that will crash Blender by arbitrarily editing this attribute. */
  static const auto int_index_clamp = mf::build::SI1_SO<int, int>(
      "Index Validate",
      [](int value) { return std::max(value, 0); },
      mf::build::exec_presets::AllSpanOrSingle());
  static BuiltinCustomDataLayerProvider corner_vert(".corner_vert",
                                                    AttrDomain::Corner,
                                                    CD_PROP_INT32,
                                                    BuiltinAttributeProvider::NonDeletable,
                                                    corner_access,
                                                    nullptr,
                                                    AttributeValidator{&int_index_clamp});
  static BuiltinCustomDataLayerProvider corner_edge(".corner_edge",
                                                    AttrDomain::Corner,
                                                    CD_PROP_INT32,
                                                    BuiltinAttributeProvider::NonDeletable,
                                                    corner_access,
                                                    nullptr,
                                                    AttributeValidator{&int_index_clamp});

  static BuiltinCustomDataLayerProvider sharp_face("sharp_face",
                                                   AttrDomain::Face,
                                                   CD_PROP_BOOL,
                                                   BuiltinAttributeProvider::Deletable,
                                                   face_access,
                                                   tag_component_sharpness_changed);

  static BuiltinCustomDataLayerProvider sharp_edge("sharp_edge",
                                                   AttrDomain::Edge,
                                                   CD_PROP_BOOL,
                                                   BuiltinAttributeProvider::Deletable,
                                                   edge_access,
                                                   tag_component_sharpness_changed);

  static MeshVertexGroupsAttributeProvider vertex_groups;
  static CustomDataAttributeProvider corner_custom_data(AttrDomain::Corner, corner_access);
  static CustomDataAttributeProvider point_custom_data(AttrDomain::Point, point_access);
  static CustomDataAttributeProvider edge_custom_data(AttrDomain::Edge, edge_access);
  static CustomDataAttributeProvider face_custom_data(AttrDomain::Face, face_access);

  return GeometryAttributeProviders({&position,
                                     &edge_verts,
                                     &corner_vert,
                                     &corner_edge,
                                     &material_index,
                                     &sharp_face,
                                     &sharp_edge},
                                    {&corner_custom_data,
                                     &vertex_groups,
                                     &point_custom_data,
                                     &edge_custom_data,
                                     &face_custom_data});
}

static AttributeAccessorFunctions get_mesh_accessor_functions()
{
  static const GeometryAttributeProviders providers = create_attribute_providers_for_mesh();
  AttributeAccessorFunctions fn =
      attribute_accessor_functions::accessor_functions_for_providers<providers>();
  fn.domain_size = [](const void *owner, const AttrDomain domain) {
    if (owner == nullptr) {
      return 0;
    }
    const Mesh &mesh = *static_cast<const Mesh *>(owner);
    switch (domain) {
      case AttrDomain::Point:
        return mesh.verts_num;
      case AttrDomain::Edge:
        return mesh.edges_num;
      case AttrDomain::Face:
        return mesh.faces_num;
      case AttrDomain::Corner:
        return mesh.corners_num;
      default:
        return 0;
    }
  };
  fn.domain_supported = [](const void * /*owner*/, const AttrDomain domain) {
    return ELEM(domain, AttrDomain::Point, AttrDomain::Edge, AttrDomain::Face, AttrDomain::Corner);
  };
  fn.adapt_domain = [](const void *owner,
                       const GVArray &varray,
                       const AttrDomain from_domain,
                       const AttrDomain to_domain) -> GVArray {
    if (owner == nullptr) {
      return {};
    }
    const Mesh &mesh = *static_cast<const Mesh *>(owner);
    return adapt_mesh_attribute_domain(mesh, varray, from_domain, to_domain);
  };
  return fn;
}

const AttributeAccessorFunctions &mesh_attribute_accessor_functions()
{
  static const AttributeAccessorFunctions fn = get_mesh_accessor_functions();
  return fn;
}

}  // namespace blender::bke
