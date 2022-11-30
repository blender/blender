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
#include "ply_import.hh"
#include "ply_import_big_endian.hh"

namespace blender::io::ply {

void ply_import_report_error(FILE *file)
{
  fprintf(stderr, "STL Importer: failed to read file");
  if (feof(file)) {
    fprintf(stderr, ", end of file reached.\n");
  }
  else if (ferror(file)) {
    perror("Error");
  }
}

void splitstr(std::string str, std::vector<std::string> &words, std::string deli = " ")
{
  int pos = 0;
  int end = str.find(deli);

  while ((pos = str.find(deli)) != std::string::npos) {
    words.push_back(str.substr(0, pos));
    str.erase(0, pos + deli.length());
  }
  // adds the final word to the vector
  words.push_back(str.substr());
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
  std::ifstream infile(import_params.filepath);

  PlyHeader header;

  // hier kan ook het PlyDataStruct gevuld worden
  while (std::getline(infile, line)) {
    header.header_size++;
    std::vector<std::string> words{};
    splitstr(line, words);

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
      if (strcmp(words[1].c_str(), "list") == 0) { // TODO: Support list properties
        continue;
      }
      std::pair<std::string, PlyDataTypes> property;
      property.first = words[2];

      if (strcmp(words[1].c_str(), "uchar") == 0) {
        property.second = PlyDataTypes::UCHAR;
      }
      else if (strcmp(words[1].c_str(), "char") == 0) {
        property.second = PlyDataTypes::CHAR;
      }
      else if (strcmp(words[1].c_str(), "short") == 0) {
        property.second = PlyDataTypes::SHORT;
      }
      else if (strcmp(words[1].c_str(), "ushort") == 0) {
        property.second = PlyDataTypes::USHORT;
      }
      else if (strcmp(words[1].c_str(), "int") == 0) {
        property.second = PlyDataTypes::UINT;
      }
      else if (strcmp(words[1].c_str(), "uint") == 0) {
        property.second = PlyDataTypes::UINT;
      }
      else if (strcmp(words[1].c_str(), "float") == 0) {
        property.second = PlyDataTypes::FLOAT;
      }
      else if (strcmp(words[1].c_str(), "double") == 0) {
        property.second = PlyDataTypes::DOUBLE;
      }

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

  Mesh *mesh = nullptr;
  if (header.type == PlyFormatType::ASCII) {
    printf("ASCII PLY \n");
  }
  else if (header.type == PlyFormatType::BINARY_BE) {
    printf("Binary Big Endian \n");
    import_ply_big_endian(infile, &header);
  }
  else {
    printf("Binary Little Endian \n");
  }
}
}  // namespace blender::io::ply
