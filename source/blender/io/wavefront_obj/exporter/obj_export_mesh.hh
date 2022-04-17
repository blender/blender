/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include <optional>

#include "BLI_math_vec_types.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_object_types.h"

#include "IO_wavefront_obj.h"

namespace blender::io::obj {
/** Denote absence for usually non-negative numbers. */
const int NOT_FOUND = -1;
/** Any negative number other than `NOT_FOUND` to initialize usually non-negative numbers. */
const int NEGATIVE_INIT = -10;

/**
 * #std::unique_ptr than handles freeing #BMesh.
 */
struct CustomBMeshDeleter {
  void operator()(BMesh *bmesh)
  {
    if (bmesh) {
      BM_mesh_free(bmesh);
    }
  }
};

using unique_bmesh_ptr = std::unique_ptr<BMesh, CustomBMeshDeleter>;

class OBJMesh : NonCopyable {
 private:
  /**
   * We need to copy the entire Object structure here because the dependency graph iterator
   * sometimes builds an Object in a temporary space that doesn't persist.
   */
  Object export_object_eval_;
  Mesh *export_mesh_eval_;
  /**
   * For curves which are converted to mesh, and triangulated meshes, a new mesh is allocated.
   */
  bool mesh_eval_needs_free_ = false;
  /**
   * Final transform of an object obtained from export settings (up_axis, forward_axis) and the
   * object's world transform matrix.
   */
  float world_and_axes_transform_[4][4];
  float world_and_axes_normal_transform_[3][3];
  bool mirrored_transform_;

  /**
   * Total UV vertices in a mesh's texture map.
   */
  int tot_uv_vertices_ = 0;
  /**
   * Per-polygon-per-vertex UV vertex indices.
   */
  Vector<Vector<int>> uv_indices_;
  /*
   * UV vertices.
   */
  Vector<float2> uv_coords_;
  /**
   * Per-loop normal index.
   */
  Vector<int> loop_to_normal_index_;
  /*
   * Normal coords.
   */
  Vector<float3> normal_coords_;
  /*
   * Total number of normal indices (maximum entry, plus 1, in
   * the loop_to_norm_index_ vector).
   */
  int tot_normal_indices_ = 0;
  /**
   * Total smooth groups in an object.
   */
  int tot_smooth_groups_ = NEGATIVE_INIT;
  /**
   * Polygon aligned array of their smooth groups.
   */
  int *poly_smooth_groups_ = nullptr;
  /**
   * Order in which the polygons should be written into the file (sorted by material index).
   */
  Vector<int> poly_order_;

 public:
  /**
   * Store evaluated Object and Mesh pointers. Conditionally triangulate a mesh, or
   * create a new Mesh from a Curve.
   */
  OBJMesh(Depsgraph *depsgraph, const OBJExportParams &export_params, Object *mesh_object);
  ~OBJMesh();

  /* Clear various arrays to release potentially large memory allocations. */
  void clear();

  int tot_vertices() const;
  int tot_polygons() const;
  int tot_uv_vertices() const;
  int tot_normal_indices() const;
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
   * Return mat_nr-th material of the object. The given index should be zero-based.
   */
  const Material *get_object_material(int16_t mat_nr) const;
  /**
   * Returns a zero-based index of a polygon's material indexing into
   * the Object's material slots.
   */
  int16_t ith_poly_matnr(int poly_index) const;

  void ensure_mesh_normals() const;
  void ensure_mesh_edges() const;

  /**
   * Calculate smooth groups of a smooth-shaded object.
   * \return A polygon aligned array of smooth group numbers.
   */
  void calc_smooth_groups(bool use_bitflags);
  /**
   * \return Smooth group of the polygon at the given index.
   */
  int ith_smooth_group(int poly_index) const;
  bool is_ith_poly_smooth(int poly_index) const;

  /**
   * Get object name as it appears in the outliner.
   */
  const char *get_object_name() const;
  /**
   * Get Object's Mesh's name.
   */
  const char *get_object_mesh_name() const;
  /**
   * Get object's material (at the given index) name. The given index should be zero-based.
   */
  const char *get_object_material_name(int16_t mat_nr) const;

  /**
   * Calculate coordinates of the vertex at the given index.
   */
  float3 calc_vertex_coords(int vert_index, float scaling_factor) const;
  /**
   * Calculate vertex indices of all vertices of the polygon at the given index.
   */
  Vector<int> calc_poly_vertex_indices(int poly_index) const;
  /**
   * Calculate UV vertex coordinates of an Object.
   * Stores the coordinates and UV vertex indices in the member variables.
   */
  void store_uv_coords_and_indices();
  /* Get UV coordinates computed by store_uv_coords_and_indices. */
  const Vector<float2> &get_uv_coords() const
  {
    return uv_coords_;
  }
  Span<int> calc_poly_uv_indices(int poly_index) const;
  /**
   * Calculate polygon normal of a polygon at given index.
   *
   * Should be used for flat-shaded polygons.
   */
  float3 calc_poly_normal(int poly_index) const;
  /**
   * Find the unique normals of the mesh and stores them in a member variable.
   * Also stores the indices into that vector with for each loop.
   */
  void store_normal_coords_and_indices();
  /* Get normals calculate by store_normal_coords_and_indices. */
  const Vector<float3> &get_normal_coords() const
  {
    return normal_coords_;
  }
  /**
   * Calculate a polygon's polygon/loop normal indices.
   * \param poly_index: Index of the polygon to calculate indices for.
   * \return Vector of normal indices, aligned with vertices of polygon.
   */
  Vector<int> calc_poly_normal_indices(int poly_index) const;
  /**
   * Find the most representative vertex group of a polygon.
   *
   * This adds up vertex group weights, and the group with the largest
   * weight sum across the polygon is the one returned.
   *
   * group_weights is temporary storage to avoid reallocations, it must
   * be the size of amount of vertex groups in the object.
   */
  int16_t get_poly_deform_group_index(int poly_index, MutableSpan<float> group_weights) const;
  /**
   * Find the name of the vertex deform group at the given index.
   * The index indices into the #Object.defbase.
   */
  const char *get_poly_deform_group_name(int16_t def_group_index) const;

  /**
   * Calculate vertex indices of an edge's corners if it is a loose edge.
   */
  std::optional<std::array<int, 2>> calc_loose_edge_vert_indices(int edge_index) const;

  /**
   * Calculate the order in which the polygons should be written into the file (sorted by material
   * index).
   */
  void calc_poly_order();

  /**
   * Remap polygon index according to polygon writing order.
   * When materials are not being written, the polygon order array
   * might be empty, in which case remap is a no-op.
   */
  int remap_poly_index(int i) const
  {
    return i < 0 || i >= poly_order_.size() ? i : poly_order_[i];
  }

 private:
  /**
   * Free the mesh if _the exporter_ created it.
   */
  void free_mesh_if_needed();
  /**
   * Allocate a new Mesh with triangulated polygons.
   *
   * The returned mesh can be the same as the old one.
   * \return Owning pointer to the new Mesh, and whether a new Mesh was created.
   */
  std::pair<Mesh *, bool> triangulate_mesh_eval();
  /**
   * Set the final transform after applying axes settings and an Object's world transform.
   */
  void set_world_axes_transform(eTransformAxisForward forward, eTransformAxisUp up);
};
}  // namespace blender::io::obj
