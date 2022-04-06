/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup obj
 */

#pragma once

#include "BLI_fileops.hh"
#include "IO_wavefront_obj.h"
#include "obj_import_mtl.hh"
#include "obj_import_objects.hh"

namespace blender::io::obj {

/* Note: the OBJ parser implementation is planned to get fairly large changes "soon",
 * so don't read too much into current implementation... */
class OBJParser {
 private:
  const OBJImportParams &import_params_;
  blender::fstream obj_file_;
  Vector<std::string> mtl_libraries_;

 public:
  /**
   * Open OBJ file at the path given in import parameters.
   */
  OBJParser(const OBJImportParams &import_params);

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
};

enum class eOBJLineKey {
  V,
  VN,
  VT,
  F,
  L,
  CSTYPE,
  DEG,
  CURV,
  PARM,
  O,
  G,
  S,
  USEMTL,
  MTLLIB,
  COMMENT
};

constexpr eOBJLineKey line_key_str_to_enum(const std::string_view key_str)
{
  if (key_str == "v" || key_str == "V") {
    return eOBJLineKey::V;
  }
  if (key_str == "vn" || key_str == "VN") {
    return eOBJLineKey::VN;
  }
  if (key_str == "vt" || key_str == "VT") {
    return eOBJLineKey::VT;
  }
  if (key_str == "f" || key_str == "F") {
    return eOBJLineKey::F;
  }
  if (key_str == "l" || key_str == "L") {
    return eOBJLineKey::L;
  }
  if (key_str == "cstype" || key_str == "CSTYPE") {
    return eOBJLineKey::CSTYPE;
  }
  if (key_str == "deg" || key_str == "DEG") {
    return eOBJLineKey::DEG;
  }
  if (key_str == "curv" || key_str == "CURV") {
    return eOBJLineKey::CURV;
  }
  if (key_str == "parm" || key_str == "PARM") {
    return eOBJLineKey::PARM;
  }
  if (key_str == "o" || key_str == "O") {
    return eOBJLineKey::O;
  }
  if (key_str == "g" || key_str == "G") {
    return eOBJLineKey::G;
  }
  if (key_str == "s" || key_str == "S") {
    return eOBJLineKey::S;
  }
  if (key_str == "usemtl" || key_str == "USEMTL") {
    return eOBJLineKey::USEMTL;
  }
  if (key_str == "mtllib" || key_str == "MTLLIB") {
    return eOBJLineKey::MTLLIB;
  }
  if (key_str == "#") {
    return eOBJLineKey::COMMENT;
  }
  return eOBJLineKey::COMMENT;
}

/**
 * All texture map options with number of arguments they accept.
 */
class TextureMapOptions {
 private:
  Map<const std::string, int> tex_map_options;

 public:
  TextureMapOptions()
  {
    tex_map_options.add_new("-blendu", 1);
    tex_map_options.add_new("-blendv", 1);
    tex_map_options.add_new("-boost", 1);
    tex_map_options.add_new("-mm", 2);
    tex_map_options.add_new("-o", 3);
    tex_map_options.add_new("-s", 3);
    tex_map_options.add_new("-t", 3);
    tex_map_options.add_new("-texres", 1);
    tex_map_options.add_new("-clamp", 1);
    tex_map_options.add_new("-bm", 1);
    tex_map_options.add_new("-imfchan", 1);
  }

  /**
   * All valid option strings.
   */
  Map<const std::string, int>::KeyIterator all_options() const
  {
    return tex_map_options.keys();
  }

  int number_of_args(StringRef option) const
  {
    return tex_map_options.lookup_as(std::string(option));
  }
};

class MTLParser {
 private:
  char mtl_file_path_[FILE_MAX];
  /**
   * Directory in which the MTL file is found.
   */
  char mtl_dir_path_[FILE_MAX];
  blender::fstream mtl_file_;

 public:
  /**
   * Open material library file.
   */
  MTLParser(StringRef mtl_library_, StringRefNull obj_filepath);

  /**
   * Read MTL file(s) and add MTLMaterial instances to the given Map reference.
   */
  void parse_and_store(Map<std::string, std::unique_ptr<MTLMaterial>> &r_mtl_materials);
};
}  // namespace blender::io::obj
