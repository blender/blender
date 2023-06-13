/* SPDX-FileCopyrightText: 2023 Blender Foundation
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "COLLADASWLibraryAnimationClips.h"
#include "DEG_depsgraph.h"
#include "ExportSettings.h"

class AnimationClipExporter : COLLADASW::LibraryAnimationClips {
 private:
  Depsgraph *depsgraph;
  Scene *scene;
  COLLADASW::StreamWriter *sw;
  BCExportSettings &export_settings;
  std::vector<std::vector<std::string>> anim_meta;

 public:
  AnimationClipExporter(Depsgraph *depsgraph,
                        COLLADASW::StreamWriter *sw,
                        BCExportSettings &export_settings,
                        std::vector<std::vector<std::string>> anim_meta)
      : COLLADASW::LibraryAnimationClips(sw),
        depsgraph(depsgraph),
        scene(nullptr),
        sw(sw),
        export_settings(export_settings),
        anim_meta(anim_meta)
  {
  }

  void exportAnimationClips(Scene *sce);
};
