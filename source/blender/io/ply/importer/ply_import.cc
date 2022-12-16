/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup ply
 */

#include <cstdio>

#include "BKE_customdata.h"
#include "BKE_layer.h"
#include "BKE_mesh.h"
#include "BKE_object.h"

#include "DNA_collection_types.h"
#include "DNA_scene_types.h"

#include "BLI_fileops.hh"
#include "BLI_math_vector.h"
#include "BLI_memory_utils.hh"

#include "DNA_object_types.h"

#include "DEG_depsgraph.h"
#include "DEG_depsgraph_build.h"

#include "intern/ply_data.hh"
#include "ply_functions.hh"
#include "ply_import.hh"
#include "ply_import_ascii.hh"
#include "ply_import_binary.hh"

namespace blender::io::ply {

void ply_import_report_error(FILE *file)
{
  fprintf(stderr, "PLY Importer: failed to read file");
  if (feof(file)) {
    fprintf(stderr, ", end of file reached.\n");
  }
  else if (ferror(file)) {
    perror("Error");
  }
}

void splitstr(std::string str, std::vector<std::string> &words, const std::string &deli)
{
  int pos = 0;

  while ((pos = int(str.find(deli))) != std::string::npos) {
    words.push_back(str.substr(0, pos));
    str.erase(0, pos + deli.length());
  }
  /* We add the final word to the vector. */
  words.push_back(str.substr());
}

enum PlyDataTypes from_string(const std::string &input)
{
  if (input == "uchar") {
    return PlyDataTypes::UCHAR;
  }
  if (input == "char") {
    return PlyDataTypes::CHAR;
  }
  if (input == "ushort") {
    return PlyDataTypes::USHORT;
  }
  if (input == "short") {
    return PlyDataTypes::SHORT;
  }
  if (input == "uint") {
    return PlyDataTypes::UINT;
  }
  if (input == "int") {
    return PlyDataTypes::INT;
  }
  if (input == "float") {
    return PlyDataTypes::FLOAT;
  }
  if (input == "double") {
    return PlyDataTypes::DOUBLE;
  }
  return PlyDataTypes::FLOAT;
}

void importer_main(bContext *C, const PLYImportParams &import_params)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  importer_main(bmain, scene, view_layer, import_params);
}

void importer_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const PLYImportParams &import_params)
{

  FILE *file = BLI_fopen(import_params.filepath, "rb");
  if (!file) {
    fprintf(stderr, "Failed to open PLY file:'%s'.\n", import_params.filepath);
    return;
  }
  BLI_SCOPED_DEFER([&]() { fclose(file); });

  std::string line;
  std::ifstream infile(import_params.filepath, std::ios::binary);

  PlyHeader header;

  while (true) {  // We break when end_header is encountered
    safe_getline(infile, line);
    header.header_size++;
    std::vector<std::string> words{};
    splitstr(line, words, " ");

    if (strcmp(words[0].c_str(), "format") == 0) {
      if (strcmp(words[1].c_str(), "ascii") == 0) {
        header.type = PlyFormatType::ASCII;
      }
      else if (strcmp(words[1].c_str(), "binary_big_endian") == 0) {
        header.type = PlyFormatType::BINARY_BE;
      }
      else if (strcmp(words[1].c_str(), "binary_little_endian") == 0) {
        header.type = PlyFormatType::BINARY_LE;
      }
    }
    else if (strcmp(words[0].c_str(), "element") == 0) {
      if (strcmp(words[1].c_str(), "vertex") == 0) {
        header.vertex_count = std::stoi(words[2]);
      }
      else if (strcmp(words[1].c_str(), "face") == 0) {
        header.face_count = std::stoi(words[2]);
      }
      else if (strcmp(words[1].c_str(), "edge") == 0) {
        header.edge_count = std::stoi(words[2]);
      }
    }
    else if (strcmp(words[0].c_str(), "property") == 0) {
      if (strcmp(words[1].c_str(), "list") == 0) {
        header.vertex_index_count_type = from_string(words[2]);
        header.vertex_index_type = from_string(words[3]);
        continue;
      }
      std::pair<std::string, PlyDataTypes> property;
      property.first = words[2];
      property.second = from_string(words[1]);

      header.properties.push_back(property);
    }
    else if (words[0] == "end_header") {
      break;
    }
  }

  /* Name used for both mesh and object. */
  char ob_name[FILE_MAX];
  BLI_strncpy(ob_name, BLI_path_basename(import_params.filepath), FILE_MAX);
  BLI_path_extension_replace(ob_name, FILE_MAX, "");

  Mesh *mesh = BKE_mesh_add(bmain, ob_name);
  if (header.type == PlyFormatType::ASCII) {
    mesh = import_ply_ascii(infile, &header, mesh);
  }
  else if (header.type == PlyFormatType::BINARY_BE) {
    mesh = import_ply_binary(infile, &header, mesh);
  }
  else {
    mesh = import_ply_binary(infile, &header, mesh);
  }

  BKE_view_layer_base_deselect_all(scene, view_layer);
  LayerCollection *lc = BKE_layer_collection_get_active(view_layer);
  Object *obj = BKE_object_add_only_object(bmain, OB_MESH, ob_name);
  BKE_mesh_assign_object(bmain, obj, mesh);
  BKE_collection_object_add(bmain, lc->collection, obj);
  BKE_view_layer_synced_ensure(scene, view_layer);
  Base *base = BKE_view_layer_base_find(view_layer, obj);
  BKE_view_layer_base_select_and_set_active(view_layer, base);

  float global_scale = import_params.global_scale;
  if ((scene->unit.system != USER_UNIT_NONE) && import_params.use_scene_unit) {
    global_scale *= scene->unit.scale_length;
  }
  float scale_vec[3] = {global_scale, global_scale, global_scale};
  float obmat3x3[3][3];
  unit_m3(obmat3x3);
  float obmat4x4[4][4];
  unit_m4(obmat4x4);
  /* +Y-forward and +Z-up are the Blender's default axis settings. */
  mat3_from_axis_conversion(
      IO_AXIS_Y, IO_AXIS_Z, import_params.forward_axis, import_params.up_axis, obmat3x3);
  copy_m4_m3(obmat4x4, obmat3x3);
  rescale_m4(obmat4x4, scale_vec);
  BKE_object_apply_mat4(obj, obmat4x4, true, false);

  DEG_id_tag_update(&lc->collection->id, ID_RECALC_COPY_ON_WRITE);
  int flags = ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION |
              ID_RECALC_BASE_FLAGS;
  DEG_id_tag_update_ex(bmain, &obj->id, flags);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  DEG_relations_tag_update(bmain);

  infile.close();
}
}  // namespace blender::io::ply
