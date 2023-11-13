/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include "COLLADASWLibraryCameras.h"
#include "COLLADASWStreamWriter.h"

#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "DNA_camera_types.h"
#include "ExportSettings.h"

class CamerasExporter : COLLADASW::LibraryCameras {
 public:
  CamerasExporter(COLLADASW::StreamWriter *sw, BCExportSettings &export_settings);
  void exportCameras(Scene *sce);
  void operator()(Object *ob, Scene *sce);

 private:
  bool exportBlenderProfile(COLLADASW::Camera &cm, Camera *cam);
  BCExportSettings &export_settings;
};
