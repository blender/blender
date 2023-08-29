/* SPDX-FileCopyrightText: 2023 Blender Authors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later */

/** \file
 * \ingroup collada
 */

#pragma once

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "DNA_action_types.h"
#include "DNA_anim_types.h"
#include "DNA_armature_types.h"
#include "DNA_collection_types.h"
#include "DNA_constraint_types.h"
#include "DNA_curve_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_mesh_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"
#include "DNA_texture_types.h"
#include "DNA_userdef_types.h"

#include "BKE_constraint.h"
#include "BKE_fcurve.h"
#include "BLI_fileops.h"
#include "BLI_path_util.h"
#include "ED_keyframing.hh"

#include "COLLADASWAsset.h"
#include "COLLADASWBaseInputElement.h"
#include "COLLADASWBindMaterial.h"
#include "COLLADASWColorOrTexture.h"
#include "COLLADASWConstants.h"
#include "COLLADASWEffectProfile.h"
#include "COLLADASWImage.h"
#include "COLLADASWInputList.h"
#include "COLLADASWInstanceCamera.h"
#include "COLLADASWInstanceController.h"
#include "COLLADASWInstanceGeometry.h"
#include "COLLADASWInstanceLight.h"
#include "COLLADASWInstanceNode.h"
#include "COLLADASWLibraryAnimations.h"
#include "COLLADASWLibraryControllers.h"
#include "COLLADASWLibraryEffects.h"
#include "COLLADASWLibraryImages.h"
#include "COLLADASWLibraryMaterials.h"
#include "COLLADASWLibraryVisualScenes.h"
#include "COLLADASWNode.h"
#include "COLLADASWParamBase.h"
#include "COLLADASWParamTemplate.h"
#include "COLLADASWPrimitves.h"
#include "COLLADASWSampler.h"
#include "COLLADASWScene.h"
#include "COLLADASWSource.h"
#include "COLLADASWSurfaceInitOption.h"
#include "COLLADASWTechnique.h"
#include "COLLADASWTexture.h"
#include "COLLADASWVertices.h"

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
