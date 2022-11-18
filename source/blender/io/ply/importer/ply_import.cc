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
  printf("TEST Import \n");
  //copyed from the STL importer
  FILE *file = BLI_fopen(import_params.filepath, "rb");
  if (!file) {
    fprintf(stderr, "Failed to open PLY file:'%s'.\n", import_params.filepath);
    return;
  }
  BLI_SCOPED_DEFER([&]() { fclose(file); });

  /* Detect STL file type by comparing file size with expected file size,
   * could check if file starts with "solid", but some files do not adhere,
   * this is the same as the old Python importer.
   */

  const size_t BINARY_HEADER_SIZE = 80;
  const size_t BINARY_STRIDE = 12 * 4 + 2;

  uint32_t num_tri = 0;
  size_t file_size = BLI_file_size(import_params.filepath);
  fseek(file, BINARY_HEADER_SIZE, SEEK_SET);
  if (fread(&num_tri, sizeof(uint32_t), 1, file) != 1) {
    ply_import_report_error(file);
    return;
  }
  bool is_ascii_stl = (file_size != (BINARY_HEADER_SIZE + 4 + BINARY_STRIDE * num_tri));

  /* Name used for both mesh and object. */
  char ob_name[FILE_MAX];
  BLI_strncpy(ob_name, BLI_path_basename(import_params.filepath), FILE_MAX);
  BLI_path_extension_replace(ob_name, FILE_MAX, "");
}
}  // namespace blender::io::ply
