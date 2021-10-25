/*
 * ***** BEGIN GPL LICENSE BLOCK *****
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file SceneExporter.h
 *  \ingroup collada
 */

#ifndef __SCENEEXPORTER_H__
#define __SCENEEXPORTER_H__

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

extern "C" {
#include "DNA_scene_types.h"
#include "DNA_object_types.h"
#include "DNA_group_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_mesh_types.h"
#include "DNA_image_types.h"
#include "DNA_material_types.h"
#include "DNA_texture_types.h"
#include "DNA_anim_types.h"
#include "DNA_action_types.h"
#include "DNA_curve_types.h"
#include "DNA_constraint_types.h"
#include "DNA_armature_types.h"
#include "DNA_modifier_types.h"
#include "DNA_userdef_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_fcurve.h"
#include "BKE_animsys.h"
#include "BLI_path_util.h"
#include "BKE_constraint.h"
#include "BLI_fileops.h"
#include "ED_keyframing.h"
}

#include "COLLADASWAsset.h"
#include "COLLADASWLibraryVisualScenes.h"
#include "COLLADASWNode.h"
#include "COLLADASWSource.h"
#include "COLLADASWInstanceGeometry.h"
#include "COLLADASWInputList.h"
#include "COLLADASWPrimitves.h"
#include "COLLADASWVertices.h"
#include "COLLADASWLibraryAnimations.h"
#include "COLLADASWLibraryImages.h"
#include "COLLADASWLibraryEffects.h"
#include "COLLADASWImage.h"
#include "COLLADASWEffectProfile.h"
#include "COLLADASWColorOrTexture.h"
#include "COLLADASWParamTemplate.h"
#include "COLLADASWParamBase.h"
#include "COLLADASWSurfaceInitOption.h"
#include "COLLADASWSampler.h"
#include "COLLADASWScene.h"
#include "COLLADASWTechnique.h"
#include "COLLADASWTexture.h"
#include "COLLADASWLibraryMaterials.h"
#include "COLLADASWBindMaterial.h"
#include "COLLADASWInstanceCamera.h"
#include "COLLADASWInstanceLight.h"
#include "COLLADASWConstants.h"
#include "COLLADASWLibraryControllers.h"
#include "COLLADASWInstanceController.h"
#include "COLLADASWInstanceNode.h"
#include "COLLADASWBaseInputElement.h"

#include "ArmatureExporter.h"

#include "ExportSettings.h"

class SceneExporter: COLLADASW::LibraryVisualScenes, protected TransformWriter, protected InstanceWriter
{
public:
	SceneExporter(COLLADASW::StreamWriter *sw, ArmatureExporter *arm, const ExportSettings *export_settings);
	void exportScene(Scene *sce);
	void setExportTransformationType(BC_export_transformation_type transformation_type);

private:
	BC_export_transformation_type transformation_type;
	// required for writeNodes() for bone-parented objects
	friend class ArmatureExporter;
	void exportHierarchy(Scene *sce);
	void writeNodes(Object *ob, Scene *sce);
	
	ArmatureExporter *arm_exporter;
	const ExportSettings *export_settings;
};

#endif
