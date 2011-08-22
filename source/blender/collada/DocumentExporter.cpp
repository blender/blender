/*
 * $Id$
 *
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
#include "COLLADASWBaseInputElement.h"

#include "collada_internal.h"
#include "DocumentExporter.h"

// can probably go after refactor is complete
#include "InstanceWriter.h"
#include "TransformWriter.h"

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
	if(layer_index < 0) return NULL;

	return data->layers[layer_index+n].name;
}

char *bc_CustomData_get_active_layer_name(const CustomData *data, int type)
{
	/* get the layer index of the active layer of type */
	int layer_index = CustomData_get_active_layer_index(data, type);
	if(layer_index < 0) return NULL;

	return data->layers[layer_index].name;
}


/*
  Utilities to avoid code duplication.
  Definition can take some time to understand, but they should be useful.
*/


template<class Functor>
void forEachObjectInScene(Scene *sce, Functor &f)
{
	Base *base= (Base*) sce->base.first;
	while(base) {
		Object *ob = base->object;
			
		f(ob);

		base= base->next;
	}
}



class SceneExporter: COLLADASW::LibraryVisualScenes, protected TransformWriter, protected InstanceWriter
{
	ArmatureExporter *arm_exporter;
public:
	SceneExporter(COLLADASW::StreamWriter *sw, ArmatureExporter *arm) : COLLADASW::LibraryVisualScenes(sw),
																		arm_exporter(arm) {}
	
	void exportScene(Scene *sce, bool export_selected) {
 		// <library_visual_scenes> <visual_scene>
		std::string id_naming = id_name(sce);
		openVisualScene(translate_id(id_naming), id_naming);

		// write <node>s
		//forEachMeshObjectInScene(sce, *this);
		//forEachCameraObjectInScene(sce, *this);
		//forEachLampObjectInScene(sce, *this);
		exportHierarchy(sce, export_selected);

		// </visual_scene> </library_visual_scenes>
		closeVisualScene();

		closeLibrary();
	}

	void exportHierarchy(Scene *sce, bool export_selected)
	{
		Base *base= (Base*) sce->base.first;
		while(base) {
			Object *ob = base->object;

			if (!ob->parent) {
				if(sce->lay & ob->lay) {
				switch(ob->type) {
				case OB_MESH:
				case OB_CAMERA:
				case OB_LAMP:
				case OB_ARMATURE:
				case OB_EMPTY:
					if (export_selected && !(ob->flag & SELECT)) {
						break;
					}
					// write nodes....
					writeNodes(ob, sce);
					break;
				}
				}
			}

			base= base->next;
		}
	}


	// called for each object
	//void operator()(Object *ob) {
	void writeNodes(Object *ob, Scene *sce)
	{
		COLLADASW::Node node(mSW);
		node.setNodeId(translate_id(id_name(ob)));
		node.setType(COLLADASW::Node::NODE);

		node.start();

		bool is_skinned_mesh = arm_exporter->is_skinned_mesh(ob);

		if (ob->type == OB_MESH && is_skinned_mesh)
			// for skinned mesh we write obmat in <bind_shape_matrix>
			TransformWriter::add_node_transform_identity(node);
		else
			TransformWriter::add_node_transform_ob(node, ob);
		
		// <instance_geometry>
		if (ob->type == OB_MESH) {
			if (is_skinned_mesh) {
				arm_exporter->add_instance_controller(ob);
			}
			else {
				COLLADASW::InstanceGeometry instGeom(mSW);
				instGeom.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_geometry_id(ob)));

				InstanceWriter::add_material_bindings(instGeom.getBindMaterial(), ob);
			
				instGeom.add();
			}
		}

		// <instance_controller>
		else if (ob->type == OB_ARMATURE) {
			arm_exporter->add_armature_bones(ob, sce);

			// XXX this looks unstable...
			node.end();
		}
		
		// <instance_camera>
		else if (ob->type == OB_CAMERA) {
			COLLADASW::InstanceCamera instCam(mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_camera_id(ob)));
			instCam.add();
		}
		
		// <instance_light>
		else if (ob->type == OB_LAMP) {
			COLLADASW::InstanceLight instLa(mSW, COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, get_light_id(ob)));
			instLa.add();
		}

		// empty object
		else if (ob->type == OB_EMPTY) {
		}

		// write nodes for child objects
		Base *b = (Base*) sce->base.first;
		while(b) {
			// cob - child object
			Object *cob = b->object;

			if (cob->parent == ob) {
				switch(cob->type) {
				case OB_MESH:
				case OB_CAMERA:
				case OB_LAMP:
				case OB_EMPTY:
				case OB_ARMATURE:
					// write node...
					writeNodes(cob, sce);
					break;
				}
			}

			b = b->next;
		}

		if (ob->type != OB_ARMATURE)
			node.end();
	}
};

// TODO: it would be better to instantiate animations rather than create a new one per object
// COLLADA allows this through multiple <channel>s in <animation>.
// For this to work, we need to know objects that use a certain action.

void DocumentExporter::exportCurrentScene(Scene *sce, const char* filename, bool selected)
{
	PointerRNA sceneptr, unit_settings;
	PropertyRNA *system; /* unused , *scale; */

	clear_global_id_map();
	
	COLLADABU::NativeString native_filename =
		COLLADABU::NativeString(std::string(filename));
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
	float linearmeasure = 1.0f;

	linearmeasure = RNA_float_get(&unit_settings, "scale_length");

	switch(RNA_property_enum_get(&unit_settings, system)) {
		case USER_UNIT_NONE:
		case USER_UNIT_METRIC:
			if(linearmeasure == 0.001f) {
				unitname = "millimeter";
			}
			else if(linearmeasure == 0.01f) {
				unitname = "centimeter";
			}
			else if(linearmeasure == 0.1f) {
				unitname = "decimeter";
			}
			else if(linearmeasure == 1.0f) {
				unitname = "meter";
			}
			else if(linearmeasure == 1000.0f) {
				unitname = "kilometer";
			}
			break;
		case USER_UNIT_IMPERIAL:
			if(linearmeasure == 0.0254f) {
				unitname = "inch";
			}
			else if(linearmeasure == 0.3048f) {
				unitname = "foot";
			}
			else if(linearmeasure == 0.9144f) {
				unitname = "yard";
			}
			break;
		default:
			break;
	}

	asset.setUnit(unitname, linearmeasure);
	asset.setUpAxisType(COLLADASW::Asset::Z_UP);
	// TODO: need an Author field in userpref
	if(strlen(U.author) > 0) {
		asset.getContributor().mAuthor = U.author;
	}
	else {
		asset.getContributor().mAuthor = "Blender User";
	}
#ifdef WITH_BUILDINFO
	char version_buf[128];
	sprintf(version_buf, "Blender %d.%02d.%d r%s", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION, build_rev);
	asset.getContributor().mAuthoringTool = version_buf;
#else
	asset.getContributor().mAuthoringTool = "Blender 2.5x";
#endif
	asset.add();
	
	// <library_cameras>
	if(has_object_type(sce, OB_CAMERA)) {
		CamerasExporter ce(&sw);
		ce.exportCameras(sce, selected);
	}
	
	// <library_lights>
	if(has_object_type(sce, OB_LAMP)) {
		LightsExporter le(&sw);
		le.exportLights(sce, selected);
	}

	// <library_images>
	ImagesExporter ie(&sw, filename);
	ie.exportImages(sce, selected);
	
	// <library_effects>
	EffectsExporter ee(&sw);
	ee.exportEffects(sce, selected);
	
	// <library_materials>
	MaterialsExporter me(&sw);
	me.exportMaterials(sce, selected);

	// <library_geometries>
	if(has_object_type(sce, OB_MESH)) {
		GeometryExporter ge(&sw);
		ge.exportGeom(sce, selected);
	}

	// <library_animations>
	AnimationExporter ae(&sw);
	ae.exportAnimations(sce);

	// <library_controllers>
	ArmatureExporter arm_exporter(&sw);
	if(has_object_type(sce, OB_ARMATURE)) {
		arm_exporter.export_controllers(sce, selected);
	}

	// <library_visual_scenes>
	SceneExporter se(&sw, &arm_exporter);
	se.exportScene(sce, selected);
	
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
