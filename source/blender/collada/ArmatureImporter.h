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

/** \file ArmatureImporter.h
 *  \ingroup collada
 */

#ifndef __ARMATUREIMPORTER_H__
#define __ARMATUREIMPORTER_H__

#include "COLLADAFWNode.h"
#include "COLLADAFWUniqueId.h"

extern "C" {
#include "BKE_context.h"

#include "DNA_armature_types.h"
#include "DNA_object_types.h"
#include "DNA_scene_types.h"

#include "ED_armature.h"
}

#include "AnimationImporter.h"
#include "MeshImporter.h"
#include "SkinInfo.h"
#include "TransformReader.h"
#include "ExtraTags.h"

#include <map>
#include <vector>

#include "collada_internal.h"
#include "collada_utils.h"

class ArmatureImporter : private TransformReader
{
private:
	Scene *scene;
	UnitConverter *unit_converter;

	// std::map<int, JointData> joint_index_to_joint_info_map;
	// std::map<COLLADAFW::UniqueId, int> joint_id_to_joint_index_map;

	struct LeafBone {
		// COLLADAFW::Node *node;
		EditBone *bone;
		char name[32];
		float mat[4][4]; // bone matrix, derived from inv_bind_mat
	};
	std::vector<LeafBone> leaf_bones;
	// int bone_direction_row; // XXX not used
	float leaf_bone_length;
	int totbone;
	// XXX not used
	// float min_angle; // minimum angle between bone head-tail and a row of bone matrix

#if 0
	struct ArmatureJoints {
		Object *ob_arm;
		std::vector<COLLADAFW::Node*> root_joints;
	};
	std::vector<ArmatureJoints> armature_joints;
#endif

	Object *empty; // empty for leaf bones

	std::map<COLLADAFW::UniqueId, COLLADAFW::UniqueId> geom_uid_by_controller_uid;
	std::map<COLLADAFW::UniqueId, COLLADAFW::Node*> joint_by_uid; // contains all joints
	std::vector<COLLADAFW::Node*> root_joints;
	std::vector<COLLADAFW::Node*> finished_joints;
	std::map<COLLADAFW::UniqueId, Object*> joint_parent_map;
	std::map<COLLADAFW::UniqueId, Object*> unskinned_armature_map;

	MeshImporterBase *mesh_importer;
	AnimationImporterBase *anim_importer;

	// This is used to store data passed in write_controller_data.
	// Arrays from COLLADAFW::SkinControllerData lose ownership, so do this class members
	// so that arrays don't get freed until we free them explicitly.

	std::map<COLLADAFW::UniqueId, SkinInfo> skin_by_data_uid; // data UID = skin controller data UID
#if 0
	JointData *get_joint_data(COLLADAFW::Node *node);
#endif

	void create_bone(SkinInfo& skin, COLLADAFW::Node *node, EditBone *parent, int totchild,
					 float parent_mat[][4], bArmature *arm);

	void create_unskinned_bone(COLLADAFW::Node *node, EditBone *parent, int totchild,
	                           float parent_mat[][4], Object * ob_arm);

	void add_leaf_bone(float mat[][4], EditBone *bone, COLLADAFW::Node * node);

	void fix_leaf_bones();
	
	void set_pose ( Object * ob_arm ,  COLLADAFW::Node * root_node , const char *parentname, float parent_mat[][4]);


#if 0
	void set_leaf_bone_shapes(Object *ob_arm);
	void set_euler_rotmode();
#endif

	Object *get_empty_for_leaves();

#if 0
	Object *find_armature(COLLADAFW::Node *node);

	ArmatureJoints& get_armature_joints(Object *ob_arm);
#endif

	void create_armature_bones(SkinInfo& skin);
	void create_armature_bones( );

	/** TagsMap typedef for uid_tags_map. */
	typedef std::map<std::string, ExtraTags*> TagsMap;
	TagsMap uid_tags_map;
public:

	ArmatureImporter(UnitConverter *conv, MeshImporterBase *mesh, AnimationImporterBase *anim, Scene *sce);
	~ArmatureImporter();

	// root - if this joint is the top joint in hierarchy, if a joint
	// is a child of a node (not joint), root should be true since
	// this is where we build armature bones from
	void add_joint(COLLADAFW::Node *node, bool root, Object *parent, Scene *sce);

#if 0
	void add_root_joint(COLLADAFW::Node *node);
#endif

	// here we add bones to armatures, having armatures previously created in write_controller
	void make_armatures(bContext *C);

#if 0
	// link with meshes, create vertex groups, assign weights
	void link_armature(Object *ob_arm, const COLLADAFW::UniqueId& geom_id, const COLLADAFW::UniqueId& controller_data_id);
#endif

	bool write_skin_controller_data(const COLLADAFW::SkinControllerData* data);

	bool write_controller(const COLLADAFW::Controller* controller);

	COLLADAFW::UniqueId *get_geometry_uid(const COLLADAFW::UniqueId& controller_uid);
	
	Object *get_armature_for_joint(COLLADAFW::Node *node);

	void get_rna_path_for_joint(COLLADAFW::Node *node, char *joint_path, size_t count);
	
	// gives a world-space mat
	bool get_joint_bind_mat(float m[][4], COLLADAFW::Node *joint);

	void set_tags_map( TagsMap& tags_map);
	
};

#endif
