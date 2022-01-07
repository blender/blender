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

#include "DNA_meshdata_types.h"

#include "BLI_map.hh"
#include "BLI_vector.hh"

#include "IO_wavefront_obj.h"
#include "obj_export_io.hh"
#include "obj_export_mtl.hh"

namespace blender::io::obj {

class OBJCurve;
class OBJMesh;
/**
 * Total vertices/ UV vertices/ normals of previous Objects
 * should be added to the current Object's indices.
 */
struct IndexOffsets {
  int vertex_offset;
  int uv_vertex_offset;
  int normal_offset;
};

/**
 * Responsible for writing a .OBJ file.
 */
class OBJWriter : NonMovable, NonCopyable {
 private:
  const OBJExportParams &export_params_;
  std::unique_ptr<FileHandler<eFileType::OBJ>> file_handler_ = nullptr;
  IndexOffsets index_offsets_{0, 0, 0};

 public:
  OBJWriter(const char *filepath, const OBJExportParams &export_params) noexcept(false)
      : export_params_(export_params)
  {
    file_handler_ = std::make_unique<FileHandler<eFileType::OBJ>>(filepath);
  }

  void write_header() const;

  /**
   * Write object's name or group.
   */
  void write_object_name(const OBJMesh &obj_mesh_data) const;
  /**
   * Write an object's group with mesh and/or material name appended conditionally.
   */
  void write_object_group(const OBJMesh &obj_mesh_data) const;
  /**
   * Write file name of Material Library in .OBJ file.
   */
  void write_mtllib_name(const StringRefNull mtl_filepath) const;
  /**
   * Write vertex coordinates for all vertices as "v x y z".
   */
  void write_vertex_coords(const OBJMesh &obj_mesh_data) const;
  /**
   * Write UV vertex coordinates for all vertices as `vt u v`.
   * \note UV indices are stored here, but written later.
   */
  void write_uv_coords(OBJMesh &obj_mesh_data) const;
  /**
   * Write loop normals for smooth-shaded polygons, and polygon normals otherwise, as "vn x y z".
   */
  void write_poly_normals(const OBJMesh &obj_mesh_data) const;
  /**
   * Write smooth group if polygon at the given index is shaded smooth else "s 0"
   */
  int write_smooth_group(const OBJMesh &obj_mesh_data,
                         int poly_index,
                         int last_poly_smooth_group) const;
  /**
   * Write material name and material group of a polygon in the .OBJ file.
   * \return #mat_nr of the polygon at the given index.
   * \note It doesn't write to the material library.
   */
  int16_t write_poly_material(const OBJMesh &obj_mesh_data,
                              int poly_index,
                              int16_t last_poly_mat_nr,
                              std::function<const char *(int)> matname_fn) const;
  /**
   * Write the name of the deform group of a polygon.
   */
  int16_t write_vertex_group(const OBJMesh &obj_mesh_data,
                             int poly_index,
                             int16_t last_poly_vertex_group) const;
  /**
   * Write polygon elements with at least vertex indices, and conditionally with UV vertex
   * indices and polygon normal indices. Also write groups: smooth, vertex, material.
   * The matname_fn turns a 0-indexed material slot number in an Object into the
   * name used in the .obj file.
   * \note UV indices were stored while writing UV vertices.
   */
  void write_poly_elements(const OBJMesh &obj_mesh_data,
                           std::function<const char *(int)> matname_fn);
  /**
   * Write loose edges of a mesh as "l v1 v2".
   */
  void write_edges_indices(const OBJMesh &obj_mesh_data) const;
  /**
   * Write a NURBS curve to the .OBJ file in parameter form.
   */
  void write_nurbs_curve(const OBJCurve &obj_nurbs_data) const;

  /**
   * When there are multiple objects in a frame, the indices of previous objects' coordinates or
   * normals add up.
   */
  void update_index_offsets(const OBJMesh &obj_mesh_data);

 private:
  using func_vert_uv_normal_indices = void (OBJWriter::*)(Span<int> vert_indices,
                                                          Span<int> uv_indices,
                                                          Span<int> normal_indices) const;
  /**
   * \return Writer function with appropriate polygon-element syntax.
   */
  func_vert_uv_normal_indices get_poly_element_writer(int total_uv_vertices) const;

  /**
   * Write one line of polygon indices as "f v1/vt1/vn1 v2/vt2/vn2 ...".
   */
  void write_vert_uv_normal_indices(Span<int> vert_indices,
                                    Span<int> uv_indices,
                                    Span<int> normal_indices) const;
  /**
   * Write one line of polygon indices as "f v1//vn1 v2//vn2 ...".
   */
  void write_vert_normal_indices(Span<int> vert_indices,
                                 Span<int> /*uv_indices*/,
                                 Span<int> normal_indices) const;
  /**
   * Write one line of polygon indices as "f v1/vt1 v2/vt2 ...".
   */
  void write_vert_uv_indices(Span<int> vert_indices,
                             Span<int> uv_indices,
                             Span<int> /*normal_indices*/) const;
  /**
   * Write one line of polygon indices as "f v1 v2 ...".
   */
  void write_vert_indices(Span<int> vert_indices,
                          Span<int> /*uv_indices*/,
                          Span<int> /*normal_indices*/) const;
};

/**
 * Responsible for writing a .MTL file.
 */
class MTLWriter : NonMovable, NonCopyable {
 private:
  std::unique_ptr<FileHandler<eFileType::MTL>> file_handler_ = nullptr;
  std::string mtl_filepath_;
  Vector<MTLMaterial> mtlmaterials_;
  /* Map from a Material* to an index into mtlmaterials_. */
  Map<const Material *, int> material_map_;

 public:
  /*
   * Create the .MTL file.
   */
  MTLWriter(const char *obj_filepath) noexcept(false);

  void write_header(const char *blen_filepath) const;
  /**
   * Write all of the material specifications to the MTL file.
   * For consistency of output from run to run (useful for testing),
   * the materials are sorted by name before writing.
   */
  void write_materials();
  StringRefNull mtl_file_path() const;
  /**
   * Add the materials of the given object to #MTLWriter, de-duplicating
   * against ones that are already there.
   * Return a Vector of indices into mtlmaterials_ that hold the #MTLMaterial
   * that corresponds to each material slot, in order, of the given Object.
   * Indexes are returned rather than pointers to the MTLMaterials themselves
   * because the mtlmaterials_ Vector may move around when resized.
   */
  Vector<int> add_materials(const OBJMesh &mesh_to_export);
  const char *mtlmaterial_name(int index);

 private:
  /**
   * Write properties sourced from p-BSDF node or #Object.Material.
   */
  void write_bsdf_properties(const MTLMaterial &mtl_material);
  /**
   * Write a texture map in the form "map_XX -s 1. 1. 1. -o 0. 0. 0. [-bm 1.] path/to/image".
   */
  void write_texture_map(const MTLMaterial &mtl_material,
                         const Map<const eMTLSyntaxElement, tex_map_XX>::Item &texture_map);
};
}  // namespace blender::io::obj
