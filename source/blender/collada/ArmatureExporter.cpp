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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Jan Diederich, Tod Liverseed,
 *                 Nathan Letwory
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/ArmatureExporter.cpp
 *  \ingroup collada
 */


#include "COLLADASWBaseInputElement.h"
#include "COLLADASWInstanceController.h"
#include "COLLADASWPrimitves.h"
#include "COLLADASWSource.h"

#include "DNA_action_types.h"
#include "DNA_meshdata_types.h"
#include "DNA_modifier_types.h"

#include "BKE_action.h"
#include "BKE_armature.h"
#include "ED_armature.h"

#include "BLI_listbase.h"

#include "GeometryExporter.h"
#include "ArmatureExporter.h"
#include "SceneExporter.h"

// XXX exporter writes wrong data for shared armatures.  A separate
// controller should be written for each armature-mesh binding how do
// we make controller ids then?
ArmatureExporter::ArmatureExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings) : COLLADASW::LibraryControllers(sw), export_settings(export_settings) {}

// write bone nodes
void ArmatureExporter::add_armature_bones(Object *ob_arm, Scene* sce,
										  SceneExporter* se,
										  std::list<Object*>& child_objects)
{
	// write bone nodes
	bArmature *arm = (bArmature*)ob_arm->data;
	for (Bone *bone = (Bone*)arm->bonebase.first; bone; bone = bone->next) {
		// start from root bones
		if (!bone->parent)
			add_bone_node(bone, ob_arm, sce, se, child_objects);
	}
}

bool ArmatureExporter::is_skinned_mesh(Object *ob)
{
	return get_assigned_armature(ob) != NULL;
}

void ArmatureExporter::add_instance_controller(Object *ob)
{
	Object *ob_arm = get_assigned_armature(ob);
	bArmature *arm = (bArmature*)ob_arm->data;

	const std::string& controller_id = get_controller_id(ob_arm, ob);

	COLLADASW::InstanceController ins(mSW);
	ins.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, controller_id));

	// write root bone URLs
	Bone *bone;
	for (bone = (Bone*)arm->bonebase.first; bone; bone = bone->next) {
		if (!bone->parent)
			ins.addSkeleton(COLLADABU::URI(COLLADABU::Utils::EMPTY_STRING, get_joint_id(bone, ob_arm)));
	}

	InstanceWriter::add_material_bindings(ins.getBindMaterial(), ob);
		
	ins.add();
}

void ArmatureExporter::export_controllers(Scene *sce)
{
	scene = sce;

	openLibrary();

	GeometryFunctor gf;
	gf.forEachMeshObjectInScene<ArmatureExporter>(sce, *this, this->export_settings->selected);

	closeLibrary();
}

void ArmatureExporter::operator()(Object *ob)
{
	Object *ob_arm = get_assigned_armature(ob);

	if (ob_arm /*&& !already_written(ob_arm)*/)
		export_controller(ob, ob_arm);
}
#if 0

bool ArmatureExporter::already_written(Object *ob_arm)
{
	return std::find(written_armatures.begin(), written_armatures.end(), ob_arm) != written_armatures.end();
}

void ArmatureExporter::wrote(Object *ob_arm)
{
	written_armatures.push_back(ob_arm);
}

void ArmatureExporter::find_objects_using_armature(Object *ob_arm, std::vector<Object *>& objects, Scene *sce)
{
	objects.clear();

	Base *base= (Base*) sce->base.first;
	while(base) {
		Object *ob = base->object;
		
		if (ob->type == OB_MESH && get_assigned_armature(ob) == ob_arm) {
			objects.push_back(ob);
		}

		base= base->next;
	}
}
#endif

Object *ArmatureExporter::get_assigned_armature(Object *ob)
{
	Object *ob_arm = NULL;

	if (ob->parent && ob->partype == PARSKEL && ob->parent->type == OB_ARMATURE) {
		ob_arm = ob->parent;
	}
	else {
		ModifierData *mod = (ModifierData*)ob->modifiers.first;
		while (mod) {
			if (mod->type == eModifierType_Armature) {
				ob_arm = ((ArmatureModifierData*)mod)->object;
			}

			mod = mod->next;
		}
	}

	return ob_arm;
}

std::string ArmatureExporter::get_joint_sid(Bone *bone, Object *ob_arm)
{
	return get_joint_id(bone, ob_arm);
}

// parent_mat is armature-space
void ArmatureExporter::add_bone_node(Bone *bone, Object *ob_arm, Scene* sce,
									 SceneExporter* se,
									 std::list<Object*>& child_objects)
{
	std::string node_id = get_joint_id(bone, ob_arm);
	std::string node_name = std::string(bone->name);
	std::string node_sid = get_joint_sid(bone, ob_arm);

	COLLADASW::Node node(mSW);

	node.setType(COLLADASW::Node::JOINT);
	node.setNodeId(node_id);
	node.setNodeName(node_name);
	node.setNodeSid(node_sid);

	/*if ( bone->childbase.first == NULL || BLI_countlist(&(bone->childbase))>=2)
		add_blender_leaf_bone( bone, ob_arm , node );
	else{*/
	node.start();

	add_bone_transform(ob_arm, bone, node);

	// Write nodes of childobjects, remove written objects from list
	std::list<Object*>::iterator i = child_objects.begin();

	while( i != child_objects.end() )
	{
		if((*i)->partype == PARBONE && (0 == strcmp((*i)->parsubstr, bone->name)))
		{
			float backup_parinv[4][4];
			copy_m4_m4(backup_parinv, (*i)->parentinv);

			// crude, temporary change to parentinv
			// so transform gets exported correctly.

			// Add bone tail- translation... don't know why
			// bone parenting is against the tail of a bone
			// and not it's head, seems arbitrary.
			(*i)->parentinv[3][1] += bone->length;

			// SECOND_LIFE_COMPATIBILITY
			// TODO: when such objects are animated as
			// single matrix the tweak must be applied
			// to the result.
			if(export_settings->second_life)
			{
				// tweak objects parentinverse to match compatibility
				float temp[4][4];

				copy_m4_m4(temp, bone->arm_mat);
				temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;

				mult_m4_m4m4((*i)->parentinv, temp, (*i)->parentinv);
			}

			se->writeNodes(*i, sce);

			copy_m4_m4((*i)->parentinv, backup_parinv);
			child_objects.erase(i++);
		}
		else i++;
	}

	for (Bone *child = (Bone*)bone->childbase.first; child; child = child->next) {
		add_bone_node(child, ob_arm, sce, se, child_objects);
	}
	node.end();
	//}
}

/*void ArmatureExporter::add_blender_leaf_bone(Bone *bone, Object *ob_arm, COLLADASW::Node& node)
{
	node.start();
	
	add_bone_transform(ob_arm, bone, node);
	
	node.addExtraTechniqueParameter("blender", "tip_x", bone->tail[0] );
	node.addExtraTechniqueParameter("blender", "tip_y", bone->tail[1] );
	node.addExtraTechniqueParameter("blender", "tip_z", bone->tail[2] );
	
	for (Bone *child = (Bone*)bone->childbase.first; child; child = child->next) {
		add_bone_node(child, ob_arm, sce, se, child_objects);
	}
	node.end();
	
}*/
void ArmatureExporter::add_bone_transform(Object *ob_arm, Bone *bone, COLLADASW::Node& node)
{
	bPoseChannel *pchan = get_pose_channel(ob_arm->pose, bone->name);

	float mat[4][4];

	if (bone->parent) {
		// get bone-space matrix from armature-space
		bPoseChannel *parchan = get_pose_channel(ob_arm->pose, bone->parent->name);

		float invpar[4][4];
		invert_m4_m4(invpar, parchan->pose_mat);
		mult_m4_m4m4(mat, invpar, pchan->pose_mat);
	}
	else {
		copy_m4_m4(mat, pchan->pose_mat);
		// Why? Joint's localspace is still it's parent node
		//get world-space from armature-space
		//mult_m4_m4m4(mat, ob_arm->obmat, pchan->pose_mat);
	}

	// SECOND_LIFE_COMPATIBILITY
	if(export_settings->second_life)
	{
		// Remove rotations vs armature from transform
		// parent_rest_rot * mat * irest_rot
		float temp[4][4];
		copy_m4_m4(temp, bone->arm_mat);
		temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;
		invert_m4(temp);

		mult_m4_m4m4(mat, mat, temp);

		if(bone->parent)
		{
			copy_m4_m4(temp, bone->parent->arm_mat);
			temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;

			mult_m4_m4m4(mat, temp, mat);
		}
	}

	TransformWriter::add_node_transform(node, mat,NULL );
}

std::string ArmatureExporter::get_controller_id(Object *ob_arm, Object *ob)
{
	return translate_id(id_name(ob_arm)) + "_" + translate_id(id_name(ob)) + SKIN_CONTROLLER_ID_SUFFIX;
}

// ob should be of type OB_MESH
// both args are required
void ArmatureExporter::export_controller(Object* ob, Object *ob_arm)
{
	// joint names
	// joint inverse bind matrices
	// vertex weights

	// input:
	// joint names: ob -> vertex group names
	// vertex group weights: me->dvert -> groups -> index, weight

	/*
	me->dvert:

	typedef struct MDeformVert {
		struct MDeformWeight *dw;
		int totweight;
		int flag;	// flag only in use for weightpaint now
	} MDeformVert;

	typedef struct MDeformWeight {
		int				def_nr;
		float			weight;
	} MDeformWeight;
	*/

	Mesh *me = (Mesh*)ob->data;
	if (!me->dvert) return;

	std::string controller_name = id_name(ob_arm);
	std::string controller_id = get_controller_id(ob_arm, ob);

	openSkin(controller_id, controller_name,
	         COLLADABU::URI(COLLADABU::Utils::EMPTY_STRING, get_geometry_id(ob)));

	add_bind_shape_mat(ob);

	std::string joints_source_id = add_joints_source(ob_arm, &ob->defbase, controller_id);
	std::string inv_bind_mat_source_id = add_inv_bind_mats_source(ob_arm, &ob->defbase, controller_id);

	std::list<int> vcounts;
	std::list<int> joints;
	std::list<float> weights;

	{
		int i, j;

		// def group index -> joint index
		std::vector<int> joint_index_by_def_index;
		bDeformGroup *def;

		for (def = (bDeformGroup*)ob->defbase.first, i = 0, j = 0; def; def = def->next, i++) {
			if (is_bone_defgroup(ob_arm, def))
				joint_index_by_def_index.push_back(j++);
			else
				joint_index_by_def_index.push_back(-1);
		}

		for (i = 0; i < me->totvert; i++) {
			MDeformVert *vert = &me->dvert[i];
			std::map<int, float> jw;

			// We're normalizing the weights later
			float sumw = 0.0f;

			for (j = 0; j < vert->totweight; j++) {
				int joint_index = joint_index_by_def_index[vert->dw[j].def_nr];
				if(joint_index != -1 && vert->dw[j].weight > 0.0f)
				{
					jw[joint_index] += vert->dw[j].weight;
					sumw += vert->dw[j].weight;
				}
			}

			if(sumw > 0.0f)
			{
				float invsumw = 1.0f/sumw;
				vcounts.push_back(jw.size());
				for(std::map<int, float>::iterator m = jw.begin(); m != jw.end(); ++m)
				{
					joints.push_back((*m).first);
					weights.push_back(invsumw*(*m).second);
				}
			}
			else
			{
				vcounts.push_back(0);
				/*vcounts.push_back(1);
				joints.push_back(-1);
				weights.push_back(1.0f);*/
			}
		}
	}

	std::string weights_source_id = add_weights_source(me, controller_id, weights);
	add_joints_element(&ob->defbase, joints_source_id, inv_bind_mat_source_id);
	add_vertex_weights_element(weights_source_id, joints_source_id, vcounts, joints);

	closeSkin();
	closeController();
}

void ArmatureExporter::add_joints_element(ListBase *defbase,
						const std::string& joints_source_id, const std::string& inv_bind_mat_source_id)
{
	COLLADASW::JointsElement joints(mSW);
	COLLADASW::InputList &input = joints.getInputList();

	input.push_back(COLLADASW::Input(COLLADASW::InputSemantic::JOINT, // constant declared in COLLADASWInputList.h
							   COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, joints_source_id)));
	input.push_back(COLLADASW::Input(COLLADASW::InputSemantic::BINDMATRIX,
							   COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, inv_bind_mat_source_id)));
	joints.add();
}

void ArmatureExporter::add_bind_shape_mat(Object *ob)
{
	double bind_mat[4][4];

	converter.mat4_to_dae_double(bind_mat, ob->obmat);

	addBindShapeTransform(bind_mat);
}

std::string ArmatureExporter::add_joints_source(Object *ob_arm, ListBase *defbase, const std::string& controller_id)
{
	std::string source_id = controller_id + JOINTS_SOURCE_ID_SUFFIX;

	int totjoint = 0;
	bDeformGroup *def;
	for (def = (bDeformGroup*)defbase->first; def; def = def->next) {
		if (is_bone_defgroup(ob_arm, def))
			totjoint++;
	}

	COLLADASW::NameSource source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(totjoint);
	source.setAccessorStride(1);
	
	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	param.push_back("JOINT");

	source.prepareToAppendValues();

	for (def = (bDeformGroup*)defbase->first; def; def = def->next) {
		Bone *bone = get_bone_from_defgroup(ob_arm, def);
		if (bone)
			source.appendValues(get_joint_sid(bone, ob_arm));
	}

	source.finish();

	return source_id;
}

std::string ArmatureExporter::add_inv_bind_mats_source(Object *ob_arm, ListBase *defbase, const std::string& controller_id)
{
	std::string source_id = controller_id + BIND_POSES_SOURCE_ID_SUFFIX;

	int totjoint = 0;
	for (bDeformGroup *def = (bDeformGroup*)defbase->first; def; def = def->next) {
		if (is_bone_defgroup(ob_arm, def))
			totjoint++;
	}

	COLLADASW::FloatSourceF source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(totjoint); //BLI_countlist(defbase));
	source.setAccessorStride(16);
	
	source.setParameterTypeName(&COLLADASW::CSWC::CSW_VALUE_TYPE_FLOAT4x4);
	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	param.push_back("TRANSFORM");

	source.prepareToAppendValues();

	bPose *pose = ob_arm->pose;
	bArmature *arm = (bArmature*)ob_arm->data;

	int flag = arm->flag;

	// put armature in rest position
	if (!(arm->flag & ARM_RESTPOS)) {
		arm->flag |= ARM_RESTPOS;
		where_is_pose(scene, ob_arm);
	}

	for (bDeformGroup *def = (bDeformGroup*)defbase->first; def; def = def->next) {
		if (is_bone_defgroup(ob_arm, def)) {
			bPoseChannel *pchan = get_pose_channel(pose, def->name);

			float mat[4][4];
			float world[4][4];
			float inv_bind_mat[4][4];

			// SECOND_LIFE_COMPATIBILITY
			if(export_settings->second_life)
			{
				// Only translations, no rotation vs armature
				float temp[4][4];
				unit_m4(temp);
				copy_v3_v3(temp[3], pchan->bone->arm_mat[3]);
				mult_m4_m4m4(world, ob_arm->obmat, temp);
			}
			else
			{
				// make world-space matrix, arm_mat is armature-space
				mult_m4_m4m4(world, ob_arm->obmat, pchan->bone->arm_mat);
			}

			invert_m4_m4(mat, world);
			converter.mat4_to_dae(inv_bind_mat, mat);

			source.appendValues(inv_bind_mat);
		}
	}

	// back from rest positon
	if (!(flag & ARM_RESTPOS)) {
		arm->flag = flag;
		where_is_pose(scene, ob_arm);
	}

	source.finish();

	return source_id;
}

Bone *ArmatureExporter::get_bone_from_defgroup(Object *ob_arm, bDeformGroup* def)
{
	bPoseChannel *pchan = get_pose_channel(ob_arm->pose, def->name);
	return pchan ? pchan->bone : NULL;
}

bool ArmatureExporter::is_bone_defgroup(Object *ob_arm, bDeformGroup* def)
{
	return get_bone_from_defgroup(ob_arm, def) != NULL;
}

std::string ArmatureExporter::add_weights_source(Mesh *me, const std::string& controller_id, const std::list<float>& weights)
{
	std::string source_id = controller_id + WEIGHTS_SOURCE_ID_SUFFIX;

	COLLADASW::FloatSourceF source(mSW);
	source.setId(source_id);
	source.setArrayId(source_id + ARRAY_ID_SUFFIX);
	source.setAccessorCount(weights.size());
	source.setAccessorStride(1);
	
	COLLADASW::SourceBase::ParameterNameList &param = source.getParameterNameList();
	param.push_back("WEIGHT");

	source.prepareToAppendValues();

	for(std::list<float>::const_iterator i = weights.begin(); i != weights.end(); ++i) {
		source.appendValues(*i);
	}

	source.finish();

	return source_id;
}

void ArmatureExporter::add_vertex_weights_element(const std::string& weights_source_id, const std::string& joints_source_id,
												  const std::list<int>& vcounts,
												  const std::list<int>& joints)
{
	COLLADASW::VertexWeightsElement weightselem(mSW);
	COLLADASW::InputList &input = weightselem.getInputList();

	int offset = 0;
	input.push_back(COLLADASW::Input(COLLADASW::InputSemantic::JOINT, // constant declared in COLLADASWInputList.h
									 COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, joints_source_id), offset++));
	input.push_back(COLLADASW::Input(COLLADASW::InputSemantic::WEIGHT,
									 COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, weights_source_id), offset++));

	weightselem.setCount(vcounts.size());

	// write number of deformers per vertex
	COLLADASW::PrimitivesBase::VCountList vcountlist;

	vcountlist.resize(vcounts.size());
	std::copy(vcounts.begin(), vcounts.end(), vcountlist.begin());

	weightselem.prepareToAppendVCountValues();
	weightselem.appendVertexCount(vcountlist);

	weightselem.CloseVCountAndOpenVElement();

	// write deformer index - weight index pairs
	int weight_index = 0;
	for(std::list<int>::const_iterator i = joints.begin(); i != joints.end(); ++i)
	{
		weightselem.appendValues(*i, weight_index++);
	}

	weightselem.finish();
}
