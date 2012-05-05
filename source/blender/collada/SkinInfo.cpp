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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Nathan Letwory.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/SkinInfo.cpp
 *  \ingroup collada
 */


#include <algorithm>

#if !defined(WIN32) || defined(FREE_WINDOWS)
#include <stdint.h>
#endif

/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include "BKE_object.h"
#include "DNA_armature_types.h"
#include "DNA_modifier_types.h"
#include "ED_mesh.h"
#include "ED_object.h"
#include "BKE_action.h"
#include "BLI_listbase.h"
#include "BLI_math.h"

#include "SkinInfo.h"
#include "collada_utils.h"

// use name, or fall back to original id if name not present (name is optional)
template<class T>
static const char *bc_get_joint_name(T *node)
{
	const std::string& id = node->getName();
	return id.size() ? id.c_str() : node->getOriginalId().c_str();
}

// This is used to store data passed in write_controller_data.
// Arrays from COLLADAFW::SkinControllerData lose ownership, so do this class members
// so that arrays don't get freed until we free them explicitly.
SkinInfo::SkinInfo() {}

SkinInfo::SkinInfo(const SkinInfo& skin) : weights(skin.weights),
								 joint_data(skin.joint_data),
								 unit_converter(skin.unit_converter),
								 ob_arm(skin.ob_arm),
								 controller_uid(skin.controller_uid),
								 parent(skin.parent)
{
	copy_m4_m4(bind_shape_matrix, (float (*)[4])skin.bind_shape_matrix);

	transfer_uint_array_data_const(skin.joints_per_vertex, joints_per_vertex);
	transfer_uint_array_data_const(skin.weight_indices, weight_indices);
	transfer_int_array_data_const(skin.joint_indices, joint_indices);
}

SkinInfo::SkinInfo(UnitConverter *conv) : unit_converter(conv), ob_arm(NULL), parent(NULL) {}

// nobody owns the data after this, so it should be freed manually with releaseMemory
template <class T>
void SkinInfo::transfer_array_data(T& src, T& dest)
{
	dest.setData(src.getData(), src.getCount());
	src.yieldOwnerShip();
	dest.yieldOwnerShip();
}

// when src is const we cannot src.yieldOwnerShip, this is used by copy constructor
void SkinInfo::transfer_int_array_data_const(const COLLADAFW::IntValuesArray& src, COLLADAFW::IntValuesArray& dest)
{
	dest.setData((int*)src.getData(), src.getCount());
	dest.yieldOwnerShip();
}

void SkinInfo::transfer_uint_array_data_const(const COLLADAFW::UIntValuesArray& src, COLLADAFW::UIntValuesArray& dest)
{
	dest.setData((unsigned int*)src.getData(), src.getCount());
	dest.yieldOwnerShip();
}

void SkinInfo::borrow_skin_controller_data(const COLLADAFW::SkinControllerData* skin)
{
	transfer_array_data((COLLADAFW::UIntValuesArray&)skin->getJointsPerVertex(), joints_per_vertex);
	transfer_array_data((COLLADAFW::UIntValuesArray&)skin->getWeightIndices(), weight_indices);
	transfer_array_data((COLLADAFW::IntValuesArray&)skin->getJointIndices(), joint_indices);
	// transfer_array_data(skin->getWeights(), weights);

	// cannot transfer data for FloatOrDoubleArray, copy values manually
	const COLLADAFW::FloatOrDoubleArray& weight = skin->getWeights();
	for (unsigned int i = 0; i < weight.getValuesCount(); i++)
		weights.push_back(bc_get_float_value(weight, i));

	unit_converter->dae_matrix_to_mat4_(bind_shape_matrix, skin->getBindShapeMatrix());
}
	
void SkinInfo::free()
{
	joints_per_vertex.releaseMemory();
	weight_indices.releaseMemory();
	joint_indices.releaseMemory();
	// weights.releaseMemory();
}

// using inverse bind matrices to construct armature
// it is safe to invert them to get the original matrices
// because if they are inverse matrices, they can be inverted
void SkinInfo::add_joint(const COLLADABU::Math::Matrix4& matrix)
{
	JointData jd;
	unit_converter->dae_matrix_to_mat4_(jd.inv_bind_mat, matrix);
	joint_data.push_back(jd);
}

void SkinInfo::set_controller(const COLLADAFW::SkinController* co)
{
	controller_uid = co->getUniqueId();

	// fill in joint UIDs
	const COLLADAFW::UniqueIdArray& joint_uids = co->getJoints();
	for (unsigned int i = 0; i < joint_uids.getCount(); i++) {
		joint_data[i].joint_uid = joint_uids[i];

		// // store armature pointer
		// JointData& jd = joint_index_to_joint_info_map[i];
		// jd.ob_arm = ob_arm;

		// now we'll be able to get inv bind matrix from joint id
		// joint_id_to_joint_index_map[joint_ids[i]] = i;
	}
}

// called from write_controller
Object *SkinInfo::create_armature(Scene *scene)
{
	ob_arm = bc_add_object(scene, OB_ARMATURE, NULL);
	return ob_arm;
}

Object* SkinInfo::set_armature(Object *ob_arm)
{
	if (this->ob_arm)
		return this->ob_arm;

	this->ob_arm = ob_arm;
	return ob_arm;
}

bool SkinInfo::get_joint_inv_bind_matrix(float inv_bind_mat[][4], COLLADAFW::Node *node)
{
	const COLLADAFW::UniqueId& uid = node->getUniqueId();
	std::vector<JointData>::iterator it;
	for (it = joint_data.begin(); it != joint_data.end(); it++) {
		if ((*it).joint_uid == uid) {
			copy_m4_m4(inv_bind_mat, (*it).inv_bind_mat);
			return true;
		}
	}

	return false;
}

Object *SkinInfo::BKE_armature_from_object()
{
	return ob_arm;
}

const COLLADAFW::UniqueId& SkinInfo::get_controller_uid()
{
	return controller_uid;
}

// check if this skin controller references a joint or any descendant of it
// 
// some nodes may not be referenced by SkinController,
// in this case to determine if the node belongs to this armature,
// we need to search down the tree
bool SkinInfo::uses_joint_or_descendant(COLLADAFW::Node *node)
{
	const COLLADAFW::UniqueId& uid = node->getUniqueId();
	std::vector<JointData>::iterator it;
	for (it = joint_data.begin(); it != joint_data.end(); it++) {
		if ((*it).joint_uid == uid)
			return true;
	}

	COLLADAFW::NodePointerArray& children = node->getChildNodes();
	for (unsigned int i = 0; i < children.getCount(); i++) {
		if (uses_joint_or_descendant(children[i]))
			return true;
	}

	return false;
}

void SkinInfo::link_armature(bContext *C, Object *ob, std::map<COLLADAFW::UniqueId, COLLADAFW::Node*>& joint_by_uid,
				   TransformReader *tm)
{
	Main *bmain = CTX_data_main(C);
	Scene *scene = CTX_data_scene(C);

	ModifierData *md = ED_object_modifier_add(NULL, bmain, scene, ob, NULL, eModifierType_Armature);
	((ArmatureModifierData *)md)->object = ob_arm;

	copy_m4_m4(ob->obmat, bind_shape_matrix);
	BKE_object_apply_mat4(ob, ob->obmat, 0, 0);
#if 1
	bc_set_parent(ob, ob_arm, C);
#else
	Object workob;
	ob->parent = ob_arm;
	ob->partype = PAROBJECT;

	BKE_object_workob_calc_parent(scene, ob, &workob);
	invert_m4_m4(ob->parentinv, workob.obmat);

	ob->recalc |= OB_RECALC_OB|OB_RECALC_DATA;

	DAG_scene_sort(bmain, scene);
	DAG_ids_flush_update(bmain, 0);
	WM_event_add_notifier(C, NC_OBJECT|ND_TRANSFORM, NULL);
#endif

	((bArmature*)ob_arm->data)->deformflag = ARM_DEF_VGROUP;

	// create all vertex groups
	std::vector<JointData>::iterator it;
	int joint_index;
	for (it = joint_data.begin(), joint_index = 0; it != joint_data.end(); it++, joint_index++) {
		const char *name = "Group";

		// skip joints that have invalid UID
		if ((*it).joint_uid == COLLADAFW::UniqueId::INVALID) continue;
		
		// name group by joint node name
		
		if (joint_by_uid.find((*it).joint_uid) != joint_by_uid.end()) {
			name = bc_get_joint_name(joint_by_uid[(*it).joint_uid]);
		}

		ED_vgroup_add_name(ob, (char*)name);
	}

	// <vcount> - number of joints per vertex - joints_per_vertex
	// <v> - [[bone index, weight index] * joints per vertex] * vertices - weight indices
	// ^ bone index can be -1 meaning weight toward bind shape, how to express this in Blender?

	// for each vertex in weight indices
	//	for each bone index in vertex
	//		add vertex to group at group index
	//		treat group index -1 specially

	// get def group by index with BLI_findlink

	for (unsigned int vertex = 0, weight = 0; vertex < joints_per_vertex.getCount(); vertex++) {

		unsigned int limit = weight + joints_per_vertex[vertex];
		for ( ; weight < limit; weight++) {
			int joint = joint_indices[weight], joint_weight = weight_indices[weight];

			// -1 means "weight towards the bind shape", we just don't assign it to any group
			if (joint != -1) {
				bDeformGroup *def = (bDeformGroup*)BLI_findlink(&ob->defbase, joint);

				ED_vgroup_vert_add(ob, def, vertex, weights[joint_weight], WEIGHT_REPLACE);
			}
		}
	}
}

bPoseChannel *SkinInfo::get_pose_channel_from_node(COLLADAFW::Node *node)
{
	return BKE_pose_channel_find_name(ob_arm->pose, bc_get_joint_name(node));
}

void SkinInfo::set_parent(Object *_parent)
{
	parent = _parent;
}

Object* SkinInfo::get_parent()
{
	return parent;
}

void SkinInfo::find_root_joints(const std::vector<COLLADAFW::Node*> &root_joints,
					  std::map<COLLADAFW::UniqueId, COLLADAFW::Node*>& joint_by_uid,
					  std::vector<COLLADAFW::Node*>& result)
{
	std::vector<COLLADAFW::Node*>::const_iterator it;
	// for each root_joint
	for (it = root_joints.begin(); it != root_joints.end(); it++) {
		COLLADAFW::Node *root = *it;
		std::vector<JointData>::iterator ji;
		//for each joint_data in this skin
		for (ji = joint_data.begin(); ji != joint_data.end(); ji++) {
			//get joint node from joint map
			COLLADAFW::Node *joint = joint_by_uid[(*ji).joint_uid];
			//find if joint node is in the tree belonging to the root_joint
			if (find_node_in_tree(joint, root)) {
				if (std::find(result.begin(), result.end(), root) == result.end())
					result.push_back(root);
			}
		}
	}
}

bool SkinInfo::find_node_in_tree(COLLADAFW::Node *node, COLLADAFW::Node *tree_root)
{
	if (node == tree_root)
		return true;

	COLLADAFW::NodePointerArray& children = tree_root->getChildNodes();
	for (unsigned int i = 0; i < children.getCount(); i++) {
		if (find_node_in_tree(node, children[i]))
			return true;
	}

	return false;
}
