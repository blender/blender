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

  void write_object_name(const OBJMesh &obj_mesh_data) const;
  void write_object_group(const OBJMesh &obj_mesh_data) const;
  void write_mtllib_name(const StringRefNull mtl_filepath) const;
  void write_vertex_coords(const OBJMesh &obj_mesh_data) const;
  void write_uv_coords(OBJMesh &obj_mesh_data) const;
  void write_poly_normals(const OBJMesh &obj_mesh_data) const;
  int write_smooth_group(const OBJMesh &obj_mesh_data,
                         int poly_index,
                         const int last_poly_smooth_group) const;
  int16_t write_poly_material(const OBJMesh &obj_mesh_data,
                              const int poly_index,
                              const int16_t last_poly_mat_nr,
                              std::function<const char *(int)> matname_fn) const;
  int16_t write_vertex_group(const OBJMesh &obj_mesh_data,
                             const int poly_index,
                             const int16_t last_poly_vertex_group) const;
  void write_poly_elements(const OBJMesh &obj_mesh_data,
                           std::function<const char *(int)> matname_fn);
  void write_edges_indices(const OBJMesh &obj_mesh_data) const;
  void write_nurbs_curve(const OBJCurve &obj_nurbs_data) const;

  void update_index_offsets(const OBJMesh &obj_mesh_data);

 private:
  using func_vert_uv_normal_indices = void (OBJWriter::*)(Span<int> vert_indices,
                                                          Span<int> uv_indices,
                                                          Span<int> normal_indices) const;
  func_vert_uv_normal_indices get_poly_element_writer(const int total_uv_vertices) const;

  void write_vert_uv_normal_indices(Span<int> vert_indices,
                                    Span<int> uv_indices,
                                    Span<int> normal_indices) const;
  void write_vert_normal_indices(Span<int> vert_indices,
                                 Span<int> /*uv_indices*/,
                                 Span<int> normal_indices) const;
  void write_vert_uv_indices(Span<int> vert_indices,
                             Span<int> uv_indices,
                             Span<int> /*normal_indices*/) const;
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
  MTLWriter(const char *obj_filepath) noexcept(false);

  void write_header(const char *blen_filepath) const;
  void write_materials();
  StringRefNull mtl_file_path() const;
  Vector<int> add_materials(const OBJMesh &mesh_to_export);
  const char *mtlmaterial_name(int index);

 private:
  void write_bsdf_properties(const MTLMaterial &mtl_material);
  void write_texture_map(const MTLMaterial &mtl_material,
                         const Map<const eMTLSyntaxElement, tex_map_XX>::Item &texture_map);
};
}  // namespace blender::io::obj
