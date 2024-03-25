/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#include "BLI_listbase.h"
#include "BLI_task.hh"

#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "BKE_attribute_math.hh"
#include "BKE_deform.hh"
#include "BKE_geometry_fields.hh"
#include "BKE_geometry_set.hh"
#include "BKE_lib_id.hh"
#include "BKE_mesh.hh"
#include "BKE_mesh_mapping.hh"

#include "FN_multi_function_builder.hh"

#include "attribute_access_intern.hh"

namespace blender::bke {

/* -------------------------------------------------------------------- */
/** \name Geometry Component Implementation
 * \{ */

MeshComponent::MeshComponent() : GeometryComponent(Type::Mesh) {}

MeshComponent::MeshComponent(Mesh *mesh, GeometryOwnershipType ownership)
    : GeometryComponent(Type::Mesh), mesh_(mesh), ownership_(ownership)
{
}

MeshComponent::~MeshComponent()
{
  this->clear();
}

GeometryComponentPtr MeshComponent::copy() const
{
  MeshComponent *new_component = new MeshComponent();
  if (mesh_ != nullptr) {
    new_component->mesh_ = BKE_mesh_copy_for_eval(mesh_);
    new_component->ownership_ = GeometryOwnershipType::Owned;
  }
  return GeometryComponentPtr(new_component);
}

void MeshComponent::clear()
{
  BLI_assert(this->is_mutable() || this->is_expired());
  if (mesh_ != nullptr) {
    if (ownership_ == GeometryOwnershipType::Owned) {
      BKE_id_free(nullptr, mesh_);
    }
    mesh_ = nullptr;
  }
}

bool MeshComponent::has_mesh() const
{
  return mesh_ != nullptr;
}

void MeshComponent::replace(Mesh *mesh, GeometryOwnershipType ownership)
{
  BLI_assert(this->is_mutable());
  this->clear();
  mesh_ = mesh;
  ownership_ = ownership;
}

Mesh *MeshComponent::release()
{
  BLI_assert(this->is_mutable());
  Mesh *mesh = mesh_;
  mesh_ = nullptr;
  return mesh;
}

const Mesh *MeshComponent::get() const
{
  return mesh_;
}

Mesh *MeshComponent::get_for_write()
{
  BLI_assert(this->is_mutable());
  if (ownership_ == GeometryOwnershipType::ReadOnly) {
    mesh_ = BKE_mesh_copy_for_eval(mesh_);
    ownership_ = GeometryOwnershipType::Owned;
  }
  return mesh_;
}

bool MeshComponent::is_empty() const
{
  return mesh_ == nullptr;
}

bool MeshComponent::owns_direct_data() const
{
  return ownership_ == GeometryOwnershipType::Owned;
}

void MeshComponent::ensure_owns_direct_data()
{
  BLI_assert(this->is_mutable());
  if (ownership_ != GeometryOwnershipType::Owned) {
    if (mesh_) {
      mesh_ = BKE_mesh_copy_for_eval(mesh_);
    }
    ownership_ = GeometryOwnershipType::Owned;
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Mesh Normals Field Input
 * \{ */

VArray<float3> mesh_normals_varray(const Mesh &mesh,
                                   const IndexMask &mask,
                                   const AttrDomain domain)
{
  switch (domain) {
    case AttrDomain::Face: {
      return VArray<float3>::ForSpan(mesh.face_normals());
    }
    case AttrDomain::Point: {
      return VArray<float3>::ForSpan(mesh.vert_normals());
    }
    case AttrDomain::Edge: {
      /* In this case, start with vertex normals and convert to the edge domain, since the
       * conversion from edges to vertices is very simple. Use "manual" domain interpolation
       * instead of the GeometryComponent API to avoid calculating unnecessary values and to
       * allow normalizing the result more simply. */
      Span<float3> vert_normals = mesh.vert_normals();
      const Span<int2> edges = mesh.edges();
      Array<float3> edge_normals(mask.min_array_size());
      mask.foreach_index([&](const int i) {
        const int2 &edge = edges[i];
        edge_normals[i] = math::normalize(
            math::interpolate(vert_normals[edge[0]], vert_normals[edge[1]], 0.5f));
      });

      return VArray<float3>::ForContainer(std::move(edge_normals));
    }
    case AttrDomain::Corner: {
      /* The normals on corners are just the mesh's face normals, so start with the face normal
       * array and copy the face normal for each of its corners. In this case using the mesh
       * component's generic domain interpolation is fine, the data will still be normalized,
       * since the face normal is just copied to every corner. */
      return mesh.attributes().adapt_domain(
          VArray<float3>::ForSpan(mesh.face_normals()), AttrDomain::Face, AttrDomain::Corner);
    }
    default:
      return {};
  }
}

/** \} */

/* -------------------------------------------------------------------- */
/** \name Attribute Access
 * \{ */

template<typename T>
static void adapt_mesh_domain_corner_to_point_impl(const Mesh &mesh,
                                                   const VArray<T> &old_values,
                                                   MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.verts_num);
  const Span<int> corner_verts = mesh.corner_verts();

  attribute_math::DefaultMixer<T> mixer(r_values);
  for (const int corner : IndexRange(mesh.corners_num)) {
    mixer.mix_in(corner_verts[corner], old_values[corner]);
  }
  mixer.finalize();
}

/* A vertex is selected if all connected face corners were selected and it is not loose. */
template<>
void adapt_mesh_domain_corner_to_point_impl(const Mesh &mesh,
                                            const VArray<bool> &old_values,
                                            MutableSpan<bool> r_values)
{
  BLI_assert(r_values.size() == mesh.verts_num);
  const Span<int> corner_verts = mesh.corner_verts();

  r_values.fill(true);
  for (const int corner : IndexRange(mesh.corners_num)) {
    const int point_index = corner_verts[corner];

    if (!old_values[corner]) {
      r_values[point_index] = false;
    }
  }

  /* Deselect loose vertices without corners that are still selected from the 'true' default. */
  const LooseVertCache &loose_verts = mesh.verts_no_face();
  if (loose_verts.count > 0) {
    const BitSpan bits = loose_verts.is_loose_bits;
    threading::parallel_for(bits.index_range(), 2048, [&](const IndexRange range) {
      for (const int vert_index : range) {
        if (bits[vert_index]) {
          r_values[vert_index] = false;
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
  return GVArray::ForGArray(std::move(values));
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
    new_varray = VArray<T>::ForFunc(
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
        new_varray = VArray<T>::ForFunc(
            faces.size(), [faces, varray = varray.typed<bool>()](const int face_index) {
              /* A face is selected if all of its corners were selected. */
              for (const int loop_index : faces[face_index]) {
                if (!varray[loop_index]) {
                  return false;
                }
              }
              return true;
            });
      }
      else {
        new_varray = VArray<T>::ForFunc(
            faces.size(), [faces, varray = varray.typed<T>()](const int face_index) {
              T return_value;
              attribute_math::DefaultMixer<T> mixer({&return_value, 1});
              for (const int loop_index : faces[face_index]) {
                const T value = varray[loop_index];
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
  return GVArray::ForGArray(std::move(values));
}

template<typename T>
void adapt_mesh_domain_face_to_point_impl(const Mesh &mesh,
                                          const VArray<T> &old_values,
                                          MutableSpan<T> r_values)
{
  BLI_assert(r_values.size() == mesh.verts_num);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  attribute_math::DefaultMixer<T> mixer(r_values);

  for (const int face_index : faces.index_range()) {
    const T value = old_values[face_index];
    for (const int vert : corner_verts.slice(faces[face_index])) {
      mixer.mix_in(vert, value);
    }
  }

  mixer.finalize();
}

/* A vertex is selected if any of the connected faces were selected. */
template<>
void adapt_mesh_domain_face_to_point_impl(const Mesh &mesh,
                                          const VArray<bool> &old_values,
                                          MutableSpan<bool> r_values)
{
  BLI_assert(r_values.size() == mesh.verts_num);
  const OffsetIndices faces = mesh.faces();
  const Span<int> corner_verts = mesh.corner_verts();

  r_values.fill(false);
  threading::parallel_for(faces.index_range(), 2048, [&](const IndexRange range) {
    for (const int face_index : range) {
      if (old_values[face_index]) {
        for (const int vert : corner_verts.slice(faces[face_index])) {
          r_values[vert] = true;
        }
      }
    }
  });
}

static GVArray adapt_mesh_domain_face_to_point(const Mesh &mesh, const GVArray &varray)
{
  GArray<> values(varray.type(), mesh.verts_num);
  attribute_math::convert_to_static_type(varray.type(), [&](auto dummy) {
    using T = decltype(dummy);
    if constexpr (!std::is_void_v<attribute_math::DefaultMixer<T>>) {
      adapt_mesh_domain_face_to_point_impl<T>(
          mesh, varray.typed<T>(), values.as_mutable_span().typed<T>());
    }
  });
  return GVArray::ForGArray(std::move(values));
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
  return GVArray::ForGArray(std::move(values));
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
  return GVArray::ForGArray(std::move(values));
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
        new_varray = VArray<T>::ForFunc(
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
        new_varray = VArray<T>::ForFunc(
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
        new_varray = VArray<bool>::ForFunc(
            edges.size(), [edges, varray = varray.typed<bool>()](const int edge_index) {
              const int2 &edge = edges[edge_index];
              return varray[edge[0]] && varray[edge[1]];
            });
      }
      else {
        new_varray = VArray<T>::ForFunc(
            edges.size(), [edges, varray = varray.typed<T>()](const int edge_index) {
              T return_value;
              attribute_math::DefaultMixer<T> mixer({&return_value, 1});
              const int2 &edge = edges[edge_index];
              mixer.mix_in(0, varray[edge[0]]);
              mixer.mix_in(0, varray[edge[1]]);
              mixer.finalize();
              return return_value;
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
    for (const int loop_index : face) {
      const int loop_index_prev = mesh::face_corner_prev(face, loop_index);
      const int edge = corner_edges[loop_index];
      const int edge_prev = corner_edges[loop_index_prev];
      mixer.mix_in(loop_index, old_values[edge]);
      mixer.mix_in(loop_index, old_values[edge_prev]);
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
      for (const int loop_index : face) {
        const int loop_index_prev = mesh::face_corner_prev(face, loop_index);
        const int edge = corner_edges[loop_index];
        const int edge_prev = corner_edges[loop_index_prev];
        if (old_values[edge] && old_values[edge_prev]) {
          r_values[loop_index] = true;
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
  return GVArray::ForGArray(std::move(values));
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
  return GVArray::ForGArray(std::move(values));
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
        new_varray = VArray<bool>::ForFunc(
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
        new_varray = VArray<T>::ForFunc(
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
      return GVArray::ForSingle(varray.type(), mesh.attributes().domain_size(to_domain), value);
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

/**
 * This provider makes vertex groups available as float attributes.
 */
class MeshVertexGroupsAttributeProvider final : public DynamicAttributesProvider {
 public:
  GAttributeReader try_get_for_read(const void *owner,
                                    const AttributeIDRef &attribute_id) const final
  {
    if (attribute_id.is_anonymous()) {
      return {};
    }
    const Mesh *mesh = static_cast<const Mesh *>(owner);
    if (mesh == nullptr) {
      return {};
    }
    const std::string name = attribute_id.name();
    const int vertex_group_index = BLI_findstringindex(
        &mesh->vertex_group_names, name.c_str(), offsetof(bDeformGroup, name));
    if (vertex_group_index < 0) {
      return {};
    }
    const Span<MDeformVert> dverts = mesh->deform_verts();
    if (dverts.is_empty()) {
      static const float default_value = 0.0f;
      return {VArray<float>::ForSingle(default_value, mesh->verts_num), AttrDomain::Point};
    }
    return {varray_for_deform_verts(dverts, vertex_group_index), AttrDomain::Point};
  }

  GAttributeWriter try_get_for_write(void *owner, const AttributeIDRef &attribute_id) const final
  {
    if (attribute_id.is_anonymous()) {
      return {};
    }
    Mesh *mesh = static_cast<Mesh *>(owner);
    if (mesh == nullptr) {
      return {};
    }

    const std::string name = attribute_id.name();
    const int vertex_group_index = BLI_findstringindex(
        &mesh->vertex_group_names, name.c_str(), offsetof(bDeformGroup, name));
    if (vertex_group_index < 0) {
      return {};
    }
    MutableSpan<MDeformVert> dverts = mesh->deform_verts_for_write();
    return {varray_for_mutable_deform_verts(dverts, vertex_group_index), AttrDomain::Point};
  }

  bool try_delete(void *owner, const AttributeIDRef &attribute_id) const final
  {
    if (attribute_id.is_anonymous()) {
      return false;
    }
    Mesh *mesh = static_cast<Mesh *>(owner);
    if (mesh == nullptr) {
      return true;
    }

    const std::string name = attribute_id.name();

    int index;
    bDeformGroup *group;
    if (!BKE_id_defgroup_name_find(&mesh->id, name.c_str(), &index, &group)) {
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

  bool foreach_attribute(const void *owner, const AttributeForeachCallback callback) const final
  {
    const Mesh *mesh = static_cast<const Mesh *>(owner);
    if (mesh == nullptr) {
      return true;
    }

    LISTBASE_FOREACH (const bDeformGroup *, group, &mesh->vertex_group_names) {
      if (!callback(group->name, {AttrDomain::Point, CD_PROP_FLOAT})) {
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

/**
 * In this function all the attribute providers for a mesh component are created. Most data in this
 * function is statically allocated, because it does not change over time.
 */
static ComponentAttributeProviders create_attribute_providers_for_mesh()
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
                                               MAKE_GET_ELEMENT_NUM_GETTER(corners_num)};
  static CustomDataAccessInfo point_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(vert_data),
                                              MAKE_CONST_CUSTOM_DATA_GETTER(vert_data),
                                              MAKE_GET_ELEMENT_NUM_GETTER(verts_num)};
  static CustomDataAccessInfo edge_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(edge_data),
                                             MAKE_CONST_CUSTOM_DATA_GETTER(edge_data),
                                             MAKE_GET_ELEMENT_NUM_GETTER(edges_num)};
  static CustomDataAccessInfo face_access = {MAKE_MUTABLE_CUSTOM_DATA_GETTER(face_data),
                                             MAKE_CONST_CUSTOM_DATA_GETTER(face_data),
                                             MAKE_GET_ELEMENT_NUM_GETTER(faces_num)};

#undef MAKE_CONST_CUSTOM_DATA_GETTER
#undef MAKE_MUTABLE_CUSTOM_DATA_GETTER

  static BuiltinCustomDataLayerProvider position("position",
                                                 AttrDomain::Point,
                                                 CD_PROP_FLOAT3,
                                                 BuiltinAttributeProvider::NonDeletable,
                                                 point_access,
                                                 tag_component_positions_changed);

  static BuiltinCustomDataLayerProvider id("id",
                                           AttrDomain::Point,
                                           CD_PROP_INT32,
                                           BuiltinAttributeProvider::Deletable,
                                           point_access,
                                           nullptr);

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
                                                       nullptr,
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

  /* Note: This clamping is more of a last resort, since it's quite easy to make an
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

  return ComponentAttributeProviders({&position,
                                      &edge_verts,
                                      &corner_vert,
                                      &corner_edge,
                                      &id,
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
  static const ComponentAttributeProviders providers = create_attribute_providers_for_mesh();
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

static const AttributeAccessorFunctions &get_mesh_accessor_functions_ref()
{
  static const AttributeAccessorFunctions fn = get_mesh_accessor_functions();
  return fn;
}

}  // namespace blender::bke

blender::bke::AttributeAccessor Mesh::attributes() const
{
  return blender::bke::AttributeAccessor(this, blender::bke::get_mesh_accessor_functions_ref());
}

blender::bke::MutableAttributeAccessor Mesh::attributes_for_write()
{
  return blender::bke::MutableAttributeAccessor(this,
                                                blender::bke::get_mesh_accessor_functions_ref());
}

namespace blender::bke {

std::optional<AttributeAccessor> MeshComponent::attributes() const
{
  return AttributeAccessor(mesh_, get_mesh_accessor_functions_ref());
}

std::optional<MutableAttributeAccessor> MeshComponent::attributes_for_write()
{
  Mesh *mesh = this->get_for_write();
  return MutableAttributeAccessor(mesh, get_mesh_accessor_functions_ref());
}

/** \} */

}  // namespace blender::bke
