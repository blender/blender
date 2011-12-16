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
 * Contributor(s): Chingiz Dyussenov, Arystanbek Dyussenov, Nathan Letwory, Sukhitha jayathilake.
 *
 * ***** END GPL LICENSE BLOCK *****
 */

/** \file blender/collada/ArmatureImporter.cpp
 *  \ingroup collada
 */


/* COLLADABU_ASSERT, may be able to remove later */
#include "COLLADABUPlatform.h"

#include <algorithm>

#include "COLLADAFWUniqueId.h"

#include "BKE_action.h"
#include "BKE_depsgraph.h"
#include "BKE_object.h"
#include "BKE_armature.h"
#include "BLI_string.h"
#include "ED_armature.h"

#include "ArmatureImporter.h"

// use node name, or fall back to original id if not present (name is optional)
template<class T>
static const char *bc_get_joint_name(T *node)
{
	const std::string& id = node->getName();
	return id.size() ? id.c_str() : node->getOriginalId().c_str();
}

ArmatureImporter::ArmatureImporter(UnitConverter *conv, MeshImporterBase *mesh, AnimationImporterBase *anim, Scene *sce) :
	TransformReader(conv), scene(sce), empty(NULL), mesh_importer(mesh), anim_importer(anim) {}

ArmatureImporter::~ArmatureImporter()
{
	// free skin controller data if we forget to do this earlier
	std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;
	for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {
		it->second.free();
	}
}

#if 0
JointData *ArmatureImporter::get_joint_data(COLLADAFW::Node *node);
{
	const COLLADAFW::UniqueId& joint_id = node->getUniqueId();

	if (joint_id_to_joint_index_map.find(joint_id) == joint_id_to_joint_index_map.end()) {
		fprintf(stderr, "Cannot find a joint index by joint id for %s.\n",
				node->getOriginalId().c_str());
		return NULL;
	}

	int joint_index = joint_id_to_joint_index_map[joint_id];

	return &joint_index_to_joint_info_map[joint_index];
}
#endif
void ArmatureImporter::create_unskinned_bone( COLLADAFW::Node *node, EditBone *parent, int totchild,
				 float parent_mat[][4], Object * ob_arm)
{
	std::vector<COLLADAFW::Node*>::iterator it;
	it = std::find(finished_joints.begin(),finished_joints.end(),node);
	if ( it != finished_joints.end()) return; 

	float mat[4][4];
	float obmat[4][4];

	// object-space
	get_node_mat(obmat, node, NULL, NULL);

	EditBone *bone = ED_armature_edit_bone_add((bArmature*)ob_arm->data, (char*)bc_get_joint_name(node));
	totbone++;
	
	if (parent) bone->parent = parent;

	float angle = 0;

	// get world-space
	if (parent){
		mult_m4_m4m4(mat, parent_mat, obmat);

	}
	else {
		copy_m4_m4(mat, obmat);

	}
	float loc[3], size[3], rot[3][3];
	mat4_to_loc_rot_size( loc, rot, size, obmat);
	mat3_to_vec_roll(rot, NULL, &angle );
	bone->roll=angle;
	// set head
	copy_v3_v3(bone->head, mat[3]);

	// set tail, don't set it to head because 0-length bones are not allowed
	float vec[3] = {0.0f, 0.5f, 0.0f};
	add_v3_v3v3(bone->tail, bone->head, vec);

	// set parent tail
	if (parent && totchild == 1) {
		copy_v3_v3(parent->tail, bone->head);
		
		// not setting BONE_CONNECTED because this would lock child bone location with respect to parent
		// bone->flag |= BONE_CONNECTED;
	
		// XXX increase this to prevent "very" small bones?
		const float epsilon = 0.000001f;

		// derive leaf bone length
		float length = len_v3v3(parent->head, parent->tail);
		if ((length < leaf_bone_length || totbone == 0) && length > epsilon) {
			leaf_bone_length = length;
		}

		// treat zero-sized bone like a leaf bone
		if (length <= epsilon) {
			add_leaf_bone(parent_mat, parent, node);
		}

	}

	COLLADAFW::NodePointerArray& children = node->getChildNodes();
	for (unsigned int i = 0; i < children.getCount(); i++) {
		create_unskinned_bone( children[i], bone, children.getCount(), mat, ob_arm);
	}

	// in second case it's not a leaf bone, but we handle it the same way
	if (!children.getCount() || children.getCount() > 1) {
		add_leaf_bone(mat, bone, node);
	}

	finished_joints.push_back(node);

}

void ArmatureImporter::create_bone(SkinInfo& skin, COLLADAFW::Node *node, EditBone *parent, int totchild,
				 float parent_mat[][4], bArmature *arm)
{
	//Checking if bone is already made.
	std::vector<COLLADAFW::Node*>::iterator it;
	it = std::find(finished_joints.begin(),finished_joints.end(),node);
	if ( it != finished_joints.end()) return; 

	float joint_inv_bind_mat[4][4];

	// JointData* jd = get_joint_data(node);

	float mat[4][4];

	// TODO rename from Node "name" attrs later
	EditBone *bone = ED_armature_edit_bone_add(arm, (char*)bc_get_joint_name(node));
	totbone++;

	if (skin.get_joint_inv_bind_matrix(joint_inv_bind_mat, node)) {
		// get original world-space matrix
		invert_m4_m4(mat, joint_inv_bind_mat);
	}
	// create a bone even if there's no joint data for it (i.e. it has no influence)
	else {
		float obmat[4][4];

		// object-space
		get_node_mat(obmat, node, NULL, NULL);

		// get world-space
		if (parent)
			mult_m4_m4m4(mat, parent_mat, obmat);
		else
			copy_m4_m4(mat, obmat);

		float loc[3], size[3], rot[3][3] , angle;
		mat4_to_loc_rot_size( loc, rot, size, obmat);
		mat3_to_vec_roll(rot, NULL, &angle );
		bone->roll=angle;
	}

	
	if (parent) bone->parent = parent;

	// set head
	copy_v3_v3(bone->head, mat[3]);

	// set tail, don't set it to head because 0-length bones are not allowed
	float vec[3] = {0.0f, 0.5f, 0.0f};
	add_v3_v3v3(bone->tail, bone->head, vec);

	// set parent tail
	if (parent && totchild == 1) {
		copy_v3_v3(parent->tail, bone->head);

		// not setting BONE_CONNECTED because this would lock child bone location with respect to parent
		// bone->flag |= BONE_CONNECTED;

		// XXX increase this to prevent "very" small bones?
		const float epsilon = 0.000001f;

		// derive leaf bone length
		float length = len_v3v3(parent->head, parent->tail);
		if ((length < leaf_bone_length || totbone == 0) && length > epsilon) {
			leaf_bone_length = length;
		}

		// treat zero-sized bone like a leaf bone
		if (length <= epsilon) {
			add_leaf_bone(parent_mat, parent, node);
		}

		/*
#if 0
		// and which row in mat is bone direction
		float vec[3];
		sub_v3_v3v3(vec, parent->tail, parent->head);
#ifdef COLLADA_DEBUG
		print_v3("tail - head", vec);
		print_m4("matrix", parent_mat);
#endif
		for (int i = 0; i < 3; i++) {
#ifdef COLLADA_DEBUG
			char *axis_names[] = {"X", "Y", "Z"};
			printf("%s-axis length is %f\n", axis_names[i], len_v3(parent_mat[i]));
#endif
			float angle = angle_v2v2(vec, parent_mat[i]);
			if (angle < min_angle) {
#ifdef COLLADA_DEBUG
				print_v3("picking", parent_mat[i]);
				printf("^ %s axis of %s's matrix\n", axis_names[i], get_dae_name(node));
#endif
				bone_direction_row = i;
				min_angle = angle;
			}
		}
#endif
		*/
	}

	COLLADAFW::NodePointerArray& children = node->getChildNodes();
	for (unsigned int i = 0; i < children.getCount(); i++) {
		create_bone(skin, children[i], bone, children.getCount(), mat, arm);
	}

	// in second case it's not a leaf bone, but we handle it the same way
	if (!children.getCount() || children.getCount() > 1) {
		add_leaf_bone(mat, bone , node);
	}

	finished_joints.push_back(node);
}

void ArmatureImporter::add_leaf_bone(float mat[][4], EditBone *bone,  COLLADAFW::Node * node)
{
	LeafBone leaf;

	leaf.bone = bone;
	copy_m4_m4(leaf.mat, mat);
	BLI_strncpy(leaf.name, bone->name, sizeof(leaf.name));
	
	TagsMap::iterator etit;
	ExtraTags *et = 0;
	etit = uid_tags_map.find(node->getUniqueId().toAscii());
	if(etit !=  uid_tags_map.end())
	{
		et = etit->second;
		//else return;

		float x,y,z;
		et->setData("tip_x",&x);
		et->setData("tip_y",&y);
		et->setData("tip_z",&z);
		float vec[3] = {x,y,z};
		copy_v3_v3(leaf.bone->tail, leaf.bone->head);
		add_v3_v3v3(leaf.bone->tail, leaf.bone->head, vec);
	}else
		leaf_bones.push_back(leaf);
}

void ArmatureImporter::fix_leaf_bones( )
{
	// just setting tail for leaf bones here

	std::vector<LeafBone>::iterator it;
	for (it = leaf_bones.begin(); it != leaf_bones.end(); it++) {
		LeafBone& leaf = *it;

		// pointing up
		float vec[3] = {0.0f, 0.0f, 0.1f};
		
		// if parent: take parent length and direction
		if(leaf.bone->parent) sub_v3_v3v3(vec, leaf.bone->parent->tail, leaf.bone->parent->head);

		copy_v3_v3(leaf.bone->tail, leaf.bone->head);
		add_v3_v3v3(leaf.bone->tail, leaf.bone->head, vec);
	}
}

#if 0
void ArmatureImporter::set_leaf_bone_shapes(Object *ob_arm)
{
	bPose *pose = ob_arm->pose;

	std::vector<LeafBone>::iterator it;
	for (it = leaf_bones.begin(); it != leaf_bones.end(); it++) {
		LeafBone& leaf = *it;

		bPoseChannel *pchan = get_pose_channel(pose, leaf.name);
		if (pchan) {
			pchan->custom = get_empty_for_leaves();
		}
		else {
			fprintf(stderr, "Cannot find a pose channel for leaf bone %s\n", leaf.name);
		}
	}
}

void ArmatureImporter::set_euler_rotmode()
{
	// just set rotmode = ROT_MODE_EUL on pose channel for each joint

	std::map<COLLADAFW::UniqueId, COLLADAFW::Node*>::iterator it;

	for (it = joint_by_uid.begin(); it != joint_by_uid.end(); it++) {

		COLLADAFW::Node *joint = it->second;

		std::map<COLLADAFW::UniqueId, SkinInfo>::iterator sit;
		
		for (sit = skin_by_data_uid.begin(); sit != skin_by_data_uid.end(); sit++) {
			SkinInfo& skin = sit->second;

			if (skin.uses_joint_or_descendant(joint)) {
				bPoseChannel *pchan = skin.get_pose_channel_from_node(joint);

				if (pchan) {
					pchan->rotmode = ROT_MODE_EUL;
				}
				else {
					fprintf(stderr, "Cannot find pose channel for %s.\n", get_joint_name(joint));
				}

				break;
			}
		}
	}
}
#endif

Object *ArmatureImporter::get_empty_for_leaves()
{
	if (empty) return empty;
	
	empty = add_object(scene, OB_EMPTY);
	empty->empty_drawtype = OB_EMPTY_SPHERE;

	return empty;
}

#if 0
Object *ArmatureImporter::find_armature(COLLADAFW::Node *node)
{
	JointData* jd = get_joint_data(node);
	if (jd) return jd->ob_arm;

	COLLADAFW::NodePointerArray& children = node->getChildNodes();
	for (int i = 0; i < children.getCount(); i++) {
		Object *ob_arm = find_armature(children[i]);
		if (ob_arm) return ob_arm;
	}

	return NULL;
}

ArmatureJoints& ArmatureImporter::get_armature_joints(Object *ob_arm)
{
	// try finding it
	std::vector<ArmatureJoints>::iterator it;
	for (it = armature_joints.begin(); it != armature_joints.end(); it++) {
		if ((*it).ob_arm == ob_arm) return *it;
	}

	// not found, create one
	ArmatureJoints aj;
	aj.ob_arm = ob_arm;
	armature_joints.push_back(aj);

	return armature_joints.back();
}
#endif
void ArmatureImporter::create_armature_bones( )
{
	std::vector<COLLADAFW::Node*>::iterator ri;
	//if there is an armature created for root_joint next root_joint
	for (ri = root_joints.begin(); ri != root_joints.end(); ri++) {
			if ( get_armature_for_joint(*ri) != NULL ) continue;
		
		//add armature object for current joint
		//Object *ob_arm = add_object(scene, OB_ARMATURE);

		Object *ob_arm = joint_parent_map[(*ri)->getUniqueId()];
		//ob_arm->type = OB_ARMATURE;
		ED_armature_to_edit(ob_arm);

		// min_angle = 360.0f;		// minimum angle between bone head-tail and a row of bone matrix

		// create unskinned bones
		/*
		   TODO:
		   check if bones have already been created for a given joint
		*/
		leaf_bone_length = FLT_MAX;
		create_unskinned_bone(*ri, NULL, (*ri)->getChildNodes().getCount(), NULL, ob_arm);

		fix_leaf_bones();

	// exit armature edit mode
	
	unskinned_armature_map[(*ri)->getUniqueId()] = ob_arm;

	ED_armature_from_edit(ob_arm);

	set_pose(ob_arm , *ri, NULL, NULL ); 

	ED_armature_edit_free(ob_arm);
	DAG_id_tag_update(&ob_arm->id, OB_RECALC_OB|OB_RECALC_DATA);
	}

	
}

void ArmatureImporter::create_armature_bones(SkinInfo& skin)
{
	// just do like so:
	// - get armature
	// - enter editmode
	// - add edit bones and head/tail properties using matrices and parent-child info
	// - exit edit mode
	// - set a sphere shape to leaf bones

	Object *ob_arm = NULL;

	/*
	 * find if there's another skin sharing at least one bone with this skin
	 * if so, use that skin's armature
	 */

	/*
	  Pseudocode:

	  find_node_in_tree(node, root_joint)

	  skin::find_root_joints(root_joints):
		std::vector root_joints;
		for each root in root_joints:
			for each joint in joints:
				if find_node_in_tree(joint, root):
					if (std::find(root_joints.begin(), root_joints.end(), root) == root_joints.end())
						root_joints.push_back(root);

	  for (each skin B with armature) {
		  find all root joints for skin B

		  for each joint X in skin A:
			for each root joint R in skin B:
				if (find_node_in_tree(X, R)) {
					shared = 1;
					goto endloop;
				}
	  }

	  endloop:
	*/

	SkinInfo *a = &skin;
	Object *shared = NULL;
	std::vector<COLLADAFW::Node*> skin_root_joints;

	std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;
	for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {
		SkinInfo *b = &it->second;
		if (b == a || b->get_armature() == NULL)
			continue;

		skin_root_joints.clear();

		b->find_root_joints(root_joints, joint_by_uid, skin_root_joints);

		std::vector<COLLADAFW::Node*>::iterator ri;
		for (ri = skin_root_joints.begin(); ri != skin_root_joints.end(); ri++) {
			if (a->uses_joint_or_descendant(*ri)) {
				shared = b->get_armature();
				break;
			}
		}

		if (shared != NULL)
			break;
	}

	if (shared)
		ob_arm = skin.set_armature(shared);
	else
		ob_arm = skin.create_armature(scene); //once for every armature

	// enter armature edit mode
	ED_armature_to_edit(ob_arm);

	leaf_bones.clear();
	totbone = 0;
	// bone_direction_row = 1; // TODO: don't default to Y but use asset and based on it decide on default row
	leaf_bone_length = FLT_MAX;
	// min_angle = 360.0f;		// minimum angle between bone head-tail and a row of bone matrix

	// create bones
	/*
	   TODO:
	   check if bones have already been created for a given joint
	*/

	std::vector<COLLADAFW::Node*>::iterator ri;
	for (ri = root_joints.begin(); ri != root_joints.end(); ri++) {
		// for shared armature check if bone tree is already created
		if (shared && std::find(skin_root_joints.begin(), skin_root_joints.end(), *ri) != skin_root_joints.end())
			continue;

		// since root_joints may contain joints for multiple controllers, we need to filter
		if (skin.uses_joint_or_descendant(*ri)) {
			create_bone(skin, *ri, NULL, (*ri)->getChildNodes().getCount(), NULL, (bArmature*)ob_arm->data);

			if (joint_parent_map.find((*ri)->getUniqueId()) != joint_parent_map.end() && !skin.get_parent())
				skin.set_parent(joint_parent_map[(*ri)->getUniqueId()]);
		}
	}

	fix_leaf_bones();

	// exit armature edit mode
	ED_armature_from_edit(ob_arm);
	ED_armature_edit_free(ob_arm);
	DAG_id_tag_update(&ob_arm->id, OB_RECALC_OB|OB_RECALC_DATA);

	// set_leaf_bone_shapes(ob_arm);
	// set_euler_rotmode();
}


// root - if this joint is the top joint in hierarchy, if a joint
// is a child of a node (not joint), root should be true since
// this is where we build armature bones from

void ArmatureImporter::set_pose ( Object * ob_arm ,  COLLADAFW::Node * root_node , const char *parentname, float parent_mat[][4])
{ 
	char * bone_name = (char *) bc_get_joint_name ( root_node);
	float mat[4][4];
   float obmat[4][4];

	float ax[3];
	float angle = 0.0f;
	
	// object-space
	get_node_mat(obmat, root_node, NULL, NULL);

	//if(*edbone)
	bPoseChannel * pchan  = get_pose_channel(ob_arm -> pose ,  bone_name); 
	//else fprintf ( "",

	// get world-space
	if (parentname){
		mult_m4_m4m4(mat, parent_mat, obmat);
		bPoseChannel *parchan = get_pose_channel(ob_arm->pose, parentname);

		mult_m4_m4m4(pchan->pose_mat, parchan->pose_mat, mat );

	}
	else {
		copy_m4_m4(mat, obmat);
		float invObmat[4][4];
		invert_m4_m4(invObmat, ob_arm->obmat);
		mult_m4_m4m4(pchan->pose_mat, invObmat, mat);
	}

	mat4_to_axis_angle(ax,&angle,mat);
	pchan->bone->roll = angle;


	COLLADAFW::NodePointerArray& children = root_node->getChildNodes();
	for (unsigned int i = 0; i < children.getCount(); i++) {
		set_pose(ob_arm, children[i], bone_name, mat);
	}

}

void ArmatureImporter::add_joint(COLLADAFW::Node *node, bool root, Object *parent, Scene *sce)
{
	joint_by_uid[node->getUniqueId()] = node;
	if (root) {
		root_joints.push_back(node);

		if (parent) {
					
			joint_parent_map[node->getUniqueId()] = parent;
		}
	}
}

#if 0
void ArmatureImporter::add_root_joint(COLLADAFW::Node *node)
{
	// root_joints.push_back(node);
	Object *ob_arm = find_armature(node);
	if (ob_arm)	{
		get_armature_joints(ob_arm).root_joints.push_back(node);
	}
#ifdef COLLADA_DEBUG
	else {
		fprintf(stderr, "%s cannot be added to armature.\n", get_joint_name(node));
	}
#endif
}
#endif

// here we add bones to armatures, having armatures previously created in write_controller
void ArmatureImporter::make_armatures(bContext *C)
{
	std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;
	for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {

		SkinInfo& skin = it->second;

		create_armature_bones(skin);

		// link armature with a mesh object
		Object *ob = mesh_importer->get_object_by_geom_uid(*get_geometry_uid(skin.get_controller_uid()));
		if (ob)
			skin.link_armature(C, ob, joint_by_uid, this);
		else
			fprintf(stderr, "Cannot find object to link armature with.\n");

		// set armature parent if any
		Object *par = skin.get_parent();
		if (par)
			bc_set_parent(skin.get_armature(), par, C, false);

		// free memory stolen from SkinControllerData
		skin.free();
	}
	
	//for bones without skins
	create_armature_bones();
}

#if 0
// link with meshes, create vertex groups, assign weights
void ArmatureImporter::link_armature(Object *ob_arm, const COLLADAFW::UniqueId& geom_id, const COLLADAFW::UniqueId& controller_data_id)
{
	Object *ob = mesh_importer->get_object_by_geom_uid(geom_id);

	if (!ob) {
		fprintf(stderr, "Cannot find object by geometry UID.\n");
		return;
	}

	if (skin_by_data_uid.find(controller_data_id) == skin_by_data_uid.end()) {
		fprintf(stderr, "Cannot find skin info by controller data UID.\n");
		return;
	}

	SkinInfo& skin = skin_by_data_uid[conroller_data_id];

	// create vertex groups
}
#endif

bool ArmatureImporter::write_skin_controller_data(const COLLADAFW::SkinControllerData* data)
{
	// at this stage we get vertex influence info that should go into me->verts and ob->defbase
	// there's no info to which object this should be long so we associate it with skin controller data UID

	// don't forget to call defgroup_unique_name before we copy

	// controller data uid -> [armature] -> joint data, 
	// [mesh object]
	// 

	SkinInfo skin(unit_converter);
	skin.borrow_skin_controller_data(data);

	// store join inv bind matrix to use it later in armature construction
	const COLLADAFW::Matrix4Array& inv_bind_mats = data->getInverseBindMatrices();
	for (unsigned int i = 0; i < data->getJointsCount(); i++) {
		skin.add_joint(inv_bind_mats[i]);
	}

	skin_by_data_uid[data->getUniqueId()] = skin;

	return true;
}

bool ArmatureImporter::write_controller(const COLLADAFW::Controller* controller)
{
	// - create and store armature object

	const COLLADAFW::UniqueId& skin_id = controller->getUniqueId();

	if (controller->getControllerType() == COLLADAFW::Controller::CONTROLLER_TYPE_SKIN) {
		COLLADAFW::SkinController *co = (COLLADAFW::SkinController*)controller;
		// to be able to find geom id by controller id
		geom_uid_by_controller_uid[skin_id] = co->getSource();

		const COLLADAFW::UniqueId& data_uid = co->getSkinControllerData();
		if (skin_by_data_uid.find(data_uid) == skin_by_data_uid.end()) {
			fprintf(stderr, "Cannot find skin by controller data UID.\n");
			return true;
		}

		skin_by_data_uid[data_uid].set_controller(co);
	}
	// morph controller
	else {
		// shape keys? :)
		fprintf(stderr, "Morph controller is not supported yet.\n");
	}

	return true;
}


COLLADAFW::UniqueId *ArmatureImporter::get_geometry_uid(const COLLADAFW::UniqueId& controller_uid)
{
	if (geom_uid_by_controller_uid.find(controller_uid) == geom_uid_by_controller_uid.end())
		return NULL;

	return &geom_uid_by_controller_uid[controller_uid];
}

Object *ArmatureImporter::get_armature_for_joint(COLLADAFW::Node *node)
{
	std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;
	for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {
		SkinInfo& skin = it->second;

		if (skin.uses_joint_or_descendant(node))
			return skin.get_armature();
	}

	std::map<COLLADAFW::UniqueId, Object*>::iterator arm;
	for (arm = unskinned_armature_map.begin(); arm != unskinned_armature_map.end(); arm++) {
		if(arm->first == node->getUniqueId() )
			return arm->second;
	}
	return NULL;
}

void ArmatureImporter::set_tags_map(TagsMap & tagsMap)
{
	this->uid_tags_map = tagsMap;
}

void ArmatureImporter::get_rna_path_for_joint(COLLADAFW::Node *node, char *joint_path, size_t count)
{
	BLI_snprintf(joint_path, count, "pose.bones[\"%s\"]", bc_get_joint_name(node));
}

// gives a world-space mat
bool ArmatureImporter::get_joint_bind_mat(float m[][4], COLLADAFW::Node *joint)
{
	std::map<COLLADAFW::UniqueId, SkinInfo>::iterator it;
	bool found = false;
	for (it = skin_by_data_uid.begin(); it != skin_by_data_uid.end(); it++) {
		SkinInfo& skin = it->second;
		if ((found = skin.get_joint_inv_bind_matrix(m, joint))) {
			invert_m4(m);
			break;
		}
	}

	return found;
}



