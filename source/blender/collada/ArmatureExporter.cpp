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

extern "C" {
#include "BKE_main.h"
#include "BKE_mesh.h"
#include "BKE_global.h"
#include "BKE_library.h"
}

#include "ED_armature.h"

#include "BLI_listbase.h"

#include "GeometryExporter.h"
#include "ArmatureExporter.h"
#include "SceneExporter.h"

#include "collada_utils.h"

// XXX exporter writes wrong data for shared armatures.  A separate
// controller should be written for each armature-mesh binding how do
// we make controller ids then?
ArmatureExporter::ArmatureExporter(COLLADASW::StreamWriter *sw, const ExportSettings *export_settings) : COLLADASW::LibraryControllers(sw), export_settings(export_settings) {
}

// write bone nodes
void ArmatureExporter::add_armature_bones(Object *ob_arm, Scene *sce,
                                          SceneExporter *se,
                                          std::list<Object *>& child_objects)
{
	// write bone nodes

	bArmature * armature = (bArmature *)ob_arm->data;
	bool is_edited = armature->edbo != NULL;

	if (!is_edited)
		ED_armature_to_edit(armature);

	for (Bone *bone = (Bone *)armature->bonebase.first; bone; bone = bone->next) {
		// start from root bones
		if (!bone->parent)
			add_bone_node(bone, ob_arm, sce, se, child_objects);
	}

	if (!is_edited) {
		ED_armature_from_edit(armature);
		ED_armature_edit_free(armature);
	}
}

void ArmatureExporter::write_bone_URLs(COLLADASW::InstanceController &ins, Object *ob_arm, Bone *bone)
{
	if (bc_is_root_bone(bone, this->export_settings->deform_bones_only))
		ins.addSkeleton(COLLADABU::URI(COLLADABU::Utils::EMPTY_STRING, get_joint_id(bone, ob_arm)));
	else {
		for (Bone *child = (Bone *)bone->childbase.first; child; child = child->next) {
			write_bone_URLs(ins, ob_arm, child);
		}
	}
}

bool ArmatureExporter::add_instance_controller(Object *ob)
{
	Object *ob_arm = bc_get_assigned_armature(ob);
	bArmature *arm = (bArmature *)ob_arm->data;

	const std::string& controller_id = get_controller_id(ob_arm, ob);

	COLLADASW::InstanceController ins(mSW);
	ins.setUrl(COLLADASW::URI(COLLADABU::Utils::EMPTY_STRING, controller_id));

	Mesh *me = (Mesh *)ob->data;
	if (!me->dvert) return false;

	// write root bone URLs
	Bone *bone;
	for (bone = (Bone *)arm->bonebase.first; bone; bone = bone->next) {
		write_bone_URLs(ins, ob_arm, bone);
	}

	InstanceWriter::add_material_bindings(ins.getBindMaterial(), 
		ob, 
		this->export_settings->active_uv_only,
		this->export_settings->export_texture_type);
		
	ins.add();
	return true;
}

#if 0
void ArmatureExporter::operator()(Object *ob)
{
	Object *ob_arm = bc_get_assigned_armature(ob);

}

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

	Base *base = (Base *) sce->base.first;
	while (base) {
		Object *ob = base->object;
		
		if (ob->type == OB_MESH && get_assigned_armature(ob) == ob_arm) {
			objects.push_back(ob);
		}

		base = base->next;
	}
}
#endif

// parent_mat is armature-space
void ArmatureExporter::add_bone_node(Bone *bone, Object *ob_arm, Scene *sce,
                                     SceneExporter *se,
                                     std::list<Object *>& child_objects)
{
	if (!(this->export_settings->deform_bones_only && bone->flag & BONE_NO_DEFORM)) {
		std::string node_id = get_joint_id(bone, ob_arm);
		std::string node_name = std::string(bone->name);
		std::string node_sid = get_joint_sid(bone, ob_arm);

		COLLADASW::Node node(mSW);

		node.setType(COLLADASW::Node::JOINT);
		node.setNodeId(node_id);
		node.setNodeName(node_name);
		node.setNodeSid(node_sid);

		if (this->export_settings->use_blender_profile)
		{
			if (bone->parent) {
				if (bone->flag & BONE_CONNECTED) {
					node.addExtraTechniqueParameter("blender", "connect", true);
				}
			}
			std::string layers = BoneExtended::get_bone_layers(bone->layer);
			node.addExtraTechniqueParameter("blender", "layer", layers);

			bArmature *armature = (bArmature *)ob_arm->data;
			EditBone *ebone = bc_get_edit_bone(armature, bone->name);
			if (ebone && ebone->roll != 0)
			{
				node.addExtraTechniqueParameter("blender", "roll", ebone->roll);
			}
			if (bc_is_leaf_bone(bone))
			{
				node.addExtraTechniqueParameter("blender", "tip_x", bone->arm_tail[0] - bone->arm_head[0]);
				node.addExtraTechniqueParameter("blender", "tip_y", bone->arm_tail[1] - bone->arm_head[1]);
				node.addExtraTechniqueParameter("blender", "tip_z", bone->arm_tail[2] - bone->arm_head[2]);
			}
		}

			node.start();

			add_bone_transform(ob_arm, bone, node);

			// Write nodes of childobjects, remove written objects from list
			std::list<Object *>::iterator i = child_objects.begin();

			while (i != child_objects.end()) {
				if ((*i)->partype == PARBONE && STREQ((*i)->parsubstr, bone->name)) {
					float backup_parinv[4][4];
					copy_m4_m4(backup_parinv, (*i)->parentinv);

					// crude, temporary change to parentinv
					// so transform gets exported correctly.

					// Add bone tail- translation... don't know why
					// bone parenting is against the tail of a bone
					// and not it's head, seems arbitrary.
					(*i)->parentinv[3][1] += bone->length;

					// OPEN_SIM_COMPATIBILITY
					// TODO: when such objects are animated as
					// single matrix the tweak must be applied
					// to the result.
					if (export_settings->open_sim) {
						// tweak objects parentinverse to match compatibility
						float temp[4][4];

						copy_m4_m4(temp, bone->arm_mat);
						temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;

						mul_m4_m4m4((*i)->parentinv, temp, (*i)->parentinv);
					}

					se->writeNodes(*i, sce);

					copy_m4_m4((*i)->parentinv, backup_parinv);
					child_objects.erase(i++);
				}
				else i++;
			}

			for (Bone *child = (Bone *)bone->childbase.first; child; child = child->next) {
				add_bone_node(child, ob_arm, sce, se, child_objects);
			}
			node.end();
		}
		else {
			for (Bone *child = (Bone *)bone->childbase.first; child; child = child->next) {
				add_bone_node(child, ob_arm, sce, se, child_objects);
			}
		}
}

void ArmatureExporter::add_bone_transform(Object *ob_arm, Bone *bone, COLLADASW::Node& node)
{
	//bPoseChannel *pchan = BKE_pose_channel_find_name(ob_arm->pose, bone->name);

	float mat[4][4];
	float bone_rest_mat[4][4]; /* derived from bone->arm_mat */
	float parent_rest_mat[4][4]; /* derived from bone->parent->arm_mat */

	bool has_restmat = bc_get_property_matrix(bone, "rest_mat", mat);

	if (!has_restmat) {

		/* Have no restpose matrix stored, try old style <= Blender 2.78 */
		
		bc_create_restpose_mat(this->export_settings, bone, bone_rest_mat, bone->arm_mat, true);

		if (bone->parent) {
			// get bone-space matrix from parent pose
			/*bPoseChannel *parchan = BKE_pose_channel_find_name(ob_arm->pose, bone->parent->name);
			float invpar[4][4];
			invert_m4_m4(invpar, parchan->pose_mat);
			mul_m4_m4m4(mat, invpar, pchan->pose_mat);*/
			float invpar[4][4];
			bc_create_restpose_mat(this->export_settings, bone->parent, parent_rest_mat, bone->parent->arm_mat, true);

			invert_m4_m4(invpar, parent_rest_mat);
			mul_m4_m4m4(mat, invpar, bone_rest_mat);

		}
		else {
			copy_m4_m4(mat, bone_rest_mat);
		}

		// OPEN_SIM_COMPATIBILITY
		if (export_settings->open_sim) {
			// Remove rotations vs armature from transform
			// parent_rest_rot * mat * irest_rot
			float temp[4][4];
			copy_m4_m4(temp, bone_rest_mat);
			temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;
			invert_m4(temp);

			mul_m4_m4m4(mat, mat, temp);

			if (bone->parent) {
				copy_m4_m4(temp, parent_rest_mat);
				temp[3][0] = temp[3][1] = temp[3][2] = 0.0f;

				mul_m4_m4m4(mat, temp, mat);
			}
		}
	}

	if (this->export_settings->limit_precision)
		bc_sanitize_mat(mat, 6);

	TransformWriter::add_node_transform(node, mat, NULL);

}

std::string ArmatureExporter::get_controller_id(Object *ob_arm, Object *ob)
{
	return translate_id(id_name(ob_arm)) + "_" + translate_id(id_name(ob)) + SKIN_CONTROLLER_ID_SUFFIX;
}
