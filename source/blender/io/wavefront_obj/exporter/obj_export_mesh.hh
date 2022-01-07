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

/** \file
 * \ingroup obj
 */

#pragma once

#include <optional>

#include "BLI_float3.hh"
#include "BLI_utility_mixins.hh"
#include "BLI_vector.hh"

#include "bmesh.h"
#include "bmesh_tools.h"

#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"

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
  Object *export_object_eval_;
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

  /**
   * Total UV vertices in a mesh's texture map.
   */
  int tot_uv_vertices_ = 0;
  /**
   * Per-polygon-per-vertex UV vertex indices.
   */
  Vector<Vector<int>> uv_indices_;
  /**
   * Total smooth groups in an object.
   */
  int tot_smooth_groups_ = NEGATIVE_INIT;
  /**
   * Polygon aligned array of their smooth groups.
   */
  int *poly_smooth_groups_ = nullptr;

 public:
  /**
   * Store evaluated Object and Mesh pointers. Conditionally triangulate a mesh, or
   * create a new Mesh from a Curve.
   */
  OBJMesh(Depsgraph *depsgraph, const OBJExportParams &export_params, Object *mesh_object);
  ~OBJMesh();

  int tot_vertices() const;
  int tot_polygons() const;
  int tot_uv_vertices() const;
  int tot_edges() const;

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
   *
   * \note Also store the UV vertex indices in the member variable.
   */
  void store_uv_coords_and_indices(Vector<std::array<float, 2>> &r_uv_coords);
  Span<int> calc_poly_uv_indices(int poly_index) const;
  /**
   * Calculate polygon normal of a polygon at given index.
   *
   * Should be used for flat-shaded polygons.
   */
  float3 calc_poly_normal(int poly_index) const;
  /**
   * Calculate a polygon's polygon/loop normal indices.
   * \param object_tot_prev_normals Number of normals of this Object written so far.
   * \return Number of distinct normal indices.
   */
  std::pair<int, Vector<int>> calc_poly_normal_indices(int poly_index,
                                                       int object_tot_prev_normals) const;
  /**
   * Calculate loop normals of a polygon at the given index.
   *
   * Should be used for smooth-shaded polygons.
   */
  void calc_loop_normals(int poly_index, Vector<float3> &r_loop_normals) const;
  /**
   * Find the index of the vertex group with the maximum number of vertices in a polygon.
   * The index indices into the #Object.defbase.
   *
   * If two or more groups have the same number of vertices (maximum), group name depends on the
   * implementation of #std::max_element.
   */
  int16_t get_poly_deform_group_index(int poly_index) const;
  /**
   * Find the name of the vertex deform group at the given index.
   * The index indices into the #Object.defbase.
   */
  const char *get_poly_deform_group_name(int16_t def_group_index) const;

  /**
   * Calculate vertex indices of an edge's corners if it is a loose edge.
   */
  std::optional<std::array<int, 2>> calc_loose_edge_vert_indices(int edge_index) const;

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
