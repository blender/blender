/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include <string>
#include <vector>

#include "COLLADASWLibraryImages.h"
#include "COLLADASWStreamWriter.h"

#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "ExportSettings.h"
#include "collada_utils.h"

class ImagesExporter : COLLADASW::LibraryImages {
 public:
  ImagesExporter(COLLADASW::StreamWriter *sw,
                 BCExportSettings &export_settings,
                 KeyImageMap &key_image_map);
  void exportImages(Scene *sce);

 private:
  BCExportSettings &export_settings;
  KeyImageMap &key_image_map;
  void export_UV_Image(Image *image, bool use_copies);
};
