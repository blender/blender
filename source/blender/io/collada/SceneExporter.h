/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include <cstdlib>

#include "DNA_object_types.h"

#include "COLLADASWLibraryVisualScenes.h"

#include "ArmatureExporter.h"
#include "ExportSettings.h"

class SceneExporter : COLLADASW::LibraryVisualScenes,
                      protected TransformWriter,
                      protected InstanceWriter {
 public:
  SceneExporter(BlenderContext &blender_context,
                COLLADASW::StreamWriter *sw,
                ArmatureExporter *arm,
                BCExportSettings &export_settings)
      : COLLADASW::LibraryVisualScenes(sw),
        blender_context(blender_context),
        arm_exporter(arm),
        export_settings(export_settings)
  {
  }

  void exportScene();

 private:
  BlenderContext &blender_context;
  friend class ArmatureExporter;
  ArmatureExporter *arm_exporter;
  BCExportSettings &export_settings;

  void exportHierarchy();
  void writeNodeList(std::vector<Object *> &child_objects, Object *parent);
  void writeNode(Object *ob);
};
