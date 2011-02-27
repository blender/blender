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
#ifdef NAN_BUILDINFO
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
	
	void exportScene(Scene *sce) {
 		// <library_visual_scenes> <visual_scene>
		std::string id_naming = id_name(sce);
		openVisualScene(translate_id(id_naming), id_naming);

		// write <node>s
		//forEachMeshObjectInScene(sce, *this);
		//forEachCameraObjectInScene(sce, *this);
		//forEachLampObjectInScene(sce, *this);
		exportHierarchy(sce);

		// </visual_scene> </library_visual_scenes>
		closeVisualScene();

		closeLibrary();
	}

	void exportHierarchy(Scene *sce)
	{
		Base *base= (Base*) sce->base.first;
		while(base) {
			Object *ob = base->object;

			if (!ob->parent) {
				switch(ob->type) {
				case OB_MESH:
				case OB_CAMERA:
				case OB_LAMP:
				case OB_EMPTY:
				case OB_ARMATURE:
					// write nodes....
					writeNodes(ob, sce);
					break;
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
class AnimationExporter: COLLADASW::LibraryAnimations
{
	Scene *scene;
	COLLADASW::StreamWriter *sw;

public:

	AnimationExporter(COLLADASW::StreamWriter *sw): COLLADASW::LibraryAnimations(sw) { this->sw = sw; }

	void exportAnimations(Scene *sce)
	{
		this->scene = sce;

		openLibrary();
		
		forEachObjectInScene(sce, *this);
		
		closeLibrary();
	}

	// called for each exported object
	void operator() (Object *ob) 
	{
		if (!ob->adt || !ob->adt->action) return;
		
		FCurve *fcu = (FCurve*)ob->adt->action->curves.first;
		
		if (ob->type == OB_ARMATURE) {
			if (!ob->data) return;

			bArmature *arm = (bArmature*)ob->data;
			for (Bone *bone = (Bone*)arm->bonebase.first; bone; bone = bone->next)
				write_bone_animation(ob, bone);
		}
		else {
			while (fcu) {
				// TODO "rotation_quaternion" is also possible for objects (although euler is default)
				if ((!strcmp(fcu->rna_path, "location") || !strcmp(fcu->rna_path, "scale")) ||
					(!strcmp(fcu->rna_path, "rotation_euler") && ob->rotmode == ROT_MODE_EUL))
					dae_animation(fcu, id_name(ob));

				fcu = fcu->next;
			}
		}
	}

protected:

	void dae_animation(FCurve *fcu, std::string ob_name)
	{
		const char *axis_names[] = {"X", "Y", "Z"};
		const char *axis_name = NULL;
		char anim_id[200];
		
		if (fcu->array_index < 3)
			axis_name = axis_names[fcu->array_index];

		BLI_snprintf(anim_id, sizeof(anim_id), "%s_%s_%s", (char*)translate_id(ob_name).c_str(),
					 fcu->rna_path, axis_names[fcu->array_index]);

		// check rna_path is one of: rotation, scale, location

		openAnimation(anim_id, COLLADABU::Utils::EMPTY_STRING);

		// create input source
		std::string input_id = create_source_from_fcurve(COLLADASW::InputSemantic::INPUT, fcu, anim_id, axis_name);

		// create output source
		std::string output_id = create_source_from_fcurve(COLLADASW::InputSemantic::OUTPUT, fcu, anim_id, axis_name);

		// create interpolations source
		std::string interpolation_id = create_interpolation_source(fcu->totvert, anim_id, axis_name);

		std::string sampler_id = std::string(anim_id) + SAMPLER_ID_SUFFIX;
		COLLADASW::LibraryAnimations::Sampler sampler(sw, sampler_id);
		std::string empty;
		sampler.addInput(COLLADASW::InputSemantic::INPUT, COLLADABU::URI(empty, input_id));
		sampler.addInput(COLLADASW::InputSemantic::OUTPUT, COLLADABU::URI(empty, output_id));

		// this input is required
		sampler.addInput(COLLADASW::InputSemantic::INTERPOLATION, COLLADABU::URI(empty, interpolation_id));

		addSampler(sampler);

		std::string target = translate_id(ob_name)
			+ "/" + get_transform_sid(fcu->rna_path, -1, axis_name, true);
		addChannel(COLLADABU::URI(empty, sampler_id), target);

		closeAnimation();
	}

	void write_bone_animation(Object *ob_arm, Bone *bone)
	{
		if (!ob_arm->adt)
			return;

		for (int i = 0; i < 3; i++)
			sample_and_write_bone_animation(ob_arm, bone, i);

		for (Bone *child = (Bone*)bone->childbase.first; child; child = child->next)
			write_bone_animation(ob_arm, child);
	}

	void sample_and_write_bone_animation(Object *ob_arm, Bone *bone, int transform_type)
	{
		bArmature *arm = (bArmature*)ob_arm->data;
		int flag = arm->flag;
		std::vector<float> fra;
		char prefix[256];

		BLI_snprintf(prefix, sizeof(prefix), "pose.bones[\"%s\"]", bone->name);

		bPoseChannel *pchan = get_pose_channel(ob_arm->pose, bone->name);
		if (!pchan)
			return;

		switch (transform_type) {
		case 0:
			find_rotation_frames(ob_arm, fra, prefix, pchan->rotmode);
			break;
		case 1:
			find_frames(ob_arm, fra, prefix, "scale");
			break;
		case 2:
			find_frames(ob_arm, fra, prefix, "location");
			break;
		default:
			return;
		}

		// exit rest position
		if (flag & ARM_RESTPOS) {
			arm->flag &= ~ARM_RESTPOS;
			where_is_pose(scene, ob_arm);
		}

		if (fra.size()) {
			float *v = (float*)MEM_callocN(sizeof(float) * 3 * fra.size(), "temp. anim frames");
			sample_animation(v, fra, transform_type, bone, ob_arm);

			if (transform_type == 0) {
				// write x, y, z curves separately if it is rotation
				float *c = (float*)MEM_callocN(sizeof(float) * fra.size(), "temp. anim frames");
				for (int i = 0; i < 3; i++) {
					for (unsigned int j = 0; j < fra.size(); j++)
						c[j] = v[j * 3 + i];

					dae_bone_animation(fra, c, transform_type, i, id_name(ob_arm), bone->name);
				}
				MEM_freeN(c);
			}
			else {
				// write xyz at once if it is location or scale
				dae_bone_animation(fra, v, transform_type, -1, id_name(ob_arm), bone->name);
			}

			MEM_freeN(v);
		}

		// restore restpos
		if (flag & ARM_RESTPOS) 
			arm->flag = flag;
		where_is_pose(scene, ob_arm);
	}

	void sample_animation(float *v, std::vector<float> &frames, int type, Bone *bone, Object *ob_arm)
	{
		bPoseChannel *pchan, *parchan = NULL;
		bPose *pose = ob_arm->pose;

		pchan = get_pose_channel(pose, bone->name);

		if (!pchan)
			return;

		parchan = pchan->parent;

		enable_fcurves(ob_arm->adt->action, bone->name);

		std::vector<float>::iterator it;
		for (it = frames.begin(); it != frames.end(); it++) {
			float mat[4][4], ipar[4][4];

			float ctime = bsystem_time(scene, ob_arm, *it, 0.0f);

			BKE_animsys_evaluate_animdata(&ob_arm->id, ob_arm->adt, *it, ADT_RECALC_ANIM);
			where_is_pose_bone(scene, ob_arm, pchan, ctime, 1);

			// compute bone local mat
			if (bone->parent) {
				invert_m4_m4(ipar, parchan->pose_mat);
				mul_m4_m4m4(mat, pchan->pose_mat, ipar);
			}
			else
				copy_m4_m4(mat, pchan->pose_mat);

			switch (type) {
			case 0:
				mat4_to_eul(v, mat);
				break;
			case 1:
				mat4_to_size(v, mat);
				break;
			case 2:
				copy_v3_v3(v, mat[3]);
				break;
			}

			v += 3;
		}

		enable_fcurves(ob_arm->adt->action, NULL);
	}

	// dae_bone_animation -> add_bone_animation
	// (blend this into dae_bone_animation)
	void dae_bone_animation(std::vector<float> &fra, float *v, int tm_type, int axis, std::string ob_name, std::string bone_name)
	{
		const char *axis_names[] = {"X", "Y", "Z"};
		const char *axis_name = NULL;
		char anim_id[200];
		bool is_rot = tm_type == 0;
		
		if (!fra.size())
			return;

		char rna_path[200];
		BLI_snprintf(rna_path, sizeof(rna_path), "pose.bones[\"%s\"].%s", bone_name.c_str(),
					 tm_type == 0 ? "rotation_quaternion" : (tm_type == 1 ? "scale" : "location"));

		if (axis > -1)
			axis_name = axis_names[axis];
		
		std::string transform_sid = get_transform_sid(NULL, tm_type, axis_name, false);
		
		BLI_snprintf(anim_id, sizeof(anim_id), "%s_%s_%s", (char*)translate_id(ob_name).c_str(),
					 (char*)translate_id(bone_name).c_str(), (char*)transform_sid.c_str());

		openAnimation(anim_id, COLLADABU::Utils::EMPTY_STRING);

		// create input source
		std::string input_id = create_source_from_vector(COLLADASW::InputSemantic::INPUT, fra, is_rot, anim_id, axis_name);

		// create output source
		std::string output_id;
		if (axis == -1)
			output_id = create_xyz_source(v, fra.size(), anim_id);
		else
			output_id = create_source_from_array(COLLADASW::InputSemantic::OUTPUT, v, fra.size(), is_rot, anim_id, axis_name);

		// create interpolations source
		std::string interpolation_id = create_interpolation_source(fra.size(), anim_id, axis_name);

		std::string sampler_id = std::string(anim_id) + SAMPLER_ID_SUFFIX;
		COLLADASW::LibraryAnimations::Sampler sampler(sw, sampler_id);
		std::string empty;
		sampler.addInput(COLLADASW::InputSemantic::INPUT, COLLADABU::URI(empty, input_id));
		sampler.addInput(COLLADASW::InputSemantic::OUTPUT, COLLADABU::URI(empty, output_id));

		// TODO create in/out tangents source

		// this input is required
		sampler.addInput(COLLADASW::InputSemantic::INTERPOLATION, COLLADABU::URI(empty, interpolation_id));

		addSampler(sampler);

		std::string target = translate_id(ob_name + "_" + bone_name) + "/" + transform_sid;
		addChannel(COLLADABU::URI(empty, sampler_id), target);

		closeAnimation();
	}

	float convert_time(float frame)
	{
		return FRA2TIME(frame);
	}

	float convert_angle(float angle)
	{
		return COLLADABU::Math::Utils::radToDegF(angle);
	}

	std::string get_semantic_suffix(COLLADASW::InputSemantic::Semantics semantic)
	{
		switch(semantic) {
		case COLLADASW::InputSemantic::INPUT:
			return INPUT_SOURCE_ID_SUFFIX;
		case COLLADASW::InputSemantic::OUTPUT:
			return OUTPUT_SOURCE_ID_SUFFIX;
		case COLLADASW::InputSemantic::INTERPOLATION:
			return INTERPOLATION_SOURCE_ID_SUFFIX;
		case COLLADASW::InputSemantic::IN_TANGENT:
			return INTANGENT_SOURCE_ID_SUFFIX;
		case COLLADASW::InputSemantic::OUT_TANGENT:
			return OUTTANGENT_SOURCE_ID_SUFFIX;
		default:
			break;
		}
		return "";
	}

	void add_source_parameters(COLLADASW::SourceBase::ParameterNameList& param,
							   COLLADASW::InputSemantic::Semantics semantic, bool is_rot, const char *axis)
	{
		switch(semantic) {
		case COLLADASW::InputSemantic::INPUT:
			param.push_back("TIME");
			break;
		case COLLADASW::InputSemantic::OUTPUT:
			if (is_rot) {
				param.push_back("ANGLE");
			}
			else {
				if (axis) {
					param.push_back(axis);
				}
				else {
					param.push_back("X");
					param.push_back("Y");
					param.push_back("Z");
				}
			}
			break;
		case COLLADASW::InputSemantic::IN_TANGENT:
		case COLLADASW::InputSemantic::OUT_TANGENT:
			param.push_back("X");
			param.push_back("Y");
			break;
		default:
			break;
		}
	}

	void get_source_values(BezTriple *bezt, COLLADASW::InputSemantic::Semantics semantic, bool rotation, float *values, int *length)
	{
		switch (semantic) {
		case COLLADASW::InputSemantic::INPUT:
			*length = 1;
			values[0] = convert_time(bezt->vec[1][0]);
			break;
		case COLLADASW::InputSemantic::OUTPUT:
			*length = 1;
			if (rotation) {
				values[0] = convert_angle(bezt->vec[1][1]);
			}
			else {
				values[0] = bezt->vec[1][1];
			}
			break;
		case COLLADASW::InputSemantic::IN_TANGENT:
		case COLLADASW::InputSemantic::OUT_TANGENT:
			// XXX
			*length = 2;
			break;
		default:
			*length = 0;
			break;
		}
	}

	std::string create_source_from_fcurve(COLLADASW::InputSemantic::Semantics semantic, FCurve *fcu, const std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		//bool is_rotation = !strcmp(fcu->rna_path, "rotation");
		bool is_rotation = false;
		
		if (strstr(fcu->rna_path, "rotation")) is_rotation = true;
		
		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(fcu->totvert);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, is_rotation, axis_name);

		source.prepareToAppendValues();

		for (unsigned int i = 0; i < fcu->totvert; i++) {
			float values[3]; // be careful!
			int length = 0;

			get_source_values(&fcu->bezt[i], semantic, is_rotation, values, &length);
			for (int j = 0; j < length; j++)
				source.appendValues(values[j]);
		}

		source.finish();

		return source_id;
	}

	std::string create_source_from_array(COLLADASW::InputSemantic::Semantics semantic, float *v, int tot, bool is_rot, const std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(tot);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, is_rot, axis_name);

		source.prepareToAppendValues();

		for (int i = 0; i < tot; i++) {
			float val = v[i];
			if (semantic == COLLADASW::InputSemantic::INPUT)
				val = convert_time(val);
			else if (is_rot)
				val = convert_angle(val);
			source.appendValues(val);
		}

		source.finish();

		return source_id;
	}

	std::string create_source_from_vector(COLLADASW::InputSemantic::Semantics semantic, std::vector<float> &fra, bool is_rot, const std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(fra.size());
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, is_rot, axis_name);

		source.prepareToAppendValues();

		std::vector<float>::iterator it;
		for (it = fra.begin(); it != fra.end(); it++) {
			float val = *it;
			if (semantic == COLLADASW::InputSemantic::INPUT)
				val = convert_time(val);
			else if (is_rot)
				val = convert_angle(val);
			source.appendValues(val);
		}

		source.finish();

		return source_id;
	}

	// only used for sources with OUTPUT semantic
	std::string create_xyz_source(float *v, int tot, const std::string& anim_id)
	{
		COLLADASW::InputSemantic::Semantics semantic = COLLADASW::InputSemantic::OUTPUT;
		std::string source_id = anim_id + get_semantic_suffix(semantic);

		COLLADASW::FloatSourceF source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(tot);
		source.setAccessorStride(3);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		add_source_parameters(param, semantic, false, NULL);

		source.prepareToAppendValues();

		for (int i = 0; i < tot; i++) {
			source.appendValues(*v, *(v + 1), *(v + 2));
			v += 3;
		}

		source.finish();

		return source_id;
	}

	std::string create_interpolation_source(int tot, const std::string& anim_id, const char *axis_name)
	{
		std::string source_id = anim_id + get_semantic_suffix(COLLADASW::InputSemantic::INTERPOLATION);

		COLLADASW::NameSource source(mSW);
		source.setId(source_id);
		source.setArrayId(source_id + ARRAY_ID_SUFFIX);
		source.setAccessorCount(tot);
		source.setAccessorStride(1);
		
		COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
		param.push_back("INTERPOLATION");

		source.prepareToAppendValues();

		for (int i = 0; i < tot; i++) {
			source.appendValues(LINEAR_NAME);
		}

		source.finish();

		return source_id;
	}

	// for rotation, axis name is always appended and the value of append_axis is ignored
	std::string get_transform_sid(char *rna_path, int tm_type, const char *axis_name, bool append_axis)
	{
		std::string tm_name;

		// when given rna_path, determine tm_type from it
		if (rna_path) {
			char *name = extract_transform_name(rna_path);

			if (strstr(name, "rotation"))
				tm_type = 0;
			else if (!strcmp(name, "scale"))
				tm_type = 1;
			else if (!strcmp(name, "location"))
				tm_type = 2;
			else
				tm_type = -1;
		}

		switch (tm_type) {
		case 0:
			return std::string("rotation") + std::string(axis_name) + ".ANGLE";
		case 1:
			tm_name = "scale";
			break;
		case 2:
			tm_name = "location";
			break;
		default:
			tm_name = "";
			break;
		}

		if (tm_name.size()) {
			if (append_axis)
				return tm_name + std::string(".") + std::string(axis_name);
			else
				return tm_name;
		}

		return std::string("");
	}

	char *extract_transform_name(char *rna_path)
	{
		char *dot = strrchr(rna_path, '.');
		return dot ? (dot + 1) : rna_path;
	}

	void find_frames(Object *ob, std::vector<float> &fra, const char *prefix, const char *tm_name)
	{
		FCurve *fcu= (FCurve*)ob->adt->action->curves.first;

		for (; fcu; fcu = fcu->next) {
			if (prefix && strncmp(prefix, fcu->rna_path, strlen(prefix)))
				continue;

			char *name = extract_transform_name(fcu->rna_path);
			if (!strcmp(name, tm_name)) {
				for (unsigned int i = 0; i < fcu->totvert; i++) {
					float f = fcu->bezt[i].vec[1][0];
					if (std::find(fra.begin(), fra.end(), f) == fra.end())
						fra.push_back(f);
				}
			}
		}

		// keep the keys in ascending order
		std::sort(fra.begin(), fra.end());
	}

	void find_rotation_frames(Object *ob, std::vector<float> &fra, const char *prefix, int rotmode)
	{
		if (rotmode > 0)
			find_frames(ob, fra, prefix, "rotation_euler");
		else if (rotmode == ROT_MODE_QUAT)
			find_frames(ob, fra, prefix, "rotation_quaternion");
		/*else if (rotmode == ROT_MODE_AXISANGLE)
			;*/
	}

	// enable fcurves driving a specific bone, disable all the rest
	// if bone_name = NULL enable all fcurves
	void enable_fcurves(bAction *act, char *bone_name)
	{
		FCurve *fcu;
		char prefix[200];

		if (bone_name)
			BLI_snprintf(prefix, sizeof(prefix), "pose.bones[\"%s\"]", bone_name);

		for (fcu = (FCurve*)act->curves.first; fcu; fcu = fcu->next) {
			if (bone_name) {
				if (!strncmp(fcu->rna_path, prefix, strlen(prefix)))
					fcu->flag &= ~FCURVE_DISABLED;
				else
					fcu->flag |= FCURVE_DISABLED;
			}
			else {
				fcu->flag &= ~FCURVE_DISABLED;
			}
		}
	}
};

void DocumentExporter::exportCurrentScene(Scene *sce, const char* filename)
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
#ifdef NAN_BUILDINFO
	char version_buf[128];
	sprintf(version_buf, "Blender %d.%02d.%d r%s", BLENDER_VERSION/100, BLENDER_VERSION%100, BLENDER_SUBVERSION, build_rev);
	asset.getContributor().mAuthoringTool = version_buf;
#else
	asset.getContributor().mAuthoringTool = "Blender 2.5x";
#endif
	asset.add();
	
	// <library_cameras>
	CamerasExporter ce(&sw);
	ce.exportCameras(sce);
	
	// <library_lights>
	LightsExporter le(&sw);
	le.exportLights(sce);

	// <library_images>
	ImagesExporter ie(&sw, filename);
	ie.exportImages(sce);
	
	// <library_effects>
	EffectsExporter ee(&sw);
	ee.exportEffects(sce);
	
	// <library_materials>
	MaterialsExporter me(&sw);
	me.exportMaterials(sce);

	// <library_geometries>
	GeometryExporter ge(&sw);
	ge.exportGeom(sce);

	// <library_animations>
	AnimationExporter ae(&sw);
	ae.exportAnimations(sce);

	// <library_controllers>
	ArmatureExporter arm_exporter(&sw);
	arm_exporter.export_controllers(sce);

	// <library_visual_scenes>
	SceneExporter se(&sw, &arm_exporter);
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
