/* SPDX-FileCopyrightText: 2026 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup spz
 */

#include "spz_import.hh"

#include "DNA_collection_types.h"
#include "DNA_object_types.h"
#include "DNA_pointcloud_types.h"
#include "DNA_scene_types.h"

#include "BLI_path_utils.hh"
#include "BLI_string.hh"

#include "BKE_context.hh"
#include "BKE_layer.hh"
#include "BKE_library.hh"
#include "BKE_object.hh"
#include "BKE_pointcloud.hh"
#include "BKE_report.hh"

#include "DEG_depsgraph.hh"
#include "DEG_depsgraph_build.hh"

#include "IO_spz.hh"

#include "spz_read.hh"

namespace blender::io::spz {

void importer_main(const bContext *C, const SPZImportParams &import_params)
{
  PointCloud *point_cloud = read_spz_file(import_params.filepath, import_params.reports);
  if (!point_cloud) {
    return;
  }

  Main *bmain = CTX_data_main(C);
  Scene *scene = CTX_data_scene(C);
  ViewLayer *view_layer = CTX_data_view_layer(C);

  /* Name used for both object and its data. */
  char object_name[FILE_MAX];
  STRNCPY(object_name, BLI_path_basename(import_params.filepath.c_str()));
  BLI_path_extension_strip(object_name);

  PointCloud *point_cloud_in_main = BKE_pointcloud_add(bmain, object_name);
  BKE_pointcloud_nomain_to_pointcloud(point_cloud, point_cloud_in_main);

  BKE_view_layer_base_deselect_all(*bmain, scene, view_layer);

  LayerCollection *layer_collection = BKE_layer_collection_get_active_editable(view_layer);
  if (!ID_IS_EDITABLE(layer_collection->collection)) {
    BKE_report(import_params.reports,
               RPT_WARNING,
               "Could not find an editable collection in current scene, imported data will not be "
               "instantiated");
  }

  Object *object = BKE_object_add_only_object(bmain, OB_POINTCLOUD, object_name);
  object->data = id_cast<ID *>(point_cloud_in_main);
  BKE_collection_object_add(bmain, layer_collection->collection, object);
  BKE_view_layer_synced_ensure(*bmain, scene, view_layer);
  if (Base *base = BKE_view_layer_base_find(view_layer, object)) {
    /* `base` will be nullptr if the Object could not be instantiated in the current view layer. */
    BKE_view_layer_base_select_and_set_active(view_layer, base);
  }

  DEG_id_tag_update(&layer_collection->collection->id, ID_RECALC_SYNC_TO_EVAL);
  DEG_id_tag_update_ex(bmain,
                       &object->id,
                       ID_RECALC_TRANSFORM | ID_RECALC_GEOMETRY | ID_RECALC_ANIMATION |
                           ID_RECALC_BASE_FLAGS);
  DEG_id_tag_update(&scene->id, ID_RECALC_BASE_FLAGS);
  DEG_relations_tag_update(bmain);
}

PointCloud *import_point_cloud(const SPZImportParams &import_params)
{
  return read_spz_file(import_params.filepath, import_params.reports);
}

}  // namespace blender::io::spz
