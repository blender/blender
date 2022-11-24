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

#include "ply_import.hh"


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
  PlyFormatType type;

  //hier kan ook het PlyDataStruct gevuld worden
  while (std::getline(infile, line)){

    if (strcmp(line.c_str(), "format ascii 1.0")== 0) {
      type = ascii;
      
    }else
    if (strcmp(line.c_str(), "format binary_big_endian 1.0") == 0) {
      type = binary_big_endian;
      
    }else
    if (strcmp(line.c_str(), "format binary_little_endian 1.0") == 0) {
      type = binary_little_endian;
    }
    if (line == "end_header") {
      break;
    }   
  }

  /* Name used for both mesh and object. */
  char ob_name[FILE_MAX];
  BLI_strncpy(ob_name, BLI_path_basename(import_params.filepath), FILE_MAX);
  BLI_path_extension_replace(ob_name, FILE_MAX, "");

  if(type == ascii){
    printf("ASCII PLY \n");
  }
  else if (type == binary_big_endian) {
    printf("Binary Big Endian \n");
  }
  else{
    printf("Binary Little Endian \n");
  }
}
}  // namespace blender::io::ply
