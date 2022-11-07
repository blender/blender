/* SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup stl
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

}
}  // namespace blender::io::ply
