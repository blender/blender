/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "IO_wavefront_obj.hh"

#include "BLI_map.hh"
#include "BLI_vector.hh"

#include "obj_import_objects.hh"

namespace blender {
struct BLI_mmap_file;
}

namespace blender::io::obj {

struct MTLMaterial;

class OBJParser {
 private:
  const OBJImportParams &import_params_;
  Vector<std::string> mtl_libraries_;
  BLI_mmap_file *mmap_file_ = nullptr;
  std::string line_buffer_;

 public:
  /**
   * Open OBJ file at the path given in import parameters.
   */
  OBJParser(const OBJImportParams &import_params);
  ~OBJParser();

  /**
   * Read the OBJ file line by line and create OBJ Geometry instances. Also store all the vertex
   * and UV vertex coordinates in a struct accessible by all objects.
   */
  void parse(Vector<std::unique_ptr<Geometry>> &r_all_geometries,
             GlobalVertices &r_global_vertices);
  /**
   * Return a list of all material library filepaths referenced by the OBJ file.
   */
  Span<std::string> mtl_libraries() const;

 private:
  void add_mtl_library(StringRef path);
  void add_default_mtl_library();
  StringRef read_next_obj_line(StringRef &buffer);
  void parse_string_buffer(StringRef &buffer_str,
                           Vector<std::unique_ptr<Geometry>> &r_all_geometries,
                           GlobalVertices &r_global_vertices,
                           Geometry *&curr_geom);
};

class MTLParser {
 private:
  char mtl_file_path_[FILE_MAX];
  /**
   * Directory in which the MTL file is found.
   */
  char mtl_dir_path_[FILE_MAX];

 public:
  /**
   * Open material library file.
   */
  MTLParser(StringRefNull mtl_library_, StringRefNull obj_filepath);

  /**
   * Read MTL file(s) and add MTLMaterial instances to the given Map reference.
   */
  void parse_and_store(Map<std::string, std::unique_ptr<MTLMaterial>> &r_materials);
};
}  // namespace blender::io::obj
