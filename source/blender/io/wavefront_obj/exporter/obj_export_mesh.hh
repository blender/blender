/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include <optional>

#include "BLI_math_matrix_types.hh"
#include "BLI_math_vector_types.hh"
#include "BLI_offset_indices.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"
#include "BLI_virtual_array.hh"

#include "DNA_material_types.h"
#include "DNA_object_types.h"

#include "IO_wavefront_obj.hh"

namespace blender::io::obj {
/** Denote absence for usually non-negative numbers. */
const int NOT_FOUND = -1;
/** Any negative number other than `NOT_FOUND` to initialize usually non-negative numbers. */
const int NEGATIVE_INIT = -10;

class OBJMesh : NonCopyable {
 private:
  std::string object_name_;
  /** A pointer to #owned_export_mesh_ or the object'ed evaluated/original mesh. */
  const Mesh *export_mesh_;
  /** A mesh owned here, if created or modified for the export. May be null. */
  Mesh *owned_export_mesh_ = nullptr;
  Span<int2> mesh_edges_;
  OffsetIndices<int> mesh_faces_;
  Span<int> mesh_corner_verts_;
  VArray<bool> sharp_faces_;

  /**
   * Final transform of an object obtained from export settings (up_axis, forward_axis) and the
   * object's world transform matrix.
   */
  float4x4 world_and_axes_transform_;
  float3x3 world_and_axes_normal_transform_;
  bool mirrored_transform_;

  /** Per-corner UV index. */
  Array<int> corner_to_uv_index_;
  /** UV vertices. */
  Vector<float2> uv_coords_;

  /** Index into #normal_coords_ for every face corner. */
  Array<int> corner_to_normal_index_;
  /** De-duplicated normals, indexed by #corner_to_normal_index_. */
  Array<float3> normal_coords_;
  /**
   * Total smooth groups in an object.
   */
  int tot_smooth_groups_ = NEGATIVE_INIT;
  /**
   * Polygon aligned array of their smooth groups.
   */
  int *face_smooth_groups_ = nullptr;
  /**
   * Order in which the faces should be written into the file (sorted by material index).
   */
  Array<int> face_order_;

 public:
  Array<const Material *> materials;

  /**
   * Store evaluated Object and Mesh pointers. Conditionally triangulate a mesh, or
   * create a new Mesh from a Curve.
   */
  OBJMesh(Depsgraph *depsgraph, const OBJExportParams &export_params, Object *mesh_object);
  ~OBJMesh();

  /* Clear various arrays to release potentially large memory allocations. */
  void clear();

  int tot_vertices() const;
  int tot_faces() const;
  int tot_uv_vertices() const;
  int tot_edges() const;
  int tot_deform_groups() const;
  bool is_mirrored_transform() const
  {
    return mirrored_transform_;
  }

  /**
   * \return Total materials in the object.
   */
  int16_t tot_materials() const;

  /**
   * Calculate smooth groups of a smooth-shaded object.
   * \return A face aligned array of smooth group numbers.
   */
  void calc_smooth_groups(bool use_bitflags);
  /**
   * \return Smooth group of the face at the given index.
   */
  int ith_smooth_group(int face_index) const;
  bool is_ith_face_smooth(int face_index) const;

  /**
   * Get object name as it appears in the outliner.
   */
  StringRef get_object_name() const;
  /**
   * Get Object's Mesh's name.
   */
  StringRef get_object_mesh_name() const;

  const float4x4 &get_world_axes_transform() const
  {
    return world_and_axes_transform_;
  }

  /**
   * Calculate vertex indices of all vertices of the face at the given index.
   */
  Span<int> calc_face_vert_indices(const int face_index) const
  {
    return mesh_corner_verts_.slice(mesh_faces_[face_index]);
  }

  /**
   * Calculate UV vertex coordinates of an Object.
   * Stores the coordinates and UV vertex indices in the member variables.
   */
  void store_uv_coords_and_indices();
  /* Get UV coordinates computed by store_uv_coords_and_indices. */
  const Span<float2> get_uv_coords() const
  {
    return uv_coords_;
  }
  Span<int> get_face_uv_indices(const int face_index) const
  {
    if (uv_coords_.is_empty()) {
      return {};
    }
    BLI_assert(face_index < mesh_faces_.size());
    return corner_to_uv_index_.as_span().slice(mesh_faces_[face_index]);
  }

  /**
   * Find the unique normals of the mesh and stores them in a member variable.
   * Also stores the indices into that vector with for each corner.
   */
  void store_normal_coords_and_indices();
  /* Get normals calculate by store_normal_coords_and_indices. */
  Span<float3> get_normal_coords() const
  {
    return normal_coords_;
  }
  /**
   * Calculate a face's face/corner normal indices.
   * \param face_index: Index of the face to calculate indices for.
   * \return Span of normal indices, aligned with vertices of face.
   */
  Span<int> get_face_normal_indices(const int face_index) const
  {
    if (corner_to_normal_index_.is_empty()) {
      return {};
    }
    const IndexRange face = mesh_faces_[face_index];
    return corner_to_normal_index_.as_span().slice(face);
  }

  /**
   * Find the most representative vertex group of a face.
   *
   * This adds up vertex group weights, and the group with the largest
   * weight sum across the face is the one returned.
   *
   * group_weights is temporary storage to avoid reallocations, it must
   * be the size of amount of vertex groups in the object.
   */
  int16_t get_face_deform_group_index(int face_index, MutableSpan<float> group_weights) const;
  /**
   * Find the name of the vertex deform group at the given index.
   * The index indices into the #Object.defbase.
   */
  const char *get_face_deform_group_name(int16_t def_group_index) const;

  /**
   * Calculate the order in which the faces should be written into the file (sorted by material
   * index).
   */
  void calc_face_order();

  /**
   * Remap face index according to face writing order.
   * When materials are not being written, the face order array
   * might be empty, in which case remap is a no-op.
   */
  int remap_face_index(int i) const
  {
    return i < 0 || i >= face_order_.size() ? i : face_order_[i];
  }

  const Mesh *get_mesh() const
  {
    return export_mesh_;
  }

 private:
  /** Override the mesh from the export scene's object. Takes ownership of the mesh. */
  void set_mesh(Mesh *mesh);
  /**
   * Triangulate the mesh pointed to by this object, potentially replacing it with a newly created
   * mesh.
   */
  void triangulate_mesh_eval();
  /**
   * Set the final transform after applying axes settings and an Object's world transform.
   */
  void set_world_axes_transform(const Object &obj_eval,
                                eIOAxis forward,
                                eIOAxis up,
                                float global_scale);
};
}  // namespace blender::io::obj
