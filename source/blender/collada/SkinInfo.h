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

/** \file SkinInfo.h
 *  \ingroup collada
 */

#ifndef __SKININFO_H__
#define __SKININFO_H__

#include <map>
#include <vector>

#include "COLLADAFWUniqueId.h"
#include "COLLADAFWTypes.h"
#include "COLLADAFWNode.h"
#include "COLLADAFWSkinController.h"
#include "COLLADAFWSkinControllerData.h"

#include "DNA_object_types.h"
#include "BKE_context.h"

#include "TransformReader.h"
#include "collada_internal.h"

// This is used to store data passed in write_controller_data.
// Arrays from COLLADAFW::SkinControllerData lose ownership, so do this class members
// so that arrays don't get freed until we free them explicitly.
class SkinInfo
{
private:
	// to build armature bones from inverse bind matrices
	struct JointData {
		float inv_bind_mat[4][4]; // joint inverse bind matrix
		COLLADAFW::UniqueId joint_uid; // joint node UID
		// Object *ob_arm;			  // armature object
	};

	float bind_shape_matrix[4][4];

	// data from COLLADAFW::SkinControllerData, each array should be freed
	COLLADAFW::UIntValuesArray joints_per_vertex;
	COLLADAFW::UIntValuesArray weight_indices;
	COLLADAFW::IntValuesArray joint_indices;
	// COLLADAFW::FloatOrDoubleArray weights;
	std::vector<float> weights;

	std::vector<JointData> joint_data; // index to this vector is joint index

	UnitConverter *unit_converter;

	Object *ob_arm;
	COLLADAFW::UniqueId controller_uid;
	Object *parent;

public:

	SkinInfo();
	SkinInfo(const SkinInfo& skin);
	SkinInfo(UnitConverter *conv);

	// nobody owns the data after this, so it should be freed manually with releaseMemory
	template <typename T>
	void transfer_array_data(T& src, T& dest);

	// when src is const we cannot src.yieldOwnerShip, this is used by copy constructor
	void transfer_int_array_data_const(const COLLADAFW::IntValuesArray& src, COLLADAFW::IntValuesArray& dest);

	void transfer_uint_array_data_const(const COLLADAFW::UIntValuesArray& src, COLLADAFW::UIntValuesArray& dest);

	void borrow_skin_controller_data(const COLLADAFW::SkinControllerData* skin);
		
	void free();

	// using inverse bind matrices to construct armature
	// it is safe to invert them to get the original matrices
	// because if they are inverse matrices, they can be inverted
	void add_joint(const COLLADABU::Math::Matrix4& matrix);

	void set_controller(const COLLADAFW::SkinController* co);

	// called from write_controller
	Object *create_armature(Scene *scene);

	Object* set_armature(Object *ob_arm);

	bool get_joint_inv_bind_matrix(float inv_bind_mat[][4], COLLADAFW::Node *node);

	Object *BKE_armature_from_object();

	const COLLADAFW::UniqueId& get_controller_uid();

	// check if this skin controller references a joint or any descendant of it
	// 
	// some nodes may not be referenced by SkinController,
	// in this case to determine if the node belongs to this armature,
	// we need to search down the tree
	bool uses_joint_or_descendant(COLLADAFW::Node *node);

	void link_armature(bContext *C, Object *ob, std::map<COLLADAFW::UniqueId, COLLADAFW::Node*>& joint_by_uid, TransformReader *tm);

	bPoseChannel *get_pose_channel_from_node(COLLADAFW::Node *node);

	void set_parent(Object *_parent);

	Object* get_parent();

	void find_root_joints(const std::vector<COLLADAFW::Node*> &root_joints,
						  std::map<COLLADAFW::UniqueId, COLLADAFW::Node*>& joint_by_uid,
						  std::vector<COLLADAFW::Node*>& result);

	bool find_node_in_tree(COLLADAFW::Node *node, COLLADAFW::Node *tree_root);

};

#endif
