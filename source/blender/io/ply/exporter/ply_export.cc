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

#include "ply_export.hh"

namespace blender::io::ply {

void exporter_main(bContext *C, const PLYExportParams &export_params)
{
  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);
  exporter_main(bmain, scene, view_layer, export_params);
}

void exporter_main(Main *bmain,
                   Scene *scene,
                   ViewLayer *view_layer,
                   const PLYExportParams &export_params)
{
  perror("Error");
}
}  // namespace blender::io::ply
