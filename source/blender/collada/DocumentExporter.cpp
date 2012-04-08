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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/DocumentExporter.cpp
 *  \ingroup collada
 */

#include <stdlib.h>
#include <stdio.h>
#include <math.h>

extern "C" 
{
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
#include "DNA_armature_types.h"
#include "DNA_modifier_types.h"
#include "DNA_userdef_types.h"

#include "BKE_DerivedMesh.h"
#include "BKE_fcurve.h"
#include "BKE_animsys.h"
#include "BLI_path_util.h"
#include "BLI_fileops.h"
#include "ED_keyframing.h"
#ifdef WITH_BUILDINFO
extern char build_rev[];
#endif
}

#include "MEM_guardedalloc.h"

#include "BKE_blender.h" // version info
#include "BKE_scene.h"
#include "BKE_global.h"
#include "BKE_main.h"
#include "BKE_material.h"
#include "BKE_action.h" // pose functions
#include "BKE_armature.h"
#include "BKE_image.h"
#include "BKE_utildefines.h"
#include "BKE_object.h"

#include "BLI_math.h"
#include "BLI_string.h"
#include "BLI_listbase.h"

#include "RNA_access.h"

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

#include "collada_internal.h"
#include "DocumentExporter.h"
#include "ExportSettings.h"

// can probably go after refactor is complete
#include "InstanceWriter.h"
#include "TransformWriter.h"

#include "SceneExporter.h"
#include "ArmatureExporter.h"
#include "AnimationExporter.h"
#include "CameraExporter.h"
#include "EffectExporter.h"
#include "GeometryExporter.h"
#include "ImageExporter.h"
#include "LightExporter.h"
#include "MaterialExporter.h"

#include <vector>
#include <algorithm> // std::find

char *bc_CustomData_get_layer_name(const struct CustomData *data, int type, int n)
{
	int layer_index = CustomData_get_layer_index(data, type);
	if (layer_index < 0) return NULL;

	return data->layers[layer_index+n].name;
}

char *bc_CustomData_get_active_layer_name(const CustomData *data, int type)
{
	/* get the layer index of the active layer of type */
	int layer_index = CustomData_get_active_layer_index(data, type);
	if (layer_index < 0) return NULL;

	return data->layers[layer_index].name;
}

DocumentExporter::DocumentExporter(const ExportSettings *export_settings) : export_settings(export_settings) {}

// TODO: it would be better to instantiate animations rather than create a new one per object
// COLLADA allows this through multiple <channel>s in <animation>.
// For this to work, we need to know objects that use a certain action.

void DocumentExporter::exportCurrentScene(Scene *sce)
{
	PointerRNA sceneptr, unit_settings;
	PropertyRNA *system; /* unused , *scale; */

	clear_global_id_map();
	
	COLLADABU::NativeString native_filename =
		COLLADABU::NativeString(std::string(this->export_settings->filepath));
	COLLADASW::StreamWriter sw(native_filename);

	// open <collada>
	sw.startDocument();

	// <asset>
	COLLADASW::Asset asset(&sw);

	RNA_id_pointer_create(&(sce->id), &sceneptr);
	unit_settings = RNA_pointer_get(&sceneptr, "unit_settings");
	system = RNA_struct_find_property(&unit_settings, "system");
	//scale = RNA_struct_find_property(&unit_settings, "scale_length");

	std::string unitname = "meter";
	float linearmeasure = RNA_float_get(&unit_settings, "scale_length");

	switch(RNA_property_enum_get(&unit_settings, system)) {
		case USER_UNIT_NONE:
		case USER_UNIT_METRIC:
			if (linearmeasure == 0.001f) {
				unitname = "millimeter";
			}
			else if (linearmeasure == 0.01f) {
				unitname = "centimeter";
			}
			else if (linearmeasure == 0.1f) {
				unitname = "decimeter";
			}
			else if (linearmeasure == 1.0f) {
				unitname = "meter";
			}
			else if (linearmeasure == 1000.0f) {
				unitname = "kilometer";
			}
			break;
		case USER_UNIT_IMPERIAL:
			if (linearmeasure == 0.0254f) {
				unitname = "inch";
			}
			else if (linearmeasure == 0.3048f) {
				unitname = "foot";
			}
			else if (linearmeasure == 0.9144f) {
				unitname = "yard";
			}
			break;
		default:
			break;
	}

	asset.setUnit(unitname, linearmeasure);
	asset.setUpAxisType(COLLADASW::Asset::Z_UP);
	if (U.author[0] != '\0') {
		asset.getContributor().mAuthor = U.author;
	}
	else {
		asset.getContributor().mAuthor = "Blender User";
	}
	char version_buf[128];
#ifdef WITH_BUILDINFO
	sprintf(version_buf, "Blender %d.%02d.%d r%s", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION, build_rev);
#else
	sprintf(version_buf, "Blender %d.%02d.%d", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION);
#endif
	asset.getContributor().mAuthoringTool = version_buf;
	asset.add();
	
	// <library_cameras>
	if (has_object_type(sce, OB_CAMERA)) {
		CamerasExporter ce(&sw, this->export_settings);
		ce.exportCameras(sce);
	}
	
	// <library_lights>
	if (has_object_type(sce, OB_LAMP)) {
		LightsExporter le(&sw, this->export_settings);
		le.exportLights(sce);
	}

	// <library_images>
	ImagesExporter ie(&sw, this->export_settings);
	ie.exportImages(sce);
	
	// <library_effects>
	EffectsExporter ee(&sw, this->export_settings);
	ee.exportEffects(sce);
	
	// <library_materials>
	MaterialsExporter me(&sw, this->export_settings);
	me.exportMaterials(sce);

	// <library_geometries>
	if (has_object_type(sce, OB_MESH)) {
		GeometryExporter ge(&sw, this->export_settings);
		ge.exportGeom(sce);
	}

	// <library_animations>
	AnimationExporter ae(&sw, this->export_settings);
	ae.exportAnimations(sce);

	// <library_controllers>
	ArmatureExporter arm_exporter(&sw, this->export_settings);
	if (has_object_type(sce, OB_ARMATURE)) {
		arm_exporter.export_controllers(sce);
	}

	// <library_visual_scenes>
	SceneExporter se(&sw, &arm_exporter, this->export_settings);
	se.exportScene(sce);
	
	// <scene>
	std::string scene_name(translate_id(id_name(sce)));
	COLLADASW::Scene scene(&sw, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING,
											   scene_name));
	scene.add();
	
	// close <Collada>
	sw.endDocument();

}

void DocumentExporter::exportScenes(const char* filename)
{
}

/*

NOTES:

* AnimationExporter::sample_animation enables all curves on armature, this is undesirable for a user

 */
